# RDR2 Outlaw Mode

A Red Dead Redemption 2 script mod that turns your epilogue save into a
free-roam outlaw sandbox. You play as Arthur, ride with the old Van der Linde
gang, and pitch your camp wherever you like. The story missions stay finished,
and your normal story saves aren't touched.

**Version 0.2 (test build).** It covers all four steps of the plan. Nothing
has been played in-game yet, so expect rough edges. `docs/TESTING.md` lists
what to try.

## What's in it

| Feature | How it works |
| --- | --- |
| **Outlaw Mode switch** | Turn it on in the mod menu while your epilogue save is loaded. Settings live in `Documents\RDR2 Outlaw Mode\outlawmode.ini`, separate from game saves. |
| **Play as Arthur** | Swaps the player to Arthur and keeps him there after deaths and reloads. The epilogue has no tuberculosis, so he stays healthy. 74 outfits to choose from. |
| **Gang recruitment** | Recruit 22 members of the gang: Dutch, Hosea, John, Sadie, Charles, Javier, Bill, Lenny, Micah, Sean, Kieran, Uncle, Pearson, Tilly, Karen, Mary-Beth, Abigail, Grimshaw, Molly, Strauss, Swanson, and Trelawny. Up to 6 ride with you. The rest wait at camp. |
| **Orders** | Follow me (F6), Wait here (F7), Attack my target (aim at someone, then press F8). Followers mount up when you ride and get down when you walk. |
| **Downed followers** | A follower who goes down recovers at camp after 10 minutes. You can also make followers unkillable. |
| **Camp** | Pitch it where you stand, or move it to 12 preset spots: the gang's old camps plus wild spots in the regions where Red Dead Online lets you camp. You can also save your own spots. The camp has a campfire, tents, and bedrolls. Gang members not riding with you hang around the fire. You can rest there for 6 hours or ride back from anywhere. |
| **Guarma by boat** | Book passage ($25) at the Saint Denis harbor pier, and come back from the Guarma pier. The sea and the map switch over correctly. |
| **Gang wars** | Eight rival gangs hold 18 territories, shown on the map in their own colour. Bronte's Italians run four parts of Saint Denis plus Caliga Hall. The other gangs are the O'Driscolls, Lemoyne Raiders, Murfree Brood, Skinner Brothers, Del Lobo, Laramie, and Night Folk. Every few minutes a gang moves on a neighbour, and you'll hear about it. Fights can break out in front of you. |
| **Hideouts** | Kill everyone holding a hideout and it's yours. The Italians and Laramie leave you alone until you choose "Make a move on this place". Your hideouts get guards, earn money every minute, and take 3 levels of upgrades. Rival gangs attack them, and if you ride there within 5 minutes, you can defend them. |
| **Properties** | Buy one of 2 homesteads, 2 ranches, or 2 saloons. Each one earns money every minute, takes 3 upgrades (such as a cattle herd, horse breeding, or card tables), and adds hands and livestock. Collect the money on site or all at once from camp. |
| **Heists** | Bank jobs at Valentine, Rhodes, Saint Denis, and Armadillo. Pick a target, case it, then pull it loud (guards plus waves of law) or quiet (night only, with a chance someone raises the alarm). Crack the vault, then ride 500m out to get paid. You can also hit a payroll train: catch it, stop the engine, fight off the guards, open the strongbox, and outrun the marshals. Each posse member adds 10% to the take. A robbed bank lies low for 45 minutes. |
| **Colter** | Hire a carriage ($15) to Colter from Valentine, Strawberry, Emerald Ranch, Annesburg, Rhodes, or Blackwater. Colter has townsfolk, a campfire, and a general store that sells ammo, meals, and snake oil. |

## Install

1. Install [Script Hook RDR2](http://www.dev-c.com/rdr2/scripthookrdr2/). Copy `ScriptHookRDR2.dll` and `dinput8.dll` into the game folder.
2. Copy `OutlawMode.asi` into the same game folder, next to `RDR2.exe`.
3. Start the game and load your **epilogue / 100% save**.
4. Press **F5** and set **Outlaw Mode** to **On**.

To uninstall, delete `OutlawMode.asi`.

## Controls

| Key | Action |
| --- | --- |
| F5 | Open or close the menu |
| Arrow keys / Enter / Backspace | Move / select / go back (the numpad works too) |
| F6 | Order: follow me |
| F7 | Order: wait here |
| F8 | Order: attack the person you're aiming at |

You can change these keys in `outlawmode.ini` under `[keys]`. Values are
[Windows virtual-key codes](https://learn.microsoft.com/windows/win32/inputdev/virtual-key-codes)
written as decimal numbers. For example, F5 is 116.

## Known limits in 0.1

- The mode starts from a save and a menu switch, not from a new option on the
  game's main menu. The game doesn't let scripts add main menu entries.
- Carriages and boat trips fade to black and move you. There's no ride in between yet.
- The preset camp spots are approximate. Pitch exactly where you want, then use
  "Save this spot".
- Colter's store is a menu, not the game's catalog screen.
- Property and hideout positions are town and site centres, so hands and guards
  may stand in slightly odd spots until we tune them in-game.
- The payroll train runs on the regular rail network from a nearby junction.

## Building

The mod is plain C++17 with no Script Hook SDK dependency. It finds Script
Hook's functions when the game loads it.

```
sudo apt install cmake g++-mingw-w64-x86-64-posix
./build.sh   # -> build/OutlawMode.asi
```

Visual Studio works too: `cmake -S . -B build -A x64 && cmake --build build --config Release`.

`src/natives.h` is generated from
[alloc8or/rdr3-nativedb-data](https://github.com/alloc8or/rdr3-nativedb-data)
by `tools/gen_natives.py`. Add a name to `tools/natives.txt` and regenerate it.
