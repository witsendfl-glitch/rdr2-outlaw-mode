// Small helpers shared by every module.
#pragma once
#include <string>
#include "natives.h"

namespace util {

Hash Joaat(const char* name);
int Now();  // game timer, milliseconds

float Dist(const Vector3& a, const Vector3& b);
Vector3 PlayerPos();
// The player's horse if riding, otherwise the player.
Entity PlayerOrMount();

// Loads a model; returns false if it is invalid or did not load in time.
bool LoadModel(Hash model, int timeoutMs = 5000);

// Snaps z to the ground at (x, y). Leaves z alone if no ground was found.
float GroundZ(float x, float y, float zHint);

// Fades out, moves the player (and their horse), waits for the area to load,
// fades back in. Followers are moved by the gang module through OnTeleport.
void Teleport(float x, float y, float z, float heading);

// Charges the player; returns false (and notifies) if they can't afford it.
// Amounts are in dollars.
bool Charge(int dollars, const char* what);

void Log(const std::string& line);

}  // namespace util
