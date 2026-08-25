/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "shader_tool_testing.hh"

namespace blender::gpu::tests {

using namespace shader::parser;
using namespace shader;
using namespace std;

TEST(shader_tool, Union)
{
  {
    string input = R"(
struct A {
  float2 a[2];
};

struct B {
  float2 b;
};

struct C {
  B b;
  float2 c;
  float2 d;
};

union U {
  A a;
  B b;
  C c;
};

C f(A a)
{
  U u;
  u.a = a;
  return u.c;
}
)";
    string expect =
        R"(
struct A {
  float2 a[2];
};
#line 2
A A_ctor_() {A r;for(int a =0;a < 2;++a) {r.a[a]=float2(0);}return r;}
#line 6
struct B {
  float2 b;
};
#line 6
B B_ctor_() {B r;r.b=float2(0);return r;}
#line 10
struct C {
  B b;
  float2 c;
  float2 d;
};
#line 10
C C_ctor_() {C r;r.b=B_ctor_();r.c=float2(0);r.d=float2(0);return r;}
#line 16
struct U {
  float4 _0; float2 _1;


};
#line 16
U U_ctor_() {U r;r._0=float4(0);r._1=float2(0);return r;}

A _a(U this_) {
  A r;
  r.a[0] = this_._0.xy;
  r.a[1] = this_._0.zw;
  return r;
}
void _a_set_(_ref(U ,this_), A v) {
  this_._0.xy = v.a[0];
  this_._0.zw = v.a[1];
}
B _b(U this_) {
  B r;
  r.b = this_._0.xy;
  return r;
}
void _b_set_(_ref(U ,this_), B v) {
  this_._0.xy = v.b;
}
C _c(U this_) {
  C r;
  r.b.b = this_._0.xy;
  r.c = this_._0.zw;
  r.d = this_._1.xy;
  return r;
}
void _c_set_(_ref(U ,this_), C v) {
  this_._0.xy = v.b.b;
  this_._0.zw = v.c;
  this_._1.xy = v.d;
}
#line 22
C f(A a)
{
  U u;
  _a_set_(u, a);
  return _c(u);
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
union U {
  struct A {
    float2 a[2][2];
  };
  A a;
};
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Multi-dimensional arrays are not supported");
  }
  {
    string input = R"(
union U {
  struct A {
    float2 a[SOME_CONST];
  };
  A a;
};
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Array size is not a constant expression");
  }
  {
    string input = R"(
union U {
  float2 a[2];
};
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Unsupported array at union base scope; wrap it inside a named struct");
  }
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

void _a_set_(_ref(T ,this_), uint4 value) {
  this_.union0.data0 = uintBitsToFloat(value);
}

int4 _b(const T this_)       {
  int4 val;
  val = floatBitsToInt(this_.union0.data0);
  return val;
}

void _b_set_(_ref(T ,this_), int4 value) {
  this_.union0.data0 = intBitsToFloat(value);
}

float4 _c(const T this_)       {
  float4 val;
  val = this_.union0.data0;
  return val;
}

void _c_set_(_ref(T ,this_), float4 value) {
  this_.union0.data0 = value;
}

)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
union {};
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(error, "Anonymous unions at namespace or global scope are not supported");
  }
  {
    string input = R"(
union A {
  uint4 a;
  int4 b;
  float4 c;
};
)";
    string expect = R"(
struct A {
  float4 _0;


};
#line 2
A A_ctor_() {A r;r._0=float4(0);return r;}

uint4 _a(A this_) {
  return floatBitsToUint(this_._0);
}
void _a_set_(_ref(A ,this_), uint4 v) {
  this_._0 = uintBitsToFloat(v.a);
}
int4 _b(A this_) {
  return floatBitsToInt(this_._0);
}
void _b_set_(_ref(A ,this_), int4 v) {
  this_._0 = intBitsToFloat(v.b);
}
float4 _c(A this_) {
  return this_._0;
}
void _c_set_(_ref(A ,this_), float4 v) {
  this_._0 = v.c;
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
struct A {
  union {
    uint4 a;
    int4 b;
  };
};

void f(A a) {
  a.a;
  a.b;
}
)";
    string expect = R"(

  struct A_a0 {
    float4 _0;

  };
