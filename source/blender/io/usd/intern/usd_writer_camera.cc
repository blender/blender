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
#include "usd_writer_camera.h"
#include "usd_hierarchy_iterator.h"

#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/tokens.h>

#include "BKE_camera.h"
#include "BLI_assert.h"

#include "DNA_camera_types.h"
#include "DNA_scene_types.h"

namespace blender {
namespace io {
namespace usd {

USDCameraWriter::USDCameraWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx)
{
}

bool USDCameraWriter::is_supported(const HierarchyContext *context) const
{
  Camera *camera = static_cast<Camera *>(context->object->data);
  return camera->type == CAM_PERSP;
}

static void camera_sensor_size_for_render(const Camera *camera,
                                          const struct RenderData *rd,
                                          float *r_sensor_x,
                                          float *r_sensor_y)
{
  /* Compute the final image size in pixels. */
  float sizex = rd->xsch * rd->xasp;
  float sizey = rd->ysch * rd->yasp;

  int sensor_fit = BKE_camera_sensor_fit(camera->sensor_fit, sizex, sizey);

  switch (sensor_fit) {
    case CAMERA_SENSOR_FIT_HOR:
      *r_sensor_x = camera->sensor_x;
      *r_sensor_y = camera->sensor_x * sizey / sizex;
      break;
    case CAMERA_SENSOR_FIT_VERT:
      *r_sensor_x = camera->sensor_y * sizex / sizey;
      *r_sensor_y = camera->sensor_y;
      break;
    case CAMERA_SENSOR_FIT_AUTO:
      BLI_assert(!"Camera fit should be either horizontal or vertical");
      break;
  }
}

void USDCameraWriter::do_write(HierarchyContext &context)
{
  pxr::UsdTimeCode timecode = get_export_time_code();
  pxr::UsdGeomCamera usd_camera = pxr::UsdGeomCamera::Define(usd_export_context_.stage,
                                                             usd_export_context_.usd_path);

  Camera *camera = static_cast<Camera *>(context.object->data);
  Scene *scene = DEG_get_evaluated_scene(usd_export_context_.depsgraph);

  usd_camera.CreateProjectionAttr().Set(pxr::UsdGeomTokens->perspective);

  /* USD stores the focal length in "millimeters or tenths of world units", because at some point
   * they decided world units might be centimeters. Quite confusing, as the USD Viewer shows the
   * correct FoV when we write millimeters and not "tenths of world units".
   */
  usd_camera.CreateFocalLengthAttr().Set(camera->lens, timecode);

  float aperture_x, aperture_y;
  camera_sensor_size_for_render(camera, &scene->r, &aperture_x, &aperture_y);

  float film_aspect = aperture_x / aperture_y;
  usd_camera.CreateHorizontalApertureAttr().Set(aperture_x, timecode);
  usd_camera.CreateVerticalApertureAttr().Set(aperture_y, timecode);
  usd_camera.CreateHorizontalApertureOffsetAttr().Set(aperture_x * camera->shiftx, timecode);
  usd_camera.CreateVerticalApertureOffsetAttr().Set(aperture_y * camera->shifty * film_aspect,
                                                    timecode);

  usd_camera.CreateClippingRangeAttr().Set(
      pxr::VtValue(pxr::GfVec2f(camera->clip_start, camera->clip_end)), timecode);

  // Write DoF-related attributes.
  if (camera->dof.flag & CAM_DOF_ENABLED) {
    usd_camera.CreateFStopAttr().Set(camera->dof.aperture_fstop, timecode);

    float focus_distance = scene->unit.scale_length *
                           BKE_camera_object_dof_distance(context.object);
    usd_camera.CreateFocusDistanceAttr().Set(focus_distance, timecode);
  }
}

}  // namespace usd
}  // namespace io
}  // namespace blender
