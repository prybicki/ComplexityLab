#include "Fact.hpp"

#include <gtest/gtest.h>

#include "ArchetypesCPP.hpp"

#include <cstddef>
#include <thread>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace {

// --- Compile-time properties ----------------------------------------------

// Minimal overhead: a Proof is one pointer.
static_assert(sizeof(Proof<int>) == sizeof(void*));

// Facts are pinned: proofs point at them, so they must never move.
static_assert(!std::is_copy_constructible_v<Fact<int>>);
static_assert(!std::is_copy_assignable_v<Fact<int>>);
static_assert(!std::is_move_constructible_v<Fact<int>>);
static_assert(!std::is_move_assignable_v<Fact<int>>);

// A Proof is always engaged evidence: no empty state to default-construct,
// and its binding is set at construction only — no reassignment.
static_assert(!std::is_default_constructible_v<Proof<int>>);
static_assert(!std::is_copy_assignable_v<Proof<int>>);
static_assert(!std::is_move_assignable_v<Proof<int>>);
static_assert(!std::is_copy_assignable_v<Proof<const int>>);
static_assert(!std::is_move_assignable_v<Proof<const int>>);

// Widening is implicit and strictly one-way.
static_assert(std::is_convertible_v<Proof<int>, Proof<const int>>);
static_assert(!std::is_constructible_v<Proof<int>, const Proof<const int>&>);
static_assert(!std::is_constructible_v<Proof<int>, Proof<const int>&&>);

// The value constructor needs a movable T; the factory path covers the rest,
// and demands a factory returning exactly T (the elision guarantee) —
// convertible is not enough.
static_assert(!std::is_constructible_v<Fact<NoMove>, NoMove>);
static_assert(std::is_constructible_v<Fact<NoMove>, FactoryOf<NoMove>>);
static_assert(std::is_constructible_v<Fact<int>, FactoryOf<int>>);
static_assert(!std::is_constructible_v<Fact<int>, FactoryOf<short>>);

// noexcept mirrors T's move.
static_assert(std::is_nothrow_constructible_v<Fact<int>, int>);
static_assert(!std::is_nothrow_constructible_v<Fact<ThrowingMove>, ThrowingMove>);

// --- Fixture ----------------------------------------------------------------

struct SpyReport {
    int calls = 0;
    FactViolation last{};
};
SpyReport spy;  // handlers are plain function pointers, so the spy is global

class testFact : public ::testing::Test {
protected:
    void SetUp() override {
        spy = {};
        setFactViolationHandler(nullptr);
        setFactViolationSemantic(FactViolationSemantic::Enforce);
    }
    void TearDown() override {
        setFactViolationHandler(nullptr);
        setFactViolationSemantic(FactViolationSemantic::Enforce);
    }
    // Observe + spy: a deliberate violation then reports the live count
    // instead of aborting — the black-box window onto the proof count.
    static void armProbe() {
        setFactViolationSemantic(FactViolationSemantic::Observe);
        setFactViolationHandler(+[](const FactViolation& violation) noexcept {
            ++spy.calls;
            spy.last = violation;
        });
    }
};

// --- Value ownership and access ---------------------------------------------

TEST_F(testFact, ConstructionPaths) {
    Fact<int> fromValue{42};
    EXPECT_EQ(*fromValue, 42);

    Fact<NoMove> fromFactory{FactoryOf<NoMove>{}};
    EXPECT_EQ(fromFactory->x, 7);

    Fact<int> defaulted{};
    EXPECT_EQ(*defaulted, 0);  // value-initialized, not garbage
}

TEST_F(testFact, ProofAccess) {
    Fact<int> fact{1};
    Proof<int> mut = fact.proofMut();
    *mut = 5;
    EXPECT_EQ(*fact, 5);

    Proof<const int> ro = fact.proof();
    EXPECT_EQ(*ro, 5);
    EXPECT_EQ(&*ro, &*fact);  // same object: no copy anywhere
    EXPECT_EQ(mut.operator->(), fact.operator->());
}

