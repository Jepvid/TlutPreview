#include "colorize.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/image_codec.h"
#include "core/sha256.h"
#include "core/types/texture.h"
#include "games/bk64_i8_transparency_map.h"
#include "games/mk64_tlut_map.h"
#include "games/mm_i8_transparency_map.h"
#include "games/mm_tlut_map.h"
#include "games/oot_i8_transparency_map.h"
#include "games/oot_tlut_map.h"
#include "games/pm64_i8_transparency_map.h"
#include "games/pm64_tlut_map.h"
#include "games/sf64_tlut_map.h"

namespace tlutpreview {

using bitdeck::RgbaImage;
using bitdeck::TextureType;

namespace {

std::vector<uint8_t> readFileBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot read " + path.string());
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void writeFileBytes(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!file) {
        throw std::runtime_error("Cannot write " + path.string());
    }
}

bool endsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool startsWith(const std::string& text, const std::string& prefix) {
    return text.rfind(prefix, 0) == 0;
}

std::string folderOf(const std::string& name) {
    return std::filesystem::path(name).parent_path().string();
}

std::string baseNameOf(const std::string& name) {
    return std::filesystem::path(name).filename().string();
}

// Matches an uppercase "...TLUT" suffix (OOT/BK64), or a lowercase "tlut"
// segment/suffix or "_palette" suffix (MK64). Case-sensitive: OOT's
// auto-generated "..._TLUT_<hexaddr>" names would also match if lowercased.
bool looksLikeTlutName(const std::string& name) {
    if (endsWith(name, "TLUT")) {
        return true;
    }
    return startsWith(name, "tlut_") || endsWith(name, "_tlut") || name.find("_tlut_") != std::string::npos ||
           endsWith(name, "_palette");
}

// Every texture name in the pack's manifest.json, the "*TLUT"-named ones
// grouped by folder, and which game the pack is from.
struct PackIndex {
    std::unordered_set<std::string> names;
    std::unordered_map<std::string, std::unordered_set<std::string>> folderToTlutNames;

    bool looksLikeBk64 = false;
    bool looksLikeMk64 = false;
    bool looksLikeMm = false;
    bool looksLikeOot = false;
    bool looksLikePm64 = false;
    bool looksLikeSf64 = false;

    bool has(const std::string& name) const { return names.count(name) != 0; }
};

// Sets the game flag for the first game-exclusive marker path found in
// names ("models/" MK64, "parameter_static/" MM, "objects/object_anubice/"
// OOT, "sprite_shading/" PM64, "ast_option/" SF64, "assets/sprite/" BK64).
// With no TLUT-game marker found, picks the TLUT game whose baked table
// pairs the most of this pack's CI textures with a TLUT also in the pack.
void detectGame(PackIndex& index, const std::vector<std::string>& ciNames) {
    for (const auto& name : index.names) {
        if (startsWith(name, "models/")) {
            index.looksLikeMk64 = true;
        } else if (startsWith(name, "parameter_static/")) {
            index.looksLikeMm = true;
        } else if (startsWith(name, "objects/object_anubice/")) {
            index.looksLikeOot = true;
        } else if (startsWith(name, "sprite_shading/")) {
            index.looksLikePm64 = true;
        } else if (startsWith(name, "ast_option/")) {
            index.looksLikeSf64 = true;
        } else if (startsWith(name, "assets/sprite/")) {
            index.looksLikeBk64 = true;
        }
    }
    if (index.looksLikeMk64 || index.looksLikeMm || index.looksLikeOot || index.looksLikePm64 ||
        index.looksLikeSf64) {
        return;
    }

    using BakedLookup = std::optional<std::string> (*)(const std::string&);
    const std::pair<bool PackIndex::*, BakedLookup> games[] = {
        {&PackIndex::looksLikeMk64, &bitdeck::mk64TlutArchivePathFor},
        {&PackIndex::looksLikeMm, &bitdeck::mmTlutArchivePathFor},
        {&PackIndex::looksLikeOot, &bitdeck::ootTlutArchivePathFor},
        {&PackIndex::looksLikePm64, &bitdeck::pm64BakedTlutArchivePathFor},
        {&PackIndex::looksLikeSf64, &bitdeck::sf64TlutArchivePathFor},
    };
    bool PackIndex::*best = nullptr;
    int bestHits = 0;
    for (const auto& [flag, lookup] : games) {
        int hits = 0;
        for (const auto& name : ciNames) {
            if (auto tlut = lookup(name); tlut.has_value() && index.has(*tlut)) {
                ++hits;
            }
        }
        if (hits > bestHits) {
            best = flag;
            bestHits = hits;
        }
    }
    if (best != nullptr) {
        index.*best = true;
    }
}

// A name with a known suffix (e.g. "Tex"/"TLUT") stripped and split into its
// non-digit "core" and digit characters, so "Shop2BgTex" and "ShopBg2TLUT"
// both normalize to core="ShopBg", digits="2" and compare equal.
struct NormalizedTextureName {
    std::string core;
    std::string digits;
};

NormalizedTextureName normalizeTextureName(std::string name, const std::string& suffix) {
    if (endsWith(name, suffix)) {
        name.resize(name.size() - suffix.size());
    }
    NormalizedTextureName result;
    for (char c : name) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            result.digits += c;
        } else {
            result.core += c;
        }
    }
    return result;
}

