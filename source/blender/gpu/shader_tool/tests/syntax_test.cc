/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "shader_tool_testing.hh"

namespace blender::gpu::tests {

using namespace shader;
using namespace std;

TEST(shader_tool, Array)
{
  {
    string input = R"(
float a[] = {0, 1};
float b[2] = {
    a[0],
    a(0, 1),
};
float d[] = {a[0], a(0, 1)};
)";
    string expect =
        R"(
float a[2] = ARRAY_T(float) ARRAY_V( 0, 1 );
float b[2] = ARRAY_T(float) ARRAY_V(
    a[0],
    a(0, 1)
 );
float d[2] = ARRAY_T(float) ARRAY_V( a[0], a(0, 1) );
)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(expect, output);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
float c[] = {};
)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error, "Array size must be greater than zero.");
  }
  {
    string input = R"(
float c[0] = {};
)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error, "Array size must be greater than zero.");
  }
  {
    string input = R"(
float2 c[2] = {{0, 1}, {0, 1}};
)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error, "Nested initializer list is not supported.");
  }
  {
    string input = R"(
float a[] = {0, 1};
float b[2] = {a[0], a[1],};
float d[] = {a[0], a[1] + a[1]};
)";
    string expect =
        R"(
float a[2] = {0, 1};
float b[2] = {a[0], a[1] };
float d[2] = {a[0], a[1] + a[1]};
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(expect, output);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(float c[];)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error,
              "Definition of variable with array type needs an explicit size or an initializer");
  }
  {
    string input = R"(float c[] = {};)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Array size must be greater than zero");
  }
  {
    string input = R"(float c[][] = {};)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Multi-dimensional arrays cannot have implicit size");
  }
  {
    string input = R"(float c[0] = {};)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Array size must be greater than zero");
  }
  {
    string input = R"(float c[1][0] = {};)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Array size must be greater than zero");
  }
  {
    string input = R"(float2 c[2] = {{0, 1}, {0, 1}};)";
    string expect = R"(float2 c[2] = {float2(0, 1), float2(0, 1)};)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(expect, output);
    EXPECT_EQ(error, "");
  }
}

TEST(shader_tool, Swizzle)
{
  {
    string input = R"(a.xyzw().aaa().xxx().grba().yzww; aaaa();)";
    string expect = R"(a.xyzw  .aaa  .xxx  .grba  .yzww; aaaa();)";
    auto [output, _, error] = process_test_local(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

TEST(shader_tool, BinaryLiterals)
{
  {
    string input = R"(0b1 0b10u 0b10001000100010001000100010001000)";
    string expect = R"(1 2u 2290649224)";
    auto [output, _, error] = process_test_local(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

TEST(shader_tool, MatrixConstructors)
{
#ifndef __APPLE__ /* This processing is only done for metal compatibility. */
  GTEST_SKIP() << "This processing is only done for metal compatibility.";
  return;
#endif
  {
    string input = R"(mat3(a); mat3 a; my_mat4x4(a); mat2x2(a); mat3x2(a);)";
    string expect = R"(__mat3x3(a); mat3 a; my_mat4x4(a); __mat2x2(a); mat3x2(a);)";
    auto [output, _, error] = process_test_string(input, Language::GLSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

TEST(shader_tool, CommaDeclaration)
{
  {
    string input = R"(
struct A {
  int a, b;
};
)";
    string expect =
        R"(
struct A {
  int a;int b;
};
#line 2
                 A A_ctor_() {A r;r.a=0;r.b=0;return r;}


)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(expect, output);
    EXPECT_EQ(error, "");
  }
}

TEST(shader_tool, Reference)
{
  {
    string input = R"(void func() { auto &a = b; a.a = 0; c = a(a); a_c_a = a; })";
    string expect = R"(void func() {              b.a = 0; c = a(b); a_c_a = b; })";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(void func() { const int &a = b; a.a = 0; c = a(a); })";
    string expect = R"(void func() {                   b.a = 0; c = a(b); })";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(void func() { const int i = 0; auto &a = b[i]; a.a = 0; })";
    string expect = R"(void func() { const int i = 0;                 b[i].a = 0; })";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(void func() { auto &a = b(0); })";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error, "Reference definitions cannot contain function calls.");
  }
  {
    string input = R"(void func() { int i = 0; auto &a = b[i++]; })";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error, "Reference definitions cannot have side effects.");
  }
  {
    string input = R"(void func() { auto &a = b[0 + 1]; })";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error,
              "Array subscript inside reference declaration must be a single variable or a "
              "constant, not an expression.");
  }
  {
    string input = R"(void func() { auto &a = b[c]; })";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error,
              "Cannot locate array subscript variable declaration. "
              "If it is a global variable, assign it to a temporary const variable for "
              "indexing inside the reference.");
  }
  {
    string input = R"(void func() { int c = 0; auto &a = b[c]; })";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error, "Array subscript variable must be declared as const qualified.");
  }
  {
    string input = R"(auto &a = b;)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error, "Unexpected token \"&\": Expecting declaration");
  }

  {
    string input = R"(
int b[1]; const int i = 0; auto &a = b[i]; a = 0;)";
    string expect = R"(
int b[1]; const int i = 0;                 b[i]= 0;)";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
int b(int){ return 0; }
void f(){ auto &a = b(0); }
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Function calls are not allowed in reference definition");
  }
  {
    string input = R"(
int b(int){ return 0; }
void f(){ int c[1]; auto &a = c[b(0)]; }
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Function calls are not allowed in reference definition");
  }
  {
    string input = R"(
int b[1]; int i = 0; auto &a = b[i + 1];)";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "Reference cannot bind to expression of non-const subscript index");
  }
  {
    string input = R"(
int b[1]; const int i = 0; auto &a = b[i + 1];)";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
int b; float &a = b;)";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "Reference to type 'float' cannot bind to a value of unrelated type 'int'");
  }
  {
    string input = R"(
int b; int &a = b + 1;)";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "Reference cannot bind to a temporary");
  }
  {
    /* Only allowed in C++ if ref is const. */
    string input = R"(
const int &a = 1;)";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
int b; int &a = b++;)";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "Reference cannot bind to an expression with side effects");
  }
  {
    string input = R"(
int b; int &a = ++b;)";
    auto [output, _, error] = process_test_local(input, Language::BSL);
    EXPECT_EQ(error, "Reference cannot bind to an expression with side effects");
  }
}

