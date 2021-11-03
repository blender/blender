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

#include "session/output_driver.h"

#include "util/function.h"
#include "util/image.h"
#include "util/string.h"
#include "util/unique_ptr.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class OIIOOutputDriver : public OutputDriver {
 public:
  typedef function<void(const string &)> LogFunction;

  OIIOOutputDriver(const string_view filepath, const string_view pass, LogFunction log);
  virtual ~OIIOOutputDriver();

  void write_render_tile(const Tile &tile) override;

 protected:
  string filepath_;
  string pass_;
  LogFunction log_;
};

CCL_NAMESPACE_END
