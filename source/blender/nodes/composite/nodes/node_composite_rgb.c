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

/** \file blender/nodes/composite/nodes/node_composite_rgb.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** RGB ******************** */
static bNodeSocketTemplate cmp_node_rgb_out[]= {
	{	SOCK_RGBA, 0, "RGBA",			0.5f, 0.5f, 0.5f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_init_rgb(bNodeTree *UNUSED(ntree), bNode *node, bNodeTemplate *UNUSED(ntemp))
{
	bNodeSocket *sock= node->outputs.first;
	float *col= ((bNodeSocketValueRGBA*)sock->default_value)->value;
	/* uses the default value of the output socket, must be initialized here */
	col[0] = 0.5f;
	col[1] = 0.5f;
	col[2] = 0.5f;
	col[3] = 1.0f;
}

static void node_composit_exec_rgb(void *UNUSED(data), bNode *node, bNodeStack **UNUSED(in), bNodeStack **out)
{
	bNodeSocket *sock= node->outputs.first;
	float *col= ((bNodeSocketValueRGBA*)sock->default_value)->value;
	
	copy_v4_v4(out[0]->vec, col);
}

void register_node_type_cmp_rgb(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_RGB, "RGB", NODE_CLASS_INPUT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, NULL, cmp_node_rgb_out);
	node_type_init(&ntype, node_composit_init_rgb);
	node_type_size(&ntype, 140, 80, 140);
	node_type_exec(&ntype, node_composit_exec_rgb);

	nodeRegisterType(ttype, &ntype);
}
