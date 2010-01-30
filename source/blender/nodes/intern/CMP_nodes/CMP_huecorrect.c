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
 * Contributor(s): Matt Ebb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../CMP_util.h"

static bNodeSocketType cmp_node_huecorrect_in[]= {
	{	SOCK_VALUE, 1, "Fac",	1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType cmp_node_huecorrect_out[]= {
	{	SOCK_RGBA, 0, "Image",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_huecorrect(bNode *node, float *out, float *in)
{
	float hsv[3], f;
	
	rgb_to_hsv(in[0], in[1], in[2], hsv, hsv+1, hsv+2);
	
	/* adjust hue, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(node->storage, 0, hsv[0]);
	hsv[0] *= (f * 2.f);
	
	/* adjust saturation, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(node->storage, 1, hsv[0]);
	hsv[1] *= (f * 2.f);
	
	/* adjust value, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(node->storage, 2, hsv[0]);
	hsv[2] *= (f * 2.f);
	
	CLAMP(hsv[0], 0.f, 1.f);
	CLAMP(hsv[1], 0.f, 1.f);
	CLAMP(hsv[2], 0.f, 1.f);
	
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
	hsv[0] *= (f * 2.f);
	
	/* adjust saturation, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(node->storage, 1, hsv[0]);
	hsv[1] *= (f * 2.f);
	
	/* adjust value, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(node->storage, 2, hsv[0]);
	hsv[2] *= (f * 2.f);
	
	CLAMP(hsv[0], 0.f, 1.f);
	CLAMP(hsv[1], 0.f, 1.f);
	CLAMP(hsv[2], 0.f, 1.f);
	
	/* convert back to rgb */
	hsv_to_rgb(hsv[0], hsv[1], hsv[2], rgb, rgb+1, rgb+2);
	
	out[0]= mfac*in[0] + *fac*rgb[0];
	out[1]= mfac*in[1] + *fac*rgb[1];
	out[2]= mfac*in[2] + *fac*rgb[2];
	out[3]= in[3];
}

static void node_composit_exec_huecorrect(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *cbuf= in[1]->data;
	CompBuf *stackbuf;
	
	/* stack order input:  fac, image, black level, white level */
	/* stack order output: image */
	
	if(out[0]->hasoutput==0)
		return;

	if(in[0]->vec[0] == 0.f && in[0]->data == NULL) {
		out[0]->data = pass_on_compbuf(cbuf);
		return;
	}
	
	/* input no image? then only color operation */
	if(in[1]->data==NULL) {
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

static void node_composit_init_huecorrect(bNode* node)
{
	CurveMapping *cumapping = node->storage= curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
	int c, i;
	
	for (c=0; c<3; c++) {
		CurveMap *cuma = &cumapping->cm[c];
		
		/* set default horizontal curve */
		if(cuma->curve)
			MEM_freeN(cuma->curve);
		
		cuma->totpoint= 9;
		cuma->curve= MEM_callocN(cuma->totpoint*sizeof(CurveMapPoint), "curve points");
		
		for (i=0; i < cuma->totpoint; i++)
		{
			cuma->curve[i].x= i / ((float)cuma->totpoint-1);
			cuma->curve[i].y= 0.5;
		}
		
		if(cuma->table) {
			MEM_freeN(cuma->table);
			cuma->table= NULL;
		}
	}
	
	/* default to showing Saturation */
	cumapping->cur = 1;
}

bNodeType cmp_node_huecorrect= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_HUECORRECT,
	/* name        */	"Hue Correct",
	/* width+range */	320, 140, 400,
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_huecorrect_in,
	/* output sock */	cmp_node_huecorrect_out,
	/* storage     */	"CurveMapping",
	/* execfunc    */	node_composit_exec_huecorrect,
	/* butfunc     */	NULL,
	/* initfunc    */	node_composit_init_huecorrect,
	/* freestoragefunc    */	node_free_curves,
	/* copystoragefunc    */	node_copy_curves,
	/* id          */	NULL
};

