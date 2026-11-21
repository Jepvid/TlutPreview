#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bitdeck {

// Looks up archivePath in PM64's baked CI-texture -> TLUT archive-path table
// (see tools/gen_pm64_tlut_map.py). Returns nullopt for any path not in the
// table, including every non-PM64 archive.
std::optional<std::string> pm64BakedTlutArchivePathFor(const std::string& archivePath);

// Every TLUT archive path that may pair with Paper Mario's (Paperboat) CI
// texture at archivePath, in lookup order, from Torch's PM64 naming
// conventions ("<name>.pal", "<name>_tlut", "<name>_pal0", ...). The caller
// takes the first candidate present in the archive.
std::vector<std::string> pm64TlutCandidatesFor(const std::string& archivePath);

// True if archivePath is named like one of Torch's PM64 palettes
// ("<name>.pal", "<name>_pal0", "<sprite>_pal_<N>", "<charset>_palette_<N>",
// "<icon>/pal", "<icon>/pal<N>", "<name>_pal").
bool pm64IsPaletteName(const std::string& archivePath);

// Each "<sprite>_raster_<i>" texture's archive path paired with its default
// "<sprite>_pal_<n>" palette, read from the raster table of the PM64 sprite
// Blob resource at spriteArchivePath ("sprites/npc_sprite_<N>" or
// "sprites/player_sprite_<N>"). Empty for any other path or malformed data.
std::vector<std::pair<std::string, std::string>> pm64SpriteRasterPalettes(const std::string& spriteArchivePath,
                                                                         const std::vector<uint8_t>& resourceData);

} // namespace bitdeck
