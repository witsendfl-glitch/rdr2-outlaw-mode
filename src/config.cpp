#include "config.h"

#include <shlobj.h>
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <utility>
#include <vector>

namespace config {
namespace {

using Section = std::vector<std::pair<std::string, std::string>>;
std::vector<std::pair<std::string, Section>> g_data;

Section& FindSection(const std::string& name) {
    for (auto& s : g_data) {
        if (s.first == name) return s.second;
    }
    g_data.emplace_back(name, Section{});
    return g_data.back().second;
}

std::string Trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string IniPath() { return Folder() + "\\outlawmode.ini"; }

}  // namespace

std::string Folder() {
    static std::string folder;
    if (folder.empty()) {
        char docs[MAX_PATH] = {};
        // CSIDL_PERSONAL follows OneDrive's Documents redirection.
        if (SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, 0, docs) != S_OK) {
            GetCurrentDirectoryA(MAX_PATH, docs);
        }
        folder = std::string(docs) + "\\RDR2 Outlaw Mode";
        CreateDirectoryA(folder.c_str(), nullptr);
    }
    return folder;
}

std::string LogPath() { return Folder() + "\\outlawmode.log"; }

void Load() {
    g_data.clear();
    std::ifstream in(IniPath());
    std::string line, current = "general";
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            current = Trim(line.substr(1, line.size() - 2));
            FindSection(current);
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        Set(current, Trim(line.substr(0, eq)), Trim(line.substr(eq + 1)));
    }
}

void Save() {
    std::ofstream out(IniPath(), std::ios::trunc);
    out << "; RDR2 Outlaw Mode save data. Safe to edit while the game is closed.\n";
    for (auto& s : g_data) {
        out << "\n[" << s.first << "]\n";
        for (auto& kv : s.second) out << kv.first << " = " << kv.second << "\n";
    }
}

std::string Get(const std::string& section, const std::string& key, const std::string& fallback) {
    for (auto& kv : FindSection(section)) {
        if (kv.first == key) return kv.second;
    }
    return fallback;
}

int GetInt(const std::string& section, const std::string& key, int fallback) {
    std::string v = Get(section, key);
    return v.empty() ? fallback : std::atoi(v.c_str());
}

float GetFloat(const std::string& section, const std::string& key, float fallback) {
    std::string v = Get(section, key);
    return v.empty() ? fallback : static_cast<float>(std::atof(v.c_str()));
}

void Set(const std::string& section, const std::string& key, const std::string& value) {
    Section& s = FindSection(section);
    for (auto& kv : s) {
        if (kv.first == key) {
            kv.second = value;
            return;
        }
    }
    s.emplace_back(key, value);
}

void SetInt(const std::string& section, const std::string& key, int value) {
    Set(section, key, std::to_string(value));
}

void SetFloat(const std::string& section, const std::string& key, float value) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.3f", value);
    Set(section, key, buf);
}

void Remove(const std::string& section, const std::string& key) {
    Section& s = FindSection(section);
    for (auto it = s.begin(); it != s.end(); ++it) {
        if (it->first == key) {
            s.erase(it);
            return;
        }
    }
}

std::vector<std::string> Keys(const std::string& section) {
    std::vector<std::string> out;
    for (auto& kv : FindSection(section)) out.push_back(kv.first);
    return out;
}

}  // namespace config
