/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "shader_tool/expression.hh"
#include "shader_tool/processor.hh"

#include "gpu_testing.hh"

namespace blender::gpu::tests {

static std::string process_test_string(std::string str,
                                       std::string &first_error,
                                       shader::metadata::Source *r_metadata = nullptr,
                                       shader::Language language = shader::Language::BLENDER_GLSL)
{
  using namespace shader;
  SourceProcessor processor(
      str,
      "test.bsl",
      language,
      [&](int /*err_line*/, int /*err_char*/, const std::string & /*line*/, const char *err_msg) {
        if (first_error.empty()) {
          first_error = err_msg;
        }
      });

  auto [result, metadata] = processor.convert();

  if (r_metadata != nullptr) {
    *r_metadata = metadata;
  }

  /* Strip first line directive as they are platform dependent. */
  size_t newline = result.find('\n');
  return result.substr(newline + 1);
}

static void test_preprocess_array()
{
  using namespace shader;
  using namespace std;
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
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(expect, output);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
float c[] = {};
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Array size must be greater than zero.");
  }
  {
    string input = R"(
float c[0] = {};
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Array size must be greater than zero.");
  }
  {
    string input = R"(
float2 c[2] = {{0, 1}, {0, 1}};
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Nested initializer list is not supported.");
  }
}
GPU_TEST(preprocess_array);

static void test_preprocess_comma_declaration()
{
  using namespace shader;
  using namespace std;
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
#line 5
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(expect, output);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_comma_declaration);

static void test_preprocess_include()
{
  using namespace shader;
  using namespace std;
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
GPU_TEST(preprocess_include);

static void test_preprocess_union()
{
  using namespace shader;
  using namespace std;
  {
    string input = R"(
struct [[host_shared]] T {
  union {
    union_t<uint4> a;
    union_t<int4> b;
    union_t<float4> c;
  };
};
)";
    string expect =
        R"(
#line 3

#define T_union0_host_shared_ T_union0
#define T_union0_host_shared_uniform_ T_union0
#line 3
struct                 T_union0 {
  float4 data0;

};
#line 3
                                        T_union0 T_union0_ctor_() {T_union0 r;r.data0=float4(0);return r;}
#line 2

#define T_host_shared_ T
#define T_host_shared_uniform_ T
#line 2
struct                 T {
         T_union0 union0;
#line 38
};

#ifndef GPU_METAL
T T_ctor_();
uint4 _a(const T this_);
void _a_set_(_ref(T ,this_), uint4 value);
int4 _b(const T this_);
void _b_set_(_ref(T ,this_), int4 value);
float4 _c(const T this_);
void _c_set_(_ref(T ,this_), float4 value);
#endif
#line 2
                                 T T_ctor_() {T r;r.union0=T_union0_ctor_();return r;}
#line 9
uint4 _a(const T this_)       {
  uint4 val;
  val = floatBitsToUint(this_.union0.data0);
  return val;
}
#line 15
void _a_set_(_ref(T ,this_), uint4 value) {
  this_.union0.data0 = uintBitsToFloat(value);
}
#line 19
int4 _b(const T this_)       {
  int4 val;
  val = floatBitsToInt(this_.union0.data0);
  return val;
}
#line 25
void _b_set_(_ref(T ,this_), int4 value) {
  this_.union0.data0 = intBitsToFloat(value);
}
#line 29
float4 _c(const T this_)       {
  float4 val;
  val = this_.union0.data0;
  return val;
}
#line 35
void _c_set_(_ref(T ,this_), float4 value) {
  this_.union0.data0 = value;
}
#line 39
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
struct [[host_shared]] T {
  float2 foo;
  float2 bar;
  union {
    union_t<uint4> a;
  };
  union {
    union_t<uint4> b;
  };
};
)";
    string expect =
        R"(
#line 5

#define T_union0_host_shared_ T_union0
#define T_union0_host_shared_uniform_ T_union0
#line 5
struct                 T_union0 {
  float4 data0;

};
#line 5
                                        T_union0 T_union0_ctor_() {T_union0 r;r.data0=float4(0);return r;}
#line 8

#define T_union1_host_shared_ T_union1
#define T_union1_host_shared_uniform_ T_union1
#line 8
struct                 T_union1 {
  float4 data0;

};
#line 8
                                        T_union1 T_union1_ctor_() {T_union1 r;r.data0=float4(0);return r;}
#line 2

#define T_host_shared_ T
#define T_host_shared_uniform_ T
#line 2
struct                 T {
  float2 foo;
  float2 bar;
         T_union0 union0;
#line 8
         T_union1 union1;
#line 31
};

#ifndef GPU_METAL
T T_ctor_();
uint4 _a(const T this_);
void _a_set_(_ref(T ,this_), uint4 value);
uint4 _b(const T this_);
void _b_set_(_ref(T ,this_), uint4 value);
#endif
#line 2
                                 T T_ctor_() {T r;r.foo=float2(0);r.bar=float2(0);r.union0=T_union0_ctor_();r.union1=T_union1_ctor_();return r;}
#line 12
uint4 _a(const T this_)       {
  uint4 val;
  val = floatBitsToUint(this_.union0.data0);
  return val;
}
#line 18
void _a_set_(_ref(T ,this_), uint4 value) {
  this_.union0.data0 = uintBitsToFloat(value);
}
#line 22
uint4 _b(const T this_)       {
  uint4 val;
  val = floatBitsToUint(this_.union1.data0);
  return val;
}
#line 28
void _b_set_(_ref(T ,this_), uint4 value) {
  this_.union1.data0 = uintBitsToFloat(value);
}
#line 32
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
struct [[host_shared]] B {
  packed_float3 a;
  float b;
};

struct [[host_shared]] A {
  struct B e;
};

struct [[host_shared]] T {
  union {
    union_t<A> a;
  };
};
)";
    string expect = R"(

#define B_host_shared_ B
#define B_host_shared_uniform_ B
#line 2
struct                 B {
  packed_float3 a;
  float b;
};
#line 2
                                 B B_ctor_() {B r;r.a=packed_float3(0);r.b=0.0f;return r;}
#line 8
#define A_host_shared_ A
#define A_host_shared_uniform_ A
#line 7
struct                 A {
         B e;
};
#line 7
                                 A A_ctor_() {A r;r.e=B_ctor_();return r;}
#line 12

#define T_union0_host_shared_ T_union0
#define T_union0_host_shared_uniform_ T_union0
#line 12
struct                 T_union0 {
  float4 data0;

};
#line 12
                                        T_union0 T_union0_ctor_() {T_union0 r;r.data0=float4(0);return r;}
