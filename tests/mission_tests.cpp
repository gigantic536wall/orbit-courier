#include <doctest/doctest.h>

#include <cmath>
#include <stdexcept>

#include "oc/levels.hpp"
#include "oc/mission.hpp"

using namespace oc;

namespace {

constexpr double kMu = 500'000.0;

std::vector<Body> onePlanet() { return {Body{.name = "P", .radius = 100.0, .mu = kMu}}; }

Station orbiting(std::string name, double r, double phase, double direction = 1.0) {
    return Station{.name = std::move(name),
                   .parent = 0,
                   .orbitRadius = r,
                   .angularSpeed = direction * orbitalAngularSpeed(kMu, r),
                   .phase = phase};
}

Level levelWith(std::vector<Station> stations, std::vector<Job> jobs) {
    return Level{.name = "Test",
                 .bodies = onePlanet(),
                 .stations = std::move(stations),
                 .fuel = 0.5,
                 .jobs = std::move(jobs),
                 .par = {.time = 60.0, .fuelLeft = 0.3}};
}

} // namespace

TEST_CASE("station velocity is the derivative of its position") {
    const World world(onePlanet(), {orbiting("S", 300.0, 0.7)}, Ship{});
    constexpr double t = 12.3;
    constexpr double h = 1e-4;
    const Vec2 numeric = (world.stationPosition(0, t + h) - world.stationPosition(0, t - h)) / (2.0 * h);
    const Vec2 analytic = world.stationVelocity(0, t);
    CHECK(analytic.x == doctest::Approx(numeric.x).epsilon(1e-6));
    CHECK(analytic.y == doctest::Approx(numeric.y).epsilon(1e-6));
    CHECK(analytic.length() == doctest::Approx(circularSpeed(kMu, 300.0)));
}

TEST_CASE("moon velocities include the parent's motion") {
    std::vector<Body> bodies = onePlanet();
    bodies.push_back(Body{.name = "M", .radius = 10.0, .parent = 0, .orbitRadius = 400.0, .angularSpeed = 0.1});
    const World world(bodies, {Station{.name = "S", .parent = 1, .orbitRadius = 50.0, .angularSpeed = 0.5}}, Ship{});
    // At t = 0 the moon moves +y at 40, and the station moves +y at 25 relative to the moon.
    CHECK(world.bodyVelocity(1, 0.0).y == doctest::Approx(40.0));
    CHECK(world.stationVelocity(0, 0.0).y == doctest::Approx(65.0));
}

TEST_CASE("the ship starts docked at the start station, facing prograde, parcel on board") {
    const Mission mission(levelWith({orbiting("A", 300.0, 0.0), orbiting("B", 400.0, 1.0)}, {{0, 1}}));
    const World& w = mission.world();
    CHECK(w.ship().position.x == doctest::Approx(300.0));
    CHECK(w.ship().velocity.y == doctest::Approx(circularSpeed(kMu, 300.0)));
    CHECK(w.ship().angle == doctest::Approx(kPi / 2.0));
    CHECK(w.ship().fuel == doctest::Approx(0.5));
    CHECK(mission.cargo() == 1);
    REQUIRE(mission.currentStop());
    CHECK(mission.currentStop()->station == 1);
    CHECK(mission.currentStop()->kind == StopKind::Dropoff);
    CHECK_FALSE(mission.lastEvent());
}

TEST_CASE("docking slowly next to the target delivers the parcel and completes the level") {
    // B flies in formation with A, 10 units ahead: the ship is already within docking range.
    const double tiny = 10.0 / 300.0;
    Mission mission(levelWith({orbiting("A", 300.0, 0.0), orbiting("B", 300.0, tiny)}, {{0, 1}}));
    mission.step({});
    CHECK(mission.state() == MissionState::Complete);
    REQUIRE(mission.lastEvent());
    CHECK(mission.lastEvent()->type == MissionEvent::Type::Delivered);
    CHECK(mission.cargo() == 0);
    CHECK(mission.result().stars == 3);

    const Ship before = mission.world().ship();
    mission.step({.thrust = true});
    CHECK(mission.world().ship().position == before.position); // finished missions stay frozen
}

TEST_CASE("arriving too fast does not count, and says why") {
    // B orbits the other way: relative speed is twice the orbital speed.
    Mission mission(levelWith({orbiting("A", 300.0, 0.0), orbiting("B", 300.0, 0.0, -1.0)}, {{0, 1}}));
    mission.step({});
    CHECK(mission.state() == MissionState::Flying);
    REQUIRE(mission.lastEvent());
    CHECK(mission.lastEvent()->type == MissionEvent::Type::TooFast);
    CHECK(mission.lastEvent()->speed == doctest::Approx(2.0 * circularSpeed(kMu, 300.0)).epsilon(0.01));
    CHECK(mission.cargo() == 1);
}

