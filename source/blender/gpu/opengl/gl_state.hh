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

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "gpu_state_private.hh"

#include "glew-mx.h"

namespace blender {
namespace gpu {

class GLStateManager : public GPUStateManager {
 private:
  /** Current state of the GL implementation. Avoids resetting the whole state for every change. */
  GPUState current_;
  GPUStateMutable current_mutable_;

 public:
  GLStateManager();

  void set_state(const GPUState &state) override;
  void set_mutable_state(const GPUStateMutable &state) override;

 private:
  static void set_write_mask(const eGPUWriteMask value);
  static void set_depth_test(const eGPUDepthTest value);
  static void set_stencil_test(const eGPUStencilTest test, const eGPUStencilOp operation);
  static void set_stencil_mask(const eGPUStencilTest test, const GPUStateMutable state);
  static void set_clip_distances(const int new_dist_len, const int old_dist_len);
  static void set_logic_op(const bool enable);
  static void set_facing(const bool invert);
  static void set_backface_culling(const eGPUFaceCullTest test);
  static void set_provoking_vert(const eGPUProvokingVertex vert);
  static void set_shadow_bias(const bool enable);
  static void set_blend(const eGPUBlend value);

  MEM_CXX_CLASS_ALLOC_FUNCS("GLStateManager")
};

}  // namespace gpu
}  // namespace blender
