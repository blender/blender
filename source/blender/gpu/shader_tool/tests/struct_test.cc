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

TEST(shader_tool, EmptyStruct)
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

TEST(shader_tool, StructuredBindings)
{
  using namespace shader;
  using namespace std;

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
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

}  // namespace blender::gpu::tests