std::optional<std::string> withoutTrailingSegment(const std::string& s) {
    size_t pos = s.rfind('_');
    if (pos != std::string::npos) {
        return s.substr(0, pos);
    }
    return std::nullopt;
}

bool isAllDigits(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
}

// A CI texture's TLUT by name correspondence in its own folder: BK64's
// "<name>_TLUT", MK64's "_texture_" -> "_tlut_" swap and kart
// "_frame<N>_wheel<M>" / "_palette" pairs, then -- for a name ending in
// "Tex" -- OOT's core+digit match against "<Base>TLUT" in the same folder
// or the sibling "..._pal_static" folder (an exact core+digit match, or the
// sole candidate when nothing matches by digit). Returns nullopt on an
// absent or ambiguous match.
std::optional<std::string> findNamedTlutMatch(const std::string& fileName, const PackIndex& index) {
    std::string baseName = baseNameOf(fileName);
    std::string folder = folderOf(fileName);

    auto exactIt = index.folderToTlutNames.find(folder);
    if (exactIt != index.folderToTlutNames.end()) {
        std::vector<std::string> candidates;

        candidates.push_back(baseName + "_TLUT");
        if (auto stripped = withoutTrailingSegment(baseName)) {
            candidates.push_back(*stripped + "_TLUT");
        }

        size_t texturePos = baseName.find("_texture_");
        if (texturePos != std::string::npos) {
            std::string swapped = baseName.substr(0, texturePos) + "_tlut_" + baseName.substr(texturePos + 9);
            candidates.push_back(swapped);
            if (auto stripped = withoutTrailingSegment(swapped)) {
                candidates.push_back(*stripped);
            }
        }

        size_t framePos = baseName.find("_frame");
        if (framePos != std::string::npos) {
            size_t digitsStart = framePos + 6; // length of "_frame"
            size_t digitsEnd = digitsStart;
            while (digitsEnd < baseName.size() && std::isdigit(static_cast<unsigned char>(baseName[digitsEnd]))) {
                ++digitsEnd;
            }
            const std::string wheelMarker = "_wheel";
            std::string afterFrameDigits = baseName.substr(digitsEnd);
            if (digitsEnd > digitsStart && startsWith(afterFrameDigits, wheelMarker)) {
                std::string wheelDigits = afterFrameDigits.substr(wheelMarker.size());
                if (isAllDigits(wheelDigits)) {
                    std::string frameDigits = baseName.substr(digitsStart, digitsEnd - digitsStart);
                    candidates.push_back(baseName.substr(0, framePos) + "_" + frameDigits + "_tlut_wheel_" +
                                         wheelDigits);
                }
            } else if (digitsEnd > digitsStart && digitsEnd == baseName.size()) {
                candidates.push_back(baseName.substr(0, framePos) + "_palette");
            }
        }

        for (const auto& candidate : candidates) {
            if (exactIt->second.count(candidate) != 0) {
                return folder + "/" + candidate;
            }
        }
    }

    if (!endsWith(baseName, "Tex")) {
        return std::nullopt;
    }
    NormalizedTextureName ciName = normalizeTextureName(baseName, "Tex");

    std::vector<std::string> candidateFolders = {folder};
    const std::string staticSuffix = "_static";
    if (endsWith(folder, staticSuffix) && !endsWith(folder, "_pal_static")) {
        candidateFolders.push_back(folder.substr(0, folder.size() - staticSuffix.size()) + "_pal_static");
    }

    std::vector<std::string> allCandidates;
    std::vector<std::string> exactMatches;
    for (const auto& candidateFolder : candidateFolders) {
        auto it = index.folderToTlutNames.find(candidateFolder);
        if (it == index.folderToTlutNames.end()) {
            continue;
        }
        for (const auto& tlutName : it->second) {
            std::string fullPath = candidateFolder + "/" + tlutName;
            allCandidates.push_back(fullPath);
            NormalizedTextureName tlutNorm = normalizeTextureName(tlutName, "TLUT");
            if (tlutNorm.core == ciName.core && tlutNorm.digits == ciName.digits) {
                exactMatches.push_back(fullPath);
            }
        }
    }

    if (exactMatches.size() == 1) {
        return exactMatches.front();
    }
    if (exactMatches.empty() && allCandidates.size() == 1) {
        return allCandidates.front();
    }
    return std::nullopt;
}

