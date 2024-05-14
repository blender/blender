/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_mapping_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector")
      .default_value({0.0f, 0.0f, 0.0f})
      .min(-FLT_MAX)
      .max(FLT_MAX)
      .description("The vector to be transformed");
  b.add_input<decl::Vector>("Location")
      .default_value({0.0f, 0.0f, 0.0f})
      .min(-FLT_MAX)
      .max(FLT_MAX)
      .subtype(PROP_TRANSLATION)
      .description("The amount of translation along each axis");
  b.add_input<decl::Vector>("Rotation")
      .default_value({0.0f, 0.0f, 0.0f})
      .min(-FLT_MAX)
      .max(FLT_MAX)
      .subtype(PROP_EULER)
      .description("The amount of rotation along each axis, XYZ order");
  b.add_input<decl::Vector>("Scale")
      .default_value({1.0f, 1.0f, 1.0f})
      .min(-FLT_MAX)
      .max(FLT_MAX)
      .subtype(PROP_XYZ)
      .description("The amount of scaling along each axis");
  b.add_output<decl::Vector>("Vector");
}

static void node_shader_buts_mapping(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "vector_type", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

static const char *gpu_shader_get_name(int mode)
{
  switch (mode) {
    case NODE_MAPPING_TYPE_POINT:
      return "mapping_point";
    case NODE_MAPPING_TYPE_TEXTURE:
      return "mapping_texture";
    case NODE_MAPPING_TYPE_VECTOR:
      return "mapping_vector";
    case NODE_MAPPING_TYPE_NORMAL:
      return "mapping_normal";
  }
  return nullptr;
}

static int gpu_shader_mapping(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData * /*execdata*/,
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  if (gpu_shader_get_name(node->custom1)) {
    return GPU_stack_link(mat, node, gpu_shader_get_name(node->custom1), in, out);
  }

  return 0;
}

static void node_shader_update_mapping(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sock = bke::nodeFindSocket(node, SOCK_IN, "Location");
  bke::nodeSetSocketAvailability(
      ntree, sock, ELEM(node->custom1, NODE_MAPPING_TYPE_POINT, NODE_MAPPING_TYPE_TEXTURE));
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem vector = get_input_value("Vector", NodeItem::Type::Vector3);
  NodeItem scale = get_input_value("Scale", NodeItem::Type::Vector3);
  NodeItem rotation = get_input_value("Rotation", NodeItem::Type::Vector3) *
                      val(float(180.0f / M_PI));

  int type = node_->custom1;
  switch (type) {
    case NODE_MAPPING_TYPE_POINT: {
      NodeItem location = get_input_value("Location", NodeItem::Type::Vector3);
      return (vector * scale).rotate(rotation) + location;
    }
    case NODE_MAPPING_TYPE_TEXTURE: {
      NodeItem location = get_input_value("Location", NodeItem::Type::Vector3);
      return (vector - location).rotate(rotation, true) / scale;
    }
    case NODE_MAPPING_TYPE_VECTOR: {
      return (vector * scale).rotate(rotation * val(MaterialX::Vector3(1.0f, 1.0f, -1.0f)));
    }
    case NODE_MAPPING_TYPE_NORMAL: {
      return (vector / scale).rotate(rotation).normalize();
    }
    default:
      BLI_assert_unreachable();
  }
  return empty();
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_mapping_cc

void register_node_type_sh_mapping()
{
  namespace file_ns = blender::nodes::node_shader_mapping_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_MAPPING, "Mapping", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_mapping;
  ntype.gpu_fn = file_ns::gpu_shader_mapping;
  ntype.updatefunc = file_ns::node_shader_update_mapping;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::nodeRegisterType(&ntype);
}
