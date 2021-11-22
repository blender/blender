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

#include "session/output_driver.h"

CCL_NAMESPACE_BEGIN

/* PathTraceTile
 *
 * Implementation of OutputDriver::Tile interface for path tracer. */

class PathTrace;

class PathTraceTile : public OutputDriver::Tile {
 public:
  PathTraceTile(PathTrace &path_trace);

  bool get_pass_pixels(const string_view pass_name, const int num_channels, float *pixels) const;
  bool set_pass_pixels(const string_view pass_name,
                       const int num_channels,
                       const float *pixels) const;

 private:
  PathTrace &path_trace_;
  bool copied_from_device_;
};

CCL_NAMESPACE_END
