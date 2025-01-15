/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "RNA_blender_cpp.hh"

#include "session/output_driver.h"

CCL_NAMESPACE_BEGIN

class BlenderOutputDriver : public OutputDriver {
 public:
  explicit BlenderOutputDriver(BL::RenderEngine &b_engine);
  ~BlenderOutputDriver() override;

  void write_render_tile(const Tile &tile) override;
  bool update_render_tile(const Tile &tile) override;
  bool read_render_tile(const Tile &tile) override;

 protected:
  BL::RenderEngine b_engine_;
};

CCL_NAMESPACE_END
