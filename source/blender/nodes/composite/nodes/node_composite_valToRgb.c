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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_valToRgb.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** VALTORGB ******************** */
static bNodeSocketTemplate cmp_node_valtorgb_in[] = {
	{	SOCK_FLOAT, 1, N_("Fac"),			0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_valtorgb_out[] = {
	{	SOCK_RGBA, 0, N_("Image")},
	{	SOCK_FLOAT, 0, N_("Alpha")},
	{	-1, 0, ""	}
};

static void node_composit_init_valtorgb(bNodeTree *UNUSED(ntree), bNode *node)
{
	node->storage = add_colorband(true);
}

void register_node_type_cmp_valtorgb(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_VALTORGB, "ColorRamp", NODE_CLASS_CONVERTOR, 0);
	node_type_socket_templates(&ntype, cmp_node_valtorgb_in, cmp_node_valtorgb_out);
	node_type_size(&ntype, 240, 200, 320);
	node_type_init(&ntype, node_composit_init_valtorgb);
	node_type_storage(&ntype, "ColorBand", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(&ntype);
}



/* **************** RGBTOBW ******************** */
static bNodeSocketTemplate cmp_node_rgbtobw_in[] = {
	{	SOCK_RGBA, 1, N_("Image"),			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_rgbtobw_out[] = {
	{	SOCK_FLOAT, 0, N_("Val"),			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

void register_node_type_cmp_rgbtobw(void)
{
	static bNodeType ntype;
	
	cmp_node_type_base(&ntype, CMP_NODE_RGBTOBW, "RGB to BW", NODE_CLASS_CONVERTOR, 0);
	node_type_socket_templates(&ntype, cmp_node_rgbtobw_in, cmp_node_rgbtobw_out);
	node_type_size_preset(&ntype, NODE_SIZE_SMALL);
	
	nodeRegisterType(&ntype);
}
