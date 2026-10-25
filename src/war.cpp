#include "war.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "camp.h"
#include "config.h"
#include "mode.h"
#include "ui.h"
#include "util.h"

namespace war {
namespace {

// ---------------------------------------------------------------------------
// Factions and territories

enum Faction {
    kYou = 0,
    kItalians,
    kODriscolls,
    kRaiders,
    kMurfree,
    kSkinners,
    kDelLobo,
    kLaramie,
    kNightFolk,
    kFactionCount
};

struct FactionInfo {
    const char* name;
    const char* group;       // our relationship group name
    const char* models[2];
    bool hostile;            // shoots on sight, like in the base game
    const char* color;       // blip colour modifier
};

const FactionInfo kFactions[kFactionCount] = {
    {"Your gang", "", {"g_m_m_unicriminals_01", "g_m_m_unicriminals_02"}, false, "BLIP_MODIFIER_MP_COLOR_8"},
    {"Bronte's Italians", "OM_ITALIANS", {"g_m_m_unibrontegoons_01", "g_m_m_unibrontegoons_01"}, false, "BLIP_MODIFIER_MP_COLOR_2"},
    {"O'Driscoll Boys", "OM_ODRISCOLLS", {"g_m_m_uniduster_01", "g_m_m_uniduster_02"}, true, "BLIP_MODIFIER_MP_COLOR_4"},
    {"Lemoyne Raiders", "OM_RAIDERS", {"g_m_y_uniexconfeds_01", "g_m_o_uniexconfeds_01"}, true, "BLIP_MODIFIER_MP_COLOR_6"},
    {"Murfree Brood", "OM_MURFREE", {"g_m_m_uniinbred_01", "g_m_m_uniinbred_01"}, true, "BLIP_MODIFIER_MP_COLOR_10"},
    {"Skinner Brothers", "OM_SKINNERS", {"g_m_m_unimountainmen_01", "g_m_m_unimountainmen_01"}, true, "BLIP_MODIFIER_MP_COLOR_12"},
    {"Del Lobo Gang", "OM_DELLOBO", {"g_m_m_unibanditos_01", "g_m_m_unibanditos_01"}, true, "BLIP_MODIFIER_MP_COLOR_14"},
    {"Laramie Gang", "OM_LARAMIE", {"g_m_m_uniranchers_01", "g_m_m_uniranchers_01"}, false, "BLIP_MODIFIER_MP_COLOR_16"},
    {"Night Folk", "OM_NIGHTFOLK", {"g_m_m_uniswamp_01", "g_m_m_uniswamp_01"}, true, "BLIP_MODIFIER_MP_COLOR_18"},
};

struct TerritoryInfo {
    const char* key;
    const char* name;
    float x, y, z;
    float radius;
    Faction startOwner;
};

const TerritoryInfo kTerritories[] = {
    {"doyles", "Doyle's Tavern, Saint Denis", 2792.44f, -1176.05f, 47.95f, 35.0f, kItalians},
    {"docks", "Saint Denis Docks", 2327.49f, -1502.42f, 46.16f, 45.0f, kItalians},
    {"fontana", "Fontana Theatre, Saint Denis", 2683.06f, -1365.81f, 47.47f, 35.0f, kItalians},
    {"brewery", "Schiffer Brewery, Saint Denis", 2626.12f, -1219.24f, 53.24f, 35.0f, kItalians},
    {"caliga", "Caliga Hall", 1748.70f, -1373.02f, 44.07f, 55.0f, kItalians},
    {"shadybelle", "Shady Belle", 1763.23f, -1772.34f, 50.68f, 55.0f, kRaiders},
    {"braithwaite", "Braithwaite Manor", 1010.88f, -1741.42f, 46.57f, 60.0f, kRaiders},
    {"lagras", "Lagras", 2105.41f, -682.16f, 42.27f, 60.0f, kNightFolk},
    {"beaverhollow", "Beaver Hollow", 2401.96f, 1237.94f, 102.54f, 50.0f, kMurfree},
    {"annesburgmine", "Annesburg Mine", 2811.51f, 1350.15f, 71.41f, 50.0f, kMurfree},
    {"clemens", "Clemens Point", 843.99f, -1029.49f, 51.99f, 50.0f, kODriscolls},
    {"willards", "Willard's Rest", 2962.59f, 2206.05f, 165.29f, 50.0f, kODriscolls},
    {"barrow", "Barrow Lagoon", -1022.50f, 1682.61f, 242.16f, 50.0f, kODriscolls},
    {"montos", "Monto's Rest", -1389.25f, -235.02f, 98.86f, 45.0f, kSkinners},
    {"swadlass", "Swadlass Point", -2573.16f, -1371.53f, 149.71f, 50.0f, kSkinners},
    {"fortmercer", "Fort Mercer", -4217.85f, -3514.64f, 36.99f, 60.0f, kDelLobo},
    {"rathskeller", "Rathskeller Fork", -5165.08f, -2143.50f, 11.77f, 50.0f, kDelLobo},
    {"gaptooth", "Gaptooth Breach", -6000.77f, -3319.31f, -21.72f, 60.0f, kLaramie},
};
constexpr int kTerritoryCount = sizeof(kTerritories) / sizeof(kTerritories[0]);

constexpr float kSpawnMargin = 130.0f;
constexpr float kDespawnMargin = 230.0f;
constexpr int kWarTickMs = 4 * 60 * 1000;
constexpr int kIncomeTickMs = 60 * 1000;
constexpr int kDefendWindowMs = 5 * 60 * 1000;
constexpr int kContestedMs = 3 * 60 * 1000;
constexpr int kUpgradeCost[] = {150, 300, 600};
constexpr int kMaxLevel = 3;

enum Scene { kNoScene, kOccupied, kAssault, kGuarded, kDefense };

struct Territory {
    Faction owner = kYou;
    int strength = 5;      // AI-held: 1..10
    int level = 0;         // yours: 0..3
    int bank = 0;          // yours: dollars waiting in the strongbox
    int heatUntil = 0;     // neutral gang is angry with you until this time
    int attackDeadline = 0;  // yours: under attack until this time
    Faction attacker = kYou;
    int contestedUntil = 0;
    Faction contester = kYou;

