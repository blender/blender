/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_domelight.hh"
#include "usd_light_convert.hh"

#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/domeLight_1.h>
#include <pxr/usd/usdLux/tokens.h>

namespace usdtokens {
// Attribute names.
static const pxr::TfToken color("color", pxr::TfToken::Immortal);
static const pxr::TfToken intensity("intensity", pxr::TfToken::Immortal);
static const pxr::TfToken texture_file("texture:file", pxr::TfToken::Immortal);
static const pxr::TfToken pole_axis("poleAxis", pxr::TfToken::Immortal);
}  // namespace usdtokens

namespace blender::io::usd {

/**
 * If the given attribute has an authored value, return its value in the r_value
 * out parameter.
 *
 * We wish to support older UsdLux APIs in older versions of USD.  For example,
 * in previous versions of the API, shader input attributes did not have the
 * "inputs:" prefix.  One can provide the older input attribute name in the
 * 'fallback_attr_name' argument, and that attribute will be queried if 'attr'
 * doesn't exist or doesn't have an authored value.
 */
template<typename T>
static bool get_authored_value(const pxr::UsdAttribute &attr,
                               const pxr::UsdTimeCode time,
                               const pxr::UsdPrim &prim,
                               const pxr::TfToken fallback_attr_name,
                               T *r_value)
{
  if (attr && attr.HasAuthoredValue()) {
    return attr.Get<T>(r_value, time);
  }

  if (!prim || fallback_attr_name.IsEmpty()) {
    return false;
  }

  pxr::UsdAttribute fallback_attr = prim.GetAttribute(fallback_attr_name);
  if (fallback_attr && fallback_attr.HasAuthoredValue()) {
    return fallback_attr.Get<T>(r_value, time);
  }

  return false;
}

template<typename T> static float get_intensity(const T &dome_light, const pxr::UsdTimeCode time)
{
  float intensity = 1.0f;
  get_authored_value(
      dome_light.GetIntensityAttr(), time, dome_light.GetPrim(), usdtokens::intensity, &intensity);
  return intensity;
}

template<typename T>
static bool get_tex_path(const T &dome_light,
                         const pxr::UsdTimeCode time,
                         pxr::SdfAssetPath *tex_path)
{
  bool has_tex = get_authored_value(dome_light.GetTextureFileAttr(),
                                    time,
                                    dome_light.GetPrim(),
                                    usdtokens::texture_file,
                                    tex_path);
  return has_tex;
}

template<typename T>
static bool get_color(const T &dome_light, const pxr::UsdTimeCode time, pxr::GfVec3f *color)
{
  bool has_color = get_authored_value(
      dome_light.GetColorAttr(), time, dome_light.GetPrim(), usdtokens::color, color);
  return has_color;
}

static pxr::TfToken get_pole_axis(const pxr::UsdLuxDomeLight_1 &dome_light,
                                  const pxr::UsdTimeCode time)
{
  pxr::TfToken pole_axis = pxr::UsdLuxTokens->scene;
  get_authored_value(dome_light.GetPoleAxisAttr(), time, dome_light.GetPrim(), {}, &pole_axis);
  return pole_axis;
}

void USDDomeLightReader::create_object(Scene *scene, Main *bmain)
{
  USDImportDomeLightData dome_light_data;

  /* Time varying dome lights are not currently supported. */
  constexpr pxr::UsdTimeCode time = 0.0;

  if (prim_.IsA<pxr::UsdLuxDomeLight>()) {
    pxr::UsdLuxDomeLight dome_light = pxr::UsdLuxDomeLight(prim_);
    dome_light_data.intensity = get_intensity(dome_light, time);
    dome_light_data.has_tex = get_tex_path(dome_light, time, &dome_light_data.tex_path);
    dome_light_data.has_color = get_color(dome_light, time, &dome_light_data.color);
    dome_light_data.pole_axis = pxr::UsdLuxTokens->Y;
  }
  else if (prim_.IsA<pxr::UsdLuxDomeLight_1>()) {
    pxr::UsdLuxDomeLight_1 dome_light = pxr::UsdLuxDomeLight_1(prim_);
    dome_light_data.intensity = get_intensity(dome_light, time);
    dome_light_data.has_tex = get_tex_path(dome_light, time, &dome_light_data.tex_path);
    dome_light_data.has_color = get_color(dome_light, time, &dome_light_data.color);
    dome_light_data.pole_axis = get_pole_axis(dome_light, time);
  }

  dome_light_to_world_material(import_params_, scene, bmain, dome_light_data, prim_);
}

}  // namespace blender::io::usd
