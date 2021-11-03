/*
 * Copyright 2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "RNA_blender_cpp.h"

#include "session/output_driver.h"

CCL_NAMESPACE_BEGIN

class BlenderOutputDriver : public OutputDriver {
 public:
  explicit BlenderOutputDriver(BL::RenderEngine &b_engine);
  ~BlenderOutputDriver();

  virtual void write_render_tile(const Tile &tile) override;
  virtual bool update_render_tile(const Tile &tile) override;
  virtual bool read_render_tile(const Tile &tile) override;

 protected:
  BL::RenderEngine b_engine_;
};

CCL_NAMESPACE_END
