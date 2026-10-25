#include "util.h"

#include <cmath>
#include <cstdio>

#include "config.h"
#include "ui.h"

namespace util {

Hash Joaat(const char* name) { return GET_HASH_KEY(name); }

int Now() { return GET_GAME_TIMER(); }

float Dist(const Vector3& a, const Vector3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

Vector3 PlayerPos() { return GET_ENTITY_COORDS(PLAYER_PED_ID(), TRUE, FALSE); }

Entity PlayerOrMount() {
    Ped player = PLAYER_PED_ID();
    if (IS_PED_ON_MOUNT(player)) return GET_MOUNT(player);
    return player;
}

bool LoadModel(Hash model, int timeoutMs) {
    if (!IS_MODEL_VALID(model)) return false;
    REQUEST_MODEL(model, FALSE);
    int deadline = Now() + timeoutMs;
    while (!HAS_MODEL_LOADED(model)) {
        if (Now() > deadline) return false;
        WAIT(0);
    }
    return true;
}

float GroundZ(float x, float y, float zHint) {
    float ground = 0.0f;
    if (GET_GROUND_Z_FOR_3D_COORD(x, y, zHint + 50.0f, &ground, FALSE)) return ground;
    return zHint;
}

void Teleport(float x, float y, float z, float heading) {
    DO_SCREEN_FADE_OUT(800);
    int deadline = Now() + 3000;
    while (!IS_SCREEN_FADED_OUT() && Now() < deadline) WAIT(0);

    Entity target = PlayerOrMount();
    SET_ENTITY_COORDS(target, x, y, z, FALSE, FALSE, FALSE, FALSE);
    SET_ENTITY_HEADING(target, heading);

    // Give the world a moment to stream in so we don't fall through it.
    deadline = Now() + 2500;
    while (Now() < deadline) {
        REQUEST_COLLISION_AT_COORD(x, y, z);
        WAIT(0);
    }
    float ground = GroundZ(x, y, z);
    if (std::fabs(ground - z) < 30.0f) {
        SET_ENTITY_COORDS(target, x, y, ground, FALSE, FALSE, FALSE, FALSE);
    }
    DO_SCREEN_FADE_IN(800);
}

bool Charge(int dollars, const char* what) {
    int cents = dollars * 100;
    if (MONEY_GET_CASH_BALANCE() < cents) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "You can't afford %s ($%d).", what, dollars);
        ui::Notify(buf);
        return false;
    }
    MONEY_DECREMENT_CASH_BALANCE(cents);
    return true;
}

void Log(const std::string& line) {
    FILE* f = std::fopen(config::LogPath().c_str(), "a");
    if (!f) return;
    std::fprintf(f, "%s\n", line.c_str());
    std::fclose(f);
}

}  // namespace util
