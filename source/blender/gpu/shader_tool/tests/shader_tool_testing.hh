/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h" /* IWYU pragma: export */

#include "processor.hh"

namespace blender::gpu::tests {

static inline std::string process_test_string(
    std::string str,
    std::string &first_error,
    shader::metadata::Source *r_metadata = nullptr,
    shader::Language language = shader::Language::BLENDER_GLSL)
{
  using namespace shader;
  SourceProcessor processor(str, "test.bsl", language);

  auto [result, metadata, error] = processor.convert();

  if (error) {
    first_error = error.value().message;
  }

  if (r_metadata != nullptr) {
    *r_metadata = metadata;
  }

  /* Strip first line directive as they are platform dependent. */
  size_t newline = result.find('\n');
  return result.substr(newline + 1);
}

/* Process a test string inside a wrapper function. */
static inline std::string process_test_local(
    std::string str,
    std::string &first_error,
    shader::metadata::Source *r_metadata = nullptr,
    shader::Language language = shader::Language::BLENDER_GLSL)
{
  std::string prefix = "void wrapper_func() {";
  std::string suffix = "\n}";
  std::string result = process_test_string(
      prefix + str + suffix, first_error, r_metadata, language);
  result = result.substr(prefix.size(), result.size() - suffix.size() - prefix.size());
  if (result.starts_with("\n#line 4")) {
    result = "\n" + result.substr(std::string("\n#line 4").size());
  }
  return result;
}

}  // namespace blender::gpu::tests
