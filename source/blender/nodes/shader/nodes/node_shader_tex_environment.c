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

#include "IMB_colormanagement.h"

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_tex_environment_in[] = {
	{	SOCK_VECTOR, 1, N_("Vector"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_environment_out[] = {
	{	SOCK_RGBA, 0, N_("Color"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_NO_INTERNAL_LINK},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_environment(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeTexEnvironment *tex = MEM_callocN(sizeof(NodeTexEnvironment), "NodeTexEnvironment");
	default_tex_mapping(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
	default_color_mapping(&tex->base.color_mapping);
	tex->color_space = SHD_COLORSPACE_COLOR;
	tex->projection = SHD_PROJ_EQUIRECTANGULAR;
	tex->iuser.frames = 1;
	tex->iuser.sfra = 1;
	tex->iuser.fie_ima = 2;
	tex->iuser.ok = 1;

	node->storage = tex;
}

static int node_shader_gpu_tex_environment(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	Image *ima = (Image *)node->id;
	ImageUser *iuser = NULL;
	NodeTexImage *tex = node->storage;
	int isdata = tex->color_space == SHD_COLORSPACE_NONE;
	int ret;

	if (!ima)
		return GPU_stack_link(mat, "node_tex_environment_empty", in, out);

	if (!in[0].link)
		in[0].link = GPU_builtin(GPU_VIEW_POSITION);

	node_shader_gpu_tex_mapping(mat, node, in, out);

	ret = GPU_stack_link(mat, "node_tex_environment", in, out, GPU_image(ima, iuser, isdata));

	if (ret) {
		ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, NULL);
		if (ibuf && (ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA) == 0 &&
		    GPU_material_do_color_management(mat))
		{
			GPU_link(mat, "srgb_to_linearrgb", out[0].link, &out[0].link);
		}
		BKE_image_release_ibuf(ima, ibuf, NULL);
	}

	return ret;
}

/* node type definition */
void register_node_type_sh_tex_environment(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_TEX_ENVIRONMENT, "Environment Texture", NODE_CLASS_TEXTURE, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_tex_environment_in, sh_node_tex_environment_out);
	node_type_init(&ntype, node_shader_init_tex_environment);
	node_type_storage(&ntype, "NodeTexEnvironment", node_free_standard_storage, node_copy_standard_storage);
	node_type_gpu(&ntype, node_shader_gpu_tex_environment);

	nodeRegisterType(&ntype);
}
