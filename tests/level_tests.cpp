#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "oc/autopilot.hpp"
#include "oc/levels.hpp"
#include "oc/mission.hpp"

using namespace oc;

TEST_CASE("the campaign has ten levels with unique names") {
    const auto levels = campaign();
    REQUIRE(levels.size() == 10);
    for (std::size_t i = 0; i < levels.size(); ++i) {
        for (std::size_t j = i + 1; j < levels.size(); ++j) {
            CHECK(levels[i].name != levels[j].name);
        }
    }
}

TEST_CASE("every campaign level is well formed") {
    for (const Level& level : campaign()) {
        CAPTURE(level.name);
        CHECK_FALSE(level.briefing.empty());
        CHECK(level.fuel > 0.0);
        CHECK(level.fuel <= 1.0);
        CHECK(level.par.fuelLeft < level.fuel);
        CHECK(level.par.time > 0.0);
        CHECK_FALSE(level.jobs.empty());
        CHECK(level.jobs.front().from == level.startStation);

        const Mission mission(level); // throws on broken references
        const World& w = mission.world();
        for (std::size_t s = 0; s < level.stations.size(); ++s) {
            CAPTURE(level.stations[s].name);
            const Body& parent = level.bodies[level.stations[s].parent];
            // Stations orbit well above the surface of their body...
            CHECK(level.stations[s].orbitRadius > parent.radius * 1.4);
            // ...and never pass through any other body during the first ten minutes.
            for (double t = 0.0; t < 600.0; t += 0.5) {
                for (std::size_t b = 0; b < level.bodies.size(); ++b) {
                    const double gap = (w.stationPosition(s, t) - w.bodyPosition(b, t)).length();
                    REQUIRE(gap > level.bodies[b].radius + 20.0);
                }
            }
        }
    }
}

TEST_CASE("every level starts in a safe orbit: coasting for 30 s neither crashes nor leaves the system") {
    for (const Level& level : campaign()) {
        CAPTURE(level.name);
        Mission mission(level);
        for (int i = 0; i < static_cast<int>(30.0 / World::kStep); ++i) {
            mission.step({});
        }
        CHECK(mission.state() == MissionState::Flying);
    }
}

TEST_CASE("fuel budgets are generous compared to an ideal transfer") {
    // For legs between stations around the same body, compare the tank with the textbook Hohmann
    // transfer (plus a full reversal for retrograde targets). Players are not ideal pilots, so ask for 3x.
    for (const Level& level : campaign()) {
        CAPTURE(level.name);
        const Mission mission(level);
        double ideal = 0.0;
        bool comparable = true;
        for (const Job& job : level.jobs) {
            const Station& a = level.stations[job.from];
            const Station& b = level.stations[job.to];
            if (a.parent != b.parent) {
                comparable = false;
                break;
            }
            const double mu = level.bodies[a.parent].mu;
            ideal += hohmannDeltaV(mu, a.orbitRadius, b.orbitRadius);
            if ((a.angularSpeed > 0) != (b.angularSpeed > 0)) {
                ideal += circularSpeed(mu, a.orbitRadius) + circularSpeed(mu, b.orbitRadius);
            }
        }
        if (comparable) {
            const double available = level.fuel * mission.world().spec().deltaVPerTank();
            CHECK(available >= 3.0 * ideal);
            // The third star still has to leave a margin over the ideal pilot.
            CHECK((level.fuel - level.par.fuelLeft) * mission.world().spec().deltaVPerTank() >= 1.5 * ideal);
        }
    }
}

TEST_CASE("Hohmann transfer matches the textbook value") {
    // LEO (6678 km) to GEO (42164 km) around Earth: about 3.9 km/s.
    CHECK(hohmannDeltaV(398600.0, 6678.0, 42164.0) == doctest::Approx(3.893).epsilon(0.005));
    CHECK(hohmannDeltaV(398600.0, 7000.0, 7000.0) == doctest::Approx(0.0));
}

namespace {

Level levelNamed(const std::string& name) {
    const auto levels = campaign();
    const auto it = std::find_if(levels.begin(), levels.end(), [&](const Level& l) { return l.name == name; });
    REQUIRE(it != levels.end());
    return *it;
}

} // namespace

TEST_CASE("transfer levels can be won by an ideal Hohmann transfer with three stars") {
    for (const std::string name : {"First Delivery", "Down the Well"}) {
        CAPTURE(name);
        const Mission mission = flyHohmann(levelNamed(name));
        REQUIRE(mission.state() == MissionState::Complete);
        CHECK(mission.result().stars == 3);
    }
}

TEST_CASE("Express is designed so that the slow textbook transfer misses the par time") {
    const Level level = levelNamed("Express");
    const Mission mission = flyHohmann(level);
    REQUIRE(mission.state() == MissionState::Complete);
    CHECK(mission.result().time > level.par.time);
    CHECK(mission.result().fuelLeft >= level.par.fuelLeft); // fuel is not the problem: speed is
    CHECK(mission.result().stars == 2);
}
