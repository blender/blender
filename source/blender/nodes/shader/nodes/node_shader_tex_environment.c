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
	BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
	BKE_texture_colormapping_default(&tex->base.color_mapping);
	tex->color_space = SHD_COLORSPACE_COLOR;
	tex->projection = SHD_PROJ_EQUIRECTANGULAR;
	tex->iuser.frames = 1;
	tex->iuser.sfra = 1;
	tex->iuser.ok = 1;

	node->storage = tex;
}

static int node_shader_gpu_tex_environment(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	Image *ima = (Image *)node->id;
	ImageUser *iuser = NULL;
	NodeTexEnvironment *tex = node->storage;
	int isdata = tex->color_space == SHD_COLORSPACE_NONE;
	GPUNodeLink *outalpha;

	if (!ima)
		return GPU_stack_link(mat, node, "node_tex_environment_empty", in, out);

	if (!in[0].link) {
		GPU_link(mat, "node_tex_environment_texco", GPU_builtin(GPU_VIEW_POSITION), &in[0].link);
	}

	node_shader_gpu_tex_mapping(mat, node, in, out);

	/* Compute texture coordinate. */
	if (tex->projection == SHD_PROJ_EQUIRECTANGULAR) {
		/* To fix pole issue we clamp the v coordinate. The clamp value depends on the filter size. */
		float clamp_size = (ELEM(tex->interpolation, SHD_INTERP_CUBIC, SHD_INTERP_SMART)) ? 1.5 : 0.5;
		GPU_link(mat, "node_tex_environment_equirectangular", in[0].link, GPU_constant(&clamp_size),
		                                                      GPU_image(ima, iuser, isdata), &in[0].link);
	}
	else {
		GPU_link(mat, "node_tex_environment_mirror_ball", in[0].link, &in[0].link);
	}

	/* Sample texture with correct interpolation. */
	switch (tex->interpolation) {
		case SHD_INTERP_LINEAR:
			/* Force the highest mipmap and don't do anisotropic filtering.
			 * This is to fix the artifact caused by derivatives discontinuity. */
			GPU_link(mat, "node_tex_image_linear_no_mip", in[0].link, GPU_image(ima, iuser, isdata), &out[0].link, &outalpha);
			break;
		case SHD_INTERP_CLOSEST:
			GPU_link(mat, "node_tex_image_nearest", in[0].link, GPU_image(ima, iuser, isdata), &out[0].link, &outalpha);
			break;
		default:
			GPU_link(mat, "node_tex_image_cubic", in[0].link, GPU_image(ima, iuser, isdata), &out[0].link, &outalpha);
			break;
	}

	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, NULL);
	if (ibuf && (ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA) == 0 &&
	    GPU_material_do_color_management(mat))
	{
		GPU_link(mat, "srgb_to_linearrgb", out[0].link, &out[0].link);
	}
	BKE_image_release_ibuf(ima, ibuf, NULL);

	return true;
}

/* node type definition */
void register_node_type_sh_tex_environment(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_TEX_ENVIRONMENT, "Environment Texture", NODE_CLASS_TEXTURE, 0);
	node_type_socket_templates(&ntype, sh_node_tex_environment_in, sh_node_tex_environment_out);
	node_type_init(&ntype, node_shader_init_tex_environment);
	node_type_storage(&ntype, "NodeTexEnvironment", node_free_standard_storage, node_copy_standard_storage);
	node_type_gpu(&ntype, node_shader_gpu_tex_environment);
	node_type_label(&ntype, node_image_label);
	node_type_size_preset(&ntype, NODE_SIZE_LARGE);

	nodeRegisterType(&ntype);
}
