/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <functional>

#include "session/output_driver.h"

#include "util/image.h"
#include "util/string.h"
#include "util/unique_ptr.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class OIIOOutputDriver : public OutputDriver {
 public:
  using LogFunction = std::function<void(const string &)>;

  OIIOOutputDriver(const string_view filepath, const string_view pass, LogFunction log);
  virtual ~OIIOOutputDriver();

  void write_render_tile(const Tile &tile) override;

 protected:
  string filepath_;
  string pass_;
  LogFunction log_;
};

CCL_NAMESPACE_END