TEST_CASE("multi-job routes alternate pickups and drop-offs") {
    const Mission mission(levelWith({orbiting("A", 300.0, 0.0), orbiting("B", 400.0, 1.0), orbiting("C", 500.0, 2.0)},
                                    {{0, 1}, {2, 0}}));
    REQUIRE(mission.route().size() == 4);
    CHECK(mission.route()[2].station == 2);
    CHECK(mission.route()[2].kind == StopKind::Pickup);
    CHECK(mission.stopsDone() == 1);
}

TEST_CASE("leaving the system loses the ship") {
    Level level = levelWith({orbiting("A", 300.0, 0.0), orbiting("B", 400.0, 1.0)}, {{0, 1}});
    level.boundary = 310.0;
    level.fuel = 1.0;
    Mission mission(level);
    for (int i = 0; i < 600 && mission.state() == MissionState::Flying; ++i) {
        mission.step({.thrust = true});
    }
    CHECK(mission.state() == MissionState::Lost);
}

TEST_CASE("crashing ends the mission") {
    Level level = levelWith({orbiting("A", 300.0, 0.0), orbiting("B", 400.0, 1.0)}, {{0, 1}});
    level.fuel = 1.0;
    Mission mission(level);
    // Point at the planet's centre and burn.
    for (int i = 0; i < 6000 && mission.state() == MissionState::Flying; ++i) {
        const Vec2 p = mission.world().ship().position;
        const double error = std::remainder(mission.world().ship().angle - std::atan2(-p.y, -p.x), kTau);
        mission.step({.turn = std::abs(error) > 0.05 ? (error > 0 ? -1 : 1) : 0, .thrust = std::abs(error) < 0.3});
    }
    CHECK(mission.state() == MissionState::Crashed);
}

TEST_CASE("scoring: deliveries, fuel and time bonus; stars for par time and par fuel") {
    Level level = levelWith({orbiting("A", 300.0, 0.0), orbiting("B", 400.0, 1.0)}, {{0, 1}, {1, 0}});
    level.par = {.time = 100.0, .fuelLeft = 0.3};

    const Result best = scoreFor(level, 80.0, 0.4);
    CHECK(best.stars == 3);
    CHECK(best.score == 2000 + 400 + 200);

    CHECK(scoreFor(level, 120.0, 0.4).stars == 2);
    CHECK(scoreFor(level, 80.0, 0.1).stars == 2);
    CHECK(scoreFor(level, 120.0, 0.1).stars == 1);
    CHECK(scoreFor(level, 120.0, 0.1).score == 2000 + 100);
}

TEST_CASE("closest approach finds the rendezvous on the coasting path") {
    // Ship on a slightly larger orbit than the station: it drifts behind and the gap only grows.
    const World world(onePlanet(), {orbiting("S", 300.0, 0.0)},
                      Ship{.position = {300.0, 0.0}, .velocity = {0.0, circularSpeed(kMu, 300.0)}});
    const Approach now = world.closestApproach(0, 30.0);
    CHECK(now.distance == doctest::Approx(0.0).epsilon(1e-9));
    CHECK(now.time == doctest::Approx(0.0));

    // Station a quarter turn ahead on a lower, faster orbit... ship coasting on a transfer ellipse.
    const World chase(onePlanet(), {orbiting("S", 300.0, 0.3)},
                      Ship{.position = {300.0, 0.0}, .velocity = {0.0, circularSpeed(kMu, 300.0) * 1.05}});
    const Approach later = chase.closestApproach(0, 60.0);
    CHECK(later.time > 0.0);
    CHECK(later.distance < (chase.ship().position - chase.stationPosition(0, 0.0)).length());
}

TEST_CASE("invalid levels are rejected") {
    CHECK_THROWS_AS(Mission{levelWith({orbiting("A", 300.0, 0.0)}, {{0, 0}})}, std::invalid_argument);
    CHECK_THROWS_AS(Mission{levelWith({orbiting("A", 300.0, 0.0)}, {{0, 5}})}, std::invalid_argument);
    Level noStart = levelWith({orbiting("A", 300.0, 0.0)}, {});
    noStart.startStation = 3;
    CHECK_THROWS_AS(Mission{noStart}, std::invalid_argument);
}
