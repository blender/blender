/*
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

/** \file blender/nodes/shader/nodes/node_shader_curves.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"


/* **************** CURVE VEC  ******************** */
static bNodeSocketTemplate sh_node_curve_vec_in[]= {
	{	SOCK_FLOAT, 0, "Fac",	1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_VECTOR, 1, "Vector",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_curve_vec_out[]= {
	{	SOCK_VECTOR, 0, "Vector"},
	{	-1, 0, ""	}
};

static void node_shader_exec_curve_vec(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	float vec[3];
	
	/* stack order input:  vec */
	/* stack order output: vec */
	nodestack_get_vec(vec, SOCK_VECTOR, in[1]);
	curvemapping_evaluate3F(node->storage, out[0]->vec, vec);
}

static void node_shader_init_curve_vec(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	node->storage= curvemapping_add(3, -1.0f, -1.0f, 1.0f, 1.0f);
}

static int gpu_shader_curve_vec(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	float *array;
	int size;

	curvemapping_table_RGBA(node->storage, &array, &size);
	return GPU_stack_link(mat, "curves_vec", in, out, GPU_texture(size, array));
}

void register_node_type_sh_curve_vec(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, SH_NODE_CURVE_VEC, "Vector Curves", NODE_CLASS_OP_VECTOR, NODE_OPTIONS);
	node_type_compatibility(&ntype, NODE_OLD_SHADING);
	node_type_socket_templates(&ntype, sh_node_curve_vec_in, sh_node_curve_vec_out);
	node_type_size(&ntype, 200, 140, 320);
	node_type_init(&ntype, node_shader_init_curve_vec);
	node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);
	node_type_exec(&ntype, node_shader_exec_curve_vec);
	node_type_gpu(&ntype, gpu_shader_curve_vec);

	nodeRegisterType(ttype, &ntype);
}


/* **************** CURVE RGB  ******************** */
static bNodeSocketTemplate sh_node_curve_rgb_in[]= {
	{	SOCK_FLOAT, 1, "Fac",	1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_FACTOR},
	{	SOCK_RGBA, 1, "Color",	0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_curve_rgb_out[]= {
	{	SOCK_RGBA, 0, "Color"},
	{	-1, 0, ""	}
};

static void node_shader_exec_curve_rgb(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	float vec[3];
	
	/* stack order input:  vec */
	/* stack order output: vec */
	nodestack_get_vec(vec, SOCK_VECTOR, in[1]);
	curvemapping_evaluateRGBF(node->storage, out[0]->vec, vec);
	if (in[0]->vec[0] != 1.0f) {
		interp_v3_v3v3(out[0]->vec, vec, out[0]->vec, *in[0]->vec);
	}
}

static void node_shader_init_curve_rgb(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	node->storage= curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
}

static int gpu_shader_curve_rgb(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	float *array;
	int size;
	curvemapping_table_RGBA(node->storage, &array, &size);
	return GPU_stack_link(mat, "curves_rgb", in, out, GPU_texture(size, array));
}

void register_node_type_sh_curve_rgb(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, SH_NODE_CURVE_RGB, "RGB Curves", NODE_CLASS_OP_COLOR, NODE_OPTIONS);
	node_type_compatibility(&ntype, NODE_OLD_SHADING);
	node_type_socket_templates(&ntype, sh_node_curve_rgb_in, sh_node_curve_rgb_out);
	node_type_size(&ntype, 200, 140, 320);
	node_type_init(&ntype, node_shader_init_curve_rgb);
	node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);
	node_type_exec(&ntype, node_shader_exec_curve_rgb);
	node_type_gpu(&ntype, gpu_shader_curve_rgb);

	nodeRegisterType(ttype, &ntype);
}
