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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../CMP_util.h"


/* **************** Z COMBINE ******************** */
    /* lazy coder note: node->custom1 is abused to send signal */
static bNodeSocketType cmp_node_zcombine_in[]= {
	{	SOCK_RGBA, 1, "Image",		0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Z",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 10000.0f},
	{	SOCK_RGBA, 1, "Image",		0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Z",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 10000.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_zcombine_out[]= {
	{	SOCK_RGBA, 0, "Image",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Z",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 10000.0f},
	{	-1, 0, ""	}
};

static void do_zcombine(bNode *node, float *out, float *src1, float *z1, float *src2, float *z2)
{
	if(*z1 <= *z2) {
		QUATCOPY(out, src1);
	}
	else {
		QUATCOPY(out, src2);
		
		if(node->custom1)
			*z1= *z2;
	}
}

static void do_zcombine_mask(bNode *node, float *out, float *z1, float *z2)
{
	if(*z1 > *z2) {
		*out= 1.0f;
		if(node->custom1)
			*z1= *z2;
	}
}

static void do_zcombine_add(bNode *node, float *out, float *col1, float *col2, float *acol)
{
	float alpha= *acol;
	float malpha= 1.0f - alpha;
	
	out[0]= malpha*col1[0] + alpha*col2[0];
	out[1]= malpha*col1[1] + alpha*col2[1];
	out[2]= malpha*col1[2] + alpha*col2[2];
	out[3]= malpha*col1[3] + alpha*col2[3];
}

static void node_composit_exec_zcombine(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	RenderData *rd= data;
	CompBuf *cbuf= in[0]->data;
	CompBuf *zbuf;

	/* stack order in: col z col z */
	/* stack order out: col z */
	if(out[0]->hasoutput==0 && out[1]->hasoutput==0) 
		return;
	
	/* no input image; do nothing now */
	if(in[0]->data==NULL) {
		return;
	}
	
	if(out[1]->hasoutput) {
		/* copy or make a buffer for for the first z value, here we write result in */
		if(in[1]->data)
			zbuf= dupalloc_compbuf(in[1]->data);
		else {
			float *zval;
			int tot= cbuf->x*cbuf->y;
			
			zbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1);
			for(zval= zbuf->rect; tot; tot--, zval++)
				*zval= in[1]->vec[0];
		}
		/* lazy coder hack */
		node->custom1= 1;
		out[1]->data= zbuf;
	}
	else {
		node->custom1= 0;
		zbuf= in[1]->data;
	}
	
	if(rd->scemode & R_FULL_SAMPLE) {
		/* make output size of first input image */
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
		
		composit4_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, zbuf, in[1]->vec, in[2]->data, in[2]->vec, 
								  in[3]->data, in[3]->vec, do_zcombine, CB_RGBA, CB_VAL, CB_RGBA, CB_VAL);
		
		out[0]->data= stackbuf;
	}
	else {
		/* make output size of first input image */
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); /* allocs */
		CompBuf *mbuf;
		float *fp;
		int x;
		char *aabuf;
		
		
		/* make a mask based on comparison, optionally write zvalue */
		mbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1);
		composit2_pixel_processor(node, mbuf, zbuf, in[1]->vec, in[3]->data, in[3]->vec, do_zcombine_mask, CB_VAL, CB_VAL);
		
		/* convert to char */
		aabuf= MEM_mallocN(cbuf->x*cbuf->y, "aa buf");
		fp= mbuf->rect;
		for(x= cbuf->x*cbuf->y-1; x>=0; x--)
			if(fp[x]==0.0f) aabuf[x]= 0;
			else aabuf[x]= 255;
		
		antialias_tagbuf(cbuf->x, cbuf->y, aabuf);
		
		/* convert to float */
		fp= mbuf->rect;
		for(x= cbuf->x*cbuf->y-1; x>=0; x--)
			if(aabuf[x]>1)
				fp[x]= (1.0f/255.0f)*(float)aabuf[x];
		
		composit3_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[2]->data, in[2]->vec, mbuf, NULL, 
								  do_zcombine_add, CB_RGBA, CB_RGBA, CB_VAL);
		/* free */
		free_compbuf(mbuf);
		MEM_freeN(aabuf);
		
		out[0]->data= stackbuf;
	}

}

bNodeType cmp_node_zcombine= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_ZCOMBINE,
	/* name        */	"Z Combine",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_zcombine_in,
	/* output sock */	cmp_node_zcombine_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_zcombine,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
	
};