// The TLUT paired with fileName: a baked ground-truth match (MK64, MM, OOT,
// SF64, PM64), the first PM64 naming-convention candidate present in the
// pack, or a name-matched candidate. Returns nullopt if none apply.
std::optional<std::string> findTlutName(const std::string& fileName, const PackIndex& index) {
    if (index.looksLikeMk64) {
        if (auto baked = bitdeck::mk64TlutArchivePathFor(fileName)) {
            return baked;
        }
    }
    if (index.looksLikeMm) {
        if (auto baked = bitdeck::mmTlutArchivePathFor(fileName)) {
            return baked;
        }
    }
    if (index.looksLikeOot) {
        if (auto baked = bitdeck::ootTlutArchivePathFor(fileName)) {
            return baked;
        }
    }
    if (index.looksLikeSf64) {
        if (auto baked = bitdeck::sf64TlutArchivePathFor(fileName)) {
            return baked;
        }
    }
    if (index.looksLikePm64) {
        if (auto baked = bitdeck::pm64BakedTlutArchivePathFor(fileName)) {
            return baked;
        }
        for (const auto& candidate : bitdeck::pm64TlutCandidatesFor(fileName)) {
            if (index.has(candidate) || index.has("alt/" + candidate)) {
                return candidate;
            }
        }
    }
    return findNamedTlutMatch(fileName, index);
}

// The kart's shared "<prefix>_palette" character palette for a wheel-frame
// texture named "<prefix>_frame<N>_wheel<M>", or nullopt for any other name.
std::optional<std::string> findKartCharacterPaletteName(const std::string& fileName, const PackIndex& index) {
    std::string baseName = baseNameOf(fileName);
    std::string folder = folderOf(fileName);

    size_t framePos = baseName.find("_frame");
    size_t wheelPos = baseName.find("_wheel");
    if (framePos == std::string::npos || wheelPos == std::string::npos || wheelPos <= framePos) {
        return std::nullopt;
    }

    std::string candidate = baseName.substr(0, framePos) + "_palette";
    auto it = index.folderToTlutNames.find(folder);
    if (it != index.folderToTlutNames.end() && it->second.count(candidate) != 0) {
        return folder + "/" + candidate;
    }
    return std::nullopt;
}

// Matches MK64's "texture_red_shell_<N>" naming.
bool isMk64RedShellTexture(const std::string& fileName) {
    return startsWith(baseNameOf(fileName), "texture_red_shell_");
}

// Swaps each palette entry's red and green channels, replicating
// SpaghettiKart's init_red_shell_texture(): red shells use
// common_tlut_green_shell with red and green swapped.
RgbaImage swapRedGreenChannels(const RgbaImage& palette) {
    RgbaImage swapped = palette;
    for (int y = 0; y < swapped.height; ++y) {
        for (int x = 0; x < swapped.width; ++x) {
            swapped.setRgba(x, y, palette.g(x, y), palette.r(x, y), palette.b(x, y), palette.a(x, y));
        }
    }
    return swapped;
}

