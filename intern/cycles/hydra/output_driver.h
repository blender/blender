/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2022 NVIDIA Corporation
 * Copyright 2022 Blender Foundation */

#pragma once

#include "hydra/config.h"
#include "session/output_driver.h"

HDCYCLES_NAMESPACE_OPEN_SCOPE

class HdCyclesOutputDriver final : public CCL_NS::OutputDriver {
 public:
  HdCyclesOutputDriver(HdCyclesSession *renderParam);

 private:
  void write_render_tile(const Tile &tile) override;
  bool update_render_tile(const Tile &tile) override;

  HdCyclesSession *const _renderParam;
};

HDCYCLES_NAMESPACE_CLOSE_SCOPE
