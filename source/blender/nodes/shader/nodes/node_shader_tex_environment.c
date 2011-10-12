/**
 * $Id: node_shader_output.c 32517 2010-10-16 14:32:17Z campbellbarton $
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

#include "../node_shader_util.h"

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_tex_environment_in[]= {
	{	SOCK_VECTOR, 1, "Vector",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_environment_out[]= {
	{	SOCK_RGBA, 0, "Color",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_environment(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeTexEnvironment *tex = MEM_callocN(sizeof(NodeTexEnvironment), "NodeTexEnvironment");
	tex->color_space = SHD_COLORSPACE_SRGB;

	node->storage = tex;
}

static void node_shader_exec_tex_environment(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	Image *ima= (Image*)node->id;
	ShaderCallData *scd= (ShaderCallData*)data;
	NodeTexEnvironment *tex= (NodeTexEnvironment*)node->storage;
	bNodeSocket *vecsock = node->inputs.first;
	float vec[3];
	
	if(vecsock->link)
		nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	else
		copy_v3_v3(vec, scd->co);

	if(ima) {
		ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);

		if(ibuf) {
			float u= (atan2f(vec[1], vec[0]) + (float)M_PI)/(2*M_PI);
			float v= atan2f(vec[2], hypotf(vec[0], vec[1]))/M_PI + 0.5f;
			float rgb[4];

			ibuf_sample(ibuf, u, v, 0.0f, 0.0f, rgb);

			if(tex->color_space == SHD_COLORSPACE_SRGB)
				srgb_to_linearrgb_v3_v3(out[0]->vec, rgb);
			else
				copy_v3_v3(out[0]->vec, rgb);
		}
	}
}

static int node_shader_gpu_tex_environment(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	Image *ima= (Image*)node->id;
	ImageUser *iuser= NULL;

	if(!ima) {
		float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		GPUNodeLink *vec = GPU_uniform(black);
		return GPU_stack_link(mat, "set_rgba", out, out, vec);
	}

	if(!in[0].link)
		in[0].link = GPU_builtin(GPU_VIEW_POSITION);

	return GPU_stack_link(mat, "node_tex_environment", in, out, GPU_image(ima, iuser));
}

/* node type definition */
void register_node_type_sh_tex_environment(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, SH_NODE_TEX_ENVIRONMENT, "Environment Texture", NODE_CLASS_TEXTURE, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_tex_environment_in, sh_node_tex_environment_out);
	node_type_size(&ntype, 150, 60, 200);
	node_type_init(&ntype, node_shader_init_tex_environment);
	node_type_storage(&ntype, "NodeTexEnvironment", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_shader_exec_tex_environment);
	node_type_gpu(&ntype, node_shader_gpu_tex_environment);

	nodeRegisterType(lb, &ntype);
};

