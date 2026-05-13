/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_colorspace_utils.hh"

#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/colorSpaceAPI.h>

#include "BLI_string_utf8.h"

#include "DNA_image_types.h"

#include "IMB_colormanagement.hh"

namespace blender::io::usd {

namespace usdtokens {
static const pxr::TfToken sourceColorSpace("sourceColorSpace", pxr::TfToken::Immortal);
static const pxr::TfToken auto_("auto", pxr::TfToken::Immortal);
static const pxr::TfToken sRGB("sRGB", pxr::TfToken::Immortal);
static const pxr::TfToken data("data", pxr::TfToken::Immortal);
static const pxr::TfToken raw("raw", pxr::TfToken::Immortal);
static const pxr::TfToken RAW("RAW", pxr::TfToken::Immortal);
}  // namespace usdtokens

pxr::TfToken colorspace_scene_linear_interop_id()
{
  const char *scene_linear_name = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);
  const ColorSpace *cs = IMB_colormanagement_space_get_named(scene_linear_name);
  StringRefNull interop_id = (cs) ? IMB_colormanagement_space_get_interop_id(cs) : "";
  return (interop_id.is_empty()) ? pxr::TfToken() : pxr::TfToken(interop_id);
}

void colorspace_apply_to_prim(const pxr::UsdPrim &prim)
{
  const pxr::TfToken interop_id = colorspace_scene_linear_interop_id();
  if (interop_id.IsEmpty()) {
    return;
  }
  pxr::UsdColorSpaceAPI cs_api = pxr::UsdColorSpaceAPI::Apply(prim);
  cs_api.CreateColorSpaceNameAttr(pxr::VtValue(interop_id));
}

static const ColorSpace *colorspace_from_attr(const pxr::UsdAttribute &attr)
{
  pxr::TfToken cs_name = pxr::UsdColorSpaceAPI::ComputeColorSpaceName(attr);
  if (cs_name.IsEmpty()) {
    return nullptr;
  }

  const ColorSpace *cs = IMB_colormanagement_space_get_named(cs_name.GetText());
  if (!cs || IMB_colormanagement_space_is_scene_linear(cs) ||
      IMB_colormanagement_space_is_data(cs))
  {
    return nullptr;
  }

  return cs;
}

void colorspace_attr_to_scene_linear(const pxr::UsdAttribute &attr, pxr::GfVec3f &color)
{
  const ColorSpace *cs = colorspace_from_attr(attr);
  if (cs) {
    IMB_colormanagement_colorspace_to_scene_linear_v3(color.data(), cs);
  }
}

void colorspace_attr_to_scene_linear(const pxr::UsdAttribute &attr, ColorGeometry4f &color)
{
  const ColorSpace *cs = colorspace_from_attr(attr);
  if (cs) {
    IMB_colormanagement_colorspace_to_scene_linear_v4(&color.r, false, cs);
  }
}

void colorspace_attr_to_scene_linear(const pxr::UsdAttribute &attr,
                                     MutableSpan<ColorGeometry4f> colors)
{
  const ColorSpace *cs = colorspace_from_attr(attr);
  if (cs && !colors.is_empty()) {
    IMB_colormanagement_colorspace_to_scene_linear(&colors[0].r, colors.size(), 1, 4, cs, false);
  }
}

static pxr::TfToken get_source_color_space(const pxr::UsdShadeShader &usd_shader)
{
  if (!usd_shader) {
    return pxr::TfToken();
  }

  pxr::UsdShadeInput color_space_input = usd_shader.GetInput(usdtokens::sourceColorSpace);

  if (!color_space_input) {
    return pxr::TfToken();
  }

  pxr::VtValue color_space_val;
  if (color_space_input.Get(&color_space_val) && color_space_val.IsHolding<pxr::TfToken>()) {
    return color_space_val.UncheckedGet<pxr::TfToken>();
  }

  return pxr::TfToken();
}

void colorspace_to_image_texture(const pxr::UsdShadeShader &usd_shader,
                                 const pxr::UsdShadeInput &file_input,
                                 const bool is_data,
                                 Image *image)
{
  /* Set texture color space.
   * TODO(makowalski): For now, just checking for RAW color space,
   * assuming sRGB otherwise, but more complex logic might be
   * required if the color space is "auto". */
  pxr::TfToken color_space = get_source_color_space(usd_shader);

  if (color_space.IsEmpty()) {
    color_space = file_input.GetAttr().GetColorSpace();
  }

  if (color_space.IsEmpty()) {
    color_space = pxr::UsdColorSpaceAPI::ComputeColorSpaceName(usd_shader.GetPrim());
  }

  if (color_space.IsEmpty()) {
    /* At this point, assume the "auto" space and translate accordingly. */
    color_space = usdtokens::auto_;
  }

  if (color_space == usdtokens::auto_) {
    /* If it's auto, determine whether to apply color correction based
     * on incoming connection (passed in from outer functions). */
    STRNCPY_UTF8(image->colorspace_settings.name,
                 IMB_colormanagement_role_colorspace_name_get(is_data ? COLOR_ROLE_DATA :
                                                                        COLOR_ROLE_DEFAULT_BYTE));
  }

  else if (color_space == usdtokens::sRGB) {
    STRNCPY_UTF8(image->colorspace_settings.name, IMB_colormanagement_srgb_colorspace_name_get());
  }
  /* Due to there being a lot of non-compliant USD assets out there, this is
   * a special case where we need to check for different spellings here.
   * On write, we are *only* using the correct, lower-case "raw" token. */
  else if (ELEM(color_space, usdtokens::data, usdtokens::RAW, usdtokens::raw)) {
    STRNCPY_UTF8(image->colorspace_settings.name,
                 IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DATA));
  }
  else {
    const ColorSpace *cs = IMB_colormanagement_space_get_named(color_space.GetText());
    if (cs) {
      STRNCPY_UTF8(image->colorspace_settings.name, IMB_colormanagement_colorspace_get_name(cs));
    }
  }
}

void colorspace_from_image_texture(const Image *image, pxr::UsdShadeShader &shader)
{
  if (!image) {
    return;
  }

  /* Write sourceColorSpace input for backward compatibility with older USD readers. */
  if (IMB_colormanagement_space_name_is_data(image->colorspace_settings.name)) {
    shader.CreateInput(usdtokens::sourceColorSpace, pxr::SdfValueTypeNames->Token)
        .Set(usdtokens::raw);
  }
  else if (IMB_colormanagement_space_name_is_srgb(image->colorspace_settings.name)) {
    shader.CreateInput(usdtokens::sourceColorSpace, pxr::SdfValueTypeNames->Token)
        .Set(usdtokens::sRGB);
  }

  /* Write ColorSpaceAPI with the interop ID, which supports any colorspace. */
  const ColorSpace *cs = IMB_colormanagement_space_get_named(image->colorspace_settings.name);
  if (cs) {
    StringRefNull interop_id = IMB_colormanagement_space_get_interop_id(cs);
    if (!interop_id.is_empty()) {
      pxr::UsdColorSpaceAPI cs_api = pxr::UsdColorSpaceAPI::Apply(shader.GetPrim());
      cs_api.CreateColorSpaceNameAttr(pxr::VtValue(pxr::TfToken(interop_id)));
    }
  }
}

}  // namespace blender::io::usd