#line 3
A_a0 A_a0_ctor_() {A_a0 r;r._0=float4(0);return r;}

uint4 _a(A_a0 this_) {
  return floatBitsToUint(this_._0);
}
void _a_set_(_ref(A_a0 ,this_), uint4 v) {
  this_._0 = uintBitsToFloat(v.a);
}
int4 _b(A_a0 this_) {
  return floatBitsToInt(this_._0);
}
void _b_set_(_ref(A_a0 ,this_), int4 v) {
  this_._0 = intBitsToFloat(v.b);
}
#line 2
struct A {
  A_a0 a0_;
#line 7
};
#line 3
  A A_ctor_() {A r;r.a0_=A_a0_ctor_();return r;}
#line 9
void f(A a) {
  _a(a.a0_);
  _b(a.a0_);
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
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

void _a_set_(_ref(T ,this_), uint4 value) {
  this_.union0.data0 = uintBitsToFloat(value);
}

uint4 _b(const T this_)       {
  uint4 val;
  val = floatBitsToUint(this_.union1.data0);
  return val;
}

void _b_set_(_ref(T ,this_), uint4 value) {
  this_.union1.data0 = uintBitsToFloat(value);
}

)";
    auto [output, _, error] = process_test_string(input);
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

void _a_set_(_ref(T ,this_), A value) {
  this_.union0.data0.xyz = value.e.a;
  this_.union0.data0.w = value.e.b;
}

)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }

  {
    string input = R"(
struct T {
  int i;
};
struct A {
  union {
    struct {
      uint4 a;
      int4 b;
    };
    uint4 c;
  };
  union {
    T d;
    uint4 e;
  };
};

void f(A a) {
a.a;
a.b;
a.c;
a.d.i;
a.e;
}
)";
    string expect = R"(
struct T {
  int i;
};
#line 2
T T_ctor_() {T r;r.i=0;return r;}
#line 7
    struct A_a0_a0 {
      uint4 a;
      int4 b;
    };
#line 7
A_a0_a0 A_a0_a0_ctor_() {A_a0_a0 r;r.a=uint4(0);r.b=int4(0);return r;}
#line 6
  struct A_a0 {
    float4 _0; float4 _1;
#line 12
  };
#line 6
A_a0 A_a0_ctor_() {A_a0 r;r._0=float4(0);r._1=float4(0);return r;}

A_a0_a0 _a0_(A_a0 this_) {
  A_a0_a0 r;
  r.a = floatBitsToUint(this_._0);
  r.b = floatBitsToInt(this_._1);
  return r;
}
void _a0__set_(_ref(A_a0 ,this_), A_a0_a0 v) {
  this_._0 = uintBitsToFloat(v.a);
  this_._1 = intBitsToFloat(v.b);
}
uint4 _c(A_a0 this_) {
  return floatBitsToUint(this_._0);
}
void _c_set_(_ref(A_a0 ,this_), uint4 v) {
  this_._0 = uintBitsToFloat(v.c);
}
#line 13
  struct A_a1 {
    float4 _0;

  };
#line 13
A_a1 A_a1_ctor_() {A_a1 r;r._0=float4(0);return r;}

T _d(A_a1 this_) {
  T r;
  r.i = floatBitsToInt(this_._0.x);
  return r;
}
void _d_set_(_ref(A_a1 ,this_), T v) {
  this_._0.x = intBitsToFloat(v.i);
}
uint4 _e(A_a1 this_) {
  return floatBitsToUint(this_._0);
}
void _e_set_(_ref(A_a1 ,this_), uint4 v) {
  this_._0 = uintBitsToFloat(v.e);
}
#line 5
struct A {
  A_a0 a0_;
#line 13
  A_a1 a1_;
#line 17
};
#line 13
  A A_ctor_() {A r;r.a0_=A_a0_ctor_();r.a1_=A_a1_ctor_();return r;}
