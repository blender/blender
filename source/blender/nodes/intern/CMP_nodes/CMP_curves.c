/**
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Björn C. Schaefer
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../CMP_util.h"


/* **************** CURVE Time  ******************** */

/* custom1 = sfra, custom2 = efra */
static bNodeSocketType cmp_node_time_out[]= {
	{	SOCK_VALUE, 0, "Fac",	1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_curves_time(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	RenderData *rd= data;
	/* stack order output: fac */
	float fac= 0.0f;
	
	if(node->custom1 < node->custom2)
		fac= (rd->cfra - node->custom1)/(float)(node->custom2-node->custom1);
	
	fac= curvemapping_evaluateF(node->storage, 0, fac);
	out[0]->vec[0]= CLAMPIS(fac, 0.0f, 1.0f);
}


static void node_composit_init_curves_time(bNode* node)
{
   node->custom1= 1;
   node->custom2= 250;
   node->storage= curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
}

bNodeType cmp_node_curve_time= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_TIME,
	/* name        */	"Time",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	cmp_node_time_out,
	/* storage     */	"CurveMapping",
	/* execfunc    */	node_composit_exec_curves_time,
	/* butfunc     */	NULL,
	/* initfunc    */	node_composit_init_curves_time,
	/* freestoragefunc    */	node_free_curves,
	/* copystoragefunc    */	node_copy_curves,
	/* id          */	NULL
};



/* **************** CURVE VEC  ******************** */
static bNodeSocketType cmp_node_curve_vec_in[]= {
	{	SOCK_VECTOR, 1, "Vector",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType cmp_node_curve_vec_out[]= {
	{	SOCK_VECTOR, 0, "Vector",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_curve_vec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order input:  vec */
	/* stack order output: vec */
	
	curvemapping_evaluate_premulRGBF(node->storage, out[0]->vec, in[0]->vec);
};

static void node_composit_init_curve_vec(bNode* node)
{
   node->storage= curvemapping_add(3, -1.0f, -1.0f, 1.0f, 1.0f);
};

bNodeType cmp_node_curve_vec= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_CURVE_VEC,
	/* name        */	"Vector Curves",
	/* width+range */	200, 140, 320,
	/* class+opts  */	NODE_CLASS_OP_VECTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_curve_vec_in,
	/* output sock */	cmp_node_curve_vec_out,
	/* storage     */	"CurveMapping",
	/* execfunc    */	node_composit_exec_curve_vec,
	/* butfunc     */	NULL,
	/* initfunc    */	node_composit_init_curve_vec,
	/* freestoragefunc    */	node_free_curves,
	/* copystoragefunc    */	node_copy_curves,
	/* id          */	NULL
	
};

/* **************** CURVE RGB  ******************** */
static bNodeSocketType cmp_node_curve_rgb_in[]= {
	{	SOCK_VALUE, 1, "Fac",	1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_RGBA, 1, "Black Level",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_RGBA, 1, "White Level",	1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType cmp_node_curve_rgb_out[]= {
	{	SOCK_RGBA, 0, "Image",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_curves(bNode *node, float *out, float *in)
{
	curvemapping_evaluate_premulRGBF(node->storage, out, in);
	out[3]= in[3];
}

static void do_curves_fac(bNode *node, float *out, float *in, float *fac)
{
	
	if(*fac>=1.0)
		curvemapping_evaluate_premulRGBF(node->storage, out, in);
	else if(*fac<=0.0) {
		VECCOPY(out, in);
	}
	else {
		float col[4], mfac= 1.0f-*fac;
		curvemapping_evaluate_premulRGBF(node->storage, col, in);
		out[0]= mfac*in[0] + *fac*col[0];
		out[1]= mfac*in[1] + *fac*col[1];
		out[2]= mfac*in[2] + *fac*col[2];
	}
	out[3]= in[3];
}

static void node_composit_exec_curve_rgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order input:  fac, image, black level, white level */
	/* stack order output: image */
	
	if(out[0]->hasoutput==0)
		return;

	/* input no image? then only color operation */
	if(in[1]->data==NULL) {
		curvemapping_evaluateRGBF(node->storage, out[0]->vec, in[1]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[1]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); /* allocs */
		
		curvemapping_set_black_white(node->storage, in[2]->vec, in[3]->vec);
		
		if(in[0]->vec[0] == 1.0)
			composit1_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, do_curves, CB_RGBA);
		else
			composit2_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[0]->data, in[0]->vec, do_curves_fac, CB_RGBA, CB_VAL);
		
		out[0]->data= stackbuf;
	}
	
};

static void node_composit_init_curve_rgb(bNode* node)
{
   node->storage= curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
};

bNodeType cmp_node_curve_rgb= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_CURVE_RGB,
	/* name        */	"RGB Curves",
	/* width+range */	200, 140, 320,
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_curve_rgb_in,
	/* output sock */	cmp_node_curve_rgb_out,
	/* storage     */	"CurveMapping",
	/* execfunc    */	node_composit_exec_curve_rgb,
	/* butfunc     */	NULL,
	/* initfunc    */	node_composit_init_curve_rgb,
	/* freestoragefunc    */	node_free_curves,
	/* copystoragefunc    */	node_copy_curves,
	/* id          */	NULL
};

