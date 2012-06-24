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
static bNodeSocketTemplate cmp_node_valtorgb_in[]= {
	{	SOCK_FLOAT, 1, N_("Fac"),			0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_valtorgb_out[]= {
	{	SOCK_RGBA, 0, N_("Image")},
	{	SOCK_FLOAT, 0, N_("Alpha")},
	{	-1, 0, ""	}
};

static void do_colorband_composit(bNode *node, float *out, float *in)
{
	do_colorband(node->storage, in[0], out);
}

static void node_composit_exec_valtorgb(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: fac */
	/* stack order out: col, alpha */
	
	if (out[0]->hasoutput==0 && out[1]->hasoutput==0) 
		return;
	
	if (node->storage) {
		/* input no image? then only color operation */
		if (in[0]->data==NULL) {
			do_colorband(node->storage, in[0]->vec[0], out[0]->vec);
		}
		else {
			/* make output size of input image */
			CompBuf *cbuf= in[0]->data;
			CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); /* allocs */
			
			composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_colorband_composit, CB_VAL);
			
			out[0]->data= stackbuf;
			
			if (out[1]->hasoutput)
				out[1]->data= valbuf_from_rgbabuf(stackbuf, CHAN_A);

		}
	}
}

static void node_composit_init_valtorgb(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	node->storage= add_colorband(1);
}

void register_node_type_cmp_valtorgb(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_VALTORGB, "ColorRamp", NODE_CLASS_CONVERTOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_valtorgb_in, cmp_node_valtorgb_out);
	node_type_size(&ntype, 240, 200, 300);
	node_type_init(&ntype, node_composit_init_valtorgb);
	node_type_storage(&ntype, "ColorBand", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_valtorgb);

	nodeRegisterType(ttype, &ntype);
}



/* **************** RGBTOBW ******************** */
static bNodeSocketTemplate cmp_node_rgbtobw_in[]= {
	{	SOCK_RGBA, 1, N_("Image"),			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_rgbtobw_out[]= {
	{	SOCK_FLOAT, 0, N_("Val"),			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_rgbtobw(bNode *UNUSED(node), float *out, float *in)
{
	out[0] = rgb_to_bw(in);
}

static void node_composit_exec_rgbtobw(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: bw */
	/* stack order in: col */
	
	if (out[0]->hasoutput==0)
		return;
	
	/* input no image? then only color operation */
	if (in[0]->data==NULL) {
		do_rgbtobw(node, out[0]->vec, in[0]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1); /* allocs */
		
		composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_rgbtobw, CB_RGBA);
		
		out[0]->data= stackbuf;
	}
}

void register_node_type_cmp_rgbtobw(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, CMP_NODE_RGBTOBW, "RGB to BW", NODE_CLASS_CONVERTOR, 0);
	node_type_socket_templates(&ntype, cmp_node_rgbtobw_in, cmp_node_rgbtobw_out);
	node_type_size(&ntype, 80, 40, 120);
	node_type_exec(&ntype, node_composit_exec_rgbtobw);
	
	nodeRegisterType(ttype, &ntype);
}
