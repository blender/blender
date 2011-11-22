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

/** \file blender/nodes/shader/nodes/node_shader_value.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"

/* **************** VALUE ******************** */
static bNodeSocketTemplate sh_node_value_out[]= {
	/* XXX value nodes use the output sockets for buttons, so we need explicit limits here! */
	{	SOCK_FLOAT, 0, "Value", 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
	{	-1, 0, ""	}
};

static void node_shader_init_value(bNodeTree *UNUSED(ntree), bNode *node, bNodeTemplate *UNUSED(ntemp))
{
	bNodeSocket *sock= node->outputs.first;
	bNodeSocketValueFloat *dval= (bNodeSocketValueFloat*)sock->default_value;
	/* uses the default value of the output socket, must be initialized here */
	dval->value = 0.5f;
	dval->min = -FLT_MAX;
	dval->max = FLT_MAX;
}

static void node_shader_exec_value(void *UNUSED(data), bNode *node, bNodeStack **UNUSED(in), bNodeStack **out)
{
	bNodeSocket *sock= node->outputs.first;
	float val= ((bNodeSocketValueFloat*)sock->default_value)->value;
	
	out[0]->vec[0]= val;
}

static int gpu_shader_value(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	bNodeSocket *sock= node->outputs.first;
	float *val= &((bNodeSocketValueFloat*)sock->default_value)->value;
	GPUNodeLink *vec = GPU_uniform(val);

	return GPU_stack_link(mat, "set_value", in, out, vec);
}

void register_node_type_sh_value(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, SH_NODE_VALUE, "Value", NODE_CLASS_INPUT, NODE_OPTIONS);
	node_type_compatibility(&ntype, NODE_OLD_SHADING|NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, NULL, sh_node_value_out);
	node_type_init(&ntype, node_shader_init_value);
	node_type_size(&ntype, 80, 50, 120);
	node_type_exec(&ntype, node_shader_exec_value);
	node_type_gpu(&ntype, gpu_shader_value);

	nodeRegisterType(ttype, &ntype);
}
