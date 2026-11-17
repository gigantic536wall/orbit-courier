#include <doctest/doctest.h>

#include <cmath>
#include <stdexcept>

#include "oc/levels.hpp"
#include "oc/world.hpp"

using namespace oc;

namespace {

constexpr double kMu = 500'000.0;
constexpr double kRadius = 100.0;

World singlePlanet(Ship ship) {
    return World({Body{.name = "P", .radius = kRadius, .mu = kMu, .center = {0.0, 0.0}}}, ship);
}

Ship circularShip(double r) {
    return Ship{.position = {r, 0.0}, .velocity = {0.0, circularSpeed(kMu, r)}, .angle = kPi / 2.0};
}

int stepsFor(double seconds) { return static_cast<int>(std::lround(seconds / World::kStep)); }

} // namespace

TEST_CASE("gravity points at the planet and follows the inverse-square law") {
    const World world = singlePlanet(circularShip(300.0));
    const Vec2 a = world.gravityAt({200.0, 0.0}, 0.0);
    CHECK(a.x == doctest::Approx(-kMu / (200.0 * 200.0)));
    CHECK(a.y == doctest::Approx(0.0));

    const Vec2 far = world.gravityAt({400.0, 0.0}, 0.0);
    CHECK(far.length() == doctest::Approx(a.length() / 4.0));
}

TEST_CASE("a circular orbit stays circular and closes after one period") {
    constexpr double r = 250.0;
    World world = singlePlanet(circularShip(r));
    const double period = kTau * r / circularSpeed(kMu, r);
    const Vec2 start = world.ship().position;

    double minR = r;
    double maxR = r;
    for (int i = 0; i < stepsFor(period); ++i) {
        world.step({});
        const double now = world.ship().position.length();
        minR = std::min(minR, now);
        maxR = std::max(maxR, now);
    }

    CHECK(world.status() == Status::Flying);
    CHECK(maxR - minR < r * 0.001);
    CHECK((world.ship().position - start).length() < r * 0.01);
}

TEST_CASE("energy is conserved over many orbits (symplectic integrator)") {
    constexpr double r = 250.0;
    Ship ship = circularShip(r);
    ship.velocity = ship.velocity * 1.15; // eccentric orbit: the hard case for integrators
    World world = singlePlanet(ship);

    auto energy = [](const World& w) {
        return 0.5 * w.ship().velocity.lengthSquared() - kMu / w.ship().position.length();
    };
    const double e0 = energy(world);
    for (int i = 0; i < stepsFor(600.0); ++i) { // ~10 laps
        world.step({});
    }
    REQUIRE(world.status() == Status::Flying);
    CHECK(std::abs((energy(world) - e0) / e0) < 1e-3);
}

TEST_CASE("the engine accelerates along the heading and burns fuel") {
    World world({}, Ship{.angle = 0.0, .fuel = 1.0});
    for (int i = 0; i < stepsFor(1.0); ++i) {
        world.step({.thrust = true});
    }
    CHECK(world.ship().velocity.x == doctest::Approx(world.spec().thrust).epsilon(0.01));
    CHECK(world.ship().fuel < 1.0);
    CHECK(world.ship().velocity.y == doctest::Approx(0.0));
    CHECK(world.ship().fuel == doctest::Approx(1.0 - world.spec().fuelPerSecond).epsilon(0.01));
}

TEST_CASE("an empty tank means no thrust") {
    World world({}, Ship{.fuel = 0.0});
    world.step({.thrust = true});
    CHECK_FALSE(world.thrusting());
    CHECK(world.ship().velocity == Vec2{});
}

TEST_CASE("turning changes the heading at the configured rate") {
    World world({}, Ship{.angle = 0.0});
    for (int i = 0; i < stepsFor(0.5); ++i) {
        world.step({.turn = 1});
    }
    CHECK(world.ship().angle == doctest::Approx(world.spec().turnRate * 0.5));
}

TEST_CASE("falling into the planet is a crash and freezes the simulation") {
    World world = singlePlanet(Ship{.position = {300.0, 0.0}});
    for (int i = 0; i < stepsFor(30.0) && world.status() == Status::Flying; ++i) {
        world.step({});
    }
    REQUIRE(world.status() == Status::Crashed);
    CHECK(world.crashedInto() == 0u);
    CHECK(world.ship().position.length() < kRadius + world.spec().radius + 2.0);

    const Ship frozen = world.ship();
    world.step({.thrust = true});
    CHECK(world.ship().position == frozen.position);
}

TEST_CASE("moons move on circular rails around their parent") {
    const World world({Body{.name = "P", .radius = 50.0, .mu = kMu, .center = {10.0, 20.0}},
                       Body{.name = "M", .radius = 10.0, .parent = 0, .orbitRadius = 300.0, .angularSpeed = 0.5}},
                      Ship{.position = {5000.0, 0.0}});
    const Vec2 atStart = world.bodyPosition(1, 0.0);
    CHECK(atStart.x == doctest::Approx(310.0));
    CHECK(atStart.y == doctest::Approx(20.0));

    const Vec2 quarter = world.bodyPosition(1, (kPi / 2.0) / 0.5);
    CHECK(quarter.x == doctest::Approx(10.0));
    CHECK(quarter.y == doctest::Approx(320.0));
}

TEST_CASE("bodies must be listed after their parent") {
    CHECK_THROWS_AS(World({Body{.name = "M", .parent = 0}}, Ship{}), std::invalid_argument);
}

TEST_CASE("the trajectory preview matches the real flight") {
    World world = makeSandbox();
    const auto preview = world.predictTrajectory(100, 4);
    REQUIRE(preview.size() == 100);
    for (int i = 0; i < 400; ++i) {
        world.step({});
    }
    CHECK(world.ship().position == preview.back());
}

TEST_CASE("the trajectory preview stops at a crash") {
    const World world = singlePlanet(Ship{.position = {300.0, 0.0}});
    const auto preview = world.predictTrajectory(10'000, 4);
    CHECK(preview.size() < 10'000);
    CHECK(preview.back().length() < kRadius + 10.0);
}

TEST_CASE("the simulation is deterministic") {
    World a = makeSandbox();
    World b = makeSandbox();
    for (int i = 0; i < 5000; ++i) {
        const Input input{.turn = (i / 200) % 3 - 1, .thrust = (i / 150) % 2 == 0};
        a.step(input);
        b.step(input);
    }
    CHECK(a.ship().position == b.ship().position);
    CHECK(a.ship().velocity == b.ship().velocity);
    CHECK(a.ship().fuel == b.ship().fuel);
}

TEST_CASE("the sandbox starts in a stable orbit") {
    World world = makeSandbox();
    for (int i = 0; i < stepsFor(60.0); ++i) {
        world.step({});
    }
    CHECK(world.status() == Status::Flying);
}
