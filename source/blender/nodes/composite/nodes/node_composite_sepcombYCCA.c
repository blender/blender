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

/** \file blender/nodes/composite/nodes/node_composite_sepcombYCCA.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** SEPARATE YCCA ******************** */
static bNodeSocketTemplate cmp_node_sepycca_in[]= {
	{  SOCK_RGBA, 1, N_("Image"),        1.0f, 1.0f, 1.0f, 1.0f},
	{  -1, 0, ""   }
};
static bNodeSocketTemplate cmp_node_sepycca_out[]= {
	{  SOCK_FLOAT, 0, N_("Y")},
	{  SOCK_FLOAT, 0, N_("Cb")},
	{  SOCK_FLOAT, 0, N_("Cr")},
	{  SOCK_FLOAT, 0, N_("A")},
	{  -1, 0, ""   }
};

static void do_sepycca_601(bNode *UNUSED(node), float *out, float *in)
{
	float y, cb, cr;
	
	rgb_to_ycc(in[0], in[1], in[2], &y, &cb, &cr, BLI_YCC_ITU_BT601);
	
	/*divided by 255 to normalize for viewing in */
	out[0]=  y/255.0f;
	out[1]= cb/255.0f;
	out[2]= cr/255.0f;
	out[3]= in[3];
}

static void do_sepycca_709(bNode *UNUSED(node), float *out, float *in)
{
	float y, cb, cr;
	
	rgb_to_ycc(in[0], in[1], in[2], &y, &cb, &cr, BLI_YCC_ITU_BT709);
	
	/*divided by 255 to normalize for viewing in */
	out[0]=  y/255.0f;
	out[1]= cb/255.0f;
	out[2]= cr/255.0f;
	out[3]= in[3];
}

static void do_sepycca_jfif(bNode *UNUSED(node), float *out, float *in)
{
	float y, cb, cr;
	
	rgb_to_ycc(in[0], in[1], in[2], &y, &cb, &cr, BLI_YCC_JFIF_0_255);
	
	/*divided by 255 to normalize for viewing in */
	out[0]=  y/255.0f;
	out[1]= cb/255.0f;
	out[2]= cr/255.0f;
	out[3]= in[3];
}

static void node_composit_exec_sepycca(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* input no image? then only color operation */
	if (in[0]->data==NULL) {
		float y, cb, cr;
	
		switch (node->custom1) {
		case 1:
			rgb_to_ycc(in[0]->vec[0], in[0]->vec[1], in[0]->vec[2], &y, &cb, &cr, BLI_YCC_ITU_BT709);
			break;
		case 2:
			rgb_to_ycc(in[0]->vec[0], in[0]->vec[1], in[0]->vec[2], &y, &cb, &cr, BLI_YCC_JFIF_0_255);
			break;
		case 0:
		default:
			rgb_to_ycc(in[0]->vec[0], in[0]->vec[1], in[0]->vec[2], &y, &cb, &cr, BLI_YCC_ITU_BT601);
			break;
		}
	
		/*divided by 255 to normalize for viewing in */
		out[0]->vec[0] =  y/255.0f;
		out[1]->vec[0] = cb/255.0f;
		out[2]->vec[0] = cr/255.0f;
		out[3]->vec[0] = in[0]->vec[3];
	}
	else if ((out[0]->hasoutput) || (out[1]->hasoutput) || (out[2]->hasoutput) || (out[3]->hasoutput)) {
		/* make copy of buffer so input buffer doesn't get corrupted */
		CompBuf *cbuf= dupalloc_compbuf(in[0]->data);
		CompBuf *cbuf2=typecheck_compbuf(cbuf, CB_RGBA);
	
		/* convert the RGB stackbuf to an HSV representation */
		switch (node->custom1) {
		case 1:
			composit1_pixel_processor(node, cbuf2, cbuf2, in[0]->vec, do_sepycca_709, CB_RGBA);
			break;
		case 2:
			composit1_pixel_processor(node, cbuf2, cbuf2, in[0]->vec, do_sepycca_jfif, CB_RGBA);
			break;
		case 0:
		default:
			composit1_pixel_processor(node, cbuf2, cbuf2, in[0]->vec, do_sepycca_601, CB_RGBA);
			break;
		}
	
		/* separate each of those channels */
		if (out[0]->hasoutput)
			out[0]->data= valbuf_from_rgbabuf(cbuf2, CHAN_R);
		if (out[1]->hasoutput)
			out[1]->data= valbuf_from_rgbabuf(cbuf2, CHAN_G);
		if (out[2]->hasoutput)
			out[2]->data= valbuf_from_rgbabuf(cbuf2, CHAN_B);
		if (out[3]->hasoutput)
			out[3]->data= valbuf_from_rgbabuf(cbuf2, CHAN_A);

		/*not used anymore */
		if (cbuf2!=cbuf)
			free_compbuf(cbuf2);
		free_compbuf(cbuf);
	}
}

