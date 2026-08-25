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
    auto [output, _, error] = process_test_local(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
for (int i = 2; i < 4; i++, y++) [[unroll]] { content += i; })";
    string expect = R"(
    {int i = 2;                             { content += i; }
#line 2
                       i++, y++;            { content += i; }
#line 2
                       i++, y++;                            })";
    auto [output, _, error] = process_test_local(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
for (int i = 2; i < 4 && i < y; i++, y++) [[unroll]] { cont += i; })";
    string expect = R"(
    {int i = 2;
#line 2
             if(i < 4 && i < y)                      { cont += i; }
#line 2
                                i++, y++;
#line 2
             if(i < 4 && i < y)                      { cont += i; }
#line 2
                                i++, y++;                         })";
    auto [output, _, error] = process_test_local(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
for (; i < j;) [[unroll_n(2)]] { content += i; })";
    string expect = R"(

{
#line 2
    if(i < j)                  { content += i; }
#line 2
    if(i < j)                  { content += i; }
#line 2
                                               })";
    auto [output, _, error] = process_test_local(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
for (; i < j;) [[unroll_n(2)]] { for (; j < k;) [[unroll_n(2)]] {} })";
    string expect = R"(

{
#line 2
    if(i < j)                  {
{
#line 2
                                     if(j < k)                  {}
#line 2
                                     if(j < k)                  {}
#line 2
                                                                 } }
#line 2
    if(i < j)                  {
{
#line 2
                                     if(j < k)                  {}
#line 2
                                     if(j < k)                  {}
#line 2
                                                                 } }
#line 2
                                                                   })";
    auto [output, _, error] = process_test_local(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(for (; i < j;) [[unroll_n(2)]] { break; })";
    auto [output, _, error] = process_test_local(input);
    EXPECT_EQ(error, "Unrolled loop cannot contain \"break\" statement.");
  }
  {
    string input = R"(for (; i < j;) [[unroll_n(2)]] { continue; })";
    auto [output, _, error] = process_test_local(input);
    EXPECT_EQ(error, "Unrolled loop cannot contain \"continue\" statement.");
  }
  {
    string input = R"(
for (; i < j;) [[unroll_n(2)]] { for (; j < k;) {break;continue;} })";
    string expect = R"(

{
#line 2
    if(i < j)                  { for (; j < k;) {break;continue;} }
#line 2
    if(i < j)                  { for (; j < k;) {break;continue;} }
#line 2
                                                                  })";
    auto [output, _, error] = process_test_local(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(for (int i = 3; i > 2; i++) [[unroll]] {})";
    auto [output, _, error] = process_test_local(input);
    EXPECT_EQ(error, "Unsupported condition in unrolled loop.");
  }
  {
    string input = R"(
for (int a = 0; a < 2; a += 1) [[unroll]] { a; }
for (uint b = 1; b < 3; ++b  ) [[unroll]] { b; }
for (int c = 2; c > 0; c--   ) [[unroll]] { c; }
for (int d = 2; d > 0; --d   ) [[unroll]] { d; }
for (int e = 0; e < 4; e += 2) [[unroll]] { e; }
for (int f = 4; f > 0; f -= 2) [[unroll]] { f; }
for (int g = 1; g < 4; g *= 2) [[unroll]] { g; }
for (int h = 4; h > 1; h /= 2) [[unroll]] { h; }
for (int i = 0; i < 2; i=i+1 ) [[unroll]] { i; }
)";
    string expect = R"(
                                          { 0; }
#line 2
                                          { 1; }
                                          { 1; }
#line 3
                                          { 2; }
                                          { 2; }
#line 4
                                          { 1; }
                                          { 2; }
#line 5
                                          { 1; }
                                          { 0; }
#line 6
                                          { 2; }
                                          { 4; }
#line 7
                                          { 2; }
                                          { 1; }
#line 8
                                          { 2; }
                                          { 4; }
#line 9
                                          { 2; }
                                          { 0; }