// True if fileName's I8 texture is listed as drawn translucent in the
// detected game's baked table.
bool i8TextureIsTranslucent(const std::string& fileName, const PackIndex& index) {
    if (index.looksLikeMm) {
        return bitdeck::mmI8TextureIsTranslucent(fileName);
    }
    if (index.looksLikeOot) {
        return bitdeck::ootI8TextureIsTranslucent(fileName);
    }
    if (index.looksLikeBk64) {
        return bitdeck::bk64I8TextureIsTranslucent(fileName);
    }
    if (index.looksLikePm64) {
        return bitdeck::pm64I8TextureIsTranslucent(fileName);
    }
    return false;
}

// Loads and caches a palette texture's PNG from the pack, preferring its
// "alt/" variant when the pack has one.
class PaletteLoader {
public:
    PaletteLoader(const std::filesystem::path& packDir, const PackIndex& index) : packDir_(packDir), index_(index) {}

    const RgbaImage* load(const std::string& name) {
        std::string resolved = index_.has("alt/" + name) ? "alt/" + name : name;
        auto cached = cache_.find(resolved);
        if (cached != cache_.end()) {
            return cached->second ? &*cached->second : nullptr;
        }
        std::optional<RgbaImage> palette;
        std::filesystem::path path = packDir_ / (resolved + ".png");
        if (index_.has(resolved) && std::filesystem::exists(path)) {
            try {
                palette = bitdeck::decodePng(readFileBytes(path));
            } catch (const std::exception&) {
                palette.reset();
            }
        }
        auto& slot = cache_[resolved] = std::move(palette);
        return slot ? &*slot : nullptr;
    }

private:
    std::filesystem::path packDir_;
    const PackIndex& index_;
    std::unordered_map<std::string, std::optional<RgbaImage>> cache_;
};

// Applies fileName's TLUT(s) onto image. Most textures get one palette at
// index 0; kart wheel frames get their character's kart palette at index 0
// and their own wheel palette right after it. Returns false if no TLUT was
// found or loaded.
bool applyTlut(RgbaImage& image, const std::string& fileName, const PackIndex& index, PaletteLoader& palettes) {
    std::optional<std::string> tlutName = findTlutName(fileName, index);
    if (!tlutName.has_value()) {
        return false;
    }

    if (auto characterName = findKartCharacterPaletteName(fileName, index)) {
        if (const RgbaImage* character = palettes.load(*characterName)) {
            bitdeck::applyTlutPalette(image, *character, 0);
            if (const RgbaImage* wheel = palettes.load(*tlutName)) {
                bitdeck::applyTlutPalette(image, *wheel, character->width * character->height);
            }
            return true;
        }
    }

    const RgbaImage* palette = palettes.load(*tlutName);
    if (palette == nullptr) {
        return false;
    }
    if (isMk64RedShellTexture(fileName)) {
        bitdeck::applyTlutPalette(image, swapRedGreenChannels(*palette), 0);
    } else {
        bitdeck::applyTlutPalette(image, *palette, 0);
    }
    return true;
}

// The same pixels as image, with alpha = intensity (red channel).
RgbaImage withAlphaFromIntensity(const RgbaImage& image) {
    RgbaImage result = RgbaImage::makeChannelImage(image.width, image.height, 4);
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            uint8_t intensity = image.r(x, y);
            result.setRgba(x, y, intensity, intensity, intensity, intensity);
        }
    }
    return result;
}

std::optional<TextureType> textureTypeOf(const QJsonObject& entry) {
    try {
        return bitdeck::textureTypeFromValue(entry.value(QStringLiteral("textureType")).toInt(-1));
    } catch (...) {
        return std::nullopt;
    }
}

bool isInsideOrEqual(const std::filesystem::path& path, const std::filesystem::path& base) {
    auto pathIt = path.begin();
    for (auto baseIt = base.begin(); baseIt != base.end(); ++baseIt, ++pathIt) {
        if (baseIt->empty()) {
            continue; // trailing separator
        }
        if (pathIt == path.end() || *pathIt != *baseIt) {
            return false;
        }
    }
    return true;
}

} // namespace

