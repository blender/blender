/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "shader_tool/shader_tool.hh"

#include "gpu_testing.hh"

namespace blender::gpu::tests {

static std::string process_test_string(std::string str,
                                       std::string &first_error,
                                       shader::metadata::Source *r_metadata = nullptr,
                                       shader::Preprocessor::SourceLanguage language =
                                           shader::Preprocessor::SourceLanguage::BLENDER_GLSL)
{
  using namespace shader;
  Preprocessor preprocessor;
  shader::metadata::Source metadata;
  std::string result = preprocessor.process(
      language,
      str,
      "test.glsl",
      false,
      true,
      [&](int /*err_line*/, int /*err_char*/, const std::string & /*line*/, const char *err_msg) {
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

static void test_preprocess_unroll()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(
[[gpu::unroll]] for (int i = 2; i < 4; i++) { content += i; })";
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
[[gpu::unroll]] for (int i = 2; i < 4; i++, y++) { content += i; })";
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
[[gpu::unroll]] for (int i = 2; i < 4 && i < y; i++, y++) { cont += i; })";
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
[[gpu::unroll(2)]] for (; i < j;) { content += i; })";
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
[[gpu::unroll(2)]] for (; i < j;) { [[gpu::unroll(2)]] for (; j < k;) {} })";
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
    string input = R"([[gpu::unroll(2)]] for (; i < j;) { break; })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Unrolled loop cannot contain \"break\" statement.");
  }
  {
    string input = R"([[gpu::unroll(2)]] for (; i < j;) { continue; })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Unrolled loop cannot contain \"continue\" statement.");
  }
  {
    string input = R"(
[[gpu::unroll(2)]] for (; i < j;) { for (; j < k;) {break;continue;} })";
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
    string input = R"([[gpu::unroll]] for (int i = 3; i > 2; i++) {})";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Unsupported condition in unrolled loop.");
  }
  {
    string input = R"(
[[gpu::unroll_define(2)]] for (int i = 0; i < DEFINE; i++) { a = i; })";
    string expect = R"(

{
#if DEFINE > 0
#line 2
                                                           { a = 0; }
#endif
#if DEFINE > 1
#line 2
                                                           { a = 1; }
#endif
#line 2
                                                                    })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
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
struct ATfloat { float a; };
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
#line 5

#if defined(CREATE_INFO_SRT)
#line 5
  void method(_ref(SRT ,this_), int t) {
    srt_access(SRT, a);
  }

#endif
#line 9
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
void func(_ref(Resources ,srt))
{

#if SRT_CONSTANT_use_color_band
#line 4
                                                               {
    test;
  }
#endif

#if SRT_CONSTANT_use_color_band== 1
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

struct A_S {int _pad;};
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
namespace B {
int func(int a)
{
  return a;
}
}
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Nested namespaces are unsupported.");
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
struct A_B_S {int _pad;};
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
#line 4
         NS_S NS_S_static_method(NS_S s) {
    return NS_S(0);
  }
  NS_S other_method(_ref(NS_S ,this_), int s) {
    some_method(this_);
    return NS_S(0);
  }
#line 12

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
#define enum_class int
constant static constexpr int enum_class_VALUE = 0;
#line 5
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
    string output = process_test_string(input, error, nullptr, Preprocessor::SourceLanguage::GLSL);
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
struct S {int _pad;};
struct T {int _pad;};
struct U {

int _pad;};
#line 5
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
#line 8
         S S_construct()
  {
    S a;
    a.member = 0;
    a.this_member = 0;
    return a;
  }
#line 18
  S function(_ref(S ,this_), int i)
  {
    this_.member = i;
    this_member++;
    return this_;
  }
#line 25
  int size(const S this_)
  {
    return this_.member;
  }
#line 30

void main()
{
  S s = S_construct();
  f(f);
  f(f(0));
  f(f());
  t(l.o);
  t(o(l, 0));
  t(o(l));
  o(l[0]);
  t(l.o[0]);
  o(l).t[0];
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
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
    EXPECT_EQ(error,
              "Method name matching swizzles and vector component "
              "accessor are forbidden.");
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
float fn(_ref(SRT ,srt)) {
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
float fn(_ref(SRT ,srt)) {

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

struct ns_FragOut {
                    float3 color;
                              uint test;
};
#line 14
struct ns_VertInTfloat {
                   float pos;
};
#line 20
           void ns_vertex_function(
#line 28
                                                                 )
{
#if defined(GPU_VERTEX_SHADER)
#line 29
  Resources srt;
  gl_BaseInstance;
  gl_PointSize;
  gl_ClipDistance;
  gl_Layer;
  gl_ViewportIndex;
  gl_Position;

#endif
#line 36
}

             void ns_fragment_function(
#line 47
                                                                           )
{
#if defined(GPU_FRAGMENT_SHADER)
#line 48
  Resources srt;
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

            void ns_compute_function(
#line 63
                                                                                  )
{
#if defined(GPU_COMPUTE_SHADER)
#line 64
  Resources srt;
  gl_GlobalInvocationID;
  gl_LocalInvocationID;
  gl_LocalInvocationIndex;
  gl_WorkGroupID;
  gl_NumWorkGroups;

#endif
#line 70
}


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
GRAPHIC_SOURCE("test.glsl")
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
COMPUTE_SOURCE("test.glsl")
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

static void test_preprocess_parser()
{
  using namespace std;
  using namespace shader::parser;

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
0;0;0;0;0;0;0;0;0;0;0+0;)";
    EXPECT_EQ(IntermediateForm(input, no_err_report).data_get().token_types, expect);
  }
  {
    string input = R"(
[[a(0,1,b), c, d(t)]]
)";
    string expect = R"(
[[w(0,0,w),w,w(w)]])";
    string scopes = R"(GABbcmmmbbcm)";
    EXPECT_EQ(IntermediateForm(input, no_err_report).data_get().token_types, expect);
    EXPECT_EQ(IntermediateForm(input, no_err_report).data_get().scope_types, scopes);
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
sw{ww=0;};Sw{ww;};)";
    EXPECT_EQ(IntermediateForm(input, no_err_report).data_get().token_types, expect);
  }
  {
    string input = R"(
namespace T {}
namespace T::U::V {}
)";
    string expect = R"(
nw{}nw::w::w{})";
    string expect_scopes = R"(GNN)";
    EXPECT_EQ(IntermediateForm(input, no_err_report).data_get().token_types, expect);
    EXPECT_EQ(IntermediateForm(input, no_err_report).data_get().scope_types, expect_scopes);
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
ww(ww=0){ww=0,w=0,w={0};{w=w=w,wP;i(wEw){r;}}})";
    EXPECT_EQ(IntermediateForm(input, no_err_report).data_get().token_types, expect);
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
w#w0
w)";
    EXPECT_EQ(parser.data_get().token_types, expect);

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

}  // namespace blender::gpu::tests
