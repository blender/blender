/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Campbell Barton, Alfredo de Greef, David Millan Escriva,
 * Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/shader/nodes/node_shader_common.c
 *  \ingroup shdnodes
 */


#include "DNA_node_types.h"

#include "BLI_utildefines.h"

#include "BKE_node.h"

#include "node_shader_util.h"
#include "NOD_common.h"
#include "node_common.h"
#include "node_exec.h"

#include "RNA_access.h"

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

static void move_stack(bNodeStack *to, bNodeStack *from)
{
	if (to != from) {
		copy_v4_v4(to->vec, from->vec);
		to->data = from->data;
		to->datatype = from->datatype;
		to->is_copy = from->is_copy;
		
		from->data = NULL;
		from->is_copy = 0;
	}
}

/**** GROUP ****/

static void *group_initexec(bNodeExecContext *context, bNode *node, bNodeInstanceKey key)
{
	bNodeTree *ngroup = (bNodeTree *)node->id;
	bNodeTreeExec *exec;
	
	if (!ngroup)
		return NULL;
	
	/* initialize the internal node tree execution */
	exec = ntreeShaderBeginExecTree_internal(context, ngroup, key);
	
	return exec;
}

static void group_freeexec(void *nodedata)
{
	bNodeTreeExec *gexec = (bNodeTreeExec *)nodedata;
	
	if (gexec)
		ntreeShaderEndExecTree_internal(gexec);
}

/* Copy inputs to the internal stack.
 */
static void group_copy_inputs(bNode *gnode, bNodeStack **in, bNodeStack *gstack)
{
	bNodeTree *ngroup = (bNodeTree *)gnode->id;
	bNode *node;
	bNodeSocket *sock;
	bNodeStack *ns;
	int a;
	
	for (node = ngroup->nodes.first; node; node = node->next) {
		if (node->type == NODE_GROUP_INPUT) {
			for (sock = node->outputs.first, a = 0; sock; sock = sock->next, ++a) {
				ns = node_get_socket_stack(gstack, sock);
				if (ns)
					copy_stack(ns, in[a]);
			}
		}
	}
}

/* Copy internal results to the external outputs.
 */
static void group_move_outputs(bNode *gnode, bNodeStack **out, bNodeStack *gstack)
{
	bNodeTree *ngroup = (bNodeTree *)gnode->id;
	bNode *node;
	bNodeSocket *sock;
	bNodeStack *ns;
	int a;
	
	for (node = ngroup->nodes.first; node; node = node->next) {
		if (node->type == NODE_GROUP_OUTPUT && (node->flag & NODE_DO_OUTPUT)) {
			for (sock = node->inputs.first, a = 0; sock; sock = sock->next, ++a) {
				ns = node_get_socket_stack(gstack, sock);
				if (ns)
					move_stack(out[a], ns);
			}
			break;  /* only one active output node */
		}
	}
}

static void group_execute(void *data, int thread, struct bNode *node, bNodeExecData *execdata, struct bNodeStack **in, struct bNodeStack **out)
{
	bNodeTreeExec *exec = execdata->data;
	bNodeThreadStack *nts;
	
	if (!exec)
		return;
	
	/* XXX same behavior as trunk: all nodes inside group are executed.
	 * it's stupid, but just makes it work. compo redesign will do this better.
	 */
	{
		bNode *inode;
		for (inode = exec->nodetree->nodes.first; inode; inode = inode->next)
			inode->need_exec = 1;
	}
	
	nts = ntreeGetThreadStack(exec, thread);
	
	group_copy_inputs(node, in, nts->stack);
	ntreeExecThreadNodes(exec, nts, data, thread);
	group_move_outputs(node, out, nts->stack);
	
	ntreeReleaseThreadStack(nts);
}

static void group_gpu_copy_inputs(bNode *gnode, GPUNodeStack *in, bNodeStack *gstack)
{
	bNodeTree *ngroup = (bNodeTree *)gnode->id;
	bNode *node;
	bNodeSocket *sock;
	bNodeStack *ns;
	int a;
	
	for (node = ngroup->nodes.first; node; node = node->next) {
		if (node->type == NODE_GROUP_INPUT) {
			for (sock = node->outputs.first, a = 0; sock; sock = sock->next, ++a) {
				ns = node_get_socket_stack(gstack, sock);
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
	bNode *node;
	bNodeSocket *sock;
	bNodeStack *ns;
	int a;
	
	for (node = ngroup->nodes.first; node; node = node->next) {
		if (node->type == NODE_GROUP_OUTPUT && (node->flag & NODE_DO_OUTPUT)) {
			for (sock = node->inputs.first, a = 0; sock; sock = sock->next, ++a) {
				ns = node_get_socket_stack(gstack, sock);
				if (ns) {
					/* convert the node stack data result back to gpu stack */
					node_gpu_stack_from_data(&out[a], sock->type, ns);
				}
			}
			break;  /* only one active output node */
		}
	}
}

static int gpu_group_execute(GPUMaterial *mat, bNode *node, bNodeExecData *execdata, GPUNodeStack *in, GPUNodeStack *out)
{
	bNodeTreeExec *exec = execdata->data;
	
	if (!node->id)
		return 0;
	
	group_gpu_copy_inputs(node, in, exec->stack);
#if 0   /* XXX NODE_GROUP_EDIT is deprecated, depends on node space */
	ntreeExecGPUNodes(exec, mat, (node->flag & NODE_GROUP_EDIT));
#else
	ntreeExecGPUNodes(exec, mat, 0, NODE_NEW_SHADING | NODE_OLD_SHADING);
#endif
	group_gpu_move_outputs(node, out, exec->stack);
	
	return 1;
}

void register_node_type_sh_group(void)
{
	static bNodeType ntype;
	
	/* NB: cannot use sh_node_type_base for node group, because it would map the node type
	 * to the shared NODE_GROUP integer type id.
	 */
	node_type_base_custom(&ntype, "ShaderNodeGroup", "Group", NODE_CLASS_GROUP, NODE_CONST_OUTPUT);
	ntype.type = NODE_GROUP;
	ntype.poll = sh_node_poll_default;
	ntype.poll_instance = node_group_poll_instance;
	ntype.update_internal_links = node_update_internal_links_default;
	ntype.ext.srna = RNA_struct_find("ShaderNodeGroup");
	BLI_assert(ntype.ext.srna != NULL);
	RNA_struct_blender_type_set(ntype.ext.srna, &ntype);
	
	node_type_compatibility(&ntype, NODE_OLD_SHADING | NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, NULL, NULL);
	node_type_size(&ntype, 140, 60, 400);
	node_type_label(&ntype, node_group_label);
	node_type_update(&ntype, NULL, node_group_verify);
	node_type_exec(&ntype, group_initexec, group_freeexec, group_execute);
	node_type_gpu(&ntype, gpu_group_execute);
	
	nodeRegisterType(&ntype);
}