#line 11

#define T_host_shared_ T
#define T_host_shared_uniform_ T
#line 11
struct                 T {
         T_union0 union0;
#line 27
};

#ifndef GPU_METAL
T T_ctor_();
A _a(const T this_);
void _a_set_(_ref(T ,this_), A value);
#endif
#line 11
                                 T T_ctor_() {T r;r.union0=T_union0_ctor_();return r;}
#line 16
A _a(const T this_)       {
  A val;
  val.e.a = this_.union0.data0.xyz;
  val.e.b = this_.union0.data0.w;
  return val;
}
#line 23
void _a_set_(_ref(T ,this_), A value) {
  this_.union0.data0.xyz = value.e.a;
  this_.union0.data0.w = value.e.b;
}
#line 28
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
struct [[host_shared]] T {
  union {
    union_t<float4x4> a;
  };
};
)";
    string expect = R"(
#line 3

#define T_union0_host_shared_ T_union0
#define T_union0_host_shared_uniform_ T_union0
#line 3
struct                 T_union0 {
  float4 data0;
  float4 data1;
  float4 data2;
  float4 data3;

};
#line 3
                                        T_union0 T_union0_ctor_() {T_union0 r;r.data0=float4(0);r.data1=float4(0);r.data2=float4(0);r.data3=float4(0);return r;}
#line 2

#define T_host_shared_ T
#define T_host_shared_uniform_ T
#line 2
struct                 T {
         T_union0 union0;
#line 22
};

#ifndef GPU_METAL
T T_ctor_();
float4x4 _a(const T this_);
void _a_set_(_ref(T ,this_), float4x4 value);
#endif
#line 2
                                 T T_ctor_() {T r;r.union0=T_union0_ctor_();return r;}
#line 7
float4x4 _a(const T this_)       {
  float4x4 val;
  val[0] = this_.union0.data0;
  val[1] = this_.union0.data1;
  val[2] = this_.union0.data2;
  val[3] = this_.union0.data3;
  return val;
}
#line 16
void _a_set_(_ref(T ,this_), float4x4 value) {
  this_.union0.data0 = value[0];
  this_.union0.data1 = value[1];
  this_.union0.data2 = value[2];
  this_.union0.data3 = value[3];
}
#line 23
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
struct [[host_shared]] T {
  union {
    uint a;
  };
};
)";
    string error;
    process_test_string(input, error);
    EXPECT_EQ(error,
              "All union members must have their type wrapped using the union_t<T> template.");
  }
}
GPU_TEST(preprocess_union);

static void test_preprocess_unroll()
{
  using namespace shader;
  using namespace std;

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
    string output = process_test_string(input, error);
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
    string output = process_test_string(input, error);
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
    string output = process_test_string(input, error);
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
    string output = process_test_string(input, error);
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
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(for (; i < j;) [[unroll_n(2)]] { break; })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Unrolled loop cannot contain \"break\" statement.");
  }
  {
    string input = R"(for (; i < j;) [[unroll_n(2)]] { continue; })";
    string error;
    string output = process_test_string(input, error);
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
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(for (int i = 3; i > 2; i++) [[unroll]] {})";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Unsupported condition in unrolled loop.");
  }
}
GPU_TEST(preprocess_unroll);

static void test_preprocess_template()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(
template<typename T>
void func(T a) {a;}
template void func<float>(float a);
)";
    string expect = R"(
#line 3
void func(float a) {a;}
#line 5
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
template<typename T, int i>
void func(T a) {
  a;
}
template void func<float, 1>(float a);
)";
    string expect = R"(
#line 3
void funcTfloatT1(float a) {
  a;
}
#line 7
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
template<enum E e, char i> E func() { return E(e + i); }
template E func<v, 2>();
)";
    string expect = R"(
E funcTvT2() { return E(v + 2); }
#line 4
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
template<> void func<T, Q>(T a) {a}
)";
    string expect = R"(
           void funcTTTQ(T a) {a}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
template<typename T, int i = 0> void func(T a) {a;}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Default arguments are not supported inside template declaration");
  }
  {
    string input = R"(
template void func(float a);
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Template instantiation unsupported syntax");
  }
  {
    string input = R"(func<float, 1>(a);)";
    string expect = R"(funcTfloatT1(a);)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(a.template func<float, 1>(a);)";
    string expect = R"(a.         funcTfloatT1(a);)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(this->template func<float, 1>(a);)";
    string expect = R"(this_.funcTfloatT1(a);)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_template);

static void test_preprocess_template_struct()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(
template<typename T>
struct A { T a; };
template struct A<float>;
)";
    string expect = R"(
#line 3
struct ATfloat {                                                              float a; };
#line 3
                       ATfloat ATfloat_ctor_() {ATfloat r;r.a=0.0f;return r;}
#line 5
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
template<> struct A<float>{
    float a;
};
)";
    string expect = R"(
           struct ATfloat{
    float a;
};
#line 2
                                 ATfloat ATfloat_ctor_() {ATfloat r;r.a=0.0f;return r;}
#line 5
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
void func(A<float> a) {}
)";
    string expect = R"(
void func(ATfloat a) {}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_template_struct);

static void test_preprocess_reference()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(void func() { auto &a = b; a.a = 0; c = a(a); a_c_a = a; })";
    string expect = R"(void func() {              b.a = 0; c = a(b); a_c_a = b; })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(void func() { const int &a = b; a.a = 0; c = a(a); })";
    string expect = R"(void func() {                   b.a = 0; c = a(b); })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(void func() { const int i = 0; auto &a = b[i]; a.a = 0; })";
    string expect = R"(void func() { const int i = 0;                 b[i].a = 0; })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(void func() { auto &a = b(0); })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Reference definitions cannot contain function calls.");
  }
  {
    string input = R"(void func() { int i = 0; auto &a = b[i++]; })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Reference definitions cannot have side effects.");
  }
  {
    string input = R"(void func() { auto &a = b[0 + 1]; })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error,
              "Array subscript inside reference declaration must be a single variable or a "
              "constant, not an expression.");
  }
  {
    string input = R"(void func() { auto &a = b[c]; })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error,
              "Cannot locate array subscript variable declaration. "
              "If it is a global variable, assign it to a temporary const variable for "
              "indexing inside the reference.");
  }
  {
    string input = R"(void func() { int c = 0; auto &a = b[c]; })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Array subscript variable must be declared as const qualified.");
  }
  {
    string input = R"(auto &a = b;)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Reference is defined inside a global or unterminated scope.");
  }
}
GPU_TEST(preprocess_reference);

