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

static bNodeSocketTemplate sh_node_bsdf_principled_in[] = {
	{	SOCK_RGBA, 1, N_("Base Color"),				0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 1, N_("Subsurface"),			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_VECTOR, 1, N_("Subsurface Radius"),	1.0f, 0.2f, 0.1f, 0.0f, 0.0f, 100.0f},
	{	SOCK_RGBA, 1, N_("Subsurface Color"),		0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 1, N_("Metallic"),				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, N_("Specular"),				0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, N_("Specular Tint"),			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, N_("Roughness"),				0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, N_("Anisotropic"),			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, N_("Anisotropic Rotation"),	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, N_("Sheen"),					0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, N_("Sheen Tint"),			0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, N_("Clearcoat"),				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, N_("Clearcoat Roughness"),	0.03f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, N_("IOR"),					1.45f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_FLOAT, 1, N_("Transmission"),			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, N_("Transmission Roughness"),0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_VECTOR, 1, N_("Normal"),				0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	SOCK_VECTOR, 1, N_("Clearcoat Normal"),		0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	SOCK_VECTOR, 1, N_("Tangent"),				0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_bsdf_principled_out[] = {
	{	SOCK_SHADER, 0, N_("BSDF")},
	{	-1, 0, ""	}
};

static void node_shader_init_principled(bNodeTree *UNUSED(ntree), bNode *node)
{
	node->custom1 = SHD_GLOSSY_GGX;
	node->custom2 = SHD_SUBSURFACE_BURLEY;
}

static int node_shader_gpu_bsdf_principled(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	GPUNodeLink *sss_scale;
#if 0 /* Old 2.7 glsl viewport */
	// normal
	if (!in[17].link)
		in[17].link = GPU_builtin(GPU_VIEW_NORMAL);
	else
		GPU_link(mat, "direction_transform_m4v3", in[17].link, GPU_builtin(GPU_VIEW_MATRIX), &in[17].link);

	// clearcoat normal
	if (!in[18].link)
		in[18].link = GPU_builtin(GPU_VIEW_NORMAL);
	else
		GPU_link(mat, "direction_transform_m4v3", in[18].link, GPU_builtin(GPU_VIEW_MATRIX), &in[18].link);
#endif

	/* Normals */
	if (!in[17].link) {
		GPU_link(mat, "world_normals_get", &in[17].link);
	}

	/* Clearcoat Normals */
	if (!in[18].link) {
		GPU_link(mat, "world_normals_get", &in[18].link);
	}

	/* Tangents */
	if (!in[19].link) {
		GPUNodeLink *orco = GPU_attribute(CD_ORCO, "");
		GPU_link(mat, "tangent_orco_z", orco, &in[19].link);
		GPU_link(mat, "node_tangent",
		        GPU_builtin(GPU_VIEW_NORMAL), in[19].link, GPU_builtin(GPU_OBJECT_MATRIX), GPU_builtin(GPU_INVERSE_VIEW_MATRIX),
		        &in[19].link);
	}

	/* SSS Profile */
	if (node->sss_id == 1) {
		static short profile = SHD_SUBSURFACE_BURLEY;
		bNodeSocket *socket = BLI_findlink(&node->original->inputs, 2);
		bNodeSocketValueRGBA *socket_data = socket->default_value;
		/* For some reason it seems that the socket value is in ARGB format. */
		GPU_material_sss_profile_create(mat, &socket_data->value[1], &profile, NULL);
	}

	if (in[2].link) {
		sss_scale = in[2].link;
	}
	else {
		float one[3] = {1.0f, 1.0f, 1.0f};
		GPU_link(mat, "set_rgb", GPU_uniform((float *)one), &sss_scale);
	}

	return GPU_stack_link(mat, node, "node_bsdf_principled_clearcoat", in, out, GPU_builtin(GPU_VIEW_POSITION),
	                      GPU_uniform(&node->ssr_id), GPU_uniform(&node->sss_id), sss_scale);
}

static void node_shader_update_principled(bNodeTree *UNUSED(ntree), bNode *node)
{
	bNodeSocket *sock;
	int distribution = node->custom1;

	for (sock = node->inputs.first; sock; sock = sock->next) {
		if (STREQ(sock->name, "Transmission Roughness")) {
			if (distribution == SHD_GLOSSY_GGX)
				sock->flag &= ~SOCK_UNAVAIL;
			else
				sock->flag |= SOCK_UNAVAIL;

		}
	}
}

/* node type definition */
void register_node_type_sh_bsdf_principled(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_BSDF_PRINCIPLED, "Principled BSDF", NODE_CLASS_SHADER, 0);
	node_type_socket_templates(&ntype, sh_node_bsdf_principled_in, sh_node_bsdf_principled_out);
	node_type_size_preset(&ntype, NODE_SIZE_LARGE);
	node_type_init(&ntype, node_shader_init_principled);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_gpu(&ntype, node_shader_gpu_bsdf_principled);
	node_type_update(&ntype, node_shader_update_principled, NULL);

	nodeRegisterType(&ntype);
}
