/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_drawlist_private.hh"

namespace blender::gpu {

class VKDrawList : public DrawList {
 public:
  void append(GPUBatch *batch, int instance_first, int instance_count) override;
  void submit() override;
};

}  // namespace blender::gpu
