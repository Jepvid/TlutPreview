#include "pm64_tlut_map.h"

#include <algorithm>
#include <cctype>

#include "../core/resource.h"

namespace bitdeck {

namespace {

bool endsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool isAllDigits(const std::string& text) {
    return !text.empty() &&
           std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); });
}

// text with a trailing "<marker><digits>" removed, or empty if it has none.
std::string withoutNumberedSuffix(const std::string& text, const std::string& marker) {
    size_t pos = text.rfind(marker);
    if (pos == std::string::npos || !isAllDigits(text.substr(pos + marker.size()))) {
        return {};
    }
    return text.substr(0, pos);
}

bool isPm64SpritePath(const std::string& path) {
    for (const std::string prefix : {"sprites/npc_sprite_", "sprites/player_sprite_"}) {
        if (path.rfind(prefix, 0) == 0) {
            return isAllDigits(path.substr(prefix.size()));
        }
    }
    return false;
}

// CI texture -> TLUT pairs set by game code, not by any archive display list
// (src/effects/fire_flower.c, src/entity/SuperBlock.c).
const std::pair<const char*, const char*> kPm64GameCodeTlutPairs[] = {
    {"effects/effect_gfx_fire_flower/tex_0", "effects/effect_gfx_fire_flower/tex_800"},
    {"effects/effect_gfx_fire_flower/tex_200", "effects/effect_gfx_fire_flower/tex_800"},
    {"effects/effect_gfx_fire_flower/tex_400", "effects/effect_gfx_fire_flower/tex_800"},
    {"entities/SuperBlockContent/tex_0", "entities/SuperBlockContent/tlut_200"},
};

uint32_t readU32Le(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint32_t>(data[offset]) | (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) | (static_cast<uint32_t>(data[offset + 3]) << 24);
}

} // namespace

std::vector<std::string> pm64TlutCandidatesFor(const std::string& archivePath) {
    size_t slash = archivePath.rfind('/');
    std::string folder = slash == std::string::npos ? std::string() : archivePath.substr(0, slash);
    std::string baseName = slash == std::string::npos ? archivePath : archivePath.substr(slash + 1);

    // Icons/HUD "<name>.pal", map/effect "<name>_tlut", background "<name>_pal0".
    std::vector<std::string> candidates = {archivePath + ".pal", archivePath + "_tlut", archivePath + "_pal0"};

    // Map texture "<name>_aux" (shared palette) and "<name>_mm<N>" mip
    // levels, and story page "<name>_img": "<name>_tlut".
    for (const std::string suffix : {"_aux", "_img"}) {
        if (endsWith(archivePath, suffix)) {
            candidates.push_back(archivePath.substr(0, archivePath.size() - suffix.size()) + "_tlut");
        }
    }
    if (std::string stem = withoutNumberedSuffix(archivePath, "_mm"); !stem.empty()) {
        candidates.push_back(stem + "_tlut");
    }

    // Icon "<folder>/img": "<folder>/pal", or the first of "<folder>/pal<N>".
    if (baseName == "img") {
        candidates.push_back(folder + "/pal");
        candidates.push_back(folder + "/pal1");
    }

    // Charset glyph "charset_<name>_<N>": palette 0 of "charset_<name>_palette".
    // Standard glyphs: dialogue palette MSG_PAL_STANDARD (10, thin dark text).
    // Title glyphs: the credits' title palette 1 of "charset_subtitle_palette".
    if (folder == "charset") {
        if (baseName.rfind("charset_standard_", 0) == 0) {
            candidates.push_back("charset/charset_standard_palette_10");
        } else if (baseName.rfind("charset_title_", 0) == 0) {
            candidates.push_back("charset/charset_subtitle_palette_1");
        } else if (std::string stem = withoutNumberedSuffix(archivePath, "_"); !stem.empty()) {
            candidates.push_back(stem + "_palette_0");
        }
    }

    // Message box pieces drawn with another piece's palette.
    if (folder == "ui/msg") {
        if (baseName == "bubble_mid" || baseName == "bubble_right" || baseName == "arrow" ||
            baseName.rfind("frame_", 0) == 0) {
            candidates.push_back("ui/msg/bubble_left.pal");
        } else if (baseName.rfind("sign_", 0) == 0) {
            candidates.push_back("ui/msg/sign_corner_topleft.pal");
        }
    }

    for (const auto& [ci, tlut] : kPm64GameCodeTlutPairs) {
        if (archivePath == ci) {
            candidates.push_back(tlut);
        }
    }

    return candidates;
}

bool pm64IsPaletteName(const std::string& archivePath) {
    size_t slash = archivePath.rfind('/');
    std::string baseName = slash == std::string::npos ? archivePath : archivePath.substr(slash + 1);
    if (endsWith(baseName, ".pal") || endsWith(baseName, "_pal") || endsWith(baseName, "_palette")) {
        return true;
    }
    if (baseName.rfind("pal", 0) == 0 && (baseName.size() == 3 || isAllDigits(baseName.substr(3)))) {
        return true;
    }
    return !withoutNumberedSuffix(baseName, "_pal").empty() || !withoutNumberedSuffix(baseName, "_pal_").empty() ||
           !withoutNumberedSuffix(baseName, "_palette_").empty();
}

std::vector<std::pair<std::string, std::string>> pm64SpriteRasterPalettes(const std::string& spriteArchivePath,
                                                                         const std::vector<uint8_t>& resourceData) {
    std::vector<std::pair<std::string, std::string>> pairs;
    const size_t blobStart = Resource::kHeaderSize + 4; // header + u32 blob size
    if (!isPm64SpritePath(spriteArchivePath) || resourceData.size() < blobStart + 4) {
        return pairs;
    }

    // Blob payload as its own buffer: SpriteAnimData (u32 rastersOffset, ...)
    // with every offset relative to the payload's start.
    size_t blobSize = std::min<size_t>(readU32Le(resourceData, Resource::kHeaderSize), resourceData.size() - blobStart);
    std::vector<uint8_t> blob(resourceData.begin() + blobStart, resourceData.begin() + blobStart + blobSize);
    if (blob.size() < 4) {
        return pairs;
    }

    // Raster list: u32 entry offsets, 0xFFFFFFFF-terminated. Each entry is
    // {u32 imageOffset, u8 width, u8 height, s8 palette, s8 quadCacheIndex}.
    uint32_t rastersOffset = readU32Le(blob, 0);
    for (size_t i = 0;; ++i) {
        size_t listPos = static_cast<size_t>(rastersOffset) + i * 4;
        if (listPos + 4 > blob.size()) {
            break;
        }
        uint32_t entry = readU32Le(blob, listPos);
        if (entry == 0xFFFFFFFF || static_cast<size_t>(entry) + 8 > blob.size()) {
            break;
        }
        int8_t palette = static_cast<int8_t>(blob[entry + 6]);
        if (palette >= 0) {
            pairs.emplace_back(spriteArchivePath + "_raster_" + std::to_string(i),
                               spriteArchivePath + "_pal_" + std::to_string(palette));
        }
    }
    return pairs;
}

} // namespace bitdeck