static void test_preprocess_cleanup()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(
#line 2
int b = 0;          
            
#if 0
           
int a = 1;
#elif 1
#line 321
#line 321
int a = 0;          
#endif
)";
    string expect = R"(
int b = 0;

#if 0
#elif 1
#line 321
int a = 0;
#endif
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_cleanup);

static void test_preprocess_default_arguments()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(
int func(int a, int b = 0)
{
  return a + b;
}
)";
    string expect = R"(
int func(int a, int b    )
{
  return a + b;
}
#line 2
int func(int a)
{
#line 2
  return func(a, 0);
}
#line 6
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
int func(int a = 0, const int b = 0)
{
  return a + b;
}
)";
    string expect = R"(
int func(int a    , const int b    )
{
  return a + b;
}
#line 2
int func(int a)
{
#line 2
  return func(a, 0);
}
#line 2
int func()
{
#line 2
  return func(0);
}
#line 6
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
int2 func(int2 a = int2(0, 0)) {
  return a;
}
)";
    string expect = R"(
int2 func(int2 a             ) {
  return a;
}
#line 2
int2 func()
{
#line 2
  return func(int2(0, 0));
}
#line 5
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
void func(int a = 0) {
  a;
}
)";
    string expect = R"(
void func(int a    ) {
  a;
}
#line 2
void func()
{
#line 2
  func(0);
}
#line 5
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_default_arguments);

static void test_preprocess_srt_template_wrapper()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(
struct SRT {
  [[resource_table]] srt_t<T> a;
};
)";
    string expect = R"(
#define access_SRT_a() T_new_()
#ifdef CREATE_INFO_RES_PASS_SRT
CREATE_INFO_RES_PASS_SRT
#endif
#ifdef CREATE_INFO_RES_BATCH_SRT
CREATE_INFO_RES_BATCH_SRT
#endif
#ifdef CREATE_INFO_RES_GEOMETRY_SRT
CREATE_INFO_RES_GEOMETRY_SRT
#endif
#ifdef CREATE_INFO_RES_SHARED_VARS_SRT
CREATE_INFO_RES_SHARED_VARS_SRT
#endif
#line 2
struct SRT {
                           T  a;
#line 12
};

#ifndef GPU_METAL
SRT SRT_ctor_();
SRT SRT_new_();
#endif
#line 2
                   SRT SRT_ctor_() {SRT r;r.a=T_ctor_();return r;}
#line 5
       SRT SRT_new_()
{
  SRT result;
  result.a = T_new_();
  return result;
#line 3
}
#line 5
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
struct SRT {
  [[resource_table]] T a;
};
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error,
              "Members declared with the [[resource_table]] attribute must wrap their type "
              "with the srt_t<T> template.");
  }
  {
    string input = R"(
struct SRT {
  srt_t<T> a;
};
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error,
              "The srt_t<T> template is only to be used with members declared with the "
              "[[resource_table]] attribute.");
  }
  {
    string input = R"(
struct SRT {
  [[resource_table]] srt_t<T> a[4];
};
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "[[resource_table]] members cannot be arrays.");
  }
}
GPU_TEST(preprocess_srt_template_wrapper);

static void test_preprocess_srt_method()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(
struct SRT {
  [[resource_table]] srt_t<T> a;

  void method(int t) {
    this->a;
  }
};
)";
    string expect = R"(
#define access_SRT_a() T_new_()
#ifdef CREATE_INFO_RES_PASS_SRT
CREATE_INFO_RES_PASS_SRT
#endif
#ifdef CREATE_INFO_RES_BATCH_SRT
CREATE_INFO_RES_BATCH_SRT
#endif
#ifdef CREATE_INFO_RES_GEOMETRY_SRT
CREATE_INFO_RES_GEOMETRY_SRT
#endif
#ifdef CREATE_INFO_RES_SHARED_VARS_SRT
CREATE_INFO_RES_SHARED_VARS_SRT
#endif
#line 2
struct SRT {
                           T  a;
#line 16
};

#ifndef GPU_METAL
SRT SRT_ctor_();
void _method(SRT  this_, int t);
SRT SRT_new_();
#endif
#line 2
                   SRT SRT_ctor_() {SRT r;r.a=T_ctor_();return r;}
#line 5

#if defined(CREATE_INFO_SRT)
#line 5
  void _method(SRT  this_, int t) {
    srt_access(SRT, a);
  }
#endif
       SRT SRT_new_()
{
  SRT result;
  result.a = T_new_();
  return result;
#line 7
}
#line 9
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_srt_method);

static void test_preprocess_static_branch()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(
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
}
)";
    string expect = R"(

#if defined(CREATE_INFO_Resources)
#line 2
void func(Resources  srt)
{

#if SRT_CONSTANT_use_color_band
#line 4
                                                               {
    test;
  }
#endif

#if SRT_CONSTANT_use_color_band == 1
#line 8
                                                                    {
    test;
  }
#else
#line 10
         {
    test;
  }
#endif

#if SRT_CONSTANT_use_color_band
#line 14
                                                               {
    test;
  }
#elif SRT_CONSTANT_use_color_band
#line 16
                                                                      {
    test;
  }
#endif

#if SRT_CONSTANT_use_color_band
#line 20
                                                               {
    test;
  }
#elif SRT_CONSTANT_use_color_band
#line 22
                                                                      {
    test;
  }
#else
#line 24
         {
    test;
  }

#endif
#line 27
}

#endif
#line 28
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
    EXPECT_EQ(error, "Expecting compilation or specialization constant.");
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
GPU_TEST(preprocess_static_branch);

static void test_preprocess_namespace()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(
namespace A {
struct S {};
int func(int a)
{
  S s;
  return B::func(int a);
}
int func2(int a)
{
  T s;
  s.S;
  s.func;
  return func(int a);
}
}
)";
    string expect = R"(

struct A_S {                                                 int _pad;};
#line 3
                   A_S A_S_ctor_() {A_S r;r._pad=0;return r;}
int A_func(int a)
{
  A_S s;
  return B_func(int a);
}
int A_func2(int a)
{
  T s;
  s.S;
  s.func;
  return A_func(int a);
}

)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
namespace A::B {
int func(int a)
{
  return a;
}
int func2(int a)
{
  return func(int a);
}
}
)";
    string expect = R"(

int A_B_func(int a)
{
  return a;
}
int A_B_func2(int a)
{
  return A_B_func(int a);
}

)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
namespace A {
void a() {}
namespace B {
void b() { a(); }
}
void f() { B::b(); }
}
)";
    string expect = R"(

