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

/** \file blender/nodes/composite/nodes/node_composite_gamma.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** Gamma Tools  ******************** */
  
static bNodeSocketTemplate cmp_node_gamma_in[]= {
	{	SOCK_RGBA, 1, "Image",			1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_FLOAT, 1, "Gamma",			1.0f, 0.0f, 0.0f, 0.0f, 0.001f, 10.0f, PROP_UNSIGNED},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_gamma_out[]= {
	{	SOCK_RGBA, 0, "Image"},
	{	-1, 0, ""	}
};

static void do_gamma(bNode *UNUSED(node), float *out, float *in, float *fac)
{
	int i=0;
	for(i=0; i<3; i++) {
		/* check for negative to avoid nan's */
		out[i] = (in[i] > 0.0f)? powf(in[i],fac[0]): in[i];
	}
	out[3] = in[3];
}
static void node_composit_exec_gamma(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: Fac, Image */
	/* stack order out: Image */
	if(out[0]->hasoutput==0) return;
	
	/* input no image? then only color operation */
	if(in[0]->data==NULL) {
		do_gamma(node, out[0]->vec, in[0]->vec, in[1]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
		
		composit2_pixel_processor(node, stackbuf, cbuf, in[0]->vec, in[1]->data, in[1]->vec, do_gamma, CB_RGBA, CB_VAL);

		out[0]->data= stackbuf;
	}
}

void register_node_type_cmp_gamma(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, CMP_NODE_GAMMA, "Gamma", NODE_CLASS_OP_COLOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_gamma_in, cmp_node_gamma_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_exec(&ntype, node_composit_exec_gamma);
	
	nodeRegisterType(ttype, &ntype);
}
