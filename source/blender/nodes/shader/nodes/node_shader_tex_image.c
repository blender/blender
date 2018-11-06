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

static bNodeSocketTemplate sh_node_tex_image_in[] = {
	{	SOCK_VECTOR, 1, N_("Vector"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_image_out[] = {
	{	SOCK_RGBA, 0, N_("Color"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_NO_INTERNAL_LINK},
	{	SOCK_FLOAT, 0, N_("Alpha"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_NO_INTERNAL_LINK},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_image(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeTexImage *tex = MEM_callocN(sizeof(NodeTexImage), "NodeTexImage");
	BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
	BKE_texture_colormapping_default(&tex->base.color_mapping);
	tex->color_space = SHD_COLORSPACE_COLOR;
	tex->iuser.frames = 1;
	tex->iuser.sfra = 1;
	tex->iuser.ok = 1;

	node->storage = tex;
}

static int node_shader_gpu_tex_image(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	static const char *names[] = {
	    "node_tex_image_linear",
	    "node_tex_image_nearest",
	    "node_tex_image_cubic",
	    "node_tex_image_smart"
	};
	static const char *names_box[] = {
	    "tex_box_sample_linear",
	    "tex_box_sample_nearest",
	    "tex_box_sample_cubic",
	    "tex_box_sample_smart"
	};

	Image *ima = (Image *)node->id;
	ImageUser *iuser = NULL;
	NodeTexImage *tex = node->storage;
	const char *gpu_node_name = (tex->projection == SHD_PROJ_BOX)
	                             ? names_box[tex->interpolation]
	                             : names[tex->interpolation];
	bool do_color_correction = false;

	GPUNodeLink *norm, *col1, *col2, *col3;

	int isdata = tex->color_space == SHD_COLORSPACE_NONE;
	float blend = tex->projection_blend;

	if (!ima)
		return GPU_stack_link(mat, node, "node_tex_image_empty", in, out);

	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, NULL);
	if ((tex->color_space == SHD_COLORSPACE_COLOR) &&
	    ibuf && (ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA) == 0 &&
	    GPU_material_do_color_management(mat))
	{
		do_color_correction = true;
	}
	BKE_image_release_ibuf(ima, ibuf, NULL);

	if (!in[0].link)
		in[0].link = GPU_attribute(CD_MTFACE, "");

	node_shader_gpu_tex_mapping(mat, node, in, out);

	switch (tex->projection) {
		case SHD_PROJ_FLAT:
			GPU_stack_link(mat, node, gpu_node_name, in, out, GPU_image(ima, iuser, isdata));
			break;
		case SHD_PROJ_BOX:
			GPU_link(mat, "direction_transform_m4v3", GPU_builtin(GPU_VIEW_NORMAL),
			                                          GPU_builtin(GPU_INVERSE_VIEW_MATRIX),
			                                          &norm);
			GPU_link(mat, "direction_transform_m4v3", norm,
			                                          GPU_builtin(GPU_INVERSE_OBJECT_MATRIX),
			                                          &norm);
			GPU_link(mat, gpu_node_name, in[0].link,
			                             norm,
			                             GPU_image(ima, iuser, isdata),
			                             &col1,
			                             &col2,
			                             &col3);
			if (do_color_correction) {
				GPU_link(mat, "srgb_to_linearrgb", col1, &col1);
				GPU_link(mat, "srgb_to_linearrgb", col2, &col2);
				GPU_link(mat, "srgb_to_linearrgb", col3, &col3);
			}
			GPU_link(mat, "node_tex_image_box", in[0].link,
			                                    norm,
			                                    col1, col2, col3,
			                                    GPU_image(ima, iuser, isdata),
			                                    GPU_uniform(&blend),
			                                    &out[0].link,
			                                    &out[1].link);
			break;
		case SHD_PROJ_SPHERE:
			GPU_link(mat, "point_texco_remap_square", in[0].link, &in[0].link);
			GPU_link(mat, "point_map_to_sphere", in[0].link, &in[0].link);
			GPU_stack_link(mat, node, gpu_node_name, in, out, GPU_image(ima, iuser, isdata));
			break;
		case SHD_PROJ_TUBE:
			GPU_link(mat, "point_texco_remap_square", in[0].link, &in[0].link);
			GPU_link(mat, "point_map_to_tube", in[0].link, &in[0].link);
			GPU_stack_link(mat, node, gpu_node_name, in, out, GPU_image(ima, iuser, isdata));
			break;
	}

	if (do_color_correction && (tex->projection != SHD_PROJ_BOX)) {
		GPU_link(mat, "srgb_to_linearrgb", out[0].link, &out[0].link);
	}

	return true;
}

/* node type definition */
void register_node_type_sh_tex_image(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_TEX_IMAGE, "Image Texture", NODE_CLASS_TEXTURE, 0);
	node_type_socket_templates(&ntype, sh_node_tex_image_in, sh_node_tex_image_out);
	node_type_init(&ntype, node_shader_init_tex_image);
	node_type_storage(&ntype, "NodeTexImage", node_free_standard_storage, node_copy_standard_storage);
	node_type_gpu(&ntype, node_shader_gpu_tex_image);
	node_type_label(&ntype, node_image_label);
	node_type_size_preset(&ntype, NODE_SIZE_LARGE);

	nodeRegisterType(&ntype);
}
