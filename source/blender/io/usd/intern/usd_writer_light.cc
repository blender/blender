/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_writer_light.hh"
#include "usd_hierarchy_iterator.hh"
#include "usd_light_convert.h"
#include "usd_lux_api_wrapper.h"

#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/shapingAPI.h>
#include <pxr/usd/usdLux/sphereLight.h>

#include "BLI_assert.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"

#include "DNA_light_types.h"

#include "WM_api.hh" //XXX needed?

namespace blender::io::usd {

USDLightWriter::USDLightWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx) {}

bool USDLightWriter::is_supported(const HierarchyContext *context) const
{
  Light *light = static_cast<Light *>(context->object->data);
  return ELEM(light->type, LA_AREA, LA_LOCAL, LA_SUN, LA_SPOT);
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
  pxr::UsdTimeCode timecode = get_export_time_code();

  /* Need to account for the scene scale when converting to nits
   * or scaling the radius. */
  float world_scale = mat4_to_scale(context.matrix_world);

  float radius_scale = usd_export_context_.export_params.scale_light_radius ? 1.0f / world_scale :
                                                                              1.0f;

  Light *light = static_cast<Light *>(context.object->data);

  UsdLuxWrapper usd_light_api;

  switch (light->type) {
    case LA_AREA: {
      switch (light->area_shape) {
        case LA_AREA_DISK:
        case LA_AREA_ELLIPSE: { /* An ellipse light will deteriorate into a disk light. */
          pxr::UsdLuxDiskLight disk_light =
              (usd_export_context_.export_params.export_as_overs) ?
                  pxr::UsdLuxDiskLight(
                      usd_export_context_.stage->OverridePrim(usd_export_context_.usd_path)) :
                  pxr::UsdLuxDiskLight::Define(usd_export_context_.stage,
                                               usd_export_context_.usd_path);
          usd_light_api = UsdLuxWrapper(disk_light.GetPrim());
          usd_light_api.CreateRadiusAttr();

          if (light->type == LA_AREA_ELLIPSE) {
            usd_light_api.SetRadiusAttr((light->area_size + light->area_sizey) / 4.0f, timecode);
          }
          else {
            usd_light_api.SetRadiusAttr(light->area_size / 2.0f, timecode);
          }
          break;
        }
        case LA_AREA_RECT: {
          pxr::UsdLuxRectLight rect_light =
              (usd_export_context_.export_params.export_as_overs) ?
                  pxr::UsdLuxRectLight(
                      usd_export_context_.stage->OverridePrim(usd_export_context_.usd_path)) :
                  pxr::UsdLuxRectLight::Define(usd_export_context_.stage,
                                               usd_export_context_.usd_path);
          usd_light_api = UsdLuxWrapper(rect_light.GetPrim());
          usd_light_api.CreateWidthAttr();
          usd_light_api.SetWidthAttr(light->area_size, timecode);
          usd_light_api.CreateHeightAttr();
          usd_light_api.SetHeightAttr(light->area_sizey, timecode);

          break;
        }
        case LA_AREA_SQUARE: {
          pxr::UsdLuxRectLight rect_light =
              (usd_export_context_.export_params.export_as_overs) ?
                  pxr::UsdLuxRectLight(
                      usd_export_context_.stage->OverridePrim(usd_export_context_.usd_path)) :
                  pxr::UsdLuxRectLight::Define(usd_export_context_.stage,
                                               usd_export_context_.usd_path);
          usd_light_api = UsdLuxWrapper(rect_light.GetPrim());
          usd_light_api.CreateWidthAttr();
          usd_light_api.SetWidthAttr(light->area_size, timecode);
          usd_light_api.CreateHeightAttr();
          usd_light_api.SetHeightAttr(light->area_size, timecode);
          break;
        }
      }
      break;
    }
    case LA_LOCAL: {

      pxr::UsdLuxSphereLight sphere_light =
          (usd_export_context_.export_params.export_as_overs) ?
              pxr::UsdLuxSphereLight(
                  usd_export_context_.stage->OverridePrim(usd_export_context_.usd_path)) :
              pxr::UsdLuxSphereLight::Define(usd_export_context_.stage,
                                             usd_export_context_.usd_path);
      usd_light_api = UsdLuxWrapper(sphere_light.GetPrim());
      usd_light_api.CreateRadiusAttr();
      usd_light_api.SetRadiusAttr(light->radius * radius_scale, timecode);
      break;
    }
    case LA_SPOT: {
      pxr::UsdLuxSphereLight spot_light =
          (usd_export_context_.export_params.export_as_overs) ?
              pxr::UsdLuxSphereLight(
                  usd_export_context_.stage->OverridePrim(usd_export_context_.usd_path)) :
              pxr::UsdLuxSphereLight::Define(usd_export_context_.stage,
                                             usd_export_context_.usd_path);

      if (!spot_light.GetPrim().ApplyAPI<pxr::UsdLuxShapingAPI>()) {
        WM_reportf(RPT_WARNING,
                   "USD export: Couldn't apply UsdLuxShapingAPI to exported spot light %s",
                   usd_export_context_.usd_path.GetAsString().c_str());
      }

      usd_light_api = UsdLuxWrapper(spot_light.GetPrim());
      usd_light_api.CreateRadiusAttr();
      usd_light_api.SetRadiusAttr(light->radius * radius_scale, timecode);

      UsdShapingWrapper shapingAPI(spot_light.GetPrim());
      float angle = (light->spotsize * (180.0f / (float)M_PI)) /
                    2.0f;  // Blender angle seems to be half of what USD expectes it to be.
      shapingAPI.CreateShapingConeAngleAttr();
      shapingAPI.SetShapingConeAngleAttr(angle);
      shapingAPI.CreateShapingConeSoftnessAttr();
      shapingAPI.SetShapingConeSoftnessAttr(light->spotblend);

      spot_light.CreateTreatAsPointAttr(pxr::VtValue(true), true);

      break;
    }
    case LA_SUN: {
      pxr::UsdLuxDistantLight sun_light =
          (usd_export_context_.export_params.export_as_overs) ?
              pxr::UsdLuxDistantLight(
                  usd_export_context_.stage->OverridePrim(usd_export_context_.usd_path)) :
              pxr::UsdLuxDistantLight::Define(usd_export_context_.stage,
                                              usd_export_context_.usd_path);
      usd_light_api = UsdLuxWrapper(sun_light.GetPrim());
      usd_light_api.CreateAngleAttr();
      usd_light_api.SetAngleAttr(light->sun_angle * (180.0f / (float)M_PI), timecode);
      break;
    }
    default:
      BLI_assert_unreachable();
      break;
  }

  float usd_intensity = light->energy * usd_export_context_.export_params.light_intensity_scale;

  if (usd_export_context_.export_params.convert_light_to_nits) {
    usd_intensity /= nits_to_energy_scale_factor(light, world_scale, radius_scale);
  }
  else if (light->type == LA_SUN) {
    /* Unclear why, but approximately matches Karma. */
    usd_intensity /= 4.0f;
  }
  else {
    /* Convert from radiant flux to intensity. */
    usd_intensity /= M_PI;
  }

  usd_light_api.CreateIntensityAttr().Set(usd_intensity, timecode);
  usd_light_api.CreateExposureAttr().Set(0.0f, timecode);
  usd_light_api.CreateColorAttr().Set(pxr::GfVec3f(light->r, light->g, light->b), timecode);
  usd_light_api.CreateDiffuseAttr().Set(light->diff_fac, timecode);
  usd_light_api.CreateSpecularAttr().Set(light->spec_fac, timecode);

  if (!usd_export_context_.export_params.convert_light_to_nits) {
    usd_light_api.CreateNormalizeAttr().Set(true, timecode);
  }

  if (!hierarchy_iterator_->get_object_data_computed_name(context.object).empty()) {
    usd_light_api.GetPrim().SetDisplayName(static_cast<ID *>(context.object->data)->name + 2);
  }

  if (usd_export_context_.export_params.export_custom_properties && light) {
    auto prim = usd_light_api.GetPrim();
    write_id_properties(prim, light->id, timecode);
  }

  auto prim = usd_light_api.GetPrim();
  write_id_properties(prim, light->id, timecode);

  set_light_extents(usd_light_api.GetPrim(), timecode);
}

}  // namespace blender::io::usd
