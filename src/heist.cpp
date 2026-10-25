#include "heist.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "config.h"
#include "gang.h"
#include "mode.h"
#include "ui.h"
#include "util.h"

namespace heist {
namespace {

// Door hashes and vault positions come from femga/rdr3_discoveries
// (doorHashes/doorhashes.lua).
struct Bank {
    const char* key;
    const char* name;
    float vx, vy, vz;   // vault door
    const char* lawModel;
    int minTake, maxTake;
    Hash doors[8];       // 0-terminated: front, inner and vault doors to unlock
};

const Bank kBanks[] = {
    {"valentine", "Valentine Bank", -307.75f, 766.35f, 117.70f, "s_m_m_valdeputy_01", 450, 900,
     {2642457609u, 3886827663u, 334467483u, 3718620420u, 968874193u, 576950805u, 1340831050u, 2343746133u}},
    {"rhodes", "Rhodes Bank", 1282.54f, -1309.32f, 76.04f, "s_m_m_ambientlawrural_01", 500, 1000,
     {3483244267u, 0}},
    {"saintdenis", "Saint Denis Bank", 2643.30f, -1300.43f, 51.26f, "s_m_m_ambientsdpolice_01", 1200, 2500,
     {1733501235u, 2089945615u, 2158285782u, 2817024187u, 965922748u, 1634115439u, 1830999060u, 1751238140u}},
    {"armadillo", "Armadillo Bank", -3661.14f, -2627.36f, -14.59f, "s_m_m_marshallsrural_01", 350, 750,
     {3101287960u, 1329318347u, 3550475905u, 0}},
};
constexpr int kBankCount = sizeof(kBanks) / sizeof(kBanks[0]);

// Points on the railway (from the BGS_Trains RedM script's junction list).
const float kTrackPoints[][3] = {
    {-281.13f, -319.66f, 89.02f}, {357.96f, 596.37f, 115.68f},   {1481.54f, 648.33f, 92.31f},
    {2464.55f, -1475.74f, 46.15f}, {2659.79f, -435.71f, 43.39f}, {610.36f, 1661.90f, 187.39f},
    {556.65f, 1725.99f, 187.80f}, {-2187.18f, -2517.21f, 65.70f}, {-3763.37f, -2782.54f, -14.43f},
};
constexpr Hash kTrainConfig = 0xCD2C7CA1u;

constexpr float kCaseRadius = 30.0f;
constexpr int kCaseMs = 20000;
constexpr float kVaultRadius = 4.0f;
constexpr float kEscapeDistance = 500.0f;
constexpr int kCooldownMs = 45 * 60 * 1000;

enum Kind { kBankJob, kTrainJob };
enum Phase { kIdle, kPlanned, kTrainChase, kTrainFight, kCracking, kEscaping };

struct Job {
    Kind kind = kBankJob;
    int bank = -1;
    bool loud = true;
    Phase phase = kIdle;
    int caseMs = 0;         // time spent casing so far
    int crackMs = 0;        // vault progress
    int crackNeeded = 0;
    int take = 0;
    int nextWave = 0;
    int wavesLeft = 0;
    int deadline = 0;
    Vector3 site{};         // where the job happened, for the escape check
    Vehicle train = 0;
    Blip blip = 0;
    std::vector<Ped> hostiles;
};
Job g_job;
Hash g_lawGroup = 0;
int g_lastTick = 0;
int g_cooldownUntil[kBankCount] = {};

bool Alive(Ped p) { return p && DOES_ENTITY_EXIST(p) && !IS_ENTITY_DEAD(p); }
int Rand(int lo, int hi) { return lo + std::rand() % (hi - lo + 1); }

Vector3 Vault(int b) { return V3(kBanks[b].vx, kBanks[b].vy, kBanks[b].vz); }

bool Scouted(int b) { return config::GetInt("heists", std::string(kBanks[b].key) + "_cased", 0) != 0; }

void SetupLaw() {
    if (g_lawGroup) return;
    ADD_RELATIONSHIP_GROUP("OM_HEIST_LAW", &g_lawGroup);
    Hash player = GET_PED_RELATIONSHIP_GROUP_HASH(PLAYER_PED_ID());
    SET_RELATIONSHIP_BETWEEN_GROUPS(5, g_lawGroup, player);
    SET_RELATIONSHIP_BETWEEN_GROUPS(5, player, g_lawGroup);
}

void ClearBlip() {
    if (g_job.blip && DOES_BLIP_EXIST(g_job.blip)) REMOVE_BLIP(&g_job.blip);
    g_job.blip = 0;
}

void Route(const Vector3& to) {
    START_GPS_MULTI_ROUTE(util::Joaat("COLOR_RED"), TRUE, TRUE);
    ADD_POINT_TO_GPS_MULTI_ROUTE(to.x, to.y, to.z, FALSE);
    SET_GPS_MULTI_ROUTE_RENDER(TRUE);
}

Ped SpawnHostile(const char* modelName, const Vector3& around, float minR, float maxR, bool mounted) {
    Hash model = util::Joaat(modelName);
    if (!util::LoadModel(model)) {
        util::Log(std::string("Heist: could not load ") + modelName);
        return 0;
    }
    float a = static_cast<float>(Rand(0, 359)) * 0.0174533f;
    float r = minR + static_cast<float>(Rand(0, 100)) / 100.0f * (maxR - minR);
    float x = around.x + std::cos(a) * r, y = around.y + std::sin(a) * r;
    float z = util::GroundZ(x, y, around.z);
    Ped p = CREATE_PED(model, x, y, z, 0.0f, FALSE, FALSE, FALSE, FALSE);
    SET_MODEL_AS_NO_LONGER_NEEDED(model);
    if (!p) return 0;
    SET_RANDOM_OUTFIT_VARIATION(p, TRUE);
    SET_ENTITY_AS_MISSION_ENTITY(p, TRUE, TRUE);
    SET_PED_RELATIONSHIP_GROUP_HASH(p, g_lawGroup);
    SET_PED_COMBAT_ATTRIBUTES(p, 46, TRUE);
    SET_PED_ACCURACY(p, 30);
    Hash reason = util::Joaat("ADD_REASON_DEFAULT");
    GIVE_WEAPON_TO_PED(p, util::Joaat(Rand(0, 1) ? "WEAPON_REPEATER_CARBINE" : "WEAPON_REVOLVER_SCHOFIELD"), 200,
                       TRUE, FALSE, 0, FALSE, 0.5f, 1.0f, reason, TRUE, 0.0f, FALSE);
    if (mounted) {
        Hash horseModel = util::Joaat("a_c_horse_kentuckysaddle_grey");
        if (util::LoadModel(horseModel)) {
            Ped horse = CREATE_PED(horseModel, x, y, z, 0.0f, FALSE, FALSE, FALSE, FALSE);
            SET_MODEL_AS_NO_LONGER_NEEDED(horseModel);
            if (horse) {
                SET_RANDOM_OUTFIT_VARIATION(horse, TRUE);
                SET_PED_ONTO_MOUNT(p, horse, -1, TRUE);
                SET_ENTITY_AS_NO_LONGER_NEEDED(&horse);
            }
        }
    }
    TASK_COMBAT_PED(p, PLAYER_PED_ID(), 0, 16);
    g_job.hostiles.push_back(p);
    return p;
}

void SpawnWave(const char* model, const Vector3& at, int count, bool mounted) {
    for (int i = 0; i < count; ++i) SpawnHostile(model, at, mounted ? 90.0f : 25.0f, mounted ? 140.0f : 45.0f, mounted);
}

int CountAlive() {
    int n = 0;
    for (Ped p : g_job.hostiles) n += Alive(p) ? 1 : 0;
    return n;
}

void ReleaseHostiles() {
    for (Ped& p : g_job.hostiles) {
        if (p && DOES_ENTITY_EXIST(p)) SET_ENTITY_AS_NO_LONGER_NEEDED(&p);
    }
    g_job.hostiles.clear();
}

void Reset() {
    ReleaseHostiles();
    ClearBlip();
    if (g_job.train && DOES_ENTITY_EXIST(g_job.train)) SET_MISSION_TRAIN_AS_NO_LONGER_NEEDED(&g_job.train, 0);
    CLEAR_GPS_MULTI_ROUTE();
    g_job = Job{};
}

// Doors stay unlocked afterwards: relocking them could shut a bank for good.
void UnlockDoors(int b) {
    for (Hash door : kBanks[b].doors) {
        if (!door) break;
        if (!IS_DOOR_REGISTERED_WITH_SYSTEM(door)) ADD_DOOR_TO_SYSTEM_NEW(door, TRUE, TRUE, FALSE, 0, 0, FALSE);
        DOOR_SYSTEM_SET_DOOR_STATE(door, 0);
    }
}

int CrewBonus(int take) { return take + take * gang::PosseCount() / 10; }

bool Night() {
    int h = GET_CLOCK_HOURS();
    return h >= 22 || h < 5;
}

void Progress(const std::string& label, int done, int needed) {
    int pct = needed > 0 ? done * 100 / needed : 0;
    if (pct > 100) pct = 100;
    ui::DrawLabel(label + ": " + std::to_string(pct) + "%", 0.42f, 0.86f, 0.4f, 255, 230, 150, 255);
}

// --- Bank job ------------------------------------------------------------

void StartBankJob(int b) {
    SetupLaw();
    g_job.phase = kCracking;
    g_job.site = Vault(b);
    g_job.crackMs = 0;
    g_job.crackNeeded = g_job.loud ? 25000 : 40000;
    g_job.take = CrewBonus(Rand(kBanks[b].minTake, kBanks[b].maxTake));
    UnlockDoors(b);
    if (g_job.loud) {
        for (int i = 0; i < 2; ++i) SpawnHostile("s_m_m_valbankguards_01", g_job.site, 3.0f, 8.0f, false);
        g_job.wavesLeft = 3;
        g_job.nextWave = util::Now() + 15000;
        ui::Notify("\"Everybody down! Nobody's a hero today!\" Get to the vault.", 6000);
    } else {
        g_job.wavesLeft = 0;
        ui::Notify("Quiet now. Get to the vault and work the lock.", 6000);
    }
    Route(g_job.site);
    config::Set("heists", std::string(kBanks[b].key) + "_cased", "0");
    config::Save();
}

void TickBankJob(int now, int dt, const Vector3& pos) {
    const Bank& bank = kBanks[g_job.bank];
    if (g_job.phase == kPlanned) {
        float d = util::Dist(pos, Vault(g_job.bank));
        if (Scouted(g_job.bank)) return;
        if (d < kCaseRadius) {
            g_job.caseMs += dt;
            Progress("Casing " + std::string(bank.name), g_job.caseMs, kCaseMs);
            if (g_job.caseMs >= kCaseMs) {
                config::SetInt("heists", std::string(bank.key) + "_cased", 1);
                config::Save();
                ui::Notify("You know the layout now. Open the menu here to start the robbery.", 7000);
            }
        }
        return;
    }
    if (g_job.phase == kCracking) {
        if (g_job.loud && g_job.wavesLeft > 0 && now >= g_job.nextWave) {
            SpawnWave(bank.lawModel, g_job.site, 3, false);
            g_job.wavesLeft--;
            g_job.nextWave = now + 25000;
            ui::Notify("Law's here!");
        }
        if (util::Dist(pos, g_job.site) < kVaultRadius) {
            g_job.crackMs += dt;
        }
        Progress(g_job.loud ? "Blowing the vault" : "Cracking the vault", g_job.crackMs, g_job.crackNeeded);
        if (g_job.crackMs >= g_job.crackNeeded) {
            g_job.phase = kEscaping;
            ui::Notify("Vault's open! You grab $" + std::to_string(g_job.take) + ". Now ride!", 7000);
            bool alarm = g_job.loud || Rand(1, 100) <= 35;
            if (alarm) {
                SpawnWave(bank.lawModel, g_job.site, 4, true);
                if (!g_job.loud) ui::Notify("Someone raised the alarm!");
            }
            CLEAR_GPS_MULTI_ROUTE();
        }
    }
}

// --- Train job -----------------------------------------------------------

bool SpawnTrain() {
    Vector3 pos = util::PlayerPos();
    int best = -1;
    float bestScore = 1e9f;
    for (int i = 0; i < static_cast<int>(sizeof(kTrackPoints) / sizeof(kTrackPoints[0])); ++i) {
        float d = util::Dist(pos, V3(kTrackPoints[i][0], kTrackPoints[i][1], kTrackPoints[i][2]));
        // Prefer a spot 400-1600m away: far enough to stage, close enough to catch.
        float score = d < 300.0f ? 1e6f + d : std::fabs(d - 900.0f);
        if (score < bestScore) {
            bestScore = score;
            best = i;
        }
    }
    if (best < 0) return false;

    int cars = GET_NUM_CARS_FROM_TRAIN_CONFIG(kTrainConfig);
    for (int i = 0; i < cars; ++i) util::LoadModel(GET_TRAIN_MODEL_FROM_TRAIN_CONFIG_BY_CAR_INDEX(kTrainConfig, i));
    const float* p = kTrackPoints[best];
    g_job.train = CREATE_MISSION_TRAIN(kTrainConfig, p[0], p[1], p[2], Rand(0, 1), FALSE, TRUE, TRUE);
    if (!g_job.train) return false;
    SET_TRAIN_MAX_SPEED(g_job.train, 14.0f);
    SET_TRAIN_CRUISE_SPEED(g_job.train, 12.0f);
    SET_TRAIN_SPEED(g_job.train, 12.0f);
    SET_TRAIN_STOPS_FOR_STATIONS(g_job.train, FALSE);
    g_job.blip = BLIP_ADD_FOR_ENTITY(util::Joaat("BLIP_STYLE_ENEMY"), g_job.train);
    SET_BLIP_NAME(g_job.blip, "Payroll train");
    return true;
}

void TickTrainJob(int now, int dt, const Vector3& pos) {
    if (!g_job.train || !DOES_ENTITY_EXIST(g_job.train)) {
        ui::Notify("The payroll train is gone. The job's off.");
        Reset();
        return;
    }
    Vector3 tpos = GET_ENTITY_COORDS(g_job.train, TRUE, FALSE);
    if (g_job.phase == kTrainChase) {
        if (now > g_job.deadline) {
            ui::Notify("The payroll train got away.");
            Reset();
            return;
        }
        if (util::Dist(pos, tpos) < 30.0f) {
            SET_TRAIN_CRUISE_SPEED(g_job.train, 0.0f);
            SET_TRAIN_SPEED(g_job.train, 0.0f);
            SetupLaw();
            SpawnWave("s_m_m_unitrainguards_01", tpos, 5, false);
            g_job.phase = kTrainFight;
            ui::Notify("You flag the engineer down at gunpoint. Guards pile out!", 6000);
        }
        return;
    }
    SET_TRAIN_SPEED(g_job.train, 0.0f);
    Vehicle express = GET_TRAIN_CARRIAGE(g_job.train, 2);
    Vector3 box = (express && DOES_ENTITY_EXIST(express)) ? GET_ENTITY_COORDS(express, TRUE, FALSE) : tpos;
    if (g_job.phase == kTrainFight) {
        if (CountAlive() == 0) {
            g_job.phase = kCracking;
            g_job.crackMs = 0;
            g_job.crackNeeded = 15000;
            g_job.take = CrewBonus(Rand(300, 800));
            g_job.site = box;
            ui::Notify("Guards are down. Get into the express car and open the strongbox.", 6000);
        }
        return;
    }
    if (g_job.phase == kCracking) {
        if (util::Dist(pos, box) < 6.0f) g_job.crackMs += dt;
        Progress("Opening the strongbox", g_job.crackMs, g_job.crackNeeded);
        if (g_job.crackMs >= g_job.crackNeeded) {
            g_job.phase = kEscaping;
            ReleaseHostiles();
            SpawnWave("s_m_m_marshallsrural_01", box, 4, true);
            ui::Notify("You take $" + std::to_string(g_job.take) + " from the strongbox. Marshals are coming!", 7000);
        }
    }
}

// --- Menu pages ----------------------------------------------------------

void PlanBankPage() {
    ui::Title("Plan a bank job");
    int now = util::Now();
    for (int b = 0; b < kBankCount; ++b) {
        std::string right = g_cooldownUntil[b] > now ? "lying low" : Scouted(b) ? "cased" : "";
        if (ui::Item(kBanks[b].name, right)) {
            if (g_cooldownUntil[b] > now) {
                ui::Notify("Too soon. The whole county is watching " + std::string(kBanks[b].name) + ".");
            } else {
                Reset();
                g_job.kind = kBankJob;
                g_job.bank = b;
                g_job.phase = kPlanned;
                Route(Vault(b));
                ui::Notify(Scouted(b) ? "Ride to " + std::string(kBanks[b].name) + "." :
                           "Ride to " + std::string(kBanks[b].name) + " and look around inside to case it.", 6000);
                ui::Pop();
            }
        }
    }
}

}  // namespace

bool Active() { return g_job.phase != kIdle; }

void Tick() {
    int now = util::Now();
    int dt = g_lastTick ? now - g_lastTick : 0;
    if (dt > 1000) dt = 1000;
    g_lastTick = now;
    if (!mode::Enabled() || g_job.phase == kIdle) return;

    Ped player = PLAYER_PED_ID();
    if (IS_ENTITY_DEAD(player) && g_job.phase != kPlanned) {
        ui::Notify("The job's a bust. The loot is lost.", 6000);
        Reset();
        return;
    }
    Vector3 pos = util::PlayerPos();

    if (g_job.phase == kEscaping) {
        float d = util::Dist(pos, g_job.site);
        ui::DrawLabel("Get away: " + std::to_string(static_cast<int>(d)) + " / " +
                          std::to_string(static_cast<int>(kEscapeDistance)) + "m",
                      0.42f, 0.86f, 0.4f, 255, 230, 150, 255);
        if (d > kEscapeDistance) {
            MONEY_INCREMENT_CASH_BALANCE(g_job.take * 100, util::Joaat("ADD_REASON_DEFAULT"));
            ui::Notify("You got away with $" + std::to_string(g_job.take) + "!", 8000);
            config::SetInt("heists", "total_take", config::GetInt("heists", "total_take", 0) + g_job.take);
            config::SetInt("heists", "jobs_done", config::GetInt("heists", "jobs_done", 0) + 1);
            config::Save();
            if (g_job.kind == kBankJob) {
                g_cooldownUntil[g_job.bank] = now + kCooldownMs;
            }
            Reset();
        }
        return;
    }
    if (g_job.kind == kBankJob) TickBankJob(now, dt, pos);
    else TickTrainJob(now, dt, pos);
}

void MenuPage() {
    ui::Title("Heists");
    ui::Text("Jobs pulled: " + std::to_string(config::GetInt("heists", "jobs_done", 0)) + "   Total take: $" +
             std::to_string(config::GetInt("heists", "total_take", 0)));
    ui::Text("Each rider in your posse adds 10% to the take.");

    if (g_job.phase == kIdle) {
        if (ui::Item("Plan a bank job...")) ui::Push(PlanBankPage);
        if (ui::Item("Hit the payroll train")) {
            Reset();
            g_job.kind = kTrainJob;
            if (SpawnTrain()) {
                g_job.phase = kTrainChase;
                g_job.deadline = util::Now() + 6 * 60 * 1000;
                ui::Close();
                ui::Notify("Word is a payroll train just left. It's on your map. Ride alongside the engine to stop it.",
                           8000);
            } else {
                ui::Notify("Couldn't find the train. Try again somewhere else.");
            }
        }
        return;
    }

    if (g_job.kind == kBankJob && g_job.phase == kPlanned) {
        const Bank& bank = kBanks[g_job.bank];
        ui::Text("Target: " + std::string(bank.name));
        bool nearby = util::Dist(util::PlayerPos(), Vault(g_job.bank)) < kCaseRadius;
        if (!Scouted(g_job.bank)) {
            ui::Text(nearby ? "Casing it. Stay inside a little longer." : "Go inside and case the place first.");
        } else if (!nearby) {
            ui::Text("Cased. Go to the bank to start.");
        } else {
            if (ui::Option("Approach", g_job.loud ? "Loud" : "Quiet (night only)") != 0) g_job.loud = !g_job.loud;
            if (ui::Item("Start the robbery")) {
                if (!g_job.loud && !Night()) {
                    ui::Notify("A quiet job needs the dark. Come back between 10pm and 5am.");
                } else {
                    ui::Close();
                    StartBankJob(g_job.bank);
                }
            }
        }
    }
    if (ui::Item("Call off the job")) {
        Reset();
        ui::Notify("Job called off.");
    }
}

}  // namespace heist
