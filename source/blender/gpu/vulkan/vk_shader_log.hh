/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

#include "gpu_shader_private.hh"

namespace blender::gpu {

class VKLogParser : public GPULogParser {
 public:
  const char *parse_line(const char *log_line, GPULogItem &log_item) override;

 protected:
  const char *skip_name(const char *log_line);
  const char *skip_severity_keyword(const char *log_line, GPULogItem &log_item);
};
}  // namespace blender::gpu
