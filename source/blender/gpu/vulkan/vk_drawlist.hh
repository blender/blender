/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_drawlist_private.hh"

namespace blender::gpu {

class VKDrawList : public DrawList {
 public:
  void append(GPUBatch *batch, int i_first, int i_count) override;
  void submit() override;
};

}  // namespace blender::gpu
