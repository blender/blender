/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. */

#pragma once

/** \file
 * \ingroup eevee
 */

#include "eevee_shader_shared.hh"

namespace blender::eevee {

class Instance;

static const float cubeface_mat[6][4][4] = {
    /* Pos X */
    {{0.0f, 0.0f, -1.0f, 0.0f},
     {0.0f, -1.0f, 0.0f, 0.0f},
     {-1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
    /* Neg X */
    {{0.0f, 0.0f, 1.0f, 0.0f},
     {0.0f, -1.0f, 0.0f, 0.0f},
     {1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
    /* Pos Y */
    {{1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, -1.0f, 0.0f},
     {0.0f, 1.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
    /* Neg Y */
    {{1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 1.0f, 0.0f},
     {0.0f, -1.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
    /* Pos Z */
    {{1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, -1.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, -1.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
    /* Neg Z */
    {{-1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, -1.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 1.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
};

inline void cubeface_winmat_get(float4x4 &winmat, float near, float far)
{
  /* Simple 90° FOV projection. */
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
         (a.fisheye_lens == b.fisheye_lens) && (a.filter_size == b.filter_size) &&
         (a.type == b.type);
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

  /** Double buffered to detect changes and have history for re-projection. */
  SwapChain<CameraDataBuf, 2> data_;
  /** Detects wrong usage. */
  bool synced_ = false;

 public:
  Camera(Instance &inst) : inst_(inst){};
  ~Camera(){};

  void init();
  void sync();

  /**
   * Getters
   **/
  const CameraData &data_get() const
  {
    BLI_assert(synced_);
    return data_.current();
  }
  const GPUUniformBuf *ubo_get() const
  {
    return data_.current();
  }
  bool is_panoramic() const
  {
    return eevee::is_panoramic(data_.current().type);
  }
  bool is_orthographic() const
  {
    return data_.current().type == CAMERA_ORTHO;
  }
  const float3 &position() const
  {
    return *reinterpret_cast<const float3 *>(data_.current().viewinv[3]);
  }
  const float3 &forward() const
  {
    return *reinterpret_cast<const float3 *>(data_.current().viewinv[2]);
  }
};

/** \} */

}  // namespace blender::eevee
