#include "oc/mission.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace oc {

namespace {

World makeWorld(const Level& level) {
    if (level.startStation >= level.stations.size()) {
        throw std::invalid_argument("Level " + level.name + ": start station does not exist");
    }
    // Build once without a ship to read the start station's state, then put the ship there.
    const World probe(level.bodies, level.stations, Ship{});
    const Vec2 velocity = probe.stationVelocity(level.startStation, 0.0);
    const Ship ship{
        .position = probe.stationPosition(level.startStation, 0.0),
        .velocity = velocity,
        .angle = std::atan2(velocity.y, velocity.x), // facing prograde
        .fuel = level.fuel,
    };
    return World(level.bodies, level.stations, ship);
}

} // namespace

Result scoreFor(const Level& level, double time, double fuelLeft) {
    Result r{.time = time, .fuelLeft = fuelLeft};
    r.stars = 1 + (time <= level.par.time ? 1 : 0) + (fuelLeft >= level.par.fuelLeft ? 1 : 0);
    const double timeBonus = std::max(0.0, level.par.time - time) * 10.0;
    r.score = static_cast<int>(level.jobs.size()) * 1000 + static_cast<int>(std::lround(fuelLeft * 1000.0)) +
              static_cast<int>(std::lround(timeBonus));
    return r;
}

Mission::Mission(Level level) : level_(std::move(level)), world_(makeWorld(level_)) {
    for (const Job& job : level_.jobs) {
        if (job.from >= level_.stations.size() || job.to >= level_.stations.size() || job.from == job.to) {
            throw std::invalid_argument("Level " + level_.name + ": invalid job");
        }
        route_.push_back({job.from, StopKind::Pickup});
        route_.push_back({job.to, StopKind::Dropoff});
    }
    // The first parcel is usually waiting where the ship starts.
    if (!route_.empty() && route_.front().station == level_.startStation) {
        arriveAt(route_.front());
        lastEvent_.reset();
    }
}

std::optional<Stop> Mission::currentStop() const {
    if (next_ < route_.size()) {
        return route_[next_];
    }
    return std::nullopt;
}

void Mission::arriveAt(const Stop& stop) {
    const double t = world_.time();
    const auto type = stop.kind == StopKind::Pickup ? MissionEvent::Type::PickedUp : MissionEvent::Type::Delivered;
    cargo_ += stop.kind == StopKind::Pickup ? 1 : -1;
    lastEvent_ = MissionEvent{.type = type, .station = stop.station, .time = t};
    ++next_;
    if (next_ == route_.size()) {
        state_ = MissionState::Complete;
        finishTime_ = t;
    }
}

void Mission::step(const Input& input) {
    if (state_ != MissionState::Flying) {
        return;
    }
    world_.step(input);

    if (world_.status() == Status::Crashed) {
        state_ = MissionState::Crashed;
        return;
    }
    if (world_.ship().position.length() > level_.boundary) {
        state_ = MissionState::Lost;
        return;
    }

    const auto stop = currentStop();
    if (!stop) {
        return;
    }
    const double t = world_.time();
    const Vec2 stationVelocity = world_.stationVelocity(stop->station, t);
    const double distance = (world_.ship().position - world_.stationPosition(stop->station, t)).length();
    if (distance > kDockRadius) {
        return;
    }
    const double speed = (world_.ship().velocity - stationVelocity).length();
    if (speed <= kMaxDockSpeed) {
        world_.matchVelocity(stationVelocity);
        arriveAt(*stop);
    } else if (t >= tooFastUntil_) {
        lastEvent_ = MissionEvent{.type = MissionEvent::Type::TooFast, .station = stop->station, .time = t, .speed = speed};
        tooFastUntil_ = t + 1.0;
    }
}

Result Mission::result() const { return scoreFor(level_, finishTime_, world_.ship().fuel); }

} // namespace oc
