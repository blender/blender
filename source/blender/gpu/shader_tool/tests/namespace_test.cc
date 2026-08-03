/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "shader_tool_testing.hh"

namespace blender::gpu::tests {

using namespace shader::parser;
using namespace shader;
using namespace std;

TEST(shader_tool, Namespace)
{
  {
    string input = R"(
struct C {};
void fn(C b) {}
namespace B {
struct D {};
void fn(D b) {}
void fn2()
{
  fn(C{});
}
}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error, "Call to function is ambiguous. Specify namespace to remove ambiguity.");
  }
  {
    string input = R"(
namespace A {
int func(int a) { return 0; }
int func2(int a)
{
  int func = func();
  return func;
}
}
)";
    string expect = R"(

int A_func(int a) { return 0; }
int A_func2(int a)
{
  int func = A_func();
  return func;
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
  {
    /* Template specialization inside namespace. */
    string input = R"(
namespace NS {
template<> Type a<Type>() {}
}
)";

    string expect = R"(

           Type NS_aTType() {}

)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    /* Half namespace specified identifiers and methods. */
    string input = R"(
namespace NS {
struct B {
  int i;
  static int D() { return B::C().R(); }
  static int E() { return C().R(); }
  static B C() { return B(0); }
  int R() { return R(); }
};
B fn() { return B::C(); }
}
)";

    string expect = R"(

struct NS_B {
  int i;
#line 9
};

#ifndef GPU_METAL
NS_B NS_B_ctor_();
int NS_B_D();
int NS_B_E();
NS_B NS_B_C();
int _R(_ref(NS_B ,this_));
#endif
#line 3
                    NS_B NS_B_ctor_() {NS_B r;r.i=0;return r;}
#line 5
         int NS_B_D() { return _R(NS_B_C()); }
         int NS_B_E() { return _R(NS_B_C()); }
         NS_B NS_B_C() { return NS_B(0); }
  int _R(_ref(NS_B ,this_)) { return _R(this_); }
#line 10
NS_B NS_fn() { return NS_B_C(); }

)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

}  // namespace blender::gpu::tests
