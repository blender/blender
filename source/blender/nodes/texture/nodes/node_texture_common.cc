/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup texnodes
 */

#include "DNA_node_types.h"

#include "BLI_utildefines.h"

#include "BKE_node.hh"

#include "NOD_common.h"
#include "node_common.h"
#include "node_exec.hh"
#include "node_texture_util.hh"

#include "RNA_access.hh"

static void copy_stack(bNodeStack *to, bNodeStack *from)
{
  if (to != from) {
    copy_v4_v4(to->vec, from->vec);
    to->data = from->data;
    to->datatype = from->datatype;

    /* tag as copy to prevent freeing */
    to->is_copy = 1;
  }
}

/**** GROUP ****/

static void *group_initexec(bNodeExecContext *context, bNode *node, bNodeInstanceKey key)
{
  bNodeTree *ngroup = (bNodeTree *)node->id;
  void *exec;

  if (!ngroup) {
    return nullptr;
  }

  /* initialize the internal node tree execution */
  exec = ntreeTexBeginExecTree_internal(context, ngroup, key);

  return exec;
}

static void group_freeexec(void *nodedata)
{
  bNodeTreeExec *gexec = (bNodeTreeExec *)nodedata;

  ntreeTexEndExecTree_internal(gexec);
}

/* Copy inputs to the internal stack.
 * This is a shallow copy, no buffers are duplicated here!
 */
static void group_copy_inputs(bNode *gnode, bNodeStack **in, bNodeStack *gstack)
{
  bNodeTree *ngroup = (bNodeTree *)gnode->id;
  bNodeSocket *sock;
  bNodeStack *ns;
  int a;

  LISTBASE_FOREACH (bNode *, node, &ngroup->nodes) {
    if (node->type == NODE_GROUP_INPUT) {
      for (sock = static_cast<bNodeSocket *>(node->outputs.first), a = 0; sock;
           sock = sock->next, a++) {
        if (in[a]) { /* shouldn't need to check this #36694. */
          ns = node_get_socket_stack(gstack, sock);
          if (ns) {
            copy_stack(ns, in[a]);
          }
        }
      }
    }
  }
}

/* Copy internal results to the external outputs.
 */
static void group_copy_outputs(bNode *gnode, bNodeStack **out, bNodeStack *gstack)
{
  const bNodeTree &ngroup = *reinterpret_cast<bNodeTree *>(gnode->id);

  ngroup.ensure_topology_cache();
  const bNode *group_output_node = ngroup.group_output_node();
  if (!group_output_node) {
    return;
  }

  int a;
  LISTBASE_FOREACH_INDEX (bNodeSocket *, sock, &group_output_node->inputs, a) {
    if (!out[a]) {
      /* shouldn't need to check this #36694. */
      continue;
    }

    bNodeStack *ns = node_get_socket_stack(gstack, sock);
    if (ns) {
      copy_stack(out[a], ns);
    }
  }
}

static void group_execute(void *data,
                          int thread,
                          bNode *node,
                          bNodeExecData *execdata,
                          bNodeStack **in,
                          bNodeStack **out)
{
  bNodeTreeExec *exec = static_cast<bNodeTreeExec *>(execdata->data);
  bNodeThreadStack *nts;

  if (!exec) {
    return;
  }

  /* XXX same behavior as trunk: all nodes inside group are executed.
   * it's stupid, but just makes it work. compo redesign will do this better.
   */
  LISTBASE_FOREACH (bNode *, inode, &exec->nodetree->nodes) {
    inode->runtime->need_exec = 1;
  }

  nts = ntreeGetThreadStack(exec, thread);

  group_copy_inputs(node, in, nts->stack);
  ntreeExecThreadNodes(exec, nts, data, thread);
  group_copy_outputs(node, out, nts->stack);

  ntreeReleaseThreadStack(nts);
}

void register_node_type_tex_group()
{
  static bNodeType ntype;

  /* NOTE: Cannot use #sh_node_type_base for node group, because it would map the node type
   * to the shared #NODE_GROUP integer type id. */

  node_type_base_custom(&ntype, "TextureNodeGroup", "Group", "GROUP", NODE_CLASS_GROUP);
  ntype.type = NODE_GROUP;
  ntype.poll = tex_node_poll_default;
  ntype.poll_instance = node_group_poll_instance;
  ntype.insert_link = node_insert_link_default;
  ntype.rna_ext.srna = RNA_struct_find("TextureNodeGroup");
  BLI_assert(ntype.rna_ext.srna != nullptr);
  RNA_struct_blender_type_set(ntype.rna_ext.srna, &ntype);

  blender::bke::node_type_size(&ntype, 140, 60, 400);
  ntype.labelfunc = node_group_label;
  ntype.declare_dynamic = blender::nodes::node_group_declare_dynamic;
  ntype.init_exec_fn = group_initexec;
  ntype.free_exec_fn = group_freeexec;
  ntype.exec_fn = group_execute;

  nodeRegisterType(&ntype);
}
