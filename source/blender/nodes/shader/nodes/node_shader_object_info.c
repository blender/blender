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

static bNodeSocketTemplate sh_node_object_info_out[] = {
	{	SOCK_VECTOR, 0, N_("Location"),       0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT,  0, N_("Object Index"),   0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT,  0, N_("Material Index"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT,  0, N_("Random"),         0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static int node_shader_gpu_object_info(GPUMaterial *mat, bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, "node_object_info", in, out, GPU_builtin(GPU_OBJECT_MATRIX), GPU_builtin(GPU_OBJECT_INFO));
}

static void node_shader_exec_object_info(void *data, int UNUSED(thread), bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), bNodeStack **UNUSED(in), bNodeStack **out)
{
	ShaderCallData *scd = (ShaderCallData *)data;
	copy_v4_v4(out[0]->vec, RE_object_instance_get_matrix(scd->shi->obi, RE_OBJECT_INSTANCE_MATRIX_OB)[3]);
	out[1]->vec[0] = RE_object_instance_get_object_pass_index(scd->shi->obi);
	out[2]->vec[0] = scd->shi->mat->index;
	out[3]->vec[0] = RE_object_instance_get_random_id(scd->shi->obi) * (1.0f / (float)0xFFFFFFFF);
}

/* node type definition */
void register_node_type_sh_object_info(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_OBJECT_INFO, "Object Info", NODE_CLASS_INPUT, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, NULL, sh_node_object_info_out);
	node_type_init(&ntype, NULL);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_gpu(&ntype, node_shader_gpu_object_info);
	node_type_exec(&ntype, NULL, NULL, node_shader_exec_object_info);

	nodeRegisterType(&ntype);
}

