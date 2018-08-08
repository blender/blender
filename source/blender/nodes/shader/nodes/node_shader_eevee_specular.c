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
 * Contributor(s): Clément Foucault.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../node_shader_util.h"

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_eevee_specular_in[] = {
	{	SOCK_RGBA, 1, N_("Base Color"),				0.8f, 0.8f, 0.8f, 1.0f},
	{	SOCK_RGBA, 1, N_("Specular"),				0.03f, 0.03f, 0.03f, 1.0f},
	{	SOCK_FLOAT, 1, N_("Roughness"),				0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_RGBA, 1, N_("Emissive Color"),			0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 1, N_("Transparency"),			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_VECTOR, 1, N_("Normal"),				0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	SOCK_FLOAT, 1, N_("Clear Coat"),			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, N_("Clear Coat Roughness"),	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_VECTOR, 1, N_("Clear Coat Normal"),	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	SOCK_FLOAT, 1, N_("Ambient Occlusion"),		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_eevee_specular_out[] = {
	{	SOCK_SHADER, 0, N_("BSDF")},
	{	-1, 0, ""	}
};

static int node_shader_gpu_eevee_specular(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	static float one = 1.0f;

	/* Normals */
	if (!in[5].link) {
		GPU_link(mat, "world_normals_get", &in[5].link);
	}

	/* Clearcoat Normals */
	if (!in[8].link) {
		GPU_link(mat, "world_normals_get", &in[8].link);
	}

	/* Occlusion */
	if (!in[9].link) {
		GPU_link(mat, "set_value", GPU_uniform(&one), &in[9].link);
	}

	GPU_material_flag_set(mat, GPU_MATFLAG_DIFFUSE | GPU_MATFLAG_GLOSSY);

	return GPU_stack_link(mat, node, "node_eevee_specular", in, out, GPU_uniform(&node->ssr_id));
}


/* node type definition */
void register_node_type_sh_eevee_specular(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_EEVEE_SPECULAR, "Specular", NODE_CLASS_SHADER, 0);
	node_type_socket_templates(&ntype, sh_node_eevee_specular_in, sh_node_eevee_specular_out);
	node_type_init(&ntype, NULL);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_gpu(&ntype, node_shader_gpu_eevee_specular);

	nodeRegisterType(&ntype);
}
