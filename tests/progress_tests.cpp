#include <doctest/doctest.h>

#include "oc/progress.hpp"

using namespace oc;

namespace {

Result run(int stars, int score, double time) { return Result{.score = score, .stars = stars, .time = time}; }

} // namespace

TEST_CASE("a new player has only the first level open and no stars") {
    const Progress p(10);
    CHECK(p.unlocked(0));
    CHECK_FALSE(p.unlocked(1));
    CHECK_FALSE(p.unlocked(10));
    CHECK(p.totalStars() == 0);
    CHECK(p.suggestedLevel() == 0);
    CHECK(p.soundOn);
}

TEST_CASE("completing a level opens the next one") {
    Progress p(10);
    CHECK(p.record(0, run(1, 1500, 80.0)));
    CHECK(p.unlocked(1));
    CHECK_FALSE(p.unlocked(2));
    CHECK(p.suggestedLevel() == 0); // not three stars yet
}

TEST_CASE("records keep the best of every attempt") {
    Progress p(3);
    p.record(0, run(3, 2000, 50.0));
    CHECK_FALSE(p.record(0, run(2, 1800, 70.0))); // worse in every way
    CHECK(p.record(0, run(1, 2500, 45.0)));       // better score and time, fewer stars
    REQUIRE(p.best(0));
    CHECK(*p.best(0) == LevelRecord{.stars = 3, .score = 2500, .time = 45.0});
    CHECK(p.totalStars() == 3);
    CHECK(p.suggestedLevel() == 1);
}

TEST_CASE("failed runs and unknown levels are ignored") {
    Progress p(3);
    CHECK_FALSE(p.record(0, run(0, 0, 0.0)));
    CHECK_FALSE(p.record(7, run(3, 100, 1.0)));
    CHECK_FALSE(p.best(0));
}

TEST_CASE("progress survives a save and load") {
    Progress p(10);
    p.record(0, run(3, 2345, 41.25));
    p.record(1, run(2, 1999, 77.5));
    p.soundOn = false;

    const Progress loaded = Progress::parse(p.serialize(), 10);
    CHECK_FALSE(loaded.soundOn);
    CHECK(loaded.best(0) == p.best(0));
    CHECK(loaded.best(1) == p.best(1));
    CHECK_FALSE(loaded.best(2));
    CHECK(loaded.unlocked(2));
    CHECK(loaded.totalStars() == 5);
}

TEST_CASE("damaged or foreign save files never break the game") {
    CHECK(Progress::parse("", 10).totalStars() == 0);
    CHECK(Progress::parse("<html>not a save</html>", 10).totalStars() == 0);

    const std::string messy = "orbit-courier-progress 1\n"
                              "level 0 3 2000 50000\n"
                              "level 1 9 2000 50000\n"  // impossible star count
                              "level 2 2 -5 50000\n"    // negative score
                              "level 99 3 2000 50000\n" // no such level
                              "level x y z\n"
                              "something else\n"
                              "level 3 2 1500 61000\n";
    const Progress p = Progress::parse(messy, 10);
    CHECK(p.best(0));
    CHECK_FALSE(p.best(1));
    CHECK_FALSE(p.best(2));
    REQUIRE(p.best(3));
    CHECK(p.best(3)->time == doctest::Approx(61.0));
    CHECK(p.totalStars() == 5);
}

TEST_CASE("a save from a longer campaign loads into a shorter one") {
    Progress big(12);
    big.record(11, run(3, 100, 1.0));
    big.record(0, run(2, 100, 1.0));
    const Progress small = Progress::parse(big.serialize(), 10);
    CHECK(small.levelCount() == 10);
    CHECK(small.totalStars() == 2);
}
