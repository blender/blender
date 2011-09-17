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

static float wood(float p[3], float size, int type, int wave, int basis, unsigned int hard, float turb)
{
	float x = p[0];
	float y = p[1];
	float z = p[2];

	if(type == SHD_WOOD_BANDS) {
		return noise_wave(wave, (x + y + z)*10.0f);
	}
	else if(type == SHD_WOOD_RINGS) {
		return noise_wave(wave, sqrt(x*x + y*y + z*z)*20.0f);
	}
	else if (type == SHD_WOOD_BAND_NOISE) {
		float psize[3] = {p[0]/size, p[1]/size, p[2]/size};
		float wi = turb*noise_basis_hard(psize, basis, hard);
		return noise_wave(wave, (x + y + z)*10.0f + wi);
	}
	else if (type == SHD_WOOD_RING_NOISE) {
		float psize[3] = {p[0]/size, p[1]/size, p[2]/size};
		float wi = turb*noise_basis_hard(psize, basis, hard);
		return noise_wave(wave, sqrt(x*x + y*y + z*z)*20.0f + wi);
	}

	return 0.0f;
}

/* **************** WOOD ******************** */

static bNodeSocketTemplate sh_node_tex_wood_in[]= {
	{	SOCK_VECTOR, 1, "Vector",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	SOCK_FLOAT, 1, "Size",			0.25f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_FLOAT, 1, "Turbulence",	5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_wood_out[]= {
	{	SOCK_FLOAT, 0, "Fac",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_wood(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeTexWood *tex = MEM_callocN(sizeof(NodeTexWood), "NodeTexWood");
	tex->type = SHD_WOOD_BANDS;
	tex->wave = SHD_WAVE_SINE;
	tex->basis = SHD_NOISE_PERLIN;
	tex->hard = 0;

	node->storage = tex;
}

static void node_shader_exec_tex_wood(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	ShaderCallData *scd= (ShaderCallData*)data;
	NodeTexWood *tex= (NodeTexWood*)node->storage;
	bNodeSocket *vecsock = node->inputs.first;
	float vec[3], size, turbulence;
	
	if(vecsock->link)
		nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	else
		copy_v3_v3(vec, scd->co);

	nodestack_get_vec(&size, SOCK_FLOAT, in[1]);
	nodestack_get_vec(&turbulence, SOCK_FLOAT, in[2]);

	out[0]->vec[0]= wood(vec, size, tex->type, tex->wave, tex->basis, tex->hard, turbulence);
}

static int node_shader_gpu_tex_wood(GPUMaterial *mat, bNode *UNUSED(node), GPUNodeStack *in, GPUNodeStack *out)
{
	if(!in[0].link)
		in[0].link = GPU_attribute(CD_ORCO, "");

	return GPU_stack_link(mat, "node_tex_wood", in, out);
}

/* node type definition */
void register_node_type_sh_tex_wood(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, SH_NODE_TEX_WOOD, "Wood Texture", NODE_CLASS_TEXTURE, 0);
	node_type_socket_templates(&ntype, sh_node_tex_wood_in, sh_node_tex_wood_out);
	node_type_size(&ntype, 150, 60, 200);
	node_type_init(&ntype, node_shader_init_tex_wood);
	node_type_storage(&ntype, "NodeTexWood", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_shader_exec_tex_wood);
	node_type_gpu(&ntype, node_shader_gpu_tex_wood);

	nodeRegisterType(lb, &ntype);
};

