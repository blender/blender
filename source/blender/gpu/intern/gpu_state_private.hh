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

#include <cstring>

namespace blender {
namespace gpu {

/* Ecapsulate all pipeline state that we need to track.
 * Try to keep small to reduce validation time. */
union GPUState {
  struct {
    eGPUWriteMask write_mask : 13;
    eGPUBlend blend : 4;
    eGPUFaceCullTest culling_test : 2;
    eGPUDepthTest depth_test : 3;
    eGPUStencilTest stencil_test : 3;
    eGPUStencilOp stencil_op : 3;
    eGPUProvokingVertex provoking_vert : 1;
    /** Enable bits. */
    uint32_t logic_op_xor : 1;
    uint32_t invert_facing : 1;
    uint32_t shadow_bias : 1;
    /** Number of clip distances enabled. */
    /* TODO(fclem) This should be a shader property. */
    uint32_t clip_distances : 3;
    /* TODO(fclem) remove, old opengl features. */
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
    /** TODO put inside GPUFramebuffer. */
    /** Offset + Extent of the drawable region inside the framebuffer. */
    int viewport_rect[4];
    /** Offset + Extent of the scissor region inside the framebuffer. */
    int scissor_rect[4];
    /** TODO remove */
    float depth_range[2];
    /** TODO remove, use explicit clear calls. */
    float clear_color[4];
    float clear_depth;
    /** Negative if using program point size. */
    /* TODO(fclem) should be passed as uniform to all shaders. */
    float point_size;
    /** Not supported on every platform. Prefer using wideline shader. */
    float line_width;
    /** Mutable stencil states. */
    uint8_t stencil_write_mask;
    uint8_t stencil_compare_mask;
    uint8_t stencil_reference;
    uint8_t _pad0;
    /* IMPORTANT: ensure x64 stuct alignment. */
  };
  /* Here to allow fast bitwise ops. */
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

class GPUStateManager {
 public:
  GPUState state;
  GPUStateMutable mutable_state;

 public:
  GPUStateManager();
  virtual ~GPUStateManager(){};

  virtual void set_state(const GPUState &state) = 0;
  virtual void set_mutable_state(const GPUStateMutable &state) = 0;

  inline void apply_state(void)
  {
    this->set_state(this->state);
    this->set_mutable_state(this->mutable_state);
  };
};

}  // namespace gpu
}  // namespace blender
