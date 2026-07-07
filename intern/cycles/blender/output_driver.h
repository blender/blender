/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "session/output_driver.h"

namespace blender {
struct RenderEngine;
}

CCL_NAMESPACE_BEGIN

class BlenderOutputDriver : public OutputDriver {
 public:
  explicit BlenderOutputDriver(blender::RenderEngine &b_engine);
  ~BlenderOutputDriver() override;

  void write_render_tile(const Tile &tile) override;
  bool update_render_tile(const Tile &tile) override;
  bool read_render_tile(const Tile &tile) override;

 protected:
  blender::RenderEngine &b_engine_;
};

CCL_NAMESPACE_END
