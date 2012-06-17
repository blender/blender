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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/shader/nodes/node_shader_normal.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"

/* **************** NORMAL  ******************** */
static bNodeSocketTemplate sh_node_normal_in[]= {
	{	SOCK_VECTOR, 1, N_("Normal"),	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_normal_out[]= {
	{	SOCK_VECTOR, 0, N_("Normal")},
	{	SOCK_FLOAT, 0, N_("Dot")},
	{	-1, 0, ""	}
};

static void node_shader_init_normal(bNodeTree *UNUSED(ntree), bNode *node, bNodeTemplate *UNUSED(ntemp))
{
	bNodeSocket *sock= node->outputs.first;
	bNodeSocketValueVector *dval= (bNodeSocketValueVector*)sock->default_value;
	
	/* output value is used for normal vector */
	dval->value[0] = 0.0f;
	dval->value[1] = 0.0f;
	dval->value[2] = 1.0f;
}

/* generates normal, does dot product */
static void node_shader_exec_normal(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	bNodeSocket *sock= node->outputs.first;
	float vec[3];
	
	/* stack order input:  normal */
	/* stack order output: normal, value */
	
	nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	
	copy_v3_v3(out[0]->vec, ((bNodeSocketValueVector*)sock->default_value)->value);
	/* render normals point inside... the widget points outside */
	out[1]->vec[0]= -dot_v3v3(out[0]->vec, vec);
}

static int gpu_shader_normal(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	bNodeSocket *sock= node->outputs.first;
	GPUNodeLink *vec = GPU_uniform(((bNodeSocketValueVector*)sock->default_value)->value);

	return GPU_stack_link(mat, "normal", in, out, vec);
}

void register_node_type_sh_normal(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, SH_NODE_NORMAL, "Normal", NODE_CLASS_OP_VECTOR, NODE_OPTIONS);
	node_type_compatibility(&ntype, NODE_OLD_SHADING|NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_normal_in, sh_node_normal_out);
	node_type_init(&ntype, node_shader_init_normal);
	node_type_exec(&ntype, node_shader_exec_normal);
	node_type_gpu(&ntype, gpu_shader_normal);
	
	nodeRegisterType(ttype, &ntype);
}
