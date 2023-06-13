/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_light.h"

#include "BKE_light.h"
#include "BKE_object.h"

#include "DNA_light_types.h"
#include "DNA_object_types.h"

#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/shapingAPI.h>
#include <pxr/usd/usdLux/sphereLight.h>

#include <iostream>

namespace blender::io::usd {

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
#if PXR_VERSION >= 2111
  pxr::UsdLuxLightAPI light_api(prim_);
#else
  pxr::UsdLuxLight light_api(prim_);
#endif

  if (!light_api) {
    return;
  }

  pxr::UsdLuxShapingAPI shaping_api;

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

    shaping_api = pxr::UsdLuxShapingAPI(prim_);

    if (shaping_api && shaping_api.GetShapingConeAngleAttr().IsAuthored()) {
      blight->type = LA_SPOT;
    }
  }
  else if (prim_.IsA<pxr::UsdLuxDistantLight>()) {
    blight->type = LA_SUN;
  }

  /* Set light values. */

  if (pxr::UsdAttribute intensity_attr = light_api.GetIntensityAttr()) {
    float intensity = 0.0f;
    if (intensity_attr.Get(&intensity, motionSampleTime)) {
      blight->energy = intensity * this->import_params_.light_intensity_scale;
    }
  }

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

  if (pxr::UsdAttribute spec_attr = light_api.GetSpecularAttr()) {
    float spec = 0.0f;
    if (spec_attr.Get(&spec, motionSampleTime)) {
      blight->spec_fac = spec;
    }
  }

  if (pxr::UsdAttribute color_attr = light_api.GetColorAttr()) {
    pxr::GfVec3f color;
    if (color_attr.Get(&color, motionSampleTime)) {
      blight->r = color[0];
      blight->g = color[1];
      blight->b = color[2];
    }
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

  switch (blight->type) {
    case LA_AREA:
      if (blight->area_shape == LA_AREA_RECT && prim_.IsA<pxr::UsdLuxRectLight>()) {

        pxr::UsdLuxRectLight rect_light(prim_);

        if (!rect_light) {
          break;
        }

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
      else if (blight->area_shape == LA_AREA_DISK && prim_.IsA<pxr::UsdLuxDiskLight>()) {

        pxr::UsdLuxDiskLight disk_light(prim_);

        if (!disk_light) {
          break;
        }

        if (pxr::UsdAttribute radius_attr = disk_light.GetRadiusAttr()) {
          float radius = 0.0f;
          if (radius_attr.Get(&radius, motionSampleTime)) {
            blight->area_size = radius * 2.0f;
          }
        }
      }
      break;
    case LA_LOCAL:
      if (prim_.IsA<pxr::UsdLuxSphereLight>()) {

        pxr::UsdLuxSphereLight sphere_light(prim_);

        if (!sphere_light) {
          break;
        }

        if (pxr::UsdAttribute radius_attr = sphere_light.GetRadiusAttr()) {
          float radius = 0.0f;
          if (radius_attr.Get(&radius, motionSampleTime)) {
            blight->radius = radius;
          }
        }
      }
      break;
    case LA_SPOT:
      if (prim_.IsA<pxr::UsdLuxSphereLight>()) {

        pxr::UsdLuxSphereLight sphere_light(prim_);

        if (!sphere_light) {
          break;
        }

        if (pxr::UsdAttribute radius_attr = sphere_light.GetRadiusAttr()) {
          float radius = 0.0f;
          if (radius_attr.Get(&radius, motionSampleTime)) {
            blight->radius = radius;
          }
        }

        if (!shaping_api) {
          break;
        }

        if (pxr::UsdAttribute cone_angle_attr = shaping_api.GetShapingConeAngleAttr()) {
          float cone_angle = 0.0f;
          if (cone_angle_attr.Get(&cone_angle, motionSampleTime)) {
            blight->spotsize = cone_angle * (float(M_PI) / 180.0f) * 2.0f;
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

        if (pxr::UsdAttribute angle_attr = distant_light.GetAngleAttr()) {
          float angle = 0.0f;
          if (angle_attr.Get(&angle, motionSampleTime)) {
            blight->sun_angle = angle * float(M_PI) / 180.0f;
          }
        }
      }
      break;
  }

  USDXformReader::read_object_data(bmain, motionSampleTime);
}

}  // namespace blender::io::usd
