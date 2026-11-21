# TlutPreview

A small Qt app that takes a texture pack folder extracted by
[BitDeck](https://github.com/Jepvid/BitDeck) ("Extract textures from otr/o2r",
which writes PNGs plus a `manifest.json`) and writes a copy of it with:

- **Apply TLUT**: CI4/CI8 textures recolored with their real palette instead
  of the synthetic grayscale one.
- **Apply IA Transparency**: I8 textures known to be drawn translucent
  (glow/spark/dust) given alpha = intensity.

The source folder is never modified. The copy's `manifest.json` records the
rewritten PNGs' new hashes, so it can be opened in BitDeck's Make Texture
Pack like any other extracted folder.

## How textures are matched

Only what is in the folder is used:

- TLUTs: the per-game baked CI-texture → TLUT tables (MK64, MM, OOT, PM64,
  SF64), PM64's palette naming conventions, and name correspondence with a
  `*TLUT` / `_tlut_` / `_palette` sibling (BK64, MK64, OOT naming).
- I8 translucency: the per-game baked tables (BK64, MM, OOT, PM64).

BitDeck's own Apply TLUT also scans the archive's display lists, which an
extracted folder doesn't contain, so textures paired only that way stay
grayscale here. The status line after a run shows how many CI textures had no
TLUT found.

## Building

Needs CMake 3.21+, a C++20 compiler, Qt 6 (Core, Widgets), libpng and libjpeg.

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/TlutPreview
ctest --test-dir build
```

## Code from BitDeck

`third_party/bitdeck/` holds BitDeck's texture codec and per-game tables,
copied unmodified. `third_party/bitdeck/SOURCE` names the BitDeck commit they
came from. To pick up newer tables:

```sh
tools/sync_from_bitdeck.sh ../BitDeck
```
