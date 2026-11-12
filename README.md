# Purple vs Green

An offline fantasy arena shooter for Linux. Play 3v3 Team Deathmatch at the Meridian as either Green or Purple. Each team fields the same battlemages. Win by reaching the elimination limit or by leading when time expires.

| Control | Action |
| --- | --- |
| WASD / mouse | Move / look |
| Left Shift / Space | Sprint / jump |
| Left mouse | Cast the selected spell |
| 1 / 2 / 3 / 4 | Witchthread / Briar Fan / Glass Lance / Cinder Comet |
| Q | Raise a ward |
| Escape | Pause or resume |

## Controller

PlayStation and Xbox-style gamepads use Unreal's native gamepad input. On DualShock 4, the left stick moves, the right stick aims, L3 toggles sprint, and Cross jumps. R2 casts, L2 wards, L1 and R1 cycle spells, and Options pauses. In menus use the D-pad or left stick to move, Cross to confirm, and Circle to go back. Use left and right on a focused slider. Xbox-style equivalents are A, B, RT, LT, LB, RB and Menu.

Settings include stick sensitivity, inverted vertical aim, and vibration. Vibration depends on controller and platform support.

Focus recovers on its own. Witchthread is fast, Briar Fan is strongest up close, Glass Lance channels before releasing, and Cinder Comet bursts on contact. A ward reduces incoming damage briefly and has its own recharge time.

## Build and run

Use Unreal Engine 5.8.4 (CL 58902052) with the bundled Linux Clang 20.1.8 SDK and a Vulkan-capable GPU. Clone the private repository to the development drive to place project intermediates there. Set these paths for your installation and an existing directory on that drive.

```bash
export PVG_ENGINE=/path/to/UE_5.8.4
export PVG_STORAGE=/path/to/pvg-build-output
Tests/run.sh
Tools/ue.sh build
Tools/ue.sh run
Tools/ue.sh package Development
Tools/ue.sh smoke /path/from-package-command/Linux
```

Launch a standalone package with `./PurpleVsGreen.sh` from its Linux directory. The adjacent build manifest records the full Git revision and SHA-256 hashes. Run the smoke command from a rendered desktop session. It uses an isolated profile and completes two matches with opposite team selections.

See [Engineering](Docs/Engineering.md) and the [asset ledger](Docs/Assets.md).

Copyright © 2026 Markus Gillyard. All rights reserved.