    Scene scene = kNoScene;
    std::vector<Ped> peds;     // occupants / guards
    std::vector<Ped> enemies;  // attackers or skirmishers
    Blip area = 0, point = 0;
};

Territory g_t[kTerritoryCount];
Hash g_groups[kFactionCount] = {};
bool g_loaded = false;
bool g_groupsReady = false;
int g_nextTick = 0, g_nextWar = 0, g_nextIncome = 0;

// ---------------------------------------------------------------------------
// Helpers

const TerritoryInfo& Info(int i) { return kTerritories[i]; }
Vector3 Center(int i) { return V3(Info(i).x, Info(i).y, Info(i).z); }

bool Alive(Ped p) { return p && DOES_ENTITY_EXIST(p) && !IS_ENTITY_DEAD(p); }

int Rand(int lo, int hi) { return lo + std::rand() % (hi - lo + 1); }

int DefenderCount(int i) {
    const Territory& t = g_t[i];
    if (t.owner == kYou) return 2 + 2 * t.level;
    int n = 3 + t.strength / 2;
    return n > 8 ? 8 : n;
}

int Income(int i) { return 5 + 5 * g_t[i].level; }        // dollars per minute
int BankCap(int i) { return 150 * (g_t[i].level + 1); }

bool Neutral(Faction f, const Territory* t = nullptr) {
    if (f == kYou || kFactions[f].hostile) return false;
    return !t || util::Now() >= t->heatUntil;
}

void Save(int i) {
    const Territory& t = g_t[i];
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d,%d,%d,%d", static_cast<int>(t.owner), t.strength, t.level, t.bank);
    config::Set("territories", Info(i).key, buf);
}

void Load() {
    for (int i = 0; i < kTerritoryCount; ++i) {
        Territory& t = g_t[i];
        t.owner = Info(i).startOwner;
        t.strength = 5;
        std::string v = config::Get("territories", Info(i).key);
        int owner, strength, level, bank;
        if (std::sscanf(v.c_str(), "%d,%d,%d,%d", &owner, &strength, &level, &bank) == 4 && owner >= 0 &&
            owner < kFactionCount) {
            t.owner = static_cast<Faction>(owner);
            t.strength = strength;
            t.level = level;
            t.bank = bank;
        }
    }
    g_loaded = true;
}

void SetupGroups() {
    if (g_groupsReady) return;
    Hash player = GET_PED_RELATIONSHIP_GROUP_HASH(PLAYER_PED_ID());
    g_groups[kYou] = player;
    for (int f = 1; f < kFactionCount; ++f) {
        Hash h = 0;
        ADD_RELATIONSHIP_GROUP(kFactions[f].group, &h);
        g_groups[f] = h;
    }
    for (int a = 1; a < kFactionCount; ++a) {
        for (int b = 1; b < kFactionCount; ++b) {
            if (a != b) SET_RELATIONSHIP_BETWEEN_GROUPS(5, g_groups[a], g_groups[b]);
        }
        int toPlayer = kFactions[a].hostile ? 5 : 3;
        SET_RELATIONSHIP_BETWEEN_GROUPS(toPlayer, g_groups[a], player);
        SET_RELATIONSHIP_BETWEEN_GROUPS(toPlayer, player, g_groups[a]);
    }
    g_groupsReady = true;
}

void SetHostileToPlayer(Faction f, bool hostile) {
    if (f == kYou) return;
    int rel = hostile ? 5 : 3;
    SET_RELATIONSHIP_BETWEEN_GROUPS(rel, g_groups[f], g_groups[kYou]);
    SET_RELATIONSHIP_BETWEEN_GROUPS(rel, g_groups[kYou], g_groups[f]);
}

Ped SpawnGunman(Faction f, const Vector3& at, int slot, int total, float spread) {
    Hash model = util::Joaat(kFactions[f].models[slot % 2]);
    if (!util::LoadModel(model)) {
        util::Log(std::string("War: could not load ") + kFactions[f].models[slot % 2]);
        return 0;
    }
    float angle = 6.2831853f * static_cast<float>(slot) / static_cast<float>(total > 0 ? total : 1);
    float r = spread * (0.5f + 0.5f * static_cast<float>(slot % 3) / 2.0f);
    float x = at.x + std::cos(angle) * r, y = at.y + std::sin(angle) * r;
    Ped p = CREATE_PED(model, x, y, util::GroundZ(x, y, at.z), angle * 57.3f, FALSE, FALSE, FALSE, FALSE);
    SET_MODEL_AS_NO_LONGER_NEEDED(model);
    if (!p) return 0;
    SET_RANDOM_OUTFIT_VARIATION(p, TRUE);
    SET_ENTITY_AS_MISSION_ENTITY(p, TRUE, TRUE);
    SET_PED_RELATIONSHIP_GROUP_HASH(p, g_groups[f]);
    SET_PED_COMBAT_ATTRIBUTES(p, 46, TRUE);
    SET_PED_ACCURACY(p, 35);
    SET_PED_SEEING_RANGE(p, 60.0f);
    Hash reason = util::Joaat("ADD_REASON_DEFAULT");
    const char* gun = (slot % 3 == 0) ? "WEAPON_REPEATER_CARBINE" : "WEAPON_REVOLVER_CATTLEMAN";
    GIVE_WEAPON_TO_PED(p, util::Joaat(gun), 200, FALSE, TRUE, 0, FALSE, 0.5f, 1.0f, reason, TRUE, 0.0f, FALSE);
    if (f == kYou) SET_PED_CAN_BE_TARGETTED_BY_PLAYER(p, PLAYER_ID(), FALSE);
    return p;
}

void Idle(Ped p, int slot) {
    static const char* kIdle[] = {"WORLD_HUMAN_SMOKE", "WORLD_HUMAN_STAND_WAITING", "WORLD_CAMP_FIRE_STANDING",
                                  "WORLD_HUMAN_LEAN_BACK_WALL", "WORLD_HUMAN_COFFEE_DRINK"};
    TASK_START_SCENARIO_IN_PLACE_HASH(p, util::Joaat(kIdle[slot % 5]), -1, TRUE, 0, -1.0f, FALSE);
}

void Clear(std::vector<Ped>& peds, bool remove) {
    for (Ped& p : peds) {
        if (!p || !DOES_ENTITY_EXIST(p)) continue;
        if (remove && !IS_ENTITY_DEAD(p)) DELETE_PED(&p);
        else SET_ENTITY_AS_NO_LONGER_NEEDED(&p);
    }
    peds.clear();
}

int CountAlive(const std::vector<Ped>& peds) {
    int n = 0;
    for (Ped p : peds) n += Alive(p) ? 1 : 0;
    return n;
}

void RefreshBlips(int i) {
    Territory& t = g_t[i];
    if (t.area && DOES_BLIP_EXIST(t.area)) REMOVE_BLIP(&t.area);
    if (t.point && DOES_BLIP_EXIST(t.point)) REMOVE_BLIP(&t.point);
    t.area = t.point = 0;
    if (!config::GetInt("war", "show_map", 1)) return;
    const TerritoryInfo& info = Info(i);
    t.area = BLIP_ADD_FOR_RADIUS(util::Joaat("BLIP_STYLE_AREA"), info.x, info.y, info.z, info.radius * 2.0f);
    BLIP_ADD_MODIFIER(t.area, util::Joaat(kFactions[t.owner].color));
    t.point = BLIP_ADD_FOR_COORDS(util::Joaat("BLIP_STYLE_CREATOR_DEFAULT"), info.x, info.y, info.z);
    SET_BLIP_SPRITE(t.point, util::Joaat(t.owner == kYou ? "blip_camp_tent" : "blip_region_hideout"), TRUE);
    std::string label = std::string(kFactions[t.owner].name) + ": " + info.name;
    SET_BLIP_NAME(t.point, label.c_str());
    BLIP_ADD_MODIFIER(t.point, util::Joaat(kFactions[t.owner].color));
}

void RefreshAllBlips() {
    for (int i = 0; i < kTerritoryCount; ++i) RefreshBlips(i);
}

void EndScene(int i, bool removePeds) {
    Territory& t = g_t[i];
    Clear(t.peds, removePeds);
    Clear(t.enemies, removePeds);
    t.scene = kNoScene;
}

void ChangeOwner(int i, Faction newOwner, int strength) {
    Territory& t = g_t[i];
    t.owner = newOwner;
    t.strength = strength;
    if (newOwner != kYou) {
        t.level = 0;
        t.bank = 0;
    }
    t.attackDeadline = 0;
    Save(i);
    config::Save();
    RefreshBlips(i);
}

// ---------------------------------------------------------------------------
// Scenes: what's physically spawned at a territory while you're nearby

void StartScene(int i) {
    Territory& t = g_t[i];
    Vector3 c = Center(i);
    int n = DefenderCount(i);
    float spread = Info(i).radius * 0.45f;
    for (int s = 0; s < n; ++s) {
        if (Ped p = SpawnGunman(t.owner, c, s, n, spread)) {
            Idle(p, s);
            t.peds.push_back(p);
        }
    }
    // A pending attack on your hideout, or a gang war skirmish nearby.
    int now = util::Now();
    if (t.owner == kYou && t.attackDeadline > now) {
        int m = 3 + Rand(1, 3);
        Vector3 from = V3(c.x + Info(i).radius * 1.6f, c.y, c.z);
        for (int s = 0; s < m; ++s) {
            if (Ped p = SpawnGunman(t.attacker, from, s, m, 8.0f)) {
                TASK_COMBAT_HATED_TARGETS_IN_AREA(p, c.x, c.y, c.z, Info(i).radius * 2.0f, 0, 0);
                t.enemies.push_back(p);
            }
        }
        ui::Notify(std::string(kFactions[t.attacker].name) + " are hitting " + Info(i).name + "! Drive them off.");
        t.scene = kDefense;
        return;
    }
    if (t.owner != kYou && t.contestedUntil > now && t.contester != t.owner) {
        int m = 3 + Rand(0, 2);
        Vector3 from = V3(c.x - Info(i).radius * 1.4f, c.y, c.z);
        for (int s = 0; s < m; ++s) {
            if (Ped p = SpawnGunman(t.contester, from, s, m, 8.0f)) {
                TASK_COMBAT_HATED_TARGETS_IN_AREA(p, c.x, c.y, c.z, Info(i).radius * 2.0f, 0, 0);
                t.enemies.push_back(p);
            }
        }
    }
    t.scene = (t.owner == kYou) ? kGuarded : kOccupied;
}

void BeginAssault(int i) {
    Territory& t = g_t[i];
    if (t.owner == kYou || t.scene == kAssault) return;
    t.heatUntil = util::Now() + 30 * 60 * 1000;
    SetHostileToPlayer(t.owner, true);
    Ped player = PLAYER_PED_ID();
    for (Ped p : t.peds) {
        if (Alive(p)) TASK_COMBAT_PED(p, player, 0, 16);
    }
    t.scene = kAssault;
}

void TickScene(int i, float dist, int now) {
    Territory& t = g_t[i];
    const TerritoryInfo& info = Info(i);

    if (t.scene == kNoScene) {
        if (dist < info.radius + kSpawnMargin) StartScene(i);
        return;
    }
    if (dist > info.radius + kDespawnMargin) {
        if (t.owner != kYou) {
            // Losses stick: a half-cleared hideout is weaker next time.
            int lost = static_cast<int>(t.peds.size()) - CountAlive(t.peds);
            t.strength -= lost / 2;
            if (t.strength < 1) t.strength = 1;
            Save(i);
        }
        if (t.owner != kYou && Neutral(t.owner)) SetHostileToPlayer(t.owner, false);
        EndScene(i, true);
        return;
    }

    // Hostile gangs open fire once you're inside their turf.
    if (t.scene == kOccupied && !Neutral(t.owner, &t) && dist < info.radius) BeginAssault(i);

    // Everyone at an enemy hideout is down: it's yours.
    if ((t.scene == kOccupied || t.scene == kAssault) && !t.peds.empty() && CountAlive(t.peds) == 0 &&
        dist < info.radius + 60.0f) {
        Faction old = t.owner;
        EndScene(i, false);
        ChangeOwner(i, kYou, 0);
        t.level = 0;
        t.bank = 0;
        Save(i);
        config::Save();
        ui::Notify("You took " + std::string(info.name) + " from " + kFactions[old].name + "!", 7000);
        ui::Notify("It's your hideout now. Upgrade it from Territories > Here.", 7000);
        return;
    }

    // Defending your hideout.
    if (t.scene == kDefense && CountAlive(t.enemies) == 0) {
        g_t[i].attackDeadline = 0;
        int a = -1;
        for (int j = 0; j < kTerritoryCount; ++j) {
            if (g_t[j].owner == t.attacker) a = j;
        }
        if (a >= 0) {
            g_t[a].strength -= 2;
            if (g_t[a].strength < 1) g_t[a].strength = 1;
            Save(a);
        }
        ui::Notify("You held " + std::string(info.name) + ".", 6000);
        Clear(t.enemies, false);
        t.scene = kGuarded;
    }
    if (t.scene == kDefense && !t.peds.empty() && CountAlive(t.peds) == 0 && CountAlive(t.enemies) > 0 &&
        dist > info.radius * 2.5f) {
        // Guards dead and you ran: it falls.
        Faction winner = t.attacker;
        EndScene(i, false);
        ChangeOwner(i, winner, 3);
        ui::Notify(std::string(kFactions[winner].name) + " took " + info.name + " from you.", 7000);
    }
}

// ---------------------------------------------------------------------------
// The wider war, resolved off-screen

int NearestRival(int from) {
    int best = -1;
    float bestD = 2600.0f;
    for (int j = 0; j < kTerritoryCount; ++j) {
        if (j == from || g_t[j].owner == g_t[from].owner) continue;
        float d = util::Dist(Center(from), Center(j));
        if (d < bestD) {
            bestD = d;
            best = j;
        }
    }
    return best;
}

void WarTick(int now) {
    // Gangs rebuild a little each round.
    for (int i = 0; i < kTerritoryCount; ++i) {
        if (g_t[i].owner != kYou && g_t[i].strength < 10 && g_t[i].scene == kNoScene) g_t[i].strength++;
    }

    // Unanswered attacks on your hideouts resolve.
    for (int i = 0; i < kTerritoryCount; ++i) {
        Territory& t = g_t[i];
        if (t.owner != kYou || t.attackDeadline == 0 || now < t.attackDeadline || t.scene != kNoScene) continue;
        int defense = 2 + 2 * t.level + Rand(0, 4);
        int attack = 4 + Rand(0, 5);
        t.attackDeadline = 0;
        if (attack > defense) {
            Faction winner = t.attacker;
            ChangeOwner(i, winner, 4);
            ui::Notify(std::string(kFactions[winner].name) + " overran your hideout at " + Info(i).name + ".", 8000);
        } else {
            ui::Notify("Your boys held " + std::string(Info(i).name) + " without you.", 6000);
        }
    }

    // One gang makes a move.
    std::vector<int> candidates;
    for (int i = 0; i < kTerritoryCount; ++i) {
        if (g_t[i].owner != kYou && g_t[i].strength >= 4) candidates.push_back(i);
    }
    if (candidates.empty()) return;
    int from = candidates[Rand(0, static_cast<int>(candidates.size()) - 1)];
    int target = NearestRival(from);
    if (target < 0) return;
    Territory& a = g_t[from];
    Territory& b = g_t[target];

    if (b.owner == kYou) {
        if (b.attackDeadline > now) return;
        b.attacker = a.owner;
        b.attackDeadline = now + kDefendWindowMs;
        ui::Notify(std::string(kFactions[a.owner].name) + " are moving on your hideout at " + Info(target).name +
                       "! Ride there within 5 minutes.",
                   9000);
        return;
    }

    b.contester = a.owner;
    b.contestedUntil = now + kContestedMs;
    if (b.scene != kNoScene) return;  // you're there: let it play out in front of you
    int attack = a.strength + Rand(0, 5);
    int defense = b.strength + Rand(0, 5);
    if (attack > defense) {
        Faction old = b.owner;
        ChangeOwner(target, a.owner, a.strength / 2 + 1);
        a.strength -= 1;
        Save(from);
        ui::Notify("Gang war: " + std::string(kFactions[a.owner].name) + " took " + Info(target).name + " from " +
                       kFactions[old].name + ".",
                   7000);
    } else {
        a.strength -= 2;
        if (a.strength < 1) a.strength = 1;
        Save(from);
    }
    config::Save();
}

void IncomeTick() {
    bool changed = false;
    for (int i = 0; i < kTerritoryCount; ++i) {
        Territory& t = g_t[i];
        if (t.owner != kYou || t.bank >= BankCap(i)) continue;
        t.bank += Income(i);
        if (t.bank > BankCap(i)) t.bank = BankCap(i);
        Save(i);
        changed = true;
    }
    if (changed) config::Save();
}

int CurrentTerritory() {
    Vector3 pos = util::PlayerPos();
    for (int i = 0; i < kTerritoryCount; ++i) {
        if (util::Dist(pos, Center(i)) < Info(i).radius + 20.0f) return i;
    }
    return -1;
}

std::string OwnerLine(int i) {
    const Territory& t = g_t[i];
    if (t.owner == kYou) return "Yours (level " + std::to_string(t.level) + ")";
    return std::string(kFactions[t.owner].name) + " (" + std::to_string(t.strength) + ")";
}

void TerritoryPage(int i) {
    const TerritoryInfo& info = Info(i);
    const Territory& t = g_t[i];
    ui::Title(info.name);
    ui::Text("Held by: " + OwnerLine(i));
    int dist = static_cast<int>(util::Dist(util::PlayerPos(), Center(i)));
    ui::Text("Distance: " + std::to_string(dist) + "m");
    if (t.owner == kYou) {
        ui::Text("Strongbox: $" + std::to_string(t.bank) + " / $" + std::to_string(BankCap(i)));
        ui::Text("Earns $" + std::to_string(Income(i)) + " a minute.");
    } else if (Neutral(t.owner, &t)) {
        ui::Text("They leave you be unless you start it.");
    } else {
        ui::Text("They'll shoot on sight.");
    }
    if (t.attackDeadline > util::Now()) ui::Text("UNDER ATTACK!");
    if (ui::Item("Mark it on my map")) {
        START_GPS_MULTI_ROUTE(util::Joaat("COLOR_RED"), TRUE, TRUE);
        ADD_POINT_TO_GPS_MULTI_ROUTE(info.x, info.y, info.z, FALSE);
        SET_GPS_MULTI_ROUTE_RENDER(TRUE);
        ui::Notify("Route set to " + std::string(info.name) + ".");
    }
}

}  // namespace

bool AtTerritory() { return CurrentTerritory() >= 0; }

int CollectAll() {
    int total = 0;
    for (int i = 0; i < kTerritoryCount; ++i) {
        if (g_t[i].owner != kYou) continue;
        total += g_t[i].bank;
        g_t[i].bank = 0;
        Save(i);
    }
    config::Save();
    return total;
}

void Tick() {
    if (!g_loaded) Load();
    if (!mode::Enabled()) {
        for (int i = 0; i < kTerritoryCount; ++i) {
            if (g_t[i].scene != kNoScene) EndScene(i, true);
            Territory& t = g_t[i];
            if (t.area && DOES_BLIP_EXIST(t.area)) REMOVE_BLIP(&t.area);
            if (t.point && DOES_BLIP_EXIST(t.point)) REMOVE_BLIP(&t.point);
        }
        return;
    }
    int now = util::Now();
    if (now < g_nextTick) return;
    g_nextTick = now + 1000;

    SetupGroups();
    // Blips (and our peds) vanish on reload; rebuild.
    if (config::GetInt("war", "show_map", 1) && (!g_t[0].point || !DOES_BLIP_EXIST(g_t[0].point))) {
        for (int i = 0; i < kTerritoryCount; ++i) {
            if (g_t[i].scene != kNoScene && (g_t[i].peds.empty() || !DOES_ENTITY_EXIST(g_t[i].peds[0]))) {
                g_t[i].peds.clear();
                g_t[i].enemies.clear();
                g_t[i].scene = kNoScene;
            }
        }
        g_groupsReady = false;
        SetupGroups();
        RefreshAllBlips();
    }

    if (g_nextWar == 0) g_nextWar = now + kWarTickMs;
    if (g_nextIncome == 0) g_nextIncome = now + kIncomeTickMs;
    if (now >= g_nextWar) {
        g_nextWar = now + kWarTickMs;
        WarTick(now);
    }
    if (now >= g_nextIncome) {
        g_nextIncome = now + kIncomeTickMs;
        IncomeTick();
    }

    if (IS_ENTITY_DEAD(PLAYER_PED_ID())) return;
    Vector3 pos = util::PlayerPos();
    for (int i = 0; i < kTerritoryCount; ++i) TickScene(i, util::Dist(pos, Center(i)), now);
}

void MapPage() {
    ui::Title("Gang Territories");
    int yours = 0;
    for (int i = 0; i < kTerritoryCount; ++i) yours += g_t[i].owner == kYou ? 1 : 0;
    ui::Text("You hold " + std::to_string(yours) + " of " + std::to_string(kTerritoryCount) + " territories.");
    for (int i = 0; i < kTerritoryCount; ++i) {
        std::string right = g_t[i].owner == kYou ? "Yours" : kFactions[g_t[i].owner].name;
        if (g_t[i].attackDeadline > util::Now()) right = "UNDER ATTACK";
        if (ui::Item(Info(i).name, right)) ui::Push([i] { TerritoryPage(i); });
    }
    if (ui::Option("Show territories on map", config::GetInt("war", "show_map", 1) ? "On" : "Off") != 0) {
        config::SetInt("war", "show_map", config::GetInt("war", "show_map", 1) ? 0 : 1);
        config::Save();
        RefreshAllBlips();
    }
}

void HerePage() {
    int i = CurrentTerritory();
    ui::Title("Here");
    if (i < 0) {
        ui::Text("You're not in anyone's territory.");
        return;
    }
    Territory& t = g_t[i];
    const TerritoryInfo& info = Info(i);
    ui::Text(std::string(info.name) + ": " + OwnerLine(i));

    if (t.owner != kYou) {
        if (Neutral(t.owner, &t)) {
            if (ui::Item("Make a move on this place")) {
                if (t.scene == kNoScene) StartScene(i);
                BeginAssault(i);
                ui::Close();
                ui::Notify("Clear out every one of " + std::string(kFactions[t.owner].name) + " here to take it.");
            }
        } else {
            ui::Text("Kill everyone holding it to take it.");
        }
        return;
    }

    ui::Text("Strongbox: $" + std::to_string(t.bank));
    if (t.bank > 0 && ui::Item("Collect earnings", "$" + std::to_string(t.bank))) {
        MONEY_INCREMENT_CASH_BALANCE(t.bank * 100, util::Joaat("ADD_REASON_DEFAULT"));
        ui::Notify("Collected $" + std::to_string(t.bank) + ".");
        t.bank = 0;
        Save(i);
        config::Save();
    }
    if (t.level < kMaxLevel) {
        int cost = kUpgradeCost[t.level];
        std::string what = t.level == 0 ? "Hire more guns and fix it up" : t.level == 1 ? "Add lookouts and a still"
                                                                                       : "Turn it into a proper fort";
        if (ui::Item(what, "$" + std::to_string(cost)) && util::Charge(cost, "the upgrade")) {
            t.level++;
            Save(i);
            config::Save();
            RefreshBlips(i);
            EndScene(i, true);  // respawn with the bigger crew
            ui::Notify(std::string(info.name) + " is now level " + std::to_string(t.level) + ". More guards, more money.");
        }
    } else {
        ui::Text("Fully upgraded.");
    }
}

}  // namespace war
