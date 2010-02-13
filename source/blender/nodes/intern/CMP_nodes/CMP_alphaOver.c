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

#include "../CMP_util.h"

/* **************** ALPHAOVER ******************** */
static bNodeSocketType cmp_node_alphaover_in[]= {
	{	SOCK_VALUE, 1, "Fac",			1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_alphaover_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_alphaover_premul(bNode *node, float *out, float *src, float *over, float *fac)
{
	
	if(over[3]<=0.0f) {
		QUATCOPY(out, src);
	}
	else if(fac[0]==1.0f && over[3]>=1.0f) {
		QUATCOPY(out, over);
	}
	else {
		float mul= 1.0f - fac[0]*over[3];

		out[0]= (mul*src[0]) + fac[0]*over[0];
		out[1]= (mul*src[1]) + fac[0]*over[1];
		out[2]= (mul*src[2]) + fac[0]*over[2];
		out[3]= (mul*src[3]) + fac[0]*over[3];
	}	
}

/* result will be still premul, but the over part is premulled */
static void do_alphaover_key(bNode *node, float *out, float *src, float *over, float *fac)
{
	
	if(over[3]<=0.0f) {
		QUATCOPY(out, src);
	}
	else if(fac[0]==1.0f && over[3]>=1.0f) {
		QUATCOPY(out, over);
	}
	else {
		float premul= fac[0]*over[3];
		float mul= 1.0f - premul;

		out[0]= (mul*src[0]) + premul*over[0];
		out[1]= (mul*src[1]) + premul*over[1];
		out[2]= (mul*src[2]) + premul*over[2];
		out[3]= (mul*src[3]) + fac[0]*over[3];
	}
}

/* result will be still premul, but the over part is premulled */
static void do_alphaover_mixed(bNode *node, float *out, float *src, float *over, float *fac)
{
	
	if(over[3]<=0.0f) {
		QUATCOPY(out, src);
	}
	else if(fac[0]==1.0f && over[3]>=1.0f) {
		QUATCOPY(out, over);
	}
	else {
		NodeTwoFloats *ntf= node->storage;
		float addfac= 1.0f - ntf->x + over[3]*ntf->x;
		float premul= fac[0]*addfac;
		float mul= 1.0f - fac[0]*over[3];
		
		out[0]= (mul*src[0]) + premul*over[0];
		out[1]= (mul*src[1]) + premul*over[1];
		out[2]= (mul*src[2]) + premul*over[2];
		out[3]= (mul*src[3]) + fac[0]*over[3];
	}
}




static void node_composit_exec_alphaover(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: col col */
	/* stack order out: col */
	if(out[0]->hasoutput==0) 
		return;
	
	/* input no image? then only color operation */
	if(in[1]->data==NULL && in[2]->data==NULL) {
		do_alphaover_premul(node, out[0]->vec, in[1]->vec, in[2]->vec, in[0]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[1]->data?in[1]->data:in[2]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); /* allocs */
		NodeTwoFloats *ntf= node->storage;
		
		if(ntf->x != 0.0f)
			composit3_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[2]->data, in[2]->vec, in[0]->data, in[0]->vec, do_alphaover_mixed, CB_RGBA, CB_RGBA, CB_VAL);
		else if(node->custom1)
			composit3_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[2]->data, in[2]->vec, in[0]->data, in[0]->vec, do_alphaover_key, CB_RGBA, CB_RGBA, CB_VAL);
		else
			composit3_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[2]->data, in[2]->vec, in[0]->data, in[0]->vec, do_alphaover_premul, CB_RGBA, CB_RGBA, CB_VAL);
		
		out[0]->data= stackbuf;
	}
}

static void node_alphaover_init(bNode* node)
{
   node->storage= MEM_callocN(sizeof(NodeTwoFloats), "NodeTwoFloats");
}

bNodeType cmp_node_alphaover= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_ALPHAOVER,
	/* name        */	"AlphaOver",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_alphaover_in,
	/* output sock */	cmp_node_alphaover_out,
	/* storage     */	"NodeTwoFloats",
	/* execfunc    */	node_composit_exec_alphaover,
	/* butfunc     */	NULL,
	/* initfunc    */	node_alphaover_init,
	/* freestoragefunc    */	node_free_standard_storage,
	/* copystoragefunc    */	node_copy_standard_storage,
	/* id          */	NULL
	
};


