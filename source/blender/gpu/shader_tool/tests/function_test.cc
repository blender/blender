/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "shader_tool_testing.hh"

namespace blender::gpu::tests {

using namespace shader::parser;
using namespace shader;
using namespace std;

TEST(shader_tool, StructMethods)
{
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

TEST(shader_tool, DefaultArguments)
{
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

}  // namespace blender::gpu::tests
