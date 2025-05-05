/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "glsl_preprocess/glsl_preprocess.hh"

#include "gpu_testing.hh"

namespace blender::gpu::tests {

static void test_preprocess_utilities()
{
  using namespace shader;
  using namespace std;
  /* get_content_between_balanced_pair */
  string input = "test (u, u(s,(s,s)), u) {t{{}},t,{};(,)} {u{}} end";
  EXPECT_EQ(Preprocessor::get_content_between_balanced_pair(input, '{', '}'), "t{{}},t,{};(,)");
  EXPECT_EQ(Preprocessor::get_content_between_balanced_pair(input, '{', '}', true), "u{}");

  EXPECT_EQ(Preprocessor::replace_char_between_balanced_pair(input, '(', ')', ',', '!'),
            "test (u! u(s!(s!s))! u) {t{{}},t,{};(!)} {u{}} end");

  vector<string> split_expect{"test (u, u(s,(s,s", "", ", u", " {t{{}},t,{};(,", "} {u{}} end"};
  vector<string> split_result = Preprocessor::split_string(input, ')');
  EXPECT_EQ_VECTOR(split_expect, split_result);

  string input2 = "u, u(s,(s,s)), u";
  vector<string> split_expect2{"u", " u(s,(s,s))", " u"};
  vector<string> split_result2 = Preprocessor::split_string_not_between_balanced_pair(
      input2, ',', '(', ')');
  EXPECT_EQ_VECTOR(split_expect2, split_result2);
}
GPU_TEST(preprocess_utilities);

static std::string process_test_string(std::string str,
                                       std::string &first_error,
                                       shader::metadata::Source *r_metadata = nullptr)
{
  using namespace shader;
  Preprocessor preprocessor;
  shader::metadata::Source metadata;
  std::string result = preprocessor.process(
      Preprocessor::SourceLanguage::BLENDER_GLSL,
      str,
      "test.glsl",
      true,
      true,
      [&](const std::smatch & /*match*/, const char *err_msg) {
        if (first_error.empty()) {
          first_error = err_msg;
        }
      },
      metadata);

  if (r_metadata != nullptr) {
    *r_metadata = metadata;
  }

  /* Strip first line directive as they are platform dependent. */
  size_t newline = result.find('\n');
  return result.substr(newline + 1);
}

static void test_preprocess_unroll()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"([[gpu::unroll]] for (int i = 2; i < 4; i++, y++) { content += i; })";
    string expect = R"({ int i = 2;
#line 1
{ content += i; }
#line 1
i++, y++;
#line 1
{ content += i; }
#line 1
i++, y++;
#line 1
})";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"([[gpu::unroll]] for (int i = 2; i < 4 && i < y; i++, y++) { cont += i; })";
    string expect = R"({ int i = 2;
#line 1
if (i < y) { cont += i; }
#line 1
i++, y++;
#line 1
if (i < y) { cont += i; }
#line 1
i++, y++;
#line 1
})";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"([[gpu::unroll(2)]] for (; i < j;) { content += i; })";
    string expect = R"({ ;
#line 1
if (i < j) { content += i; }
#line 1
;
#line 1
if (i < j) { content += i; }
#line 1
;
#line 1
})";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"([[gpu::unroll(2)]] for (; i < j;) { [[gpu::unroll(2)]] for (; j < k;) {} })";
    string expect = R"({ ;
#line 1
if (i < j) { { ;
#line 1
 if (j < k) {}
#line 1
 ;
#line 1
 if (j < k) {}
#line 1
 ;
#line 1
 } }
#line 1
;
#line 1
if (i < j) { { ;
#line 1
 if (j < k) {}
#line 1
 ;
#line 1
 if (j < k) {}
#line 1
 ;
#line 1
 } }
#line 1
;
#line 1
})";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"([[gpu::unroll(2)]] for (; i < j;) { break; })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Error: Unrolled loop cannot contain \"break\" statement.");
  }
  {
    string input = R"([[gpu::unroll(2)]] for (; i < j;) { continue; })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Error: Unrolled loop cannot contain \"continue\" statement.");
  }
  {
    string input = R"([[gpu::unroll(2)]] for (; i < j;) { for (; j < k;) {break;continue;} })";
    string expect = R"({ ;
#line 1
if (i < j) { for (; j < k;) {break;continue;} }
#line 1
;
#line 1
if (i < j) { for (; j < k;) {break;continue;} }
#line 1
;
#line 1
})";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"([[gpu::unroll]] for (int i = 3; i > 2; i++) {})";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Error: Unsupported condition in unrolled loop.");
  }
}
GPU_TEST(preprocess_unroll);

}  // namespace blender::gpu::tests
