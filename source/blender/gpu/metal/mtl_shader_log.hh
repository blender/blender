/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_private.hh"

namespace blender::gpu {

class MTLLogParser : public GPULogParser {
 public:
  const char *parse_line(const char *source_combined,
                         const char *log_line,
                         GPULogItem &log_item) override;

 protected:
  bool wrapper_error_ = false;
  bool parsed_error_ = false;

  const char *skip_name(const char *log_line);
  const char *skip_severity_keyword(const char *log_line, GPULogItem &log_item);
  const char *skip_line(const char *cursor) const;
};

}  // namespace blender::gpu
