/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_camera.hh"
#include "usd_attribute_utils.hh"
#include "usd_hierarchy_iterator.hh"

#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/tokens.h>

#include "BKE_camera.h"
#include "BLI_assert.h"

#include "DEG_depsgraph_query.hh"

#include "DNA_camera_types.h"
#include "DNA_scene_types.h"

namespace blender::io::usd {

USDCameraWriter::USDCameraWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx) {}

bool USDCameraWriter::is_supported(const HierarchyContext *context) const
{
  const Camera *camera = static_cast<const Camera *>(context->object->data);
  return camera->type == CAM_PERSP;
}

static void camera_sensor_size_for_render(const Camera *camera,
                                          const RenderData *rd,
                                          float *r_sensor,
                                          float *r_sensor_x,
                                          float *r_sensor_y)
{
  /* Compute the final image size in pixels. */
  float sizex = rd->xsch * rd->xasp;
  float sizey = rd->ysch * rd->yasp;

  int sensor_fit = BKE_camera_sensor_fit(camera->sensor_fit, sizex, sizey);
  float sensor_size = BKE_camera_sensor_size(
      camera->sensor_fit, camera->sensor_x, camera->sensor_y);
  *r_sensor = sensor_size;

  switch (sensor_fit) {
    case CAMERA_SENSOR_FIT_HOR:
      *r_sensor_x = sensor_size;
      *r_sensor_y = sensor_size * sizey / sizex;
      break;
    case CAMERA_SENSOR_FIT_VERT:
      *r_sensor_x = sensor_size * sizex / sizey;
      *r_sensor_y = sensor_size;
      break;
    case CAMERA_SENSOR_FIT_AUTO:
      BLI_assert_msg(0, "Camera fit should be either horizontal or vertical");
      break;
  }
}

void USDCameraWriter::do_write(HierarchyContext &context)
{
  const double meters_per_unit = get_meters_per_unit(usd_export_context_.export_params);
  const float unit_scale = float(1.0 / meters_per_unit);

  pxr::UsdTimeCode time = get_export_time_code();
  pxr::UsdGeomCamera usd_camera = pxr::UsdGeomCamera::Define(usd_export_context_.stage,
                                                             usd_export_context_.usd_path);

  const Camera *camera = static_cast<const Camera *>(context.object->data);
  const Scene *scene = DEG_get_evaluated_scene(usd_export_context_.depsgraph);

  usd_camera.CreateProjectionAttr().Set(pxr::UsdGeomTokens->perspective);

  /*
   * For USD, these camera properties are in tenths of a world unit.
   * https://graphics.pixar.com/usd/release/api/class_usd_geom_camera.html#UsdGeom_CameraUnits
   *
   * tenth_unit_to_meters  = stage_meters_per_unit / 10
   * tenth_unit_to_millimeters = 1000 * unit_to_tenth_unit
   *                           = 100 * stage_meters_per_unit
   */
  const float tenth_unit_to_mm = float(100.0 * meters_per_unit * scene->unit.scale_length);

  float sensor_size, aperture_x, aperture_y;
  camera_sensor_size_for_render(camera, &scene->r, &sensor_size, &aperture_x, &aperture_y);

  set_attribute(usd_camera.CreateFocalLengthAttr(pxr::VtValue(), true),
                camera->lens / tenth_unit_to_mm,
                time,
                usd_value_writer_);
  set_attribute(usd_camera.CreateHorizontalApertureAttr(pxr::VtValue(), true),
                aperture_x / tenth_unit_to_mm,
                time,
                usd_value_writer_);
  set_attribute(usd_camera.CreateVerticalApertureAttr(pxr::VtValue(), true),
                aperture_y / tenth_unit_to_mm,
                time,
                usd_value_writer_);
  set_attribute(usd_camera.CreateHorizontalApertureOffsetAttr(pxr::VtValue(), true),
                sensor_size * camera->shiftx / tenth_unit_to_mm,
                time,
                usd_value_writer_);
  set_attribute(usd_camera.CreateVerticalApertureOffsetAttr(pxr::VtValue(), true),
                sensor_size * camera->shifty / tenth_unit_to_mm,
                time,
                usd_value_writer_);
  set_attribute(usd_camera.CreateClippingRangeAttr(pxr::VtValue(), true),
                pxr::GfVec2f(camera->clip_start * unit_scale, camera->clip_end * unit_scale),
                time,
                usd_value_writer_);

  /* Write DoF-related attributes. */
  if (camera->dof.flag & CAM_DOF_ENABLED) {
    const float focus_distance = BKE_camera_object_dof_distance(context.object);
    set_attribute(usd_camera.CreateFStopAttr(pxr::VtValue(), true),
                  camera->dof.aperture_fstop,
                  time,
                  usd_value_writer_);
    set_attribute(usd_camera.CreateFocusDistanceAttr(pxr::VtValue(), true),
                  focus_distance * unit_scale,
                  time,
                  usd_value_writer_);
  }
  else {
    set_attribute(usd_camera.CreateFStopAttr(pxr::VtValue(), true), 0.0f, time, usd_value_writer_);
  }

  auto prim = usd_camera.GetPrim();
  add_to_prim_map(prim.GetPath(), &camera->id);
  write_id_properties(prim, camera->id, time);
}

}  // namespace blender::io::usd
