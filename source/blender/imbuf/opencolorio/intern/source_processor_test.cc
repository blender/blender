/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "source_processor.hh"

#include "testing/testing.h"

namespace blender::ocio {

TEST(ocio_source_processor, source_comment_out_uniforms)
{
  {
    std::string source = "int main() { return 0; }";
    source_comment_out_uniforms(source);
    EXPECT_EQ(source, "int main() { return 0; }");
  }

  {
    std::string source = "uniform vec3 pos;\nuniform vec4 color;\n";
    source_comment_out_uniforms(source);
    EXPECT_EQ(source, "//iform vec3 pos;\n//iform vec4 color;\n");
  }
}

}  // namespace blender::ocio
