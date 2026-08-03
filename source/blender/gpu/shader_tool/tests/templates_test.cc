/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "shader_tool_testing.hh"

namespace blender::gpu::tests {

using namespace shader::parser;
using namespace shader;
using namespace std;

TEST(shader_tool, Template)
{
  {
    string input = R"(
template<typename T>
void func() { T::fn(); }
template void func<A>();
)";
    string expect = R"(
#line 3
void funcTA() { A_fn(); }
#line 5
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
template<typename T>
void func(T a) {a;}
template void func<float>(float a);
template<typename T>
void foo(T &a) {a;}
template void foo<float>(float &a);

void f(float a)
{
  func(a);
  foo(a);
}
)";
    string expect = R"(
#line 3
void func(float a) {a;}
#line 6
void foo(_ref(float ,a)) {a;}
#line 9
void f(float a)
{
  func(a);
  foo(a);
}
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
template<int i, uint j, int k> E func() { return E(i + j + k); }
template E func<0x1, 2, -1>();
)";
    string expect = R"(
E funcT0x1T2T_1() { return E(0x1 + 2 + -1); }
#line 4
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
    EXPECT_EQ(error,
              "Template instantiation and specialization require explicit template arguments");
  }
  {
    string input = R"(
template A<f> fn(A<f> a);
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error,
              "Template instantiation and specialization require explicit template arguments");
  }
  {
    string input = R"(
template<> A fn(A a) {}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error,
              "Template instantiation and specialization require explicit template arguments");
  }
  {
    string input = R"(
template<> A<f> fn(A<f> a) {}
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(error,
              "Template instantiation and specialization require explicit template arguments");
  }
  {
    string input = R"(func<float, 1>(a);)";
    string expect = R"(funcTfloatT1(a);)";
    string error;
    string output = process_test_local(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(a.template func<float, 1>(a);)";
    string expect = R"(_funcTfloatT1(a, a);)";
    string error;
    string output = process_test_local(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(this->template func<float, 1>(a);)";
    string expect = R"(_funcTfloatT1(this_, a);)";
    string error;
    string output = process_test_local(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(A<B<1, 2>, C<1, D<T, -1>>> a;)";
    string expect = R"(ATBT1T2TCT1TDTTT_1 a;)";
    string error;
    string output = process_test_local(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

TEST(shader_tool, TemplateStruct)
{
  {
    string input = R"(
template<typename T>
struct A {
  T a;
  A method(T b) const
  {
    return A<T>{b};
  }
};
template struct A<float>;
)";
    string expect = R"(
#line 3
struct ATfloat {
  float a;
#line 9
};
#line 12
#ifndef GPU_METAL
ATfloat ATfloat_ctor_();
ATfloat _method(const ATfloat this_, float b);
#endif
#line 3
                       ATfloat ATfloat_ctor_() {ATfloat r;r.a=0.0f;return r;}
#line 5
  ATfloat _method(const ATfloat this_, float b)
  {
    return _ctor(ATfloat) b _rotc() ;
  }
#line 11
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
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
  {
    /* Struct templated methods. */
    string input = R"(
namespace N {

template<typename B> struct A {
  B i;
  template<typename T> static void fn1(T a) {}
  template<typename T> static T fn2() { return T(0); }
  template<typename T> void fn3(T a) { i += int(fn4<T>()); }
  template<typename T> T fn4() { fn3(0); return T(0); }
};

template struct A<int>;

template void A<int>::fn1<int>(int);
template int A<int>::fn2<int>();
template void A<int>::fn3<int>(int);
template int A<int>::fn4<int>();

void fn(A<int> a)
{
  A<int>::fn1(0);
  A<int>::fn2<int>();
  a.fn3(0);
  a.fn4<int>();
}

}
)";
    string expect = R"(
#line 4
struct N_ATint {
  int i;
#line 10
};
#line 14
#ifndef GPU_METAL
N_ATint N_ATint_ctor_();
void N_ATint_fn1(int a);
int N_ATint_fn2Tint();
void _fn3(_ref(N_ATint ,this_), int a);
int _fn4Tint(_ref(N_ATint ,this_));
#endif
#line 4
                       N_ATint N_ATint_ctor_() {N_ATint r;r.i=0;return r;}
#line 6
       void N_ATint_fn1(int a) {}
       int N_ATint_fn2Tint() { return int(0); }
void _fn3(_ref(N_ATint ,this_), int a) { this_.i += int(_fn4Tint(this_)); }
int _fn4Tint(_ref(N_ATint ,this_)) { _fn3(this_, 0); return int(0); }
#line 19
void N_fn(N_ATint a)
{
  N_ATint_fn1(0);
  N_ATint_fn2Tint();
  _fn3(a, 0);
  _fn4Tint(a);
}


)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

}  // namespace blender::gpu::tests