void A_a() {}

void A_B_b() { A_a(); }

void A_f() { A_B_b(); }

)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
namespace A {
int test(int a) {}
int func(int a)
{
  using B::test;
  return test(a);
}
}
)";
    string expect = R"(

int A_test(int a) {}
int A_func(int a)
{

  return B_test(a);
}

)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
int func(int a)
{
  using B = A::S;
  B b;
  using C = A::F;
  C f = A::B();
  f = B();
  B d;
}
)";
    string expect = R"(
int func(int a)
{

  A_S b;

  A_F f = A_B();
  f = B();
  A_S d;
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
namespace A::B {
void func() {}
struct S {};
}
namespace A::B {
using A::B::func;
using S = A::B::S;
void test() {
  S s;
  func();
}
}
)";
    string expect = R"(

void A_B_func() {}
struct A_B_S {                                                       int _pad;};
#line 4
                     A_B_S A_B_S_ctor_() {A_B_S r;r._pad=0;return r;}
#line 9
void A_B_test() {
  A_B_S s;
  A_B_func();
}

)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
using B = A::T;
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "The `using` keyword is not allowed in global scope.");
  }
  {
    string input = R"(
namespace A {
using namespace B;
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error,
              "Unsupported `using namespace`. "
              "Add individual `using` directives for each needed symbol.");
  }
  {
    string input = R"(
namespace A {
using B::func;
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error,
              "The `using` keyword is only allowed in namespace scope to make visible symbols "
              "from the same namespace declared in another scope, potentially from another "
              "file.");
  }
  {
    string input = R"(
namespace A {
using C = B::func;
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error,
              "The `using` keyword is only allowed in namespace scope to make visible symbols "
              "from the same namespace declared in another scope, potentially from another "
              "file.");
  }
  {
    /* Template on the same line as function signature inside a namespace.
     * Template instantiation with other functions. */
    string input = R"(
namespace NS {
template<typename T> T read(T a)
{
  return a;
}
template float read<float>(float);
float write(float a){ return a; }
}
)";

    string expect = R"(
#line 3
float NS_read(float a)
{
  return a;
}
#line 8
float NS_write(float a){ return a; }

)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    /* Struct with member function inside namespace. */
    string input = R"(
namespace NS {
struct S {
  static S static_method(S s) {
    return S(0);
  }
  S other_method(int s) {
    this->some_method();
    return S(0);
  }
};
} // End of namespace
)";

    string expect = R"(

struct NS_S {
#line 11
int _pad;};
#line 14
#ifndef GPU_METAL
NS_S NS_S_ctor_();
NS_S NS_S_static_method(NS_S s);
NS_S _other_method(_ref(NS_S ,this_), int s);
#endif
#line 3
                    NS_S NS_S_ctor_() {NS_S r;r._pad=0;return r;}
         NS_S NS_S_static_method(NS_S s) {
    return NS_S(0);
  }
  NS_S _other_method(_ref(NS_S ,this_), int s) {
    _some_method(this_);
    return NS_S(0);
  }
#line 13
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_namespace);

static void test_preprocess_swizzle()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(a.xyzw().aaa().xxx().grba().yzww; aaaa();)";
    string expect = R"(a.xyzw  .aaa  .xxx  .grba  .yzww; aaaa();)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_swizzle);

static void test_preprocess_enum()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(
enum class enum_class : int {
  VALUE = 0,
};
)";
    string expect = R"(
#line 3
constant static constexpr int enum_class_VALUE = 0;

#define enum_class int
#line 2

enum_class enum_class_ctor_() { return enum_class(0); }
#line 2



)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }

  {
    string input = R"(
enum E : int { A, B = 2, C, D = 1, E };
)";
    string expect = R"(
constant static constexpr int A = 0;
#line 2
constant static constexpr int B = 2;
#line 2
constant static constexpr int C = B + 1;
#line 2
constant static constexpr int D = 1;
#line 2
constant static constexpr int E = D + 1;

#define E int
#line 2

E E_ctor_() { return E(0); }
#line 2

)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
enum class enum_class : int {
  VALUE = 0,
};
)";
    string expect = R"(
#line 3
constant static constexpr int enum_class_VALUE = 0;

#define enum_class int
#line 2

enum_class enum_class_ctor_() { return enum_class(0); }
#line 2



)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
enum class enum_class {
  VALUE = 0,
};
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "enum declaration must explicitly use an underlying type");
  }
}
GPU_TEST(preprocess_enum);

#ifdef __APPLE__ /* This processing is only done for metal compatibility. */
static void test_preprocess_matrix_constructors()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(mat3(a); mat3 a; my_mat4x4(a); mat2x2(a); mat3x2(a);)";
    string expect = R"(__mat3x3(a); mat3 a; my_mat4x4(a); __mat2x2(a); mat3x2(a);)";
    string error;
    string output = process_test_string(input, error, nullptr, Language::GLSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_matrix_constructors);
#endif

static void test_preprocess_resource_guard()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(
void my_func() {
  interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_index;
}
)";
    string expect = R"(
void my_func() {

#if defined(CREATE_INFO_draw_resource_id_varying)
#line 3
  interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_index;

#endif
#line 4
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
uint my_func() {
  uint i = 0;
  i += interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_index;
  return i;
}
)";
    string expect = R"(
uint my_func() {

#if defined(CREATE_INFO_draw_resource_id_varying)
#line 3
  uint i = 0;
  i += interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_index;
  return i;

#else
#line 3
  return uint(0);
#endif
#line 6
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
uint my_func() {
  uint i = 0;
  {
    i += interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_index;
  }
  return i;
}
)";
    string expect = R"(
uint my_func() {
  uint i = 0;
  {

#if defined(CREATE_INFO_draw_resource_id_varying)
#line 5
    i += interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_index;

#endif
#line 6
  }
  return i;
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
uint my_func() {
  uint i = 0;
  {
    i += interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_index;
    i += buffer_get(draw_resource_id, resource_id_buf)[0];
  }
  return i;
}
)";
    string expect = R"(
uint my_func() {
  uint i = 0;
  {

#if defined(CREATE_INFO_draw_resource_id_varying)
#line 5

#if defined(CREATE_INFO_draw_resource_id)
#line 5
    i += interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_index;
    i += buffer_get(draw_resource_id, resource_id_buf)[0];

#endif

#endif
#line 7
  }
  return i;
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    /* Guard in template. */
    string input = R"(
template<> uint my_func<uint>(uint i) {
  return buffer_get(draw_resource_id, resource_id_buf)[i];
}
)";
    string expect = R"(
           uint my_funcTuint(uint i) {

#if defined(CREATE_INFO_draw_resource_id)
#line 3
  return buffer_get(draw_resource_id, resource_id_buf)[i];

#else
#line 3
  return uint(0);
#endif
#line 4
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_resource_guard);

