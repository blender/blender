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
 * Adapted from the Blender Alembic importer implementation.
 *
 * Modifications Copyright (C) 2021 Tangent Animation.
 * All rights reserved.
 */

#include "usd_reader_camera.h"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"

#include "BKE_camera.h"
#include "BKE_object.h"

#include "BLI_math.h"

#include <pxr/pxr.h>
#include <pxr/usd/usdGeom/camera.h>

namespace blender::io::usd {

void USDCameraReader::create_object(Main *bmain, const double /* motionSampleTime */)
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

  bcam->lens = usd_cam.GetFocalLength();

  bcam->sensor_x = apperture_x;
  bcam->sensor_y = apperture_y;

  bcam->shiftx = h_film_offset / apperture_x;
  bcam->shifty = v_film_offset / apperture_y / film_aspect;

  pxr::GfRange1f usd_clip_range = usd_cam.GetClippingRange();
  bcam->clip_start = usd_clip_range.GetMin() * settings_->scale;
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
