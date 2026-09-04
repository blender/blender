/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

#include "BKE_camera.h"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BLI_enum_flags.hh"
#include "BLI_math_base.hh"
#include "BLI_math_matrix.hh"
#include "BLI_rect.hh"

#include "DRW_render.hh"

#include "DNA_camera_types.h"
#include "DNA_view3d_types.h"

#include "RE_engine.h"
#include "RE_pipeline.h"
#include "render_types.h"

#include "eevee_camera.hh"
#include "eevee_instance.hh"

namespace blender::eevee {

/** Bitmask identifying which of the 6 cubemap faces (in `cubeface_mat()` order) are needed to
 * cover a given panoramic projection. `FRONT`/`BACK` alias the `NEG_Z`/`POS_Z` faces since those
 * are the primary and opposite view directions for all panoramic projections. */
enum class PanoramicViewBits : uint32_t {
  POS_X = 1u << 0u,
  NEG_X = 1u << 1u,
  POS_Y = 1u << 2u,
  NEG_Y = 1u << 3u,
  BACK = 1u << 4u,  /* Pos Z. */
  FRONT = 1u << 5u, /* Neg Z. */
};
ENUM_OPERATORS(PanoramicViewBits)
constexpr PanoramicViewBits PANORAMIC_VIEW_ALL = PanoramicViewBits((1u << 6u) - 1u);
constexpr PanoramicViewBits PANORAMIC_VIEW_SIDES = PanoramicViewBits::POS_X |
                                                   PanoramicViewBits::NEG_X |
                                                   PanoramicViewBits::POS_Y |
                                                   PanoramicViewBits::NEG_Y;

static PanoramicViewBits panoramic_fisheye_view_mask_get(const CameraData &cam)
{
  constexpr float angle_epsilon = 1.0e-4f;
  const float half_fov = cam.fisheye_fov * 0.5f;
  PanoramicViewBits mask = PanoramicViewBits::FRONT;
  if (half_fov >= float(M_PI_4) - angle_epsilon) {
    mask |= PANORAMIC_VIEW_SIDES;
  }
  if (half_fov >= 3.0f * float(M_PI_4) - angle_epsilon) {
    mask |= PanoramicViewBits::BACK;
  }
  return mask;
}

/* True if `tan(theta) >= 1` somewhere in `[theta_lo, theta_hi]`. */
static bool tan_range_reaches_one(float theta_lo, float theta_hi)
{
  BLI_assert(theta_lo <= theta_hi);
  constexpr float inv_period = 1.0f / (2.0f * float(M_PI_2));
  float min_k = ceilf((theta_lo - float(M_PI_2)) * inv_period);
  float max_k = floorf((theta_hi - float(M_PI_4)) * inv_period);
  return min_k <= max_k;
}

/* Same as `tan_range_reaches_one` but for `tan(theta) <= -1`. */
static bool tan_range_reaches_minus_one(float theta_lo, float theta_hi)
{
  return tan_range_reaches_one(-theta_hi, -theta_lo);
}

/* Shifting or zooming push the sampled UV out of the front face so we need to include the faces
 * it spills into. Note that `tan` is cyclical so a wide shift wraps around and needs the
 * opposite neighbor face */
static PanoramicViewBits panoramic_equiangular_cubemap_face_view_mask_get(const CameraData &cam)
{
  const float2 lo = math::min(cam.uv_bias, cam.uv_bias + cam.uv_scale);
  const float2 hi = math::max(cam.uv_bias, cam.uv_bias + cam.uv_scale);

  /* see `equiangular_cubemap_face_to_direction` in eevee_camera_lib.bsl.hh */
  const float alpha_lo = (0.5f - hi.x) * float(M_PI_2);
  const float alpha_hi = (0.5f - lo.x) * float(M_PI_2);
  const float beta_lo = (lo.y - 0.5f) * float(M_PI_2);
  const float beta_hi = (hi.y - 0.5f) * float(M_PI_2);

  PanoramicViewBits mask = PanoramicViewBits::FRONT;
  /* direction.x = -tan(alpha). */
  if (tan_range_reaches_one(alpha_lo, alpha_hi)) {
    mask |= PanoramicViewBits::NEG_X;
  }
  if (tan_range_reaches_minus_one(alpha_lo, alpha_hi)) {
    mask |= PanoramicViewBits::POS_X;
  }
  /* direction.y = tan(beta). */
  if (tan_range_reaches_one(beta_lo, beta_hi)) {
    mask |= PanoramicViewBits::POS_Y;
  }
  if (tan_range_reaches_minus_one(beta_lo, beta_hi)) {
    mask |= PanoramicViewBits::NEG_Y;
  }
  return mask;
}

static PanoramicViewBits panoramic_view_mask_get(const CameraData &cam)
{
  switch (cam.type) {
    case CAMERA_PANO_EQUIANGULAR_CUBEMAP_FACE:
      return panoramic_equiangular_cubemap_face_view_mask_get(cam);
    case CAMERA_PANO_EQUIDISTANT:
    case CAMERA_PANO_EQUISOLID:
    case CAMERA_PANO_FISHEYE_LENS_POLYNOMIAL:
      return panoramic_fisheye_view_mask_get(cam);
    case CAMERA_PANO_EQUIRECT:
    case CAMERA_PANO_CENTRAL_CYLINDRICAL:
    case CAMERA_PANO_MIRROR:
      /* TODO(L3GiaBao): Can exclude faces when longitude/latitude (or u/v for central cylindrical)
       * is restricted. */
      return PANORAMIC_VIEW_ALL;
    default:
      return PanoramicViewBits::FRONT;
  }
}

/* -------------------------------------------------------------------- */
/** \name Camera
 * \{ */

void Camera::init()
{
  const Object *camera_eval = inst_.camera_eval_object;

  CameraData &data = data_;

  if (camera_eval && camera_eval->type == OB_CAMERA) {
    const blender::Camera *cam = reinterpret_cast<const blender::Camera *>(camera_eval->data);
    switch (cam->type) {
      default:
      case CAM_PERSP:
        data.type = CAMERA_PERSP;
        break;
      case CAM_ORTHO:
        data.type = CAMERA_ORTHO;
        break;
      case CAM_PANO: {
        switch (cam->panorama_type) {
          default:
          case CAM_PANORAMA_EQUIRECTANGULAR:
            data.type = CAMERA_PANO_EQUIRECT;
            break;
          case CAM_PANORAMA_EQUIANGULAR_CUBEMAP_FACE:
            data.type = CAMERA_PANO_EQUIANGULAR_CUBEMAP_FACE;
            break;
          case CAM_PANORAMA_FISHEYE_EQUIDISTANT:
            data.type = CAMERA_PANO_EQUIDISTANT;
            break;
          case CAM_PANORAMA_FISHEYE_EQUISOLID:
            data.type = CAMERA_PANO_EQUISOLID;
            break;
          case CAM_PANORAMA_FISHEYE_LENS_POLYNOMIAL:
            data.type = CAMERA_PANO_FISHEYE_LENS_POLYNOMIAL;
            break;
          case CAM_PANORAMA_CENTRAL_CYLINDRICAL:
            data.type = CAMERA_PANO_CENTRAL_CYLINDRICAL;
            break;
          case CAM_PANORAMA_MIRRORBALL:
            data.type = CAMERA_PANO_MIRROR;
            break;
        }
        break;
      }
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
  /* Over-scan in film pixel. Not the same as `render_overscan_get`.
   * Panoramic camera render through per-face subviews so this isn't applicable */
  int film_overscan = this->is_panoramic() ? 0 : Film::overscan_pixels_get(overscan_, film_extent);

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
    data.fisheye_sensor = float2(1.0f);
    data.fisheye_polynomial_bias = 0.0f;
    data.fisheye_polynomial_coefficients = float4(0.0f);
    data.central_cylindrical_range = float4(0.0f);
    data.equirect_bias = float2(0.0f);
    data.equirect_scale = float2(0.0f);
    data.uv_scale = float2(1.0f);
    data.uv_bias = float2(0.0f);
  }
  else if (inst_.drw_view) {
    data.viewmat = inst_.drw_view->viewmat();
    data.viewinv = inst_.drw_view->viewinv();
    data.winmat = inst_.drw_view->winmat();

    if (film_offset != int2(0) || film_extent != display_extent) {
      data.winmat = projection_crop_matrix(film_offset, film_extent, display_extent) * data.winmat;
    }

    if (overscan_ != 0.0f) {
      data.winmat = projection_overscan_matrix(film_extent, int2(film_overscan)) * data.winmat;
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

  data.wininv = math::invert(data.winmat);
  data.persmat = data.winmat * data.viewmat;
  data.persinv = math::invert(data.persmat);

  is_camera_object_ = false;
  if (camera_eval && camera_eval->type == OB_CAMERA) {
    const blender::Camera *cam = reinterpret_cast<const blender::Camera *>(camera_eval->data);
    data.clip_near = cam->clip_start;
    data.clip_far = cam->clip_end;
    if (this->is_panoramic()) {
      /* Panoramic projection doesn't use winmat so offset the shared uv_bias instead. */
      data.uv_bias += float2(cam->shiftx, cam->shifty);
    }
    if (data.type == CAMERA_PANO_EQUIRECT) {
      data.equirect_bias.x = -cam->longitude_min + M_PI_2;
      data.equirect_bias.y = -cam->latitude_min + M_PI_2;
      data.equirect_scale.x = cam->longitude_min - cam->longitude_max;
      data.equirect_scale.y = cam->latitude_min - cam->latitude_max;
      /* Combine with uv_scale/bias to avoid doing extra computation. */
      data.equirect_bias += data.uv_bias * data.equirect_scale;
      data.equirect_scale *= data.uv_scale;
      data.equirect_scale_inv = 1.0f / data.equirect_scale;
    }
    else {
      data.equirect_bias = float2(0.0f);
      data.equirect_scale = float2(0.0f);
      data.equirect_scale_inv = float2(0.0f);
    }
    data.fisheye_fov = cam->fisheye_fov;
    data.fisheye_lens = cam->fisheye_lens;
    data.fisheye_polynomial_bias = cam->fisheye_polynomial_k0;
    data.fisheye_polynomial_coefficients = float4(cam->fisheye_polynomial_k1,
                                                  cam->fisheye_polynomial_k2,
                                                  cam->fisheye_polynomial_k3,
                                                  cam->fisheye_polynomial_k4);
    data.central_cylindrical_range = float4(
        -cam->central_cylindrical_range_u_min,
        -cam->central_cylindrical_range_u_max,
        cam->central_cylindrical_range_v_min / cam->central_cylindrical_radius,
        cam->central_cylindrical_range_v_max / cam->central_cylindrical_radius);
    int render_width, render_height;
    BKE_render_resolution(&inst_.scene->r, false, &render_width, &render_height);
    const float fit_xratio = float(render_width) * inst_.scene->r.xasp;
    const float fit_yratio = float(render_height) * inst_.scene->r.yasp;
    const int sensor_fit = BKE_camera_sensor_fit(cam->sensor_fit, fit_xratio, fit_yratio);
    if (sensor_fit == CAMERA_SENSOR_FIT_HOR) {
      data.fisheye_sensor.x = cam->sensor_x;
      data.fisheye_sensor.y = cam->sensor_x * fit_yratio / fit_xratio;
    }
    else {
      data.fisheye_sensor.x = cam->sensor_y * fit_xratio / fit_yratio;
      data.fisheye_sensor.y = cam->sensor_y;
    }
    is_camera_object_ = true;
  }
  else if (inst_.drw_view) {
    /* \note Follow camera parameters where distances are positive in front of the camera. */
    data.clip_near = -inst_.drw_view->near_clip();
    data.clip_far = -inst_.drw_view->far_clip();
    data.fisheye_fov = data.fisheye_lens = -1.0f;
    data.fisheye_sensor = float2(1.0f);
    data.fisheye_polynomial_bias = 0.0f;
    data.fisheye_polynomial_coefficients = float4(0.0f);
    data.central_cylindrical_range = float4(0.0f);
    data.equirect_bias = float2(0.0f);
    data.equirect_scale = float2(0.0f);
    data.equirect_scale_inv = float2(0.0f);
  }

  /* 5% over-scan for DoF (see eevee_view.cc) plus user config. */
  data.panoramic_view_overscan = this->is_panoramic() ? 1.05f + overscan_ : 1.0f;
  data.panoramic_view_mask = uint32_t(this->is_panoramic() ? panoramic_view_mask_get(data) :
                                                             PanoramicViewBits::FRONT);
  data_.initialized = true;

  update_bounds();
}

void Camera::update_bounds()
{
  if (this->is_panoramic()) {
    bound_sphere.center = data_.viewinv.location();
    bound_sphere.radius = data_.clip_far;
    /* Each cubemap face covers a fixed 90 degree FOV. At unit distance from the camera, a face's
     * corners are at (+-1, +-1), so the diagonal is the diagonal of that 2x2 square. */
    data_.screen_diagonal_length = 2.0f * float(M_SQRT2);
    return;
  }

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

float4x4 Camera::projection_crop_matrix(int2 film_offset, int2 film_extent, int2 display_extent)
{
  float2 uv_min = float2(film_offset) / float2(display_extent);
  float2 uv_max = float2(film_offset + film_extent) / float2(display_extent);

  float2 ndc_min = uv_min * 2.0f - 1.0f;
  float2 ndc_max = uv_max * 2.0f - 1.0f;

  float2 ndc_size = ndc_max - ndc_min;
  float2 ndc_center = (ndc_min + ndc_max) * 0.5f;

  float2 scale = 2.0f / ndc_size;
  float2 offset = -ndc_center * scale;

  float4x4 crop_matrix = float4x4::identity();
  crop_matrix[0][0] = scale.x;
  crop_matrix[1][1] = scale.y;
  crop_matrix[3][0] = offset.x;
  crop_matrix[3][1] = offset.y;

  return crop_matrix;
}

float4x4 Camera::projection_overscan_matrix(int2 film_extent, int2 film_overscan)
{
  float2 overscan_scale = float2(film_extent) / float2(film_extent + film_overscan * 2);

  float4x4 overscan_matrix = float4x4::identity();
  overscan_matrix[0][0] = overscan_scale.x;
  overscan_matrix[1][1] = overscan_scale.y;

  return overscan_matrix;
}

/** \} */

}  // namespace blender::eevee
