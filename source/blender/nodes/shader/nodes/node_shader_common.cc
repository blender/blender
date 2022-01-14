/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 * Juho Vepsäläinen
 */

/** \file
 * \ingroup shdnodes
 */

#include "DNA_node_types.h"

#include "BLI_utildefines.h"

#include "BKE_node.h"

#include "NOD_common.h"
#include "node_common.h"
#include "node_exec.h"
#include "node_shader_util.hh"

#include "RNA_access.h"

/**** GROUP ****/

static void group_gpu_copy_inputs(bNode *gnode, GPUNodeStack *in, bNodeStack *gstack)
{
  bNodeTree *ngroup = (bNodeTree *)gnode->id;

  LISTBASE_FOREACH (bNode *, node, &ngroup->nodes) {
    if (node->type == NODE_GROUP_INPUT) {
      int a;
      LISTBASE_FOREACH_INDEX (bNodeSocket *, sock, &node->outputs, a) {
        bNodeStack *ns = node_get_socket_stack(gstack, sock);
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
  bNodeTree *ngroup = (bNodeTree *)gnode->id;

  LISTBASE_FOREACH (bNode *, node, &ngroup->nodes) {
    if (node->type == NODE_GROUP_OUTPUT && (node->flag & NODE_DO_OUTPUT)) {
      int a;
      LISTBASE_FOREACH_INDEX (bNodeSocket *, sock, &node->inputs, a) {
        bNodeStack *ns = node_get_socket_stack(gstack, sock);
        if (ns) {
          /* convert the node stack data result back to gpu stack */
          node_gpu_stack_from_data(&out[a], sock->type, ns);
        }
      }
      break; /* only one active output node */
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
  static bNodeType ntype;

  /* NOTE: cannot use #sh_node_type_base for node group, because it would map the node type
   * to the shared #NODE_GROUP integer type id. */

  node_type_base_custom(&ntype, "ShaderNodeGroup", "Group", NODE_CLASS_GROUP);
  ntype.type = NODE_GROUP;
  ntype.poll = sh_node_poll_default;
  ntype.poll_instance = node_group_poll_instance;
  ntype.insert_link = node_insert_link_default;
  ntype.rna_ext.srna = RNA_struct_find("ShaderNodeGroup");
  BLI_assert(ntype.rna_ext.srna != nullptr);
  RNA_struct_blender_type_set(ntype.rna_ext.srna, &ntype);

  node_type_size(&ntype, 140, 60, 400);
  ntype.labelfunc = node_group_label;
  node_type_group_update(&ntype, node_group_update);
  node_type_gpu(&ntype, gpu_group_execute);

  nodeRegisterType(&ntype);
}

void register_node_type_sh_custom_group(bNodeType *ntype)
{
  /* These methods can be overridden but need a default implementation otherwise. */
  if (ntype->poll == nullptr) {
    ntype->poll = sh_node_poll_default;
  }
  if (ntype->insert_link == nullptr) {
    ntype->insert_link = node_insert_link_default;
  }

  node_type_gpu(ntype, gpu_group_execute);
}
