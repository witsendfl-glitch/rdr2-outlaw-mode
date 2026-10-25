#include "property.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "config.h"
#include "mode.h"
#include "ui.h"
#include "util.h"

namespace property {
namespace {

enum Type { kHomestead, kRanch, kSaloon };

struct TypeInfo {
    const char* label;
    int baseIncome, incomePerLevel;  // dollars per real minute
    int upgradeCost[3];
    const char* upgrades[3];
    const char* animal;  // livestock that shows up as you upgrade (nullptr = none)
    const char* worker;
    const char* workScenario;
};

const TypeInfo kTypes[] = {
    {"Homestead", 4, 4, {150, 300, 500},
     {"Plant a vegetable garden", "Build a chicken coop and pig pen", "Build a smokehouse"},
     "a_c_chicken_01", "a_m_m_rancher_01", "WORLD_HUMAN_SMOKE"},
    {"Ranch", 8, 8, {400, 800, 1500},
     {"Buy a cattle herd", "Build a horse breeding corral", "Hire more ranch hands"},
     "a_c_cow", "a_m_m_rancher_01", "WORLD_HUMAN_STAND_WAITING"},
    {"Saloon", 6, 7, {300, 600, 1200},
     {"Restock with good whiskey", "Add card tables", "Hire a piano player and rent rooms"},
     nullptr, "a_m_m_valtownfolk_01", "WORLD_HUMAN_DRINKING"},
};

struct Place {
    const char* key;
    const char* name;
    Type type;
    int price;
    float x, y, z;
};

// Coordinates are town / site centres from rdr2mods.com's teleport list.
const Place kPlaces[] = {
    {"veterans", "Veteran's Homestead", kHomestead, 600, 1695.54f, 1517.76f, 146.75f},
    {"ridgewood", "Ridgewood Farm", kHomestead, 750, -4823.54f, -2711.38f, -13.75f},
    {"emerald", "Emerald Ranch", kRanch, 2000, 1418.27f, 316.29f, 88.49f},
    {"macfarlane", "MacFarlane's Ranch", kRanch, 2500, -2300.93f, -2450.02f, 61.97f},
    {"vanhorn", "Old Light Saloon, Van Horn", kSaloon, 1500, 2983.45f, 430.15f, 51.18f},
    {"tumbleweed", "Tumbleweed Saloon", kSaloon, 1200, -5517.38f, -2936.82f, -2.22f},
};
constexpr int kCount = sizeof(kPlaces) / sizeof(kPlaces[0]);
constexpr float kNearRadius = 45.0f;
constexpr float kSpawnRadius = 160.0f;
constexpr float kDespawnRadius = 240.0f;

struct State {
    bool owned = false;
    int level = 0;
    int bank = 0;
    Blip blip = 0;
    std::vector<Ped> peds;
    bool spawned = false;
};
State g_s[kCount];
bool g_loaded = false;
int g_nextTick = 0, g_nextIncome = 0;

Vector3 Center(int i) { return V3(kPlaces[i].x, kPlaces[i].y, kPlaces[i].z); }
const TypeInfo& TypeOf(int i) { return kTypes[kPlaces[i].type]; }
int Income(int i) { return TypeOf(i).baseIncome + TypeOf(i).incomePerLevel * g_s[i].level; }
int Cap(int i) { return 120 * (g_s[i].level + 1) * (kPlaces[i].type == kRanch ? 2 : 1); }

void Save(int i) {
    if (!g_s[i].owned) {
        config::Remove("property", kPlaces[i].key);
        return;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d,%d", g_s[i].level, g_s[i].bank);
    config::Set("property", kPlaces[i].key, buf);
}

void Load() {
    for (int i = 0; i < kCount; ++i) {
        std::string v = config::Get("property", kPlaces[i].key);
        int level = 0, bank = 0;
        if (std::sscanf(v.c_str(), "%d,%d", &level, &bank) == 2) {
            g_s[i].owned = true;
            g_s[i].level = level;
            g_s[i].bank = bank;
        }
    }
    g_loaded = true;
}

void RefreshBlip(int i) {
    State& s = g_s[i];
    if (s.blip && DOES_BLIP_EXIST(s.blip)) REMOVE_BLIP(&s.blip);
    const Place& p = kPlaces[i];
    s.blip = BLIP_ADD_FOR_COORDS(util::Joaat("BLIP_STYLE_CREATOR_DEFAULT"), p.x, p.y, p.z);
    SET_BLIP_SPRITE(s.blip, util::Joaat(s.owned ? "blip_camp_tent" : "blip_shop_store"), TRUE);
    std::string name = s.owned ? std::string("Your ") + TypeOf(i).label + ": " + p.name
                               : std::string("For sale: ") + p.name + " ($" + std::to_string(p.price) + ")";
    SET_BLIP_NAME(s.blip, name.c_str());
    if (s.owned) BLIP_ADD_MODIFIER(s.blip, util::Joaat("BLIP_MODIFIER_FRIENDLY"));
}

Ped SpawnAt(const char* modelName, const Vector3& c, float angleDeg, float radius) {
    Hash model = util::Joaat(modelName);
    if (!util::LoadModel(model)) return 0;
    float a = angleDeg * 0.0174533f;
    float x = c.x + std::cos(a) * radius, y = c.y + std::sin(a) * radius;
    Ped p = CREATE_PED(model, x, y, util::GroundZ(x, y, c.z), angleDeg, FALSE, FALSE, FALSE, FALSE);
    SET_MODEL_AS_NO_LONGER_NEEDED(model);
    if (!p) return 0;
    SET_RANDOM_OUTFIT_VARIATION(p, TRUE);
    SET_ENTITY_AS_MISSION_ENTITY(p, TRUE, TRUE);
    return p;
}

void Spawn(int i) {
    State& s = g_s[i];
    const TypeInfo& t = TypeOf(i);
    Vector3 c = Center(i);
    int workers = 1 + s.level;
    for (int w = 0; w < workers; ++w) {
        if (Ped p = SpawnAt(t.worker, c, 40.0f + 70.0f * w, 8.0f + 2.0f * w)) {
            SET_PED_RELATIONSHIP_GROUP_HASH(p, GET_PED_RELATIONSHIP_GROUP_HASH(PLAYER_PED_ID()));
            TASK_START_SCENARIO_IN_PLACE_HASH(p, util::Joaat(t.workScenario), -1, TRUE, 0, -1.0f, FALSE);
            s.peds.push_back(p);
        }
    }
    if (t.animal && s.level > 0) {
        int animals = 3 * s.level;
        for (int a = 0; a < animals; ++a) {
            if (Ped p = SpawnAt(t.animal, c, 200.0f + 25.0f * a, 18.0f + 2.0f * (a % 3))) {
                TASK_WANDER_IN_AREA(p, c.x - 15.0f, c.y - 10.0f, c.z, 12.0f, 3.0f, 8.0f, 0);
                s.peds.push_back(p);
            }
        }
    }
    s.spawned = true;
}

void Despawn(int i) {
    for (Ped& p : g_s[i].peds) {
        if (p && DOES_ENTITY_EXIST(p)) DELETE_PED(&p);
    }
    g_s[i].peds.clear();
    g_s[i].spawned = false;
}

void PlacePage(int i) {
    const Place& p = kPlaces[i];
    State& s = g_s[i];
    const TypeInfo& t = TypeOf(i);
    ui::Title(p.name);
    bool here = util::Dist(util::PlayerPos(), Center(i)) < kNearRadius;
    if (!s.owned) {
        ui::Text(std::string(t.label) + " for sale: $" + std::to_string(p.price));
        ui::Text("Earns $" + std::to_string(t.baseIncome) + " a minute to start.");
        if (!here) {
            ui::Text("Ride out there to buy it.");
        } else if (ui::Item("Buy it", "$" + std::to_string(p.price)) && util::Charge(p.price, p.name)) {
            s.owned = true;
            s.level = 0;
            s.bank = 0;
            Save(i);
            config::Save();
            RefreshBlip(i);
            Despawn(i);
            ui::Notify(std::string("The deed to ") + p.name + " is yours.", 7000);
        }
    } else {
        ui::Text("Level " + std::to_string(s.level) + "   Earns $" + std::to_string(Income(i)) + " a minute");
        ui::Text("Strongbox: $" + std::to_string(s.bank) + " / $" + std::to_string(Cap(i)));
        if (here && s.bank > 0 && ui::Item("Collect earnings", "$" + std::to_string(s.bank))) {
            MONEY_INCREMENT_CASH_BALANCE(s.bank * 100, util::Joaat("ADD_REASON_DEFAULT"));
            ui::Notify("Collected $" + std::to_string(s.bank) + ".");
            s.bank = 0;
            Save(i);
            config::Save();
        }
        if (s.level < 3) {
            int cost = t.upgradeCost[s.level];
            if (!here) {
                ui::Text("Next: " + std::string(t.upgrades[s.level]) + " ($" + std::to_string(cost) + ")");
            } else if (ui::Item(t.upgrades[s.level], "$" + std::to_string(cost)) && util::Charge(cost, "the upgrade")) {
                s.level++;
                Save(i);
                config::Save();
                Despawn(i);  // respawn with more hands and stock
                ui::Notify(std::string(p.name) + " is now level " + std::to_string(s.level) + ".");
            }
        } else {
            ui::Text("Fully built up.");
        }
    }
    if (ui::Item("Mark it on my map")) {
        START_GPS_MULTI_ROUTE(util::Joaat("COLOR_YELLOW"), TRUE, TRUE);
        ADD_POINT_TO_GPS_MULTI_ROUTE(p.x, p.y, p.z, FALSE);
        SET_GPS_MULTI_ROUTE_RENDER(TRUE);
    }
}

}  // namespace

int CollectAll() {
    int total = 0;
    for (int i = 0; i < kCount; ++i) {
        if (!g_s[i].owned) continue;
        total += g_s[i].bank;
        g_s[i].bank = 0;
        Save(i);
    }
    config::Save();
    return total;
}

void Tick() {
    if (!g_loaded) Load();
    if (!mode::Enabled()) {
        for (int i = 0; i < kCount; ++i) {
            if (g_s[i].spawned) Despawn(i);
            if (g_s[i].blip && DOES_BLIP_EXIST(g_s[i].blip)) REMOVE_BLIP(&g_s[i].blip);
        }
        return;
    }
    int now = util::Now();
    if (now < g_nextTick) return;
    g_nextTick = now + 1000;

    if (g_nextIncome == 0) g_nextIncome = now + 60000;
    bool earn = now >= g_nextIncome;
    if (earn) g_nextIncome = now + 60000;

    Vector3 pos = util::PlayerPos();
    for (int i = 0; i < kCount; ++i) {
        State& s = g_s[i];
        if (!s.blip || !DOES_BLIP_EXIST(s.blip)) RefreshBlip(i);
        if (!s.owned) continue;
        if (earn && s.bank < Cap(i)) {
            s.bank += Income(i);
            if (s.bank > Cap(i)) s.bank = Cap(i);
            Save(i);
        }
        // A reload wipes our peds; rebuild.
        if (s.spawned && !s.peds.empty() && !DOES_ENTITY_EXIST(s.peds[0])) {
            s.peds.clear();
            s.spawned = false;
        }
        float d = util::Dist(pos, Center(i));
        if (!s.spawned && d < kSpawnRadius) Spawn(i);
        else if (s.spawned && d > kDespawnRadius) Despawn(i);
    }
    if (earn) config::Save();
}

void MenuPage() {
    ui::Title("Properties");
    for (int i = 0; i < kCount; ++i) {
        std::string right = g_s[i].owned ? "Yours, lvl " + std::to_string(g_s[i].level)
                                         : "$" + std::to_string(kPlaces[i].price);
        if (ui::Item(kPlaces[i].name, right)) ui::Push([i] { PlacePage(i); });
    }
}

}  // namespace property
