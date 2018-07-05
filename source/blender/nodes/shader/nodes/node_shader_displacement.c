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

static bNodeSocketTemplate sh_node_displacement_in[] = {
	{	SOCK_FLOAT, 0, N_("Height"), 0.00f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_FLOAT, 0, N_("Midlevel"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_FLOAT, 0, N_("Scale"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_VECTOR, 1, N_("Normal"), 0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_displacement_out[] = {
	{	SOCK_VECTOR, 0, N_("Displacement"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_init_displacement(bNodeTree *UNUSED(ntree), bNode *node)
{
	node->custom1 = SHD_SPACE_OBJECT; /* space */

	/* Set default value here for backwards compatibility. */
	for (bNodeSocket *sock = node->inputs.first; sock; sock = sock->next) {
		if (STREQ(sock->name, "Midlevel")) {
			((bNodeSocketValueFloat *)sock->default_value)->value = 0.5f;
		}
	}
}

static int gpu_shader_displacement(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	if (!in[3].link) {
		GPU_link(mat, "direction_transform_m4v3", GPU_builtin(GPU_VIEW_NORMAL), GPU_builtin(GPU_INVERSE_VIEW_MATRIX), &in[3].link);
	}

	if (node->custom1 == SHD_SPACE_OBJECT) {
		return GPU_stack_link(mat, node, "node_displacement_object", in, out, GPU_builtin(GPU_OBJECT_MATRIX));
	}
	else {
		return GPU_stack_link(mat, node, "node_displacement_world", in, out, GPU_builtin(GPU_OBJECT_MATRIX));
	}
}

/* node type definition */
void register_node_type_sh_displacement(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_DISPLACEMENT, "Displacement", NODE_CLASS_OP_VECTOR, 0);
	node_type_socket_templates(&ntype, sh_node_displacement_in, sh_node_displacement_out);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_init(&ntype, node_shader_init_displacement);
	node_type_gpu(&ntype, gpu_shader_displacement);

	nodeRegisterType(&ntype);
}