static void test_preprocess_empty_struct()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(
class S {};
struct T {};
struct U {
  static void fn() {}
};
)";
    string expect = R"(
struct S {                                           int _pad;};
#line 2
                 S S_ctor_() {S r;r._pad=0;return r;}
struct T {                                           int _pad;};
#line 3
                 T T_ctor_() {T r;r._pad=0;return r;}
struct U {

int _pad;};

#ifndef GPU_METAL
U U_ctor_();
void U_fn();
#endif
#line 4
                 U U_ctor_() {U r;r._pad=0;return r;}
         void U_fn() {}
#line 7
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_empty_struct);

static void test_preprocess_struct_methods()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(
class S {
 private:
  int member;
  int this_member;

 public:
  static S construct()
  {
    S a;
    a.member = 0;
    a.this_member = 0;
    return a;
  }

  int another_member;

  S function(int i)
  {
    this->member = i;
    this_member++;
    return *this;
  }

  int size() const
  {
    return this->member;
  }
};

void main()
{
  S s = S::construct();
  f.f();
  f(0).f();
  f().f();
  l.o.t();
  l.o(0).t();
  l.o().t();
  l[0].o();
  l.o[0].t();
  l.o().t[0];
}
)";
    string expect = R"(
struct S {

  int member;
  int this_member;
#line 16
  int another_member;
#line 29
};
#line 32
#ifndef GPU_METAL
S S_ctor_();
S S_construct();
S _function(_ref(S ,this_), int i);
int _size(const S this_);
#endif
#line 2
                 S S_ctor_() {S r;r.member=0;r.this_member=0;r.another_member=0;return r;}
#line 8
         S S_construct()
  {
    S a;
    a.member = 0;
    a.this_member = 0;
    return a;
  }
#line 18
  S _function(_ref(S ,this_), int i)
  {
    this_.member = i;
    this_.this_member++;
    return this_;
  }
#line 25
  int _size(const S this_)
  {
    return this_.member;
  }
#line 31
void main()
{
  S s = S_construct();
  _f(f);
  _f(f(0));
  _f(f());
  _t(l.o);
  _t(_o(l, 0));
  _t(_o(l));
  _o(l[0]);
  _t(l.o[0]);
  _o(l).t[0];
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
struct A {
  int a;
  uint b;
  float fn1() { return a; }
  float fn2() { int fn2; return fn1(); }
  static float fn3() { int a; return a; }
};
)";
    string expect = R"(
struct A {
  int a;
  uint b;
#line 8
};

#ifndef GPU_METAL
A A_ctor_();
float _fn1(_ref(A ,this_));
float _fn2(_ref(A ,this_));
float A_fn3();
#endif
#line 2
                 A A_ctor_() {A r;r.a=0;r.b=0u;return r;}
#line 5
  float _fn1(_ref(A ,this_)) { return this_.a; }
  float _fn2(_ref(A ,this_)) { int fn2; return _fn1(this_); }
         float A_fn3() { int a; return a; }
#line 9
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
struct A {
  int a;
  float fn1(int a) { return a; }
};
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Class member shadowing.");
  }
  {
    string input = R"(
struct A {
  int a;
  float fn1() { int a; return a; }
};
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Class member shadowing.");
  }
  {
    string input = R"(
class S {
  int xzwy() const
  {
  }
};
)";
    string expect = R"(
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Method name matching swizzles accessor are forbidden.");
  }
}
GPU_TEST(preprocess_struct_methods);

static void test_preprocess_srt_mutations()
{
  using namespace std;
  using namespace shader::parser;

  report_callback no_err_report = [](int, int, string, const char *) {};

  {
    string input = R"(
float fn([[resource_table]] SRT &srt) {
  return srt.member;
}
)";
    string expect = R"(

#if defined(CREATE_INFO_SRT)
#line 2
float fn(SRT  srt) {
  return srt_access(SRT, member);
}

#endif
#line 5
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
float fn([[resource_table]] SRT srt) {
  return srt.member;
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Shader Resource Table arguments must be references.");
  }
  {
    string input = R"(
float fn([[resource_table]] SRT &srt) {
  [[resource_table]] OtherSRT &other_srt = srt.other_srt;
  return other_srt.member;
}
)";
    string expect = R"(

#if defined(CREATE_INFO_SRT)
#line 2
float fn(SRT  srt) {

  return srt_access(OtherSRT, member);
}

#endif
#line 6
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_srt_mutations);