// --- The count: one entry per engaged proof ---------------------------------
// Probe pattern: build a proof topology on a heap Fact, destroy the Fact
// under Observe, and read the count from the violation report. Surviving
// proofs are leaked on purpose — after the violation the Fact is gone, and
// destroying a survivor would decrement freed memory.

TEST_F(testFact, CountsCopyAndMove) {
    armProbe();
    auto* fact = new Fact<int>{0};
    auto* original = new Proof<const int>{fact->proof()};
    auto* copy = new Proof<const int>{*original};              // one more entry
    auto* stolen = new Proof<const int>{std::move(*original)}; // transfer only
    (void)copy;
    (void)stolen;
    delete fact;

    EXPECT_EQ(spy.calls, 1);
    EXPECT_EQ(spy.last.liveProofs, 2u);  // copy + stolen; moved-from not counted
}

TEST_F(testFact, WideningCountsLikeCopyAndMove) {
    armProbe();
    auto* fact = new Fact<int>{3};
    auto* mut = new Proof<int>{fact->proofMut()};
    auto* widenedCopy = new Proof<const int>{*mut};              // one more entry
    auto* widenedSteal = new Proof<const int>{std::move(*mut)};  // transfer only
    (void)widenedCopy;
    (void)widenedSteal;
    delete fact;

    EXPECT_EQ(spy.calls, 1);
    EXPECT_EQ(spy.last.liveProofs, 2u);
}

TEST_F(testFact, DestructionReleases) {
    armProbe();
    {
        Fact<int> fact{1};
        Proof<const int> proof = fact.proof();
        Proof<const int> copy = proof;
    }  // proofs die before the fact: a clean death reports nothing
    EXPECT_EQ(spy.calls, 0);
}

TEST_F(testFact, CounterIsAtomic) {
    armProbe();
    auto* fact = new Fact<int>{0};
    auto* base = new Proof<const int>{fact->proof()};

    int threads = std::thread::hardware_concurrency();
    constexpr int iterations = 65'536; // under 50ms
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t)
        pool.emplace_back([base] {
            for (int i = 0; i < iterations; ++i) {
                Proof<const int> churn = *base;
                (void)churn;
            }
        });
    for (std::thread& t : pool) t.join();

    delete fact;  // exactly the base survives: no increment or decrement lost
    EXPECT_EQ(spy.calls, 1);
    EXPECT_EQ(spy.last.liveProofs, 1u);
}

// --- Violation detection -----------------------------------------------------

TEST_F(testFact, ObserveReportsAndContinues) {
    armProbe();
    auto* fact = new Fact<int>{1};
    const void* factAddress = fact;
    auto* survivor = new Proof<const int>{fact->proof()};
    auto* second = new Proof<const int>{*survivor};
    (void)second;
    delete fact;  // violation fires here; Observe lets execution continue

    EXPECT_EQ(spy.calls, 1);
    EXPECT_EQ(spy.last.liveProofs, 2u);
    EXPECT_EQ(spy.last.fact, factAddress);
    EXPECT_STREQ(spy.last.type, typeid(int).name());
}

TEST_F(testFact, EnforceAborts) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH(
        {
            Fact<int> fact{1};
            auto* survivor = new Proof<const int>{fact.proof()};
            (void)survivor;
        },
        "still held");
}

TEST_F(testFact, SetterProtocol) {
    FactViolationHandler quiet = +[](const FactViolation&) noexcept {};

    EXPECT_EQ(setFactViolationHandler(quiet), &defaultFactViolationHandler);
    EXPECT_EQ(setFactViolationHandler(nullptr), quiet);
    EXPECT_EQ(setFactViolationHandler(quiet), &defaultFactViolationHandler);

    EXPECT_EQ(setFactViolationSemantic(FactViolationSemantic::Observe),
              FactViolationSemantic::Enforce);
    EXPECT_EQ(setFactViolationSemantic(FactViolationSemantic::Enforce),
              FactViolationSemantic::Observe);
}

}  // namespace
