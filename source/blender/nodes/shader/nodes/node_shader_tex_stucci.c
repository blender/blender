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

static float stucci(int type, int basis, int hard, float turbulence, float size, float vec[3])
{
	float p[3], b2, ofs, r;

	mul_v3_v3fl(p, vec, 1.0f/size);
	b2 = noise_basis_hard(p, basis, hard);
	ofs = turbulence/200.0f;

	if(type != SHD_STUCCI_PLASTIC)
		ofs *= b2*b2;

	p[2] += ofs;
	r = noise_basis_hard(p, basis, hard);

	if(type == SHD_STUCCI_WALL_OUT)
		r = 1.0f - r;

	return MAX2(r, 0.0f);
}

/* **************** STUCCI ******************** */

static bNodeSocketTemplate sh_node_tex_stucci_in[]= {
	{	SOCK_VECTOR, 1, "Vector",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	SOCK_FLOAT, 1, "Size",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_FLOAT, 1, "Turbulence",	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_stucci_out[]= {
	{	SOCK_FLOAT, 0, "Fac",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_stucci(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeTexStucci *tex = MEM_callocN(sizeof(NodeTexStucci), "NodeTexStucci");
	default_tex_mapping(&tex->base.tex_mapping);
	default_color_mapping(&tex->base.color_mapping);
	tex->type = SHD_STUCCI_PLASTIC;
	tex->basis = SHD_NOISE_PERLIN;
	tex->hard = 0;

	node->storage = tex;
}

static void node_shader_exec_tex_stucci(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	ShaderCallData *scd= (ShaderCallData*)data;
	NodeTexStucci *tex= (NodeTexStucci*)node->storage;
	bNodeSocket *vecsock = node->inputs.first;
	float vec[3], size, turbulence;
	
	if(vecsock->link)
		nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	else
		copy_v3_v3(vec, scd->co);

	nodestack_get_vec(&size, SOCK_FLOAT, in[1]);
	nodestack_get_vec(&turbulence, SOCK_FLOAT, in[2]);

	out[0]->vec[0]= stucci(tex->type, tex->basis, tex->hard, turbulence, size, vec);
}

static int node_shader_gpu_tex_stucci(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	if(!in[0].link)
		in[0].link = GPU_attribute(CD_ORCO, "");

	node_shader_gpu_tex_mapping(mat, node, in, out);

	return GPU_stack_link(mat, "node_tex_stucci", in, out);
}

/* node type definition */
void register_node_type_sh_tex_stucci(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, SH_NODE_TEX_STUCCI, "Stucci Texture", NODE_CLASS_TEXTURE, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_tex_stucci_in, sh_node_tex_stucci_out);
	node_type_size(&ntype, 150, 60, 200);
	node_type_init(&ntype, node_shader_init_tex_stucci);
	node_type_storage(&ntype, "NodeTexStucci", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_shader_exec_tex_stucci);
	node_type_gpu(&ntype, node_shader_gpu_tex_stucci);

	nodeRegisterType(lb, &ntype);
};

