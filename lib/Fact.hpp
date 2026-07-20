#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <utility>

// Fact<T> owns a value; dependents hold Proof<T> — counted, non-owning
// handles — while they rely on it. Expresses the dependency at minimal
// cost and verifies the lifetime ordering it implies:
//
//   shared_ptr  extends the owner's lifetime to cover its users.
//   weak_ptr    owner may die first; users detect it, paying lock() per use.
//   Fact        the fact must outlive its proofs, and that is verified:
//               raw-pointer-speed access, one check in ~Fact, in all builds.
//
// The count does not synchronize access to T and cannot see raw pointers
// that escape a Proof. Typical use — a precondition pinned by dependents:
//
//   Fact<GlfwInitialization> glfw{GlfwInitialization::init};
//
//   struct Window {
//       explicit Window(Proof<const GlfwInitialization> p)
//           : glfw_(std::move(p)) {}
//       Proof<const GlfwInitialization> glfw_;  // GLFW stays initialized
//   };                                          // while any Window lives

// Violation reporting, shaped like C++26 contracts so migration stays
// mechanical: the handler reports, the semantic decides what follows —
// Enforce (default) aborts after the handler returns; Observe continues,
// leaving the surviving proofs dangling (diagnostics only).
//
// The handler runs before any damage: the value and the dependents are
// still alive, so emergency work (saving data) is well-defined. It must
// not throw, not rely on services already torn down (it may run during
// unwinding or static teardown), and not trip another violation.
struct FactViolation {
    const char* type;        // typeid name of the Fact's T
    std::size_t liveProofs;  // proofs still held as the Fact dies
    const void* fact;        // address of the dying Fact
};

using FactViolationHandler = void (*)(const FactViolation&) noexcept;

enum class FactViolationSemantic { Enforce, Observe };

inline void defaultFactViolationHandler(const FactViolation& violation) noexcept {
    std::fprintf(stderr,
                 "Fact<%s> at %p destroyed while %zu proof(s) of it are "
                 "still held — a proof outlived the fact it proves\n",
                 violation.type, violation.fact, violation.liveProofs);
}

inline std::atomic<FactViolationHandler> factViolationHandler{&defaultFactViolationHandler};
inline std::atomic<FactViolationSemantic> factViolationSemantic{FactViolationSemantic::Enforce};

// Both setters return the previous value; a null handler restores the default.
inline FactViolationHandler setFactViolationHandler(FactViolationHandler handler) noexcept {
    return factViolationHandler.exchange(handler ? handler : &defaultFactViolationHandler);
}

inline FactViolationSemantic setFactViolationSemantic(FactViolationSemantic semantic) noexcept {
    return factViolationSemantic.exchange(semantic);
}

template <typename T> struct [[nodiscard]] Proof;

template <typename T>
struct Fact {
    static_assert(std::is_object_v<T> && !std::is_const_v<T> &&
                      !std::is_volatile_v<T> && !std::is_array_v<T>,
                  "Fact<T> requires a cv-unqualified, non-array object type");

    // T is value-initialized (deleted if T is not default-constructible).
    Fact() = default;

    explicit Fact(T value) noexcept(std::is_nothrow_move_constructible_v<T>)
        requires std::is_move_constructible_v<T>
        : value(std::move(value)) {}

    // Build T in place from a factory returning T by value — the path for
    // non-movable or privately-constructed values (guaranteed copy elision).
    template <typename Factory>
        requires std::is_same_v<std::invoke_result_t<Factory&>, T>
    explicit Fact(Factory&& makeValue) noexcept(std::is_nothrow_invocable_v<Factory&>)
        : value(makeValue()) {}

    template <typename... Args>
        requires std::is_constructible_v<T, Args...>
    explicit Fact(std::in_place_t, Args&&... args)  // _AI
        noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : value(std::forward<Args>(args)...) {}

    // Proofs point at this object, so its address must stay fixed.
    Fact(const Fact&) = delete;
    Fact& operator=(const Fact&) = delete;
    Fact(Fact&&) = delete;
    Fact& operator=(Fact&&) = delete;

    ~Fact() {
        // acquire pairs with ~Proof's releasing decrement: observing 0 means
        // every use of T through proofs happens-before this point.
        const std::size_t live = proofCount.load(std::memory_order_acquire);
        if (live == 0) return;
        const FactViolation violation{typeid(T).name(), live, this};
        factViolationHandler.load()(violation);
        if (factViolationSemantic.load() == FactViolationSemantic::Enforce)
            std::abort();
    }

    // Direct access for the Fact's holder; needs no proof.
    [[nodiscard]] T& operator*() noexcept { return value; }
    [[nodiscard]] const T& operator*() const noexcept { return value; }
    [[nodiscard]] T* operator->() noexcept { return std::addressof(value); }
    [[nodiscard]] const T* operator->() const noexcept { return std::addressof(value); }

    Proof<const T> proof() const noexcept;
    Proof<T> proofMut() noexcept;

private:
    template <typename> friend class Proof;

    T value{};
    mutable std::atomic<std::size_t> proofCount{0};
};

// Non-owning, counted evidence that a Fact<T> is alive. Proof<const T>
// reads, Proof<T> also writes; Proof<T> widens implicitly to Proof<const T>.
// Copyable — proofs are not exclusive, a copy is one more count — and
// movable, but never reassigned: a binding is set at construction and ends
// at destruction; a moved-from Proof may only be destroyed.
template <typename T>
struct [[nodiscard]] Proof {
    Proof(const Proof& other) noexcept : fact(other.fact) {
        if (fact) fact->proofCount.fetch_add(1, std::memory_order_relaxed);
    }

    Proof(Proof&& other) noexcept : fact(std::exchange(other.fact, nullptr)) {}

    // Bindings are set at construction only; to drop a proof, destroy it.
    Proof& operator=(const Proof&) = delete;
    Proof& operator=(Proof&&) = delete;

    // Widening: a read-write proof is usable wherever a read-only one is.
    Proof(const Proof<std::remove_const_t<T>>& other) noexcept
        requires std::is_const_v<T>
        : fact(other.fact) {
        if (fact) fact->proofCount.fetch_add(1, std::memory_order_relaxed);
    }

    Proof(Proof<std::remove_const_t<T>>&& other) noexcept
        requires std::is_const_v<T>
        : fact(std::exchange(other.fact, nullptr)) {}

    // The releasing decrement pairs with the acquire load in ~Fact.
    ~Proof() {
        if (fact) fact->proofCount.fetch_sub(1, std::memory_order_release);
    }

    [[nodiscard]] T& operator*() const noexcept {
        assert(fact && "dereferencing a moved-from Proof");
        return fact->value;
    }
    [[nodiscard]] T* operator->() const noexcept {
        assert(fact && "dereferencing a moved-from Proof");
        return std::addressof(fact->value);
    }

private:
    template <typename> friend class Fact;
    template <typename> friend class Proof;  // widening constructors

    // The Fact this proof refers to; const iff the proof is read-only.
    using Owner = std::conditional_t<std::is_const_v<T>,
                                     const Fact<std::remove_const_t<T>>,
                                     Fact<T>>;

    explicit Proof(Owner& owner) noexcept : fact(&owner) {
        fact->proofCount.fetch_add(1, std::memory_order_relaxed);
    }

    Owner* fact;
};

template <typename T>
Proof<const T> Fact<T>::proof() const noexcept {
    return Proof<const T>{*this};
}

template <typename T>
Proof<T> Fact<T>::proofMut() noexcept {
    return Proof<T>{*this};
}