#line 10
                                          { 1; }
)";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
int i;
for (; i < 2; i++) [[unroll]] { i; }
  )";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "Init statement needs to define the loop variable for unrolled loops");
  }
  {
    string input = R"(
for (float i = 0; i < 2; i++) [[unroll]] { i; }
  )";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "Loop variable needs to be an integer type for unrolled loops");
  }
  {
    string input = R"(
for (int i = 0, j = 0; i < 2; i++) [[unroll]] { i; }
  )";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "Multiple variables declared in unrolled loop");
  }
  {
    string input = R"(
for (float i = 0.0; i < 2.0; i += 1.0) [[unroll]] { i; }
  )";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "Loop variable needs to be an integer type for unrolled loops");
  }
  {
    string input = R"(
for (int i; i < 2; i++) [[unroll]] { i; }
  )";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error,
              "Loop variable needs to be assigned a value (using assignment) for "
              "unrolled loops");
  }
  {
    string input = R"(
for (int i = atomicAdd(i, i); i < 2; i++) [[unroll]] { i; }
  )";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "Non-constant expression in unrolled loop control");
  }
  {
    string input = R"(
for (int i = 0; i < 2; i++, i++) [[unroll]] { i; }
  )";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "Comma operator is not allowed in unrolled loop statement");
  }
  {
    string input = R"(
for (int i = 0; i < atomicAdd(i, i); i++) [[unroll]] { i; }
  )";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "Non-constant expression in unrolled loop control");
  }
  {
    string input = R"(
for (int i = 0; i < 65; i++) [[unroll]] { i; }
  )";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "Loop unrolling generates too many iterations (over 64)");
  }
  {
    string input = R"(
int j = 0;
for (int i = 0; i < 2; j++) [[unroll]] { i; }
  )";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "Unrolled loop expression must assign to 'i'");
  }
  {
    string input = R"(
for (int i = 0; i < 2; i + 1) [[unroll]] { i; }
  )";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error,
              "Expected '++i', '--i', 'i++', 'i--' or 'i = expr', 'i += expr', 'i -= expr', 'i /= "
              "expr', 'i *= expr' for unrolled loop expression");
  }
  {
    string input = R"(
for (int i = 1; i < 2; i <<= 1) [[unroll]] { i; }
  )";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error,
              "Expected '++i', '--i', 'i++', 'i--' or 'i = expr', 'i += expr', 'i -= expr', 'i /= "
              "expr', 'i *= expr' for unrolled loop expression");
  }
  {
    string input = R"(
for (int i = 0; i < 2; i += atomicAdd(i, i)) [[unroll]] { i; }
  )";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "Non-constant expression in unrolled loop control");
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


