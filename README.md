# TlutPreview

A companion tool that makes textures exported by
[Retro](https://github.com/HarbourMasters/retro),
[NeoRetro](https://github.com/HarbourMasters/neoretro) or
[BitDeck](https://github.com/Jepvid/BitDeck) easier to edit visually.

Those tools export CI textures as grayscale palette indices, and translucent
intensity textures without alpha. Neither looks the way it does in game, which
makes them hard to judge or paint over in an image editor. TlutPreview writes
a copy of an extracted texture folder with:

- **Apply TLUT**: CI4/CI8 textures recolored with their real palette instead
  of the synthetic grayscale one.
- **Apply IA Transparency**: I8 textures known to be drawn translucent
  (glow/spark/dust) given alpha = intensity.

The input is an extracted texture folder: PNGs plus the `manifest.json` that
all three tools write when extracting an otr/o2r. The source folder is never
modified.

The copy is only a base for editing: open its textures in your image editor to
get images that show the real colors and transparency. Make the texture pack
from the original extracted folder. The copy has no `manifest.json`, so it
can't be packed.

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
