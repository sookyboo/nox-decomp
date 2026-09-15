- Added Tier 4.5/5 Reloaded/Panic EUD compatibility for discard-handler chaining, portable EUD allocations, typed ThingDB/GameData access, DwordCopy and MagicMissile without executing injected x86
- Added Tier 3 Reloaded/Panic EUD semantic compatibility for known spell, object, monster-action, wall, NPC equipment, sound and simple FX helpers without executing injected x86
- Extended Reloaded/Panic EUD compatibility with portable live object/player pointer tokens and semantic UnitToPtr support without executing injected x86
- Added initial Reloaded/Panic EUD map compatibility for checked legacy BYTE/WORD/DWORD data-memory access without executing injected x86
- Fixed all known rendering issues (obliterate, force of nature, dispell undead, lasers, unclear fonts caused due to scaling)
- Fixed all known audio glitches
- Play native audio formats instead of requiring conversion
- Fixed a rare segfault on lynch with conjurer staff and logic issues around its summoning
- Improved windows launcher, doesn't bring up a dos window in the background anymore, stays open in the background and provides verbose logging to try and find out why sometimes it takes multiple launches to launch the game
- Windows launcher now can install either from installed directory or gog setup file
- Fixes gamepads on windows (includes the config file and finds it in launcher directory)
- Introduces new gamepad modes can toggle between them with START + L1 or START + R1 or START + R1 + R2 - the R1 options directly control the player with the left thumb stick
- Fully built from source code all directly in github actions
- Intro video is displayed on first start
- Remove resolution settings when the launcher is used because the launcher picks the best resolution for you
- Remove unlock surfaces option in graphics
- Remove 8bit mode
- Multiplayer lobby server failover detection and two backup servers so that multiplayer never goes down again
- Fix game pad skipping cutscenes
- Consistency fixes across launchers for different platforms

There is one known issue on linux with btrfs in multiplayer where custom maps are downloaded even if present - there are no known other issues

Thanks szhublox for all your fixes and testing
Thanks Lovyxia, F1rehand, bmdhacks for suggestions and testing
Thanks Bluey, JP440, Mongrethod | Marcel, MadBrother, Butter, Fraxinus88, LordAinz, ZOMGUgoff, Ganimoth, kloptops  for your testing
Thanks Kemosabe for spontaneously helping to test the multiplayer version with a good multiplayer game