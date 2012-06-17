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
 * Contributor(s): Matt Ebb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_huecorrect.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

static bNodeSocketTemplate cmp_node_huecorrect_in[]= {
	{	SOCK_FLOAT, 1, N_("Fac"),	1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_RGBA, 1, N_("Image"),	1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_huecorrect_out[]= {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

static void do_huecorrect(bNode *node, float *out, float *in)
{
	float hsv[3], f;
	
	rgb_to_hsv(in[0], in[1], in[2], hsv, hsv+1, hsv+2);
	
	/* adjust hue, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(node->storage, 0, hsv[0]);
	hsv[0] += f-0.5f;
	
	/* adjust saturation, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(node->storage, 1, hsv[0]);
	hsv[1] *= (f * 2.f);
	
	/* adjust value, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(node->storage, 2, hsv[0]);
	hsv[2] *= (f * 2.f);
	
	hsv[0] = hsv[0] - floorf(hsv[0]); /* mod 1.0 */
	CLAMP(hsv[1], 0.f, 1.f);
	
	/* convert back to rgb */
	hsv_to_rgb(hsv[0], hsv[1], hsv[2], out, out+1, out+2);
	
	out[3]= in[3];
}

static void do_huecorrect_fac(bNode *node, float *out, float *in, float *fac)
{
	float hsv[3], rgb[3], f;
	const float mfac = 1.f-*fac;
	
	rgb_to_hsv(in[0], in[1], in[2], hsv, hsv+1, hsv+2);
	
	/* adjust hue, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(node->storage, 0, hsv[0]);
	hsv[0] += f-0.5f;
	
	/* adjust saturation, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(node->storage, 1, hsv[0]);
	hsv[1] *= (f * 2.f);
	
	/* adjust value, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(node->storage, 2, hsv[0]);
	hsv[2] *= (f * 2.f);
	
	hsv[0] = hsv[0] - floorf(hsv[0]);  /* mod 1.0 */
	CLAMP(hsv[1], 0.f, 1.f);
	
	/* convert back to rgb */
	hsv_to_rgb(hsv[0], hsv[1], hsv[2], rgb, rgb+1, rgb+2);
	
	out[0]= mfac*in[0] + *fac*rgb[0];
	out[1]= mfac*in[1] + *fac*rgb[1];
	out[2]= mfac*in[2] + *fac*rgb[2];
	out[3]= in[3];
}

static void node_composit_exec_huecorrect(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *cbuf= in[1]->data;
	CompBuf *stackbuf;
	
	/* stack order input:  fac, image, black level, white level */
	/* stack order output: image */
	
	if (out[0]->hasoutput==0)
		return;

	if (in[0]->vec[0] == 0.f && in[0]->data == NULL) {
		out[0]->data = pass_on_compbuf(cbuf);
		return;
	}
	
	/* input no image? then only color operation */
	if (in[1]->data==NULL) {
		do_huecorrect_fac(node, out[0]->vec, in[1]->vec, in[0]->vec);
	}
	
	if (cbuf) {
		stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); /* make output size of input image */
		
		if ((in[0]->data==NULL) && (in[0]->vec[0] >= 1.f))
			composit1_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, do_huecorrect, CB_RGBA);
		else
			composit2_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[0]->data, in[0]->vec, do_huecorrect_fac, CB_RGBA, CB_VAL);
		
		out[0]->data= stackbuf;
	}
	
}

static void node_composit_init_huecorrect(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	CurveMapping *cumapping = node->storage= curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
	int c;
	
	cumapping->preset = CURVE_PRESET_MID9;
	
	for (c=0; c<3; c++) {
		CurveMap *cuma = &cumapping->cm[c];
		curvemap_reset(cuma, &cumapping->clipr, cumapping->preset, CURVEMAP_SLOPE_POSITIVE);
	}
	
	/* default to showing Saturation */
	cumapping->cur = 1;
}

void register_node_type_cmp_huecorrect(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_HUECORRECT, "Hue Correct", NODE_CLASS_OP_COLOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_huecorrect_in, cmp_node_huecorrect_out);
	node_type_size(&ntype, 320, 140, 400);
	node_type_init(&ntype, node_composit_init_huecorrect);
	node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);
	node_type_exec(&ntype, node_composit_exec_huecorrect);

	nodeRegisterType(ttype, &ntype);
}
