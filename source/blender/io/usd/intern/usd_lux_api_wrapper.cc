#include "usd_lux_api_wrapper.h"

namespace blender::io::usd {
namespace usdtokens {
// Attribute names.
const pxr::TfToken Intensity("intensity", pxr::TfToken::Immortal);
const pxr::TfToken Exposure("exposure", pxr::TfToken::Immortal);
const pxr::TfToken Diffuse("diffuse", pxr::TfToken::Immortal);
const pxr::TfToken Specular("specular", pxr::TfToken::Immortal);
const pxr::TfToken Normalize("normalize", pxr::TfToken::Immortal);
const pxr::TfToken Color("color", pxr::TfToken::Immortal);

const pxr::TfToken Radius("radius", pxr::TfToken::Immortal);
const pxr::TfToken Width("width", pxr::TfToken::Immortal);
const pxr::TfToken Height("height", pxr::TfToken::Immortal);
const pxr::TfToken Angle("angle", pxr::TfToken::Immortal);

const pxr::TfToken EnableColorTemperature("enableColorTemperature", pxr::TfToken::Immortal);
const pxr::TfToken ColorTemperature("olorTemperature", pxr::TfToken::Immortal);

const pxr::TfToken ShapingFocus("shaping:focus", pxr::TfToken::Immortal);
const pxr::TfToken ShapingFocusTint("shaping:focus:tint", pxr::TfToken::Immortal);
const pxr::TfToken ShapingConeAngle("shaping:cone:angle", pxr::TfToken::Immortal);
const pxr::TfToken ShapingConeSoftness("shaping:cone:softness", pxr::TfToken::Immortal);
const pxr::TfToken ShapingIesFile("shaping:ies:file", pxr::TfToken::Immortal);
const pxr::TfToken ShapingIesNormalize("shaping:ies:normalize", pxr::TfToken::Immortal);
}  // namespace usdtokens
}  // namespace blender::io::usd
