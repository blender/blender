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

/** \file blender/nodes/shader/nodes/node_shader_valToRgb.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"

/* **************** VALTORGB ******************** */
static bNodeSocketTemplate sh_node_valtorgb_in[]= {
	{	SOCK_FLOAT, 1, N_("Fac"),			0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate sh_node_valtorgb_out[]= {
	{	SOCK_RGBA, 0, N_("Color")},
	{	SOCK_FLOAT, 0, N_("Alpha")},
	{	-1, 0, ""	}
};

static void node_shader_exec_valtorgb(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: fac */
	/* stack order out: col, alpha */
	
	if (node->storage) {
		float fac;
		nodestack_get_vec(&fac, SOCK_FLOAT, in[0]);

		do_colorband(node->storage, fac, out[0]->vec);
		out[1]->vec[0]= out[0]->vec[3];
	}
}

static void node_shader_init_valtorgb(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	node->storage= add_colorband(1);
}

static int gpu_shader_valtorgb(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	float *array;
	int size;

	colorband_table_RGBA(node->storage, &array, &size);
	return GPU_stack_link(mat, "valtorgb", in, out, GPU_texture(size, array));
}

void register_node_type_sh_valtorgb(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, SH_NODE_VALTORGB, "ColorRamp", NODE_CLASS_CONVERTOR, NODE_OPTIONS);
	node_type_compatibility(&ntype, NODE_OLD_SHADING|NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_valtorgb_in, sh_node_valtorgb_out);
	node_type_size(&ntype, 240, 200, 300);
	node_type_init(&ntype, node_shader_init_valtorgb);
	node_type_storage(&ntype, "ColorBand", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_shader_exec_valtorgb);
	node_type_gpu(&ntype, gpu_shader_valtorgb);

	nodeRegisterType(ttype, &ntype);
}


/* **************** RGBTOBW ******************** */
static bNodeSocketTemplate sh_node_rgbtobw_in[]= {
	{	SOCK_RGBA, 1, N_("Color"),			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate sh_node_rgbtobw_out[]= {
	{	SOCK_FLOAT, 0, N_("Val"),			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};


static void node_shader_exec_rgbtobw(void *UNUSED(data), bNode *UNUSED(node), bNodeStack **in, bNodeStack **out)
{
	/* stack order out: bw */
	/* stack order in: col */

	out[0]->vec[0] = rgb_to_bw(in[0]->vec);
}

static int gpu_shader_rgbtobw(GPUMaterial *mat, bNode *UNUSED(node), GPUNodeStack *in, GPUNodeStack *out)
{
	return GPU_stack_link(mat, "rgbtobw", in, out);
}

void register_node_type_sh_rgbtobw(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, SH_NODE_RGBTOBW, "RGB to BW", NODE_CLASS_CONVERTOR, 0);
	node_type_compatibility(&ntype, NODE_OLD_SHADING|NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_rgbtobw_in, sh_node_rgbtobw_out);
	node_type_size(&ntype, 80, 40, 120);
	node_type_exec(&ntype, node_shader_exec_rgbtobw);
	node_type_gpu(&ntype, gpu_shader_rgbtobw);

	nodeRegisterType(ttype, &ntype);
}
