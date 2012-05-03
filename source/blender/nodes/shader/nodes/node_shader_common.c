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

#include "BKE_node.h"

#include "node_shader_util.h"
#include "node_common.h"
#include "node_exec.h"

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
		
		zero_v4(from->vec);
		from->data = NULL;
		from->datatype = 0;
		from->is_copy = 0;
	}
}

/**** GROUP ****/

static void *group_initexec(bNode *node)
{
	bNodeTree *ngroup= (bNodeTree*)node->id;
	bNodeTreeExec *exec;
	
	if (!ngroup)
		return NULL;
	
	/* initialize the internal node tree execution */
	exec = ntreeShaderBeginExecTree(ngroup, 0);
	
	return exec;
}

static void group_freeexec(bNode *UNUSED(node), void *nodedata)
{
	bNodeTreeExec*gexec= (bNodeTreeExec*)nodedata;
	
	ntreeShaderEndExecTree(gexec, 0);
}

/* Copy inputs to the internal stack.
 */
static void group_copy_inputs(bNode *node, bNodeStack **in, bNodeStack *gstack)
{
	bNodeSocket *sock;
	bNodeStack *ns;
	int a;
	for (sock=node->inputs.first, a=0; sock; sock=sock->next, ++a) {
		if (sock->groupsock) {
			ns = node_get_socket_stack(gstack, sock->groupsock);
			copy_stack(ns, in[a]);
		}
	}
}

/* Copy internal results to the external outputs.
 */
static void group_move_outputs(bNode *node, bNodeStack **out, bNodeStack *gstack)
{
	bNodeSocket *sock;
	bNodeStack *ns;
	int a;
	for (sock=node->outputs.first, a=0; sock; sock=sock->next, ++a) {
		if (sock->groupsock) {
			ns = node_get_socket_stack(gstack, sock->groupsock);
			move_stack(out[a], ns);
		}
	}
}

static void group_execute(void *data, int thread, struct bNode *node, void *nodedata, struct bNodeStack **in, struct bNodeStack **out)
{
	bNodeTreeExec *exec= (bNodeTreeExec*)nodedata;
	bNodeThreadStack *nts;
	
	/* XXX same behavior as trunk: all nodes inside group are executed.
	 * it's stupid, but just makes it work. compo redesign will do this better.
	 */
	{
		bNode *inode;
		for (inode=exec->nodetree->nodes.first; inode; inode=inode->next)
			inode->need_exec = 1;
	}
	
	nts = ntreeGetThreadStack(exec, thread);
	
	group_copy_inputs(node, in, nts->stack);
	ntreeExecThreadNodes(exec, nts, data, thread);
	group_move_outputs(node, out, nts->stack);
	
	ntreeReleaseThreadStack(nts);
}

static void group_gpu_copy_inputs(bNode *node, GPUNodeStack *in, bNodeStack *gstack)
{
	bNodeSocket *sock;
	bNodeStack *ns;
	int a;
	for (sock=node->inputs.first, a=0; sock; sock=sock->next, ++a) {
		if (sock->groupsock) {
			ns = node_get_socket_stack(gstack, sock->groupsock);
			/* convert the external gpu stack back to internal node stack data */
			node_data_from_gpu_stack(ns, &in[a]);
		}
	}
}

/* Copy internal results to the external outputs.
 */
static void group_gpu_move_outputs(bNode *node, GPUNodeStack *out, bNodeStack *gstack)
{
	bNodeSocket *sock;
	bNodeStack *ns;
	int a;
	for (sock=node->outputs.first, a=0; sock; sock=sock->next, ++a) {
		if (sock->groupsock) {
			ns = node_get_socket_stack(gstack, sock->groupsock);
			/* convert the node stack data result back to gpu stack */
			node_gpu_stack_from_data(&out[a], sock->type, ns);
		}
	}
}

static int gpu_group_execute(GPUMaterial *mat, bNode *node, void *nodedata, GPUNodeStack *in, GPUNodeStack *out)
{
	bNodeTreeExec *exec= (bNodeTreeExec*)nodedata;
	
	group_gpu_copy_inputs(node, in, exec->stack);
	ntreeExecGPUNodes(exec, mat, (node->flag & NODE_GROUP_EDIT));
	group_gpu_move_outputs(node, out, exec->stack);
	
	return 1;
}

void register_node_type_sh_group(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, NODE_GROUP, "Group", NODE_CLASS_GROUP, NODE_OPTIONS|NODE_CONST_OUTPUT);
	node_type_socket_templates(&ntype, NULL, NULL);
	node_type_size(&ntype, 120, 60, 200);
	node_type_label(&ntype, node_group_label);
	node_type_init(&ntype, node_group_init);
	node_type_valid(&ntype, node_group_valid);
	node_type_template(&ntype, node_group_template);
	node_type_update(&ntype, NULL, node_group_verify);
	node_type_group_edit(&ntype, node_group_edit_get, node_group_edit_set, node_group_edit_clear);
	node_type_exec_new(&ntype, group_initexec, group_freeexec, group_execute);
	node_type_gpu_ext(&ntype, gpu_group_execute);
	
	nodeRegisterType(ttype, &ntype);
}


