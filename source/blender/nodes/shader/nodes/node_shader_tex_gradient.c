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
 * The Original Code is Copyright (C) 2005 Gradienter Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../node_shader_util.h"

static float gradient(float p[3], int type)
{
	float x, y;

	x= p[0];
	y= p[1];

	if(type == SHD_BLEND_LINEAR) {
		return (1.0f + x)/2.0f;
	}
	else if(type == SHD_BLEND_QUADRATIC) {
		float r = MAX2((1.0f + x)/2.0f, 0.0f);
		return r*r;
	}
	else if(type == SHD_BLEND_EASING) {
		float r = MIN2(MAX2((1.0f + x)/2.0f, 0.0f), 1.0f);
		float t = r*r;
		
		return (3.0f*t - 2.0f*t*r);
	}
	else if(type == SHD_BLEND_DIAGONAL) {
		return (2.0f + x + y)/4.0f;
	}
	else if(type == SHD_BLEND_RADIAL) {
		return atan2(y, x)/(2.0f*(float)M_PI) + 0.5f;
	}
	else {
		float r = MAX2(1.0f - sqrtf(x*x + y*y + p[2]*p[2]), 0.0f);

		if(type == SHD_BLEND_QUADRATIC_SPHERE)
			return r*r;
		else if(type == SHD_BLEND_SPHERICAL)
			return r;
	}

	return 0.0f;
}

/* **************** BLEND ******************** */

static bNodeSocketTemplate sh_node_tex_gradient_in[]= {
	{	SOCK_VECTOR, 1, "Vector",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_gradient_out[]= {
	{	SOCK_RGBA, 0, "Color",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 0, "Fac",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_gradient(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeTexGradient *tex = MEM_callocN(sizeof(NodeTexGradient), "NodeTexGradient");
	default_tex_mapping(&tex->base.tex_mapping);
	default_color_mapping(&tex->base.color_mapping);
	tex->gradient_type = SHD_BLEND_LINEAR;

	node->storage = tex;
}

static void node_shader_exec_tex_gradient(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	ShaderCallData *scd= (ShaderCallData*)data;
	NodeTexGradient *tex= (NodeTexGradient*)node->storage;
	bNodeSocket *vecsock = node->inputs.first;
	float vec[3], fac;
	
	if(vecsock->link)
		nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	else
		copy_v3_v3(vec, scd->co);
	
	fac= gradient(vec, tex->gradient_type);
	CLAMP(fac, 0.0f, 1.0f);
	
	out[0]->vec[0]= fac;
	out[0]->vec[1]= fac;
	out[0]->vec[2]= fac;
	out[1]->vec[0]= fac;
}

static int node_shader_gpu_tex_gradient(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	if(!in[0].link)
		in[0].link = GPU_attribute(CD_ORCO, "");

	node_shader_gpu_tex_mapping(mat, node, in, out);

	return GPU_stack_link(mat, "node_tex_gradient", in, out);
}

/* node type definition */
void register_node_type_sh_tex_gradient(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, SH_NODE_TEX_GRADIENT, "Gradient Texture", NODE_CLASS_TEXTURE, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_tex_gradient_in, sh_node_tex_gradient_out);
	node_type_size(&ntype, 150, 60, 200);
	node_type_init(&ntype, node_shader_init_tex_gradient);
	node_type_storage(&ntype, "NodeTexGradient", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_shader_exec_tex_gradient);
	node_type_gpu(&ntype, node_shader_gpu_tex_gradient);

	nodeRegisterType(lb, &ntype);
};

