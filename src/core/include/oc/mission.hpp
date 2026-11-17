#pragma once

#include <optional>
#include <string>
#include <vector>

#include "oc/world.hpp"

namespace oc {

/// Carry one parcel from station `from` to station `to`.
struct Job {
    std::size_t from = 0;
    std::size_t to = 0;
};

/// Targets for the second and third star.
struct Par {
    double time = 0.0;     ///< Finish within this many seconds.
    double fuelLeft = 0.0; ///< Finish with at least this much fuel in the tank.
};

struct Level {
    std::string name;
    std::string briefing;
    std::vector<Body> bodies;
    std::vector<Station> stations;
    std::size_t startStation = 0; ///< The ship starts docked here.
    double fuel = 1.0;
    std::vector<Job> jobs;
    Par par;
    double boundary = 2500.0; ///< Flying further from the origin means the ship is lost.
};

enum class StopKind { Pickup, Dropoff };

struct Stop {
    std::size_t station = 0;
    StopKind kind = StopKind::Pickup;
};

enum class MissionState { Flying, Complete, Crashed, Lost };

struct MissionEvent {
    enum class Type { PickedUp, Delivered, TooFast };
    Type type = Type::PickedUp;
    std::size_t station = 0;
    double time = 0.0;
    double speed = 0.0; ///< Relative speed at the station.
};

struct Result {
    int score = 0;
    int stars = 0;
    double time = 0.0;
    double fuelLeft = 0.0;
};

/// Points and stars for a finished level. Pure function, so the rules are easy to test and to show.
[[nodiscard]] Result scoreFor(const Level& level, double time, double fuelLeft);

/// One attempt at a level: the world plus the delivery rules.
class Mission {
public:
    static constexpr double kDockRadius = 22.0;   ///< Distance to the station for docking.
    static constexpr double kMaxDockSpeed = 15.0; ///< Highest relative speed that still counts as docking.

    explicit Mission(Level level);

    void step(const Input& input);

    [[nodiscard]] const World& world() const { return world_; }
    [[nodiscard]] const Level& level() const { return level_; }
    [[nodiscard]] MissionState state() const { return state_; }
    [[nodiscard]] const std::vector<Stop>& route() const { return route_; }
    [[nodiscard]] std::size_t stopsDone() const { return next_; }
    [[nodiscard]] std::optional<Stop> currentStop() const;
    [[nodiscard]] int cargo() const { return cargo_; }
    [[nodiscard]] const std::optional<MissionEvent>& lastEvent() const { return lastEvent_; }
    /// Only meaningful once the mission is complete.
    [[nodiscard]] Result result() const;

private:
    void arriveAt(const Stop& stop);

    Level level_;
    World world_;
    std::vector<Stop> route_;
    std::size_t next_ = 0;
    int cargo_ = 0;
    MissionState state_ = MissionState::Flying;
    std::optional<MissionEvent> lastEvent_;
    double tooFastUntil_ = -1.0;
    double finishTime_ = 0.0;
};

} // namespace oc
