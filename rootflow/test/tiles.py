"""Decode a peek of four map tiles (one 32-bit word) into tile names. Usage: python3 tiles.py VALUE..."""
import sys
NAMES = ("DEAD DEAD_CRACK DIRT GRASS GRASS_TUFT FLOWERS TREE DEAD_TREE ROCK WATER WATER_B BRIDGE THORN "
         "HOMESITE CHEST CHEST_OPEN WRECK DRY_GRASS PEBBLES MENHIR DRY_RIVER DRY_BRIDGE PATH FLOWERBED "
         "FLOWERBED_EMPTY SAPLING FRUIT_TREE FRUIT_TREE_BARE LANTERN BENCH DAM LEVER LEVER_PULLED LOST_STONE "
         "BLOSSOM FOUNTAIN STATUE PINWHEEL VAEL_PUMP VAEL_PUMP_B "
         + " ".join(f"{h}{l}_{p}" for h in ("GROVE", "SPRING", "CAIRN", "NOOK") for l in (1, 2) for p in ("ROOF", "BASE"))).split()
for v in sys.argv[1:]:
    w = int(v) & 0xFFFFFFFF
    print(" ".join(NAMES[(w >> (8 * k)) & 255] if ((w >> (8 * k)) & 255) < len(NAMES) else "?" for k in range(4)))
