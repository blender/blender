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

static bNodeSocketTemplate sh_node_ambient_occlusion_in[] = {
	{	SOCK_RGBA, 1, N_("Color"),		1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 0, N_("Distance"),	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_VECTOR, 1, N_("Normal"),	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_ambient_occlusion_out[] = {
	{	SOCK_RGBA, 1, N_("Color"),	1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 0, N_("AO"),	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static int node_shader_gpu_ambient_occlusion(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	if (!in[2].link)
		GPU_link(mat, "world_normals_get", &in[2].link);

	return GPU_stack_link(mat, node, "node_ambient_occlusion", in, out);
}

static void node_shader_init_ambient_occlusion(bNodeTree *UNUSED(ntree), bNode *node)
{
	node->custom1 = 16; /* samples */
	node->custom2 = 0;
}

/* node type definition */
void register_node_type_sh_ambient_occlusion(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_AMBIENT_OCCLUSION, "Ambient Occlusion", NODE_CLASS_INPUT, 0);
	node_type_socket_templates(&ntype, sh_node_ambient_occlusion_in, sh_node_ambient_occlusion_out);
	node_type_init(&ntype, node_shader_init_ambient_occlusion);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_gpu(&ntype, node_shader_gpu_ambient_occlusion);

	nodeRegisterType(&ntype);
}