#line 19
void f(A a) {
_a0_(a.a0_).a;
_a0_(a.a0_).b;
_c(a.a0_);
_d(a.a1_).i;
_e(a.a1_);
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
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

void _a_set_(_ref(T ,this_), float4x4 value) {
  this_.union0.data0 = value[0];
  this_.union0.data1 = value[1];
  this_.union0.data2 = value[2];
  this_.union0.data3 = value[3];
}

)";
    auto [output, _, error] = process_test_string(input);
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
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error,
              "All union members must have their type wrapped using the union_t<T> template.");
  }
}

TEST(shader_tool, Enum)
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

static constexpr int enum_class_VALUE = 0;

#define enum_class int
#line 2

enum_class enum_class_ctor_() { return enum_class(0); }
#line 2



)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }

  {
    string input = R"(
enum E : int { A, B = 2, C, D = 1, E };
)";
    string expect = R"(
static constexpr int A = 0;
#line 2
static constexpr int B = 2;
#line 2
static constexpr int C = B + 1;
#line 2
static constexpr int D = 1;
#line 2
static constexpr int E = D + 1;

#define E int
#line 2

E E_ctor_() { return E(0); }
#line 2

)";
    auto [output, _, error] = process_test_string(input);
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

static constexpr int enum_class_VALUE = 0;

#define enum_class int
#line 2

enum_class enum_class_ctor_() { return enum_class(0); }
#line 2



)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
enum class enum_class {
  VALUE = 0,
};
)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(error, "enum declaration must explicitly use an underlying type");
  }
}

TEST(shader_tool, EmptyStruct)
{
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

)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
class S {};
struct T {};
struct U {
  static void fn() {}
};
)";
    string expect = R"(
struct S {int _pad;
#line 2
         };
#line 2
S S_ctor_() {S r;r._pad=0;return r;}
struct T {int _pad;
#line 3
          };
#line 3
T T_ctor_() {T r;r._pad=0;return r;}
struct U {
  int _pad;
};
#line 4
U U_ctor_() {U r;r._pad=0;return r;}

#ifndef GPU_METAL
       void U_fn();
#endif
#line 5
         void U_fn() {
#line 5
                    }
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

TEST(shader_tool, NestedStruct)
{
  {
    string input = R"(
struct S {
  int i;
  struct {
    int b;
  };
  struct C {
    int b;
  };
  C c;
};
)";
    string expect = R"(#line 4
  struct S_a0 {
    int b;
  };
#line 4
S_a0 S_a0_ctor_() {S_a0 r;r.b=0;return r;}


  struct S_C {
    int b;
  };
#line 7
S_C S_C_ctor_() {S_C r;r.b=0;return r;}
#line 2
struct S {
  int i;
  S_a0 a0_;
#line 10
  S_C c;
};
#line 4
  S S_ctor_() {S r;r.i=0;r.a0_=S_a0_ctor_();r.c=S_C_ctor_();return r;}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

TEST(shader_tool, StructuredBindings)
{
  {
    string input = R"(
struct S {
  int i;
  float b;
};

S test()
{
  return S{};
}

void fn(S u, S &v)
{
  S t;
  S &r = t;
  {
    int u;
    int t;
  }
  auto [a, b] = S{};
  auto [c, d] = test();
  auto [e, f] = t;
  auto [g, h] = u;
  auto [i, j] = r;
  auto [k, l] = v;
}
)";
    string expect = R"(
struct S {
  int i;
  float b;
};
#line 2
                 S S_ctor_() {S r;r.i=0;r.b=0.0f;return r;}
#line 7
S test()
{
  return S_ctor_();
}

void fn(S u, _ref(S ,v))
{
  S t;

  {
    int u;
    int t;
  }
  S _u0= S_ctor_();int a=_u0.i;float b=_u0.b;
  S _u1= test();int c=_u1.i;float d=_u1.b;
  S _u2= t;int e=_u2.i;float f=_u2.b;
  S _u3= u;int g=_u3.i;float h=_u3.b;
  S _u4= t;int i=_u4.i;float j=_u4.b;
  S _u5= v;int k=_u5.i;float l=_u5.b;
}
)";
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
struct A {
  int foo, bar;
};

void f()
{
  A c;
  auto [a, b] = c;
  a + b;
}
)";
    string expect = R"(
