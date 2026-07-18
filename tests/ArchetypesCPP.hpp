#pragma once

// Archetypes of C++ type properties: minimal types that exist only to satisfy
// — or violate — one specific property, for probing constraints in tests. Keep each
// one single-purpose; a new property gets a new archetype, not a new flag on
// an old one.

// Default-constructible, but neither copyable nor movable. Carries one
// observable member so in-place construction can be verified.
struct NoMove {
    NoMove() = default;
    NoMove(const NoMove&) = delete;
    NoMove& operator=(const NoMove&) = delete;
    int x = 7;
};

// Movable, but the move may throw — distinguishes noexcept-conditional
// interfaces from unconditionally noexcept ones.
struct ThrowingMove {
    ThrowingMove() = default;
    ThrowingMove(ThrowingMove&&) noexcept(false) {}
};

// A factory returning exactly T by value. FactoryOf<U> with U convertible
// to T probes exact-return-type constraints against merely-convertible ones.
template <typename T>
struct FactoryOf {
    T operator()() { return T{}; }
};
