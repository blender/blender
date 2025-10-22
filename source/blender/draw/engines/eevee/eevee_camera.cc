/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

#include "BLI_bounds.hh"
#include "BLI_rect.h"

#include "DRW_render.hh"

#include "DNA_camera_types.h"
#include "DNA_view3d_types.h"

#include "RE_engine.h"
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
    data.type = inst_.drw_view->is_persp() ? CAMERA_PERSP : CAMERA_ORTHO;
  }
  else {
    /* Light-probe baking. */
    data.type = CAMERA_PERSP;
  }

  float overscan = 0.0f;
  if ((inst_.scene->eevee.flag & SCE_EEVEE_OVERSCAN) && (inst_.drw_view || inst_.render)) {
    overscan = inst_.scene->eevee.overscan / 100.0f;
    if (inst_.drw_view && (inst_.rv3d->dist == 0.0f || v3d_camera_params_get().lens == 0.0f)) {
      /* In these cases we need to use the v3d winmat as-is. */
      overscan = 0.0f;
    }
  }
  overscan_changed_ = assign_if_different(overscan_, overscan);
  camera_changed_ = assign_if_different(last_camera_object_, inst_.camera_orig_object);
}

void Camera::sync()
{
  const Object *camera_eval = inst_.camera_eval_object;

  CameraData &data = data_;

  int2 display_extent = inst_.film.display_extent_get();
  int2 film_extent = inst_.film.film_extent_get();
  int2 film_offset = inst_.film.film_offset_get();
  /* Over-scan in film pixel. Not the same as `render_overscan_get`. */
  int film_overscan = Film::overscan_pixels_get(overscan_, film_extent);

  rcti film_rect;
  BLI_rcti_init(&film_rect,
                film_offset.x,
                film_offset.x + film_extent.x,
                film_offset.y,
                film_offset.y + film_extent.y);

  Bounds<float2> uv_region = {float2(0.0f), float2(display_extent)};
  if (inst_.drw_view) {
    float2 uv_scale = float4(inst_.rv3d->viewcamtexcofac).xy();
    float2 uv_bias = float4(inst_.rv3d->viewcamtexcofac).zw();
    /* UV region inside the display extent reference frame. */
    uv_region.min = (-uv_bias * float2(display_extent)) / uv_scale;
    uv_region.max = uv_region.min + (float2(display_extent) / uv_scale);
  }

  data.uv_scale = float2(film_extent + film_overscan * 2) / uv_region.size();
  data.uv_bias = (float2(film_offset - film_overscan) - uv_region.min) / uv_region.size();

  if (inst_.is_baking()) {
    /* Any view so that shadows and light culling works during irradiance bake. */
    draw::View &view = inst_.volume_probes.bake.view_z_;
    data.viewmat = view.viewmat();
    data.viewinv = view.viewinv();
    data.winmat = view.winmat();
    data.type = CAMERA_ORTHO;

    /* \note Follow camera parameters where distances are positive in front of the camera. */
    data.clip_near = -view.far_clip();
    data.clip_far = -view.near_clip();
    data.fisheye_fov = data.fisheye_lens = -1.0f;
    data.equirect_bias = float2(0.0f);
    data.equirect_scale = float2(0.0f);
    data.uv_scale = float2(1.0f);
    data.uv_bias = float2(0.0f);
  }
  else if (inst_.drw_view) {
    data.viewmat = inst_.drw_view->viewmat();
    data.viewinv = inst_.drw_view->viewinv();

    CameraParams params = v3d_camera_params_get();

    if (inst_.rv3d->dist > 0.0f && params.lens > 0.0f) {
      BKE_camera_params_compute_viewplane(&params, UNPACK2(display_extent), 1.0f, 1.0f);

      BLI_assert(BLI_rctf_size_x(&params.viewplane) > 0.0f);
      BLI_assert(BLI_rctf_size_y(&params.viewplane) > 0.0f);

      BKE_camera_params_crop_viewplane(&params.viewplane, UNPACK2(display_extent), &film_rect);

      RE_GetWindowMatrixWithOverscan(params.is_ortho,
                                     params.clip_start,
                                     params.clip_end,
                                     params.viewplane,
                                     overscan_,
                                     data.winmat.ptr());
    }
    else {
      /* Can happen for the case of XR or if `rv3d->dist == 0`.
       * In this case the produced winmat is degenerate. So just revert to the input matrix. */
      data.winmat = inst_.drw_view->winmat();
    }
  }
  else if (inst_.render) {
    const Render *re = inst_.render->re;

    RE_GetCameraWindow(inst_.render->re, camera_eval, data.winmat.ptr());

    RE_GetCameraModelMatrix(re, camera_eval, data.viewinv.ptr());
    data.viewmat = math::invert(data.viewinv);

    rctf viewplane = re->viewplane;
    BKE_camera_params_crop_viewplane(&viewplane, UNPACK2(display_extent), &film_rect);

    RE_GetWindowMatrixWithOverscan(this->is_orthographic(),
                                   re->clip_start,
                                   re->clip_end,
                                   viewplane,
                                   overscan_,
                                   data.winmat.ptr());
  }
  else {
    data.viewmat = float4x4::identity();
    data.viewinv = float4x4::identity();
    data.winmat = math::projection::perspective(-0.1f, 0.1f, -0.1f, 0.1f, 0.1f, 1.0f);
  }

  /* Compute a part of the frustrum planes. In some cases (#134320, #148258)
   * the window matrix becomes degenerate during render or draw_view.
   * Simply fall back to something we can render with. */
  float bottom = (-data.winmat[3][1] - 1.f) / data.winmat[1][1];
  if (std::isnan(bottom) || std::isinf(std::abs(bottom))) {
    data.winmat = math::projection::orthographic(0.01f, 0.01f, 0.01f, 0.01f, -1000.0f, +1000.0f);
  }

  data.wininv = math::invert(data.winmat);
  data.persmat = data.winmat * data.viewmat;
  data.persinv = math::invert(data.persmat);

  is_camera_object_ = false;
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
    is_camera_object_ = true;
  }
  else if (inst_.drw_view) {
    /* \note Follow camera parameters where distances are positive in front of the camera. */
    data.clip_near = -inst_.drw_view->near_clip();
    data.clip_far = -inst_.drw_view->far_clip();
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

CameraParams Camera::v3d_camera_params_get() const
{
  BLI_assert(inst_.drw_view);

  CameraParams params;
  BKE_camera_params_init(&params);

  if (inst_.rv3d->persp == RV3D_CAMOB && inst_.is_viewport_image_render) {
    /* We are rendering camera view, no need for pan/zoom params from viewport. */
    BKE_camera_params_from_object(&params, inst_.camera_eval_object);
  }
  else {
    BKE_camera_params_from_view3d(&params, inst_.depsgraph, inst_.v3d, inst_.rv3d);
  }

  return params;
}

/** \} */

}  // namespace blender::eevee