TEST(shader_tool, Constexpr)
{
  {
    string input = R"(
static constexpr int a{1};
static constexpr int b = {3};
static constexpr int c = int(3);
static constexpr int d = 4;
static constexpr int e = {{4}};
static constexpr int f = (a + b / 2) * d - e;
static constexpr int g = floatBitsToInt(24);
static constexpr float h = intBitsToFloat(floatBitsToInt(24));
int func() { return f; })";
    string expect = R"(
static constexpr int a= 1;
static constexpr int b = 3;
static constexpr int c = 3;
static constexpr int d = 4;
static constexpr int e = 4;
static constexpr int f = 4;
static constexpr int g = 1103101952;
static constexpr float h = 24.0f;
int func() { return 4; })";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

TEST(shader_tool, ConstexprVector)
{
  {
    string input = R"(
static constexpr float3 i = float3(0, 1.5f, 2u);
static constexpr int3 j = int3(i + int3(0, 1, 2)) + 1;
static constexpr float3 k = i + int3(0, 1, 2);
)";
    string expect = R"(
static constexpr float3 i = float3(0.0f, 1.5f, 2.0f);
static constexpr int3 j = int3(1, 3, 5);
static constexpr float3 k = float3(0.0f, 2.5f, 4.0f);
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
static constexpr int3 j = int3(0, 1, 2) + int2(1);
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Invalid operands to binary expression ('int3' + 'int2')");
  }
  {
    string input = R"(
static constexpr int3 j = int3(int2(0), 2);
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Constexpr variable 'j' must be initialized by a constant expression");
  }
  {
    string input = R"(
static constexpr int3 j = int3(0, 1, 2) + false;
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Invalid operands to binary expression ('int3' + 'bool')");
  }
  {
    string input = R"(
static constexpr int3 j = int2(0);
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error,
              "Constexpr variable 'j' must be initialized by a constant expression of type 'int3' "
              "but got 'int2'");
  }

  {
    string input = R"(
static constexpr float3 a = float3(2.5f);
static constexpr int4 b = int4(-1);
)";
    string expect = R"(
static constexpr float3 a = float3(2.5f, 2.5f, 2.5f);
static constexpr int4 b = int4(-1, -1, -1, -1);
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }

  {
    string input = R"(
static constexpr float3 v = float3(1.0f, 2.0f, 3.0f);
static constexpr float3 mul_right = v * 2.0f;
static constexpr float3 mul_left = 2.0f * v;
static constexpr float3 sub_scalar = 10.0f - v;
static constexpr float3 div_scalar = v / 2.0f;
)";
    string expect = R"(
static constexpr float3 v = float3(1.0f, 2.0f, 3.0f);
static constexpr float3 mul_right = float3(2.0f, 4.0f, 6.0f);
static constexpr float3 mul_left = float3(2.0f, 4.0f, 6.0f);
static constexpr float3 sub_scalar = float3(9.0f, 8.0f, 7.0f);
static constexpr float3 div_scalar = float3(0.5f, 1.0f, 1.5f);
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }

  {
    string input = R"(
static constexpr int3 a = int3(1, -2, 3);
static constexpr int3 neg = -a;
static constexpr int3 bit_and = int3(7, 3, 1) & int3(3, 2, 1);
static constexpr int3 bit_or = int3(4, 2, 0) | int3(1, 1, 0);
static constexpr int3 shift = int3(1, 2, 4) << int3(1, 2, 0);
)";
    string expect = R"(
