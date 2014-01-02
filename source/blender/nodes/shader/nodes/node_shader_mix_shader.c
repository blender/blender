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

#include "../node_shader_util.h"

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_mix_shader_in[] = {
	{	SOCK_FLOAT,  1, N_("Fac"),		0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_SHADER, 1, N_("Shader")},
	{	SOCK_SHADER, 1, N_("Shader")},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_mix_shader_out[] = {
	{	SOCK_SHADER, 0, N_("Shader")},
	{	-1, 0, ""	}
};

static int node_shader_gpu_mix_shader(GPUMaterial *mat, bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, "node_mix_shader", in, out);
}

/* node type definition */
void register_node_type_sh_mix_shader(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_MIX_SHADER, "Mix Shader", NODE_CLASS_SHADER, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_mix_shader_in, sh_node_mix_shader_out);
	node_type_init(&ntype, NULL);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_gpu(&ntype, node_shader_gpu_mix_shader);

	nodeRegisterType(&ntype);
}
