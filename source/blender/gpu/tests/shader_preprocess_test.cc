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

  string input_reference = "void func(int &a, int (&c)[2]) {{ int &b = a; }} int &b = a;";
  int fn_ref_count = 0, arg_ref_count = 0, global_ref_count = 0;
  Preprocessor::reference_search(
      input_reference, [&](int parenthesis_depth, int bracket_depth, char & /*c*/) {
        if ((parenthesis_depth == 1 || parenthesis_depth == 2) && bracket_depth == 0) {
          arg_ref_count += 1;
        }
        else if (bracket_depth > 0) {
          fn_ref_count += 1;
        }
        else if (bracket_depth == 0 && parenthesis_depth == 0) {
          global_ref_count += 1;
        }
      });
  EXPECT_EQ(arg_ref_count, 2);
  EXPECT_EQ(fn_ref_count, 1);
  EXPECT_EQ(global_ref_count, 1);
}
GPU_TEST(preprocess_utilities);

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

static void test_preprocess_template()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(
template<typename T>
void func(T a) {a;}
template void func<float>(float a);)";
    string expect = R"(
#define func_TEMPLATE(T) \
void func(T a) {a;}
func_TEMPLATE(float)/*float a*/)";
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
template void func<float, 1>(float a);)";
    string expect = R"(
#define func_TEMPLATE(T, i) \
void func_##T##_##i##_(T a) { \
  a; \
}
func_TEMPLATE(float, 1)/*float a*/)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(template<typename T, int i = 0> void func(T a) {a;)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Template declaration unsupported syntax");
  }
  {
    string input = R"(template void func(float a);)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Template instantiation unsupported syntax");
  }
  {
    string input = R"(func<float, 1>(a);)";
    string expect = R"(TEMPLATE_GLUE2(func, float, 1)(a);)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_template);

static void test_preprocess_reference()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(void func() { auto &a = b; a.a = 0; c = a(a); a_c_a = a; })";
    string expect = R"(void func() { b.a = 0; c = a(b); a_c_a = b; })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(void func() { const int &a = b; a.a = 0; c = a(a); })";
    string expect = R"(void func() { b.a = 0; c = a(b); })";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(void func() { const int i = 0; auto &a = b[i]; a.a = 0; })";
    string expect = R"(void func() { const int i = 0; b[i].a = 0; })";
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
int func(int a, int b)
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
int func(int a, const int b)
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
int2 func(int2 a) {
  return a;
}
#line 2
int2 func()
{
#line 2
  return func(int2(0, 0));
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
void func(int a = 0) {
  a;
}
)";
    string expect = R"(
void func(int a) {
  a;
}
#line 2
void func()
{
#line 2
  func(0);
}
#line 6
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_default_arguments);

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

struct A_S {};
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
struct A_B_S {};




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

#define NS_read_TEMPLATE(T) T NS_read(T a) \
{ \
  return a; \
}
NS_read_TEMPLATE(float)/*float*/
float NS_write(float a){ return a; }

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

static void test_preprocess_stage_attribute()
{
  using namespace shader;
  using namespace std;

  {
    string input = R"(
[[gpu::vertex_function]] void my_func() {
  return;
}
)";
    string expect = R"(
#if defined(GPU_VERTEX_SHADER)
#line 2
                         void my_func() {
  return;
}
#endif
#line 5
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}
GPU_TEST(preprocess_stage_attribute);

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
    // EXPECT_EQ(output, expect); /* TODO: Add support. */
    EXPECT_EQ(error,
              "Return statement with values are not supported inside the same scope as "
              "resource access function.");
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
#line 7
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
}
GPU_TEST(preprocess_resource_guard);

}  // namespace blender::gpu::tests
