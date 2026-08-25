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

void funcTA() { A_fn(); }

)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
template<typename T> void f(T a) {}
struct A {};
template void f<A>(A);
)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
template<typename T>
void func() { T a; }
template void func<float>();
)";
    string expect = R"(

void funcTfloat() {
#line 3
              float a;
#line 3
                   }
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
template<int U>
int func() { return U; }
template void func<1 + 3>();
)";
    string expect = R"(

int funcT4() {
#line 3
             return 4; }
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    /* CRTP. */
    string input = R"(
template<typename T> struct A {
  void f(T t)
  {
    t.f();
  }
};

struct U {
  void f() {}
};

template struct A<U>;
)";
    string expect = R"(#line 9
struct U {
  int _pad;
};
#line 9
U U_ctor_() {U r;r._pad=0;return r;}

#ifndef GPU_METAL
void _f(_ref(U ,this_));
#endif
#line 10
  void _f(_ref(U ,this_)) {
#line 10
            }
#line 2
                     struct ATU {
  int _pad;
#line 7
};
#line 2
ATU ATU_ctor_() {ATU r;r._pad=0;return r;}

#ifndef GPU_METAL
void _f(_ref(ATU ,this_), U _0);
#endif
#line 3
  void _f(_ref(ATU ,this_), U t)
  {
    _f(t);
  }
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
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

void func(float a) {a;}


void foo(_ref(float ,a)) {a;}


void f(float a)
{
  func(a);
  foo(a);
}
)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  /* Disabled until BSL 5.3 is default. */
  if (false) {
    string input = R"(
template<typename T> void foo(T a) { a; }
template void foo<float>(float a);
template void foo<int>(int a);
template<> void foo<uint>(uint a) {}

void f(float a, int b, uint c)
{
  foo(a);
  foo(b);
  foo(c);
  foo<float>(a);
  foo<int>(b);
  foo<uint>(c);
}
)";
    string expect = R"(
                     void fooTfloat(float a) {
#line 2
                                     a; }
#line 2
                     void fooTint(int a) {
#line 2
                                     a; }


           void fooTuint(uint a) { }

void f(float a, int b, uint c)
{
  fooTfloat(a);
  fooTint(b);
  fooTuint(c);
  fooTfloat(a);
  fooTint(b);
  fooTuint(c);
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
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

void funcTfloatT1(float a) {
  a;
}

)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
enum E : char { A, B };
template<int i, uint j, int k> int foo() { return int(i + j + k); }
template int foo<0xF + 2, 2, -1 - 2>();
template<enum E e, char i> E f() { return e; }
template E f<B, 2>();

int bar()
{
  foo<0xF + 2, 2, -1 - 2>();
  f<B, 1 + 1>();
}
)";
    string expect = R"(
#define E char
#line 2
static constexpr char E_A = 0;
#line 2
static constexpr char E_B  = 1;
                               int fooT17T2T_3() {
#line 3
                                           return int(17 + 2 + -3);
#line 3
                                                                  }

                           E fT1T2() {
#line 5
                                   return 1; }


