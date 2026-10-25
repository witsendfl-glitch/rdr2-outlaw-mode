// The Outlaw Mode switch and playing as Arthur.
#pragma once

namespace mode {

bool Enabled();
void SetEnabled(bool on);

bool PlayAsArthur();
void SetPlayAsArthur(bool on);

int Outfit();
void SetOutfit(int preset);  // applies immediately when playing as Arthur
int OutfitCount();

void Tick();

}  // namespace mode
