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
 * Contributor(s): Björn C. Schaefer
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_curves.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** CURVE Time  ******************** */

/* custom1 = sfra, custom2 = efra */
static bNodeSocketTemplate cmp_node_time_out[]= {
	{	SOCK_FLOAT, 0, N_("Fac")},
	{	-1, 0, ""	}
};

static void node_composit_exec_curves_time(void *data, bNode *node, bNodeStack **UNUSED(in), bNodeStack **out)
{
	RenderData *rd= data;
	/* stack order output: fac */
	float fac= 0.0f;
	
	if (node->custom1 < node->custom2)
		fac= (rd->cfra - node->custom1)/(float)(node->custom2-node->custom1);
	
	fac= curvemapping_evaluateF(node->storage, 0, fac);
	out[0]->vec[0]= CLAMPIS(fac, 0.0f, 1.0f);
}


static void node_composit_init_curves_time(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	node->custom1= 1;
	node->custom2= 250;
	node->storage= curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
}

void register_node_type_cmp_curve_time(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_TIME, "Time", NODE_CLASS_INPUT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, NULL, cmp_node_time_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_init(&ntype, node_composit_init_curves_time);
	node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);
	node_type_exec(&ntype, node_composit_exec_curves_time);

	nodeRegisterType(ttype, &ntype);
}



/* **************** CURVE VEC  ******************** */
static bNodeSocketTemplate cmp_node_curve_vec_in[]= {
	{	SOCK_VECTOR, 1, N_("Vector"),	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_curve_vec_out[]= {
	{	SOCK_VECTOR, 0, N_("Vector")},
	{	-1, 0, ""	}
};

static void node_composit_exec_curve_vec(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order input:  vec */
	/* stack order output: vec */
	
	curvemapping_evaluate_premulRGBF(node->storage, out[0]->vec, in[0]->vec);
}

static void node_composit_init_curve_vec(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	node->storage= curvemapping_add(3, -1.0f, -1.0f, 1.0f, 1.0f);
}

void register_node_type_cmp_curve_vec(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_CURVE_VEC, "Vector Curves", NODE_CLASS_OP_VECTOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_curve_vec_in, cmp_node_curve_vec_out);
	node_type_size(&ntype, 200, 140, 320);
	node_type_init(&ntype, node_composit_init_curve_vec);
	node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);
	node_type_exec(&ntype, node_composit_exec_curve_vec);

	nodeRegisterType(ttype, &ntype);
}


/* **************** CURVE RGB  ******************** */
static bNodeSocketTemplate cmp_node_curve_rgb_in[]= {
	{	SOCK_FLOAT, 1, N_("Fac"),	1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_FACTOR},
	{	SOCK_RGBA, 1, N_("Image"),	1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_RGBA, 1, N_("Black Level"),	0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, N_("White Level"),	1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_curve_rgb_out[]= {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

static void do_curves(bNode *node, float *out, float *in)
{
	curvemapping_evaluate_premulRGBF(node->storage, out, in);
	out[3]= in[3];
}

static void do_curves_fac(bNode *node, float *out, float *in, float *fac)
{
	
	if (*fac >= 1.0f)
		curvemapping_evaluate_premulRGBF(node->storage, out, in);
	else if (*fac <= 0.0f) {
		copy_v3_v3(out, in);
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

static void node_composit_exec_curve_rgb(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order input:  fac, image, black level, white level */
	/* stack order output: image */
	
	if (out[0]->hasoutput==0)
		return;

	/* input no image? then only color operation */
	if (in[1]->data==NULL) {
		curvemapping_evaluateRGBF(node->storage, out[0]->vec, in[1]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[1]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); /* allocs */
		
		curvemapping_set_black_white(node->storage, in[2]->vec, in[3]->vec);
		
		if (in[0]->data==NULL && in[0]->vec[0] == 1.0f)
			composit1_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, do_curves, CB_RGBA);
		else
			composit2_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[0]->data, in[0]->vec, do_curves_fac, CB_RGBA, CB_VAL);
		
		out[0]->data= stackbuf;
	}
	
}

static void node_composit_init_curve_rgb(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	node->storage= curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
}

void register_node_type_cmp_curve_rgb(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_CURVE_RGB, "RGB Curves", NODE_CLASS_OP_COLOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_curve_rgb_in, cmp_node_curve_rgb_out);
	node_type_size(&ntype, 200, 140, 320);
	node_type_init(&ntype, node_composit_init_curve_rgb);
	node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);
	node_type_exec(&ntype, node_composit_exec_curve_rgb);

	nodeRegisterType(ttype, &ntype);
}
