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
 * Contributor(s): Matt Ebb.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_colorbalance.c
 *  \ingroup cmpnodes
 */



#include "node_composite_util.h"


/* ******************* Color Balance ********************************* */
static bNodeSocketTemplate cmp_node_colorbalance_in[]={
	{SOCK_FLOAT, 1, "Fac",	1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_FACTOR},
	{SOCK_RGBA,1,"Image", 1.0f, 1.0f, 1.0f, 1.0f},
	{-1,0,""}
};

static bNodeSocketTemplate cmp_node_colorbalance_out[]={
	{SOCK_RGBA,0,"Image"},
	{-1,0,""}
};

/* this function implements ASC-CDL according to the spec at http://www.asctech.org/
 Slope
       S = in * slope
 Offset
       O = S + offset 
         = (in * slope) + offset
 Power
     out = Clamp(O) ^ power
         = Clamp((in * slope) + offset) ^ power
 */
DO_INLINE float colorbalance_cdl(float in, float offset, float power, float slope)
{
	float x = in * slope + offset;
	
	/* prevent NaN */
	CLAMP(x, 0.0f, 1.0f);
	
	return powf(x, power);
}

/* note: lift_lgg is just 2-lift, gamma_inv is 1.0/gamma */
DO_INLINE float colorbalance_lgg(float in, float lift_lgg, float gamma_inv, float gain)
{
	/* 1:1 match with the sequencer with linear/srgb conversions, the conversion isn'tisn't pretty
	 * but best keep it this way, sice testing for durian shows a similar calculation
	 * without lin/srgb conversions gives bad results (over-saturated shadows) with colors
	 * slightly below 1.0. some correction can be done but it ends up looking bad for shadows or lighter tones - campbell */
	float x= (((linearrgb_to_srgb(in) - 1.0f) * lift_lgg) + 1.0f) * gain;

	/* prevent NaN */
	if (x < 0.f) x = 0.f;

	return powf(srgb_to_linearrgb(x), gamma_inv);
}

static void do_colorbalance_cdl(bNode *node, float* out, float *in)
{
	NodeColorBalance *n= (NodeColorBalance *)node->storage;
	
	out[0] = colorbalance_cdl(in[0], n->lift[0], n->gamma[0], n->gain[0]);
	out[1] = colorbalance_cdl(in[1], n->lift[1], n->gamma[1], n->gain[1]);
	out[2] = colorbalance_cdl(in[2], n->lift[2], n->gamma[2], n->gain[2]);
	out[3] = in[3];
}

static void do_colorbalance_cdl_fac(bNode *node, float* out, float *in, float *fac)
{
	NodeColorBalance *n= (NodeColorBalance *)node->storage;
	const float mfac= 1.0f - *fac;
	
	out[0] = mfac*in[0] + *fac * colorbalance_cdl(in[0], n->lift[0], n->gamma[0], n->gain[0]);
	out[1] = mfac*in[1] + *fac * colorbalance_cdl(in[1], n->lift[1], n->gamma[1], n->gain[1]);
	out[2] = mfac*in[2] + *fac * colorbalance_cdl(in[2], n->lift[2], n->gamma[2], n->gain[2]);
	out[3] = in[3];
}

static void do_colorbalance_lgg(bNode *node, float* out, float *in)
{
	NodeColorBalance *n= (NodeColorBalance *)node->storage;

	out[0] = colorbalance_lgg(in[0], n->lift_lgg[0], n->gamma_inv[0], n->gain[0]);
	out[1] = colorbalance_lgg(in[1], n->lift_lgg[1], n->gamma_inv[1], n->gain[1]);
	out[2] = colorbalance_lgg(in[2], n->lift_lgg[2], n->gamma_inv[2], n->gain[2]);
	out[3] = in[3];
}

static void do_colorbalance_lgg_fac(bNode *node, float* out, float *in, float *fac)
{
	NodeColorBalance *n= (NodeColorBalance *)node->storage;
	const float mfac= 1.0f - *fac;

	out[0] = mfac*in[0] + *fac * colorbalance_lgg(in[0], n->lift_lgg[0], n->gamma_inv[0], n->gain[0]);
	out[1] = mfac*in[1] + *fac * colorbalance_lgg(in[1], n->lift_lgg[1], n->gamma_inv[1], n->gain[1]);
	out[2] = mfac*in[2] + *fac * colorbalance_lgg(in[2], n->lift_lgg[2], n->gamma_inv[2], n->gain[2]);
	out[3] = in[3];
}

static void node_composit_exec_colorbalance(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *cbuf= in[1]->data;
	CompBuf *stackbuf;
	
	/* stack order input:  fac, image */
	/* stack order output: image */
	if (out[0]->hasoutput==0) return;
	
	if (in[0]->vec[0] == 0.f && in[0]->data == NULL) {
		out[0]->data = pass_on_compbuf(cbuf);
		return;
	}

	{
		NodeColorBalance *n= (NodeColorBalance *)node->storage;
		int c;

		for (c = 0; c < 3; c++) {
			n->lift_lgg[c] = 2.0f - n->lift[c];
			n->gamma_inv[c] = (n->gamma[c] != 0.0f) ? 1.0f/n->gamma[c] : 1000000.0f;
		}
	}

	if (cbuf) {
		stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); /* create output based on image input */
			
		if (node->custom1 == 0) {
			/* lift gamma gain */
			if ((in[0]->data==NULL) && (in[0]->vec[0] >= 1.f)) {
				composit1_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, do_colorbalance_lgg, CB_RGBA);
			}
			else {
				composit2_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[0]->data, in[0]->vec, do_colorbalance_lgg_fac, CB_RGBA, CB_VAL);
			}
		}
		else {
			/* offset/power/slope : ASC-CDL */
			if ((in[0]->data==NULL) && (in[0]->vec[0] >= 1.f)) {
				composit1_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, do_colorbalance_cdl, CB_RGBA);
			}
			else {
				composit2_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[0]->data, in[0]->vec, do_colorbalance_cdl_fac, CB_RGBA, CB_VAL);
			}
			
		}

		out[0]->data=stackbuf;
	}
}

static void node_composit_init_colorbalance(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeColorBalance *n= node->storage= MEM_callocN(sizeof(NodeColorBalance), "node colorbalance");

	n->lift[0] = n->lift[1] = n->lift[2] = 1.0f;
	n->gamma[0] = n->gamma[1] = n->gamma[2] = 1.0f;
	n->gain[0] = n->gain[1] = n->gain[2] = 1.0f;
}

void register_node_type_cmp_colorbalance(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_COLORBALANCE, "Color Balance", NODE_CLASS_OP_COLOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_colorbalance_in, cmp_node_colorbalance_out);
	node_type_size(&ntype, 400, 200, 400);
	node_type_init(&ntype, node_composit_init_colorbalance);
	node_type_storage(&ntype, "NodeColorBalance", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_colorbalance);

	nodeRegisterType(ttype, &ntype);
}
