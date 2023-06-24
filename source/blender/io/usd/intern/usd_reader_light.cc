/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_light.h"
#include "usd_light_convert.h"

#include "BKE_light.h"
#include "BKE_object.h"

#include "DNA_light_types.h"
#include "DNA_object_types.h"

#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/shapingAPI.h>
#include <pxr/usd/usdLux/sphereLight.h>

#include "usd_lux_api_wrapper.h"

#include <iostream>


namespace {

template<typename T>
bool get_authored_value(const pxr::UsdAttribute attr, const double motionSampleTime, T *r_value)
{
  if (attr && attr.HasAuthoredValue()) {
    return attr.Get<T>(r_value, motionSampleTime);
  }

  return false;
}

}  // End anonymous namespace.

namespace blender::io::usd {

USDLightReader::USDLightReader(const pxr::UsdPrim &prim,
                               const USDImportParams &import_params,
                               const ImportSettings &settings,
                               pxr::UsdGeomXformCache *xf_cache)
    : USDXformReader(prim, import_params, settings), usd_world_scale_(1.0f)
{
  if (xf_cache && import_params.convert_light_from_nits) {
    pxr::GfMatrix4d xf = xf_cache->GetLocalToWorldTransform(prim);
    pxr::GfMatrix4d r;
    pxr::GfVec3d s;
    pxr::GfMatrix4d u;
    pxr::GfVec3d t;
    pxr::GfMatrix4d p;
    xf.Factor(&r, &s, &u, &t, &p);

    usd_world_scale_ = (s[0] + s[1] + s[2]) / 3.0f;
  }
}

void USDLightReader::create_object(Main *bmain, const double /* motionSampleTime */)
{
  Light *blight = static_cast<Light *>(BKE_light_add(bmain, name_.c_str()));

  object_ = BKE_object_add_only_object(bmain, OB_LAMP, name_.c_str());
  object_->data = blight;
}

void USDLightReader::read_object_data(Main *bmain, const double motionSampleTime)
{
  Light *blight = (Light *)object_->data;

  if (blight == nullptr) {
    return;
  }

  if (!prim_) {
    return;
  }
  UsdLuxWrapper light_api(prim_);

  if (!light_api) {
    return;
  }

  UsdShapingWrapper shaping_api(prim_);

  /* Set light type. */

  if (prim_.IsA<pxr::UsdLuxDiskLight>()) {
    blight->type = LA_AREA;
    blight->area_shape = LA_AREA_DISK;
    /* Ellipse lights are not currently supported */
  }
  else if (prim_.IsA<pxr::UsdLuxRectLight>()) {
    blight->type = LA_AREA;
    blight->area_shape = LA_AREA_RECT;
  }
  else if (prim_.IsA<pxr::UsdLuxSphereLight>()) {
    blight->type = LA_LOCAL;

    /* We don't call static_cast<bool>(shaping_api) in the
     * following conditional because some lights might have
     * shaping attributes authored without an applied shaping
     * API schema. */
    if (shaping_api.GetShapingConeAngleAttr().IsAuthored()) {
      blight->type = LA_SPOT;
    }
  }
  else if (prim_.IsA<pxr::UsdLuxDistantLight>()) {
    blight->type = LA_SUN;
  }

  /* Set light values. */

  /* In USD 21, light attributes were renamed to have an 'inputs:' prefix
   * (e.g., 'inputs:intensity'). Here and below, for backward compatibility
   * with older USD versions, we also query attributes using the previous
   * naming scheme that omits this prefix. */

  /* TODO(makowalsk): Not currently supported. */
#if 0
  pxr::VtValue exposure;
  light_prim.GetExposureAttr().Get(&exposure, motionSampleTime);
#endif

  /* TODO(makowalsk): Not currently supported */
#if 0
  pxr::VtValue diffuse;
  light_prim.GetDiffuseAttr().Get(&diffuse, motionSampleTime);
#endif

  float specular;
  if (get_authored_value(light_api.GetSpecularAttr(), motionSampleTime, &specular)) {
    blight->spec_fac = specular;
  }

  pxr::GfVec3f color;
  if (get_authored_value(light_api.GetColorAttr(), motionSampleTime, &color)) {
    blight->r = color[0];
    blight->g = color[1];
    blight->b = color[2];
  }

  /* TODO(makowalski): Not currently supported. */
#if 0
  pxr::VtValue use_color_temp;
  light_prim.GetEnableColorTemperatureAttr().Get(&use_color_temp, motionSampleTime);
#endif

  /* TODO(makowalski): Not currently supported. */
#if 0
  pxr::VtValue color_temp;
  light_prim.GetColorTemperatureAttr().Get(&color_temp, motionSampleTime);
#endif

  // XXX - apply scene scale to local and spot lights but not area lights (?)
  switch (blight->type) {
    case LA_AREA:
      if (blight->area_shape == LA_AREA_RECT && prim_.IsA<pxr::UsdLuxRectLight>()) {

        pxr::UsdLuxRectLight rect_light(prim_);

        if (!rect_light) {
          break;
        }

        float width;
        if (get_authored_value(light_api.GetWidthAttr(), motionSampleTime, &width)) {
          blight->area_size = width;
        }

        float height;
        if (get_authored_value(light_api.GetHeightAttr(), motionSampleTime, &height)) {
          blight->area_sizey = height;
        }
      }
      else if (blight->area_shape == LA_AREA_DISK && prim_.IsA<pxr::UsdLuxDiskLight>()) {

        pxr::UsdLuxDiskLight disk_light(prim_);

        if (!disk_light) {
          break;
        }

        float radius;
        if (get_authored_value(light_api.GetRadiusAttr(), motionSampleTime, &radius)) {
          blight->area_size = radius * 2.0f;
        }
      }
      break;
    case LA_LOCAL:
      if (prim_.IsA<pxr::UsdLuxSphereLight>()) {

        pxr::UsdLuxSphereLight sphere_light(prim_);

        if (!sphere_light) {
          break;
        }

        float radius;
        if (get_authored_value(light_api.GetRadiusAttr(), motionSampleTime, &radius)) {
          blight->radius = radius;
        }
      }
      break;
    case LA_SPOT:
      if (prim_.IsA<pxr::UsdLuxSphereLight>()) {
        pxr::UsdLuxSphereLight sphere_light(prim_);

        if (!sphere_light) {
          break;
        }

        float radius;
        if (get_authored_value(light_api.GetRadiusAttr(), motionSampleTime, &radius)) {
          blight->radius = radius;
        }

        if (!shaping_api) {
          break;
        }

        if (pxr::UsdAttribute cone_angle_attr = shaping_api.GetShapingConeAngleAttr()) {
          float cone_angle = 0.0f;
          if (cone_angle_attr.Get(&cone_angle, motionSampleTime)) {
            float spot_size = cone_angle * (float(M_PI) / 180.0f) * 2.0f;

            if (spot_size <= 180) {
              blight->spotsize = spot_size;
            }
            else {
              /* The spot size is greater the 180 degrees, which Blender doesn't support so we
               * make this a sphere light instead. */
              blight->type = LA_LOCAL;
              break;
            }
          }
        }

        if (pxr::UsdAttribute cone_softness_attr = shaping_api.GetShapingConeSoftnessAttr()) {
          float cone_softness = 0.0f;
          if (cone_softness_attr.Get(&cone_softness, motionSampleTime)) {
            blight->spotblend = cone_softness;
          }
        }
      }
      break;
    case LA_SUN:
      if (prim_.IsA<pxr::UsdLuxDistantLight>()) {
        pxr::UsdLuxDistantLight distant_light(prim_);

        if (!distant_light) {
          break;
        }

        float angle;
        if (get_authored_value(light_api.GetAngleAttr(), motionSampleTime, &angle)) {
          blight->sun_angle = angle * float(M_PI) / 180.0f;
        }
      }
      break;
  }

  const float meters_per_unit = static_cast<float>(
      pxr::UsdGeomGetStageMetersPerUnit(prim_.GetStage()));

  const float radius_scale = meters_per_unit * usd_world_scale_;

  float intensity;
  if (get_authored_value(light_api.GetIntensityAttr(), motionSampleTime, &intensity)) {

    float intensity_scale = this->import_params_.light_intensity_scale;

    if (import_params_.convert_light_from_nits) {
      intensity_scale *= nits_to_energy_scale_factor(blight, radius_scale);
    }

    blight->energy = intensity * intensity_scale;
  }

  if ((blight->type == LA_SPOT || blight->type == LA_LOCAL) && import_params_.scale_light_radius) {
    blight->radius *= radius_scale;
  }

  USDXformReader::read_object_data(bmain, motionSampleTime);
}

}  // namespace blender::io::usd
