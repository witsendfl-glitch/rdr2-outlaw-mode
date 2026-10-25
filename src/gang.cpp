#include "gang.h"

#include <string>
#include <vector>

#include "camp.h"
#include "config.h"
#include "mode.h"
#include "ui.h"
#include "util.h"

namespace gang {
namespace {

constexpr int kMaxPosse = 6;
constexpr float kCatchUpDistance = 220.0f;
constexpr int kRecoverMs = 10 * 60 * 1000;  // real time before a downed member is back at camp

enum Status { kNone = 0, kPosse = 1, kAtCamp = 2, kWounded = 3 };
enum Order { kFollow, kWait };

struct Member {
    const char* key;
    const char* name;
    const char* model;
    const char* horse;
    const char* sidearm;
    const char* longarm;
};

const Member kRoster[] = {
    {"dutch", "Dutch van der Linde", "cs_dutch", "a_c_horse_arabian_white", "WEAPON_REVOLVER_SCHOFIELD", "WEAPON_REPEATER_WINCHESTER"},
    {"hosea", "Hosea Matthews", "cs_hoseamatthews", "a_c_horse_kentuckysaddle_grey", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_RIFLE_SPRINGFIELD"},
    {"john", "John Marston", "cs_johnmarston", "a_c_horse_kentuckysaddle_black", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_REPEATER_CARBINE"},
    {"sadie", "Sadie Adler", "cs_mrsadler", "a_c_horse_dutchwarmblood_sootybuckskin", "WEAPON_REVOLVER_SCHOFIELD", "WEAPON_SHOTGUN_PUMP"},
    {"charles", "Charles Smith", "cs_charlessmith", "a_c_horse_americanstandardbred_black", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_BOW"},
    {"javier", "Javier Escuella", "cs_javierescuella", "a_c_horse_kentuckysaddle_silverbay", "WEAPON_REVOLVER_DOUBLEACTION", "WEAPON_REPEATER_CARBINE"},
    {"bill", "Bill Williamson", "cs_billwilliamson", "a_c_horse_dutchwarmblood_chocolateroan", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_SHOTGUN_DOUBLEBARREL"},
    {"lenny", "Lenny Summers", "cs_lenny", "a_c_horse_kentuckysaddle_chestnutpinto", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_REPEATER_WINCHESTER"},
    {"micah", "Micah Bell", "cs_micahbell", "a_c_horse_dutchwarmblood_sealbrown", "WEAPON_REVOLVER_DOUBLEACTION", "WEAPON_REPEATER_HENRY"},
    {"sean", "Sean MacGuire", "cs_sean", "a_c_horse_kentuckysaddle_grey", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_REPEATER_CARBINE"},
    {"kieran", "Kieran Duffy", "cs_kieran", "a_c_horse_kentuckysaddle_black", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_REPEATER_CARBINE"},
    {"uncle", "Uncle", "cs_uncle", "a_c_horse_americanstandardbred_black", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_SHOTGUN_DOUBLEBARREL"},
    {"pearson", "Simon Pearson", "cs_mrpearson", "a_c_horse_kentuckysaddle_silverbay", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_SHOTGUN_DOUBLEBARREL"},
    {"tilly", "Tilly Jackson", "cs_tilly", "a_c_horse_kentuckysaddle_chestnutpinto", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_REPEATER_CARBINE"},
    {"karen", "Karen Jones", "cs_karen", "a_c_horse_kentuckysaddle_grey", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_REPEATER_CARBINE"},
    {"marybeth", "Mary-Beth Gaskill", "cs_marybeth", "a_c_horse_kentuckysaddle_silverbay", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_REPEATER_CARBINE"},
    {"abigail", "Abigail Roberts", "cs_abigailroberts", "a_c_horse_kentuckysaddle_black", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_REPEATER_CARBINE"},
    {"susan", "Susan Grimshaw", "cs_susangrimshaw", "a_c_horse_americanstandardbred_black", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_SHOTGUN_DOUBLEBARREL"},
    {"molly", "Molly O'Shea", "cs_mollyoshea", "a_c_horse_kentuckysaddle_grey", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_REPEATER_CARBINE"},
    {"strauss", "Leopold Strauss", "cs_leostrauss", "a_c_horse_kentuckysaddle_chestnutpinto", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_REPEATER_CARBINE"},
    {"swanson", "Reverend Swanson", "cs_revswanson", "a_c_horse_kentuckysaddle_silverbay", "WEAPON_REVOLVER_CATTLEMAN", "WEAPON_REPEATER_CARBINE"},
    {"trelawny", "Josiah Trelawny", "cs_josiahtrelawny", "a_c_horse_arabian_white", "WEAPON_REVOLVER_LEMAT", "WEAPON_REPEATER_CARBINE"},
};
constexpr int kRosterSize = sizeof(kRoster) / sizeof(kRoster[0]);

const char* kCampScenarios[] = {
    "WORLD_HUMAN_SIT_GROUND", "WORLD_CAMP_FIRE_STANDING", "WORLD_HUMAN_SMOKE",
    "WORLD_HUMAN_COFFEE_DRINK", "WORLD_HUMAN_SIT_GROUND", "WORLD_CAMP_FIRE_STANDING",
};

struct State {
    Status status = kNone;
    Ped ped = 0;
    Ped horse = 0;
    Blip blip = 0;
    int recoverAt = 0;
    int nextTask = 0;
    bool atCampSpawn = false;  // spawned at camp rather than with the posse
};
State g_state[kRosterSize];
Order g_order = kFollow;
int g_nextTick = 0;

bool Alive(Ped p) { return p && DOES_ENTITY_EXIST(p) && !IS_ENTITY_DEAD(p); }

void SaveStatus(int i) {
    Status s = g_state[i].status;
    if (s == kWounded) s = kAtCamp;  // a reload finds them recovered at camp
    if (s == kNone) config::Remove("gang", kRoster[i].key);
    else config::SetInt("gang", kRoster[i].key, s);
    config::Save();
}

void Despawn(int i, bool deleteEntities) {
    State& st = g_state[i];
    if (st.blip && DOES_BLIP_EXIST(st.blip)) REMOVE_BLIP(&st.blip);
    st.blip = 0;
    if (deleteEntities) {
        if (st.ped && DOES_ENTITY_EXIST(st.ped)) DELETE_PED(&st.ped);
        if (st.horse && DOES_ENTITY_EXIST(st.horse)) DELETE_PED(&st.horse);
    } else {
        if (st.ped && DOES_ENTITY_EXIST(st.ped)) SET_ENTITY_AS_NO_LONGER_NEEDED(&st.ped);
        if (st.horse && DOES_ENTITY_EXIST(st.horse)) SET_ENTITY_AS_NO_LONGER_NEEDED(&st.horse);
    }
    st.ped = st.horse = 0;
}

int PosseSlot(int i) {
    int slot = 0;
    for (int j = 0; j < i; ++j) {
        if (g_state[j].status == kPosse) slot++;
    }
    return slot;
}

void SlotOffset(int slot, float* ox, float* oy) {
    float side = (slot % 2 == 0) ? -1.0f : 1.0f;
    *ox = side * (2.0f + 1.5f * static_cast<float>(slot / 2));
    *oy = -4.0f - 3.0f * static_cast<float>(slot / 2);
}

void Arm(Ped ped, const Member& m) {
    Hash reason = util::Joaat("ADD_REASON_DEFAULT");
    GIVE_WEAPON_TO_PED(ped, util::Joaat(m.longarm), 500, FALSE, TRUE, 0, FALSE, 0.5f, 1.0f, reason, TRUE, 0.0f, FALSE);
    GIVE_WEAPON_TO_PED(ped, util::Joaat(m.sidearm), 500, TRUE, FALSE, 0, FALSE, 0.5f, 1.0f, reason, TRUE, 0.0f, FALSE);
}

void MakeFriendly(Ped ped) {
    Ped player = PLAYER_PED_ID();
    SET_PED_RELATIONSHIP_GROUP_HASH(ped, GET_PED_RELATIONSHIP_GROUP_HASH(player));
    SET_PED_CAN_BE_TARGETTED_BY_PLAYER(ped, PLAYER_ID(), FALSE);
    SET_PED_COMBAT_ATTRIBUTES(ped, 46, TRUE);  // always fight
    SET_PED_COMBAT_ATTRIBUTES(ped, 5, TRUE);   // fight armed peds even when outgunned
    SET_PED_FLEE_ATTRIBUTES(ped, 0, FALSE);
    SET_PED_COMBAT_ABILITY(ped, 2);
    SET_PED_ACCURACY(ped, 55);
    SET_PED_KEEP_TASK(ped, TRUE);
    if (config::GetInt("gang", "invincible", 0)) SET_ENTITY_INVINCIBLE(ped, TRUE);
}

Ped CreateHorse(const Member& m, float x, float y, float z, float heading) {
    Hash model = util::Joaat(m.horse);
    if (!util::LoadModel(model)) return 0;
    Ped horse = CREATE_PED(model, x, y, z, heading, FALSE, FALSE, FALSE, FALSE);
    SET_MODEL_AS_NO_LONGER_NEEDED(model);
    if (!horse) return 0;
    SET_RANDOM_OUTFIT_VARIATION(horse, TRUE);
    SET_ENTITY_AS_MISSION_ENTITY(horse, TRUE, TRUE);
    SET_PED_RELATIONSHIP_GROUP_HASH(horse, GET_PED_RELATIONSHIP_GROUP_HASH(PLAYER_PED_ID()));
    SET_MOUNT_SECURITY_ENABLED(horse, TRUE);  // stops the player stealing it by accident
    return horse;
}

// Spawns member i near the player (posse) or at their camp slot.
bool Spawn(int i, bool atCamp) {
    const Member& m = kRoster[i];
    State& st = g_state[i];
    Hash model = util::Joaat(m.model);
    if (!util::LoadModel(model)) {
        util::Log(std::string("Gang: could not load ") + m.model);
        ui::Notify(std::string("Couldn't find ") + m.name + " (model failed to load).");
        return false;
    }

    float x, y, z, heading = 0.0f;
    if (atCamp) {
        Vector3 s = camp::Slot(i, &heading);
        x = s.x; y = s.y; z = s.z;
    } else {
        float ox, oy;
        SlotOffset(PosseSlot(i), &ox, &oy);
        Vector3 p = GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(util::PlayerOrMount(), ox, oy - 4.0f, 0.0f);
        x = p.x; y = p.y; z = util::GroundZ(p.x, p.y, p.z);
        heading = GET_ENTITY_HEADING(PLAYER_PED_ID());
    }

    Ped ped = CREATE_PED(model, x, y, z, heading, FALSE, FALSE, FALSE, FALSE);
    SET_MODEL_AS_NO_LONGER_NEEDED(model);
    if (!ped) return false;
    SET_RANDOM_OUTFIT_VARIATION(ped, TRUE);
    SET_ENTITY_AS_MISSION_ENTITY(ped, TRUE, TRUE);
    SET_PED_PROMPT_NAME(ped, m.name);
    MakeFriendly(ped);
    Arm(ped, m);
    st.ped = ped;
    st.atCampSpawn = atCamp;

    st.blip = BLIP_ADD_FOR_ENTITY(util::Joaat("BLIP_STYLE_FRIENDLY"), ped);
    SET_BLIP_NAME(st.blip, m.name);

    if (atCamp) {
        TASK_START_SCENARIO_IN_PLACE_HASH(ped, util::Joaat(kCampScenarios[i % 6]), -1, TRUE, 0, heading, FALSE);
    } else {
        st.horse = CreateHorse(m, x + 1.5f, y, z, heading);
        if (st.horse) {
            SET_PED_OWNS_ANIMAL(ped, st.horse, TRUE);
            if (IS_PED_ON_MOUNT(PLAYER_PED_ID())) SET_PED_ONTO_MOUNT(ped, st.horse, -1, TRUE);
        }
        if (g_order == kFollow) SET_PED_AS_GROUP_MEMBER(ped, GET_PLAYER_GROUP(PLAYER_ID()));
    }
    return true;
}

void ApplyOrder(int i) {
    State& st = g_state[i];
    if (!Alive(st.ped)) return;
    int group = GET_PLAYER_GROUP(PLAYER_ID());
    CLEAR_PED_TASKS(st.ped, FALSE, FALSE);
    if (g_order == kFollow) {
        SET_PED_AS_GROUP_MEMBER(st.ped, group);
    } else {
        REMOVE_PED_FROM_GROUP(st.ped);
        TASK_GUARD_CURRENT_POSITION(st.ped, 8.0f, 12.0f, TRUE);
    }
    st.nextTask = 0;
}

// Keeps a posse member riding or walking with the player.
void DrivePosseMember(int i, const Vector3& playerPos, bool playerMounted, int now) {
    State& st = g_state[i];
    const Member& m = kRoster[i];
    Vector3 pos = GET_ENTITY_COORDS(st.ped, TRUE, FALSE);
    float dist = util::Dist(pos, playerPos);

    if (g_order != kFollow) return;

    if (dist > kCatchUpDistance && !IS_ENTITY_ON_SCREEN(st.ped)) {
        float ox, oy;
        SlotOffset(PosseSlot(i), &ox, &oy);
        Vector3 p = GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(util::PlayerOrMount(), ox, oy - 25.0f, 0.0f);
        Entity mover = (st.horse && IS_PED_ON_MOUNT(st.ped)) ? st.horse : st.ped;
        SET_ENTITY_COORDS(mover, p.x, p.y, util::GroundZ(p.x, p.y, p.z), FALSE, FALSE, FALSE, FALSE);
        st.nextTask = 0;
        return;
    }
    if (IS_PED_IN_COMBAT(st.ped, 0) || now < st.nextTask) return;
    st.nextTask = now + 4000;

    bool mounted = IS_PED_ON_MOUNT(st.ped);
    if (playerMounted && !mounted) {
        if (!Alive(st.horse)) {
            st.horse = CreateHorse(m, pos.x + 2.0f, pos.y, pos.z, 0.0f);
            if (st.horse) SET_PED_OWNS_ANIMAL(st.ped, st.horse, TRUE);
        }
        if (st.horse) {
            Vector3 hp = GET_ENTITY_COORDS(st.horse, TRUE, FALSE);
            if (util::Dist(hp, pos) > 30.0f) {
                SET_ENTITY_COORDS(st.horse, pos.x + 2.0f, pos.y, pos.z, FALSE, FALSE, FALSE, FALSE);
            }
            TASK_MOUNT_ANIMAL(st.ped, st.horse, -1, -1, 2.0f, 1, 0, 0);
            st.nextTask = now + 7000;
        }
        return;
    }
    if (!playerMounted && mounted && dist < 25.0f) {
        TASK_DISMOUNT_ANIMAL(st.ped, 0, 0, 0, 0, 0);
        st.nextTask = now + 5000;
        return;
    }
    if (mounted) {
        float ox, oy;
        SlotOffset(PosseSlot(i), &ox, &oy);
        TASK_FOLLOW_TO_OFFSET_OF_ENTITY(st.ped, util::PlayerOrMount(), ox, oy, 0.0f, 3.0f, -1, 2.0f, TRUE,
                                        FALSE, FALSE, FALSE, FALSE, FALSE);
        st.nextTask = now + 10000;
    } else {
        // On foot the player's group handles following; make sure they're in it.
        int group = GET_PLAYER_GROUP(PLAYER_ID());
        if (!IS_PED_GROUP_MEMBER(st.ped, group, FALSE)) SET_PED_AS_GROUP_MEMBER(st.ped, group);
    }
}

void TickMember(int i, const Vector3& playerPos, bool playerMounted, int now) {
    State& st = g_state[i];
    const Member& m = kRoster[i];

    if (st.status == kWounded) {
        if (now >= st.recoverAt) {
            st.status = kAtCamp;
            SaveStatus(i);
            ui::Notify(std::string(m.name) + " has recovered and is back at camp.");
        }
        return;
    }

    if (st.ped && DOES_ENTITY_EXIST(st.ped) && IS_PED_DEAD_OR_DYING(st.ped, TRUE)) {
        ui::Notify(std::string(m.name) + " is down! They'll patch up at camp.");
        Despawn(i, false);
        st.status = kWounded;
        st.recoverAt = now + kRecoverMs;
        SaveStatus(i);
        return;
    }

    if (st.status == kPosse) {
        if (st.ped && DOES_ENTITY_EXIST(st.ped) && st.atCampSpawn) {
            // Was hanging around camp; now joins the posse.
            st.atCampSpawn = false;
            REMOVE_PED_FROM_GROUP(st.ped);
            ApplyOrder(i);
        }
        if (!Alive(st.ped)) {
            if (now < st.nextTask) return;
            Despawn(i, true);
            if (!Spawn(i, false)) st.nextTask = now + 30000;
            return;
        }
        DrivePosseMember(i, playerPos, playerMounted, now);
        return;
    }

    if (st.status == kAtCamp) {
        bool want = camp::Active() && camp::PlayerNearby();
        if (want && !Alive(st.ped)) {
            if (now < st.nextTask) return;
            Despawn(i, true);
            if (!Spawn(i, true)) st.nextTask = now + 30000;
        } else if (!want && st.ped) {
            Despawn(i, true);
        } else if (want && st.ped && !st.atCampSpawn) {
            // Sent back to camp while riding with us: walk them home.
            Despawn(i, true);
            Spawn(i, true);
        }
    }
}

std::string StatusText(int i) {
    switch (g_state[i].status) {
        case kPosse: return "Riding with you";
        case kAtCamp: return "At camp";
        case kWounded: return "Recovering";
        default: return "";
    }
}

void MemberPage(int i) {
    const Member& m = kRoster[i];
    State& st = g_state[i];
    ui::Title(m.name);
    switch (st.status) {
        case kNone:
            if (ui::Item("Recruit")) {
                if (PosseCount() < kMaxPosse) {
                    st.status = kPosse;
                    ui::Notify(std::string(m.name) + " rides with you.");
                } else {
                    st.status = camp::Active() ? kAtCamp : kNone;
                    ui::Notify(camp::Active() ? std::string(m.name) + " will meet you at camp (posse is full)."
                                              : std::string("Your posse is full. Pitch a camp to recruit more."));
                }
                SaveStatus(i);
            }
            break;
        case kAtCamp:
            if (ui::Item("Ride with me")) {
                if (PosseCount() >= kMaxPosse) {
                    ui::Notify("Your posse is full (6). Send someone to camp first.");
                } else {
                    st.status = kPosse;
                    SaveStatus(i);
                    if (!st.ped) ui::Notify(std::string(m.name) + " is riding out to meet you.");
                }
            }
            break;
        case kPosse:
            if (ui::Item("Send to camp")) {
                if (!camp::Active()) {
                    ui::Notify("You don't have a camp yet. Pitch one from the Camp menu.");
                } else {
                    st.status = kAtCamp;
                    SaveStatus(i);
                    Despawn(i, true);
                    ui::Notify(std::string(m.name) + " heads back to camp.");
                }
            }
            break;
        case kWounded:
            ui::Text("Recovering from their wounds.");
            break;
    }
    if (st.status != kNone && ui::Item("Part ways")) {
        Despawn(i, st.status != kWounded);
        st.status = kNone;
        SaveStatus(i);
        ui::Notify(std::string(m.name) + " goes their own way. You can recruit them again later.");
        ui::Pop();
    }
}

}  // namespace

int PosseCount() {
    int n = 0;
    for (auto& s : g_state) {
        if (s.status == kPosse) n++;
    }
    return n;
}

void Tick() {
    static bool loaded = false;
    if (!loaded) {
        for (int i = 0; i < kRosterSize; ++i) {
            g_state[i].status = static_cast<Status>(config::GetInt("gang", kRoster[i].key, kNone));
        }
        g_order = config::GetInt("gang", "order_wait", 0) ? kWait : kFollow;
        loaded = true;
    }

    if (!mode::Enabled()) {
        for (int i = 0; i < kRosterSize; ++i) {
            if (g_state[i].ped) Despawn(i, true);
        }
        return;
    }

    int now = util::Now();
    if (now < g_nextTick) return;
    g_nextTick = now + 500;

    Ped player = PLAYER_PED_ID();
    if (IS_ENTITY_DEAD(player)) return;
    Vector3 playerPos = util::PlayerPos();
    bool playerMounted = IS_PED_ON_MOUNT(player);
    for (int i = 0; i < kRosterSize; ++i) {
        // John can't ride alongside himself.
        if (std::string(kRoster[i].key) == "john" && !mode::PlayAsArthur()) continue;
        TickMember(i, playerPos, playerMounted, now);
    }
}

void RosterPage() {
    ui::Title("Gang");
    ui::Text("Posse: " + std::to_string(PosseCount()) + " / " + std::to_string(kMaxPosse));
    for (int i = 0; i < kRosterSize; ++i) {
        if (ui::Item(kRoster[i].name, StatusText(i))) {
            ui::Push([i] { MemberPage(i); });
        }
    }
    if (ui::Option("Followers can't die", config::GetInt("gang", "invincible", 0) ? "On" : "Off") != 0) {
        bool on = !config::GetInt("gang", "invincible", 0);
        config::SetInt("gang", "invincible", on);
        config::Save();
        for (auto& s : g_state) {
            if (Alive(s.ped)) SET_ENTITY_INVINCIBLE(s.ped, on);
        }
    }
}

void CommandsPage() {
    ui::Title("Orders");
    ui::Text(std::string("Current order: ") + (g_order == kFollow ? "Follow me" : "Wait here"));
    if (ui::Item("Follow me", "F6")) OrderFollow();
    if (ui::Item("Wait here", "F7")) OrderWait();
    if (ui::Item("Attack my target", "F8")) OrderAttackTarget();
}

void OrderFollow() {
    g_order = kFollow;
    config::SetInt("gang", "order_wait", 0);
    config::Save();
    for (int i = 0; i < kRosterSize; ++i) {
        if (g_state[i].status == kPosse) ApplyOrder(i);
    }
    if (PosseCount()) ui::Notify("\"On me, let's ride.\"");
}

void OrderWait() {
    g_order = kWait;
    config::SetInt("gang", "order_wait", 1);
    config::Save();
    for (int i = 0; i < kRosterSize; ++i) {
        if (g_state[i].status == kPosse) ApplyOrder(i);
    }
    if (PosseCount()) ui::Notify("\"Hold here. Keep your eyes open.\"");
}

void OrderAttackTarget() {
    Entity target = 0;
    Player me = PLAYER_ID();
    if (!GET_ENTITY_PLAYER_IS_FREE_AIMING_AT(me, &target) || !target) GET_PLAYER_TARGET_ENTITY(me, &target);
    if (!target || !IS_ENTITY_A_PED(target) || IS_ENTITY_DEAD(target)) {
        ui::Notify("Aim at someone first, then give the order.");
        return;
    }
    for (int i = 0; i < kRosterSize; ++i) {
        State& st = g_state[i];
        if (st.ped == target) {
            ui::Notify("That's one of ours!");
            return;
        }
    }
    int n = 0;
    for (int i = 0; i < kRosterSize; ++i) {
        State& st = g_state[i];
        if (st.status == kPosse && Alive(st.ped)) {
            TASK_COMBAT_PED(st.ped, target, 0, 16);
            n++;
        }
    }
    ui::Notify(n ? "\"Take 'em down!\"" : "Nobody is riding with you.");
}

void GatherAroundPlayer() {
    for (int i = 0; i < kRosterSize; ++i) {
        State& st = g_state[i];
        if (st.status != kPosse || !Alive(st.ped)) continue;
        float ox, oy;
        SlotOffset(PosseSlot(i), &ox, &oy);
        Vector3 p = GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(util::PlayerOrMount(), ox, oy, 0.0f);
        float z = util::GroundZ(p.x, p.y, p.z);
        Entity mover = (st.horse && IS_PED_ON_MOUNT(st.ped)) ? st.horse : st.ped;
        SET_ENTITY_COORDS(mover, p.x, p.y, z, FALSE, FALSE, FALSE, FALSE);
        if (mover == st.ped && Alive(st.horse)) {
            SET_ENTITY_COORDS(st.horse, p.x + 2.0f, p.y, z, FALSE, FALSE, FALSE, FALSE);
        }
        st.nextTask = 0;
    }
}

}  // namespace gang
