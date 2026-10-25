// Outlaw Mode's own save data, kept apart from the game's saves in
// Documents\RDR2 Outlaw Mode\outlawmode.ini.
#pragma once
#include <string>
#include <vector>

namespace config {

void Load();
void Save();  // writes immediately

std::string Get(const std::string& section, const std::string& key, const std::string& fallback = "");
int GetInt(const std::string& section, const std::string& key, int fallback = 0);
float GetFloat(const std::string& section, const std::string& key, float fallback = 0.0f);

void Set(const std::string& section, const std::string& key, const std::string& value);
void SetInt(const std::string& section, const std::string& key, int value);
void SetFloat(const std::string& section, const std::string& key, float value);
void Remove(const std::string& section, const std::string& key);

// Keys present in a section, in file order.
std::vector<std::string> Keys(const std::string& section);

std::string Folder();
std::string LogPath();

}  // namespace config
