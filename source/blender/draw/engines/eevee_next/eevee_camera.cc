/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

#include <array>

#include "DRW_render.hh"

#include "DNA_camera_types.h"
#include "DNA_view3d_types.h"

#include "BKE_camera.h"

#include "DEG_depsgraph_query.hh"

#include "ED_view3d.hh"

#include "RE_pipeline.h"
#include "render_types.h"

#include "eevee_camera.hh"
#include "eevee_instance.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Camera
 * \{ */

void Camera::init()
{
  const Object *camera_eval = inst_.camera_eval_object;

  CameraData &data = data_;

  if (camera_eval && camera_eval->type == OB_CAMERA) {
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

  float overscan = 0.0f;
  if ((inst_.scene->eevee.flag & SCE_EEVEE_OVERSCAN) && (inst_.drw_view || inst_.render)) {
    overscan = inst_.scene->eevee.overscan / 100.0f;
  }
  overscan_changed_ = assign_if_different(overscan_, overscan);
}

void Camera::sync()
{
  const Object *camera_eval = inst_.camera_eval_object;

  CameraData &data = data_;

  float2 resolution = float2(inst_.film.display_extent_get());
  float2 overscan_margin = float2(overscan_ * math::max(UNPACK2(resolution)));
  float2 overscan_resolution = resolution + overscan_margin * 2.0f;
  float2 camera_min = overscan_margin;
  float2 camera_max = camera_min + resolution;

  if (inst_.drw_view) {
    /* Viewport camera view. */
    float2 camera_uv_scale = float2(inst_.rv3d->viewcamtexcofac);
    float2 camera_uv_bias = float2(inst_.rv3d->viewcamtexcofac + 2);
    float2 camera_region_min = (-camera_uv_bias * resolution) / camera_uv_scale;
    float2 camera_region_size = resolution / camera_uv_scale;
    camera_min = overscan_margin + camera_region_min;
    camera_max = camera_min + camera_region_size;
  }

  data.uv_scale = overscan_resolution / (camera_max - camera_min);
  data.uv_bias = -camera_min / (camera_max - camera_min);

  if (inst_.is_baking()) {
    /* Any view so that shadows and light culling works during irradiance bake. */
    draw::View &view = inst_.volume_probes.bake.view_z_;
    data.viewmat = view.viewmat();
    data.viewinv = view.viewinv();
    data.winmat = view.winmat();
    data.type = CAMERA_ORTHO;

    /* \note: Follow camera parameters where distances are positive in front of the camera. */
    data.clip_near = -view.far_clip();
    data.clip_far = -view.near_clip();
    data.fisheye_fov = data.fisheye_lens = -1.0f;
    data.equirect_bias = float2(0.0f);
    data.equirect_scale = float2(0.0f);
    data.uv_scale = float2(1.0f);
    data.uv_bias = float2(0.0f);
  }
  else if (inst_.drw_view) {
    DRW_view_viewmat_get(inst_.drw_view, data.viewmat.ptr(), false);
    DRW_view_viewmat_get(inst_.drw_view, data.viewinv.ptr(), true);
    if (overscan_ == 0.0f) {
      DRW_view_winmat_get(inst_.drw_view, data.winmat.ptr(), false);
    }
    else {
      rctf viewplane;
      float clip_start;
      float clip_end;
      bool is_ortho = ED_view3d_viewplane_get(inst_.depsgraph,
                                              inst_.v3d,
                                              inst_.rv3d,
                                              UNPACK2(inst_.film.display_extent_get()),
                                              &viewplane,
                                              &clip_start,
                                              &clip_end,
                                              nullptr);

      RE_GetWindowMatrixWithOverscan(
          is_ortho, clip_start, clip_end, viewplane, overscan_, data.winmat.ptr());
    }
  }
  else if (inst_.render) {
    RE_GetCameraModelMatrix(inst_.render->re, camera_eval, data.viewinv.ptr());
    data.viewmat = math::invert(data.viewinv);
    RE_GetCameraWindow(inst_.render->re, camera_eval, data.winmat.ptr());
    if (overscan_ != 0.0f) {
      RE_GetCameraWindowWithOverscan(inst_.render->re, overscan_, data.winmat.ptr());
    }
  }
  else {
    data.viewmat = float4x4::identity();
    data.viewinv = float4x4::identity();
    data.winmat = math::projection::perspective(-0.1f, 0.1f, -0.1f, 0.1f, 0.1f, 1.0f);
  }

  data.wininv = math::invert(data.winmat);
  data.persmat = data.winmat * data.viewmat;
  data.persinv = math::invert(data.persmat);

  if (camera_eval && camera_eval->type == OB_CAMERA) {
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
    /* \note: Follow camera parameters where distances are positive in front of the camera. */
    data.clip_near = -DRW_view_near_distance_get(inst_.drw_view);
    data.clip_far = -DRW_view_far_distance_get(inst_.drw_view);
    data.fisheye_fov = data.fisheye_lens = -1.0f;
    data.equirect_bias = float2(0.0f);
    data.equirect_scale = float2(0.0f);
  }

  data_.initialized = true;

  update_bounds();
}

void Camera::update_bounds()
{
  float left, right, bottom, top, near, far;
  projmat_dimensions(data_.winmat.ptr(), &left, &right, &bottom, &top, &near, &far);

  BoundBox bbox;
  bbox.vec[0][2] = bbox.vec[3][2] = bbox.vec[7][2] = bbox.vec[4][2] = -near;
  bbox.vec[0][0] = bbox.vec[3][0] = left;
  bbox.vec[4][0] = bbox.vec[7][0] = right;
  bbox.vec[0][1] = bbox.vec[4][1] = bottom;
  bbox.vec[7][1] = bbox.vec[3][1] = top;

  /* Get the coordinates of the far plane. */
  if (!this->is_orthographic()) {
    float sca_far = far / near;
    left *= sca_far;
    right *= sca_far;
    bottom *= sca_far;
    top *= sca_far;
  }

  bbox.vec[1][2] = bbox.vec[2][2] = bbox.vec[6][2] = bbox.vec[5][2] = -far;
  bbox.vec[1][0] = bbox.vec[2][0] = left;
  bbox.vec[6][0] = bbox.vec[5][0] = right;
  bbox.vec[1][1] = bbox.vec[5][1] = bottom;
  bbox.vec[2][1] = bbox.vec[6][1] = top;

  bound_sphere.center = {0.0f, 0.0f, 0.0f};
  bound_sphere.radius = 0.0f;

  for (auto i : IndexRange(8)) {
    bound_sphere.center += float3(bbox.vec[i]);
  }
  bound_sphere.center /= 8.0f;
  for (auto i : IndexRange(8)) {
    float dist_sqr = math::distance_squared(bound_sphere.center, float3(bbox.vec[i]));
    bound_sphere.radius = max_ff(bound_sphere.radius, dist_sqr);
  }
  bound_sphere.radius = sqrtf(bound_sphere.radius);

  /* Transform into world space. */
  bound_sphere.center = math::transform_point(data_.viewinv, bound_sphere.center);

  /* Compute diagonal length. */
  float2 p0 = float2(bbox.vec[0]) / (this->is_perspective() ? bbox.vec[0][2] : 1.0f);
  float2 p1 = float2(bbox.vec[7]) / (this->is_perspective() ? bbox.vec[7][2] : 1.0f);
  data_.screen_diagonal_length = math::distance(p0, p1);
}

/** \} */

}  // namespace blender::eevee