static void test_preprocess_entry_point_resources()
{
  using namespace std;
  using namespace shader::parser;

  report_callback no_err_report = [](int, int, string, const char *) {};

  {
    string input = R"(
namespace ns {

struct VertOut {
  [[smooth]] float3 local_pos;
};

struct FragOut {
  [[frag_color(0)]] float3 color;
  [[frag_color(1), index(2)]] uint test;
};

template<typename T>
struct VertIn {
  [[attribute(0)]] T pos;
};
template struct VertIn<float>;


[[vertex]] void vertex_function([[resource_table]] Resources &srt,
                                [[in]] const VertIn<float> &v_in,
                                [[out, condition(cond)]] VertOut &v_out,
                                [[base_instance]] const int &base_instance,
                                [[point_size]] float &point_size,
                                [[clip_distance]] float (&clip_distance)[6],
                                [[layer]] int &layer,
                                [[viewport_index]] int &viewport_index,
                                [[position]] float4 &out_position)
{
  base_instance;
  point_size;
  clip_distance;
  layer;
  viewport_index;
  out_position;
}

[[fragment]] void fragment_function([[resource_table]] Resources &srt,
                                    [[in, condition(cond)]] const VertOut &v_out,
                                    [[out]] FragOut &frag_out,
                                    [[frag_depth(greater)]] float depth,
                                    [[frag_stencil_ref]] int stencil,
                                    [[layer]] const int &layer,
                                    [[viewport_index]] const int &viewport_index,
                                    [[point_coord]] const float2 pt_co,
                                    [[front_facing]] const bool facing,
                                    [[frag_coord]] const float4 in_position)
{
  layer;
  viewport_index;
  depth;
  stencil;
  in_position;
  pt_co;
  facing;
}

[[compute]] void compute_function([[resource_table]] Resources &srt,
                                  [[global_invocation_id]] const uint3 &global_invocation_id,
                                  [[local_invocation_id]] const uint3 &local_invocation_id,
                                  [[local_invocation_index]] const uint &local_invocation_index,
                                  [[work_group_id]] const uint3 &workgroup_id,
                                  [[num_work_groups]] const uint3 &num_work_groups)
{
  global_invocation_id;
  local_invocation_id;
  local_invocation_index;
  workgroup_id;
  num_work_groups;
}

}
)";
    string expect = R"(
#line 4
struct ns_VertOut {
             float3 local_pos;
};
#line 4
                          ns_VertOut ns_VertOut_ctor_() {ns_VertOut r;r.local_pos=float3(0);return r;}
#line 8
struct ns_FragOut {
                    float3 color;
                              uint test;
};
#line 8
                          ns_FragOut ns_FragOut_ctor_() {ns_FragOut r;r.color=float3(0);r.test=0u;return r;}
#line 14
struct ns_VertInTfloat {
                   float pos;
};
#line 14
                               ns_VertInTfloat ns_VertInTfloat_ctor_() {ns_VertInTfloat r;r.pos=0.0f;return r;}
#line 20

#if defined(CREATE_INFO_Resources)
#line 20

#if defined(ENTRY_POINT_ns_vertex_function)
#line 20
           void ns_vertex_function(
#line 28
                                                                 )
{
#if defined(GPU_VERTEX_SHADER)
#line 29
  Resources srt = Resources_ctor_();
  gl_BaseInstance;
  gl_PointSize;
  gl_ClipDistance;
  gl_Layer;
  gl_ViewportIndex;
  gl_Position;

#endif
#line 36
}
#endif
#endif
#line 38

#if defined(CREATE_INFO_Resources)
#line 38

#if defined(ENTRY_POINT_ns_fragment_function)
#line 38
             void ns_fragment_function(
#line 47
                                                                           )
{
#if defined(GPU_FRAGMENT_SHADER)
#line 48
  Resources srt = Resources_ctor_();
  gl_Layer;
  gl_ViewportIndex;
  gl_FragDepth;
  gl_FragStencilRefARB;
  gl_FragCoord;
  gl_PointCoord;
  gl_FrontFacing;

#endif
#line 56
}
#endif
#endif
#line 58

#if defined(CREATE_INFO_Resources)
#line 58

#if defined(ENTRY_POINT_ns_compute_function)
#line 58
            void ns_compute_function(
#line 63
                                                                                  )
{
#if defined(GPU_COMPUTE_SHADER)
#line 64
  Resources srt = Resources_ctor_();
  gl_GlobalInvocationID;
  gl_LocalInvocationID;
  gl_LocalInvocationIndex;
  gl_WorkGroupID;
  gl_NumWorkGroups;

#endif
#line 70
}
#endif
#endif
)";
    string expect_infos = R"(#pragma once



GPU_SHADER_CREATE_INFO(ns_VertInTfloat)
VERTEX_IN(0, float, pos)
GPU_SHADER_CREATE_END()


GPU_SHADER_CREATE_INFO(ns_FragOut)
FRAGMENT_OUT(0, float3, ns_FragOut_color)
FRAGMENT_OUT_DUAL(1, uint, ns_FragOut_test, 2)
GPU_SHADER_CREATE_END()


GPU_SHADER_INTERFACE_INFO(ns_VertOut_t)
SMOOTH(float3, ns_VertOut_local_pos)
GPU_SHADER_INTERFACE_END()



GPU_SHADER_CREATE_INFO(ns_vertex_function_infos_)
ADDITIONAL_INFO(Resources)
ADDITIONAL_INFO(ns_VertInTfloat)
VERTEX_OUT(ns_VertOut_t)
BUILTINS(BuiltinBits::POINT_SIZE)
BUILTINS(BuiltinBits::LAYER)
BUILTINS(BuiltinBits::VIEWPORT_INDEX)
BUILTINS(BuiltinBits::CLIP_DISTANCES)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(ns_fragment_function_infos_)
DEPTH_WRITE(GREATER)
BUILTINS(BuiltinBits::STENCIL_REF)
BUILTINS(BuiltinBits::POINT_COORD)
BUILTINS(BuiltinBits::FRONT_FACING)
BUILTINS(BuiltinBits::FRAG_COORD)
ADDITIONAL_INFO(Resources)
ADDITIONAL_INFO(ns_FragOut)
BUILTINS(BuiltinBits::LAYER)
BUILTINS(BuiltinBits::VIEWPORT_INDEX)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(ns_compute_function_infos_)
ADDITIONAL_INFO(Resources)
BUILTINS(BuiltinBits::GLOBAL_INVOCATION_ID)
BUILTINS(BuiltinBits::LOCAL_INVOCATION_ID)
BUILTINS(BuiltinBits::LOCAL_INVOCATION_INDEX)
BUILTINS(BuiltinBits::WORK_GROUP_ID)
BUILTINS(BuiltinBits::NUM_WORK_GROUP)
GPU_SHADER_CREATE_END()

)";
    string error;
    shader::metadata::Source metadata;
    string output = process_test_string(input, error, &metadata);
    string infos = metadata.serialize_infos();

    EXPECT_EQ(output, expect);
    EXPECT_EQ(infos, expect_infos);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_entry_point_resources);

static void test_preprocess_pipeline_description()
{
  using namespace std;
  using namespace shader::parser;

  report_callback no_err_report = [](int, int, string, const char *) {};

  {
    string input = R"(
namespace ns {

PipelineGraphic graphic_pipe(vertex_func, fragment_func, Type{.a = true, .b = 9, .c = 3u});
PipelineCompute compute_pipe(compute_func, Type{.a = true, .b = 8, .c = 7u});

}
)";
    string expect = R"(






)";
    string expect_infos = R"(#pragma once







GPU_SHADER_CREATE_INFO(ns_graphic_pipe)
GRAPHIC_SOURCE("test.bsl")
VERTEX_FUNCTION("vertex_func")
FRAGMENT_FUNCTION("fragment_func")
ADDITIONAL_INFO(vertex_func_infos_)
ADDITIONAL_INFO(fragment_func_infos_)
COMPILATION_CONSTANT(bool, a, true)
COMPILATION_CONSTANT(int, b, 9)
COMPILATION_CONSTANT(uint, c, 3u)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(ns_compute_pipe)
COMPUTE_SOURCE("test.bsl")
COMPUTE_FUNCTION("compute_func")
ADDITIONAL_INFO(compute_func_infos_)
COMPILATION_CONSTANT(bool, a, true)
COMPILATION_CONSTANT(int, b, 8)
COMPILATION_CONSTANT(uint, c, 7u)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

)";
    string error;
    shader::metadata::Source metadata;
    string output = process_test_string(input, error, &metadata);
    string infos = metadata.serialize_infos();

    EXPECT_EQ(output, expect);
    EXPECT_EQ(infos, expect_infos);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_pipeline_description);

