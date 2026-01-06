/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "NOD_common.hh"
#include "NOD_shader.h"
#include "node_common.h"
#include "node_exec.hh"
#include "node_shader_util.hh"

#include "RNA_access.hh"

namespace blender {

/**** GROUP ****/

static void group_gpu_copy_inputs(bNode *gnode, GPUNodeStack *in, bNodeStack *gstack)
{
  bNodeTree *ngroup = id_cast<bNodeTree *>(gnode->id);

  for (bNode *node : ngroup->all_nodes()) {
    if (node->is_group_input()) {

      for (const auto [a, sock] : node->outputs.enumerate()) {
        bNodeStack *ns = node_get_socket_stack(gstack, &sock);
        if (ns) {
          /* convert the external gpu stack back to internal node stack data */
          node_data_from_gpu_stack(ns, &in[a]);
        }
      }
    }
  }
}

/* Copy internal results to the external outputs.
 */
static void group_gpu_move_outputs(bNode *gnode, GPUNodeStack *out, bNodeStack *gstack)
{
  bNodeTree &ngroup = *reinterpret_cast<bNodeTree *>(gnode->id);

  ngroup.ensure_topology_cache();
  bNode *group_output_node = ngroup.group_output_node();
  if (!group_output_node) {
    return;
  }

  for (const auto [a, sock] : group_output_node->inputs.enumerate()) {
    bNodeStack *ns = node_get_socket_stack(gstack, &sock);
    if (ns) {
      /* convert the node stack data result back to gpu stack */
      node_gpu_stack_from_data(&out[a], &sock, ns);
    }
  }
}

static int gpu_group_execute(
    GPUMaterial *mat, bNode *node, bNodeExecData *execdata, GPUNodeStack *in, GPUNodeStack *out)
{
  bNodeTreeExec *exec = static_cast<bNodeTreeExec *>(execdata->data);

  if (!node->id) {
    return 0;
  }

  group_gpu_copy_inputs(node, in, exec->stack);
  ntreeExecGPUNodes(exec, mat, nullptr);
  group_gpu_move_outputs(node, out, exec->stack);

  return 1;
}

void register_node_type_sh_group()
{
  static bke::bNodeType ntype;

  /* NOTE: cannot use #sh_node_type_base for node group, because it would map the node type
   * to the shared #NODE_GROUP integer type id. */

  bke::node_type_base_custom(ntype, "ShaderNodeGroup", "Group", "GROUP", NODE_CLASS_GROUP);
  ntype.enum_name_legacy = "GROUP";
  ntype.type_legacy = NODE_GROUP;
  ntype.poll = sh_node_poll_default;
  ntype.poll_instance = node_group_poll_instance;
  ntype.insert_link = node_insert_link_default;
  ntype.ui_class = node_group_ui_class;
  ntype.ui_description_fn = node_group_ui_description;
  ntype.rna_ext.srna = RNA_struct_find("ShaderNodeGroup");
  BLI_assert(ntype.rna_ext.srna != nullptr);
  RNA_struct_blender_type_set(ntype.rna_ext.srna, &ntype);

  bke::node_type_size(ntype, GROUP_NODE_DEFAULT_WIDTH, GROUP_NODE_MIN_WIDTH, GROUP_NODE_MAX_WIDTH);
  ntype.labelfunc = node_group_label;
  ntype.declare = nodes::node_group_declare;
  ntype.gpu_fn = gpu_group_execute;

  bke::node_register_type(ntype);
}

void register_node_type_sh_custom_group(bke::bNodeType *ntype)
{
  /* These methods can be overridden but need a default implementation otherwise. */
  if (ntype->poll == nullptr) {
    ntype->poll = sh_node_poll_default;
  }
  if (ntype->insert_link == nullptr) {
    ntype->insert_link = node_insert_link_default;
  }
  ntype->declare = nodes::node_group_declare;
  ntype->gpu_fn = gpu_group_execute;
}

}  // namespace blender
