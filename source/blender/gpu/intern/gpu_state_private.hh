/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_utildefines.h"

#include "GPU_state.hh"

#include "gpu_texture_private.hh"

#include <cstring>

namespace blender::gpu {

/* Encapsulate all pipeline state that we need to track.
 * Try to keep small to reduce validation time. */
union GPUState {
  struct {
    /** GPUWriteMask */
    uint32_t write_mask : 13;
    /** GPUBlend */
    uint32_t blend : 4;
    /** GPUFaceCullTest */
    uint32_t culling_test : 2;
    /** GPUDepthTest */
    uint32_t depth_test : 3;
    /** GPUStencilTest */
    uint32_t stencil_test : 3;
    /** GPUStencilOp */
    uint32_t stencil_op : 3;
    /** GPUProvokingVertex */
    uint32_t provoking_vert : 1;
    /** Enable bits. */
    uint32_t logic_op_xor : 1;
    uint32_t invert_facing : 1;
    uint32_t shadow_bias : 1;
    /** Clip range of 0..1 on OpenGL. */
    uint32_t clip_control : 1;
    /** Number of clip distances enabled. */
    /* TODO(fclem): This should be a shader property. */
    uint32_t clip_distances : 3;
    /* TODO(fclem): remove, old opengl features. */
    uint32_t polygon_smooth : 1;
    uint32_t line_smooth : 1;
  };
  /* Here to allow fast bit-wise ops. */
  uint64_t data;
};

BLI_STATIC_ASSERT(sizeof(GPUState) == sizeof(uint64_t), "GPUState is too big.");

inline bool operator==(const GPUState &a, const GPUState &b)
{
  return a.data == b.data;
}

inline bool operator!=(const GPUState &a, const GPUState &b)
{
  return !(a == b);
}

inline GPUState operator^(const GPUState &a, const GPUState &b)
{
  GPUState r;
  r.data = a.data ^ b.data;
  return r;
}

inline GPUState operator~(const GPUState &a)
{
  GPUState r;
  r.data = ~a.data;
  return r;
}

/* Mutable state that does not require pipeline change. */
union GPUStateMutable {
  struct {
    /* Viewport State */
    /** TODO: remove. */
    float depth_range[2];
    /** Positive if using program point size. */
    /* TODO(fclem): should be passed as uniform to all shaders. */
    float point_size;
    /** Not supported on every platform. Prefer using wide-line shader. */
    float line_width;
    /** Mutable stencil states. */
    uint8_t stencil_write_mask;
    uint8_t stencil_compare_mask;
    uint8_t stencil_reference;
    uint8_t _pad0[5];
    /* IMPORTANT: ensure x64 struct alignment. */
  };
  /* Here to allow fast bit-wise ops. */
  uint64_t data[3];
};

BLI_STATIC_ASSERT(sizeof(GPUStateMutable) == sizeof(GPUStateMutable::data),
                  "GPUStateMutable is too big.");

inline bool operator==(const GPUStateMutable &a, const GPUStateMutable &b)
{
  return a.data[0] == b.data[0] && a.data[1] == b.data[1] && a.data[2] == b.data[2];
}

inline bool operator!=(const GPUStateMutable &a, const GPUStateMutable &b)
{
  return !(a == b);
}

inline GPUStateMutable operator^(const GPUStateMutable &a, const GPUStateMutable &b)
{
  GPUStateMutable r;
  for (int i = 0; i < ARRAY_SIZE(a.data); i++) {
    r.data[i] = a.data[i] ^ b.data[i];
  }
  return r;
}

inline GPUStateMutable operator~(const GPUStateMutable &a)
{
  GPUStateMutable r;
  for (int i = 0; i < ARRAY_SIZE(a.data); i++) {
    r.data[i] = ~a.data[i];
  }
  return r;
}

/**
 * State manager keeping track of the draw state and applying it before drawing.
 * Base class which is then specialized for each implementation (GL, VK, ...).
 */
class StateManager {
 public:
  GPUState state;
  GPUStateMutable mutable_state;

  /* Formats of all image units. */
  std::array<TextureWriteFormat, GPU_MAX_IMAGE> image_formats;

  StateManager();
  virtual ~StateManager() = default;

  virtual void apply_state() = 0;
  virtual void force_state() = 0;

  virtual void issue_barrier(GPUBarrier barrier_bits) = 0;

  virtual void texture_bind(Texture *tex, GPUSamplerState sampler, int unit) = 0;
  virtual void texture_unbind(Texture *tex) = 0;
  virtual void texture_unbind_all() = 0;

  virtual void image_bind(Texture *tex, int unit) = 0;
  virtual void image_unbind(Texture *tex) = 0;
  virtual void image_unbind_all() = 0;

  virtual void texture_unpack_row_length_set(uint len) = 0;
};

/**
 * GPUFence.
 */
class Fence {
 protected:
  bool signalled_ = false;

 public:
  Fence() = default;
  virtual ~Fence() = default;

  virtual void signal() = 0;
  virtual void wait() = 0;
};

/* Syntactic sugar. */
static inline GPUFence *wrap(Fence *fence)
{
  return reinterpret_cast<GPUFence *>(fence);
}
static inline Fence *unwrap(GPUFence *fence)
{
  return reinterpret_cast<Fence *>(fence);
}
static inline const Fence *unwrap(const GPUFence *fence)
{
  return reinterpret_cast<const Fence *>(fence);
}

}  // namespace blender::gpu
