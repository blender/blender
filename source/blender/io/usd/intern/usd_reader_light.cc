/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_light.hh"

#include "BLI_math_rotation.h"

#include "BKE_light.h"
#include "BKE_object.hh"

#include "DNA_light_types.h"
#include "DNA_object_types.h"

#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/shapingAPI.h>
#include <pxr/usd/usdLux/sphereLight.h>

namespace blender::io::usd {

void USDLightReader::create_object(Main *bmain, const double /*motionSampleTime*/)
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
#if PXR_VERSION >= 2111
  pxr::UsdLuxLightAPI light_api(prim_);
#else
  pxr::UsdLuxLight light_api(prim_);
#endif

  if (!light_api) {
    return;
  }

  float light_surface_area = 1.0f;

  if (prim_.IsA<pxr::UsdLuxDiskLight>()) {
    /* Disk area light. */
    blight->type = LA_AREA;
    blight->area_shape = LA_AREA_DISK;

    pxr::UsdLuxDiskLight disk_light(prim_);
    if (disk_light) {
      if (pxr::UsdAttribute radius_attr = disk_light.GetRadiusAttr()) {
        float radius = 0.0f;
        if (radius_attr.Get(&radius, motionSampleTime)) {
          blight->area_size = radius * 2.0f;
        }
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
      if (pxr::UsdAttribute width_attr = rect_light.GetWidthAttr()) {
        float width = 0.0f;
        if (width_attr.Get(&width, motionSampleTime)) {
          blight->area_size = width;
        }
      }

      if (pxr::UsdAttribute height_attr = rect_light.GetHeightAttr()) {
        float height = 0.0f;
        if (height_attr.Get(&height, motionSampleTime)) {
          blight->area_sizey = height;
        }
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
          treatAsPoint)
      {
        blight->radius = 0.0f;
      }
      else if (pxr::UsdAttribute radius_attr = sphere_light.GetRadiusAttr()) {
        float radius = 0.0f;
        if (radius_attr.Get(&radius, motionSampleTime)) {
          blight->radius = radius;
        }
      }
    }

    light_surface_area = 4.0f * M_PI * blight->radius * blight->radius;

    pxr::UsdLuxShapingAPI shaping_api = pxr::UsdLuxShapingAPI(prim_);
    if (shaping_api && shaping_api.GetShapingConeAngleAttr().IsAuthored()) {
      blight->type = LA_SPOT;

      if (pxr::UsdAttribute cone_angle_attr = shaping_api.GetShapingConeAngleAttr()) {
        float cone_angle = 0.0f;
        if (cone_angle_attr.Get(&cone_angle, motionSampleTime)) {
          blight->spotsize = DEG2RADF(cone_angle) * 2.0f;
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
      if (pxr::UsdAttribute angle_attr = distant_light.GetAngleAttr()) {
        float angle = 0.0f;
        if (angle_attr.Get(&angle, motionSampleTime)) {
          blight->sun_angle = DEG2RADF(angle * 2.0f);
        }
      }
    }
  }

  /* Intensity */
  if (pxr::UsdAttribute intensity_attr = light_api.GetIntensityAttr()) {
    float intensity = 0.0f;
    if (intensity_attr.Get(&intensity, motionSampleTime)) {
      if (blight->type == LA_SUN) {
        /* Unclear why, but approximately matches Karma. */
        blight->energy = intensity * 4.0f;
      }
      else {
        /* Convert from intensity to radiant flux. */
        blight->energy = intensity * M_PI;
      }
      blight->energy *= this->import_params_.light_intensity_scale;
    }
  }

  /* Exposure. */
  if (pxr::UsdAttribute exposure_attr = light_api.GetExposureAttr()) {
    float exposure = 0.0f;
    if (exposure_attr.Get(&exposure, motionSampleTime)) {
      blight->energy *= pow(2.0f, exposure);
    }
  }

  /* Color. */
  if (pxr::UsdAttribute color_attr = light_api.GetColorAttr()) {
    pxr::GfVec3f color;
    if (color_attr.Get(&color, motionSampleTime)) {
      blight->r = color[0];
      blight->g = color[1];
      blight->b = color[2];
    }
  }

  /* Diffuse and Specular. */
  if (pxr::UsdAttribute diff_attr = light_api.GetDiffuseAttr()) {
    float diff_fac = 1.0f;
    if (diff_attr.Get(&diff_fac, motionSampleTime)) {
      blight->diff_fac = diff_fac;
    }
  }
  if (pxr::UsdAttribute spec_attr = light_api.GetSpecularAttr()) {
    float spec_fac = 1.0f;
    if (spec_attr.Get(&spec_fac, motionSampleTime)) {
      blight->spec_fac = spec_fac;
    }
  }

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

  /* TODO:
   * bool GetEnableColorTemperatureAttr
   * float GetColorTemperatureAttr
   */

  USDXformReader::read_object_data(bmain, motionSampleTime);
}

}  // namespace blender::io::usd
