#pragma once

#include <string>

namespace game {

/// Persistent key-value text storage: localStorage in the browser, a file next to the game on desktop.
/// Both functions are best-effort: failures mean "nothing saved", never a crash.
std::string loadText(const std::string& key);
void saveText(const std::string& key, const std::string& text);

} // namespace game
