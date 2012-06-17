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

/** \file blender/nodes/composite/nodes/node_composite_premulkey.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** Premul and Key Alpha Convert ******************** */

static bNodeSocketTemplate cmp_node_premulkey_in[]= {
	{	SOCK_RGBA, 1, N_("Image"),			1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_premulkey_out[]= {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

static void node_composit_exec_premulkey(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	if (out[0]->hasoutput==0)
		return;
	
	if (in[0]->data) {
		CompBuf *stackbuf, *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);

		stackbuf= dupalloc_compbuf(cbuf);
		premul_compbuf(stackbuf, node->custom1 == 1);

		out[0]->data = stackbuf;
		if (cbuf != in[0]->data)
			free_compbuf(cbuf);
	}
}

void register_node_type_cmp_premulkey(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_PREMULKEY, "Alpha Convert", NODE_CLASS_CONVERTOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_premulkey_in, cmp_node_premulkey_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_exec(&ntype, node_composit_exec_premulkey);

	nodeRegisterType(ttype, &ntype);
}
