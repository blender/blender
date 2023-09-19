/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_light.h"
#include "usd_light_convert.h"

#include "BLI_math_rotation.h"

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

  float light_surface_area = 1.0f;

  if (prim_.IsA<pxr::UsdLuxDiskLight>()) {
    /* Disk area light. */
    blight->type = LA_AREA;
    blight->area_shape = LA_AREA_DISK;

    pxr::UsdLuxDiskLight disk_light(prim_);
    if (disk_light) {
      float radius;
      if (get_authored_value(light_api.GetRadiusAttr(), motionSampleTime, &radius)) {
        blight->area_size = radius * 2.0f;
      }
    }

    const float radius = 0.5f * blight->area_size;
    light_surface_area = radius * radius * M_PI;
  }
  else if (prim_.IsA<pxr::UsdLuxRectLight>()) {
    /* Rectangular area light. */
    blight->type = LA_AREA;
    blight->area_shape = LA_AREA_RECT;

    pxr::UsdLuxRectLight rect_light(prim_);
    if (rect_light) {
      float width;
      if (get_authored_value(light_api.GetWidthAttr(), motionSampleTime, &width)) {
        blight->area_size = width;
      }

      float height;
      if (get_authored_value(light_api.GetHeightAttr(), motionSampleTime, &height)) {
        blight->area_sizey = height;
      }
    }

    light_surface_area = blight->area_size * blight->area_sizey;
  }
  else if (prim_.IsA<pxr::UsdLuxSphereLight>()) {
    /* Point and spot light. */
    blight->type = LA_LOCAL;

    pxr::UsdLuxSphereLight sphere_light(prim_);
    if (sphere_light) {
      pxr::UsdAttribute treatAsPoint_attr = sphere_light.GetTreatAsPointAttr();
      bool treatAsPoint;
      if (treatAsPoint_attr && treatAsPoint_attr.Get(&treatAsPoint, motionSampleTime) &&
          treatAsPoint) {
        blight->radius = 0.0f;
      }
      else {
        float radius = 0.0f;
        if (get_authored_value(light_api.GetRadiusAttr(), motionSampleTime, &radius)) {
          blight->radius = radius;
        }
      }
    }

    light_surface_area = 4.0f * M_PI * blight->radius * blight->radius;

    if (shaping_api && shaping_api.GetShapingConeAngleAttr().IsAuthored()) {
      blight->type = LA_SPOT;

      if (pxr::UsdAttribute cone_angle_attr = shaping_api.GetShapingConeAngleAttr()) {
        float cone_angle = 0.0f;
        if (cone_angle_attr.Get(&cone_angle, motionSampleTime)) {
          /* Blender spot size is twice the USD cone angle in radians. */
          if (cone_angle <= 90.0f) {
            blight->spotsize = cone_angle * (float(M_PI) / 180.0f) * 2.0f;
          }
          else {
            /* The cone angle is greater than 90 degrees, which translates to a
             * spotsize greater than M_PI (2 * 90 degrees converted to radians).
             * Blender's maximum spotsize is M_PI, so we make this a point light instead. */
            blight->type = LA_LOCAL;
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
  }
  else if (prim_.IsA<pxr::UsdLuxDistantLight>()) {
    blight->type = LA_SUN;

    pxr::UsdLuxDistantLight distant_light(prim_);
    if (distant_light) {
      float angle;
      if (get_authored_value(light_api.GetAngleAttr(), motionSampleTime, &angle)) {
        blight->sun_angle = angle * float(M_PI) / 180.0f;
      }
    }
  }

  const float meters_per_unit = static_cast<float>(
      pxr::UsdGeomGetStageMetersPerUnit(prim_.GetStage()));

  const float radius_scale = meters_per_unit * usd_world_scale_;

  /* Intensity */

  float intensity = 0.0f;
  if (get_authored_value(light_api.GetIntensityAttr(), motionSampleTime, &intensity)) {

    float intensity_scale = this->import_params_.light_intensity_scale;

    if (import_params_.convert_light_from_nits) {
      intensity_scale *= nits_to_energy_scale_factor(blight, radius_scale);
      blight->energy = intensity;
    }
    else if (blight->type == LA_SUN) {
      /* Unclear why, but approximately matches Karma. */
      blight->energy = intensity * 4.0f;
    }
    else {
      /* Convert from intensity to radiant flux. */
      blight->energy = intensity * M_PI;
    }
    blight->energy *= intensity_scale;
  }

  /* Exposure. */
  float exposure = 0.0f;
  if (get_authored_value(light_api.GetExposureAttr(), motionSampleTime, &exposure)) {
    blight->energy *= pow(2.0f, exposure);
  }

  /* Color. */
  pxr::GfVec3f color;
  if (get_authored_value(light_api.GetColorAttr(), motionSampleTime, &color)) {
    blight->r = color[0];
    blight->g = color[1];
    blight->b = color[2];
  }

  /* Diffuse and Specular. */
  if (pxr::UsdAttribute diff_attr = light_api.GetDiffuseAttr()) {
    float diff_fac = 1.0f;
    if (diff_attr.Get(&diff_fac, motionSampleTime)) {
      blight->diff_fac = diff_fac;
    }
  }

  float spec_fac = 1.0f;
  if (get_authored_value(light_api.GetSpecularAttr(), motionSampleTime, &spec_fac)) {
    blight->spec_fac = spec_fac;
  }

  if (!import_params_.convert_light_from_nits) {
    /* Normalize: Blender lights are always normalized, so inverse correct for it
     * TODO: take into account object transform, or natively support this as a
     * setting on lights in Blender. */
    bool normalize = false;
    if (pxr::UsdAttribute normalize_attr = light_api.GetNormalizeAttr()) {
      normalize_attr.Get(&normalize, motionSampleTime);
    }
    if (!normalize) {
      blight->energy *= light_surface_area;
    }
  }

  /* TODO:
   * bool GetEnableColorTemperatureAttr
   * float GetColorTemperatureAttr
   */

  if ((blight->type == LA_SPOT || blight->type == LA_LOCAL) && import_params_.scale_light_radius) {
    blight->radius *= radius_scale;
  }

  USDXformReader::read_object_data(bmain, motionSampleTime);
}

}  // namespace blender::io::usd
