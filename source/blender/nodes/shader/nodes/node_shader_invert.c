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

/** \file blender/nodes/shader/nodes/node_shader_invert.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"



/* **************** INVERT ******************** */ 
static bNodeSocketTemplate sh_node_invert_in[]= { 
	{ SOCK_FLOAT, 1, "Fac", 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR}, 
	{ SOCK_RGBA, 1, "Color", 0.0f, 0.0f, 0.0f, 1.0f}, 
	{ -1, 0, "" } 
};

static bNodeSocketTemplate sh_node_invert_out[]= { 
	{ SOCK_RGBA, 0, "Color"}, 
	{ -1, 0, "" } 
};

static void node_shader_exec_invert(void *UNUSED(data), bNode *UNUSED(node), bNodeStack **in, 
bNodeStack **out) 
{
	float col[3], facm;

	col[0] = 1.0f - in[1]->vec[0];
	col[1] = 1.0f - in[1]->vec[1];
	col[2] = 1.0f - in[1]->vec[2];
	
	/* if fac, blend result against original input */
	if (in[0]->vec[0] < 1.0f) {
		facm = 1.0f - in[0]->vec[0];

		col[0] = in[0]->vec[0]*col[0] + (facm*in[1]->vec[0]);
		col[1] = in[0]->vec[0]*col[1] + (facm*in[1]->vec[1]);
		col[2] = in[0]->vec[0]*col[2] + (facm*in[1]->vec[2]);
	}
	
	copy_v3_v3(out[0]->vec, col);
}

static int gpu_shader_invert(GPUMaterial *mat, bNode *UNUSED(node), GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, "invert", in, out);
}

void register_node_type_sh_invert(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, SH_NODE_INVERT, "Invert", NODE_CLASS_OP_COLOR, NODE_OPTIONS);
	node_type_compatibility(&ntype, NODE_OLD_SHADING|NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_invert_in, sh_node_invert_out);
	node_type_size(&ntype, 90, 80, 100);
	node_type_exec(&ntype, node_shader_exec_invert);
	node_type_gpu(&ntype, gpu_shader_invert);

	nodeRegisterType(ttype, &ntype);
}