void register_node_type_cmp_sepycca(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_SEPYCCA, "Separate YCbCrA", NODE_CLASS_CONVERTOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_sepycca_in, cmp_node_sepycca_out);
	node_type_size(&ntype, 80, 40, 140);
	node_type_exec(&ntype, node_composit_exec_sepycca);

	nodeRegisterType(ttype, &ntype);
}



/* **************** COMBINE YCCA ******************** */
static bNodeSocketTemplate cmp_node_combycca_in[]= {
	{	SOCK_FLOAT, 1, N_("Y"),			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{	SOCK_FLOAT, 1, N_("Cb"),			0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{	SOCK_FLOAT, 1, N_("Cr"),			0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{	SOCK_FLOAT, 1, N_("A"),			1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_combycca_out[]= {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

static void do_comb_ycca_601(bNode *UNUSED(node), float *out, float *in1, float *in2, float *in3, float *in4)
{
	float r, g, b;
	float y, cb, cr;

	/*need to un-normalize the data*/
	y=in1[0]*255;
	cb=in2[0]*255;
	cr=in3[0]*255;

	ycc_to_rgb(y, cb, cr, &r, &g, &b, BLI_YCC_ITU_BT601);
	
	out[0] = r;
	out[1] = g;
	out[2] = b;
	out[3] = in4[0];
}

static void do_comb_ycca_709(bNode *UNUSED(node), float *out, float *in1, float *in2, float *in3, float *in4)
{
	float r, g, b;
	float y, cb, cr;

	/*need to un-normalize the data*/
	y=in1[0]*255;
	cb=in2[0]*255;
	cr=in3[0]*255;

	ycc_to_rgb(y, cb, cr, &r, &g, &b, BLI_YCC_ITU_BT709);
	
	out[0] = r;
	out[1] = g;
	out[2] = b;
	out[3] = in4[0];
}

static void do_comb_ycca_jfif(bNode *UNUSED(node), float *out, float *in1, float *in2, float *in3, float *in4)
{
	float r, g, b;
	float y, cb, cr;

	/*need to un-normalize the data*/
	y=in1[0]*255;
	cb=in2[0]*255;
	cr=in3[0]*255;

	ycc_to_rgb(y, cb, cr, &r, &g, &b, BLI_YCC_JFIF_0_255);
	
	out[0] = r;
	out[1] = g;
	out[2] = b;
	out[3] = in4[0];
}

static void node_composit_exec_combycca(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: 1 ycca channels */
	/* stack order in: 4 value channels */
	
	/* input no image? then only color operation */
	if ((in[0]->data==NULL) && (in[1]->data==NULL) && (in[2]->data==NULL) && (in[3]->data==NULL)) {
		float y = in[0]->vec[0] * 255;
		float cb = in[1]->vec[0] * 255;
		float cr = in[2]->vec[0] * 255;
		
		switch (node->custom1) {
		case 1:
			ycc_to_rgb(y, cb, cr, &out[0]->vec[0], &out[0]->vec[1], &out[0]->vec[2], BLI_YCC_ITU_BT709);
			break;
		case 2:
			ycc_to_rgb(y, cb, cr, &out[0]->vec[0], &out[0]->vec[1], &out[0]->vec[2], BLI_YCC_JFIF_0_255);
			break;
		case 0:
		default:
			ycc_to_rgb(y, cb, cr, &out[0]->vec[0], &out[0]->vec[1], &out[0]->vec[2], BLI_YCC_ITU_BT601);
			break;
		}
		
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
		
		
		switch (node->custom1) {
		case 1:
			composit4_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, 
			                          in[2]->data, in[2]->vec, in[3]->data, in[3]->vec, 
			                          do_comb_ycca_709, CB_VAL, CB_VAL, CB_VAL, CB_VAL);
			break;
		
		case 2:
			composit4_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, 
			                          in[2]->data, in[2]->vec, in[3]->data, in[3]->vec, 
			                          do_comb_ycca_jfif, CB_VAL, CB_VAL, CB_VAL, CB_VAL);
			break;
		case 0:
		default:
			composit4_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, 
			                          in[2]->data, in[2]->vec, in[3]->data, in[3]->vec, 
			                          do_comb_ycca_601, CB_VAL, CB_VAL, CB_VAL, CB_VAL);
			break;
		}

		out[0]->data= stackbuf;
	}	
}

void register_node_type_cmp_combycca(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_COMBYCCA, "Combine YCbCrA", NODE_CLASS_CONVERTOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_combycca_in, cmp_node_combycca_out);
	node_type_size(&ntype, 80, 40, 140);
	node_type_exec(&ntype, node_composit_exec_combycca);

	nodeRegisterType(ttype, &ntype);
}
