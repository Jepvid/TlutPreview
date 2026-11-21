#pragma once

#include <string>

namespace bitdeck {

// True if archivePath is a PM64 I8 texture confirmed safe for the
// 'Preview I8 transparency from brightness' toggle (see
// tools/gen_pm64_transparency_map.py) -- drawn only with a translucent
// render mode by its display lists, map models, effect or entity code.
bool pm64I8TextureIsTranslucent(const std::string& archivePath);

} // namespace bitdeck
