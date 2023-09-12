/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup eevee
 */

#include "eevee_shader_shared.hh"

namespace blender::eevee {

class Instance;

inline float4x4 cubeface_mat(int face)
{
  switch (face) {
    default:
    case 0:
      /* Pos X */
      return float4x4({0.0f, 0.0f, -1.0f, 0.0f},
                      {0.0f, -1.0f, 0.0f, 0.0f},
                      {-1.0f, 0.0f, 0.0f, 0.0f},
                      {0.0f, 0.0f, 0.0f, 1.0f});
    case 1:
      /* Neg X */
      return float4x4({0.0f, 0.0f, 1.0f, 0.0f},
                      {0.0f, -1.0f, 0.0f, 0.0f},
                      {1.0f, 0.0f, 0.0f, 0.0f},
                      {0.0f, 0.0f, 0.0f, 1.0f});
    case 2:
      /* Pos Y */
      return float4x4({1.0f, 0.0f, 0.0f, 0.0f},
                      {0.0f, 0.0f, -1.0f, 0.0f},
                      {0.0f, 1.0f, 0.0f, 0.0f},
                      {0.0f, 0.0f, 0.0f, 1.0f});
    case 3:
      /* Neg Y */
      return float4x4({1.0f, 0.0f, 0.0f, 0.0f},
                      {0.0f, 0.0f, 1.0f, 0.0f},
                      {0.0f, -1.0f, 0.0f, 0.0f},
                      {0.0f, 0.0f, 0.0f, 1.0f});
    case 4:
      /* Pos Z */
      return float4x4({1.0f, 0.0f, 0.0f, 0.0f},
                      {0.0f, -1.0f, 0.0f, 0.0f},
                      {0.0f, 0.0f, -1.0f, 0.0f},
                      {0.0f, 0.0f, 0.0f, 1.0f});
    case 5:
      /* Neg Z */
      return float4x4({-1.0f, 0.0f, 0.0f, 0.0f},
                      {0.0f, -1.0f, 0.0f, 0.0f},
                      {0.0f, 0.0f, 1.0f, 0.0f},
                      {0.0f, 0.0f, 0.0f, 1.0f});
  }
}

inline void cubeface_winmat_get(float4x4 &winmat, float near, float far)
{
  /* Simple 90 degree FOV projection. */
  perspective_m4(winmat.ptr(), -near, near, -near, near, near, far);
}

/* -------------------------------------------------------------------- */
/** \name CameraData operators
 * \{ */

inline bool operator==(const CameraData &a, const CameraData &b)
{
  return compare_m4m4(a.persmat.ptr(), b.persmat.ptr(), FLT_MIN) && (a.uv_scale == b.uv_scale) &&
         (a.uv_bias == b.uv_bias) && (a.equirect_scale == b.equirect_scale) &&
         (a.equirect_bias == b.equirect_bias) && (a.fisheye_fov == b.fisheye_fov) &&
         (a.fisheye_lens == b.fisheye_lens) && (a.type == b.type);
}

inline bool operator!=(const CameraData &a, const CameraData &b)
{
  return !(a == b);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera
 * \{ */

/**
 * Point of view in the scene. Can be init from viewport or camera object.
 */
class Camera {
 private:
  Instance &inst_;

  CameraData &data_;

  struct {
    float3 center;
    float radius;
  } bound_sphere;

  float overscan_;
  bool overscan_changed_;

 public:
  Camera(Instance &inst, CameraData &data) : inst_(inst), data_(data){};
  ~Camera(){};

  void init();
  void sync();

  /**
   * Getters
   **/
  const CameraData &data_get() const
  {
    BLI_assert(data_.initialized);
    return data_;
  }
  bool is_panoramic() const
  {
    return eevee::is_panoramic(data_.type);
  }
  bool is_orthographic() const
  {
    return data_.type == CAMERA_ORTHO;
  }
  bool is_perspective() const
  {
    return data_.type == CAMERA_PERSP;
  }
  const float3 &position() const
  {
    return data_.viewinv.location();
  }
  const float3 &forward() const
  {
    return data_.viewinv.z_axis();
  }
  const float3 &bound_center() const
  {
    return bound_sphere.center;
  }
  const float &bound_radius() const
  {
    return bound_sphere.radius;
  }
  float overscan() const
  {
    return overscan_;
  }
  bool overscan_changed() const
  {
    return overscan_changed_;
  }

 private:
  void update_bounds();
};

/** \} */

}  // namespace blender::eevee
