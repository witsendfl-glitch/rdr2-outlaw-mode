#include "camp.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "config.h"
#include "mode.h"
#include "places.h"
#include "ui.h"
#include "util.h"

namespace camp {
namespace {

constexpr float kAliveRadius = 180.0f;
constexpr float kDespawnRadius = 260.0f;
constexpr float kRestRadius = 15.0f;
constexpr float kPi = 3.14159265f;

struct Prop {
    const char* model;
    float angle;   // degrees around the fire
    float radius;
    float facing;  // added to the "face the fire" heading
};

const Prop kProps[] = {
    {"p_campfire01x", 0, 0, 0},
    {"p_ambtentscrub01b", 20, 8.0f, 180},
    {"p_ambtentburlap01b", 110, 8.0f, 180},
    {"p_ambtentoilskin01b", 200, 8.0f, 180},
    {"p_ambtentplaid01b", 290, 8.0f, 180},
    {"p_bedrollopen01x", 60, 4.5f, 90},
    {"p_bedrollopen01x", 160, 4.5f, 90},
    {"p_bedrollopen01x", 250, 4.5f, 90},
};

std::vector<Object> g_objects;
Blip g_blip = 0;
bool g_spawned = false;
int g_nextCheck = 0;

float CX() { return config::GetFloat("camp", "x"); }
float CY() { return config::GetFloat("camp", "y"); }
float CZ() { return config::GetFloat("camp", "z"); }

Vector3 Around(float angleDeg, float radius) {
    float a = angleDeg * kPi / 180.0f;
    float x = CX() + std::cos(a) * radius;
    float y = CY() + std::sin(a) * radius;
    return V3(x, y, util::GroundZ(x, y, CZ()));
}

float FacingCenter(const Vector3& from) {
    float dx = CX() - from.x, dy = CY() - from.y;
    // Game headings: 0 = north (+y), increasing counter-clockwise.
    return std::atan2(-dx, dy) * 180.0f / kPi;
}

void Despawn() {
    for (Object& o : g_objects) {
        if (o && DOES_ENTITY_EXIST(o)) DELETE_OBJECT(&o);
    }
    g_objects.clear();
    if (g_blip && DOES_BLIP_EXIST(g_blip)) REMOVE_BLIP(&g_blip);
    g_blip = 0;
    g_spawned = false;
}

void Spawn() {
    for (const Prop& p : kProps) {
        Hash model = util::Joaat(p.model);
        if (!util::LoadModel(model)) {
            util::Log(std::string("Camp: could not load ") + p.model);
            continue;
        }
        Vector3 at = p.radius > 0 ? Around(p.angle, p.radius) : V3(CX(), CY(), util::GroundZ(CX(), CY(), CZ()));
        Object o = CREATE_OBJECT(model, at.x, at.y, at.z, FALSE, FALSE, FALSE, FALSE, FALSE);
        SET_MODEL_AS_NO_LONGER_NEEDED(model);
        if (!o) continue;
        SET_ENTITY_HEADING(o, FacingCenter(at) + p.facing);
        PLACE_OBJECT_ON_GROUND_PROPERLY(o, TRUE);
        FREEZE_ENTITY_POSITION(o, TRUE);
        g_objects.push_back(o);
    }
    if (g_blip && DOES_BLIP_EXIST(g_blip)) REMOVE_BLIP(&g_blip);
    g_blip = BLIP_ADD_FOR_COORDS(util::Joaat("BLIP_STYLE_CAMP"), CX(), CY(), CZ());
    SET_BLIP_SPRITE(g_blip, util::Joaat("blip_camp_tent"), TRUE);
    SET_BLIP_NAME(g_blip, "Gang Camp");
    g_spawned = true;
}

void PitchAt(float x, float y, float z, const std::string& where) {
    Despawn();
    config::SetInt("camp", "active", 1);
    config::SetFloat("camp", "x", x);
    config::SetFloat("camp", "y", y);
    config::SetFloat("camp", "z", z);
    config::Save();
    g_nextCheck = 0;
    ui::Notify("Camp pitched at " + where + ".");
}

struct Spot {
    std::string name;
    float x, y, z;
};

std::vector<Spot> SavedSpots() {
    std::vector<Spot> out;
    for (const std::string& key : config::Keys("camp_spots")) {
        std::string v = config::Get("camp_spots", key);
        Spot s{key, 0, 0, 0};
        if (std::sscanf(v.c_str(), "%f,%f,%f", &s.x, &s.y, &s.z) == 3) out.push_back(s);
    }
    return out;
}

void SpotsPage() {
    ui::Title("Move camp to...");
    for (const Place& p : places::kCampSpots) {
        if (ui::Item(p.name)) {
            PitchAt(p.x, p.y, p.z, p.name);
            ui::Pop();
        }
    }
    for (const Spot& s : SavedSpots()) {
        if (ui::Item(s.name, "saved")) {
            PitchAt(s.x, s.y, s.z, s.name);
            ui::Pop();
        }
    }
}

}  // namespace

bool Active() { return config::GetInt("camp", "active", 0) != 0; }

Vector3 Center() { return V3(CX(), CY(), CZ()); }

Vector3 Slot(int i, float* heading) {
    float angle = 15.0f + static_cast<float>(i) * (360.0f / 22.0f);
    Vector3 at = Around(angle, 3.0f + static_cast<float>(i % 2) * 1.2f);
    if (heading) *heading = FacingCenter(at);
    return at;
}

bool PlayerNearby() {
    return Active() && util::Dist(util::PlayerPos(), Center()) < kAliveRadius;
}

void Tick() {
    if (!mode::Enabled() || !Active()) {
        if (g_spawned) Despawn();
        return;
    }
    int now = util::Now();
    if (now < g_nextCheck) return;
    g_nextCheck = now + 1000;

    // A reloaded save wipes our props; notice and rebuild.
    if (g_spawned && (g_objects.empty() || !DOES_ENTITY_EXIST(g_objects.front()))) Despawn();
    if (g_spawned && !DOES_BLIP_EXIST(g_blip)) Despawn();

    float d = util::Dist(util::PlayerPos(), Center());
    if (!g_spawned && d < kAliveRadius) Spawn();
    else if (g_spawned && d > kDespawnRadius) Despawn();

    // Keep the map blip even when far away.
    if (!g_spawned && (!g_blip || !DOES_BLIP_EXIST(g_blip))) {
        g_blip = BLIP_ADD_FOR_COORDS(util::Joaat("BLIP_STYLE_CAMP"), CX(), CY(), CZ());
        SET_BLIP_SPRITE(g_blip, util::Joaat("blip_camp_tent"), TRUE);
        SET_BLIP_NAME(g_blip, "Gang Camp");
    }
}

void MenuPage() {
    ui::Title("Camp");
    Vector3 pos = util::PlayerPos();
    if (Active()) {
        int dist = static_cast<int>(util::Dist(pos, Center()));
        ui::Text("Your camp is " + std::to_string(dist) + "m away.");
    } else {
        ui::Text("You haven't pitched a camp yet.");
    }

    if (ui::Item(Active() ? "Move camp here" : "Pitch camp here")) {
        Vector3 f = GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(PLAYER_PED_ID(), 0.0f, 5.0f, 0.0f);
        PitchAt(f.x, f.y, util::GroundZ(f.x, f.y, f.z), "your location");
    }
    if (ui::Item("Move camp to a known spot...")) ui::Push(SpotsPage);
    if (ui::Item("Save this spot for later")) {
        int n = static_cast<int>(SavedSpots().size()) + 1;
        char value[96];
        std::snprintf(value, sizeof(value), "%.2f,%.2f,%.2f", pos.x, pos.y, pos.z);
        std::string name = "My spot " + std::to_string(n);
        config::Set("camp_spots", name, value);
        config::Save();
        ui::Notify("Saved as \"" + name + "\". You can rename it in outlawmode.ini.");
    }
    if (!Active()) return;

    if (util::Dist(pos, Center()) > 120.0f && ui::Item("Ride back to camp")) {
        ui::Close();
        Vector3 c = Center();
        util::Teleport(c.x + 6.0f, c.y + 6.0f, c.z, 0.0f);
    }
    if (util::Dist(pos, Center()) < kRestRadius && ui::Item("Rest by the fire (6 hours)")) {
        ui::Close();
        DO_SCREEN_FADE_OUT(1000);
        int deadline = util::Now() + 2500;
        while (!IS_SCREEN_FADED_OUT() && util::Now() < deadline) WAIT(0);
        ADD_TO_CLOCK_TIME(6, 0, 0);
        Ped p = PLAYER_PED_ID();
        for (int core = 0; core < 3; ++core) SET_ATTRIBUTE_CORE_VALUE(p, core, 100);
        SET_ENTITY_HEALTH(p, GET_ENTITY_MAX_HEALTH(p, FALSE), 0);
        WAIT(1500);
        DO_SCREEN_FADE_IN(1000);
        ui::Notify("You feel rested.");
    }
    if (ui::Item("Strike camp")) {
        Despawn();
        config::SetInt("camp", "active", 0);
        config::Save();
        ui::Notify("Camp packed up. Anyone waiting there will meet you when you pitch a new one.");
    }
}

}  // namespace camp
