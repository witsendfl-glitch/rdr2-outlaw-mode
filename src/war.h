// Gang wars: rival gangs hold territories and hideouts, fight each other for
// them over time, and you can take hideouts, upgrade them and collect income.
#pragma once

namespace war {

void Tick();
void MapPage();     // territory list ("gang map")
void HerePage();    // actions for the territory you're standing in
bool AtTerritory();  // standing inside any territory
int CollectAll();   // empties every owned hideout's strongbox; returns dollars

}  // namespace war
