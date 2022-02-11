/* SPDX-License-Identifier: GPL-2.0-or-later
 * Adapted from the Blender Alembic importer implementation.
 * Modifications Copyright 2021 Tangent Animation. All rights reserved. */

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
  Camera *bcam = (Camera *)object_->data;

  pxr::UsdGeomCamera cam_prim(prim_);

  if (!cam_prim) {
    return;
  }

  pxr::VtValue val;
  cam_prim.GetFocalLengthAttr().Get(&val, motionSampleTime);
  pxr::VtValue verApOffset;
  cam_prim.GetVerticalApertureOffsetAttr().Get(&verApOffset, motionSampleTime);
  pxr::VtValue horApOffset;
  cam_prim.GetHorizontalApertureOffsetAttr().Get(&horApOffset, motionSampleTime);
  pxr::VtValue clippingRangeVal;
  cam_prim.GetClippingRangeAttr().Get(&clippingRangeVal, motionSampleTime);
  pxr::VtValue focalDistanceVal;
  cam_prim.GetFocusDistanceAttr().Get(&focalDistanceVal, motionSampleTime);
  pxr::VtValue fstopVal;
  cam_prim.GetFStopAttr().Get(&fstopVal, motionSampleTime);
  pxr::VtValue projectionVal;
  cam_prim.GetProjectionAttr().Get(&projectionVal, motionSampleTime);
  pxr::VtValue verAp;
  cam_prim.GetVerticalApertureAttr().Get(&verAp, motionSampleTime);
  pxr::VtValue horAp;
  cam_prim.GetHorizontalApertureAttr().Get(&horAp, motionSampleTime);

  bcam->lens = val.Get<float>();
  /* TODO(@makowalski): support sensor size. */
#if 0
   bcam->sensor_x = 0.0f;
   bcam->sensor_y = 0.0f;
#endif
  bcam->shiftx = verApOffset.Get<float>();
  bcam->shifty = horApOffset.Get<float>();

  bcam->type = (projectionVal.Get<pxr::TfToken>().GetString() == "perspective") ? CAM_PERSP :
                                                                                  CAM_ORTHO;

  /* Calling UncheckedGet() to silence compiler warnings. */
  bcam->clip_start = max_ff(0.1f, clippingRangeVal.UncheckedGet<pxr::GfVec2f>()[0]);
  bcam->clip_end = clippingRangeVal.UncheckedGet<pxr::GfVec2f>()[1];

  bcam->dof.focus_distance = focalDistanceVal.Get<float>();
  bcam->dof.aperture_fstop = static_cast<float>(fstopVal.Get<float>());

  if (bcam->type == CAM_ORTHO) {
    bcam->ortho_scale = max_ff(verAp.Get<float>(), horAp.Get<float>());
  }

  USDXformReader::read_object_data(bmain, motionSampleTime);
}

}  // namespace blender::io::usd
