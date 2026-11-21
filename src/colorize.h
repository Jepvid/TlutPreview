#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace tlutpreview {

struct ColorizeOptions {
    // Recolors CI4/CI8 textures with the TLUT paired with them.
    bool applyTlut = true;
    // Gives I8 textures known to be drawn translucent alpha = intensity.
    bool applyIaTransparency = true;
};

struct ColorizeResult {
    int filesCopied = 0;
    int tlutApplied = 0;   // CI textures recolored
    int tlutUnmatched = 0; // CI textures with no TLUT found (left grayscale)
    int iaApplied = 0;     // I8 textures given alpha = intensity
};

// done/total over the manifest entries processed.
using ColorizeProgress = std::function<void(int done, int total)>;

// Copies the extracted texture pack at sourceDir (a folder holding
// manifest.json, as written by BitDeck's "Extract textures from otr/o2r")
// to targetDir, then rewrites the copy's CI4/CI8 PNGs with their TLUT
// applied and its translucent I8 PNGs with alpha = intensity, per options.
// manifest.json in targetDir carries the rewritten files' new hashes;
// sourceDir is left untouched.
//
// TLUT pairing comes from the per-game baked CI-texture -> TLUT tables
// (MK64, MM, OOT, PM64, SF64), PM64's naming conventions, and name
// correspondence with a "*TLUT"/"_tlut_"/"_palette" sibling. I8
// translucency comes from the per-game baked tables (BK64, MM, OOT, PM64).
//
// Throws std::runtime_error if sourceDir has no manifest.json, if targetDir
// is sourceDir or inside it, or if targetDir exists and is not empty.
ColorizeResult colorizeTexturePack(const std::filesystem::path& sourceDir, const std::filesystem::path& targetDir,
                                   const ColorizeOptions& options, const ColorizeProgress& progress = {});

} // namespace tlutpreview
