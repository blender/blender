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

/* **************** Layer Weight ******************** */

static bNodeSocketTemplate sh_node_layer_weight_in[] = {
	{	SOCK_FLOAT, 1, N_("Blend"),		0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 1, N_("Normal"),	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_layer_weight_out[] = {
	{	SOCK_FLOAT, 0, N_("Fresnel"),	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 0, N_("Facing"),	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static int node_shader_gpu_layer_weight(GPUMaterial *mat, bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	if (!in[1].link)
		in[1].link = GPU_builtin(GPU_VIEW_NORMAL);
	else if (GPU_material_use_world_space_shading(mat)) {
		GPU_link(mat, "direction_transform_m4v3", in[1].link, GPU_builtin(GPU_VIEW_MATRIX), &in[1].link);
	}

	return GPU_stack_link(mat, "node_layer_weight", in, out, GPU_builtin(GPU_VIEW_POSITION));
}

static void node_shader_exec_layer_weight(void *data, int UNUSED(thread), bNode *node, bNodeExecData *UNUSED(execdata), bNodeStack **in, bNodeStack **out)
{
	ShadeInput *shi = ((ShaderCallData *)data)->shi;

	/* Compute IOR. */
	float blend;
	nodestack_get_vec(&blend, SOCK_FLOAT, in[0]);
	float eta = max_ff(1 - blend, 0.00001);
	eta = shi->flippednor ? eta : 1 / eta;

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


	if (shi->use_world_space_shading)
		mul_mat3_m4_v3((float (*)[4])RE_render_current_get_matrix(RE_VIEW_MATRIX), n);

	out[0]->vec[0] = RE_fresnel_dielectric(shi->view, n, eta);

	float facing = fabs(dot_v3v3(shi->view, n));
	if (blend != 0.5) {
		CLAMP(blend, 0.0, 0.99999);
		blend = (blend < 0.5) ? 2.0 * blend : 0.5 / (1.0 - blend);
		facing = pow(facing, blend);
	}
	out[1]->vec[0] = 1.0 - facing;
}

/* node type definition */
void register_node_type_sh_layer_weight(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_LAYER_WEIGHT, "Layer Weight", NODE_CLASS_INPUT, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING | NODE_OLD_SHADING);
	node_type_socket_templates(&ntype, sh_node_layer_weight_in, sh_node_layer_weight_out);
	node_type_init(&ntype, NULL);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_gpu(&ntype, node_shader_gpu_layer_weight);
	node_type_exec(&ntype, NULL, NULL, node_shader_exec_layer_weight);

	nodeRegisterType(&ntype);
}
