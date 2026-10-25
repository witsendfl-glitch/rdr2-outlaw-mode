#include "travel.h"

#include <string>

#include "gang.h"
#include "mode.h"
#include "places.h"
#include "ui.h"
#include "util.h"

namespace travel {
namespace {

constexpr int kGuarmaFare = 25;
constexpr int kCoachFare = 15;
constexpr float kPierRadius = 20.0f;
constexpr float kStationRadius = 60.0f;

// Zone hashes from femga/rdr3_discoveries (zones/README.md).
constexpr Hash kZoneGuarma = 1935063277u;
constexpr Hash kZoneWorld = static_cast<Hash>(-1868977180);

bool g_guarmaWorld = false;
int g_nextCheck = 0;
Blip g_pierBlip = 0;

void SetGuarmaWorld(bool on) {
    SET_GUARMA_WORLDHORIZON_ACTIVE(on);
    SET_WORLD_WATER_TYPE(on ? 1 : 0);
    SET_MINIMAP_ZONE(on ? kZoneGuarma : kZoneWorld);
    g_guarmaWorld = on;
}

void Go(const Place& to, float heading) {
    bool toGuarma = places::InGuarma(to.x, to.y);
    if (toGuarma != g_guarmaWorld) SetGuarmaWorld(toGuarma);
    util::Teleport(to.x, to.y, to.z, heading);
    gang::GatherAroundPlayer();
}

const Place* NearestStation(const Vector3& pos) {
    for (const Place& p : places::kCoachStations) {
        if (util::Dist(pos, V3(p.x, p.y, p.z)) < kStationRadius) return &p;
    }
    return nullptr;
}

bool NearColter(const Vector3& pos) {
    return util::Dist(pos, V3(places::kColter.x, places::kColter.y, places::kColter.z)) < kStationRadius;
}

bool NearPlace(const Vector3& pos, const Place& p, float radius) {
    return util::Dist(pos, V3(p.x, p.y, p.z)) < radius;
}

void EnsurePierBlip() {
    if (g_pierBlip && DOES_BLIP_EXIST(g_pierBlip)) return;
    const Place& p = places::kSaintDenisPier;
    g_pierBlip = BLIP_ADD_FOR_COORDS(util::Joaat("BLIP_STYLE_CREATOR_DEFAULT"), p.x, p.y, p.z);
    SET_BLIP_SPRITE(g_pierBlip, util::Joaat("blip_ambient_riverboat"), TRUE);
    SET_BLIP_NAME(g_pierBlip, "Passage to Guarma");
}

}  // namespace

bool AnyAvailable() {
    Vector3 pos = util::PlayerPos();
    return NearPlace(pos, places::kSaintDenisPier, kPierRadius) ||
           NearPlace(pos, places::kGuarmaPier, kPierRadius) || NearestStation(pos) || NearColter(pos);
}

void Tick() {
    if (!mode::Enabled()) return;
    EnsurePierBlip();

    int now = util::Now();
    if (now >= g_nextCheck) {
        g_nextCheck = now + 500;
        Vector3 pos = util::PlayerPos();
        bool inGuarma = places::InGuarma(pos.x, pos.y);
        if (inGuarma != g_guarmaWorld) SetGuarmaWorld(inGuarma);
    }

    if (!ui::IsOpen() && AnyAvailable()) {
        ui::DrawLabel("Travel available: press F5, then Travel", 0.40f, 0.90f, 0.32f, 240, 220, 160, 255);
    }
}

void MenuPage() {
    ui::Title("Travel");
    Vector3 pos = util::PlayerPos();
    bool any = false;

    if (NearPlace(pos, places::kSaintDenisPier, kPierRadius)) {
        any = true;
        if (ui::Item("Book passage to Guarma", "$" + std::to_string(kGuarmaFare)) &&
            util::Charge(kGuarmaFare, "passage to Guarma")) {
            ui::Close();
            ui::Notify("You board a steamer bound for Guarma...");
            Go(places::kGuarmaArrival, 0.0f);
        }
    }
    if (NearPlace(pos, places::kGuarmaPier, kPierRadius)) {
        any = true;
        if (ui::Item("Book passage to Saint Denis", "$" + std::to_string(kGuarmaFare)) &&
            util::Charge(kGuarmaFare, "passage to Saint Denis")) {
            ui::Close();
            ui::Notify("You sail back to Saint Denis...");
            Go(places::kSaintDenisArrival, 180.0f);
        }
    }
    if (const Place* station = NearestStation(pos)) {
        any = true;
        if (ui::Item("Hire a carriage to Colter", "$" + std::to_string(kCoachFare)) &&
            util::Charge(kCoachFare, "a carriage to Colter")) {
            ui::Close();
            ui::Notify(std::string("The driver takes you up the mountain from ") + station->name + "...");
            Go(places::kColter, 0.0f);
        }
    }
    if (NearColter(pos)) {
        any = true;
        for (const Place& p : places::kCoachStations) {
            if (ui::Item(std::string("Hire a carriage to ") + p.name, "$" + std::to_string(kCoachFare)) &&
                util::Charge(kCoachFare, "a carriage")) {
                ui::Close();
                ui::Notify(std::string("The driver takes you down to ") + p.name + "...");
                Go(p, 0.0f);
            }
        }
    }
    if (!any) {
        ui::Text("Nothing to book here.");
        ui::Text("Boats: Saint Denis harbor pier.");
        ui::Text("Carriages to Colter: Valentine, Strawberry,");
        ui::Text("Emerald Ranch, Annesburg, Rhodes, Blackwater.");
    }
}

}  // namespace travel
