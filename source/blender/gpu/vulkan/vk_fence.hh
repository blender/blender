/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_state_private.hh"

#include "vk_common.hh"

namespace blender::gpu {

class VKFence : public Fence {
 private:
  TimelineValue timeline_value_;

 public:
  void signal() override;
  void wait() override;
};

}  // namespace blender::gpu
