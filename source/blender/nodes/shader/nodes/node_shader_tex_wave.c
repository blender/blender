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

static float wave(float vec[3], float scale, int type, float distortion, float detail)
{
	float p[3], w, n;

	mul_v3_v3fl(p, vec, scale);

	if(type == SHD_WAVE_BANDS)
		n= (p[0] + p[1] + p[2])*10.0f;
	else /* if(type == SHD_WAVE_RINGS) */
		n= len_v3(p)*20.0f;
	
	w = noise_wave(SHD_WAVE_SINE, n);
	
	/* XXX size compare! */
	if(distortion != 0.0f)
		w += distortion * noise_turbulence(p, SHD_NOISE_PERLIN, detail, 0);

	return w;
}

/* **************** WAVE ******************** */

static bNodeSocketTemplate sh_node_tex_wave_in[]= {
	{	SOCK_VECTOR, 1, "Vector",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	SOCK_FLOAT, 1, "Scale",			5.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
	{	SOCK_FLOAT, 1, "Distortion",	0.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
	{	SOCK_FLOAT, 1, "Detail",		2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 16.0f},
	{	SOCK_FLOAT, 1, "Detail Scale",	1.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_wave_out[]= {
	{	SOCK_RGBA, 0, "Color",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 0, "Fac",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_wave(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeTexWave *tex = MEM_callocN(sizeof(NodeTexWave), "NodeTexWave");
	default_tex_mapping(&tex->base.tex_mapping);
	default_color_mapping(&tex->base.color_mapping);
	tex->wave_type = SHD_WAVE_BANDS;

	node->storage = tex;
}

static void node_shader_exec_tex_wave(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	ShaderCallData *scd= (ShaderCallData*)data;
	NodeTexWave *tex= (NodeTexWave*)node->storage;
	bNodeSocket *vecsock = node->inputs.first;
	float vec[3], scale, detail, distortion, fac;
	
	if(vecsock->link)
		nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	else
		copy_v3_v3(vec, scd->co);

	nodestack_get_vec(&scale, SOCK_FLOAT, in[1]);
	nodestack_get_vec(&detail, SOCK_FLOAT, in[1]);
	nodestack_get_vec(&distortion, SOCK_FLOAT, in[2]);

	fac= wave(vec, scale, tex->wave_type, distortion, detail);
	out[0]->vec[0]= fac;
	out[0]->vec[1]= fac;
	out[0]->vec[2]= fac;
	out[1]->vec[0]= fac;
}

static int node_shader_gpu_tex_wave(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	if(!in[0].link)
		in[0].link = GPU_attribute(CD_ORCO, "");

	node_shader_gpu_tex_mapping(mat, node, in, out);

	return GPU_stack_link(mat, "node_tex_wave", in, out);
}

/* node type definition */
void register_node_type_sh_tex_wave(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, SH_NODE_TEX_WAVE, "Wave Texture", NODE_CLASS_TEXTURE, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_tex_wave_in, sh_node_tex_wave_out);
	node_type_size(&ntype, 150, 60, 200);
	node_type_init(&ntype, node_shader_init_tex_wave);
	node_type_storage(&ntype, "NodeTexWave", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_shader_exec_tex_wave);
	node_type_gpu(&ntype, node_shader_gpu_tex_wave);

	nodeRegisterType(lb, &ntype);
}

