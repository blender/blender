/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_node_types.h"
#include "node_shader_util.hh"
#include "node_util.hh"

#include "BKE_context.hh"
#include "BKE_node_runtime.hh"

#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_normal_map_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Strength")
      .default_value(1.0f)
      .min(0.0f)
      .max(10.0f)
      .description("Strength of the normal mapping effect")
      .translation_context(BLT_I18NCONTEXT_AMOUNT);
  b.add_input<decl::Color>("Color")
      .default_value({0.5f, 0.5f, 1.0f, 1.0f})
      .description("Color that encodes the normal map in the specified space");
  b.add_output<decl::Vector>("Normal");
}

static void node_shader_buts_normal_map(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  layout->prop(ptr, "space", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  if (RNA_enum_get(ptr, "space") == SHD_SPACE_TANGENT) {
    PointerRNA obptr = CTX_data_pointer_get(C, "active_object");
    Object *object = static_cast<Object *>(obptr.data);

    if (object && object->type == OB_MESH) {
      Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

      if (depsgraph) {
        Object *object_eval = DEG_get_evaluated(depsgraph, object);
        PointerRNA dataptr = RNA_id_pointer_create(static_cast<ID *>(object_eval->data));
        layout->prop_search(ptr, "uv_map", &dataptr, "uv_layers", "", ICON_GROUP_UVS);
        return;
      }
    }

    layout->prop(ptr, "uv_map", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  }
}

static void node_shader_init_normal_map(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderNormalMap *attr = MEM_callocN<NodeShaderNormalMap>("NodeShaderNormalMap");
  node->storage = attr;
}

static int gpu_shader_normal_map(GPUMaterial *mat,
                                 bNode *node,
                                 bNodeExecData * /*execdata*/,
                                 GPUNodeStack *in,
                                 GPUNodeStack *out)
{
  NodeShaderNormalMap *nm = static_cast<NodeShaderNormalMap *>(node->storage);

  GPUNodeLink *strength;
  if (in[0].link) {
    strength = in[0].link;
  }
  else if (node->runtime->original) {
    bNodeSocket *socket = static_cast<bNodeSocket *>(
        BLI_findlink(&node->runtime->original->inputs, 0));
    bNodeSocketValueFloat *socket_data = static_cast<bNodeSocketValueFloat *>(
        socket->default_value);
    strength = GPU_uniform(&socket_data->value);
  }
  else {
    strength = GPU_constant(in[0].vec);
  }

  GPUNodeLink *newnormal;
  if (in[1].link) {
    newnormal = in[1].link;
  }
  else if (node->runtime->original) {
    bNodeSocket *socket = static_cast<bNodeSocket *>(
        BLI_findlink(&node->runtime->original->inputs, 1));
    bNodeSocketValueRGBA *socket_data = static_cast<bNodeSocketValueRGBA *>(socket->default_value);
    newnormal = GPU_uniform(socket_data->value);
  }
  else {
    newnormal = GPU_constant(in[1].vec);
  }

  const char *color_to_normal_fnc_name = "color_to_normal_new_shading";
  if (ELEM(nm->space, SHD_SPACE_BLENDER_OBJECT, SHD_SPACE_BLENDER_WORLD)) {
    color_to_normal_fnc_name = "color_to_blender_normal_new_shading";
  }

  GPU_link(mat, color_to_normal_fnc_name, newnormal, &newnormal);
  switch (nm->space) {
    case SHD_SPACE_TANGENT:
      GPU_material_flag_set(mat, GPU_MATFLAG_OBJECT_INFO);
      /* We return directly from the node_normal_map as strength
       * has already been applied for the tangent case */
      GPU_link(mat,
               "node_normal_map",
               GPU_attribute(mat, CD_TANGENT, nm->uv_map),
               strength,
               newnormal,
               &out[0].link);
      return true;
    case SHD_SPACE_OBJECT:
    case SHD_SPACE_BLENDER_OBJECT:
      GPU_link(mat, "normal_transform_object_to_world", newnormal, &newnormal);
      break;
    case SHD_SPACE_WORLD:
    case SHD_SPACE_BLENDER_WORLD:
      /* Nothing to do. */
      break;
  }

  /* Final step - mix and apply strength for all other than tangent space. */
  GPU_link(mat, "node_normal_map_mix", strength, newnormal, &out[0].link);

  return true;
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeShaderNormalMap *normal_map_node = static_cast<NodeShaderNormalMap *>(node_->storage);
  NodeItem color = get_input_value("Color", NodeItem::Type::Vector3);
  NodeItem strength = get_input_value("Strength", NodeItem::Type::Float);

#  if MATERIALX_MAJOR_VERSION <= 1 && MATERIALX_MINOR_VERSION <= 38
  std::string space;
  switch (normal_map_node->space) {
    case SHD_SPACE_TANGENT:
      space = "tangent";
      break;
    case SHD_SPACE_OBJECT:
    case SHD_SPACE_BLENDER_OBJECT:
      space = "object";
      break;
    case SHD_SPACE_WORLD:
    case SHD_SPACE_BLENDER_WORLD:
      /* World isn't supported, tangent space will be used */
      space = "tangent";
      break;
    default:
      BLI_assert_unreachable();
  }

  return create_node("normalmap",
                     NodeItem::Type::Vector3,
                     {{"in", color}, {"scale", strength}, {"space", val(space)}});
#  else
  if (normal_map_node->space == SHD_SPACE_TANGENT) {
    return create_node("normalmap", NodeItem::Type::Vector3, {{"in", color}, {"scale", strength}});
  }

  /* Object space not supported yet. Despite the 1.38 implementation accepting
   * object space argument, that seems to work either. */
  NodeItem tangent = val(MaterialX::Vector3(1.0f, 0.0f, 0.0f));
  NodeItem bitangent = val(MaterialX::Vector3(0.0f, 1.0f, 0.0f));
  NodeItem normal = val(MaterialX::Vector3(0.0f, 0.0f, 1.0f));

  return create_node("normalmap",
                     NodeItem::Type::Vector3,
                     {{"in", color},
                      {"scale", strength},
                      {"tangent", tangent},
                      {"bitangent", bitangent},
                      {"normal", normal}});
#  endif
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_normal_map_cc

/* node type definition */
void register_node_type_sh_normal_map()
{
  namespace file_ns = blender::nodes::node_shader_normal_map_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeNormalMap", SH_NODE_NORMAL_MAP);
  ntype.ui_name = "Normal Map";
  ntype.ui_description =
      "Generate a perturbed normal from an RGB normal map image. Typically used for faking highly "
      "detailed surfaces";
  ntype.enum_name_legacy = "NORMAL_MAP";
  ntype.nclass = NODE_CLASS_OP_VECTOR;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_normal_map;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Middle);
  ntype.initfunc = file_ns::node_shader_init_normal_map;
  blender::bke::node_type_storage(
      ntype, "NodeShaderNormalMap", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::gpu_shader_normal_map;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
