/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 * SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Adapted from the Blender Alembic importer implementation. */

#include "usd_reader_camera.hh"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"

#include "BLI_math_base.h"

#include "BKE_camera.h"
#include "BKE_object.hh"

#include <pxr/pxr.h>
#include <pxr/usd/usdGeom/camera.h>

namespace blender::io::usd {

void USDCameraReader::create_object(Main *bmain, const double /*motionSampleTime*/)
{
  Camera *bcam = static_cast<Camera *>(BKE_camera_add(bmain, name_.c_str()));

  object_ = BKE_object_add_only_object(bmain, OB_CAMERA, name_.c_str());
  object_->data = bcam;
}

void USDCameraReader::read_object_data(Main *bmain, const double motionSampleTime)
{
  if (!object_->data) {
    return;
  }

  Camera *bcam = static_cast<Camera *>(object_->data);

  pxr::UsdGeomCamera cam_prim(prim_);

  if (!cam_prim) {
    return;
  }

  pxr::GfCamera usd_cam = cam_prim.GetCamera(motionSampleTime);

  const float apperture_x = usd_cam.GetHorizontalAperture();
  const float apperture_y = usd_cam.GetVerticalAperture();
  const float h_film_offset = usd_cam.GetHorizontalApertureOffset();
  const float v_film_offset = usd_cam.GetVerticalApertureOffset();
  const float film_aspect = apperture_x / apperture_y;

  bcam->type = usd_cam.GetProjection() == pxr::GfCamera::Perspective ? CAM_PERSP : CAM_ORTHO;

  /*
   * For USD, these camera properties are in tenths of a world unit.
   * https://graphics.pixar.com/usd/release/api/class_usd_geom_camera.html#UsdGeom_CameraUnits
   *
   * tenth_unit_to_meters  = stage_meters_per_unit / 10
   * tenth_unit_to_millimeters = 1000 * unit_to_tenth_unit
   *                           = 100 * stage_meters_per_unit
   */
  const double scale_to_mm = 100.0 * settings_->stage_meters_per_unit;

  bcam->lens = usd_cam.GetFocalLength() * scale_to_mm;

  bcam->sensor_x = apperture_x * scale_to_mm;
  bcam->sensor_y = apperture_y * scale_to_mm;

  bcam->shiftx = h_film_offset / apperture_x;
  bcam->shifty = v_film_offset / apperture_y / film_aspect;

  pxr::GfRange1f usd_clip_range = usd_cam.GetClippingRange();
  /* Clamp to 1e-6 matching range defined in RNA. */
  bcam->clip_start = max_ff(1e-6f, usd_clip_range.GetMin() * settings_->scale);
  bcam->clip_end = usd_clip_range.GetMax() * settings_->scale;

  bcam->dof.focus_distance = usd_cam.GetFocusDistance() * settings_->scale;
  bcam->dof.aperture_fstop = usd_cam.GetFStop();

  if (bcam->dof.focus_distance > 0.0f || bcam->dof.aperture_fstop > 0.0f) {
    bcam->dof.flag |= CAM_DOF_ENABLED;
  }

  if (bcam->type == CAM_ORTHO) {
    bcam->ortho_scale = max_ff(apperture_x, apperture_y);
  }

  USDXformReader::read_object_data(bmain, motionSampleTime);
}

}  // namespace blender::io::usd