static constexpr int3 a = int3(1, -2, 3);
static constexpr int3 neg = int3(-1, 2, -3);
static constexpr int3 bit_and = int3(3, 2, 1);
static constexpr int3 bit_or = int3(5, 3, 0);
static constexpr int3 shift = int3(2, 8, 4);
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }

  {
    string input = R"(
static constexpr float3 v1 = float3(1.0f, 2.0f, 3.0f);
static constexpr float3 v2 = float3(2.0f, 2.0f, 1.0f);
static constexpr bool3 eq = equal(v1, v2);
static constexpr bool3 lt = lessThan(v1, v2);
static constexpr bool3 gte = greaterThanEqual(v1, v2);
)";
    string expect = R"(
static constexpr float3 v1 = float3(1.0f, 2.0f, 3.0f);
static constexpr float3 v2 = float3(2.0f, 2.0f, 1.0f);
static constexpr bool3 eq = bool3(false, true, false);
static constexpr bool3 lt = bool3(true, false, false);
static constexpr bool3 gte = bool3(false, true, true);
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }

#if 0 /* Would be nice to support at some points. */
  {
      string input = R"(
static constexpr float3 a = float3(-1.0f, 2.0f, -5.0f);
static constexpr float3 b_abs = abs(a);
static constexpr float3 b_min = min(a, float3(0.0f));
static constexpr float3 b_clamp = clamp(a, float3(-2.0f), float3(1.0f));
static constexpr float d = dot(float3(1.0f, 2.0f, 3.0f), float3(4.0f, 5.0f, 6.0f));
static constexpr float3 c = cross(float3(1, 0, 0), float3(0, 1, 0));
static constexpr bool is_all = all(bool3(true, true, true));
static constexpr bool is_any = any(bool3(false, true, false));
)";
      string expect = R"(
static constexpr float3 a = float3(-1.0f, 2.0f, -5.0f);
static constexpr float3 b_abs = float3(1.0f, 2.0f, 5.0f);
static constexpr float3 b_min = float3(-1.0f, 0.0f, -5.0f);
static constexpr float3 b_clamp = float3(-1.0f, 1.0f, -2.0f);
static constexpr float d = 32.0f;
static constexpr float3 c = float3(0.0f, 0.0f, 1.0f);
static constexpr bool is_all = true;
static constexpr bool is_any = true;
)";
      auto [output, _, error] = process_test_string(input, Language::BSL);
      EXPECT_EQ(output, expect);
      EXPECT_EQ(error, "");
    }
  {
    string input = R"(
static constexpr float4 v = float4(1.0f, 2.0f, 3.0f, 4.0f);
static constexpr float2 s1 = v.xz;
static constexpr float3 s2 = v.wzy + 1.0f;
static constexpr float s3 = v.x + v.w;
)";
    string expect = R"(
static constexpr float4 v = float4(1.0f, 2.0f, 3.0f, 4.0f);
static constexpr float2 s1 = float2(1.0f, 3.0f);
static constexpr float3 s2 = float3(5.0f, 4.0f, 3.0f);
static constexpr float s3 = 5.0f;
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
#endif

  {
    string input = R"(
static constexpr int3 v = int3(1, 2, 3) / int3(1, 0, 1);
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Division by zero during constexpr evaluation");
  }

  {
    string input = R"(
static constexpr int3 j = float3(1.0f, 2.0f, 3.0f);
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error,
              "Constexpr variable 'j' must be initialized by a constant expression of type 'int3' "
              "but got 'float3'");
  }
}

TEST(shader_tool, Auto)
{
  {
    string input =
        "auto a{0};\n"
        "auto b{0.0f};\n"
        "auto c{0.0f + 0};\n"
        "auto d = 0;\n"
        "auto e = 0.0f;\n"
        "auto f = 0.0f + 0;\n"
        "auto g = a * b;\n"
        "auto h = a * a;\n"
        "float4x4 i[4];\n"
        "auto j = i[0];\n"
        "auto k = i[0][0];\n"
        "auto l = i[0][0][0];\n";
    string expect =
        "int a=int(0);\n"
        "float b=float(0.0f);\n"
        "float c=float(0.0f + 0);\n"
        "int d = 0;\n"
        "float e = 0.0f;\n"
        "float f = 0.0f + 0;\n"
        "float g = a * b;\n"
        "int h = a * a;\n"
        "float4x4 i[4];\n"
        "float4x4 j = i[0];\n"
        "float4 k = i[0][0];\n"
        "float l = i[0][0][0];\n";
    auto [output, _, error] = process_test_string(input, shader::Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

TEST(shader_tool, ArrayAggregate)
{
  {
    string input = "int a[4] = {0, 1, 2, 3};\n";
    string expect = "int a[4] = {0, 1, 2, 3};\n";
    auto [output, _, error] = process_test_string(input, shader::Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = "int a[4][2] = {{0,0}, {1,1}, {int{2},{2}}, {3,3}};\n";
    string expect = "int a[4][2] = {{0,0}, {1,1}, {int(2),int(2)}, {3,3}};\n";
    auto [output, _, error] = process_test_string(input, shader::Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

}  // namespace blender::gpu::tests
