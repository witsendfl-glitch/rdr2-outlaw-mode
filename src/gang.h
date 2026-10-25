// Recruitable members of the Van der Linde gang who ride with you as
// followers or wait at your camp.
#pragma once

namespace gang {

void Tick();
void RosterPage();    // recruit and manage members
void CommandsPage();  // orders for everyone riding with you

// Orders, also bound to hotkeys.
void OrderFollow();
void OrderWait();
void OrderAttackTarget();

// After a teleport, bring the posse along.
void GatherAroundPlayer();

int PosseCount();

}  // namespace gang
