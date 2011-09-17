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

static float blend(float p[3], int type, int axis)
{
	float x, y;

	if(axis == SHD_BLEND_VERTICAL) {
		x= p[1];
		y= p[0];
	}
	else {
		x= p[0];
		y= p[1];
	}

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

static bNodeSocketTemplate sh_node_tex_blend_in[]= {
	{	SOCK_VECTOR, 1, "Vector",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_blend_out[]= {
	{	SOCK_FLOAT, 0, "Fac",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_blend(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeTexBlend *tex = MEM_callocN(sizeof(NodeTexBlend), "NodeTexBlend");
	tex->progression = SHD_BLEND_LINEAR;
	tex->axis = SHD_BLEND_HORIZONTAL;

	node->storage = tex;
}

static void node_shader_exec_tex_blend(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	ShaderCallData *scd= (ShaderCallData*)data;
	NodeTexBlend *tex= (NodeTexBlend*)node->storage;
	bNodeSocket *vecsock = node->inputs.first;
	float vec[3];
	
	if(vecsock->link)
		nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	else
		copy_v3_v3(vec, scd->co);
	
	out[0]->vec[0]= blend(vec, tex->progression, tex->axis);
}

static int node_shader_gpu_tex_blend(GPUMaterial *mat, bNode *UNUSED(node), GPUNodeStack *in, GPUNodeStack *out)
{
	if(!in[0].link)
		in[0].link = GPU_attribute(CD_ORCO, "");

	return GPU_stack_link(mat, "node_tex_blend", in, out);
}

/* node type definition */
void register_node_type_sh_tex_blend(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, SH_NODE_TEX_BLEND, "Blend Texture", NODE_CLASS_TEXTURE, 0);
	node_type_socket_templates(&ntype, sh_node_tex_blend_in, sh_node_tex_blend_out);
	node_type_size(&ntype, 150, 60, 200);
	node_type_init(&ntype, node_shader_init_tex_blend);
	node_type_storage(&ntype, "NodeTexBlend", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_shader_exec_tex_blend);
	node_type_gpu(&ntype, node_shader_gpu_tex_blend);

	nodeRegisterType(lb, &ntype);
};

