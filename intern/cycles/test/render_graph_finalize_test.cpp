/*
 * Copyright 2011-2016 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "testing/mock_log.h"
#include "testing/testing.h"

#include "device/device.h"

#include "render/graph.h"
#include "render/nodes.h"
#include "render/scene.h"

#include "util/util_array.h"
#include "util/util_logging.h"
#include "util/util_stats.h"
#include "util/util_string.h"
#include "util/util_vector.h"

using testing::_;
using testing::AnyNumber;
using testing::HasSubstr;
using testing::ScopedMockLog;

CCL_NAMESPACE_BEGIN

namespace {

template<typename T> class ShaderNodeBuilder {
 public:
  ShaderNodeBuilder(const string &name) : name_(name)
  {
    node_ = new T();
    node_->name = name;
  }

  const string &name() const
  {
    return name_;
  }

  ShaderNode *node() const
  {
    return node_;
  }

  template<typename V> ShaderNodeBuilder &set(const string &input_name, V value)
  {
    ShaderInput *input_socket = node_->input(input_name.c_str());
    EXPECT_NE((void *)NULL, input_socket);
    input_socket->set(value);
    return *this;
  }

  template<typename T2, typename V> ShaderNodeBuilder &set(V T2::*pfield, V value)
  {
    static_cast<T *>(node_)->*pfield = value;
    return *this;
  }

 protected:
  string name_;
  ShaderNode *node_;
};

class ShaderGraphBuilder {
 public:
  ShaderGraphBuilder(ShaderGraph *graph) : graph_(graph)
  {
    node_map_["Output"] = graph->output();
  }

  ShaderNode *find_node(const string &name)
  {
    map<string, ShaderNode *>::iterator it = node_map_.find(name);
    if (it == node_map_.end()) {
      return NULL;
    }
    return it->second;
  }

  template<typename T> ShaderGraphBuilder &add_node(const T &node)
  {
    EXPECT_EQ(find_node(node.name()), (void *)NULL);
    graph_->add(node.node());
    node_map_[node.name()] = node.node();
    return *this;
  }

  ShaderGraphBuilder &add_connection(const string &from, const string &to)
  {
    vector<string> tokens_from, tokens_to;
    string_split(tokens_from, from, "::");
    string_split(tokens_to, to, "::");
    EXPECT_EQ(tokens_from.size(), 2);
    EXPECT_EQ(tokens_to.size(), 2);
    ShaderNode *node_from = find_node(tokens_from[0]), *node_to = find_node(tokens_to[0]);
    EXPECT_NE((void *)NULL, node_from);
    EXPECT_NE((void *)NULL, node_to);
    EXPECT_NE(node_from, node_to);
    ShaderOutput *socket_from = node_from->output(tokens_from[1].c_str());
    ShaderInput *socket_to = node_to->input(tokens_to[1].c_str());
    EXPECT_NE((void *)NULL, socket_from);
    EXPECT_NE((void *)NULL, socket_to);
    graph_->connect(socket_from, socket_to);
    return *this;
  }

  /* Common input/output boilerplate. */
  ShaderGraphBuilder &add_attribute(const string &name)
  {
    return (*this).add_node(
        ShaderNodeBuilder<AttributeNode>(name).set(&AttributeNode::attribute, ustring(name)));
  }

  ShaderGraphBuilder &output_closure(const string &from)
  {
    return (*this).add_connection(from, "Output::Surface");
  }

  ShaderGraphBuilder &output_color(const string &from)
  {
    return (*this)
        .add_node(ShaderNodeBuilder<EmissionNode>("EmissionNode"))
        .add_connection(from, "EmissionNode::Color")
        .output_closure("EmissionNode::Emission");
  }

  ShaderGraphBuilder &output_value(const string &from)
  {
    return (*this)
        .add_node(ShaderNodeBuilder<EmissionNode>("EmissionNode"))
        .add_connection(from, "EmissionNode::Strength")
        .output_closure("EmissionNode::Emission");
  }

 protected:
  ShaderGraph *graph_;
  map<string, ShaderNode *> node_map_;
};

}  // namespace

class RenderGraph : public testing::Test {
 protected:
  ScopedMockLog log;
  Stats stats;
  Profiler profiler;
  DeviceInfo device_info;
  Device *device_cpu;
  SceneParams scene_params;
  Scene *scene;
  ShaderGraph graph;
  ShaderGraphBuilder builder;

  RenderGraph() : testing::Test(), builder(&graph)
  {
  }

  virtual void SetUp()
  {
    util_logging_start();
    util_logging_verbosity_set(1);

    device_cpu = Device::create(device_info, stats, profiler, true);
    scene = new Scene(scene_params, device_cpu);
  }

  virtual void TearDown()
  {
    delete scene;
    delete device_cpu;
  }
};

#define EXPECT_ANY_MESSAGE(log) EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());

#define CORRECT_INFO_MESSAGE(log, message) \
  EXPECT_CALL(log, Log(google::INFO, _, HasSubstr(message)));

#define INVALID_INFO_MESSAGE(log, message) \
  EXPECT_CALL(log, Log(google::INFO, _, HasSubstr(message))).Times(0);

/*
 * Test deduplication of nodes that have inputs, some of them folded.
 */
TEST_F(RenderGraph, deduplicate_deep)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Value1::Value to constant (0.8).");
  CORRECT_INFO_MESSAGE(log, "Folding Value2::Value to constant (0.8).");
  CORRECT_INFO_MESSAGE(log, "Deduplicated 2 nodes.");

  builder.add_node(ShaderNodeBuilder<GeometryNode>("Geometry1"))
      .add_node(ShaderNodeBuilder<GeometryNode>("Geometry2"))
      .add_node(ShaderNodeBuilder<ValueNode>("Value1").set(&ValueNode::value, 0.8f))
      .add_node(ShaderNodeBuilder<ValueNode>("Value2").set(&ValueNode::value, 0.8f))
      .add_node(ShaderNodeBuilder<NoiseTextureNode>("Noise1"))
      .add_node(ShaderNodeBuilder<NoiseTextureNode>("Noise2"))
      .add_node(
          ShaderNodeBuilder<MixNode>("Mix").set(&MixNode::type, NODE_MIX_BLEND).set("Fac", 0.5f))
      .add_connection("Geometry1::Parametric", "Noise1::Vector")
      .add_connection("Value1::Value", "Noise1::Scale")
      .add_connection("Noise1::Color", "Mix::Color1")
      .add_connection("Geometry2::Parametric", "Noise2::Vector")
      .add_connection("Value2::Value", "Noise2::Scale")
      .add_connection("Noise2::Color", "Mix::Color2")
      .output_color("Mix::Color");

  graph.finalize(scene);

  EXPECT_EQ(graph.nodes.size(), 5);
}

/*
 * Test RGB to BW node.
 */
TEST_F(RenderGraph, constant_fold_rgb_to_bw)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding RGBToBWNodeNode::Val to constant (0.8).");
  CORRECT_INFO_MESSAGE(log,
                       "Folding convert_float_to_color::value_color to constant (0.8, 0.8, 0.8).");

  builder
      .add_node(ShaderNodeBuilder<RGBToBWNode>("RGBToBWNodeNode")
                    .set("Color", make_float3(0.8f, 0.8f, 0.8f)))
      .output_color("RGBToBWNodeNode::Val");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - folding of Emission nodes that don't emit to nothing.
 */
