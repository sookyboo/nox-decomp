# Gameplay cheats

The gameplay cheat commands are split across `src/GAME1.c` and
`src/GAME3.c`. `sub_4EF500` owns the god-mode toggle and must modify only the
`NOX_CHEAT_GOD` bit (`0x20`) in `byte_5D4594[2650636]`. It deliberately does
not iterate players or call the spell-table functions, so enabling god mode
does not change learned spells or creature knowledge.

`sub_442480` and `sub_4424C0` own the sage/spells toggle. Their shared helper
`nox_apply_spells_cheat_to_all` checks the same cheats-allowed gate, toggles
`NOX_CHEAT_SPELLS` (`0x10`), then iterates `sub_416EA0`/`sub_416EE0` and
re-applies `sub_4EFD80`, `sub_4EFC80`, and `sub_4EFE10` for each active player.
This reapplication is the observed route by which sage changes spell tables;
the exact contents of those tables are reverse-engineered and not fully
documented here.

`tests/cheat_spell_test.c` drives test-only wrappers around both production
flag owners used by the command handlers. It verifies god and spell flags are
isolated. The shared
cheats-allowed gate is also checked with a deterministic test-build gate;
player spell-table contents and console/UI messaging remain integration
coverage.