#ifndef GPU_METAL
Resources Resources_ctor_();
void _fn(Resources  this_);
Resources Resources_new_();
#endif
#line 2
                         Resources Resources_ctor_() {Resources r;r._pad=0;return r;}



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
    auto [output, _, error] = process_test_string(input);
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
    auto [output, _, error] = process_test_string(input);
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
    auto [output, _, error] = process_test_string(input);
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
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error, "Expecting single condition.");
  }
  {
    string input = R"(
template<int N> int implicit_test(int x)
{
  if (N == 0) {
    return 0;
  }
  else if (x == 1) {
    return 1;
  }
  else if (N == 1) {
    return 2;
  }
  else {
    return 3;
  }
}

template int implicit_test<0>();
template int implicit_test<1>();
template int implicit_test<2>();
)";
    string expect = R"(
                int implicit_testT0(int x)
{
              {
    return 0;
  }
#line 16
}
#line 2
                int implicit_testT1(int x)
{
#line 7
       if (x == 1) {
    return 1;
  }
  else             {
    return 2;
  }
#line 16
}
#line 2
                int implicit_testT2(int x)
{
#line 7
       if (x == 1) {
    return 1;
  }
#line 13
  else {
    return 3;
  }
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
template<int B> int chain_test(int y)
{
  if (y == 0) {
    return 0;
  }
  else if constexpr (B == 1) {
    return 1;
  }
  else if (y == 2) {
    return 2;
  }
  else {
    return 3;
  }
}

template int chain_test<1>();
template int chain_test<0>();
)";
    string expect = R"(
                int chain_testT1(int y)
{
  if (y == 0) {
    return 0;
  }
  else                       {
    return 1;
  }
#line 16
}
#line 2
                int chain_testT0(int y)
{
  if (y == 0) {
    return 0;
  }
#line 10
  else if (y == 2) {
    return 2;
  }
  else {
    return 3;
  }
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
template<int i> int f(int a)
{
  if constexpr (i == 0) {
    return 0;
  }
  else if (i == 1) {
    return 1;
  }
  else if (i + a == 1) {
    return 3;
  }
  else if constexpr (i == 2) {
    return 2;
  }
  else {
    return 4;
  }
}

template int f<0>(int);
template int f<1>(int);
template int f<2>(int);
template int f<3>(int);
)";
    string expect = R"(
                int fT0(int a)
{
                        {
    return 0;
  }
#line 19
}
#line 2
                int fT1(int a)
{
#line 7
                   {
    return 1;
  }
#line 19
}
#line 2
                int fT2(int a)
{
#line 10
       if (2 + a == 1) {
    return 3;
  }
  else                       {
    return 2;
  }
#line 19
}
#line 2
                int fT3(int a)
{
#line 10
       if (3 + a == 1) {
    return 3;
  }
#line 16
  else {
    return 4;
  }
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
void f(int a)
{
  if constexpr (a == 0) {
    return;
  }
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Constexpr if condition is not a constant expression");
  }
  {
    string input = R"(
enum E : int {
  ENUM = 1,
};

struct Res {
  [[compilation_constant]] const int i;
};

void f(Res res)
{
  if (res.i + 1 + E::ENUM == ENUM) [[static_branch]] {
    0;
  }
  else if (res.i == 1) [[static_branch]] {
    1;
  }
  else {
    2;
  }
}
)";
    string expect = R"(
#define E int
static constexpr int E_ENUM  = 1;
#define access_Res_i() i


#ifdef CREATE_INFO_RES_PASS_Res
CREATE_INFO_RES_PASS_Res
#endif
#ifdef CREATE_INFO_RES_BATCH_Res
CREATE_INFO_RES_BATCH_Res
#endif
#ifdef CREATE_INFO_RES_GEOMETRY_Res
CREATE_INFO_RES_GEOMETRY_Res
#endif
#ifdef CREATE_INFO_RES_SHARED_VARS_Res
CREATE_INFO_RES_SHARED_VARS_Res
#endif
#line 6
struct Res {

int _pad;
#line 8
};
#line 6
Res Res_ctor_() {Res r;r._pad=0;return r;}
#if defined(CREATE_INFO_Res)


void f(Res res)
{
  #if SRT_CONSTANT_i+1+1==1
#line 12
                                                     {
    0;
  }
  #elif SRT_CONSTANT_i==1
#line 15
                                         {
    1;
  }
  #else
#line 18
       {
    2;
  }
  #endif
#line 21
}

#endif
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

TEST(shader_tool, BoolCast)
{
  {
    string input = R"(int i; if (i == 0) {})";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(int i; if (i == 0u) {})";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    /* TODO(fclem): Should be turned on at some point, but current codebase uses it a lot. */
    // EXPECT_EQ(error, "Invalid operands to binary expression ('int' == 'uint')");
  }
  {
    string input = R"(if (1) {})";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
uint i; if (i) {})";
    string expect = R"(
uint i; if (i!=0u) {
#line 2
                })";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
int i; if (i) {})";
    string expect = R"(
int i; if (i!=0) {
#line 2
               })";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

TEST(shader_tool, ForceInline)
{
  /* Call inside itself (recursion). */
  {
    string input = R"(
[[force_inline]] void f() { f(); }
void foo() { f(); }
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Cannot inline recursive call");
  }
  {
    string input = R"(
int f3(int a);
[[force_inline]] int f2(int a) {
  return f3(a);
}
[[force_inline]] int f3(int b) {
  return f2(b);
}
void foo() {
  int result = f3(5);
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Cannot inline recursive call");
  }

  /* Call inside compound expression. */
  {
    string input = R"(
[[force_inline]] int get_val() { return 42; }
void foo() {
  int x = get_val() + 10;
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Cannot inline function call inside a compound expression");
  }

  /* Call inside loops. */
  {
    string input = R"(
[[force_inline]] int add_one(int val) {
  return val + 1;
}
void foo() {
  int sum = 0;
  for (int i = 0; i < 10; ++i) {
    sum = add_one(sum);
  }
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    /* TODO(fclem): We should support that at some point. */
    EXPECT_EQ(error, "Cannot inline function call inside a compound expression");
  }

  /* Call inside if conditions. */
  {
    string input = R"(
[[force_inline]] bool is_active() { return true; }
void foo() {
  if (is_active()) {
    int x = 1;
  }
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Cannot inline function call inside a compound expression");
  }

  /* Call inside other inlined functions (nesting). */
  {
    string input = R"(
[[force_inline]] int inner(int a) {
  return a * 2;
}
[[force_inline]] int outer(int b) {
  return inner(b);
}
void foo() {
  int result = outer(5);
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "");
  }
}

}  // namespace blender::gpu::tests
