/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_light.hh"
#include "usd_hierarchy_iterator.hh"

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

bool USDLightWriter::is_supported(const HierarchyContext * /*context*/) const
{
  return true;
}

static void set_light_extents(const pxr::UsdPrim &prim, const pxr::UsdTimeCode time)
{
  if (auto boundable = pxr::UsdGeomBoundable(prim)) {
    pxr::VtArray<pxr::GfVec3f> extent;
    pxr::UsdGeomBoundable::ComputeExtentFromPlugins(boundable, time, &extent);
    boundable.CreateExtentAttr().Set(extent, time);
  }

  /* We're intentionally not setting an error on non-boundable lights,
   * because overly noisy errors are annoying. */
}

void USDLightWriter::do_write(HierarchyContext &context)
{
  pxr::UsdStageRefPtr stage = usd_export_context_.stage;
  const pxr::SdfPath &usd_path = usd_export_context_.usd_path;
  pxr::UsdTimeCode timecode = get_export_time_code();

  Light *light = static_cast<Light *>(context.object->data);
  pxr::UsdLuxLightAPI usd_light_api;

  switch (light->type) {
    case LA_AREA: {
      switch (light->area_shape) {
        case LA_AREA_RECT: {
          pxr::UsdLuxRectLight rect_light = pxr::UsdLuxRectLight::Define(stage, usd_path);
          rect_light.CreateWidthAttr().Set(light->area_size, timecode);
          rect_light.CreateHeightAttr().Set(light->area_sizey, timecode);
          usd_light_api = rect_light.LightAPI();
          break;
        }
        case LA_AREA_SQUARE: {
          pxr::UsdLuxRectLight rect_light = pxr::UsdLuxRectLight::Define(stage, usd_path);
          rect_light.CreateWidthAttr().Set(light->area_size, timecode);
          rect_light.CreateHeightAttr().Set(light->area_size, timecode);
          usd_light_api = rect_light.LightAPI();
          break;
        }
        case LA_AREA_DISK: {
          pxr::UsdLuxDiskLight disk_light = pxr::UsdLuxDiskLight::Define(stage, usd_path);
          disk_light.CreateRadiusAttr().Set(light->area_size / 2.0f, timecode);
          usd_light_api = disk_light.LightAPI();
          break;
        }
        case LA_AREA_ELLIPSE: {
          /* An ellipse light deteriorates into a disk light. */
          pxr::UsdLuxDiskLight disk_light = pxr::UsdLuxDiskLight::Define(stage, usd_path);
          disk_light.CreateRadiusAttr().Set((light->area_size + light->area_sizey) / 4.0f,
                                            timecode);
          usd_light_api = disk_light.LightAPI();
          break;
        }
      }
      break;
    }
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

      usd_light_api = sphere_light.LightAPI();
      break;
    }
    case LA_SUN: {
      pxr::UsdLuxDistantLight distant_light = pxr::UsdLuxDistantLight::Define(stage, usd_path);
      distant_light.CreateAngleAttr().Set(RAD2DEGF(light->sun_angle / 2.0f), timecode);
      usd_light_api = distant_light.LightAPI();
      break;
    }
    default:
      BLI_assert_unreachable();
      break;
  }

  float intensity;
  if (light->type == LA_SUN) {
    /* Unclear why, but approximately matches Karma. */
    intensity = light->energy / 4.0f;
  }
  else {
    /* Convert from radiant flux to intensity. */
    intensity = light->energy / M_PI;
  }

  usd_light_api.CreateIntensityAttr().Set(intensity, timecode);
  usd_light_api.CreateExposureAttr().Set(0.0f, timecode);
  usd_light_api.CreateColorAttr().Set(pxr::GfVec3f(light->r, light->g, light->b), timecode);
  usd_light_api.CreateDiffuseAttr().Set(light->diff_fac, timecode);
  usd_light_api.CreateSpecularAttr().Set(light->spec_fac, timecode);
  usd_light_api.CreateNormalizeAttr().Set(true, timecode);

  set_light_extents(usd_light_api.GetPrim(), timecode);
}

}  // namespace blender::io::usd