static void test_preprocess_initializer_list()
{
  using namespace std;
  using namespace shader::parser;

  {
    string input = R"(
T fn1() { return T{1, 2}; }
T fn2() { return T{1, 2, }; }
T fn3() { return T{.a=1, .b=2}; }
T fn4() { return T{.a=1, .b=2, }; }
T fn5() { return {1, 2}; }
T fn6() { return {1, 2, }; }
T fn7() { return {.a=1, .b=2}; }
T fn8() { return {.a=1, .b=2, }; }
void fn() {
  T t1=T{1, 2};
  T t2=T{1, 2, };
  T t3=T{.a=1, .b=2};
  T t4=T{.a=1, .b=2, };
  T t5={1, 2};
  T t6={1, 2, };
  T t7={.a=1, .b=2};
  T t8={.a=1, .b=2, };
  T t9=T{.a=1, .b=T{0, 2}.x};
  T t10=T{1, T{0, 2}.x};
}
)";
    string expect = R"(
T fn1() { return _ctor(T) 1, 2 _rotc() ; }
T fn2() { return _ctor(T) 1, 2   _rotc() ; }
T fn3() { {T _tmp ;    _tmp.a=1;  _tmp.b=2;   return T_tmp;}; }
T fn4() { {T _tmp ;    _tmp.a=1;  _tmp.b=2  ;   return T_tmp;}; }
T fn5() { return _ctor(T) 1, 2 _rotc() ; }
T fn6() { return _ctor(T) 1, 2   _rotc() ; }
T fn7() { {T _tmp ;    _tmp.a=1;  _tmp.b=2;   return _tmp;}; }
T fn8() { {T _tmp ;    _tmp.a=1;  _tmp.b=2  ;   return _tmp;}; }
void fn() {
  T t1=_ctor(T) 1, 2 _rotc() ;
  T t2=_ctor(T) 1, 2   _rotc() ;
  T t3;   t3.a=1;  t3.b=2;
  T t4;   t4.a=1;  t4.b=2  ;
  T t5=_ctor(T) 1, 2 _rotc() ;
  T t6=_ctor(T) 1, 2   _rotc() ;
  T t7;   t7.a=1;  t7.b=2;
  T t8;   t8.a=1;  t8.b=2  ;
  T t9;   t9.a=1;  t9.b=_ctor(T) 0, 2 _rotc() .x;
  T t10=_ctor(T) 1, _ctor(T) 0, 2 _rotc() .x _rotc() ;
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
void fn() {
  T t9={1, T{.a=1, .b=2}.a};
}
)";
    string error;
    shader::metadata::Source metadata;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Designated initializers are only supported in assignments");
  }
  {
    string input = R"(
void fn() {
  T t10={1, float4{0}};
}
)";
    string error;
    shader::metadata::Source metadata;
    string output = process_test_string(input, error);
    EXPECT_EQ(
        error,
        "Aggregate is error prone for built-in vector and matrix types, use constructors instead");
  }
  {
    string input = R"(
void fn() {
  T t11={.a=1, .b=T{.a=1, .b=2}.a};
}
)";
    string error;
    shader::metadata::Source metadata;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Nested initializer lists are not supported");
  }
  {
    string input = R"(
void fn() {
  T t12={.a=1, .b=float4{0}};
}
)";
    string error;
    shader::metadata::Source metadata;
    string output = process_test_string(input, error);
    EXPECT_EQ(
        error,
        "Aggregate is error prone for built-in vector and matrix types, use constructors instead");
  }
}
GPU_TEST(preprocess_initializer_list);

static void test_preprocess_parser()
{
  using namespace std;
  using namespace shader::parser;

  using IntermediateForm = IntermediateForm<FullLexer, FullParser>;

  report_callback no_err_report = [](int, int, string, const char *) {};

  {
    string input = R"(
1;
1.0;
2e10;
2e10f;
2.e10f;
2.0e-1f;
2.0e-1;
2.0e-1f;
0xFF;
0xFFu;
0+8;
)";
    string expect = R"(
1;1;1;1;1;1;1;1;1;1;1+1;)";
    EXPECT_EQ(IntermediateForm(input, no_err_report).data_get().lex.token_types_str, expect);
  }
  {
    string input = R"(
[[a(0,1,b), c, d(t)]]
)";
    string expect = R"(
[[A(1,1,A),A,A(A)]])";
    string scopes = R"(GABbcmmmbbcm)";
    EXPECT_EQ(IntermediateForm(input, no_err_report).data_get().lex.token_types_str, expect);
    EXPECT_EQ(IntermediateForm(input, no_err_report).data_get().scope_types_str, scopes);
  }
  {
    string input = R"(
struct T {
    int t = 1;
};
class B {
    T t;
};
)";
    string expect = R"(
sA{AA=1;};SA{AA;};)";
    EXPECT_EQ(IntermediateForm(input, no_err_report).data_get().lex.token_types_str, expect);
  }
  {
    string input = R"(
namespace T {}
namespace T::U::V {}
)";
    string expect = R"(
nA{}nA::A::A{})";
    string expect_scopes = R"(GNN)";
    EXPECT_EQ(IntermediateForm(input, no_err_report).data_get().lex.token_types_str, expect);
    EXPECT_EQ(IntermediateForm(input, no_err_report).data_get().scope_types_str, expect_scopes);
  }
  {
    string input = R"(
void f(int t = 0) {
  int i = 0, u = 2, v = {1.0f};
  {
    v = i = u, v++;
    if (v == i) {
      return;
    }
  }
}
)";
    string expect = R"(
AA(AA=1){AA=1,A=1,A={1};{A=A=A,AP;i(AEA){r;}}})";
    EXPECT_EQ(IntermediateForm(input, no_err_report).data_get().lex.token_types_str, expect);
  }
  {
    IntermediateForm parser("float i;", no_err_report);
    parser.insert_after(Token::from_position(&parser.data_get(), 0), "A ");
    parser.insert_after(Token::from_position(&parser.data_get(), 0), "B  ");
    EXPECT_EQ(parser.result_get(), "float A B  i;");
  }
  {
    string input = R"(
A
#line 100
B
)";
    IntermediateForm parser(input, no_err_report);
    string expect = R"(
