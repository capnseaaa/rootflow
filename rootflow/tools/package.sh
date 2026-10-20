#!/bin/sh
# Package for the R36. release/gba/ mirrors the SD card's gba/ folder:
#   release/gba/rootflow.gba
#   release/gba/downloaded_images/rootflow.png   (cover)
# Copy those into gba/ on the SD card.
# The card already has its own gba/gamelist.xml (one entry per game, ~1900 games).
# Never replace it. release/gamelist-entry.xml is one <game> block to insert
# before </gameList> in that file (once; later builds only need the ROM copied).
set -e
cd "$(dirname "$0")/.."
python3 tools/make_cover.py
rm -rf release
mkdir -p release/gba/downloaded_images
cp rootflow.gba release/gba/rootflow.gba
cp art/cover.png release/gba/downloaded_images/rootflow.png
cat > release/gamelist-entry.xml <<XML
	<game>
		<path>./rootflow.gba</path>
		<name>ROOTFLOW</name>
		<desc>Small roots. Big change. Walk a dead gray valley, befriend land spirits, and bring life back.</desc>
		<image>./downloaded_images/rootflow.png</image>
		<developer>Caleb</developer>
		<genre>Adventure</genre>
		<players>1</players>
	</game>
XML
echo "packaged:"; find release -type f
