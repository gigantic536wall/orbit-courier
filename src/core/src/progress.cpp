#include "oc/progress.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>

namespace oc {

namespace {

constexpr const char* kHeader = "orbit-courier-progress 1";

template <typename T> bool parseNumber(const std::string& token, T& out) {
    const char* end = token.data() + token.size();
    const auto [ptr, ec] = std::from_chars(token.data(), end, out);
    return ec == std::errc{} && ptr == end;
}

} // namespace

Progress::Progress(std::size_t levelCount) : records_(levelCount) {}

bool Progress::record(std::size_t level, const Result& result) {
    if (level >= records_.size() || result.stars <= 0) {
        return false;
    }
    auto& slot = records_[level];
    if (!slot) {
        slot = LevelRecord{.stars = result.stars, .score = result.score, .time = result.time};
        return true;
    }
    const LevelRecord before = *slot;
    slot->stars = std::max(slot->stars, result.stars);
    slot->score = std::max(slot->score, result.score);
    slot->time = std::min(slot->time, result.time);
    return *slot != before;
}

bool Progress::unlocked(std::size_t level) const {
    if (level >= records_.size()) {
        return false;
    }
    return level == 0 || records_[level - 1].has_value();
}

int Progress::totalStars() const {
    int total = 0;
    for (const auto& r : records_) {
        total += r ? r->stars : 0;
    }
    return total;
}

std::size_t Progress::suggestedLevel() const {
    for (std::size_t i = 0; i < records_.size(); ++i) {
        if (unlocked(i) && (!records_[i] || records_[i]->stars < 3)) {
            return i;
        }
    }
    return records_.empty() ? 0 : records_.size() - 1;
}

std::string Progress::serialize() const {
    std::ostringstream out;
    out << kHeader << '\n';
    out << "sound " << (soundOn ? 1 : 0) << '\n';
    for (std::size_t i = 0; i < records_.size(); ++i) {
        if (const auto& r = records_[i]) {
            // Time in milliseconds keeps the file free of locale-dependent decimal separators.
            out << "level " << i << ' ' << r->stars << ' ' << r->score << ' '
                << static_cast<long long>(std::llround(r->time * 1000.0)) << '\n';
        }
    }
    return out.str();
}

Progress Progress::parse(const std::string& text, std::size_t levelCount) {
    Progress progress(levelCount);
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line != kHeader) {
        return progress; // unknown format: start fresh
    }
    while (std::getline(in, line)) {
        std::istringstream words(line);
        std::string key;
        words >> key;
        if (key == "sound") {
            std::string value;
            words >> value;
            progress.soundOn = value != "0";
        } else if (key == "level") {
            std::string a, b, c, d;
            words >> a >> b >> c >> d;
            std::size_t index = 0;
            int stars = 0;
            int score = 0;
            long long millis = 0;
            if (parseNumber(a, index) && parseNumber(b, stars) && parseNumber(c, score) && parseNumber(d, millis) &&
                index < levelCount && stars >= 1 && stars <= 3 && score >= 0 && millis >= 0) {
                progress.records_[index] =
                    LevelRecord{.stars = stars, .score = score, .time = static_cast<double>(millis) / 1000.0};
            }
        }
    }
    return progress;
}

} // namespace oc
