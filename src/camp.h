// Your camp: pitch it anywhere or at a preset spot, and the gang members who
// aren't riding with you hang around it.
#pragma once
#include "scripthook.h"

namespace camp {

bool Active();
Vector3 Center();
// A spot around the fire for gang member number i, facing the fire.
Vector3 Slot(int i, float* heading);
// True when the player is close enough that camp should be "alive".
bool PlayerNearby();

void Tick();
void MenuPage();

}  // namespace camp
