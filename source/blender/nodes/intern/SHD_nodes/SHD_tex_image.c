/**
 * $Id: SHD_output.c 32517 2010-10-16 14:32:17Z campbellbarton $
 *
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

#include "../SHD_util.h"

/* **************** OUTPUT ******************** */

static bNodeSocketType sh_node_tex_image_in[]= {
	{	SOCK_VECTOR, 1, "Vector",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, SOCK_NO_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketType sh_node_tex_image_out[]= {
	{	SOCK_RGBA, 0, "Color",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_image(bNode *node)
{
	NodeTexImage *tex = MEM_callocN(sizeof(NodeTexImage), "NodeTexImage");
	tex->color_space = SHD_COLORSPACE_SRGB;

	node->storage = tex;
}

static void node_shader_exec_tex_image(void *data, bNode *node, bNodeStack **in, bNodeStack **UNUSED(out))
{
}

static int node_shader_gpu_tex_image(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	Image *ima= (Image*)node->id;
	ImageUser *iuser= NULL;

	return GPU_stack_link(mat, "node_tex_image", in, out, GPU_image(ima, iuser));
}

/* node type definition */
void register_node_type_sh_tex_image(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, SH_NODE_TEX_IMAGE, "Image Texture", NODE_CLASS_TEXTURE, 0,
		sh_node_tex_image_in, sh_node_tex_image_out);
	node_type_size(&ntype, 150, 60, 200);
	node_type_init(&ntype, node_shader_init_tex_image);
	node_type_storage(&ntype, "NodeTexImage", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_shader_exec_tex_image);
	node_type_gpu(&ntype, node_shader_gpu_tex_image);

	nodeRegisterType(lb, &ntype);
};

