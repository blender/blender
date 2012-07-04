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
 * Contributor(s): Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/texture/nodes/node_texture_valToRgb.c
 *  \ingroup texnodes
 */


#include "node_texture_util.h"
#include "NOD_texture.h"

/* **************** VALTORGB ******************** */
static bNodeSocketTemplate valtorgb_in[]= {
	{	SOCK_FLOAT, 1, N_("Fac"),			0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate valtorgb_out[]= {
	{	SOCK_RGBA, 0, N_("Color")},
	{	-1, 0, ""	}
};

static void valtorgb_colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	if (node->storage) {
		float fac = tex_input_value(in[0], p, thread);

		do_colorband(node->storage, fac, out);
	}
}

static void valtorgb_exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out) 
{
	tex_output(node, in, out[0], &valtorgb_colorfn, data);
}

static void valtorgb_init(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	node->storage = add_colorband(1);
}

void register_node_type_tex_valtorgb(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, TEX_NODE_VALTORGB, "ColorRamp", NODE_CLASS_CONVERTOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, valtorgb_in, valtorgb_out);
	node_type_size(&ntype, 240, 200, 300);
	node_type_init(&ntype, valtorgb_init);
	node_type_storage(&ntype, "ColorBand", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, valtorgb_exec);
	
	nodeRegisterType(ttype, &ntype);
}

/* **************** RGBTOBW ******************** */
static bNodeSocketTemplate rgbtobw_in[]= {
	{	SOCK_RGBA, 1, N_("Color"),			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate rgbtobw_out[]= {
	{	SOCK_FLOAT, 0, N_("Val"),			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};


static void rgbtobw_valuefn(float *out, TexParams *p, bNode *UNUSED(node), bNodeStack **in, short thread)
{
	float cin[4];
	tex_input_rgba(cin, in[0], p, thread);
	*out = rgb_to_bw(cin);
}

static void rgbtobw_exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &rgbtobw_valuefn, data);
}

void register_node_type_tex_rgbtobw(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, TEX_NODE_RGBTOBW, "RGB to BW", NODE_CLASS_CONVERTOR, 0);
	node_type_socket_templates(&ntype, rgbtobw_in, rgbtobw_out);
	node_type_size(&ntype, 80, 40, 120);
	node_type_exec(&ntype, rgbtobw_exec);
	
	nodeRegisterType(ttype, &ntype);
}
