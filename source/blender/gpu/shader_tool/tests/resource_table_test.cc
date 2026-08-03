/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "shader_tool_testing.hh"

namespace blender::gpu::tests {

using namespace shader::parser;
using namespace shader;
using namespace std;

TEST(shader_tool, ResourceGuard)
{
  {
    string input = R"(
void my_func() {
  interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_id;
}
)";
    string expect = R"(
void my_func() {

#if defined(CREATE_INFO_draw_resource_id_varying)
#line 3
  interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_id;

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
  i += interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_id;
  return i;
}
)";
    string expect = R"(
uint my_func() {

#if defined(CREATE_INFO_draw_resource_id_varying)
#line 3
  uint i = 0;
  i += interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_id;
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
    i += interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_id;
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
    i += interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_id;

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
    i += interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_id;
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
    i += interface_get(draw_resource_id_varying, drw_ResourceID_iface).resource_id;
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

TEST(shader_tool, SrtMutations)
{
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

TEST(shader_tool, SrtMethod)
{
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

TEST(shader_tool, SrtTemplateWrapper)
{
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

TEST(shader_tool, EntryPointResources)
{
  {
    string input = R"(
namespace ns {

struct VertOut {
  [[smooth]] float3 local_pos;
};

struct FragOut {
  [[frag_color(0)]] float3 color;
  [[frag_color(1), index(1)]] uint test;
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
  gpu_BaseInstance;
  gl_PointSize;
  gl_ClipDistance;
  gl_Layer;
  gpu_ViewportIndex;
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
  gpu_ViewportIndex;
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
FRAGMENT_OUT_DUAL(1, uint, ns_FragOut_test, SRC_1)
GPU_SHADER_CREATE_END()


GPU_SHADER_INTERFACE_INFO(ns_VertOut_t)
SMOOTH(float3, ns_VertOut_local_pos)
GPU_SHADER_INTERFACE_END()



GPU_SHADER_CREATE_INFO(ns_vertex_function_infos_)
ADDITIONAL_INFO(Resources)
ADDITIONAL_INFO(ns_VertInTfloat)
VERTEX_OUT(ns_VertOut_t)
BUILTINS(BuiltinBits::INSTANCE_ID)
BUILTINS(BuiltinBits::POINT_SIZE)
BUILTINS(BuiltinBits::LAYER)
BUILTINS(BuiltinBits::VIEWPORT_INDEX)
BUILTINS(BuiltinBits::CLIP_DISTANCES)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(ns_fragment_function_infos_)
DEPTH_WRITE(DepthWrite::GREATER)
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

TEST(shader_tool, PipelineDescription)
{
  using namespace std;
  using namespace shader::parser;

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

TEST(shader_tool, InitializerList)
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

}  // namespace blender::gpu::tests