/**** FOR LOOP ****/

#if 0 /* XXX loop nodes don't work nicely with current trees */
static void forloop_execute(void *data, int thread, struct bNode *node, void *nodedata, struct bNodeStack **in, struct bNodeStack **out)
{
	bNodeTreeExec *exec= (bNodeTreeExec*)nodedata;
	bNodeThreadStack *nts;
	int iterations= (int)in[0]->vec[0];
	bNodeSocket *sock;
	bNodeStack *ns;
	int iteration;
	
	/* XXX same behavior as trunk: all nodes inside group are executed.
	 * it's stupid, but just makes it work. compo redesign will do this better.
	 */
	{
		bNode *inode;
		for (inode=exec->nodetree->nodes.first; inode; inode=inode->next)
			inode->need_exec = 1;
	}
	
	nts = ntreeGetThreadStack(exec, thread);
	
	/* "Iteration" socket */
	sock = exec->nodetree->inputs.first;
	ns = node_get_socket_stack(nts->stack, sock);
	
//	group_copy_inputs(node, in, nts->stack);
	for (iteration=0; iteration < iterations; ++iteration) {
		/* first input contains current iteration counter */
		ns->vec[0] = (float)iteration;
		ns->vec[1]=ns->vec[2]=ns->vec[3] = 0.0f;
		
//		if (iteration > 0)
//			loop_init_iteration(exec->nodetree, nts->stack);
//		ntreeExecThreadNodes(exec, nts, data, thread);
	}
//	loop_copy_outputs(node, in, out, exec->stack);
	
	ntreeReleaseThreadStack(nts);
}

void register_node_type_sh_forloop(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, NODE_FORLOOP, "For", NODE_CLASS_GROUP, NODE_OPTIONS);
	node_type_socket_templates(&ntype, NULL, NULL);
	node_type_size(&ntype, 120, 60, 200);
	node_type_label(&ntype, node_group_label);
	node_type_init(&ntype, node_forloop_init);
	node_type_valid(&ntype, node_group_valid);
	node_type_template(&ntype, node_forloop_template);
	node_type_update(&ntype, NULL, node_group_verify);
	node_type_tree(&ntype, node_forloop_init_tree, node_loop_update_tree);
	node_type_group_edit(&ntype, node_group_edit_get, node_group_edit_set, node_group_edit_clear);
	node_type_exec_new(&ntype, group_initexec, group_freeexec, forloop_execute);
	
	nodeRegisterType(ttype, &ntype);
}
#endif

/**** WHILE LOOP ****/

#if 0 /* XXX loop nodes don't work nicely with current trees */
static void whileloop_execute(void *data, int thread, struct bNode *node, void *nodedata, struct bNodeStack **in, struct bNodeStack **out)
{
	bNodeTreeExec *exec= (bNodeTreeExec*)nodedata;
	bNodeThreadStack *nts;
	int condition= (in[0]->vec[0] > 0.0f);
	bNodeSocket *sock;
	bNodeStack *ns;
	int iteration;
	
	/* XXX same behavior as trunk: all nodes inside group are executed.
	 * it's stupid, but just makes it work. compo redesign will do this better.
	 */
	{
		bNode *inode;
		for (inode=exec->nodetree->nodes.first; inode; inode=inode->next)
			inode->need_exec = 1;
	}
	
	nts = ntreeGetThreadStack(exec, thread);
	
	/* "Condition" socket */
	sock = exec->nodetree->outputs.first;
	ns = node_get_socket_stack(nts->stack, sock);
	
	iteration = 0;
//	group_copy_inputs(node, in, nts->stack);
	while (condition && iteration < node->custom1) {
//		if (iteration > 0)
//			loop_init_iteration(exec->nodetree, nts->stack);
//		ntreeExecThreadNodes(exec, nts, data, thread);
		
		condition = (ns->vec[0] > 0.0f);
		++iteration;
	}
//	loop_copy_outputs(node, in, out, exec->stack);
	
	ntreeReleaseThreadStack(nts);
}

void register_node_type_sh_whileloop(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, NODE_WHILELOOP, "While", NODE_CLASS_GROUP, NODE_OPTIONS);
	node_type_socket_templates(&ntype, NULL, NULL);
	node_type_size(&ntype, 120, 60, 200);
	node_type_label(&ntype, node_group_label);
	node_type_init(&ntype, node_whileloop_init);
	node_type_valid(&ntype, node_group_valid);
	node_type_template(&ntype, node_whileloop_template);
	node_type_update(&ntype, NULL, node_group_verify);
	node_type_tree(&ntype, node_whileloop_init_tree, node_loop_update_tree);
	node_type_group_edit(&ntype, node_group_edit_get, node_group_edit_set, node_group_edit_clear);
	node_type_exec_new(&ntype, group_initexec, group_freeexec, whileloop_execute);
	
	nodeRegisterType(ttype, &ntype);
}
#endif
