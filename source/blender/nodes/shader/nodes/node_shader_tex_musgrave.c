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

/* Musgrave fBm
 *
 * H: fractal increment parameter
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 *
 * from "Texturing and Modelling: A procedural approach"
 */

static float noise_musgrave_fBm(float p[3], int basis, float H, float lacunarity, float octaves)
{
	float rmd;
	float value = 0.0f;
	float pwr = 1.0f;
	float pwHL = pow(lacunarity, -H);
	int i;

	for(i = 0; i < (int)octaves; i++) {
		value += noise_basis(p, basis) * pwr;
		pwr *= pwHL;
		mul_v3_fl(p, lacunarity);
	}

	rmd = octaves - floor(octaves);
	if(rmd != 0.0f)
		value += rmd * noise_basis(p, basis) * pwr;

	return value;
}

/* Musgrave Multifractal
 *
 * H: highest fractal dimension
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 */

static float noise_musgrave_multi_fractal(float p[3], int basis, float H, float lacunarity, float octaves)
{
	float rmd;
	float value = 1.0f;
	float pwr = 1.0f;
	float pwHL = pow(lacunarity, -H);
	int i;

	for(i = 0; i < (int)octaves; i++) {
		value *= (pwr * noise_basis(p, basis) + 1.0f);
		pwr *= pwHL;
		mul_v3_fl(p, lacunarity);
	}

	rmd = octaves - floor(octaves);
	if(rmd != 0.0f)
		value *= (rmd * pwr * noise_basis(p, basis) + 1.0f); /* correct? */

	return value;
}

/* Musgrave Heterogeneous Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

static float noise_musgrave_hetero_terrain(float p[3], int basis, float H, float lacunarity, float octaves, float offset)
{
	float value, increment, rmd;
	float pwHL = pow(lacunarity, -H);
	float pwr = pwHL;
	int i;

	/* first unscaled octave of function; later octaves are scaled */
	value = offset + noise_basis(p, basis);
	mul_v3_fl(p, lacunarity);

	for(i = 1; i < (int)octaves; i++) {
		increment = (noise_basis(p, basis) + offset) * pwr * value;
		value += increment;
		pwr *= pwHL;
		mul_v3_fl(p, lacunarity);
	}

	rmd = octaves - floor(octaves);
	if(rmd != 0.0f) {
		increment = (noise_basis(p, basis) + offset) * pwr * value;
		value += rmd * increment;
	}

	return value;
}

/* Hybrid Additive/Multiplicative Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

static float noise_musgrave_hybrid_multi_fractal(float p[3], int basis, float H, float lacunarity, float octaves, float offset, float gain)
{
	float result, signal, weight, rmd;
	float pwHL = pow(lacunarity, -H);
	float pwr = pwHL;
	int i;

	result = noise_basis(p, basis) + offset;
	weight = gain * result;
	mul_v3_fl(p, lacunarity);

	for(i = 1; (weight > 0.001f) && (i < (int)octaves); i++) {
		if(weight > 1.0f)
			weight = 1.0f;

		signal = (noise_basis(p, basis) + offset) * pwr;
		pwr *= pwHL;
		result += weight * signal;
		weight *= gain * signal;
		mul_v3_fl(p, lacunarity);
	}

	rmd = octaves - floor(octaves);
	if(rmd != 0.0f)
		result += rmd * ((noise_basis(p, basis) + offset) * pwr);

	return result;
}

/* Ridged Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

static float noise_musgrave_ridged_multi_fractal(float p[3], int basis, float H, float lacunarity, float octaves, float offset, float gain)
{
	float result, signal, weight;
	float pwHL = pow(lacunarity, -H);
	float pwr = pwHL;
	int i;

	signal = offset - fabsf(noise_basis(p, basis));
	signal *= signal;
	result = signal;
	weight = 1.0f;

	for(i = 1; i < (int)octaves; i++) {
		mul_v3_fl(p, lacunarity);
		weight = CLAMPIS(signal * gain, 0.0f, 1.0f);
		signal = offset - fabsf(noise_basis(p, basis));
		signal *= signal;
		signal *= weight;
		result += signal * pwr;
		pwr *= pwHL;
	}

	return result;
}

static float musgrave(int type, int basis, float dimension, float lacunarity, float octaves, float offset, float intensity, float gain, float size, float vec[3])
{
	float p[3];

	mul_v3_v3fl(p, vec, 1.0f/size);

	if(type == SHD_MUSGRAVE_MULTIFRACTAL)
		return intensity*noise_musgrave_multi_fractal(p, basis, dimension, lacunarity, octaves);
	else if(type == SHD_MUSGRAVE_FBM)
		return intensity*noise_musgrave_fBm(p, basis, dimension, lacunarity, octaves);
	else if(type == SHD_MUSGRAVE_HYBRID_MULTIFRACTAL)
		return intensity*noise_musgrave_hybrid_multi_fractal(p, basis, dimension, lacunarity, octaves, offset, gain);
	else if(type == SHD_MUSGRAVE_RIDGED_MULTIFRACTAL)
		return intensity*noise_musgrave_ridged_multi_fractal(p, basis, dimension, lacunarity, octaves, offset, gain);
	else if(type == SHD_MUSGRAVE_HETERO_TERRAIN)
		return intensity*noise_musgrave_hetero_terrain(p, basis, dimension, lacunarity, octaves, offset);
	
	return 0.0f;
}

/* **************** MUSGRAVE ******************** */

