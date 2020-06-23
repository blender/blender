/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */
#include "usd_writer_light.h"
#include "usd_hierarchy_iterator.h"

#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/sphereLight.h>

#include "BLI_assert.h"
#include "BLI_utildefines.h"

#include "DNA_light_types.h"
#include "DNA_object_types.h"

namespace blender {
namespace io {
namespace usd {

USDLightWriter::USDLightWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx)
{
}

bool USDLightWriter::is_supported(const HierarchyContext *context) const
{
  Light *light = static_cast<Light *>(context->object->data);
  return ELEM(light->type, LA_AREA, LA_LOCAL, LA_SUN);
}

void USDLightWriter::do_write(HierarchyContext &context)
{
  pxr::UsdStageRefPtr stage = usd_export_context_.stage;
  const pxr::SdfPath &usd_path = usd_export_context_.usd_path;
  pxr::UsdTimeCode timecode = get_export_time_code();

  Light *light = static_cast<Light *>(context.object->data);
  pxr::UsdLuxLight usd_light;

  switch (light->type) {
    case LA_AREA:
      switch (light->area_shape) {
        case LA_AREA_DISK:
        case LA_AREA_ELLIPSE: { /* An ellipse light will deteriorate into a disk light. */
          pxr::UsdLuxDiskLight disk_light = pxr::UsdLuxDiskLight::Define(stage, usd_path);
          disk_light.CreateRadiusAttr().Set(light->area_size, timecode);
          usd_light = disk_light;
          break;
        }
        case LA_AREA_RECT: {
          pxr::UsdLuxRectLight rect_light = pxr::UsdLuxRectLight::Define(stage, usd_path);
          rect_light.CreateWidthAttr().Set(light->area_size, timecode);
          rect_light.CreateHeightAttr().Set(light->area_sizey, timecode);
          usd_light = rect_light;
          break;
        }
        case LA_AREA_SQUARE: {
          pxr::UsdLuxRectLight rect_light = pxr::UsdLuxRectLight::Define(stage, usd_path);
          rect_light.CreateWidthAttr().Set(light->area_size, timecode);
          rect_light.CreateHeightAttr().Set(light->area_size, timecode);
          usd_light = rect_light;
          break;
        }
      }
      break;
    case LA_LOCAL: {
      pxr::UsdLuxSphereLight sphere_light = pxr::UsdLuxSphereLight::Define(stage, usd_path);
      sphere_light.CreateRadiusAttr().Set(light->area_size, timecode);
      usd_light = sphere_light;
      break;
    }
    case LA_SUN:
      usd_light = pxr::UsdLuxDistantLight::Define(stage, usd_path);
      break;
    default:
      BLI_assert(!"is_supported() returned true for unsupported light type");
  }

  /* Scale factor to get to somewhat-similar illumination. Since the USDViewer had similar
   * over-exposure as Blender Internal with the same values, this code applies the reverse of the
   * versioning code in light_emission_unify(). */
  float usd_intensity;
  if (light->type == LA_SUN) {
    /* Untested, as the Hydra GL viewport of USDViewer doesn't support distant lights. */
    usd_intensity = light->energy;
  }
  else {
    usd_intensity = light->energy / 100.f;
  }
  usd_light.CreateIntensityAttr().Set(usd_intensity, timecode);

  usd_light.CreateColorAttr().Set(pxr::GfVec3f(light->r, light->g, light->b), timecode);
  usd_light.CreateSpecularAttr().Set(light->spec_fac, timecode);
}

}  // namespace usd
}  // namespace io
}  // namespace blender