A#A1
A)";
    EXPECT_EQ(parser.data_get().lex.token_types_str, expect);

    Token A = Token::from_position(&parser.data_get(), 1);
    Token B = Token::from_position(&parser.data_get(), 6);

    EXPECT_EQ(A.str(), "A");
    EXPECT_EQ(B.str(), "B");
    EXPECT_EQ(A.line_number(), 2);
    EXPECT_EQ(B.line_number(), 100);
  }
  {
    string input = R"(
const bool foo;
[[a]] int bar[0];
)";

    string expect = R"(
match(, const, bool, , foo, , ;)
match([a], , int, , bar, [0], ;)
)";

    IntermediateForm parser(input, no_err_report);

    string result = "\n";
    parser().foreach_declaration([&](Scope attributes,
                                     Token const_tok,
                                     Token type,
                                     Scope template_scope,
                                     Token name,
                                     Scope array,
                                     Token decl_end) {
      result += "match(";
      result += attributes.str() + ", ";
      result += const_tok.str() + ", ";
      result += type.str() + ", ";
      result += template_scope.str() + ", ";
      result += name.str() + ", ";
      result += array.str() + ", ";
      result += decl_end.str() + ")\n";
    });

    EXPECT_EQ(expect, result);
  }
}
GPU_TEST(preprocess_parser);

static int test_expression(std::string str)
{
  using namespace shader::parser;
  report_callback no_err_report = [](int, int, std::string, const char *) {};
  ExpressionLexer lexer;
  lexer.lexical_analysis(str);
  try {
    return ExpressionParser(lexer).eval();
  }
  catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 9999999;
  }
}

static void test_preprocess_expression_parser()
{
  using namespace std;
  using namespace shader::parser;

  /* --- Basic arithmetic --- */
  EXPECT_EQ(test_expression("1+2+3"), 6);
  EXPECT_EQ(test_expression("1*2+3"), 5);
  EXPECT_EQ(test_expression("1+2*3"), 7);
  EXPECT_EQ(test_expression("10-3-2"), 5);
  EXPECT_EQ(test_expression("10-(3-2)"), 9);
  EXPECT_EQ(test_expression("20/5/2"), 2);

  /* --- Parenthesis --- */
  EXPECT_EQ(test_expression("(1+2)*3"), 9);
  EXPECT_EQ(test_expression("((2+3)*4)"), 20);

  /* --- Unary operators --- */
  EXPECT_EQ(test_expression("-1+2"), 1);
  EXPECT_EQ(test_expression("~0"), ~0);
  EXPECT_EQ(test_expression("!0"), 1);
  EXPECT_EQ(test_expression("!5"), 0);

  /* --- Bitwise operators --- */
  EXPECT_EQ(test_expression("1|2"), 3);
  EXPECT_EQ(test_expression("3&1"), 1);
  EXPECT_EQ(test_expression("1^3"), 2);
  /* Not supported yet. */
  // EXPECT_EQ(test_expression("1 << 3"), 8);
  // EXPECT_EQ(test_expression("8 >> 2"), 2);

  /* --- Bitwise vs arithmetic precedence --- */
  /* Not supported yet. */
  // EXPECT_EQ(test_expression("1 + 2 << 2"), 12); /* (1+2)<<2 */
  // EXPECT_EQ(test_expression("1 << 2 + 1"), 8);  /* 1<<(2+1) */

  /* --- Comparison operators --- */
  EXPECT_EQ(test_expression("1 < 2"), 1);
  EXPECT_EQ(test_expression("2 <= 2"), 1);
  EXPECT_EQ(test_expression("3 > 5"), 0);
  EXPECT_EQ(test_expression("3 != 4"), 1);
  EXPECT_EQ(test_expression("3 == 3"), 1);

  /* --- Logical operators --- */
  EXPECT_EQ(test_expression("1 && 1"), 1);
  EXPECT_EQ(test_expression("1 && 0"), 0);
  EXPECT_EQ(test_expression("0 || 1"), 1);
  EXPECT_EQ(test_expression("0 || 0"), 0);
  EXPECT_EQ(test_expression("0 || 0 || 1"), 1);

  /* --- Logical precedence --- */
  EXPECT_EQ(test_expression("0 || 1 && 0"), 0); /* && before || */
  EXPECT_EQ(test_expression("(0 || 1) && 0"), 0);

  /* --- Ternary operator --- */
  EXPECT_EQ(test_expression("1 ? 2 : 3"), 2);
  EXPECT_EQ(test_expression("0 ? 2 : 3"), 3);
  EXPECT_EQ(test_expression("1 ? 0 ? 2 : 3 : 4"), 3);
  EXPECT_EQ(test_expression("0 ? 1 : 2 ? 3 : 4"), 3);

  /* --- Mixed complex expressions --- */
  EXPECT_EQ(test_expression("(1+2*3) == 7 && (4|1) == 5"), 1);
  EXPECT_EQ(test_expression("!((3<1) == 0)"), 0);
  EXPECT_EQ(test_expression("!0 && !0"), 1);
  EXPECT_EQ(test_expression("!1 && !0"), 0);
  EXPECT_EQ(test_expression("!!1 && !0"), 1);

  /* --- Deep Ternary Nesting --- */
  EXPECT_EQ(test_expression("1 ? 10 + 5 : 20"), 15);
  EXPECT_EQ(test_expression("0 ? 1 : 0 ? 2 : 3"), 3);
  EXPECT_EQ(test_expression("1 ? (0 ? 1 : 2) : 3"), 2);
  EXPECT_EQ(test_expression("10 + (1 ? 5 : 0) * 2"), 20);

  /* --- Unary Chains --- */
  EXPECT_EQ(test_expression("! ~ -1"), 1);
  EXPECT_EQ(test_expression("-5 * -2"), 10);

  /* --- Precedence Boundary Tests --- */
  EXPECT_EQ(test_expression("1 == 1 | 2"), 3);
  EXPECT_EQ(test_expression("1 + 2 < 4"), 1);
  EXPECT_EQ(test_expression("1 | 2 && 0"), 0);

  /* --- Complex Boolean Logic --- */
  EXPECT_EQ(test_expression("!((1 + 2 == 3) && (4 * 5 <= 20) || (0 ? 1 : 0))"), 0);

  /* --- The Kitchen Sink --- */
  EXPECT_EQ(test_expression("(10 - 2 * 3 == 4) ? 50 : 100 + !0"), 50);
}
GPU_TEST(preprocess_expression_parser);

}  // namespace blender::gpu::tests
