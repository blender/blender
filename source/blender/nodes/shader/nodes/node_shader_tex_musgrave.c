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

/* **************** MUSGRAVE ******************** */

static bNodeSocketTemplate sh_node_tex_musgrave_in[] = {
	{	SOCK_VECTOR, 1, N_("Vector"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	SOCK_FLOAT, 1, N_("Scale"),			5.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
	{	SOCK_FLOAT, 1, N_("Detail"),		2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 16.0f},
	{	SOCK_FLOAT, 1, N_("Dimension"),		2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_FLOAT, 1, N_("Lacunarity"),	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_FLOAT, 1, N_("Offset"),		0.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
	{	SOCK_FLOAT, 1, N_("Gain"),			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_musgrave_out[] = {
	{	SOCK_RGBA, 0, N_("Color"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_NO_INTERNAL_LINK},
	{	SOCK_FLOAT, 0, N_("Fac"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR, SOCK_NO_INTERNAL_LINK},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_musgrave(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeTexMusgrave *tex = MEM_callocN(sizeof(NodeTexMusgrave), "NodeTexMusgrave");
	BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
	BKE_texture_colormapping_default(&tex->base.color_mapping);
	tex->musgrave_type = SHD_MUSGRAVE_FBM;

	node->storage = tex;
}

static int node_shader_gpu_tex_musgrave(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	if (!in[0].link) {
		in[0].link = GPU_attribute(CD_ORCO, "");
		GPU_link(mat, "generated_from_orco", in[0].link, &in[0].link);
	}

	node_shader_gpu_tex_mapping(mat, node, in, out);

	NodeTexMusgrave *tex = (NodeTexMusgrave *)node->storage;
	float type = tex->musgrave_type;

	return GPU_stack_link(mat, "node_tex_musgrave", in, out, GPU_uniform(&type));
}

/* node type definition */
void register_node_type_sh_tex_musgrave(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_TEX_MUSGRAVE, "Musgrave Texture", NODE_CLASS_TEXTURE, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_tex_musgrave_in, sh_node_tex_musgrave_out);
	node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
	node_type_init(&ntype, node_shader_init_tex_musgrave);
	node_type_storage(&ntype, "NodeTexMusgrave", node_free_standard_storage, node_copy_standard_storage);
	node_type_gpu(&ntype, node_shader_gpu_tex_musgrave);

	nodeRegisterType(&ntype);
}