static bNodeSocketTemplate sh_node_tex_musgrave_in[]= {
	{	SOCK_VECTOR, 1, "Vector",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	SOCK_FLOAT, 1, "Size",			0.25f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_FLOAT, 1, "Dimension",		2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_FLOAT, 1, "Lacunarity",	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_FLOAT, 1, "Octaves",		2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_FLOAT, 1, "Offset",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_FLOAT, 1, "Gain",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_musgrave_out[]= {
	{	SOCK_FLOAT, 0, "Fac",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_musgrave(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeTexMusgrave *tex = MEM_callocN(sizeof(NodeTexMusgrave), "NodeTexMusgrave");
	tex->type = SHD_MUSGRAVE_FBM;
	tex->basis = SHD_NOISE_PERLIN;

	node->storage = tex;
}

static void node_shader_exec_tex_musgrave(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	ShaderCallData *scd= (ShaderCallData*)data;
	NodeTexMusgrave *tex= (NodeTexMusgrave*)node->storage;
	bNodeSocket *vecsock = node->inputs.first;
	float vec[3], size, dimension, lacunarity, octaves, offset, gain;
	
	if(vecsock->link)
		nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	else
		copy_v3_v3(vec, scd->co);

	nodestack_get_vec(&size, SOCK_FLOAT, in[1]);
	nodestack_get_vec(&dimension, SOCK_FLOAT, in[2]);
	nodestack_get_vec(&lacunarity, SOCK_FLOAT, in[3]);
	nodestack_get_vec(&octaves, SOCK_FLOAT, in[4]);
	nodestack_get_vec(&offset, SOCK_FLOAT, in[5]);
	nodestack_get_vec(&gain, SOCK_FLOAT, in[6]);

	out[0]->vec[0]= musgrave(tex->type, tex->basis, dimension, lacunarity, octaves, offset, 1.0f, gain, size, vec);
}

static int node_shader_gpu_tex_musgrave(GPUMaterial *mat, bNode *UNUSED(node), GPUNodeStack *in, GPUNodeStack *out)
{
	if(!in[0].link)
		in[0].link = GPU_attribute(CD_ORCO, "");

	return GPU_stack_link(mat, "node_tex_musgrave", in, out);
}

/* node type definition */
void register_node_type_sh_tex_musgrave(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, SH_NODE_TEX_MUSGRAVE, "Musgrave Texture", NODE_CLASS_TEXTURE, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_tex_musgrave_in, sh_node_tex_musgrave_out);
	node_type_size(&ntype, 150, 60, 200);
	node_type_init(&ntype, node_shader_init_tex_musgrave);
	node_type_storage(&ntype, "NodeTexMusgrave", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_shader_exec_tex_musgrave);
	node_type_gpu(&ntype, node_shader_gpu_tex_musgrave);

	nodeRegisterType(lb, &ntype);
};

