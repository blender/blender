/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_light.h"
#include "usd_hierarchy_iterator.h"

#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/shapingAPI.h>
#include <pxr/usd/usdLux/sphereLight.h>

#include "BLI_assert.h"
#include "BLI_math_rotation.h"
#include "BLI_utildefines.h"

#include "DNA_light_types.h"
#include "DNA_object_types.h"

namespace blender::io::usd {

USDLightWriter::USDLightWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx) {}

bool USDLightWriter::is_supported(const HierarchyContext *context) const
{
  Light *light = static_cast<Light *>(context->object->data);
  return ELEM(light->type, LA_AREA, LA_LOCAL, LA_SUN, LA_SPOT);
}

void USDLightWriter::do_write(HierarchyContext &context)
{
  pxr::UsdStageRefPtr stage = usd_export_context_.stage;
  const pxr::SdfPath &usd_path = usd_export_context_.usd_path;
  pxr::UsdTimeCode timecode = get_export_time_code();

  Light *light = static_cast<Light *>(context.object->data);
#if PXR_VERSION >= 2111
  pxr::UsdLuxLightAPI usd_light_api;
#else
  pxr::UsdLuxLight usd_light_api;

#endif

  switch (light->type) {
    case LA_AREA:
      switch (light->area_shape) {
        case LA_AREA_DISK:
        case LA_AREA_ELLIPSE: { /* An ellipse light will deteriorate into a disk light. */
          pxr::UsdLuxDiskLight disk_light = pxr::UsdLuxDiskLight::Define(stage, usd_path);
          disk_light.CreateRadiusAttr().Set(light->area_size, timecode);
#if PXR_VERSION >= 2111
          usd_light_api = disk_light.LightAPI();
#else
          usd_light_api = disk_light;
#endif
          break;
        }
        case LA_AREA_RECT: {
          pxr::UsdLuxRectLight rect_light = pxr::UsdLuxRectLight::Define(stage, usd_path);
          rect_light.CreateWidthAttr().Set(light->area_size, timecode);
          rect_light.CreateHeightAttr().Set(light->area_sizey, timecode);
#if PXR_VERSION >= 2111
          usd_light_api = rect_light.LightAPI();
#else
          usd_light_api = rect_light;
#endif
          break;
        }
        case LA_AREA_SQUARE: {
          pxr::UsdLuxRectLight rect_light = pxr::UsdLuxRectLight::Define(stage, usd_path);
          rect_light.CreateWidthAttr().Set(light->area_size, timecode);
          rect_light.CreateHeightAttr().Set(light->area_size, timecode);
#if PXR_VERSION >= 2111
          usd_light_api = rect_light.LightAPI();
#else
          usd_light_api = rect_light;
#endif
          break;
        }
      }
      break;
    case LA_LOCAL:
    case LA_SPOT: {
      pxr::UsdLuxSphereLight sphere_light = pxr::UsdLuxSphereLight::Define(stage, usd_path);
      sphere_light.CreateRadiusAttr().Set(light->radius, timecode);
      if (light->radius == 0.0f) {
        sphere_light.CreateTreatAsPointAttr().Set(true, timecode);
      }

      if (light->type == LA_SPOT) {
        pxr::UsdLuxShapingAPI shaping_api = pxr::UsdLuxShapingAPI::Apply(sphere_light.GetPrim());
        if (shaping_api) {
          shaping_api.CreateShapingConeAngleAttr().Set(RAD2DEGF(light->spotsize) / 2.0f, timecode);
          shaping_api.CreateShapingConeSoftnessAttr().Set(light->spotblend, timecode);
        }
      }

#if PXR_VERSION >= 2111
      usd_light_api = sphere_light.LightAPI();
#else
      usd_light_api = sphere_light;
#endif
      break;
    }
    case LA_SUN: {
      pxr::UsdLuxDistantLight distant_light = pxr::UsdLuxDistantLight::Define(stage, usd_path);
      /* TODO(makowalski): set angle attribute here. */
#if PXR_VERSION >= 2111
      usd_light_api = distant_light.LightAPI();
#else
      usd_light_api = distant_light;
#endif
      break;
    }
    default:
      BLI_assert_msg(0, "is_supported() returned true for unsupported light type");
  }

  float usd_intensity;
  if (light->type == LA_SUN) {
    /* Unclear why, but approximately matches Karma. */
    usd_intensity = light->energy / 4.0f;
  }
  else {
    /* Convert from radiant flux to intensity. */
    usd_intensity = light->energy / M_PI;
  }

  usd_light_api.CreateIntensityAttr().Set(usd_intensity, timecode);
  usd_light_api.CreateColorAttr().Set(pxr::GfVec3f(light->r, light->g, light->b), timecode);
  usd_light_api.CreateSpecularAttr().Set(light->spec_fac, timecode);
  usd_light_api.CreateNormalizeAttr().Set(true, timecode);
}

}  // namespace blender::io::usd
