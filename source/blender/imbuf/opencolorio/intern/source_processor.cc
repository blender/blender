/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "source_processor.hh"

namespace blender::ocio {

void source_comment_out_uniforms(std::string &source)
{
  size_t index = 0;
  while (true) {
    index = source.find("uniform ", index);
    if (index == -1) {
      break;
    }
    source.replace(index, 2, "//");
    index += 2;
  }
}

}  // namespace blender::ocio
