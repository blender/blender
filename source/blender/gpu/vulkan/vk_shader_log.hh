/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

#include "gpu_shader_private.hh"

namespace blender::gpu {

class VKLogParser : public GPULogParser {
 public:
  char *parse_line(char *log_line, GPULogItem &log_item) override;

 protected:
  char *skip_name(char *log_line);
  char *skip_severity_keyword(char *log_line, GPULogItem &log_item);
};
}  // namespace blender::gpu
