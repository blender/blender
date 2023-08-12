/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "BKE_context.h"
#include "BKE_node_runtime.hh"

#include "DEG_depsgraph_query.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_normal_map_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Strength").default_value(1.0f).min(0.0f).max(10.0f);
  b.add_input<decl::Color>("Color").default_value({0.5f, 0.5f, 1.0f, 1.0f});
  b.add_output<decl::Vector>("Normal");
}

static void node_shader_buts_normal_map(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "space", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  if (RNA_enum_get(ptr, "space") == SHD_SPACE_TANGENT) {
    PointerRNA obptr = CTX_data_pointer_get(C, "active_object");

    if (obptr.data && RNA_enum_get(&obptr, "type") == OB_MESH) {
      PointerRNA eval_obptr;

      Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
      DEG_get_evaluated_rna_pointer(depsgraph, &obptr, &eval_obptr);
      PointerRNA dataptr = RNA_pointer_get(&eval_obptr, "data");
      uiItemPointerR(layout, ptr, "uv_map", &dataptr, "uv_layers", "", ICON_NONE);
    }
    else {
      uiItemR(layout, ptr, "uv_map", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
    }
  }
}

static void node_shader_init_normal_map(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderNormalMap *attr = MEM_cnew<NodeShaderNormalMap>("NodeShaderNormalMap");
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

}  // namespace blender::nodes::node_shader_normal_map_cc

/* node type definition */
void register_node_type_sh_normal_map()
{
  namespace file_ns = blender::nodes::node_shader_normal_map_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_NORMAL_MAP, "Normal Map", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_normal_map;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::MIDDLE);
  ntype.initfunc = file_ns::node_shader_init_normal_map;
  node_type_storage(
      &ntype, "NodeShaderNormalMap", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::gpu_shader_normal_map;

  nodeRegisterType(&ntype);
}