TEST_F(RenderGraph, constant_fold_emission1)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Discarding closure Emission.");

  builder
      .add_node(
          ShaderNodeBuilder<EmissionNode>("Emission").set("Color", make_float3(0.0f, 0.0f, 0.0f)))
      .output_closure("Emission::Emission");

  graph.finalize(scene);
}

TEST_F(RenderGraph, constant_fold_emission2)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Discarding closure Emission.");

  builder.add_node(ShaderNodeBuilder<EmissionNode>("Emission").set("Strength", 0.0f))
      .output_closure("Emission::Emission");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - folding of Background nodes that don't emit to nothing.
 */
TEST_F(RenderGraph, constant_fold_background1)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Discarding closure Background.");

  builder
      .add_node(ShaderNodeBuilder<BackgroundNode>("Background")
                    .set("Color", make_float3(0.0f, 0.0f, 0.0f)))
      .output_closure("Background::Background");

  graph.finalize(scene);
}

TEST_F(RenderGraph, constant_fold_background2)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Discarding closure Background.");

  builder.add_node(ShaderNodeBuilder<BackgroundNode>("Background").set("Strength", 0.0f))
      .output_closure("Background::Background");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of Add Closure with only one input.
 */
TEST_F(RenderGraph, constant_fold_shader_add)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding AddClosure1::Closure to socket Diffuse::BSDF.");
  CORRECT_INFO_MESSAGE(log, "Folding AddClosure2::Closure to socket Diffuse::BSDF.");
  INVALID_INFO_MESSAGE(log, "Folding AddClosure3");

  builder.add_node(ShaderNodeBuilder<DiffuseBsdfNode>("Diffuse"))
      .add_node(ShaderNodeBuilder<AddClosureNode>("AddClosure1"))
      .add_node(ShaderNodeBuilder<AddClosureNode>("AddClosure2"))
      .add_node(ShaderNodeBuilder<AddClosureNode>("AddClosure3"))
      .add_connection("Diffuse::BSDF", "AddClosure1::Closure1")
      .add_connection("Diffuse::BSDF", "AddClosure2::Closure2")
      .add_connection("AddClosure1::Closure", "AddClosure3::Closure1")
      .add_connection("AddClosure2::Closure", "AddClosure3::Closure2")
      .output_closure("AddClosure3::Closure");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of Mix Closure with 0 or 1 fac.
 *  - Folding of Mix Closure with both inputs folded to the same node.
 */
TEST_F(RenderGraph, constant_fold_shader_mix)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding MixClosure1::Closure to socket Diffuse::BSDF.");
  CORRECT_INFO_MESSAGE(log, "Folding MixClosure2::Closure to socket Diffuse::BSDF.");
  CORRECT_INFO_MESSAGE(log, "Folding MixClosure3::Closure to socket Diffuse::BSDF.");

  builder.add_attribute("Attribute")
      .add_node(ShaderNodeBuilder<DiffuseBsdfNode>("Diffuse"))
      /* choose left */
      .add_node(ShaderNodeBuilder<MixClosureNode>("MixClosure1").set("Fac", 0.0f))
      .add_connection("Diffuse::BSDF", "MixClosure1::Closure1")
      /* choose right */
      .add_node(ShaderNodeBuilder<MixClosureNode>("MixClosure2").set("Fac", 1.0f))
      .add_connection("Diffuse::BSDF", "MixClosure2::Closure2")
      /* both inputs folded the same */
      .add_node(ShaderNodeBuilder<MixClosureNode>("MixClosure3"))
      .add_connection("Attribute::Fac", "MixClosure3::Fac")
      .add_connection("MixClosure1::Closure", "MixClosure3::Closure1")
      .add_connection("MixClosure2::Closure", "MixClosure3::Closure2")
      .output_closure("MixClosure3::Closure");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of Invert with all constant inputs.
 */
