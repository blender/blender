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

/** \file blender/nodes/composite/nodes/node_composite_sepcombHSVA.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** SEPARATE HSVA ******************** */
static bNodeSocketTemplate cmp_node_sephsva_in[]= {
	{	SOCK_RGBA, 1, "Image",			1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_sephsva_out[]= {
	{	SOCK_FLOAT, 0, "H"},
	{	SOCK_FLOAT, 0, "S"},
	{	SOCK_FLOAT, 0, "V"},
	{	SOCK_FLOAT, 0, "A"},
	{	-1, 0, ""	}
};

static void do_sephsva(bNode *UNUSED(node), float *out, float *in)
{
	float h, s, v;
	
	rgb_to_hsv(in[0], in[1], in[2], &h, &s, &v);
	
	out[0]= h;
	out[1]= s;
	out[2]= v;
	out[3]= in[3];
}

static void node_composit_exec_sephsva(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: bw channels */
	/* stack order in: col */

	/* input no image? then only color operation */
	if(in[0]->data==NULL) {
		float h, s, v;
	
	rgb_to_hsv(in[0]->vec[0], in[0]->vec[1], in[0]->vec[2], &h, &s, &v);
		
		out[0]->vec[0] = h;
		out[1]->vec[0] = s;
		out[2]->vec[0] = v;
		out[3]->vec[0] = in[0]->vec[3];
	}
	else if ((out[0]->hasoutput) || (out[1]->hasoutput) || (out[2]->hasoutput) || (out[3]->hasoutput)) {
		/* create new buffer so input buffer doesn't get corrupted */
		CompBuf *cbuf= dupalloc_compbuf(in[0]->data);
		CompBuf *cbuf2= typecheck_compbuf(cbuf, CB_RGBA);

		/* convert the RGB stackbuf to an HSV representation */
		composit1_pixel_processor(node, cbuf2, cbuf2, in[0]->vec, do_sephsva, CB_RGBA);

		/* separate each of those channels */
		if(out[0]->hasoutput)
			out[0]->data= valbuf_from_rgbabuf(cbuf2, CHAN_R);
		if(out[1]->hasoutput)
			out[1]->data= valbuf_from_rgbabuf(cbuf2, CHAN_G);
		if(out[2]->hasoutput)
			out[2]->data= valbuf_from_rgbabuf(cbuf2, CHAN_B);
		if(out[3]->hasoutput)
			out[3]->data= valbuf_from_rgbabuf(cbuf2, CHAN_A);

		/*not used anymore */
		if(cbuf2!=cbuf)
			free_compbuf(cbuf2);
		free_compbuf(cbuf);	
	}
}

void register_node_type_cmp_sephsva(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_SEPHSVA, "Separate HSVA", NODE_CLASS_CONVERTOR, 0);
	node_type_socket_templates(&ntype, cmp_node_sephsva_in, cmp_node_sephsva_out);
	node_type_size(&ntype, 80, 40, 140);
	node_type_exec(&ntype, node_composit_exec_sephsva);

	nodeRegisterType(ttype, &ntype);
}


/* **************** COMBINE HSVA ******************** */
static bNodeSocketTemplate cmp_node_combhsva_in[]= {
   {	SOCK_FLOAT, 1, "H",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
   {	SOCK_FLOAT, 1, "S",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
   {	SOCK_FLOAT, 1, "V",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
   {	SOCK_FLOAT, 1, "A",			1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
   {	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_combhsva_out[]= {
   {	SOCK_RGBA, 0, "Image"},
   {	-1, 0, ""	}
};

static void do_comb_hsva(bNode *UNUSED(node), float *out, float *in1, float *in2, float *in3, float *in4)
{
	float r,g,b;
	hsv_to_rgb(in1[0], in2[0], in3[0], &r, &g, &b);

	out[0] = r;
	out[1] = g;
	out[2] = b;
	out[3] = in4[0];
}

static void node_composit_exec_combhsva(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: 1 rgba channels */
	/* stack order in: 4 value channels */

	/* input no image? then only color operation in HSV */
	if((in[0]->data==NULL) && (in[1]->data==NULL) && (in[2]->data==NULL) && (in[3]->data==NULL)) {
		hsv_to_rgb(in[0]->vec[0], in[1]->vec[0], in[2]->vec[0],
		&out[0]->vec[0], &out[0]->vec[1], &out[0]->vec[2]);
		out[0]->vec[3] = in[3]->vec[0];
	}
	else {
		/* make output size of first available input image */
		CompBuf *cbuf;
		CompBuf *stackbuf;

		/* allocate a CompBuf the size of the first available input */
		if (in[0]->data) cbuf = in[0]->data;
		else if (in[1]->data) cbuf = in[1]->data;
		else if (in[2]->data) cbuf = in[2]->data;
		else cbuf = in[3]->data;

		stackbuf = alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); /* allocs */

		composit4_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec,
		in[2]->data, in[2]->vec, in[3]->data, in[3]->vec,
		do_comb_hsva, CB_VAL, CB_VAL, CB_VAL, CB_VAL);

		out[0]->data= stackbuf;
	}
}

void register_node_type_cmp_combhsva(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_COMBHSVA, "Combine HSVA", NODE_CLASS_CONVERTOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_combhsva_in, cmp_node_combhsva_out);
	node_type_size(&ntype, 80, 40, 140);
	node_type_exec(&ntype, node_composit_exec_combhsva);

	nodeRegisterType(ttype, &ntype);
}
