// Map coordinates used across the mod.
//
// Sources: rdr2mods.com's teleport list and the open-source hails_guarma
// RedM script (Saint Denis pier and Guarma pier).
#pragma once

struct Place {
    const char* name;
    float x, y, z;
};

namespace places {

constexpr Place kSaintDenisPier = {"Saint Denis harbor", 2671.13f, -1552.96f, 46.47f};
constexpr Place kSaintDenisArrival = {"Saint Denis harbor", 2670.69f, -1547.34f, 45.82f};
constexpr Place kGuarmaPier = {"Guarma pier", 1265.84f, -6852.16f, 43.42f};
constexpr Place kGuarmaArrival = {"Guarma pier", 1269.33f, -6851.98f, 43.17f};

constexpr Place kColter = {"Colter", -1343.56f, 2425.95f, 307.40f};

// Where a carriage to Colter can be hired.
constexpr Place kCoachStations[] = {
    {"Valentine", -231.34f, 703.27f, 113.74f},
    {"Strawberry", -1731.43f, -412.90f, 154.87f},
    {"Emerald Ranch", 1418.27f, 316.29f, 88.49f},
    {"Annesburg", 2904.37f, 1248.81f, 44.87f},
    {"Rhodes", 1232.21f, -1251.09f, 73.68f},
    {"Blackwater", -798.98f, -1247.72f, 43.43f},
};

// Preset camp spots: the gang's old camps plus wild spots in the regions
// where Red Dead Online lets you pitch a camp. Approximate; you can also
// pitch anywhere and save your own spots from the camp menu.
constexpr Place kCampSpots[] = {
    {"Horseshoe Overlook (Heartlands)", -69.79f, 109.28f, 89.52f},
    {"Clemens Point (Lemoyne)", 843.99f, -1029.49f, 51.99f},
    {"Beaver Hollow (Roanoke Ridge)", 2401.96f, 1237.94f, 102.54f},
    {"Colter (Grizzlies)", -1343.56f, 2425.95f, 307.40f},
    {"Shady Belle (Bayou)", 1763.23f, -1772.34f, 50.68f},
    {"Lagras (Bayou Nwa)", 2105.41f, -682.16f, 42.27f},
    {"Barrow Lagoon (Grizzlies West)", -1022.50f, 1682.61f, 242.16f},
    {"Monto's Rest (Big Valley)", -1389.25f, -235.02f, 98.86f},
    {"Upper Montana River (Big Valley)", -2003.78f, -1058.10f, 77.03f},
    {"Manzanita Post (Tall Trees)", -1962.67f, -1614.49f, 116.03f},
    {"Rathskeller Fork (Rio Bravo)", -5165.08f, -2143.50f, 11.77f},
    {"Ridgewood Farm (Cholla Springs)", -4823.54f, -2711.38f, -13.75f},
};

inline bool InGuarma(float x, float y) { return x >= 0.0f && y <= -4096.0f; }

}  // namespace places