ColorizeResult colorizeTexturePack(const std::filesystem::path& sourceDir, const std::filesystem::path& targetDir,
                                   const ColorizeOptions& options, const ColorizeProgress& progress) {
    std::filesystem::path source = std::filesystem::weakly_canonical(sourceDir);
    std::filesystem::path target = std::filesystem::weakly_canonical(targetDir);

    std::filesystem::path manifestPath = source / "manifest.json";
    if (!std::filesystem::exists(manifestPath)) {
        throw std::runtime_error("No manifest.json in " + source.string());
    }
    if (isInsideOrEqual(target, source)) {
        throw std::runtime_error("The output folder can't be the texture pack folder or inside it.");
    }
    if (std::filesystem::exists(target) && !std::filesystem::is_empty(target)) {
        throw std::runtime_error("The output folder " + target.string() + " already exists and isn't empty.");
    }

    std::vector<uint8_t> manifestBytes = readFileBytes(manifestPath);
    QJsonDocument manifestDoc =
        QJsonDocument::fromJson(QByteArray(reinterpret_cast<const char*>(manifestBytes.data()),
                                           static_cast<qsizetype>(manifestBytes.size())));
    if (!manifestDoc.isObject()) {
        throw std::runtime_error("manifest.json is not a JSON object.");
    }
    QJsonObject manifest = manifestDoc.object();

    PackIndex index;
    std::vector<std::string> ciNames;
    std::vector<std::string> i8Names;
    for (auto it = manifest.begin(); it != manifest.end(); ++it) {
        std::string name = it.key().toStdString();
        index.names.insert(name);
        if (looksLikeTlutName(baseNameOf(name))) {
            index.folderToTlutNames[folderOf(name)].insert(baseNameOf(name));
        }
        std::optional<TextureType> type = textureTypeOf(it.value().toObject());
        if (type == TextureType::Palette4bpp || type == TextureType::Palette8bpp) {
            ciNames.push_back(name);
        } else if (type == TextureType::Grayscale8bpp) {
            i8Names.push_back(name);
        }
    }
    detectGame(index, ciNames);

    ColorizeResult result;
    std::filesystem::create_directories(target);
    std::filesystem::copy(source, target, std::filesystem::copy_options::recursive);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(target)) {
        if (entry.is_regular_file()) {
            ++result.filesCopied;
        }
    }

    PaletteLoader palettes(source, index);
    int total = static_cast<int>((options.applyTlut ? ciNames.size() : 0) +
                                 (options.applyIaTransparency ? i8Names.size() : 0));
    int done = 0;

    auto rewrite = [&](const std::string& name, const RgbaImage& image) {
        std::vector<uint8_t> png = bitdeck::encodePng(image);
        writeFileBytes(target / (name + ".png"), png);
        QJsonObject entry = manifest.value(QString::fromStdString(name)).toObject();
        entry[QStringLiteral("hash")] = QString::fromStdString(bitdeck::sha256Hex(png));
        manifest[QString::fromStdString(name)] = entry;
    };

    if (options.applyTlut) {
        for (const auto& name : ciNames) {
            std::filesystem::path pngPath = source / (name + ".png");
            bool applied = false;
            if (std::filesystem::exists(pngPath)) {
                RgbaImage image = bitdeck::decodePng(readFileBytes(pngPath));
                if (image.withPalette && applyTlut(image, name, index, palettes)) {
                    rewrite(name, image);
                    applied = true;
                }
            }
            ++(applied ? result.tlutApplied : result.tlutUnmatched);
            if (progress) {
                progress(++done, total);
            }
        }
    }

    if (options.applyIaTransparency) {
        for (const auto& name : i8Names) {
            std::filesystem::path pngPath = source / (name + ".png");
            if (i8TextureIsTranslucent(name, index) && std::filesystem::exists(pngPath)) {
                rewrite(name, withAlphaFromIntensity(bitdeck::decodePng(readFileBytes(pngPath))));
                ++result.iaApplied;
            }
            if (progress) {
                progress(++done, total);
            }
        }
    }

    QByteArray manifestJson = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
    writeFileBytes(target / "manifest.json", std::vector<uint8_t>(manifestJson.begin(), manifestJson.end()));

    return result;
}

} // namespace tlutpreview
