/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "BKE_context.h"

#include "DEG_depsgraph_query.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_tangent_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("Tangent");
}

static void node_shader_buts_tangent(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiLayout *split, *row;

  split = uiLayoutSplit(layout, 0.0f, false);

  uiItemR(split, ptr, "direction_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  row = uiLayoutRow(split, false);

  if (RNA_enum_get(ptr, "direction_type") == SHD_TANGENT_UVMAP) {
    PointerRNA obptr = CTX_data_pointer_get(C, "active_object");

    if (obptr.data && RNA_enum_get(&obptr, "type") == OB_MESH) {
      PointerRNA eval_obptr;

      Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
      DEG_get_evaluated_rna_pointer(depsgraph, &obptr, &eval_obptr);
      PointerRNA dataptr = RNA_pointer_get(&eval_obptr, "data");
      uiItemPointerR(row, ptr, "uv_map", &dataptr, "uv_layers", "", ICON_NONE);
    }
    else {
      uiItemR(row, ptr, "uv_map", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
    }
  }
  else {
    uiItemR(row, ptr, "axis", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  }
}

static void node_shader_init_tangent(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderTangent *attr = MEM_cnew<NodeShaderTangent>("NodeShaderTangent");
  attr->axis = SHD_TANGENT_AXIS_Z;
  node->storage = attr;
}

static int node_shader_gpu_tangent(GPUMaterial *mat,
                                   bNode *node,
                                   bNodeExecData * /*execdata*/,
                                   GPUNodeStack *in,
                                   GPUNodeStack *out)
{
  NodeShaderTangent *attr = static_cast<NodeShaderTangent *>(node->storage);

  if (attr->direction_type == SHD_TANGENT_UVMAP) {
    return GPU_stack_link(
        mat, node, "node_tangentmap", in, out, GPU_attribute(mat, CD_TANGENT, attr->uv_map));
  }

  GPUNodeLink *orco = GPU_attribute(mat, CD_ORCO, "");

  if (attr->axis == SHD_TANGENT_AXIS_X) {
    GPU_link(mat, "tangent_orco_x", orco, &orco);
  }
  else if (attr->axis == SHD_TANGENT_AXIS_Y) {
    GPU_link(mat, "tangent_orco_y", orco, &orco);
  }
  else {
    GPU_link(mat, "tangent_orco_z", orco, &orco);
  }

  return GPU_stack_link(mat, node, "node_tangent", in, out, orco);
}

}  // namespace blender::nodes::node_shader_tangent_cc

/* node type definition */
void register_node_type_sh_tangent()
{
  namespace file_ns = blender::nodes::node_shader_tangent_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TANGENT, "Tangent", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tangent;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::MIDDLE);
  ntype.initfunc = file_ns::node_shader_init_tangent;
  ntype.gpu_fn = file_ns::node_shader_gpu_tangent;
  node_type_storage(
      &ntype, "NodeShaderTangent", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
