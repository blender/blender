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

/** \file blender/nodes/composite/nodes/node_composite_translate.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** Translate  ******************** */

static bNodeSocketTemplate cmp_node_translate_in[]= {
	{	SOCK_RGBA, 1, "Image",			1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_FLOAT, 1, "X",	0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f, PROP_NONE},
	{	SOCK_FLOAT, 1, "Y",	0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f, PROP_NONE},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_translate_out[]= {
	{	SOCK_RGBA, 0, "Image"},
	{	-1, 0, ""	}
};

static void node_composit_exec_translate(void *UNUSED(data), bNode *UNUSED(node), bNodeStack **in, bNodeStack **out)
{
	if (in[0]->data) {
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= pass_on_compbuf(cbuf);
	
		stackbuf->xof+= (int)floor(in[1]->vec[0]);
		stackbuf->yof+= (int)floor(in[2]->vec[0]);
		
		out[0]->data= stackbuf;
	}
}

void register_node_type_cmp_translate(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_TRANSLATE, "Translate", NODE_CLASS_DISTORT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_translate_in, cmp_node_translate_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_exec(&ntype, node_composit_exec_translate);

	nodeRegisterType(ttype, &ntype);
}
