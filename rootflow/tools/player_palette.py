"""The player's 15 colors (+ transparent). The ChatGPT prompt lists these exact hex values,
and tools/import_sheet.py --grid snaps the designer image to them."""

PAL = [
    (255, 0, 255),    # 0 transparent
    (22, 14, 30),     # 1 outline
    (48, 54, 128),    # 2 blue dark
    (78, 90, 190),    # 3 blue
    (118, 136, 224),  # 4 blue light
    (242, 200, 90),   # 5 gold
    (196, 138, 60),   # 6 gold dark
    (246, 208, 170),  # 7 skin
    (214, 160, 122),  # 8 skin shade
    (122, 74, 44),    # 9 hair
    (78, 48, 32),     # A boots
    (170, 114, 62),   # B staff wood
    (120, 170, 96),   # C pack green
    (70, 116, 70),    # D pack green dark
    (250, 246, 236),  # E white
    (104, 66, 42),    # F belt leather
]
