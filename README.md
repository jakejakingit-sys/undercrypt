# UNDERCRYPT — TI-84 Plus CE dungeon game

UNDERCRYPT is a room-by-room action dungeon with three real-time guardian fights. Arrow keys move, 2ND fires, and ALPHA dashes while leaving an afterimage that attracts aimed attacks. The Bone Regent fires marked volleys, the Glass Widow marks and sweeps laser lanes, and the Hollow Engine sends rings with a visible safe gap. Each guardian speeds up its pattern below half health.

Rooms, obstacles, enemy placement, and attack timing vary each run. Beating a guardian increases your fire rate and restores one health point. Find the mint healing sigil in some rooms. Clear or lose ends the run; press ENTER to start another randomized descent.

All graphics are drawn from small colored shapes in the program. There are no external sprite or data files.

## Controls

- Arrow keys: move
- 2ND: fire; hold to fire repeatedly
- ALPHA: dash and leave a decoy afterimage
- CLEAR: pause; ENTER resumes, CLEAR returns to title
- ENTER: start or replay

## Build an 8XP in GitHub

1. Create a GitHub repository and upload the contents of this folder to its root.
2. Commit and push. The included `.github/workflows/main.yml` builds `bin/UNDERCRYPT.8xp`.
3. Open **Actions**, download the `UNDERCRYPT-8XP` artifact, and send the `.8xp` file to the calculator with TI Connect CE.

The workflow rejects a program larger than **76,800 bytes (75 KiB)**. It uses CEdev's C toolchain; no AppVars are required.
