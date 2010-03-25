/**
 * 
 *
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


#include "../CMP_util.h"


/* ******************* Color Balance ********************************* */
static bNodeSocketType cmp_node_colorbalance_in[]={
	{SOCK_VALUE, 1, "Fac",	1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{SOCK_RGBA,1,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static bNodeSocketType cmp_node_colorbalance_out[]={
	{SOCK_RGBA,0,"Image", 0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{-1,0,""}
};

DO_INLINE float colorbalance_cdl(float in, float offset, float power, float slope)
{
	float x = in * slope + offset;
	
	/* prevent NaN */
	if (x < 0.f) x = 0.f;
	
	//powf(in * slope + offset, power)
	return powf(x, 1.f/power);
}

DO_INLINE float colorbalance_lgg(float in, float lift, float gamma, float gain)
{
	float x = gain*(in+(lift-1)*(1-in));
	
	/* prevent NaN */
	if (x < 0.f) x = 0.f;
	
	return powf(x, (1.f/gamma));
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
	
	out[0] = colorbalance_lgg(in[0], n->lift[0], n->gamma[0], n->gain[0]);
	out[1] = colorbalance_lgg(in[1], n->lift[1], n->gamma[1], n->gain[1]);
	out[2] = colorbalance_lgg(in[2], n->lift[2], n->gamma[2], n->gain[2]);
	out[3] = in[3];
}

static void do_colorbalance_lgg_fac(bNode *node, float* out, float *in, float *fac)
{
	NodeColorBalance *n= (NodeColorBalance *)node->storage;
	const float mfac= 1.0f - *fac;
	
	out[0] = mfac*in[0] + *fac * colorbalance_lgg(in[0], n->lift[0], n->gamma[0], n->gain[0]);
	out[1] = mfac*in[1] + *fac * colorbalance_lgg(in[1], n->lift[1], n->gamma[1], n->gain[1]);
	out[2] = mfac*in[2] + *fac * colorbalance_lgg(in[2], n->lift[2], n->gamma[2], n->gain[2]);
	out[3] = in[3];
}

static void node_composit_exec_colorbalance(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *cbuf= in[1]->data;
	CompBuf *stackbuf;
	
	/* stack order input:  fac, image */
	/* stack order output: image */
	if(out[0]->hasoutput==0) return;
	
	if(in[0]->vec[0] == 0.f && in[0]->data == NULL) {
		out[0]->data = pass_on_compbuf(cbuf);
		return;
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
		} else {
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

static void node_composit_init_colorbalance(bNode *node)
{
	NodeColorBalance *n= node->storage= MEM_callocN(sizeof(NodeColorBalance), "node colorbalance");

	n->lift[0] = n->lift[1] = n->lift[2] = 1.0f;
	n->gamma[0] = n->gamma[1] = n->gamma[2] = 1.0f;
	n->gain[0] = n->gain[1] = n->gain[2] = 1.0f;
}

bNodeType cmp_node_colorbalance={
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_COLORBALANCE,
	/* name        */	"Color Balance",
	/* width+range */	400, 200, 400,
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_colorbalance_in,
	/* output sock */	cmp_node_colorbalance_out,
	/* storage     */	"NodeColorBalance",
	/* execfunc    */	node_composit_exec_colorbalance,
	/* butfunc     */	NULL,
	/* initfunc    */	node_composit_init_colorbalance,
	/* freestoragefunc    */	node_free_standard_storage,
	/* copystoragefunc    */	node_copy_standard_storage,
	/* id          */	NULL
};

