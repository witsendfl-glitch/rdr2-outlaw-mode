// Buyable homesteads, ranches and saloons that earn money while you ride.
#pragma once

namespace property {

void Tick();
void MenuPage();   // list of properties
int CollectAll();  // empties every strongbox; returns dollars

}  // namespace property
