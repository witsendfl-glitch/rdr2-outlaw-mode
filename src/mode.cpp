#include "mode.h"

#include "config.h"
#include "ui.h"
#include "util.h"

namespace mode {
namespace {

// player_zero is Arthur's model; the game lists 74 outfit presets for it.
constexpr int kArthurOutfits = 74;
int g_nextCheck = 0;

Hash ArthurModel() {
    static Hash h = util::Joaat("player_zero");
    return h;
}

void ApplyOutfit(Ped ped) {
    EQUIP_META_PED_OUTFIT_PRESET(ped, Outfit(), FALSE);
}

void BecomeArthur() {
    Hash model = ArthurModel();
    if (!util::LoadModel(model)) {
        util::Log("Could not load player_zero model");
        return;
    }
    Ped before = PLAYER_PED_ID();
    int health = GET_ENTITY_HEALTH(before);
    SET_PLAYER_MODEL(PLAYER_ID(), model, FALSE);
    WAIT(0);
    Ped arthur = PLAYER_PED_ID();
    ApplyOutfit(arthur);
    if (health > 0) SET_ENTITY_HEALTH(arthur, health, 0);
    SET_MODEL_AS_NO_LONGER_NEEDED(model);
    util::Log("Switched player to Arthur");
}

}  // namespace

bool Enabled() { return config::GetInt("mode", "enabled", 0) != 0; }

void SetEnabled(bool on) {
    config::SetInt("mode", "enabled", on ? 1 : 0);
    config::Save();
    ui::Notify(on ? "Outlaw Mode is on." : "Outlaw Mode is off. Your followers and camp are saved.");
}

bool PlayAsArthur() { return config::GetInt("mode", "arthur", 1) != 0; }

void SetPlayAsArthur(bool on) {
    config::SetInt("mode", "arthur", on ? 1 : 0);
    config::Save();
    if (!on) ui::Notify("Arthur stays until you reload your save.");
}

int Outfit() { return config::GetInt("mode", "outfit", 0); }

void SetOutfit(int preset) {
    preset = (preset % kArthurOutfits + kArthurOutfits) % kArthurOutfits;
    config::SetInt("mode", "outfit", preset);
    config::Save();
    Ped ped = PLAYER_PED_ID();
    if (GET_ENTITY_MODEL(ped) == ArthurModel()) ApplyOutfit(ped);
}

int OutfitCount() { return kArthurOutfits; }

void Tick() {
    if (!Enabled() || !PlayAsArthur()) return;
    int now = util::Now();
    if (now < g_nextCheck) return;
    g_nextCheck = now + 1000;

    Ped ped = PLAYER_PED_ID();
    if (IS_ENTITY_DEAD(ped)) return;
    // Reloads, deaths and cutscenes can restore John, so keep checking.
    if (GET_ENTITY_MODEL(ped) != ArthurModel()) BecomeArthur();
}

}  // namespace mode
