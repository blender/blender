/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "shader_tool_testing.hh"

namespace blender::gpu::tests {

using namespace shader::parser;
using namespace shader;
using namespace std;

TEST(shader_tool, Unroll)
{
  {
    string input = R"(
for (int i = 2; i < 4; i++) [[unroll]] { content += i; })";
    string expect = R"(

{
#line 2
                                       { content += 2; }
#line 2
                                       { content += 3; }
#line 2
                                                       })";
    string error;
    string output = process_test_local(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
for (int i = 2; i < 4; i++, y++) [[unroll]] { content += i; })";
    string expect = R"(
    {int i = 2;
#line 2
                                            { content += i; }
#line 2
                       i++, y++;
#line 2
                                            { content += i; }
#line 2
                       i++, y++;
#line 2
                                                            })";
    string error;
    string output = process_test_local(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
for (int i = 2; i < 4 && i < y; i++, y++) [[unroll]] { cont += i; })";
    string expect = R"(
    {int i = 2;
#line 2
             if(i < 4 && i < y)
#line 2
                                                     { cont += i; }
#line 2
                                i++, y++;
#line 2
             if(i < 4 && i < y)
#line 2
                                                     { cont += i; }
#line 2
                                i++, y++;
#line 2
                                                                  })";
    string error;
    string output = process_test_local(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
for (; i < j;) [[unroll_n(2)]] { content += i; })";
    string expect = R"(

{
#line 2
    if(i < j)
#line 2
                               { content += i; }
#line 2
    if(i < j)
#line 2
                               { content += i; }
#line 2
                                               })";
    string error;
    string output = process_test_local(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
for (; i < j;) [[unroll_n(2)]] { for (; j < k;) [[unroll_n(2)]] {} })";
    string expect = R"(

{
#line 2
    if(i < j)
#line 2
                               {
{
#line 2
                                     if(j < k)
#line 2
                                                                {}
#line 2
                                     if(j < k)
#line 2
                                                                {}
#line 2
                                                                 } }
#line 2
    if(i < j)
#line 2
                               {
{
#line 2
                                     if(j < k)
#line 2
                                                                {}
#line 2
                                     if(j < k)
#line 2
                                                                {}
#line 2
                                                                 } }
#line 2
                                                                   })";
    string error;
    string output = process_test_local(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(for (; i < j;) [[unroll_n(2)]] { break; })";
    string error;
    string output = process_test_local(input, error);
    EXPECT_EQ(error, "Unrolled loop cannot contain \"break\" statement.");
  }
  {
    string input = R"(for (; i < j;) [[unroll_n(2)]] { continue; })";
    string error;
    string output = process_test_local(input, error);
    EXPECT_EQ(error, "Unrolled loop cannot contain \"continue\" statement.");
  }
  {
    string input = R"(
for (; i < j;) [[unroll_n(2)]] { for (; j < k;) {break;continue;} })";
    string expect = R"(

{
#line 2
    if(i < j)
#line 2
                               { for (; j < k;) {break;continue;} }
#line 2
    if(i < j)
#line 2
                               { for (; j < k;) {break;continue;} }
#line 2
                                                                  })";
    string error;
    string output = process_test_local(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(for (int i = 3; i > 2; i++) [[unroll]] {})";
    string error;
    string output = process_test_local(input, error);
    EXPECT_EQ(error, "Unsupported condition in unrolled loop.");
  }
}

TEST(shader_tool, StaticBranch)
{
  {
    string input = R"(
struct Resources {
  [[compilation_constant]] const int use_color_band;

  void fn() {
    if (use_color_band) [[static_branch]] {
      test;
    }
  }
};

void func([[resource_table]] Resources &srt)
{
  if (srt.use_color_band) [[static_branch]] {
    test;
  }

  if (srt.use_color_band == 1) [[static_branch]] {
    test;
  } else {
    test;
  }

  if (srt.use_color_band) [[static_branch]] {
    test;
  } else if (srt.use_color_band) [[static_branch]] {
    test;
  }

  if (srt.use_color_band) [[static_branch]] {
    test;
  } else if (srt.use_color_band) [[static_branch]] {
    test;
  } else {
    test;
  }

  if (srt.use_color_band) [[static_branch]] {
    if (srt.use_color_band) [[static_branch]] {
      test;
    }
  }
}
)";
    string expect = R"(
#define access_Resources_use_color_band() use_color_band
#ifdef CREATE_INFO_RES_PASS_Resources
CREATE_INFO_RES_PASS_Resources
#endif
#ifdef CREATE_INFO_RES_BATCH_Resources
CREATE_INFO_RES_BATCH_Resources
#endif
#ifdef CREATE_INFO_RES_GEOMETRY_Resources
CREATE_INFO_RES_GEOMETRY_Resources
#endif
#ifdef CREATE_INFO_RES_SHARED_VARS_Resources
CREATE_INFO_RES_SHARED_VARS_Resources
#endif
#line 2
struct Resources {
#line 18
int _pad;};
#line 21
#ifndef GPU_METAL
Resources Resources_ctor_();
void _fn(Resources  this_);
Resources Resources_new_();
#endif
#line 2
                         Resources Resources_ctor_() {Resources r;r._pad=0;return r;}
#line 5

#if defined(CREATE_INFO_Resources)
#line 5
  void _fn(Resources  this_) {

#if SRT_CONSTANT_use_color_band
#line 6
                                                                 {
      test;
    }

#endif
#line 9
  }
#endif
       Resources Resources_new_()
{
  Resources result;
  result._pad = 0;
  return result;
#line 9
}
#line 12

#if defined(CREATE_INFO_Resources)
#line 12
void func(Resources  srt)
{

#if SRT_CONSTANT_use_color_band
#line 14
                                                               {
    test;
  }
#endif

#if SRT_CONSTANT_use_color_band == 1
#line 18
                                                                    {
    test;
  }
#else
#line 20
         {
    test;
  }
#endif

#if SRT_CONSTANT_use_color_band
#line 24
                                                               {
    test;
  }
#elif SRT_CONSTANT_use_color_band
#line 26
                                                                      {
    test;
  }
#endif

#if SRT_CONSTANT_use_color_band
#line 30
                                                               {
    test;
  }
#elif SRT_CONSTANT_use_color_band
#line 32
                                                                      {
    test;
  }
#else
#line 34
         {
    test;
  }
#endif

#if SRT_CONSTANT_use_color_band
#line 38
                                                               {

#if SRT_CONSTANT_use_color_band
#line 39
                                                                 {
      test;
    }

#endif
#line 42
  }

#endif
#line 43
}

#endif
#line 44
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
void func([[resource_table]] Resources &srt)
{
  if (srt.use_color_band) [[static_branch]] {
    test;
  } else if (srt.use_color_band) {
    test;
  }
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Expecting next if statement to also be a static branch.");
  }
  {
    string input = R"(
void func([[resource_table]] Resources &srt)
{
  if (use_color_band) [[static_branch]] {
    test;
  }
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error,
              "Expecting compilation or specialization constant. Make sure SRT arguments "
              "have the [[resource_table]] attribute.");
  }
  {
    string input = R"(
void func([[resource_table]] Resources &srt)
{
  if (srt.use_color_band && srt.use_color_band) [[static_branch]] {
    test;
  }
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Expecting single condition.");
  }
}

}  // namespace blender::gpu::tests
