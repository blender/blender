/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Mimics old style opengl immediate mode drawing.
 */

#pragma once

#include "MEM_guardedalloc.h"
#include "gpu_immediate_private.hh"

#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

namespace blender::gpu {

class MTLImmediate : public Immediate {
 private:
  MTLContext *context_ = nullptr;
  MTLTemporaryBuffer current_allocation_;
  MTLPrimitiveTopologyClass metal_primitive_mode_;
  MTLPrimitiveType metal_primitive_type_;
  bool has_begun_ = false;

 public:
  MTLImmediate(MTLContext *ctx);
  ~MTLImmediate();

  uchar *begin() override;
  void end() override;
  bool imm_is_recording()
  {
    return has_begun_;
  }
};

}  // namespace blender::gpu
