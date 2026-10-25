#include "colter.h"

#include <string>
#include <vector>

#include "mode.h"
#include "places.h"
#include "ui.h"
#include "util.h"

namespace colter {
namespace {

constexpr float kSpawnRadius = 220.0f;
constexpr float kDespawnRadius = 320.0f;
constexpr float kStoreRadius = 5.0f;

struct Resident {
    const char* model;
    float dx, dy;          // offset from Colter's centre
    const char* scenario;  // nullptr = wander around town
};

const Resident kStorekeeper = {"u_m_m_valgenstoreowner_01", 4.0f, 6.0f, "WORLD_HUMAN_STAND_WAITING"};
const Resident kResidents[] = {
    {"a_m_m_grisurvivalist_01", 9.0f, -2.0f, "WORLD_CAMP_FIRE_STANDING"},
    {"a_m_m_griroughtravellers_01", 11.0f, 1.5f, "WORLD_CAMP_FIRE_STANDING"},
    {"a_f_m_valtownfolk_01", 7.5f, 1.0f, "WORLD_HUMAN_COFFEE_DRINK"},
    {"a_m_m_rancher_01", -6.0f, 8.0f, "WORLD_HUMAN_SMOKE"},
    {"a_m_m_rhdtownfolk_01", -10.0f, -6.0f, nullptr},
    {"a_f_m_rhdtownfolk_01", 14.0f, 10.0f, nullptr},
    {"a_m_m_ranchertravelers_cool_01", -3.0f, -12.0f, nullptr},
};

std::vector<Ped> g_peds;
Ped g_keeper = 0;
Object g_fire = 0;
Blip g_storeBlip = 0;
bool g_spawned = false;
int g_nextCheck = 0;

Vector3 Center() { return V3(places::kColter.x, places::kColter.y, places::kColter.z); }

Ped SpawnResident(const Resident& r) {
    Hash model = util::Joaat(r.model);
    if (!util::LoadModel(model)) {
        util::Log(std::string("Colter: could not load ") + r.model);
        return 0;
    }
    Vector3 c = Center();
    float x = c.x + r.dx, y = c.y + r.dy;
    float z = util::GroundZ(x, y, c.z);
    Ped ped = CREATE_PED(model, x, y, z, 0.0f, FALSE, FALSE, FALSE, FALSE);
    SET_MODEL_AS_NO_LONGER_NEEDED(model);
    if (!ped) return 0;
    SET_RANDOM_OUTFIT_VARIATION(ped, TRUE);
    SET_ENTITY_AS_MISSION_ENTITY(ped, TRUE, TRUE);
    if (r.scenario) {
        TASK_START_SCENARIO_IN_PLACE_HASH(ped, util::Joaat(r.scenario), -1, TRUE, 0, -1.0f, FALSE);
    } else {
        TASK_WANDER_IN_AREA(ped, c.x, c.y, c.z, 25.0f, 5.0f, 10.0f, 0);
    }
    return ped;
}

void Spawn() {
    Vector3 c = Center();
    Hash fireModel = util::Joaat("p_campfire01x");
    if (util::LoadModel(fireModel)) {
        float fx = c.x + 9.5f, fy = c.y;
        g_fire = CREATE_OBJECT(fireModel, fx, fy, util::GroundZ(fx, fy, c.z), FALSE, FALSE, FALSE, FALSE, FALSE);
        PLACE_OBJECT_ON_GROUND_PROPERLY(g_fire, TRUE);
        SET_MODEL_AS_NO_LONGER_NEEDED(fireModel);
    }

    g_keeper = SpawnResident(kStorekeeper);
    if (g_keeper) {
        FREEZE_ENTITY_POSITION(g_keeper, TRUE);
        SET_PED_PROMPT_NAME(g_keeper, "Colter Storekeeper");
        g_storeBlip = BLIP_ADD_FOR_ENTITY(util::Joaat("BLIP_STYLE_SHOP"), g_keeper);
        SET_BLIP_SPRITE(g_storeBlip, util::Joaat("blip_shop_store"), TRUE);
        SET_BLIP_NAME(g_storeBlip, "Colter General Store");
    }
    for (const Resident& r : kResidents) {
        if (Ped p = SpawnResident(r)) g_peds.push_back(p);
    }
    g_spawned = true;
    util::Log("Colter populated");
}

void Despawn() {
    for (Ped& p : g_peds) {
        if (DOES_ENTITY_EXIST(p)) DELETE_PED(&p);
    }
    g_peds.clear();
    if (g_keeper && DOES_ENTITY_EXIST(g_keeper)) DELETE_PED(&g_keeper);
    g_keeper = 0;
    if (g_fire && DOES_ENTITY_EXIST(g_fire)) DELETE_OBJECT(&g_fire);
    g_fire = 0;
    if (g_storeBlip && DOES_BLIP_EXIST(g_storeBlip)) REMOVE_BLIP(&g_storeBlip);
    g_storeBlip = 0;
    g_spawned = false;
}

void BuyAmmo(const char* label, const char* ammoType, int amount, int price) {
    std::string right = "$" + std::to_string(price);
    if (ui::Item(std::string(label) + " x" + std::to_string(amount), right) && util::Charge(price, label)) {
        ADD_AMMO_TO_PED_BY_TYPE(PLAYER_PED_ID(), util::Joaat(ammoType), amount, util::Joaat("ADD_REASON_DEFAULT"));
        ui::Notify(std::string("Bought ") + label + ".");
    }
}

}  // namespace

void Tick() {
    if (!mode::Enabled()) {
        if (g_spawned) Despawn();
        return;
    }
    int now = util::Now();
    if (now < g_nextCheck) return;
    g_nextCheck = now + 1000;

    float d = util::Dist(util::PlayerPos(), Center());
    // Loading a save wipes our peds; respawn if the keeper vanished.
    if (g_spawned && (!g_keeper || !DOES_ENTITY_EXIST(g_keeper))) Despawn();
    if (!g_spawned && d < kSpawnRadius) Spawn();
    else if (g_spawned && d > kDespawnRadius) Despawn();
}

bool NearStore() {
    if (!g_keeper || !DOES_ENTITY_EXIST(g_keeper) || IS_ENTITY_DEAD(g_keeper)) return false;
    return util::Dist(util::PlayerPos(), GET_ENTITY_COORDS(g_keeper, TRUE, FALSE)) < kStoreRadius;
}

void StorePage() {
    ui::Title("Colter General Store");
    if (!NearStore()) {
        ui::Text("Walk up to the storekeeper first.");
        return;
    }
    BuyAmmo("Revolver ammo", "AMMO_REVOLVER", 50, 5);
    BuyAmmo("Pistol ammo", "AMMO_PISTOL", 50, 5);
    BuyAmmo("Repeater ammo", "AMMO_REPEATER", 50, 6);
    BuyAmmo("Rifle ammo", "AMMO_RIFLE", 40, 8);
    BuyAmmo("Shotgun shells", "AMMO_SHOTGUN", 30, 8);
    BuyAmmo("Arrows", "AMMO_ARROW", 20, 3);
    if (ui::Item("Hot meal (fills Health and Stamina)", "$3") && util::Charge(3, "a hot meal")) {
        Ped p = PLAYER_PED_ID();
        SET_ATTRIBUTE_CORE_VALUE(p, 0, 100);
        SET_ATTRIBUTE_CORE_VALUE(p, 1, 100);
        SET_ENTITY_HEALTH(p, GET_ENTITY_MAX_HEALTH(p, FALSE), 0);
        ui::Notify("That hit the spot.");
    }
    if (ui::Item("Snake oil (fills Dead Eye)", "$6") && util::Charge(6, "snake oil")) {
        SET_ATTRIBUTE_CORE_VALUE(PLAYER_PED_ID(), 2, 100);
        ui::Notify("Your eyes sharpen.");
    }
}

}  // namespace colter
