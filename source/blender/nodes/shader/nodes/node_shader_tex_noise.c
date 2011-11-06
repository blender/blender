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
#include "node_shader_noise.h"

static float noise_texture(float scale, float detail, float distortion, float vec[3], float color[3])
{
	float p[3], pg[3], pb[3];
	int basis= SHD_NOISE_PERLIN;

	mul_v3_v3fl(p, vec, scale);

	if(distortion != 0.0f) {
		float r[3], p_offset[3], p_noffset[3];
		float offset[3] = {13.5f, 13.5f, 13.5f};

		add_v3_v3v3(p_offset, p, offset);
		sub_v3_v3v3(p_noffset, p, offset);

		r[0] = noise_basis(p_offset, basis) * distortion;
		r[1] = noise_basis(p, basis) * distortion;
		r[2] = noise_basis(p_noffset, basis) * distortion;

		add_v3_v3(p, r);
	}

	pg[0]= p[1];
	pg[1]= p[0];
	pg[2]= p[2];

	pb[0]= p[1];
	pb[1]= p[2];
	pb[2]= p[0];

	color[0]= noise_turbulence(p, basis, detail, 0);
	color[1]= noise_turbulence(pg, basis, detail, 0);
	color[2]= noise_turbulence(pb, basis, detail, 0);

	return color[0];
}

/* **************** NOISE ******************** */

static bNodeSocketTemplate sh_node_tex_noise_in[]= {
	{	SOCK_VECTOR, 1, "Vector",    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	SOCK_FLOAT, 1, "Scale",      5.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
	{	SOCK_FLOAT, 1, "Detail",     2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 16.0f},
	{	SOCK_FLOAT, 1, "Distortion", 0.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_noise_out[]= {
	{	SOCK_RGBA, 0, "Color",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 0, "Fac",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_noise(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeTexNoise *tex = MEM_callocN(sizeof(NodeTexNoise), "NodeTexNoise");
	default_tex_mapping(&tex->base.tex_mapping);
	default_color_mapping(&tex->base.color_mapping);

	node->storage = tex;
}

static void node_shader_exec_tex_noise(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	ShaderCallData *scd= (ShaderCallData*)data;
	bNodeSocket *vecsock = node->inputs.first;
	float vec[3], scale, detail, distortion;
	
	if(vecsock->link)
		nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	else
		copy_v3_v3(vec, scd->co);

	nodestack_get_vec(&scale, SOCK_FLOAT, in[1]);
	nodestack_get_vec(&detail, SOCK_FLOAT, in[2]);
	nodestack_get_vec(&distortion, SOCK_FLOAT, in[3]);

	out[1]->vec[0]= noise_texture(scale, detail, distortion, vec, out[0]->vec);
}

static int node_shader_gpu_tex_noise(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	if(!in[0].link)
		in[0].link = GPU_attribute(CD_ORCO, "");

	node_shader_gpu_tex_mapping(mat, node, in, out);

	return GPU_stack_link(mat, "node_tex_noise", in, out);
}

/* node type definition */
void register_node_type_sh_tex_noise(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, SH_NODE_TEX_NOISE, "Noise Texture", NODE_CLASS_TEXTURE, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_tex_noise_in, sh_node_tex_noise_out);
	node_type_size(&ntype, 150, 60, 200);
	node_type_init(&ntype, node_shader_init_tex_noise);
	node_type_storage(&ntype, "NodeTexNoise", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_shader_exec_tex_noise);
	node_type_gpu(&ntype, node_shader_gpu_tex_noise);

	nodeRegisterType(lb, &ntype);
};

