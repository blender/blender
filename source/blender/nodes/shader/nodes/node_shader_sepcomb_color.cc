/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

static void node_combsep_color_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeCombSepColor *data = MEM_cnew<NodeCombSepColor>(__func__);
  data->mode = NODE_COMBSEP_COLOR_RGB;
  node->storage = data;
}

/* **************** SEPARATE COLOR ******************** */

namespace blender::nodes::node_shader_separate_color_cc {

NODE_STORAGE_FUNCS(NodeCombSepColor)

static void sh_node_sepcolor_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_output<decl::Float>("Red");
  b.add_output<decl::Float>("Green");
  b.add_output<decl::Float>("Blue");
}

static void node_sepcolor_update(bNodeTree * /*ntree*/, bNode *node)
{
  const NodeCombSepColor &storage = node_storage(*node);
  node_combsep_color_label(&node->outputs, (NodeCombSepColorMode)storage.mode);
}

static const char *gpu_shader_get_name(int mode)
{
  switch (mode) {
    case NODE_COMBSEP_COLOR_RGB:
      return "separate_color_rgb";
    case NODE_COMBSEP_COLOR_HSV:
      return "separate_color_hsv";
    case NODE_COMBSEP_COLOR_HSL:
      return "separate_color_hsl";
  }

  return nullptr;
}

static int gpu_shader_sepcolor(GPUMaterial *mat,
                               bNode *node,
                               bNodeExecData * /*execdata*/,
                               GPUNodeStack *in,
                               GPUNodeStack *out)
{
  const NodeCombSepColor &storage = node_storage(*node);
  const char *name = gpu_shader_get_name(storage.mode);
  if (name != nullptr) {
    return GPU_stack_link(mat, node, name, in, out);
  }

  return 0;
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  int mode = static_cast<NodeCombSepColor *>(node_->storage)->mode;
  NodeItem color = get_input_value("Color", NodeItem::Type::Color3);

  NodeItem convert = empty();
  switch (mode) {
    case NODE_COMBSEP_COLOR_RGB:
      convert = color;
      break;
    case NODE_COMBSEP_COLOR_HSV:
    case NODE_COMBSEP_COLOR_HSL:
      /* NOTE: HSL is unsupported color model, using HSV instead */
      convert = create_node("rgbtohsv", NodeItem::Type::Color3, {{"in", color}});
      break;
    default:
      BLI_assert_unreachable();
  }

  int index = STREQ(socket_out_->name, "Red") ? 0 : STREQ(socket_out_->name, "Green") ? 1 : 2;
  return convert[index];
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_separate_color_cc

void register_node_type_sh_sepcolor()
{
  namespace file_ns = blender::nodes::node_shader_separate_color_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_SEPARATE_COLOR, "Separate Color", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::sh_node_sepcolor_declare;
  ntype.updatefunc = file_ns::node_sepcolor_update;
  ntype.initfunc = node_combsep_color_init;
  blender::bke::node_type_storage(
      &ntype, "NodeCombSepColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::gpu_shader_sepcolor;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::nodeRegisterType(&ntype);
}

/* **************** COMBINE COLOR ******************** */

namespace blender::nodes::node_shader_combine_color_cc {

NODE_STORAGE_FUNCS(NodeCombSepColor)

static void sh_node_combcolor_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Red").default_value(0.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Green").default_value(0.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Blue").default_value(0.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_output<decl::Color>("Color");
}

static void node_combcolor_update(bNodeTree * /*ntree*/, bNode *node)
{
  const NodeCombSepColor &storage = node_storage(*node);
  node_combsep_color_label(&node->inputs, (NodeCombSepColorMode)storage.mode);
}

static const char *gpu_shader_get_name(int mode)
{
  switch (mode) {
    case NODE_COMBSEP_COLOR_RGB:
      return "combine_color_rgb";
    case NODE_COMBSEP_COLOR_HSV:
      return "combine_color_hsv";
    case NODE_COMBSEP_COLOR_HSL:
      return "combine_color_hsl";
  }

  return nullptr;
}

static int gpu_shader_combcolor(GPUMaterial *mat,
                                bNode *node,
                                bNodeExecData * /*execdata*/,
                                GPUNodeStack *in,
                                GPUNodeStack *out)
{
  const NodeCombSepColor &storage = node_storage(*node);
  const char *name = gpu_shader_get_name(storage.mode);
  if (name != nullptr) {
    return GPU_stack_link(mat, node, name, in, out);
  }

  return 0;
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  int mode = static_cast<NodeCombSepColor *>(node_->storage)->mode;
  NodeItem red = get_input_value("Red", NodeItem::Type::Float);
  NodeItem green = get_input_value("Green", NodeItem::Type::Float);
  NodeItem blue = get_input_value("Blue", NodeItem::Type::Float);

  NodeItem combine = create_node("combine3", NodeItem::Type::Color3);
  combine.set_input("in1", red);
  combine.set_input("in2", green);
  combine.set_input("in3", blue);

  NodeItem res = empty();
  switch (mode) {
    case NODE_COMBSEP_COLOR_RGB:
      res = combine;
      break;
    case NODE_COMBSEP_COLOR_HSV:
    case NODE_COMBSEP_COLOR_HSL:
      /* NOTE: HSL is unsupported color model, using HSV instead */
      res = create_node("hsvtorgb", NodeItem::Type::Color3);
      res.set_input("in", combine);
      break;
    default:
      BLI_assert_unreachable();
  }
  return res;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_combine_color_cc

void register_node_type_sh_combcolor()
{
  namespace file_ns = blender::nodes::node_shader_combine_color_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_COMBINE_COLOR, "Combine Color", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::sh_node_combcolor_declare;
  ntype.updatefunc = file_ns::node_combcolor_update;
  ntype.initfunc = node_combsep_color_init;
  blender::bke::node_type_storage(
      &ntype, "NodeCombSepColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::gpu_shader_combcolor;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::nodeRegisterType(&ntype);
}
