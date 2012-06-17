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

/** \file blender/nodes/shader/nodes/node_shader_rgb.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"

/* **************** RGB ******************** */
static bNodeSocketTemplate sh_node_rgb_out[]= {
	{	SOCK_RGBA, 0, N_("Color")},
	{	-1, 0, ""	}
};

static void node_shader_init_rgb(bNodeTree *UNUSED(ntree), bNode *node, bNodeTemplate *UNUSED(ntemp))
{
	bNodeSocket *sock= node->outputs.first;
	bNodeSocketValueRGBA *dval= (bNodeSocketValueRGBA*)sock->default_value;
	/* uses the default value of the output socket, must be initialized here */
	dval->value[0] = 0.5f;
	dval->value[1] = 0.5f;
	dval->value[2] = 0.5f;
	dval->value[3] = 1.0f;
}

static void node_shader_exec_rgb(void *UNUSED(data), bNode *node, bNodeStack **UNUSED(in), bNodeStack **out)
{
	bNodeSocket *sock= node->outputs.first;
	float *col= ((bNodeSocketValueRGBA*)sock->default_value)->value;
	
	copy_v3_v3(out[0]->vec, col);
}

static int gpu_shader_rgb(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	bNodeSocket *sock= node->outputs.first;
	float *col= ((bNodeSocketValueRGBA*)sock->default_value)->value;
	GPUNodeLink *vec = GPU_uniform(col);

	return GPU_stack_link(mat, "set_rgba", in, out, vec);
}

void register_node_type_sh_rgb(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, SH_NODE_RGB, "RGB", NODE_CLASS_INPUT, NODE_OPTIONS);
	node_type_compatibility(&ntype, NODE_OLD_SHADING|NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, NULL, sh_node_rgb_out);
	node_type_init(&ntype, node_shader_init_rgb);
	node_type_size(&ntype, 140, 80, 140);
	node_type_exec(&ntype, node_shader_exec_rgb);
	node_type_gpu(&ntype, gpu_shader_rgb);

	nodeRegisterType(ttype, &ntype);
}
