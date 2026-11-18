#include "storage.hpp"

#if defined(__EMSCRIPTEN__)
#include <cstdlib>

#include <emscripten/emscripten.h>

// clang-format off
EM_JS(char*, oc_storage_load, (const char* key), {
    try {
        const value = window.localStorage.getItem(UTF8ToString(key));
        if (value === null) return 0;
        return stringToNewUTF8(value);
    } catch (e) {
        return 0; // storage disabled (private mode, blocked cookies)
    }
});

EM_JS(void, oc_storage_save, (const char* key, const char* text), {
    try {
        window.localStorage.setItem(UTF8ToString(key), UTF8ToString(text));
    } catch (e) {
    }
});
// clang-format on

namespace game {

std::string loadText(const std::string& key) {
    char* raw = oc_storage_load(key.c_str());
    if (raw == nullptr) {
        return {};
    }
    std::string text(raw);
    std::free(raw);
    return text;
}

void saveText(const std::string& key, const std::string& text) { oc_storage_save(key.c_str(), text.c_str()); }

} // namespace game

#else
#include <cstdio>
#include <fstream>
#include <sstream>

#include <raylib.h>

namespace game {

namespace {

std::string pathFor(const std::string& key) { return std::string(GetApplicationDirectory()) + key + ".txt"; }

} // namespace

std::string loadText(const std::string& key) {
    std::ifstream in(pathFor(key), std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void saveText(const std::string& key, const std::string& text) {
    // Write to a temporary file first, so a crash mid-write never leaves a half-written save.
    const std::string path = pathFor(key);
    const std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!(out << text)) {
            return;
        }
    }
    std::remove(path.c_str());
    std::rename(temp.c_str(), path.c_str());
}

} // namespace game
#endif
