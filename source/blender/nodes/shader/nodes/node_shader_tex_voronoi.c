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

static float voronoi_tex(int distance_metric, int coloring,
	float weight1, float weight2, float weight3, float weight4,
	float exponent, float intensity, float size, float vec[3], float color[3])
{
	float aw1 = fabsf(weight1);
	float aw2 = fabsf(weight2);
	float aw3 = fabsf(weight3);
	float aw4 = fabsf(weight4);
	float sc = (aw1 + aw2 + aw3 + aw4);
	float da[4];
	float pa[4][3];
	float fac;
	float p[3];

	if(sc != 0.0f)
		sc = intensity/sc;
	
	/* compute distance and point coordinate of 4 nearest neighbours */
	mul_v3_v3fl(p, vec, 1.0f/size);
	voronoi_generic(p, distance_metric, exponent, da, pa);

	/* Scalar output */
	fac = sc * fabsf(weight1*da[0] + weight2*da[1] + weight3*da[2] + weight4*da[3]);

	/* colored output */
	if(coloring == SHD_VORONOI_INTENSITY) {
		color[0]= color[1]= color[2]= fac;
	}
	else {
		float rgb1[3], rgb2[3], rgb3[3], rgb4[3];

		cellnoise_color(rgb1, pa[0]);
		cellnoise_color(rgb2, pa[1]);
		cellnoise_color(rgb3, pa[2]);
		cellnoise_color(rgb4, pa[3]);

		mul_v3_v3fl(color, rgb1, aw1);
		madd_v3_v3fl(color, rgb2, aw2);
		madd_v3_v3fl(color, rgb3, aw3);
		madd_v3_v3fl(color, rgb4, aw4);

		if(coloring != SHD_VORONOI_POSITION) {
			float t1 = MIN2((da[1] - da[0])*10.0f, 1.0f);

			if(coloring == SHD_VORONOI_POSITION_OUTLINE_INTENSITY)
				mul_v3_fl(color, t1*fac);
			else if(coloring == SHD_VORONOI_POSITION_OUTLINE)
				mul_v3_fl(color, t1*sc);
		}
		else {
			mul_v3_fl(color, sc);
		}
	}

	return fac;
}

/* **************** VORONOI ******************** */

static bNodeSocketTemplate sh_node_tex_voronoi_in[]= {
	{	SOCK_VECTOR, 1, "Vector",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	SOCK_FLOAT, 1, "Size",			0.25f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_FLOAT, 1, "Weight1",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f},
	{	SOCK_FLOAT, 1, "Weight2",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f},
	{	SOCK_FLOAT, 1, "Weight3",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f},
	{	SOCK_FLOAT, 1, "Weight4",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f},
	{	SOCK_FLOAT, 1, "Exponent",		2.5f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_voronoi_out[]= {
	{	SOCK_RGBA, 0, "Color",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 0, "Fac",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_voronoi(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeTexVoronoi *tex = MEM_callocN(sizeof(NodeTexVoronoi), "NodeTexVoronoi");
	default_tex_mapping(&tex->base.tex_mapping);
	default_color_mapping(&tex->base.color_mapping);
	tex->distance_metric = SHD_VORONOI_ACTUAL_DISTANCE;
	tex->coloring = SHD_VORONOI_INTENSITY;

	node->storage = tex;
}

static void node_shader_exec_tex_voronoi(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	ShaderCallData *scd= (ShaderCallData*)data;
	NodeTexVoronoi *tex= (NodeTexVoronoi*)node->storage;
	bNodeSocket *vecsock = node->inputs.first;
	float vec[3], size, w1, w2, w3, w4, exponent;
	
	if(vecsock->link)
		nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	else
		copy_v3_v3(vec, scd->co);

	nodestack_get_vec(&size, SOCK_FLOAT, in[1]);
	nodestack_get_vec(&w1, SOCK_FLOAT, in[2]);
	nodestack_get_vec(&w2, SOCK_FLOAT, in[3]);
	nodestack_get_vec(&w3, SOCK_FLOAT, in[4]);
	nodestack_get_vec(&w4, SOCK_FLOAT, in[5]);
	nodestack_get_vec(&exponent, SOCK_FLOAT, in[6]);

	out[1]->vec[0]= voronoi_tex(tex->distance_metric, tex->coloring, w1, w2, w3, w4,
		exponent, 1.0f, size, vec, out[0]->vec);
}

static int node_shader_gpu_tex_voronoi(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	if(!in[0].link)
		in[0].link = GPU_attribute(CD_ORCO, "");

	node_shader_gpu_tex_mapping(mat, node, in, out);

	return GPU_stack_link(mat, "node_tex_voronoi", in, out);
}

/* node type definition */
void register_node_type_sh_tex_voronoi(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, SH_NODE_TEX_VORONOI, "Voronoi Texture", NODE_CLASS_TEXTURE, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_tex_voronoi_in, sh_node_tex_voronoi_out);
	node_type_size(&ntype, 150, 60, 200);
	node_type_init(&ntype, node_shader_init_tex_voronoi);
	node_type_storage(&ntype, "NodeTexVoronoi", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_shader_exec_tex_voronoi);
	node_type_gpu(&ntype, node_shader_gpu_tex_voronoi);

	nodeRegisterType(lb, &ntype);
};

