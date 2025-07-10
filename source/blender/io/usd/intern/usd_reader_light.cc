/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_light.hh"

#include "BLI_math_rotation.h"

#include "BKE_light.h"
#include "BKE_object.hh"

#include "IMB_colormanagement.hh"

#include "DNA_light_types.h"
#include "DNA_object_types.h"

#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/shapingAPI.h>
#include <pxr/usd/usdLux/sphereLight.h>

namespace blender::io::usd {

void USDLightReader::create_object(Main *bmain)
{
  Light *blight = BKE_light_add(bmain, name_.c_str());

  object_ = BKE_object_add_only_object(bmain, OB_LAMP, name_.c_str());
  object_->data = blight;
}

void USDLightReader::read_object_data(Main *bmain, const pxr::UsdTimeCode time)
{
  Light *blight = (Light *)object_->data;

  if (blight == nullptr) {
    return;
  }

  pxr::UsdLuxLightAPI light_api(prim_);
  if (!light_api) {
    return;
  }

  if (prim_.IsA<pxr::UsdLuxDiskLight>()) {
    /* Disk area light. */
    blight->type = LA_AREA;
    blight->area_shape = LA_AREA_DISK;

    pxr::UsdLuxDiskLight disk_light(prim_);
    if (disk_light) {
      if (pxr::UsdAttribute radius_attr = disk_light.GetRadiusAttr()) {
        float radius = 0.0f;
        if (radius_attr.Get(&radius, time)) {
          blight->area_size = radius * 2.0f;
        }
      }
    }
  }
  else if (prim_.IsA<pxr::UsdLuxRectLight>()) {
    /* Rectangular area light. */
    blight->type = LA_AREA;
    blight->area_shape = LA_AREA_RECT;

    pxr::UsdLuxRectLight rect_light(prim_);
    if (rect_light) {
      if (pxr::UsdAttribute width_attr = rect_light.GetWidthAttr()) {
        float width = 0.0f;
        if (width_attr.Get(&width, time)) {
          blight->area_size = width;
        }
      }

      if (pxr::UsdAttribute height_attr = rect_light.GetHeightAttr()) {
        float height = 0.0f;
        if (height_attr.Get(&height, time)) {
          blight->area_sizey = height;
        }
      }
    }
  }
  else if (prim_.IsA<pxr::UsdLuxSphereLight>()) {
    /* Point and spot light. */
    blight->type = LA_LOCAL;

    pxr::UsdLuxSphereLight sphere_light(prim_);
    if (sphere_light) {
      pxr::UsdAttribute treatAsPoint_attr = sphere_light.GetTreatAsPointAttr();
      bool treatAsPoint;
      if (treatAsPoint_attr && treatAsPoint_attr.Get(&treatAsPoint, time) && treatAsPoint) {
        blight->radius = 0.0f;
      }
      else if (pxr::UsdAttribute radius_attr = sphere_light.GetRadiusAttr()) {
        float radius = 0.0f;
        if (radius_attr.Get(&radius, time)) {
          blight->radius = radius;
        }
      }
    }

    pxr::UsdLuxShapingAPI shaping_api = pxr::UsdLuxShapingAPI(prim_);
    if (shaping_api && shaping_api.GetShapingConeAngleAttr().IsAuthored()) {
      blight->type = LA_SPOT;

      if (pxr::UsdAttribute cone_angle_attr = shaping_api.GetShapingConeAngleAttr()) {
        float cone_angle = 0.0f;
        if (cone_angle_attr.Get(&cone_angle, time)) {
          blight->spotsize = DEG2RADF(cone_angle) * 2.0f;
        }
      }

      if (pxr::UsdAttribute cone_softness_attr = shaping_api.GetShapingConeSoftnessAttr()) {
        float cone_softness = 0.0f;
        if (cone_softness_attr.Get(&cone_softness, time)) {
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
        if (angle_attr.Get(&angle, time)) {
          blight->sun_angle = DEG2RADF(angle * 2.0f);
        }
      }
    }
  }

  /* Intensity */
  if (pxr::UsdAttribute intensity_attr = light_api.GetIntensityAttr()) {
    float intensity = 0.0f;
    if (intensity_attr.Get(&intensity, time)) {
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
    if (exposure_attr.Get(&exposure, time)) {
      blight->exposure = exposure;
    }
  }

  /* Color. */
  if (pxr::UsdAttribute color_attr = light_api.GetColorAttr()) {
    pxr::GfVec3f color;
    if (color_attr.Get(&color, time)) {
      blight->r = color[0];
      blight->g = color[1];
      blight->b = color[2];
    }
  }

  /* Temperature */
  if (pxr::UsdAttribute enable_temperature_attr = light_api.GetEnableColorTemperatureAttr()) {
    bool enable_temperature = false;
    if (enable_temperature_attr.Get(&enable_temperature, time)) {
      if (enable_temperature) {
        blight->mode |= LA_USE_TEMPERATURE;
      }
    }
  }

  if (pxr::UsdAttribute color_temperature_attr = light_api.GetColorTemperatureAttr()) {
    float color_temperature = 6500.0f;
    if (color_temperature_attr.Get(&color_temperature, time)) {
      blight->temperature = color_temperature;
    }
  }

  /* Diffuse and Specular. */
  if (pxr::UsdAttribute diff_attr = light_api.GetDiffuseAttr()) {
    float diff_fac = 1.0f;
    if (diff_attr.Get(&diff_fac, time)) {
      blight->diff_fac = diff_fac;
    }
  }
  if (pxr::UsdAttribute spec_attr = light_api.GetSpecularAttr()) {
    float spec_fac = 1.0f;
    if (spec_attr.Get(&spec_fac, time)) {
      blight->spec_fac = spec_fac;
    }
  }

  /* Normalize */
  if (pxr::UsdAttribute normalize_attr = light_api.GetNormalizeAttr()) {
    bool normalize = false;
    if (normalize_attr.Get(&normalize, time)) {
      if (!normalize) {
        blight->mode |= LA_UNNORMALIZED;
      }
    }
  }

  USDXformReader::read_object_data(bmain, time);
}

}  // namespace blender::io::usd