struct A {
  int foo, bar;
};
#line 2
A A_ctor_() {A r;r.foo=0;r.bar=0;return r;}
#line 6
void f()
{
  A c;
  A _4        = c;
  _4.foo + _4.bar;
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

TEST(shader_tool, InitializerList)
{
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
T fn3() { {T _tmp ;    _tmp.a=1;  _tmp.b=2;   return _tmp;}; }
T fn4() { {T _tmp ;    _tmp.a=1;  _tmp.b=2  ;   return _tmp;}; }
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
    auto [output, _, error] = process_test_string(input);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
  {
    string input = R"(
void fn() {
  T t9={1, T{.a=1, .b=2}.a};
}
)";
    auto [output, metadata, error] = process_test_string(input);
    EXPECT_EQ(error, "Designated initializers are only supported in assignments");
  }
  {
    string input = R"(
void fn() {
  T t10={1, float4{0}};
}
)";
    auto [output, metadata, error] = process_test_string(input);
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
    auto [output, metadata, error] = process_test_string(input);
    EXPECT_EQ(error, "Nested initializer lists are not supported");
  }
  {
    string input = R"(
void fn() {
  T t12={.a=1, .b=float4{0}};
}
)";
    auto [output, metadata, error] = process_test_string(input);
    EXPECT_EQ(
        error,
        "Aggregate is error prone for built-in vector and matrix types, use constructors instead");
  }

  {
    string input = R"(
struct T{int a,b;};
T fn1() { return T{1, 2}; }
T fn2() { return T{1, 2, }; }
T fn3() { return T{.a=1, .b=2}; }
T fn4() { return T{.a=1, .b=2, }; }
T fn5() { return {1, 2}; }
T fn6() { return {1, 2, }; }
T fn7() { return {.a=1, .b=2}; }
T fn8() { return {.a=1, .b=2, }; }
void fn() {
  T t0={};
  T t1=T{1, 2};
  T t2=T{1, 2, };
  T t3=T{.a=1, .b=2};
  T t4=T{.a=1, .b=2, };
  T t5={1, 2};
  T t6={1, 2, };
  T t7={.a=1, .b=2};
  T t8={.a=1, .b=2, };
  T t9=T{.a=1, .b=T{0, 2}.a};
  T t10=T{1, T{0, 2}.a};
}
)";
    string expect = R"(
struct T{
#line 2
         int a,b;
#line 2
                 };
#line 2
T T_ctor_() {T r;r.a=0;r.b=0;return r;}
T fn1() { return _ctor(T)1, 2 _rotc();
#line 3
                          }
T fn2() { return _ctor(T)1, 2   _rotc();
#line 4
                            }
T fn3() { return _ctor(T)1, 2 _rotc();
#line 5
                                }
T fn4() { return _ctor(T)1, 2   _rotc();
#line 6
                                  }
T fn5() { return _ctor(T)1, 2 _rotc();
#line 7
                         }
T fn6() { return _ctor(T)1, 2   _rotc();
#line 8
                           }
T fn7() { return _ctor(T)1, 2 _rotc();
#line 9
                               }
T fn8() { return _ctor(T)1, 2   _rotc();
#line 10
                                 }
void fn() {
  T t0=T_ctor_();
  T t1=_ctor(T)1, 2 _rotc();
  T t2=_ctor(T)1, 2   _rotc();
  T t3=_ctor(T)1, 2 _rotc();
  T t4=_ctor(T)1, 2   _rotc();
  T t5=_ctor(T)1, 2 _rotc();
  T t6=_ctor(T)1, 2   _rotc();
  T t7=_ctor(T)1, 2 _rotc();
  T t8=_ctor(T)1, 2   _rotc();
  T t9=_ctor(T)1, _ctor(T)0, 2 _rotc().a _rotc();
  T t10=_ctor(T)1, _ctor(T)0, 2 _rotc().a _rotc();
}
)";
    auto [output, _, error] = process_test_string(input, Language::BSL);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

