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
static bNodeSocketTemplate sh_node_normal_in[] = {
	{	SOCK_VECTOR, 1, N_("Normal"),	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_DIRECTION},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_normal_out[] = {
	{	SOCK_VECTOR, 0, N_("Normal"), 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 1.0f, PROP_DIRECTION},
	{	SOCK_FLOAT, 0, N_("Dot")},
	{	-1, 0, ""	}
};

/* generates normal, does dot product */
static void node_shader_exec_normal(void *UNUSED(data), int UNUSED(thread), bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), bNodeStack **in, bNodeStack **out)
{
	float vec[3];
	
	/* stack order input:  normal */
	/* stack order output: normal, value */
	
	nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	
	/* render normals point inside... the widget points outside */
	out[1]->vec[0] = -dot_v3v3(vec, out[0]->vec);
}

static int gpu_shader_normal(GPUMaterial *mat, bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	GPUNodeLink *vec = GPU_uniform(out[0].vec);
	return GPU_stack_link(mat, "normal", in, out, vec);
}

void register_node_type_sh_normal(void)
{
	static bNodeType ntype;
	
	sh_node_type_base(&ntype, SH_NODE_NORMAL, "Normal", NODE_CLASS_OP_VECTOR, 0);
	node_type_compatibility(&ntype, NODE_OLD_SHADING | NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_normal_in, sh_node_normal_out);
	node_type_exec(&ntype, NULL, NULL, node_shader_exec_normal);
	node_type_gpu(&ntype, gpu_shader_normal);
	
	nodeRegisterType(&ntype);
}
