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
 * Copyright 2020, Blender Foundation.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_utildefines.h"

#include "GPU_state.h"

#include "gpu_texture_private.hh"

#include <cstring>

namespace blender {
namespace gpu {

/* Encapsulate all pipeline state that we need to track.
 * Try to keep small to reduce validation time. */
union GPUState {
  struct {
    /** eGPUWriteMask */
    uint32_t write_mask : 13;
    /** eGPUBlend */
    uint32_t blend : 4;
    /** eGPUFaceCullTest */
    uint32_t culling_test : 2;
    /** eGPUDepthTest */
    uint32_t depth_test : 3;
    /** eGPUStencilTest */
    uint32_t stencil_test : 3;
    /** eGPUStencilOp */
    uint32_t stencil_op : 3;
    /** eGPUProvokingVertex */
    uint32_t provoking_vert : 1;
    /** Enable bits. */
    uint32_t logic_op_xor : 1;
    uint32_t invert_facing : 1;
    uint32_t shadow_bias : 1;
    /** Number of clip distances enabled. */
    /* TODO(fclem): This should be a shader property. */
    uint32_t clip_distances : 3;
    /* TODO(fclem): remove, old opengl features. */
    uint32_t polygon_smooth : 1;
    uint32_t line_smooth : 1;
  };
  /* Here to allow fast bitwise ops. */
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
    /** TODO remove */
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
    uint8_t _pad0;
    /* IMPORTANT: ensure x64 struct alignment. */
  };
  /* Here to allow fast bit-wise ops. */
  uint64_t data[9];
};

BLI_STATIC_ASSERT(sizeof(GPUStateMutable) == sizeof(GPUStateMutable::data),
                  "GPUStateMutable is too big.");

inline bool operator==(const GPUStateMutable &a, const GPUStateMutable &b)
{
  return memcmp(&a, &b, sizeof(GPUStateMutable)) == 0;
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
  bool use_bgl = false;

 public:
  StateManager();
  virtual ~StateManager(){};

  virtual void apply_state(void) = 0;
  virtual void force_state(void) = 0;

  virtual void issue_barrier(eGPUBarrier barrier_bits) = 0;

  virtual void texture_bind(Texture *tex, eGPUSamplerState sampler, int unit) = 0;
  virtual void texture_unbind(Texture *tex) = 0;
  virtual void texture_unbind_all(void) = 0;

  virtual void image_bind(Texture *tex, int unit) = 0;
  virtual void image_unbind(Texture *tex) = 0;
  virtual void image_unbind_all(void) = 0;

  virtual void texture_unpack_row_length_set(uint len) = 0;
};

}  // namespace gpu
}  // namespace blender
