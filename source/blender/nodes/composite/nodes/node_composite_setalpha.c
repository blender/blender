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

/** \file blender/nodes/composite/nodes/node_composite_setalpha.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** SET ALPHA ******************** */
static bNodeSocketTemplate cmp_node_setalpha_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 1, "Alpha",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_setalpha_out[]= {
	{	SOCK_RGBA, 0, "Image"},
	{	-1, 0, ""	}
};

static void node_composit_exec_setalpha(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: RGBA image */
	/* stack order in: col, alpha */
	
	/* input no image? then only color operation */
	if (in[0]->data==NULL && in[1]->data==NULL) {
		out[0]->vec[0] = in[0]->vec[0];
		out[0]->vec[1] = in[0]->vec[1];
		out[0]->vec[2] = in[0]->vec[2];
		out[0]->vec[3] = in[1]->vec[0];
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data?in[0]->data:in[1]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); /* allocs */
		
		if (in[1]->data==NULL && in[1]->vec[0]==1.0f) {
			/* pass on image */
			composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_copy_rgb, CB_RGBA);
		}
		else {
			/* send an compbuf or a value to set as alpha - composit2_pixel_processor handles choosing the right one */
			composit2_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, do_copy_a_rgba, CB_RGBA, CB_VAL);
		}
	
		out[0]->data= stackbuf;
	}
}

void register_node_type_cmp_setalpha(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_SETALPHA, "Set Alpha", NODE_CLASS_CONVERTOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_setalpha_in, cmp_node_setalpha_out);
	node_type_size(&ntype, 120, 40, 140);
	node_type_exec(&ntype, node_composit_exec_setalpha);

	nodeRegisterType(ttype, &ntype);
}
