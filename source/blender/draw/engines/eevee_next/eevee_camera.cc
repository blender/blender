/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 */

#include <array>

#include "DRW_render.h"

#include "DNA_camera_types.h"
#include "DNA_view3d_types.h"

#include "BKE_camera.h"
#include "DEG_depsgraph_query.h"
#include "RE_pipeline.h"

#include "eevee_camera.hh"
#include "eevee_instance.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Camera
 * \{ */

void Camera::init()
{
  const Object *camera_eval = inst_.camera_eval_object;
  synced_ = false;
  data_.swap();

  CameraData &data = data_.current();

  if (camera_eval) {
    const ::Camera *cam = reinterpret_cast<const ::Camera *>(camera_eval->data);
    switch (cam->type) {
      default:
      case CAM_PERSP:
        data.type = CAMERA_PERSP;
        break;
      case CAM_ORTHO:
        data.type = CAMERA_ORTHO;
        break;
#if 0 /* TODO(fclem): Make fisheye properties inside blender. */
      case CAM_PANO: {
        switch (cam->panorama_type) {
          default:
          case CAM_PANO_EQUIRECTANGULAR:
            data.type = CAMERA_PANO_EQUIRECT;
            break;
          case CAM_PANO_FISHEYE_EQUIDISTANT:
            data.type = CAMERA_PANO_EQUIDISTANT;
            break;
          case CAM_PANO_FISHEYE_EQUISOLID:
            data.type = CAMERA_PANO_EQUISOLID;
            break;
          case CAM_PANO_MIRRORBALL:
            data.type = CAMERA_PANO_MIRROR;
            break;
        }
      }
#endif
    }
  }
  else if (inst_.drw_view) {
    data.type = DRW_view_is_persp_get(inst_.drw_view) ? CAMERA_PERSP : CAMERA_ORTHO;
  }
  else {
    /* Light-probe baking. */
    data.type = CAMERA_PERSP;
  }
}

void Camera::sync()
{
  const Object *camera_eval = inst_.camera_eval_object;

  data_.swap();

  CameraData &data = data_.current();

  if (inst_.drw_view) {
    DRW_view_viewmat_get(inst_.drw_view, data.viewmat.ptr(), false);
    DRW_view_viewmat_get(inst_.drw_view, data.viewinv.ptr(), true);
    DRW_view_winmat_get(inst_.drw_view, data.winmat.ptr(), false);
    DRW_view_winmat_get(inst_.drw_view, data.wininv.ptr(), true);
    DRW_view_persmat_get(inst_.drw_view, data.persmat.ptr(), false);
    DRW_view_persmat_get(inst_.drw_view, data.persinv.ptr(), true);
    DRW_view_camtexco_get(inst_.drw_view, data.uv_scale);
  }
  else if (inst_.render) {
    /* TODO(@fclem): Over-scan. */
    // RE_GetCameraWindowWithOverscan(inst_.render->re, g_data->overscan, data.winmat);
    RE_GetCameraWindow(inst_.render->re, camera_eval, data.winmat.ptr());
    RE_GetCameraModelMatrix(inst_.render->re, camera_eval, data.viewinv.ptr());
    invert_m4_m4(data.viewmat.ptr(), data.viewinv.ptr());
    invert_m4_m4(data.wininv.ptr(), data.winmat.ptr());
    mul_m4_m4m4(data.persmat.ptr(), data.winmat.ptr(), data.viewmat.ptr());
    invert_m4_m4(data.persinv.ptr(), data.persmat.ptr());
    data.uv_scale = float2(1.0f);
    data.uv_bias = float2(0.0f);
  }
  else {
    data.viewmat = float4x4::identity();
    data.viewinv = float4x4::identity();
    perspective_m4(data.winmat.ptr(), -0.1f, 0.1f, -0.1f, 0.1f, 0.1f, 1.0f);
    data.wininv = data.winmat.inverted();
    data.persmat = data.winmat * data.viewmat;
    data.persinv = data.persmat.inverted();
  }

  if (camera_eval) {
    const ::Camera *cam = reinterpret_cast<const ::Camera *>(camera_eval->data);
    data.clip_near = cam->clip_start;
    data.clip_far = cam->clip_end;
#if 0 /* TODO(fclem): Make fisheye properties inside blender. */
    data.fisheye_fov = cam->fisheye_fov;
    data.fisheye_lens = cam->fisheye_lens;
    data.equirect_bias.x = -cam->longitude_min + M_PI_2;
    data.equirect_bias.y = -cam->latitude_min + M_PI_2;
    data.equirect_scale.x = cam->longitude_min - cam->longitude_max;
    data.equirect_scale.y = cam->latitude_min - cam->latitude_max;
    /* Combine with uv_scale/bias to avoid doing extra computation. */
    data.equirect_bias += data.uv_bias * data.equirect_scale;
    data.equirect_scale *= data.uv_scale;

    data.equirect_scale_inv = 1.0f / data.equirect_scale;
#else
    data.fisheye_fov = data.fisheye_lens = -1.0f;
    data.equirect_bias = float2(0.0f);
    data.equirect_scale = float2(0.0f);
#endif
  }
  else if (inst_.drw_view) {
    data.clip_near = DRW_view_near_distance_get(inst_.drw_view);
    data.clip_far = DRW_view_far_distance_get(inst_.drw_view);
    data.fisheye_fov = data.fisheye_lens = -1.0f;
    data.equirect_bias = float2(0.0f);
    data.equirect_scale = float2(0.0f);
  }

  data_.current().push_update();

  synced_ = true;

  /* Detect changes in parameters. */
  if (data_.current() != data_.previous()) {
    inst_.sampling.reset();
  }
}

/** \} */

}  // namespace blender::eevee