int bar()
{
  fooT17T2T_3();
  fT1T2();
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
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

)";
    auto [output, _, error] = process_test_string(input);
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

)";
    auto [output, _, error] = process_test_string(input);
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
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(template<typename T, int i = 0> void func(T a) {a;})";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error, "Default arguments are not supported inside template declaration");
  }
  {
    string input = R"(template void func(float a);)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error,
              "Template instantiation and specialization require explicit template arguments");
  }
  {
    string input = R"(template A<f> fn(A<f> a);)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error,
              "Template instantiation and specialization require explicit template arguments");
  }
  {
    string input = R"(template<> A fn(A a) {})";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error,
              "Template instantiation and specialization require explicit template arguments");
  }
  {
    string input = R"(template<> A<f> fn(A<f> a) {})";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error,
              "Template instantiation and specialization require explicit template arguments");
  }
  {
    string input = R"(template<typename T, int i = 0> void func(T a) {a;})";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Default arguments are not supported inside template declaration");
  }
  {
    string input = R"(template void func(float a);)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Expected template arguments");
  }
  {
    string input = R"(template A<f> fn(A<f> a);)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Expected template arguments");
  }
  {
    string input = R"(template<> A fn(A a) {})";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Expected template arguments");
  }
  {
    string input = R"(template<> A<f> fn(A<f> a) {})";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Expected template arguments");
  }
  {
    string input = R"(func<float, 1>(a);)";
    string expect = R"(funcTfloatT1(a);)";
    auto [output, _, error] = process_test_local(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(a.template func<float, 1>(a);)";
    string expect = R"(_funcTfloatT1(a, a);)";
    auto [output, _, error] = process_test_local(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(this->template func<float, 1>(a);)";
    string expect = R"(_funcTfloatT1(this_, a);)";
    auto [output, _, error] = process_test_local(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(A<B<1, 2>, C<1, D<T, -1>>> a;)";
    string expect = R"(ATBT1T2TCT1TDTTT_1 a;)";
    auto [output, _, error] = process_test_local(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
struct T {};
template<int T, int U> struct B {};
template<int T, typename U> struct C {};
template<typename T, int U> struct D {};
template<typename T, typename U> struct A {};

template struct B<1, 2>;
template struct D<T, -1>;
template struct C<1, D<T, -1>>;
template struct A<B<1, 2>, C<1, D<T, -1>>>;

A<B<1, 2>, C<1, D<T, -1>>> a;
)";
    string expect = R"(
struct T {int _pad;
#line 2
          };
#line 2
T T_ctor_() {T r;r._pad=0;return r;}
                       struct BT1T2 {int _pad;
#line 3
                                 };
#line 3
BT1T2 BT1T2_ctor_() {BT1T2 r;r._pad=0;return r;}

                            struct DTTT_1 {int _pad;
#line 5
                                      };
#line 5
DTTT_1 DTTT_1_ctor_() {DTTT_1 r;r._pad=0;return r;}
#line 4
                            struct CT1TDTTT_1 {int _pad;
#line 4
                                      };
#line 4
CT1TDTTT_1 CT1TDTTT_1_ctor_() {CT1TDTTT_1 r;r._pad=0;return r;}

                                 struct ATBT1T2TCT1TDTTT_1 {int _pad;
#line 6
                                           };
#line 6
ATBT1T2TCT1TDTTT_1 ATBT1T2TCT1TDTTT_1_ctor_() {ATBT1T2TCT1TDTTT_1 r;r._pad=0;return r;}
#line 13
ATBT1T2TCT1TDTTT_1 a;
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
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

struct ATfloat {
  float a;
#line 9
};


#ifndef GPU_METAL
ATfloat ATfloat_ctor_();
ATfloat _method(const ATfloat this_, float b);
#endif
#line 3
                       ATfloat ATfloat_ctor_() {ATfloat r;r.a=0.0f;return r;}

  ATfloat _method(const ATfloat this_, float b)
  {
    return _ctor(ATfloat) b _rotc() ;
  }


)";
    auto [output, _, error] = process_test_string(input);
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

struct ATfloat {                                                              float a; };
#line 3
                       ATfloat ATfloat_ctor_() {ATfloat r;r.a=0.0f;return r;}

)";
    auto [output, _, error] = process_test_string(input);
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


)";
    auto [output, _, error] = process_test_string(input);
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
    auto [output, _, error] = process_test_string(input);
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
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
template<typename T> struct A {
  T a;
  A method(T b) const
  {
    A<T> a;
    A c;
    a.a = b;
    c.a = b;
    return a;
  }
};
template struct A<float>;
)";
    string expect = R"(
                     struct ATfloat {
  float a;
#line 12
};
#line 2
ATfloat ATfloat_ctor_() {ATfloat r;r.a=0.0f;return r;}

#ifndef GPU_METAL
ATfloat _method(ATfloat this_, float _0);
#endif
#line 4
  ATfloat _method(ATfloat this_, float b)
  {
    ATfloat a;
    ATfloat c;
    a.a = b;
    c.a = b;
    return a;
  }
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }

  {
    string input = R"(
template<int i> struct T {};

template<> struct T<0> {
  int b[1];
};

int f(T<0> a)
{
  return a.b[1];
}
)";
    string expect = R"(#line 4
           struct TT0 {
  int b[1];
};
#line 4
TT0 TT0_ctor_() {TT0 r;for(int a =0;a < 1;++a) {r.b[a]=0;}return r;}
#line 8
int f(TT0 a)
{
  return a.b[1];
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

}  // namespace blender::gpu::tests
