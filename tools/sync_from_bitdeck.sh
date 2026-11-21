#!/usr/bin/env bash
# Copies the BitDeck sources TlutPreview builds on into third_party/bitdeck/,
# unmodified, and records the BitDeck commit they came from.
#
#   tools/sync_from_bitdeck.sh [path/to/BitDeck]   (default: ../BitDeck)
set -euo pipefail

here="$(cd "$(dirname "$0")/.." && pwd)"
bitdeck="$(cd "${1:-$here/../BitDeck}" && pwd)"
dest="$here/third_party/bitdeck"

files=(
    core/crc64.h core/crc64.cpp
    core/endianness.h
    core/image_codec.h core/image_codec.cpp
    core/resource.h core/resource.cpp
    core/resource_type.h
    core/sha256.h
    core/version.h
    core/types/texture.h core/types/texture.cpp
    games/mk64_tlut_map.h games/mk64_tlut_map.cpp
    games/mm_tlut_map.h games/mm_tlut_map.cpp
    games/oot_tlut_map.h games/oot_tlut_map.cpp
    games/sf64_tlut_map.h games/sf64_tlut_map.cpp
    games/pm64_tlut_map.h games/pm64_tlut_map.cpp games/pm64_baked_tlut_map.cpp
    games/bk64_i8_transparency_map.h games/bk64_i8_transparency_map.cpp
    games/mm_i8_transparency_map.h games/mm_i8_transparency_map.cpp
    games/oot_i8_transparency_map.h games/oot_i8_transparency_map.cpp
    games/pm64_i8_transparency_map.h games/pm64_i8_transparency_map.cpp
)

for file in "${files[@]}"; do
    mkdir -p "$dest/$(dirname "$file")"
    cp "$bitdeck/src/$file" "$dest/$file"
done

commit="$(git -C "$bitdeck" rev-parse --short HEAD)"
dirty=""
if ! git -C "$bitdeck" diff --quiet HEAD -- "${files[@]/#/src/}"; then
    dirty=" (plus uncommitted changes)"
fi
echo "Copied from BitDeck $commit$dirty" > "$dest/SOURCE"
echo "Synced ${#files[@]} files from BitDeck $commit$dirty"
