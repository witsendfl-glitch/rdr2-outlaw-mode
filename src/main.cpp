// RDR2 Outlaw Mode: entry point, main loop and the root menu.
#include <windows.h>

#include <string>

#include "camp.h"
#include "colter.h"
#include "config.h"
#include "gang.h"
#include "mode.h"
#include "travel.h"
#include "ui.h"
#include "util.h"
#include "war.h"

#define OUTLAW_MODE_VERSION "0.1.0"

namespace {

int Key(const char* name, int fallback) {
    int vk = config::GetInt("keys", name, 0);
    if (vk <= 0) {
        config::SetInt("keys", name, fallback);
        vk = fallback;
    }
    return vk;
}

void ArthurPage() {
    ui::Title("Arthur");
    if (ui::Option("Play as Arthur", mode::PlayAsArthur() ? "On" : "Off") != 0) {
        mode::SetPlayAsArthur(!mode::PlayAsArthur());
    }
    int delta = ui::Option("Outfit", std::to_string(mode::Outfit() + 1) + " / " + std::to_string(mode::OutfitCount()));
    if (delta != 0) mode::SetOutfit(mode::Outfit() + delta);
    ui::Text("Arthur is healthy here: no tuberculosis.");
}

void RootPage() {
    ui::Title("Outlaw Mode " OUTLAW_MODE_VERSION);
    if (ui::Option("Outlaw Mode", mode::Enabled() ? "On" : "Off") != 0) {
        mode::SetEnabled(!mode::Enabled());
    }
    if (!mode::Enabled()) {
        ui::Text("Load your epilogue save first, then");
        ui::Text("turn Outlaw Mode on. Your normal story");
        ui::Text("saves are left alone while it's off.");
        return;
    }
    if (ui::Item("Gang", std::to_string(gang::PosseCount()) + " riding")) ui::Push(gang::RosterPage);
    if (ui::Item("Orders")) ui::Push(gang::CommandsPage);
    if (ui::Item("Camp", camp::Active() ? "pitched" : "")) ui::Push(camp::MenuPage);
    if (ui::Item("Gang Territories")) ui::Push(war::MapPage);
    if (war::AtTerritory() && ui::Item("This territory")) ui::Push(war::HerePage);
    if (ui::Item("Travel", travel::AnyAvailable() ? "available here" : "")) ui::Push(travel::MenuPage);
    if (colter::NearStore() && ui::Item("Colter General Store")) ui::Push(colter::StorePage);
    if (ui::Item("Arthur")) ui::Push(ArthurPage);
}

bool GameReady() {
    if (GET_IS_LOADING_SCREEN_ACTIVE() || IS_LOADING_SCREEN_VISIBLE()) return false;
    return IS_PLAYER_PLAYING(PLAYER_ID()) != 0;
}

void ScriptMain() {
    config::Load();
    int menuKey = Key("menu", VK_F5);
    int followKey = Key("follow", VK_F6);
    int waitKey = Key("wait", VK_F7);
    int attackKey = Key("attack", VK_F8);
    config::Save();
    util::Log("Outlaw Mode " OUTLAW_MODE_VERSION " started");

    while (true) {
        ui::UpdateInput();
        if (GameReady() && !IS_PAUSE_MENU_ACTIVE()) {
            if (ui::KeyPressed(menuKey)) {
                if (ui::IsOpen()) ui::Close();
                else ui::Open(RootPage);
            }
            mode::Tick();
            travel::Tick();
            colter::Tick();
            camp::Tick();
            gang::Tick();
            war::Tick();
            if (mode::Enabled()) {
                if (ui::KeyPressed(followKey)) gang::OrderFollow();
                if (ui::KeyPressed(waitKey)) gang::OrderWait();
                if (ui::KeyPressed(attackKey)) gang::OrderAttackTarget();
            }
            ui::Run();
            ui::DrawNotifications();
        }
        WAIT(0);
    }
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        if (sh::Resolve()) sh::scriptRegister(module, ScriptMain);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (sh::scriptUnregister) sh::scriptUnregister(module);
    }
    return TRUE;
}