TEST_F(RenderGraph, constant_fold_invert)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Invert::Color to constant (0.68, 0.5, 0.32).");

  builder
      .add_node(ShaderNodeBuilder<InvertNode>("Invert")
                    .set("Fac", 0.8f)
                    .set("Color", make_float3(0.2f, 0.5f, 0.8f)))
      .output_color("Invert::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of Invert with zero Fac.
 */
TEST_F(RenderGraph, constant_fold_invert_fac_0)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Invert::Color to socket Attribute::Color.");

  builder.add_attribute("Attribute")
      .add_node(ShaderNodeBuilder<InvertNode>("Invert").set("Fac", 0.0f))
      .add_connection("Attribute::Color", "Invert::Color")
      .output_color("Invert::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of Invert with zero Fac and constant input.
 */
TEST_F(RenderGraph, constant_fold_invert_fac_0_const)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Invert::Color to constant (0.2, 0.5, 0.8).");

  builder
      .add_node(ShaderNodeBuilder<InvertNode>("Invert")
                    .set("Fac", 0.0f)
                    .set("Color", make_float3(0.2f, 0.5f, 0.8f)))
      .output_color("Invert::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of MixRGB Add with all constant inputs (clamp false).
 */
TEST_F(RenderGraph, constant_fold_mix_add)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding MixAdd::Color to constant (0.62, 1.14, 1.42).");

  builder
      .add_node(ShaderNodeBuilder<MixNode>("MixAdd")
                    .set(&MixNode::type, NODE_MIX_ADD)
                    .set(&MixNode::use_clamp, false)
                    .set("Fac", 0.8f)
                    .set("Color1", make_float3(0.3, 0.5, 0.7))
                    .set("Color2", make_float3(0.4, 0.8, 0.9)))
      .output_color("MixAdd::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of MixRGB Add with all constant inputs (clamp true).
 */
TEST_F(RenderGraph, constant_fold_mix_add_clamp)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding MixAdd::Color to constant (0.62, 1, 1).");

  builder
      .add_node(ShaderNodeBuilder<MixNode>("MixAdd")
                    .set(&MixNode::type, NODE_MIX_ADD)
                    .set(&MixNode::use_clamp, true)
                    .set("Fac", 0.8f)
                    .set("Color1", make_float3(0.3, 0.5, 0.7))
                    .set("Color2", make_float3(0.4, 0.8, 0.9)))
      .output_color("MixAdd::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - No folding on fac 0 for dodge.
 */
TEST_F(RenderGraph, constant_fold_part_mix_dodge_no_fac_0)
{
  EXPECT_ANY_MESSAGE(log);
  INVALID_INFO_MESSAGE(log, "Folding ");

  builder.add_attribute("Attribute1")
      .add_attribute("Attribute2")
      .add_node(ShaderNodeBuilder<MixNode>("Mix")
                    .set(&MixNode::type, NODE_MIX_DODGE)
                    .set(&MixNode::use_clamp, false)
                    .set("Fac", 0.0f))
      .add_connection("Attribute1::Color", "Mix::Color1")
      .add_connection("Attribute2::Color", "Mix::Color2")
      .output_color("Mix::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - No folding on fac 0 for light.
 */
TEST_F(RenderGraph, constant_fold_part_mix_light_no_fac_0)
{
  EXPECT_ANY_MESSAGE(log);
  INVALID_INFO_MESSAGE(log, "Folding ");

  builder.add_attribute("Attribute1")
      .add_attribute("Attribute2")
      .add_node(ShaderNodeBuilder<MixNode>("Mix")
                    .set(&MixNode::type, NODE_MIX_LIGHT)
                    .set(&MixNode::use_clamp, false)
                    .set("Fac", 0.0f))
      .add_connection("Attribute1::Color", "Mix::Color1")
      .add_connection("Attribute2::Color", "Mix::Color2")
      .output_color("Mix::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - No folding on fac 0 for burn.
 */
TEST_F(RenderGraph, constant_fold_part_mix_burn_no_fac_0)
{
  EXPECT_ANY_MESSAGE(log);
  INVALID_INFO_MESSAGE(log, "Folding ");

  builder.add_attribute("Attribute1")
      .add_attribute("Attribute2")
      .add_node(ShaderNodeBuilder<MixNode>("Mix")
                    .set(&MixNode::type, NODE_MIX_BURN)
                    .set(&MixNode::use_clamp, false)
                    .set("Fac", 0.0f))
      .add_connection("Attribute1::Color", "Mix::Color1")
      .add_connection("Attribute2::Color", "Mix::Color2")
      .output_color("Mix::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - No folding on fac 0 for clamped blend.
 */
TEST_F(RenderGraph, constant_fold_part_mix_blend_clamped_no_fac_0)
{
  EXPECT_ANY_MESSAGE(log);
  INVALID_INFO_MESSAGE(log, "Folding ");

  builder.add_attribute("Attribute1")
      .add_attribute("Attribute2")
      .add_node(ShaderNodeBuilder<MixNode>("Mix")
                    .set(&MixNode::type, NODE_MIX_BLEND)
                    .set(&MixNode::use_clamp, true)
                    .set("Fac", 0.0f))
      .add_connection("Attribute1::Color", "Mix::Color1")
      .add_connection("Attribute2::Color", "Mix::Color2")
      .output_color("Mix::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of Mix with 0 or 1 Fac.
 *  - Folding of Mix with both inputs folded to the same node.
 */
TEST_F(RenderGraph, constant_fold_part_mix_blend)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding MixBlend1::Color to socket Attribute1::Color.");
  CORRECT_INFO_MESSAGE(log, "Folding MixBlend2::Color to socket Attribute1::Color.");
  CORRECT_INFO_MESSAGE(log, "Folding MixBlend3::Color to socket Attribute1::Color.");

  builder.add_attribute("Attribute1")
      .add_attribute("Attribute2")
      /* choose left */
      .add_node(ShaderNodeBuilder<MixNode>("MixBlend1")
                    .set(&MixNode::type, NODE_MIX_BLEND)
                    .set(&MixNode::use_clamp, false)
                    .set("Fac", 0.0f))
      .add_connection("Attribute1::Color", "MixBlend1::Color1")
      .add_connection("Attribute2::Color", "MixBlend1::Color2")
      /* choose right */
      .add_node(ShaderNodeBuilder<MixNode>("MixBlend2")
                    .set(&MixNode::type, NODE_MIX_BLEND)
                    .set(&MixNode::use_clamp, false)
                    .set("Fac", 1.0f))
      .add_connection("Attribute1::Color", "MixBlend2::Color2")
      .add_connection("Attribute2::Color", "MixBlend2::Color1")
      /* both inputs folded to Attribute1 */
      .add_node(ShaderNodeBuilder<MixNode>("MixBlend3")
                    .set(&MixNode::type, NODE_MIX_BLEND)
                    .set(&MixNode::use_clamp, false))
      .add_connection("Attribute1::Fac", "MixBlend3::Fac")
      .add_connection("MixBlend1::Color", "MixBlend3::Color1")
      .add_connection("MixBlend2::Color", "MixBlend3::Color2")
      .output_color("MixBlend3::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - NOT folding of MixRGB Sub with the same inputs and fac NOT 1.
 */
TEST_F(RenderGraph, constant_fold_part_mix_sub_same_fac_bad)
{
  EXPECT_ANY_MESSAGE(log);
  INVALID_INFO_MESSAGE(log, "Folding Mix::");

  builder.add_attribute("Attribute")
      .add_node(ShaderNodeBuilder<MixNode>("Mix")
                    .set(&MixNode::type, NODE_MIX_SUB)
                    .set(&MixNode::use_clamp, true)
                    .set("Fac", 0.5f))
      .add_connection("Attribute::Color", "Mix::Color1")
      .add_connection("Attribute::Color", "Mix::Color2")
      .output_color("Mix::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of MixRGB Sub with the same inputs and fac 1.
 */
TEST_F(RenderGraph, constant_fold_part_mix_sub_same_fac_1)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Mix::Color to constant (0, 0, 0).");

  builder.add_attribute("Attribute")
      .add_node(ShaderNodeBuilder<MixNode>("Mix")
                    .set(&MixNode::type, NODE_MIX_SUB)
                    .set(&MixNode::use_clamp, true)
                    .set("Fac", 1.0f))
      .add_connection("Attribute::Color", "Mix::Color1")
      .add_connection("Attribute::Color", "Mix::Color2")
      .output_color("Mix::Color");

  graph.finalize(scene);
}

/*
 * Graph for testing partial folds of MixRGB with one constant argument.
 * Includes 4 tests: constant on each side with fac either unknown or 1.
 */
static void build_mix_partial_test_graph(ShaderGraphBuilder &builder,
                                         NodeMix type,
                                         float3 constval)
{
  builder
      .add_attribute("Attribute")
      /* constant on the left */
      .add_node(ShaderNodeBuilder<MixNode>("Mix_Cx_Fx")
                    .set(&MixNode::type, type)
                    .set(&MixNode::use_clamp, false)
                    .set("Color1", constval))
      .add_node(ShaderNodeBuilder<MixNode>("Mix_Cx_F1")
                    .set(&MixNode::type, type)
                    .set(&MixNode::use_clamp, false)
                    .set("Color1", constval)
                    .set("Fac", 1.0f))
      .add_connection("Attribute::Fac", "Mix_Cx_Fx::Fac")
      .add_connection("Attribute::Color", "Mix_Cx_Fx::Color2")
      .add_connection("Attribute::Color", "Mix_Cx_F1::Color2")
      /* constant on the right */
      .add_node(ShaderNodeBuilder<MixNode>("Mix_xC_Fx")
                    .set(&MixNode::type, type)
                    .set(&MixNode::use_clamp, false)
                    .set("Color2", constval))
      .add_node(ShaderNodeBuilder<MixNode>("Mix_xC_F1")
                    .set(&MixNode::type, type)
                    .set(&MixNode::use_clamp, false)
                    .set("Color2", constval)
                    .set("Fac", 1.0f))
      .add_connection("Attribute::Fac", "Mix_xC_Fx::Fac")
      .add_connection("Attribute::Color", "Mix_xC_Fx::Color1")
      .add_connection("Attribute::Color", "Mix_xC_F1::Color1")
      /* results of actual tests simply added up to connect to output */
      .add_node(ShaderNodeBuilder<MixNode>("Out12")
                    .set(&MixNode::type, NODE_MIX_ADD)
                    .set(&MixNode::use_clamp, true)
                    .set("Fac", 1.0f))
      .add_node(ShaderNodeBuilder<MixNode>("Out34")
                    .set(&MixNode::type, NODE_MIX_ADD)
                    .set(&MixNode::use_clamp, true)
                    .set("Fac", 1.0f))
      .add_node(ShaderNodeBuilder<MixNode>("Out1234")
                    .set(&MixNode::type, NODE_MIX_ADD)
                    .set(&MixNode::use_clamp, true)
                    .set("Fac", 1.0f))
      .add_connection("Mix_Cx_Fx::Color", "Out12::Color1")
      .add_connection("Mix_Cx_F1::Color", "Out12::Color2")
      .add_connection("Mix_xC_Fx::Color", "Out34::Color1")
      .add_connection("Mix_xC_F1::Color", "Out34::Color2")
      .add_connection("Out12::Color", "Out1234::Color1")
      .add_connection("Out34::Color", "Out1234::Color2")
      .output_color("Out1234::Color");
}

/*
 * Tests: partial folding for RGB Add with known 0.
 */
TEST_F(RenderGraph, constant_fold_part_mix_add_0)
{
  EXPECT_ANY_MESSAGE(log);
  /* 0 + X (fac 1) == X */
  INVALID_INFO_MESSAGE(log, "Folding Mix_Cx_Fx::Color");
  CORRECT_INFO_MESSAGE(log, "Folding Mix_Cx_F1::Color to socket Attribute::Color.");
  /* X + 0 (fac ?) == X */
  CORRECT_INFO_MESSAGE(log, "Folding Mix_xC_Fx::Color to socket Attribute::Color.");
  CORRECT_INFO_MESSAGE(log, "Folding Mix_xC_F1::Color to socket Attribute::Color.");
  INVALID_INFO_MESSAGE(log, "Folding Out");

  build_mix_partial_test_graph(builder, NODE_MIX_ADD, make_float3(0, 0, 0));
  graph.finalize(scene);
}

/*
 * Tests: partial folding for RGB Sub with known 0.
 */
TEST_F(RenderGraph, constant_fold_part_mix_sub_0)
{
  EXPECT_ANY_MESSAGE(log);
  INVALID_INFO_MESSAGE(log, "Folding Mix_Cx_Fx::Color");
  INVALID_INFO_MESSAGE(log, "Folding Mix_Cx_F1::Color");
  /* X - 0 (fac ?) == X */
  CORRECT_INFO_MESSAGE(log, "Folding Mix_xC_Fx::Color to socket Attribute::Color.");
  CORRECT_INFO_MESSAGE(log, "Folding Mix_xC_F1::Color to socket Attribute::Color.");
  INVALID_INFO_MESSAGE(log, "Folding Out");

  build_mix_partial_test_graph(builder, NODE_MIX_SUB, make_float3(0, 0, 0));
  graph.finalize(scene);
}

/*
 * Tests: partial folding for RGB Mul with known 1.
 */
TEST_F(RenderGraph, constant_fold_part_mix_mul_1)
{
  EXPECT_ANY_MESSAGE(log);
  /* 1 * X (fac 1) == X */
  INVALID_INFO_MESSAGE(log, "Folding Mix_Cx_Fx::Color");
  CORRECT_INFO_MESSAGE(log, "Folding Mix_Cx_F1::Color to socket Attribute::Color.");
  /* X * 1 (fac ?) == X */
  CORRECT_INFO_MESSAGE(log, "Folding Mix_xC_Fx::Color to socket Attribute::Color.");
  CORRECT_INFO_MESSAGE(log, "Folding Mix_xC_F1::Color to socket Attribute::Color.");
  INVALID_INFO_MESSAGE(log, "Folding Out");

  build_mix_partial_test_graph(builder, NODE_MIX_MUL, make_float3(1, 1, 1));
  graph.finalize(scene);
}

/*
 * Tests: partial folding for RGB Div with known 1.
 */
TEST_F(RenderGraph, constant_fold_part_mix_div_1)
{
  EXPECT_ANY_MESSAGE(log);
  INVALID_INFO_MESSAGE(log, "Folding Mix_Cx_Fx::Color");
  INVALID_INFO_MESSAGE(log, "Folding Mix_Cx_F1::Color");
  /* X / 1 (fac ?) == X */
  CORRECT_INFO_MESSAGE(log, "Folding Mix_xC_Fx::Color to socket Attribute::Color.");
  CORRECT_INFO_MESSAGE(log, "Folding Mix_xC_F1::Color to socket Attribute::Color.");
  INVALID_INFO_MESSAGE(log, "Folding Out");

  build_mix_partial_test_graph(builder, NODE_MIX_DIV, make_float3(1, 1, 1));
  graph.finalize(scene);
}

/*
 * Tests: partial folding for RGB Mul with known 0.
 */
TEST_F(RenderGraph, constant_fold_part_mix_mul_0)
{
  EXPECT_ANY_MESSAGE(log);
  /* 0 * ? (fac ?) == 0 */
  CORRECT_INFO_MESSAGE(log, "Folding Mix_Cx_Fx::Color to constant (0, 0, 0).");
  CORRECT_INFO_MESSAGE(log, "Folding Mix_Cx_F1::Color to constant (0, 0, 0).");
  /* ? * 0 (fac 1) == 0 */
  INVALID_INFO_MESSAGE(log, "Folding Mix_xC_Fx::Color");
  CORRECT_INFO_MESSAGE(log, "Folding Mix_xC_F1::Color to constant (0, 0, 0).");

  CORRECT_INFO_MESSAGE(log, "Folding Out12::Color to constant (0, 0, 0).");
  INVALID_INFO_MESSAGE(log, "Folding Out1234");

  build_mix_partial_test_graph(builder, NODE_MIX_MUL, make_float3(0, 0, 0));
  graph.finalize(scene);
}

/*
 * Tests: partial folding for RGB Div with known 0.
 */
TEST_F(RenderGraph, constant_fold_part_mix_div_0)
{
  EXPECT_ANY_MESSAGE(log);
  /* 0 / ? (fac ?) == 0 */
  CORRECT_INFO_MESSAGE(log, "Folding Mix_Cx_Fx::Color to constant (0, 0, 0).");
  CORRECT_INFO_MESSAGE(log, "Folding Mix_Cx_F1::Color to constant (0, 0, 0).");
  INVALID_INFO_MESSAGE(log, "Folding Mix_xC_Fx::Color");
  INVALID_INFO_MESSAGE(log, "Folding Mix_xC_F1::Color");

  CORRECT_INFO_MESSAGE(log, "Folding Out12::Color to constant (0, 0, 0).");
  INVALID_INFO_MESSAGE(log, "Folding Out1234");

  build_mix_partial_test_graph(builder, NODE_MIX_DIV, make_float3(0, 0, 0));
  graph.finalize(scene);
}

/*
 * Tests: Separate/Combine RGB with all constant inputs.
 */
TEST_F(RenderGraph, constant_fold_separate_combine_rgb)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding SeparateRGB::R to constant (0.3).");
  CORRECT_INFO_MESSAGE(log, "Folding SeparateRGB::G to constant (0.5).");
  CORRECT_INFO_MESSAGE(log, "Folding SeparateRGB::B to constant (0.7).");
  CORRECT_INFO_MESSAGE(log, "Folding CombineRGB::Image to constant (0.3, 0.5, 0.7).");

  builder
      .add_node(ShaderNodeBuilder<SeparateRGBNode>("SeparateRGB")
                    .set("Image", make_float3(0.3f, 0.5f, 0.7f)))
      .add_node(ShaderNodeBuilder<CombineRGBNode>("CombineRGB"))
      .add_connection("SeparateRGB::R", "CombineRGB::R")
      .add_connection("SeparateRGB::G", "CombineRGB::G")
      .add_connection("SeparateRGB::B", "CombineRGB::B")
      .output_color("CombineRGB::Image");

  graph.finalize(scene);
}

/*
 * Tests: Separate/Combine XYZ with all constant inputs.
 */
TEST_F(RenderGraph, constant_fold_separate_combine_xyz)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding SeparateXYZ::X to constant (0.3).");
  CORRECT_INFO_MESSAGE(log, "Folding SeparateXYZ::Y to constant (0.5).");
  CORRECT_INFO_MESSAGE(log, "Folding SeparateXYZ::Z to constant (0.7).");
  CORRECT_INFO_MESSAGE(log, "Folding CombineXYZ::Vector to constant (0.3, 0.5, 0.7).");
  CORRECT_INFO_MESSAGE(
      log, "Folding convert_vector_to_color::value_color to constant (0.3, 0.5, 0.7).");

  builder
      .add_node(ShaderNodeBuilder<SeparateXYZNode>("SeparateXYZ")
                    .set("Vector", make_float3(0.3f, 0.5f, 0.7f)))
      .add_node(ShaderNodeBuilder<CombineXYZNode>("CombineXYZ"))
      .add_connection("SeparateXYZ::X", "CombineXYZ::X")
      .add_connection("SeparateXYZ::Y", "CombineXYZ::Y")
      .add_connection("SeparateXYZ::Z", "CombineXYZ::Z")
      .output_color("CombineXYZ::Vector");

  graph.finalize(scene);
}

/*
 * Tests: Separate/Combine HSV with all constant inputs.
 */
TEST_F(RenderGraph, constant_fold_separate_combine_hsv)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding SeparateHSV::H to constant (0.583333).");
  CORRECT_INFO_MESSAGE(log, "Folding SeparateHSV::S to constant (0.571429).");
  CORRECT_INFO_MESSAGE(log, "Folding SeparateHSV::V to constant (0.7).");
  CORRECT_INFO_MESSAGE(log, "Folding CombineHSV::Color to constant (0.3, 0.5, 0.7).");

  builder
      .add_node(ShaderNodeBuilder<SeparateHSVNode>("SeparateHSV")
                    .set("Color", make_float3(0.3f, 0.5f, 0.7f)))
      .add_node(ShaderNodeBuilder<CombineHSVNode>("CombineHSV"))
      .add_connection("SeparateHSV::H", "CombineHSV::H")
      .add_connection("SeparateHSV::S", "CombineHSV::S")
      .add_connection("SeparateHSV::V", "CombineHSV::V")
      .output_color("CombineHSV::Color");

  graph.finalize(scene);
}

/*
 * Tests: Gamma with all constant inputs.
 */
TEST_F(RenderGraph, constant_fold_gamma)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Gamma::Color to constant (0.164317, 0.353553, 0.585662).");

  builder
      .add_node(ShaderNodeBuilder<GammaNode>("Gamma")
                    .set("Color", make_float3(0.3f, 0.5f, 0.7f))
                    .set("Gamma", 1.5f))
      .output_color("Gamma::Color");

  graph.finalize(scene);
}

/*
 * Tests: Gamma with one constant 0 input.
 */
TEST_F(RenderGraph, constant_fold_gamma_part_0)
{
  EXPECT_ANY_MESSAGE(log);
  INVALID_INFO_MESSAGE(log, "Folding Gamma_Cx::");
  CORRECT_INFO_MESSAGE(log, "Folding Gamma_xC::Color to constant (1, 1, 1).");

  builder
      .add_attribute("Attribute")
      /* constant on the left */
      .add_node(
          ShaderNodeBuilder<GammaNode>("Gamma_Cx").set("Color", make_float3(0.0f, 0.0f, 0.0f)))
      .add_connection("Attribute::Fac", "Gamma_Cx::Gamma")
      /* constant on the right */
      .add_node(ShaderNodeBuilder<GammaNode>("Gamma_xC").set("Gamma", 0.0f))
      .add_connection("Attribute::Color", "Gamma_xC::Color")
      /* output sum */
      .add_node(ShaderNodeBuilder<MixNode>("Out")
                    .set(&MixNode::type, NODE_MIX_ADD)
                    .set(&MixNode::use_clamp, true)
                    .set("Fac", 1.0f))
      .add_connection("Gamma_Cx::Color", "Out::Color1")
      .add_connection("Gamma_xC::Color", "Out::Color2")
      .output_color("Out::Color");

  graph.finalize(scene);
}

/*
 * Tests: Gamma with one constant 1 input.
 */
TEST_F(RenderGraph, constant_fold_gamma_part_1)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Gamma_Cx::Color to constant (1, 1, 1).");
  CORRECT_INFO_MESSAGE(log, "Folding Gamma_xC::Color to socket Attribute::Color.");

  builder
      .add_attribute("Attribute")
      /* constant on the left */
      .add_node(
          ShaderNodeBuilder<GammaNode>("Gamma_Cx").set("Color", make_float3(1.0f, 1.0f, 1.0f)))
      .add_connection("Attribute::Fac", "Gamma_Cx::Gamma")
      /* constant on the right */
      .add_node(ShaderNodeBuilder<GammaNode>("Gamma_xC").set("Gamma", 1.0f))
      .add_connection("Attribute::Color", "Gamma_xC::Color")
      /* output sum */
      .add_node(ShaderNodeBuilder<MixNode>("Out")
                    .set(&MixNode::type, NODE_MIX_ADD)
                    .set(&MixNode::use_clamp, true)
                    .set("Fac", 1.0f))
      .add_connection("Gamma_Cx::Color", "Out::Color1")
      .add_connection("Gamma_xC::Color", "Out::Color2")
      .output_color("Out::Color");

  graph.finalize(scene);
}

/*
 * Tests: BrightnessContrast with all constant inputs.
 */
TEST_F(RenderGraph, constant_fold_bright_contrast)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding BrightContrast::Color to constant (0.16, 0.6, 1.04).");

  builder
      .add_node(ShaderNodeBuilder<BrightContrastNode>("BrightContrast")
                    .set("Color", make_float3(0.3f, 0.5f, 0.7f))
                    .set("Bright", 0.1f)
                    .set("Contrast", 1.2f))
      .output_color("BrightContrast::Color");

  graph.finalize(scene);
}

/*
 * Tests: blackbody with all constant inputs.
 */
TEST_F(RenderGraph, constant_fold_blackbody)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Blackbody::Color to constant (3.94163, 0.226523, 0).");

  builder.add_node(ShaderNodeBuilder<BlackbodyNode>("Blackbody").set("Temperature", 1200.0f))
      .output_color("Blackbody::Color");

  graph.finalize(scene);
}

/* A Note About The Math Node
 *
 * The clamp option is implemented using graph expansion, where a
 * Clamp node named "clamp" is added and connected to the output.
 * So the final result is actually from the node "clamp".
 */

/*
 * Tests: Math with all constant inputs (clamp false).
 */
TEST_F(RenderGraph, constant_fold_math)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Math::Value to constant (1.6).");

  builder
      .add_node(ShaderNodeBuilder<MathNode>("Math")
                    .set(&MathNode::type, NODE_MATH_ADD)
                    .set(&MathNode::use_clamp, false)
                    .set("Value1", 0.7f)
                    .set("Value2", 0.9f))
      .output_value("Math::Value");

  graph.finalize(scene);
}

/*
 * Tests: Math with all constant inputs (clamp true).
 */
TEST_F(RenderGraph, constant_fold_math_clamp)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding clamp::Result to constant (1).");

  builder
      .add_node(ShaderNodeBuilder<MathNode>("Math")
                    .set(&MathNode::type, NODE_MATH_ADD)
                    .set(&MathNode::use_clamp, true)
                    .set("Value1", 0.7f)
                    .set("Value2", 0.9f))
      .output_value("Math::Value");

  graph.finalize(scene);
}

/*
 * Graph for testing partial folds of Math with one constant argument.
 * Includes 2 tests: constant on each side.
 */
static void build_math_partial_test_graph(ShaderGraphBuilder &builder,
                                          NodeMathType type,
                                          float constval)
{
  builder
      .add_attribute("Attribute")
      /* constant on the left */
      .add_node(ShaderNodeBuilder<MathNode>("Math_Cx")
                    .set(&MathNode::type, type)
                    .set(&MathNode::use_clamp, false)
                    .set("Value1", constval))
      .add_connection("Attribute::Fac", "Math_Cx::Value2")
      /* constant on the right */
      .add_node(ShaderNodeBuilder<MathNode>("Math_xC")
                    .set(&MathNode::type, type)
                    .set(&MathNode::use_clamp, false)
                    .set("Value2", constval))
      .add_connection("Attribute::Fac", "Math_xC::Value1")
      /* output sum */
      .add_node(ShaderNodeBuilder<MathNode>("Out")
                    .set(&MathNode::type, NODE_MATH_ADD)
                    .set(&MathNode::use_clamp, true))
      .add_connection("Math_Cx::Value", "Out::Value1")
      .add_connection("Math_xC::Value", "Out::Value2")
      .output_value("Out::Value");
}

/*
 * Tests: partial folding for Math Add with known 0.
 */
TEST_F(RenderGraph, constant_fold_part_math_add_0)
{
  EXPECT_ANY_MESSAGE(log);
  /* X + 0 == 0 + X == X */
  CORRECT_INFO_MESSAGE(log, "Folding Math_Cx::Value to socket Attribute::Fac.");
  CORRECT_INFO_MESSAGE(log, "Folding Math_xC::Value to socket Attribute::Fac.");
  INVALID_INFO_MESSAGE(log, "Folding clamp::");

  build_math_partial_test_graph(builder, NODE_MATH_ADD, 0.0f);
  graph.finalize(scene);
}

/*
 * Tests: partial folding for Math Sub with known 0.
 */
TEST_F(RenderGraph, constant_fold_part_math_sub_0)
{
  EXPECT_ANY_MESSAGE(log);
  /* X - 0 == X */
  INVALID_INFO_MESSAGE(log, "Folding Math_Cx::");
  CORRECT_INFO_MESSAGE(log, "Folding Math_xC::Value to socket Attribute::Fac.");
  INVALID_INFO_MESSAGE(log, "Folding clamp::");

  build_math_partial_test_graph(builder, NODE_MATH_SUBTRACT, 0.0f);
  graph.finalize(scene);
}

/*
 * Tests: partial folding for Math Mul with known 1.
 */
TEST_F(RenderGraph, constant_fold_part_math_mul_1)
{
  EXPECT_ANY_MESSAGE(log);
  /* X * 1 == 1 * X == X */
  CORRECT_INFO_MESSAGE(log, "Folding Math_Cx::Value to socket Attribute::Fac.");
  CORRECT_INFO_MESSAGE(log, "Folding Math_xC::Value to socket Attribute::Fac.");
  INVALID_INFO_MESSAGE(log, "Folding clamp::");

  build_math_partial_test_graph(builder, NODE_MATH_MULTIPLY, 1.0f);
  graph.finalize(scene);
}

/*
 * Tests: partial folding for Math Div with known 1.
 */
TEST_F(RenderGraph, constant_fold_part_math_div_1)
{
  EXPECT_ANY_MESSAGE(log);
  /* X / 1 == X */
  INVALID_INFO_MESSAGE(log, "Folding Math_Cx::");
  CORRECT_INFO_MESSAGE(log, "Folding Math_xC::Value to socket Attribute::Fac.");
  INVALID_INFO_MESSAGE(log, "Folding clamp::");

  build_math_partial_test_graph(builder, NODE_MATH_DIVIDE, 1.0f);
  graph.finalize(scene);
}

/*
 * Tests: partial folding for Math Mul with known 0.
 */
TEST_F(RenderGraph, constant_fold_part_math_mul_0)
{
  EXPECT_ANY_MESSAGE(log);
  /* X * 0 == 0 * X == 0 */
  CORRECT_INFO_MESSAGE(log, "Folding Math_Cx::Value to constant (0).");
  CORRECT_INFO_MESSAGE(log, "Folding Math_xC::Value to constant (0).");
  CORRECT_INFO_MESSAGE(log, "Folding clamp::Result to constant (0)");
  CORRECT_INFO_MESSAGE(log, "Discarding closure EmissionNode.");

  build_math_partial_test_graph(builder, NODE_MATH_MULTIPLY, 0.0f);
  graph.finalize(scene);
}

/*
 * Tests: partial folding for Math Div with known 0.
 */
TEST_F(RenderGraph, constant_fold_part_math_div_0)
{
  EXPECT_ANY_MESSAGE(log);
  /* 0 / X == 0 */
  CORRECT_INFO_MESSAGE(log, "Folding Math_Cx::Value to constant (0).");
  INVALID_INFO_MESSAGE(log, "Folding Math_xC::");
  INVALID_INFO_MESSAGE(log, "Folding clamp::");

  build_math_partial_test_graph(builder, NODE_MATH_DIVIDE, 0.0f);
  graph.finalize(scene);
}

/*
 * Tests: partial folding for Math Power with known 0.
 */
TEST_F(RenderGraph, constant_fold_part_math_pow_0)
{
  EXPECT_ANY_MESSAGE(log);
  /* X ^ 0 == 1 */
  INVALID_INFO_MESSAGE(log, "Folding Math_Cx::");
  CORRECT_INFO_MESSAGE(log, "Folding Math_xC::Value to constant (1).");
  INVALID_INFO_MESSAGE(log, "Folding clamp::");

  build_math_partial_test_graph(builder, NODE_MATH_POWER, 0.0f);
  graph.finalize(scene);
}

/*
 * Tests: partial folding for Math Power with known 1.
 */
TEST_F(RenderGraph, constant_fold_part_math_pow_1)
{
  EXPECT_ANY_MESSAGE(log);
  /* 1 ^ X == 1; X ^ 1 == X */
  CORRECT_INFO_MESSAGE(log, "Folding Math_Cx::Value to constant (1)");
  CORRECT_INFO_MESSAGE(log, "Folding Math_xC::Value to socket Attribute::Fac.");
  INVALID_INFO_MESSAGE(log, "Folding clamp::");

  build_math_partial_test_graph(builder, NODE_MATH_POWER, 1.0f);
  graph.finalize(scene);
}

/*
 * Tests: Vector Math with all constant inputs.
 */
TEST_F(RenderGraph, constant_fold_vector_math)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding VectorMath::Vector to constant (3, 0, 0).");

  builder
      .add_node(ShaderNodeBuilder<VectorMathNode>("VectorMath")
                    .set(&VectorMathNode::type, NODE_VECTOR_MATH_SUBTRACT)
                    .set("Vector1", make_float3(1.3f, 0.5f, 0.7f))
                    .set("Vector2", make_float3(-1.7f, 0.5f, 0.7f)))
      .output_color("VectorMath::Vector");

  graph.finalize(scene);
}

/*
 * Graph for testing partial folds of Vector Math with one constant argument.
 * Includes 2 tests: constant on each side.
 */
static void build_vecmath_partial_test_graph(ShaderGraphBuilder &builder,
                                             NodeVectorMathType type,
                                             float3 constval)
{
  builder
      .add_attribute("Attribute")
      /* constant on the left */
      .add_node(ShaderNodeBuilder<VectorMathNode>("Math_Cx")
                    .set(&VectorMathNode::type, type)
                    .set("Vector1", constval))
      .add_connection("Attribute::Vector", "Math_Cx::Vector2")
      /* constant on the right */
      .add_node(ShaderNodeBuilder<VectorMathNode>("Math_xC")
                    .set(&VectorMathNode::type, type)
                    .set("Vector2", constval))
      .add_connection("Attribute::Vector", "Math_xC::Vector1")
      /* output sum */
      .add_node(ShaderNodeBuilder<VectorMathNode>("Out").set(&VectorMathNode::type,
                                                             NODE_VECTOR_MATH_ADD))
      .add_connection("Math_Cx::Vector", "Out::Vector1")
      .add_connection("Math_xC::Vector", "Out::Vector2")
      .output_color("Out::Vector");
}

/*
 * Tests: partial folding for Vector Math Add with known 0.
 */
TEST_F(RenderGraph, constant_fold_part_vecmath_add_0)
{
  EXPECT_ANY_MESSAGE(log);
  /* X + 0 == 0 + X == X */
  CORRECT_INFO_MESSAGE(log, "Folding Math_Cx::Vector to socket Attribute::Vector.");
  CORRECT_INFO_MESSAGE(log, "Folding Math_xC::Vector to socket Attribute::Vector.");
  INVALID_INFO_MESSAGE(log, "Folding Out::");

  build_vecmath_partial_test_graph(builder, NODE_VECTOR_MATH_ADD, make_float3(0, 0, 0));
  graph.finalize(scene);
}

/*
 * Tests: partial folding for Vector Math Sub with known 0.
 */
TEST_F(RenderGraph, constant_fold_part_vecmath_sub_0)
{
  EXPECT_ANY_MESSAGE(log);
  /* X - 0 == X */
  INVALID_INFO_MESSAGE(log, "Folding Math_Cx::");
  CORRECT_INFO_MESSAGE(log, "Folding Math_xC::Vector to socket Attribute::Vector.");
  INVALID_INFO_MESSAGE(log, "Folding Out::");

  build_vecmath_partial_test_graph(builder, NODE_VECTOR_MATH_SUBTRACT, make_float3(0, 0, 0));
  graph.finalize(scene);
}

/*
 * Tests: partial folding for Vector Math Cross Product with known 0.
 */
TEST_F(RenderGraph, constant_fold_part_vecmath_cross_0)
{
  EXPECT_ANY_MESSAGE(log);
  /* X * 0 == 0 * X == X */
  CORRECT_INFO_MESSAGE(log, "Folding Math_Cx::Vector to constant (0, 0, 0).");
  CORRECT_INFO_MESSAGE(log, "Folding Math_xC::Vector to constant (0, 0, 0).");
  CORRECT_INFO_MESSAGE(log, "Folding Out::Vector to constant (0, 0, 0).");
  CORRECT_INFO_MESSAGE(log, "Discarding closure EmissionNode.");

  build_vecmath_partial_test_graph(builder, NODE_VECTOR_MATH_CROSS_PRODUCT, make_float3(0, 0, 0));
  graph.finalize(scene);
}

/*
 * Tests: Bump with no height input folded to Normal input.
 */
TEST_F(RenderGraph, constant_fold_bump)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Bump::Normal to socket Geometry1::Normal.");

  builder.add_node(ShaderNodeBuilder<GeometryNode>("Geometry1"))
      .add_node(ShaderNodeBuilder<BumpNode>("Bump"))
      .add_connection("Geometry1::Normal", "Bump::Normal")
      .output_color("Bump::Normal");

  graph.finalize(scene);
}

/*
 * Tests: Bump with no inputs folded to Geometry::Normal.
 */
TEST_F(RenderGraph, constant_fold_bump_no_input)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Bump::Normal to socket geometry::Normal.");

  builder.add_node(ShaderNodeBuilder<BumpNode>("Bump")).output_color("Bump::Normal");

  graph.finalize(scene);
}

template<class T> void init_test_curve(array<T> &buffer, T start, T end, int steps)
{
  buffer.resize(steps);

  for (int i = 0; i < steps; i++) {
    buffer[i] = lerp(start, end, float(i) / (steps - 1));
  }
}

/*
 * Tests:
 *  - Folding of RGB Curves with all constant inputs.
 */
TEST_F(RenderGraph, constant_fold_rgb_curves)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Curves::Color to constant (0.275, 0.5, 0.475).");

  array<float3> curve;
  init_test_curve(curve, make_float3(0.0f, 0.25f, 1.0f), make_float3(1.0f, 0.75f, 0.0f), 257);

  builder
      .add_node(ShaderNodeBuilder<RGBCurvesNode>("Curves")
                    .set(&CurvesNode::curves, curve)
                    .set(&CurvesNode::min_x, 0.1f)
                    .set(&CurvesNode::max_x, 0.9f)
                    .set("Fac", 0.5f)
                    .set("Color", make_float3(0.3f, 0.5f, 0.7f)))
      .output_color("Curves::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of RGB Curves with zero Fac.
 */
TEST_F(RenderGraph, constant_fold_rgb_curves_fac_0)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Curves::Color to socket Attribute::Color.");

  array<float3> curve;
  init_test_curve(curve, make_float3(0.0f, 0.25f, 1.0f), make_float3(1.0f, 0.75f, 0.0f), 257);

  builder.add_attribute("Attribute")
      .add_node(ShaderNodeBuilder<RGBCurvesNode>("Curves")
                    .set(&CurvesNode::curves, curve)
                    .set(&CurvesNode::min_x, 0.1f)
                    .set(&CurvesNode::max_x, 0.9f)
                    .set("Fac", 0.0f))
      .add_connection("Attribute::Color", "Curves::Color")
      .output_color("Curves::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of RGB Curves with zero Fac and all constant inputs.
 */
TEST_F(RenderGraph, constant_fold_rgb_curves_fac_0_const)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Curves::Color to constant (0.3, 0.5, 0.7).");

  array<float3> curve;
  init_test_curve(curve, make_float3(0.0f, 0.25f, 1.0f), make_float3(1.0f, 0.75f, 0.0f), 257);

  builder
      .add_node(ShaderNodeBuilder<RGBCurvesNode>("Curves")
                    .set(&CurvesNode::curves, curve)
                    .set(&CurvesNode::min_x, 0.1f)
                    .set(&CurvesNode::max_x, 0.9f)
                    .set("Fac", 0.0f)
                    .set("Color", make_float3(0.3f, 0.5f, 0.7f)))
      .output_color("Curves::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of Vector Curves with all constant inputs.
 */
TEST_F(RenderGraph, constant_fold_vector_curves)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Curves::Vector to constant (0.275, 0.5, 0.475).");

  array<float3> curve;
  init_test_curve(curve, make_float3(0.0f, 0.25f, 1.0f), make_float3(1.0f, 0.75f, 0.0f), 257);

  builder
      .add_node(ShaderNodeBuilder<VectorCurvesNode>("Curves")
                    .set(&CurvesNode::curves, curve)
                    .set(&CurvesNode::min_x, 0.1f)
                    .set(&CurvesNode::max_x, 0.9f)
                    .set("Fac", 0.5f)
                    .set("Vector", make_float3(0.3f, 0.5f, 0.7f)))
      .output_color("Curves::Vector");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of Vector Curves with zero Fac.
 */
TEST_F(RenderGraph, constant_fold_vector_curves_fac_0)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Curves::Vector to socket Attribute::Vector.");

  array<float3> curve;
  init_test_curve(curve, make_float3(0.0f, 0.25f, 1.0f), make_float3(1.0f, 0.75f, 0.0f), 257);

  builder.add_attribute("Attribute")
      .add_node(ShaderNodeBuilder<VectorCurvesNode>("Curves")
                    .set(&CurvesNode::curves, curve)
                    .set(&CurvesNode::min_x, 0.1f)
                    .set(&CurvesNode::max_x, 0.9f)
                    .set("Fac", 0.0f))
      .add_connection("Attribute::Vector", "Curves::Vector")
      .output_color("Curves::Vector");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of Color Ramp with all constant inputs.
 */
TEST_F(RenderGraph, constant_fold_rgb_ramp)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Ramp::Color to constant (0.14, 0.39, 0.64).");
  CORRECT_INFO_MESSAGE(log, "Folding Ramp::Alpha to constant (0.89).");

  array<float3> curve;
  array<float> alpha;
  init_test_curve(curve, make_float3(0.0f, 0.25f, 0.5f), make_float3(0.25f, 0.5f, 0.75f), 9);
  init_test_curve(alpha, 0.75f, 1.0f, 9);

  builder
      .add_node(ShaderNodeBuilder<RGBRampNode>("Ramp")
                    .set(&RGBRampNode::ramp, curve)
                    .set(&RGBRampNode::ramp_alpha, alpha)
                    .set(&RGBRampNode::interpolate, true)
                    .set("Fac", 0.56f))
      .add_node(ShaderNodeBuilder<MixNode>("Mix").set(&MixNode::type, NODE_MIX_ADD))
      .add_connection("Ramp::Color", "Mix::Color1")
      .add_connection("Ramp::Alpha", "Mix::Color2")
      .output_color("Mix::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of Color Ramp with all constant inputs (interpolate false).
 */
TEST_F(RenderGraph, constant_fold_rgb_ramp_flat)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log, "Folding Ramp::Color to constant (0.125, 0.375, 0.625).");
  CORRECT_INFO_MESSAGE(log, "Folding Ramp::Alpha to constant (0.875).");

  array<float3> curve;
  array<float> alpha;
  init_test_curve(curve, make_float3(0.0f, 0.25f, 0.5f), make_float3(0.25f, 0.5f, 0.75f), 9);
  init_test_curve(alpha, 0.75f, 1.0f, 9);

  builder
      .add_node(ShaderNodeBuilder<RGBRampNode>("Ramp")
                    .set(&RGBRampNode::ramp, curve)
                    .set(&RGBRampNode::ramp_alpha, alpha)
                    .set(&RGBRampNode::interpolate, false)
                    .set("Fac", 0.56f))
      .add_node(ShaderNodeBuilder<MixNode>("Mix").set(&MixNode::type, NODE_MIX_ADD))
      .add_connection("Ramp::Color", "Mix::Color1")
      .add_connection("Ramp::Alpha", "Mix::Color2")
      .output_color("Mix::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of redundant conversion of float to color to float.
 */
TEST_F(RenderGraph, constant_fold_convert_float_color_float)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log,
                       "Folding Invert::Color to socket convert_float_to_color::value_color.");
  CORRECT_INFO_MESSAGE(log,
                       "Folding convert_color_to_float::value_float to socket Attribute::Fac.");

  builder.add_attribute("Attribute")
      .add_node(ShaderNodeBuilder<InvertNode>("Invert").set("Fac", 0.0f))
      .add_connection("Attribute::Fac", "Invert::Color")
      .output_value("Invert::Color");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - Folding of redundant conversion of color to vector to color.
 */
TEST_F(RenderGraph, constant_fold_convert_color_vector_color)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log,
                       "Folding VecAdd::Vector to socket convert_color_to_vector::value_vector.");
  CORRECT_INFO_MESSAGE(log,
                       "Folding convert_vector_to_color::value_color to socket Attribute::Color.");

  builder.add_attribute("Attribute")
      .add_node(ShaderNodeBuilder<VectorMathNode>("VecAdd")
                    .set(&VectorMathNode::type, NODE_VECTOR_MATH_ADD)
                    .set("Vector2", make_float3(0, 0, 0)))
      .add_connection("Attribute::Color", "VecAdd::Vector1")
      .output_color("VecAdd::Vector");

  graph.finalize(scene);
}

/*
 * Tests:
 *  - NOT folding conversion of color to float to color.
 */
TEST_F(RenderGraph, constant_fold_convert_color_float_color)
{
  EXPECT_ANY_MESSAGE(log);
  CORRECT_INFO_MESSAGE(log,
                       "Folding MathAdd::Value to socket convert_color_to_float::value_float.");
  INVALID_INFO_MESSAGE(log, "Folding convert_float_to_color::");

  builder.add_attribute("Attribute")
      .add_node(ShaderNodeBuilder<MathNode>("MathAdd")
                    .set(&MathNode::type, NODE_MATH_ADD)
                    .set("Value2", 0.0f))
      .add_connection("Attribute::Color", "MathAdd::Value1")
      .output_color("MathAdd::Value");

  graph.finalize(scene);
}

CCL_NAMESPACE_END