/* Process a test string inside a wrapper function. */
static inline Result process_test_buffer(std::string str,
                                         std::string buffer,
                                         shader::Language language)
{
  std::string prefix = "struct S {\n";
  std::string suffix = R"(
};
struct Res {
  )" + buffer + R"( S &a;
};
)";
  auto [result, metadata, error] = process_test_string(prefix + str + suffix, language);
  result = result.substr(prefix.size(), result.size() - suffix.size() - prefix.size());
  return {result, metadata, error};
}

static inline Result process_test_uniform(
    std::string str, shader::Language language = shader::Language::BLENDER_GLSL)
{
  return process_test_buffer(str, "[[uniform(0)]]", language);
}

static inline Result process_test_storage(
    std::string str, shader::Language language = shader::Language::BLENDER_GLSL)
{
  return process_test_buffer(str, "[[storage(0, read)]]", language);
}

TEST(shader_tool, StructBufferLinting)
{
  {
    string input = R"(char d;)";
    auto [output, _, error] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error, "Type 'char' is not allowed in storage buffer");
  }
  {
    string input = R"(float3 d;)";
    auto [output, _, error] = process_test_uniform(input, Language::BSL);
    EXPECT_EQ(
        error,
        "Type 'float3' is not allowed in uniform and storage buffer; use 'packed_float3' instead");
  }
  {
    string input = R"(float3 d;)";
    auto [output, _, error] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error,
              "Type 'float3' is not allowed in storage buffer; use 'packed_float3' instead");
  }
  {
    string input = R"(bool d;)";
    auto [output, _, error] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error, "Type 'bool' is not allowed in storage buffer; use 'bool32_t' instead");
  }
  {
    string input = R"(float3x3 d;)";
    auto [output, _, error] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error, "Type 'float3x3' is not allowed in storage buffer; use 'float3x4' instead");
  }

  /* Basic padding. */
  {
    string input = R"(
float a;
float2 b;)";
    auto [output, _, error] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error, "Implicit 4 bytes padding before 'b', add manual padding");
  }
  {
    string input = R"(
float a;
packed_float3 b;)";
    auto [output, _, error] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error, "Implicit 12 bytes padding before 'b', add manual padding");
  }
  {
    string input = R"(
float a;
float4 b;)";
    auto [output, _, error] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error, "Implicit 12 bytes padding before 'b', add manual padding");
  }
  {
    string input = R"(
float a;
float4x4 b;)";
    auto [output, _, error] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error, "Implicit 12 bytes padding before 'b', add manual padding");
  }
  {
    string input = R"(
float a;
float3x4 b;)";
    auto [output, _, error] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error, "Implicit 12 bytes padding before 'b', add manual padding");
  }
  {
    string input = R"(
float A;
float V;
float C;
float D;
float4 a;
packed_float3 b;
float c;
float S;
float R;
float2 d;
float e;
float f;
float2 g;)";
    auto [output, _, error] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error, "");
  }

  {
    /* std140 arrays force 16-byte alignment. */
    string input = R"(
  float a;
  float b[2];
  float c;
  )";
    auto [output, _, error] = process_test_uniform(input, Language::BSL);
    EXPECT_EQ(error,
              "Implicit padding of 12 bytes per array element, use a 16 bytes type or use a "
              "storage buffer");
    auto [output_ssbo, m, error_ssbo] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error_ssbo, "Implicit 12 bytes padding before 'b', add manual padding");
  }
  {
    /* std140 arrays force 16-byte alignment. */
    string input = R"(
  float2 a;
  float2 b[2];
  float2 c;
  )";
    auto [output, _, error] = process_test_uniform(input, Language::BSL);
    EXPECT_EQ(error,
              "Implicit padding of 8 bytes per array element, use a 16 bytes type or use a "
              "storage buffer");
    auto [output_ssbo, m, error_ssbo] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error_ssbo, "Implicit 8 bytes padding before 'b', add manual padding");
  }
  {
    /* std140 arrays force 16-byte alignment. */
    string input = R"(
  float b[2];
  float a;
  float c;
  float d;
  )";
    auto [output, _, error] = process_test_uniform(input, Language::BSL);
    EXPECT_EQ(error,
              "Implicit padding of 12 bytes per array element, use a 16 bytes type or use a "
              "storage buffer");
    auto [output_ssbo, m, error_ssbo] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error_ssbo, "");
  }
  {
    /* std140 arrays force 16-byte alignment. */
    string input = R"(
  float2 b[2];
  float2 a;
  float2 c;
  )";
    auto [output, _, error] = process_test_uniform(input, Language::BSL);
    EXPECT_EQ(error,
              "Implicit padding of 8 bytes per array element, use a 16 bytes type or use a "
              "storage buffer");
    auto [output_ssbo, m, error_ssbo] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error_ssbo, "");
  }
  {
    /* Unknown size. */
    string input = R"(
  float b[SOME_CONST];
  )";
    auto [output, _, error] = process_test_uniform(input, Language::BSL);
    EXPECT_EQ(error, "Array size is not a constant expression");
  }
  {
    /* Tiny struct. */
    string input = R"(
  float a;
  )";
    auto [output, _, error] = process_test_uniform(input, Language::BSL);
    EXPECT_EQ(error, "Implicit 12 bytes padding at the end of 'S', add manual padding");
    auto [output_ssbo, m, error_ssbo] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error_ssbo, "");
  }
  {
    /* Tiny struct. */
    string input = R"(
  float2 a;
  )";
    auto [output, _, error] = process_test_uniform(input, Language::BSL);
    EXPECT_EQ(error, "Implicit 8 bytes padding at the end of 'S', add manual padding");
    auto [output_ssbo, m, error_ssbo] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error_ssbo, "");
  }
  {
    string input = R"(
  packed_float3 a;
  float2 b;
  )";
    auto [output, _, error] = process_test_uniform(input, Language::BSL);
    EXPECT_EQ(error, "Implicit 4 bytes padding before 'b', add manual padding");
  }
  {
    string input = R"(
  float2 a;
  float b;
  )";
    auto [output, _, error] = process_test_uniform(input, Language::BSL);
    EXPECT_EQ(error, "Implicit 4 bytes padding at the end of 'S', add manual padding");
  }
  {
    string input = R"(
  packed_float3 a;
  )";
    auto [output, _, error] = process_test_uniform(input, Language::BSL);
    EXPECT_EQ(error, "Implicit 4 bytes padding at the end of 'S', add manual padding");
  }
  {
    string input = R"(
  float a;
  float b;
  float c;
  )";
    auto [output, _, error] = process_test_uniform(input, Language::BSL);
    EXPECT_EQ(error, "Implicit 4 bytes padding at the end of 'S', add manual padding");
    auto [output_ssbo, m, error_ssbo] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error_ssbo, "");
  }
  {
    string input = R"(
struct T { uint a; };
T a;
T b[2];
)";
    auto [output, _, error] = process_test_uniform(input, Language::BSL);
    EXPECT_EQ(error, "Implicit 12 bytes padding at the end of 'T', add manual padding");
    auto [output_ssbo, m, error_ssbo] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error_ssbo, "Implicit 12 bytes padding before 'b', add manual padding");
  }
  {
    string input = R"(
struct T { float2 a; };
float a;
T b;
)";
    auto [output, _, error] = process_test_uniform(input, Language::BSL);
    EXPECT_EQ(error, "Implicit 8 bytes padding at the end of 'T', add manual padding");
    auto [output_ssbo, m, error_ssbo] = process_test_storage(input, Language::BSL);
    EXPECT_EQ(error_ssbo, "Implicit 12 bytes padding before 'b', add manual padding");
  }
}

}  // namespace blender::gpu::tests
