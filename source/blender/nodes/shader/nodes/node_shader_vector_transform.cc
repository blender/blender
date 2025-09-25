/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_vector_transform_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector")
      .default_value({0.5f, 0.5f, 0.5f})
      .min(-10000.0f)
      .max(10000.0f)
      .description("Vector, point, or normal which will be used for conversion between spaces");
  b.add_output<decl::Vector>("Vector");
}

static void node_shader_buts_vect_transform(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(
      ptr, "vector_type", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  layout->prop(ptr, "convert_from", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  layout->prop(ptr, "convert_to", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_vect_transform(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderVectTransform *vect = MEM_callocN<NodeShaderVectTransform>("NodeShaderVectTransform");

  /* Convert World into Object Space per default */
  vect->convert_to = 1;

  node->storage = vect;
}

static const char *get_gpufn_name_from_to(short from, short to, bool is_direction)
{
  switch (from) {
    case SHD_VECT_TRANSFORM_SPACE_OBJECT:
      switch (to) {
        case SHD_VECT_TRANSFORM_SPACE_OBJECT:
          return nullptr;
        case SHD_VECT_TRANSFORM_SPACE_WORLD:
          return is_direction ? "direction_transform_object_to_world" :
                                "point_transform_object_to_world";
        case SHD_VECT_TRANSFORM_SPACE_CAMERA:
          return is_direction ? "direction_transform_object_to_view" :
                                "point_transform_object_to_view";
      }
      break;
    case SHD_VECT_TRANSFORM_SPACE_WORLD:
      switch (to) {
        case SHD_VECT_TRANSFORM_SPACE_WORLD:
          return nullptr;
        case SHD_VECT_TRANSFORM_SPACE_CAMERA:
          return is_direction ? "direction_transform_world_to_view" :
                                "point_transform_world_to_view";
        case SHD_VECT_TRANSFORM_SPACE_OBJECT:
          return is_direction ? "direction_transform_world_to_object" :
                                "point_transform_world_to_object";
      }
      break;
    case SHD_VECT_TRANSFORM_SPACE_CAMERA:
      switch (to) {
        case SHD_VECT_TRANSFORM_SPACE_CAMERA:
          return nullptr;
        case SHD_VECT_TRANSFORM_SPACE_WORLD:
          return is_direction ? "direction_transform_view_to_world" :
                                "point_transform_view_to_world";
        case SHD_VECT_TRANSFORM_SPACE_OBJECT:
          return is_direction ? "direction_transform_view_to_object" :
                                "point_transform_view_to_object";
      }
      break;
  }
  return nullptr;
}

static int gpu_shader_vect_transform(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  GPUNodeLink *inputlink;

  NodeShaderVectTransform *nodeprop = (NodeShaderVectTransform *)node->storage;

  if (in[0].hasinput) {
    inputlink = in[0].link;
  }
  else {
    inputlink = GPU_uniform(in[0].vec);
  }

  const bool is_direction = (nodeprop->type != SHD_VECT_TRANSFORM_TYPE_POINT);
  const char *func_name = get_gpufn_name_from_to(
      nodeprop->convert_from, nodeprop->convert_to, is_direction);

  if (func_name) {
    /* For cycles we have inverted Z */
    /* TODO: pass here the correct matrices */
    if (nodeprop->convert_from == SHD_VECT_TRANSFORM_SPACE_CAMERA &&
        nodeprop->convert_to != SHD_VECT_TRANSFORM_SPACE_CAMERA)
    {
      GPU_link(mat, "invert_z", inputlink, &inputlink);
    }

    GPU_link(mat, func_name, inputlink, &out[0].link);

    if (nodeprop->convert_to == SHD_VECT_TRANSFORM_SPACE_CAMERA &&
        nodeprop->convert_from != SHD_VECT_TRANSFORM_SPACE_CAMERA)
    {
      GPU_link(mat, "invert_z", out[0].link, &out[0].link);
    }
  }
  else {
    GPU_link(mat, "set_rgb", inputlink, &out[0].link);
  }

  if (nodeprop->type == SHD_VECT_TRANSFORM_TYPE_NORMAL) {
    GPU_link(mat, "vector_normalize", out[0].link, &out[0].link);
  }

  return true;
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem res = empty();
  NodeShaderVectTransform *nodeprop = (NodeShaderVectTransform *)node_->storage;
  std::string fromspace;
  std::string tospace;
  std::string category;
  NodeItem vector = get_input_value("Vector", NodeItem::Type::Vector3);

  switch (nodeprop->convert_from) {
    case SHD_VECT_TRANSFORM_SPACE_WORLD:
      fromspace = "world";
      break;
    case SHD_VECT_TRANSFORM_SPACE_OBJECT:
      fromspace = "object";
      break;
    default:
      /* NOTE: SHD_VECT_TRANSFORM_SPACE_CAMERA don't have an implementation in MaterialX. */
      BLI_assert_unreachable();
      return vector;
  }

  switch (nodeprop->convert_to) {
    case SHD_VECT_TRANSFORM_SPACE_WORLD:
      tospace = "world";
      break;
    case SHD_VECT_TRANSFORM_SPACE_OBJECT:
      tospace = "object";
      break;
    default:
      /* NOTE: SHD_VECT_TRANSFORM_SPACE_CAMERA don't have an implementation in MaterialX. */
      BLI_assert_unreachable();
      return vector;
  }

  if (fromspace == tospace) {
    return vector;
  }

  switch (nodeprop->type) {
    case SHD_VECT_TRANSFORM_TYPE_POINT:
      category = "transformpoint";
      break;
    case SHD_VECT_TRANSFORM_TYPE_NORMAL:
      category = "transformnormal";
      break;
    case SHD_VECT_TRANSFORM_TYPE_VECTOR:
      category = "transformvector";
      break;
    default:
      BLI_assert_unreachable();
      return vector;
  }

  return create_node(category,
                     NodeItem::Type::Vector3,
                     {{"in", vector}, {"fromspace", val(fromspace)}, {"tospace", val(tospace)}});
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_vector_transform_cc

void register_node_type_sh_vect_transform()
{
  namespace file_ns = blender::nodes::node_shader_vector_transform_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeVectorTransform", SH_NODE_VECT_TRANSFORM);
  ntype.ui_name = "Vector Transform";
  ntype.ui_description =
      "Convert a vector, point, or normal between world, camera, and object coordinate space";
  ntype.enum_name_legacy = "VECT_TRANSFORM";
  ntype.nclass = NODE_CLASS_OP_VECTOR;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_vect_transform;
  ntype.initfunc = file_ns::node_shader_init_vect_transform;
  blender::bke::node_type_storage(
      ntype, "NodeShaderVectTransform", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::gpu_shader_vect_transform;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
