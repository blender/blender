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

static bNodeSocketTemplate sh_node_subsurface_scattering_in[] = {
	{	SOCK_RGBA, 1, N_("Color"),			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 1, N_("Scale"),			1.0, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_VECTOR, 1, N_("Radius"),		1.0f, 0.2f, 0.1f, 0.0f, 0.0f, 100.0f},
	{	SOCK_FLOAT, 1, N_("Sharpness"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, N_("Texture Blur"),	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_VECTOR, 1, N_("Normal"),		0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_subsurface_scattering_out[] = {
	{	SOCK_SHADER, 0, N_("BSSRDF")},
	{	-1, 0, ""	}
};

static void node_shader_init_subsurface_scattering(bNodeTree *UNUSED(ntree), bNode *node)
{
	node->custom1 = SHD_SUBSURFACE_BURLEY;
}

static int node_shader_gpu_subsurface_scattering(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	if (!in[5].link)
		GPU_link(mat, "world_normals_get", &in[5].link);

	GPU_material_flag_set(mat, GPU_MATFLAG_DIFFUSE | GPU_MATFLAG_SSS);

	if (node->sss_id == 1) {
		bNodeSocket *socket = BLI_findlink(&node->original->inputs, 2);
		bNodeSocketValueRGBA *socket_data = socket->default_value;
		bNodeSocket *socket_sharp = BLI_findlink(&node->original->inputs, 3);
		bNodeSocketValueFloat *socket_data_sharp = socket_sharp->default_value;
		/* For some reason it seems that the socket value is in ARGB format. */
		GPU_material_sss_profile_create(mat, &socket_data->value[1],
		                                     &node->original->custom1,
		                                     &socket_data_sharp->value);
	}

	return GPU_stack_link(mat, node, "node_subsurface_scattering", in, out, GPU_constant(&node->sss_id));
}

static void node_shader_update_subsurface_scattering(bNodeTree *UNUSED(ntree), bNode *node)
{
	bNodeSocket *sock;
	int falloff = node->custom1;

	for (sock = node->inputs.first; sock; sock = sock->next) {
		if (STREQ(sock->name, "Sharpness")) {
			if (falloff == SHD_SUBSURFACE_CUBIC)
				sock->flag &= ~SOCK_UNAVAIL;
			else
				sock->flag |= SOCK_UNAVAIL;

		}
	}
}

/* node type definition */
void register_node_type_sh_subsurface_scattering(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_SUBSURFACE_SCATTERING, "Subsurface Scattering", NODE_CLASS_SHADER, 0);
	node_type_socket_templates(&ntype, sh_node_subsurface_scattering_in, sh_node_subsurface_scattering_out);
	node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
	node_type_init(&ntype, node_shader_init_subsurface_scattering);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_gpu(&ntype, node_shader_gpu_subsurface_scattering);
	node_type_update(&ntype, node_shader_update_subsurface_scattering, NULL);

	nodeRegisterType(&ntype);
}
