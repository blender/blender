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

/* **************** Fresnel ******************** */
static bNodeSocketTemplate sh_node_fresnel_in[] = {
	{	SOCK_FLOAT, 1, N_("IOR"),	1.45f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_VECTOR, 1, N_("Normal"),	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_fresnel_out[] = {
	{	SOCK_FLOAT, 0, N_("Fac"),	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	-1, 0, ""	}
};

static int node_shader_gpu_fresnel(GPUMaterial *mat, bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	if (!in[1].link) {
		in[1].link = GPU_builtin(GPU_VIEW_NORMAL);
	}
	else if (GPU_material_use_world_space_shading(mat)) {
		GPU_link(mat, "direction_transform_m4v3", in[1].link, GPU_builtin(GPU_VIEW_MATRIX), &in[1].link);
	}
	
	return GPU_stack_link(mat, "node_fresnel", in, out, GPU_builtin(GPU_VIEW_POSITION));
}

static void node_shader_exec_fresnel(void *data, int UNUSED(thread), bNode *node, bNodeExecData *UNUSED(execdata), bNodeStack **in, bNodeStack **out)
{
	ShadeInput *shi = ((ShaderCallData *)data)->shi;

	/* Compute IOR. */
	float eta;
	nodestack_get_vec(&eta, SOCK_FLOAT, in[0]);
	eta = max_ff(eta, 0.00001);
	eta = shi->flippednor ? 1 / eta : eta;

	/* Get normal from socket, but only if linked. */
	bNodeSocket *sock_normal = node->inputs.first;
	sock_normal = sock_normal->next;

	float n[3];
	if (sock_normal->link) {
		nodestack_get_vec(n, SOCK_VECTOR, in[1]);
	}
	else {
		copy_v3_v3(n, shi->vn);
	}

	if (shi->use_world_space_shading) {
		mul_mat3_m4_v3((float (*)[4])RE_render_current_get_matrix(RE_VIEW_MATRIX), n);
	}

	out[0]->vec[0] = RE_fresnel_dielectric(shi->view, n, eta);
}

/* node type definition */
void register_node_type_sh_fresnel(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_FRESNEL, "Fresnel", NODE_CLASS_INPUT, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING | NODE_OLD_SHADING);
	node_type_socket_templates(&ntype, sh_node_fresnel_in, sh_node_fresnel_out);
	node_type_init(&ntype, NULL);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_gpu(&ntype, node_shader_gpu_fresnel);
	node_type_exec(&ntype, NULL, NULL, node_shader_exec_fresnel);

	nodeRegisterType(&ntype);
}
