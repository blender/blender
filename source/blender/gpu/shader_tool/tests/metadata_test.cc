/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "shader_tool_testing.hh"

namespace blender::gpu::tests {

using namespace shader::parser;
using namespace shader;
using namespace std;

TEST(shader_tool, Include)
{
  {
    string input = R"(
#include "a.hh"
#include "b.glsl"
#if 0
#  include "c.hh"
#else
#  include "d.hh"
#endif
#if !defined(GPU_SHADER)
#  include "e.hh"
#endif
)";
    string expect =
        R"(static void test(GPUSource &source, GPUFunctionDictionary *g_functions, GPUPrintFormatMap *g_formats) {
  source.add_dependency("a.hh");
  source.add_dependency("b.glsl");
  source.add_dependency("d.hh");
  UNUSED_VARS(source, g_functions, g_formats);
}
)";
    string error;
    shader::metadata::Source metadata;
    string output = process_test_string(input, error, &metadata);
    EXPECT_EQ(expect, metadata.serialize("test"));
    EXPECT_EQ(error, "");
  }
}

}  // namespace blender::gpu::tests
