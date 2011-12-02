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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/shader/nodes/node_shader_sepcombRGB.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"

/* **************** SEPARATE RGBA ******************** */
static bNodeSocketTemplate sh_node_seprgb_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate sh_node_seprgb_out[]= {
	{	SOCK_FLOAT, 0, "R"},
	{	SOCK_FLOAT, 0, "G"},
	{	SOCK_FLOAT, 0, "B"},
	{	-1, 0, ""	}
};

static void node_shader_exec_seprgb(void *UNUSED(data), bNode *UNUSED(node), bNodeStack **in, bNodeStack **out)
{
	out[0]->vec[0] = in[0]->vec[0];
	out[1]->vec[0] = in[0]->vec[1];
	out[2]->vec[0] = in[0]->vec[2];
}

static int gpu_shader_seprgb(GPUMaterial *mat, bNode *UNUSED(node), GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, "separate_rgb", in, out);
}

void register_node_type_sh_seprgb(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, SH_NODE_SEPRGB, "Separate RGB", NODE_CLASS_CONVERTOR, 0);
	node_type_compatibility(&ntype, NODE_OLD_SHADING|NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_seprgb_in, sh_node_seprgb_out);
	node_type_size(&ntype, 80, 40, 140);
	node_type_exec(&ntype, node_shader_exec_seprgb);
	node_type_gpu(&ntype, gpu_shader_seprgb);

	nodeRegisterType(ttype, &ntype);
}



/* **************** COMBINE RGB ******************** */
static bNodeSocketTemplate sh_node_combrgb_in[]= {
	{	SOCK_FLOAT, 1, "R",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_UNSIGNED},
	{	SOCK_FLOAT, 1, "G",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_UNSIGNED},
	{	SOCK_FLOAT, 1, "B",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_UNSIGNED},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate sh_node_combrgb_out[]= {
	{	SOCK_RGBA, 0, "Image"},
	{	-1, 0, ""	}
};

static void node_shader_exec_combrgb(void *UNUSED(data), bNode *UNUSED(node), bNodeStack **in, bNodeStack **out)
{
	out[0]->vec[0] = in[0]->vec[0];
	out[0]->vec[1] = in[1]->vec[0];
	out[0]->vec[2] = in[2]->vec[0];
}

static int gpu_shader_combrgb(GPUMaterial *mat, bNode *UNUSED(node), GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, "combine_rgb", in, out);
}

void register_node_type_sh_combrgb(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, SH_NODE_COMBRGB, "Combine RGB", NODE_CLASS_CONVERTOR, NODE_OPTIONS);
	node_type_compatibility(&ntype, NODE_OLD_SHADING|NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_combrgb_in, sh_node_combrgb_out);
	node_type_size(&ntype, 80, 40, 140);
	node_type_exec(&ntype, node_shader_exec_combrgb);
	node_type_gpu(&ntype, gpu_shader_combrgb);

	nodeRegisterType(ttype, &ntype);
}
