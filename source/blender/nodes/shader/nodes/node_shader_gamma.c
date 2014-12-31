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
 * Contributor(s): none yet.
 * 
 * ***** END GPL LICENSE BLOCK *****

*/

#include "node_shader_util.h"

/* **************** Gamma Tools  ******************** */

static bNodeSocketTemplate sh_node_gamma_in[] = {
	{	SOCK_RGBA, 1, N_("Color"),			1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_FLOAT, 1, N_("Gamma"),			1.0f, 0.0f, 0.0f, 0.0f, 0.001f, 10.0f, PROP_UNSIGNED},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_gamma_out[] = {
	{	SOCK_RGBA, 0, N_("Color")},
	{	-1, 0, ""	}
};

static void node_shader_exec_gamma(void *UNUSED(data), int UNUSED(thread), bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), bNodeStack **in, bNodeStack **out)
{
	float col[3];
	float gamma;
	nodestack_get_vec(col, SOCK_VECTOR, in[0]);
	nodestack_get_vec(&gamma, SOCK_FLOAT, in[1]);

	out[0]->vec[0] = col[0] > 0.0 ? pow(col[0], gamma) : col[0];
	out[0]->vec[1] = col[1] > 0.0 ? pow(col[1], gamma) : col[1];
	out[0]->vec[2] = col[2] > 0.0 ? pow(col[2], gamma) : col[2];
}

static int node_shader_gpu_gamma(GPUMaterial *mat, bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, "node_gamma", in, out);
}

void register_node_type_sh_gamma(void)
{
	static bNodeType ntype;
	
	sh_node_type_base(&ntype, SH_NODE_GAMMA, "Gamma", NODE_CLASS_OP_COLOR, 0);
	node_type_compatibility(&ntype, NODE_OLD_SHADING | NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_gamma_in, sh_node_gamma_out);
	node_type_init(&ntype, NULL);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_exec(&ntype, NULL, NULL, node_shader_exec_gamma);
	node_type_gpu(&ntype, node_shader_gpu_gamma);
	
	nodeRegisterType(&ntype);
}
