OPTIONAL CONTROLLER MAPPINGS
============================

DIGS uses SDL2's built-in controller database, which already recognises
Xbox 360/One/Series, DualShock 3 and 4, DualSense, Switch Pro, Joy-Con,
and Steam Deck pads with no extra files.

If your controller is not detected, copy the community mapping database
into this directory as:

    gamecontrollerdb.txt

A copy is included with this bundle at:

    extras/gamecontrollerdb.txt

or download the current version from:

    https://github.com/mdqinc/SDL_GameControllerDB

You can also set DIGS_GAMECONTROLLERDB to any mapping file before launching
DIGS. The supplied Windows launcher automatically uses a mapping copied to the
root-level share/digs/controllers directory.

The database is not installed by default because it is larger than the
game itself and almost never needed.
