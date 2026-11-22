/* Included ahead of every source (cflags -include): stand-ins for firmware
 * headers that a neighbour includes in quotes, so they would be found in the
 * firmware's own folder first. Each stand-in uses the original's include
 * guard, so the original is then skipped. */
#include "reset.h" /* EmuMD's template stand-in: no ARM code */
