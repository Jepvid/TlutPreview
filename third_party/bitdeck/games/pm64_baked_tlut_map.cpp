#include "pm64_tlut_map.h"

#include <iterator>
#include <unordered_map>

namespace bitdeck {

namespace {

// Ground-truth CI-texture -> TLUT archive path pairs, derived from
// Paperboat's assets/yaml/us/**/*.yml ("tlut:" numeric offset fields).
const std::pair<const char*, const char*> kPm64TlutPairs[] = {
    {"effects/effect_gfx_balloon/tex_200", "effects/effect_gfx_balloon/tex_400"},
    {"effects/effect_gfx_balloon/tex_800", "effects/effect_gfx_balloon/tex_A00"},
    {"effects/effect_gfx_balloon/tex_E00", "effects/effect_gfx_balloon/tex_1000"},
    {"effects/effect_gfx_big_smoke_puff/tex_0", "effects/effect_gfx_big_smoke_puff/tex_200"},
    {"effects/effect_gfx_big_smoke_puff/tex_220", "effects/effect_gfx_big_smoke_puff/tex_420"},
    {"effects/effect_gfx_big_smoke_puff/tex_440", "effects/effect_gfx_big_smoke_puff/tex_640"},
    {"effects/effect_gfx_big_smoke_puff/tex_660", "effects/effect_gfx_big_smoke_puff/tex_860"},
    {"effects/effect_gfx_big_smoke_puff/tex_880", "effects/effect_gfx_big_smoke_puff/tex_A80"},
    {"effects/effect_gfx_big_smoke_puff/tex_AA0", "effects/effect_gfx_big_smoke_puff/tex_CA0"},
    {"effects/effect_gfx_big_smoke_puff/tex_CC0", "effects/effect_gfx_big_smoke_puff/tex_EC0"},
    {"effects/effect_gfx_butterflies/tex_0", "effects/effect_gfx_butterflies/tex_400"},
    {"effects/effect_gfx_butterflies/tex_1070", "effects/effect_gfx_butterflies/tex_1470"},
    {"effects/effect_gfx_butterflies/tex_1480", "effects/effect_gfx_butterflies/tex_1880"},
    {"effects/effect_gfx_butterflies/tex_18A0", "effects/effect_gfx_butterflies/tex_1CA0"},
    {"effects/effect_gfx_butterflies/tex_1CC0", "effects/effect_gfx_butterflies/tex_20C0"},
    {"effects/effect_gfx_butterflies/tex_20E0", "effects/effect_gfx_butterflies/tex_24E0"},
    {"effects/effect_gfx_butterflies/tex_2500", "effects/effect_gfx_butterflies/tex_2900"},
    {"effects/effect_gfx_butterflies/tex_2920", "effects/effect_gfx_butterflies/tex_2D20"},
    {"effects/effect_gfx_butterflies/tex_420", "effects/effect_gfx_butterflies/tex_820"},
    {"effects/effect_gfx_butterflies/tex_840", "effects/effect_gfx_butterflies/tex_C40"},
    {"effects/effect_gfx_butterflies/tex_C50", "effects/effect_gfx_butterflies/tex_1050"},
    {"effects/effect_gfx_cloud_puff_trail/tex_0", "effects/effect_gfx_cloud_puff_trail/tex_80"},
    {"effects/effect_gfx_ending_decals/tex_1800", "effects/effect_gfx_ending_decals/tex_1C00"},
    {"effects/effect_gfx_fire_flower/tex_600", "effects/effect_gfx_fire_flower/tex_800"},
    {"effects/effect_gfx_fire_flower/tex_A00", "effects/effect_gfx_fire_flower/tex_A80"},
    {"effects/effect_gfx_floating_flower/tex_0", "effects/effect_gfx_floating_flower/tex_80"},
    {"effects/effect_gfx_flower_splash_trail/tex_0", "effects/effect_gfx_flower_splash_trail/tex_A0"},
    {"effects/effect_gfx_fright_jar/tex_0", "effects/effect_gfx_fright_jar/tex_2000"},
    {"effects/effect_gfx_fright_jar/tex_2020", "effects/effect_gfx_fright_jar/tex_4020"},
    {"effects/effect_gfx_landing_dust/tex_2E10", "effects/effect_gfx_landing_dust/tex_2E90"},
    {"effects/effect_gfx_lil_oink/tex_17B0", "effects/effect_gfx_lil_oink/tex_17F0"},
    {"effects/effect_gfx_lil_oink/tex_19F0", "effects/effect_gfx_lil_oink/tex_1A30"},
    {"effects/effect_gfx_lil_oink/tex_1C30", "effects/effect_gfx_lil_oink/tex_1C70"},
    {"effects/effect_gfx_lil_oink/tex_1E70", "effects/effect_gfx_lil_oink/tex_1EB0"},
    {"effects/effect_gfx_lil_oink/tex_20B0", "effects/effect_gfx_lil_oink/tex_20F0"},
    {"effects/effect_gfx_lil_oink/tex_22F0", "effects/effect_gfx_lil_oink/tex_2330"},
    {"effects/effect_gfx_lil_oink/tex_2530", "effects/effect_gfx_lil_oink/tex_2570"},
    {"effects/effect_gfx_lil_oink/tex_2770", "effects/effect_gfx_lil_oink/tex_2870"},
    {"effects/effect_gfx_lil_oink/tex_2A70", "effects/effect_gfx_lil_oink/tex_2B70"},
    {"effects/effect_gfx_partner_buff/tex_0", "effects/effect_gfx_partner_buff/tex_200"},
    {"effects/effect_gfx_partner_buff/tex_1400", "effects/effect_gfx_partner_buff/tex_1600"},
    {"effects/effect_gfx_partner_buff/tex_400", "effects/effect_gfx_partner_buff/tex_600"},
    {"effects/effect_gfx_partner_buff/tex_800", "effects/effect_gfx_partner_buff/tex_A00"},
    {"effects/effect_gfx_peach_star_beam/tex_13E8", "effects/effect_gfx_peach_star_beam/tex_1BE8"},
    {"effects/effect_gfx_peach_star_beam/tex_1DE8", "effects/effect_gfx_peach_star_beam/tex_25E8"},
    {"effects/effect_gfx_peach_star_beam/tex_27E8", "effects/effect_gfx_peach_star_beam/tex_2FE8"},
    {"effects/effect_gfx_peach_star_beam/tex_31E8", "effects/effect_gfx_peach_star_beam/tex_39E8"},
    {"effects/effect_gfx_peach_star_beam/tex_3BE8", "effects/effect_gfx_peach_star_beam/tex_43E8"},
    {"effects/effect_gfx_peach_star_beam/tex_45E8", "effects/effect_gfx_peach_star_beam/tex_4DE8"},
    {"effects/effect_gfx_peach_star_beam/tex_5E8", "effects/effect_gfx_peach_star_beam/tex_7E8"},
    {"effects/effect_gfx_peach_star_beam/tex_9E8", "effects/effect_gfx_peach_star_beam/tex_11E8"},
    {"effects/effect_gfx_quizmo_assistant/tex_0", "effects/effect_gfx_quizmo_assistant/tex_800"},
    {"effects/effect_gfx_quizmo_assistant/tex_1000", "effects/effect_gfx_quizmo_assistant/tex_1800"},
    {"effects/effect_gfx_quizmo_assistant/tex_1A00", "effects/effect_gfx_quizmo_assistant/tex_2200"},
    {"effects/effect_gfx_quizmo_assistant/tex_A00", "effects/effect_gfx_quizmo_assistant/tex_E00"},
    {"effects/effect_gfx_quizmo_stage/tex_12F8", "effects/effect_gfx_quizmo_stage/tex_14F8"},
    {"effects/effect_gfx_quizmo_stage/tex_1828", "effects/effect_gfx_quizmo_stage/tex_1C28"},
    {"effects/effect_gfx_quizmo_stage/tex_2D58", "effects/effect_gfx_quizmo_stage/tex_3558"},
    {"effects/effect_gfx_quizmo_stage/tex_3E88", "effects/effect_gfx_quizmo_stage/tex_4088"},
    {"effects/effect_gfx_quizmo_stage/tex_4248", "effects/effect_gfx_quizmo_stage/tex_4A48"},
    {"effects/effect_gfx_snowman_doll/tex_878", "effects/effect_gfx_snowman_doll/tex_A78"},
    {"effects/effect_gfx_spirit_card/tex_0", "effects/effect_gfx_spirit_card/tex_200"},
    {"effects/effect_gfx_spirit_card/tex_1080", "effects/effect_gfx_spirit_card/tex_1500"},
    {"effects/effect_gfx_spirit_card/tex_1700", "effects/effect_gfx_spirit_card/tex_1B80"},
    {"effects/effect_gfx_spirit_card/tex_1D80", "effects/effect_gfx_spirit_card/tex_2200"},
    {"effects/effect_gfx_spirit_card/tex_2400", "effects/effect_gfx_spirit_card/tex_2880"},
    {"effects/effect_gfx_spirit_card/tex_2A80", "effects/effect_gfx_spirit_card/tex_2F00"},
    {"effects/effect_gfx_spirit_card/tex_3100", "effects/effect_gfx_spirit_card/tex_3580"},
    {"effects/effect_gfx_spirit_card/tex_3780", "effects/effect_gfx_spirit_card/tex_3C00"},
    {"effects/effect_gfx_spirit_card/tex_400", "effects/effect_gfx_spirit_card/tex_800"},
    {"effects/effect_gfx_spirit_card/tex_A00", "effects/effect_gfx_spirit_card/tex_C00"},
    {"effects/effect_gfx_spirit_card/tex_E00", "effects/effect_gfx_spirit_card/tex_E80"},
    {"effects/effect_gfx_stat_change/tex_2000", "effects/effect_gfx_stat_change/tex_2400"},
    {"effects/effect_gfx_static_status/tex_0", "effects/effect_gfx_static_status/tex_80"},
    {"effects/effect_gfx_throw_spiny/tex_0", "effects/effect_gfx_throw_spiny/tex_200"},
    {"effects/effect_gfx_throw_spiny/tex_400", "effects/effect_gfx_throw_spiny/tex_600"},
    {"effects/effect_gfx_tubba_heart_attack/tex_0", "effects/effect_gfx_tubba_heart_attack/tex_200"},
    {"theater/theater_floor", "theater/theater_floor_pal"},
};

} // namespace

std::optional<std::string> pm64BakedTlutArchivePathFor(const std::string& archivePath) {
    static const std::unordered_map<std::string, std::string> kMap = [] {
        std::unordered_map<std::string, std::string> map;
        map.reserve(std::size(kPm64TlutPairs));
        for (const auto& [ci, tlut] : kPm64TlutPairs) {
            map.emplace(ci, tlut);
        }
        return map;
    }();
    auto it = kMap.find(archivePath);
    if (it == kMap.end()) {
        return std::nullopt;
    }
    return it->second;
}

} // namespace bitdeck
