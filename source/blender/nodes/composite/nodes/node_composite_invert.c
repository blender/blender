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

/** \file blender/nodes/composite/nodes/node_composite_invert.c
 *  \ingroup cmpnodes
 */

#include "node_composite_util.h"

/* **************** INVERT ******************** */
static bNodeSocketTemplate cmp_node_invert_in[]= { 
	{ SOCK_FLOAT, 1, "Fac", 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR}, 
	{ SOCK_RGBA, 1, "Color", 1.0f, 1.0f, 1.0f, 1.0f}, 
	{ -1, 0, "" } 
};

static bNodeSocketTemplate cmp_node_invert_out[]= { 
	{ SOCK_RGBA, 0, "Color"}, 
	{ -1, 0, "" } 
};

static void do_invert(bNode *node, float *out, float *in)
{
	if (node->custom1 & CMP_CHAN_RGB) {
		out[0] = 1.0f - in[0];
		out[1] = 1.0f - in[1];
		out[2] = 1.0f - in[2];
	}
	else {
		copy_v3_v3(out, in);
	}
		
	if (node->custom1 & CMP_CHAN_A)
		out[3] = 1.0f - in[3];
	else
		out[3] = in[3];
}

static void do_invert_fac(bNode *node, float *out, float *in, float *fac)
{
	float col[4], facm;

	do_invert(node, col, in);

	/* blend inverted result against original input with fac */
	facm = 1.0f - fac[0];

	if (node->custom1 & CMP_CHAN_RGB) {
		col[0] = fac[0]*col[0] + (facm*in[0]);
		col[1] = fac[0]*col[1] + (facm*in[1]);
		col[2] = fac[0]*col[2] + (facm*in[2]);
	}
	if (node->custom1 & CMP_CHAN_A)
		col[3] = fac[0]*col[3] + (facm*in[3]);
	
	copy_v4_v4(out, col);
}

static void node_composit_exec_invert(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: fac, Image, Image */
	/* stack order out: Image */
	float *fac= in[0]->vec;
	
	if (out[0]->hasoutput==0) return;
	
	/* input no image? then only color operation */
	if (in[1]->data==NULL && in[0]->data==NULL) {
		do_invert_fac(node, out[0]->vec, in[1]->vec, fac);
	}
	else {
		/* make output size of first available input image, or then size of fac */
		CompBuf *cbuf= in[1]->data?in[1]->data:in[0]->data;

		/* if neither RGB or A toggled on, pass through */
		if (node->custom1 != 0) {
			CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); /* allocs */
			
			if (fac[0] < 1.0f || in[0]->data!=NULL)
				composit2_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[0]->data, fac, do_invert_fac, CB_RGBA, CB_VAL);
			else
				composit1_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, do_invert, CB_RGBA);
			out[0]->data= stackbuf;
			return;
			
		}
		else {
			out[0]->data = pass_on_compbuf(cbuf);
			return;
		}
	}
}

static void node_composit_init_invert(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	node->custom1 |= CMP_CHAN_RGB;
}

/* custom1 = mix type */
void register_node_type_cmp_invert(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_INVERT, "Invert", NODE_CLASS_OP_COLOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_invert_in, cmp_node_invert_out);
	node_type_size(&ntype, 120, 120, 140);
	node_type_init(&ntype, node_composit_init_invert);
	node_type_exec(&ntype, node_composit_exec_invert);

	nodeRegisterType(ttype, &ntype);
}
