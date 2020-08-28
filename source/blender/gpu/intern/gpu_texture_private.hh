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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_assert.h"

namespace blender {
namespace gpu {

class Texture {
 public:
  /** TODO(fclem): make it a non-static function. */
  static GPUAttachmentType attachment_type(GPUTexture *tex, int slot)
  {
    switch (GPU_texture_format(tex)) {
      case GPU_DEPTH_COMPONENT32F:
      case GPU_DEPTH_COMPONENT24:
      case GPU_DEPTH_COMPONENT16:
        BLI_assert(slot == 0);
        return GPU_FB_DEPTH_ATTACHMENT;
      case GPU_DEPTH24_STENCIL8:
      case GPU_DEPTH32F_STENCIL8:
        BLI_assert(slot == 0);
        return GPU_FB_DEPTH_STENCIL_ATTACHMENT;
      default:
        return static_cast<GPUAttachmentType>(GPU_FB_COLOR_ATTACHMENT0 + slot);
    }
  }
};

}  // namespace gpu
}  // namespace blender
