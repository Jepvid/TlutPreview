// End-to-end walkthrough of colorizeTexturePack(): builds small extracted
// texture pack folders by hand (PNGs + manifest.json, the same shape BitDeck
// writes), runs the colorizer, and checks the copy it produces.

#include <QJsonDocument>
#include <QJsonObject>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

#include "colorize.h"
#include "core/image_codec.h"
#include "core/sha256.h"
#include "core/types/texture.h"

namespace {

using bitdeck::RgbaColor;
using bitdeck::RgbaImage;
using bitdeck::TextureType;
namespace fs = std::filesystem;

int g_failures = 0;

void check(bool condition, const char* description) {
    if (condition) {
        std::printf("[PASS] %s\n", description);
    } else {
        std::printf("[FAIL] %s\n", description);
        ++g_failures;
    }
}

std::vector<uint8_t> readFileBytes(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void writeFileBytes(const fs::path& path, const std::vector<uint8_t>& data) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

// A texture pack folder under construction: each added texture is written
// as "<name>.png" and recorded in manifest.json with its type and hash.
class PackBuilder {
public:
    explicit PackBuilder(fs::path dir) : dir_(std::move(dir)) {
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }

    void add(const std::string& name, const RgbaImage& image, TextureType type) {
        std::vector<uint8_t> png = bitdeck::encodePng(image);
        writeFileBytes(dir_ / (name + ".png"), png);
        QJsonObject entry;
        entry["hash"] = QString::fromStdString(bitdeck::sha256Hex(png));
        entry["textureType"] = static_cast<int>(type);
        entry["textureWidth"] = image.width;
        entry["textureHeight"] = image.height;
        manifest_[QString::fromStdString(name)] = entry;
    }

    void finish() {
        QByteArray json = QJsonDocument(manifest_).toJson();
        writeFileBytes(dir_ / "manifest.json", std::vector<uint8_t>(json.begin(), json.end()));
    }

    const fs::path& dir() const { return dir_; }

private:
    fs::path dir_;
    QJsonObject manifest_;
};

// A 2x1 CI8 texture as BitDeck extracts it: indices 0 and 1 over the
// synthetic grayscale palette.
RgbaImage grayscaleCi8() {
    RgbaImage image = RgbaImage::makePaletteImage(2, 1);
    image.setIndex(0, 0, 0);
    image.setIndex(1, 0, 1);
    image.setPaletteEntry(0, RgbaColor{0, 0, 0, 255});
    image.setPaletteEntry(1, RgbaColor{1, 1, 1, 255});
    return image;
}

// A 2-color TLUT: red, then green.
RgbaImage redGreenTlut() {
    RgbaImage tlut = RgbaImage::makeChannelImage(2, 1, 4);
    tlut.setRgba(0, 0, 255, 0, 0, 255);
    tlut.setRgba(1, 0, 0, 255, 0, 255);
    return tlut;
}

// A 2x1 I8 texture as BitDeck extracts it: opaque gray (dark, bright).
RgbaImage opaqueI8() {
    RgbaImage image = RgbaImage::makeChannelImage(2, 1, 3);
    image.setRgba(0, 0, 40, 40, 40, 255);
    image.setRgba(1, 0, 200, 200, 200, 255);
    return image;
}

RgbaImage readPng(const fs::path& path) {
    return bitdeck::decodePng(readFileBytes(path));
}

bool pixelIs(const RgbaImage& image, int x, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return image.r(x, 0) == r && image.g(x, 0) == g && image.b(x, 0) == b && image.a(x, 0) == a;
}

} // namespace

int main() {
    using tlutpreview::ColorizeOptions;
    using tlutpreview::ColorizeResult;
    using tlutpreview::colorizeTexturePack;

    fs::path root = fs::temp_directory_path() / "tlutpreview_tests";
    fs::remove_all(root);

    // -- An OOT pack: "objects/object_anubice/" marks it as OOT, which turns
    //    on OOT's baked CI-texture -> TLUT table and I8 translucency table.
    PackBuilder oot(root / "oot_pack");
    oot.add("objects/object_anubice/gAnubiceMarkerTex", opaqueI8(), TextureType::RGBA32bpp);
    oot.add("objects/object_ane/gCuccoLadyHairTex", grayscaleCi8(), TextureType::Palette8bpp); // baked -> gCuccoLadyTLUT
    oot.add("objects/object_ane/gCuccoLadyTLUT", redGreenTlut(), TextureType::RGBA16bpp);
    oot.add("objects/object_ane/gUnpairedTex_1234", grayscaleCi8(), TextureType::Palette8bpp); // no TLUT anywhere
    oot.add("objects/gameplay_keep/gDust1Tex", opaqueI8(), TextureType::Grayscale8bpp); // baked translucent
    oot.add("objects/gameplay_keep/gSolidWallTex", opaqueI8(), TextureType::Grayscale8bpp); // not listed
    oot.finish();
    std::vector<uint8_t> sourceHairBytes = readFileBytes(oot.dir() / "objects/object_ane/gCuccoLadyHairTex.png");

    fs::path ootOut = root / "oot_pack (TLUT)";
    ColorizeResult result = colorizeTexturePack(oot.dir(), ootOut, ColorizeOptions{});

    check(result.filesCopied == 6, "copy: every PNG lands in the output folder");
    check(!fs::exists(ootOut / "manifest.json"), "copy: the output folder has no manifest.json, so it can't be packed");
    check(result.tlutApplied == 1 && result.tlutUnmatched == 1,
          "TLUT: the baked-table CI texture is recolored, the unpaired one is counted as unmatched");
    check(result.iaApplied == 1, "IA: only the I8 texture in OOT's translucency table is changed");

    RgbaImage hair = readPng(ootOut / "objects/object_ane/gCuccoLadyHairTex.png");
    check(hair.withPalette, "TLUT: the recolored CI texture is still an indexed PNG");
    check(pixelIs(hair, 0, 255, 0, 0, 255) && pixelIs(hair, 1, 0, 255, 0, 255),
          "TLUT: index 0 reads red and index 1 reads green, from gCuccoLadyTLUT");
    check(pixelIs(readPng(ootOut / "objects/object_ane/gUnpairedTex_1234.png"), 1, 1, 1, 1, 255),
          "TLUT: the unpaired CI texture keeps its grayscale palette");

    RgbaImage dust = readPng(ootOut / "objects/gameplay_keep/gDust1Tex.png");
    check(pixelIs(dust, 0, 40, 40, 40, 40) && pixelIs(dust, 1, 200, 200, 200, 200),
          "IA: gDust1Tex gets alpha = intensity (dark is see-through)");
    check(readPng(ootOut / "objects/gameplay_keep/gSolidWallTex.png").a(0, 0) == 255,
          "IA: an I8 texture not in the table stays opaque");

    check(readFileBytes(oot.dir() / "objects/object_ane/gCuccoLadyHairTex.png") == sourceHairBytes,
          "source: the original pack folder is left untouched");

    // -- Options off: only the matching step runs.
    ColorizeOptions iaOnly;
    iaOnly.applyTlut = false;
    ColorizeResult iaOnlyResult = colorizeTexturePack(oot.dir(), root / "oot_ia_only", iaOnly);
    check(iaOnlyResult.tlutApplied == 0 && iaOnlyResult.iaApplied == 1,
          "options: with Apply TLUT off, CI textures are copied as-is and IA still applies");

    // -- A pack with no game marker: TLUTs still pair by name with a
    //    "<name>_TLUT" sibling in the same folder (BK64 naming).
    PackBuilder named(root / "named_pack");
    named.add("assets/model/crate_tex_0", grayscaleCi8(), TextureType::Palette4bpp);
    named.add("assets/model/crate_tex_0_TLUT", redGreenTlut(), TextureType::RGBA16bpp);
    named.finish();
    ColorizeResult namedResult = colorizeTexturePack(named.dir(), root / "named_out", ColorizeOptions{});
    check(namedResult.tlutApplied == 1, "name match: crate_tex_0 pairs with crate_tex_0_TLUT");
    check(pixelIs(readPng(root / "named_out/assets/model/crate_tex_0.png"), 1, 0, 255, 0, 255),
          "name match: index 1 reads green");

    // -- Refusals: the output must be a new folder outside the pack.
    auto throws = [&](const fs::path& target) {
        try {
            colorizeTexturePack(named.dir(), target, ColorizeOptions{});
        } catch (const std::runtime_error&) {
            return true;
        }
        return false;
    };
    check(throws(named.dir()), "refuses: output folder is the pack folder itself");
    check(throws(named.dir() / "sub"), "refuses: output folder inside the pack folder");
    check(throws(root / "named_out"), "refuses: output folder that already has files in it");

    fs::remove_all(root);
    std::printf("%s\n", g_failures == 0 ? "All tests passed." : "Some tests FAILED.");
    return g_failures == 0 ? 0 : 1;
}
