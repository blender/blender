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

/* **************** FILTER  ******************** */
static bNodeSocketType cmp_node_filter_in[]= {
	{	SOCK_VALUE, 1, "Fac",			1.0f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_filter_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_filter_edge(CompBuf *out, CompBuf *in, float *filter, float fac)
{
	float *row1, *row2, *row3;
	float *fp, f1, f2, mfac= 1.0f-fac;
	int rowlen, x, y, c, pix= in->type;
	
	rowlen= in->x;
	
	for(y=0; y<in->y; y++) {
		/* setup rows */
		if(y==0) row1= in->rect;
		else row1= in->rect + pix*(y-1)*rowlen;
		
		row2= in->rect + y*pix*rowlen;
		
		if(y==in->y-1) row3= row2;
		else row3= row2 + pix*rowlen;
		
		fp= out->rect + pix*y*rowlen;
		
		if(pix==CB_RGBA) {
			QUATCOPY(fp, row2);
			fp+= pix;
			
			for(x=2; x<rowlen; x++) {
				for(c=0; c<3; c++) {
					f1= filter[0]*row1[0] + filter[1]*row1[4] + filter[2]*row1[8] + filter[3]*row2[0] + filter[4]*row2[4] + filter[5]*row2[8] + filter[6]*row3[0] + filter[7]*row3[4] + filter[8]*row3[8];
					f2= filter[0]*row1[0] + filter[3]*row1[4] + filter[6]*row1[8] + filter[1]*row2[0] + filter[4]*row2[4] + filter[7]*row2[8] + filter[2]*row3[0] + filter[5]*row3[4] + filter[8]*row3[8];
					fp[0]= mfac*row2[4] + fac*sqrt(f1*f1 + f2*f2);
					fp++; row1++; row2++; row3++;
				}
				fp[0]= row2[4];
				/* no alpha... will clear it completely */
				fp++; row1++; row2++; row3++;
			}
			QUATCOPY(fp, row2+4);
		}
		else if(pix==CB_VAL) {
			fp+= pix;
			for(x=2; x<rowlen; x++) {
				f1= filter[0]*row1[0] + filter[1]*row1[1] + filter[2]*row1[2] + filter[3]*row2[0] + filter[4]*row2[1] + filter[5]*row2[2] + filter[6]*row3[0] + filter[7]*row3[1] + filter[8]*row3[2];
				f2= filter[0]*row1[0] + filter[3]*row1[1] + filter[6]*row1[2] + filter[1]*row2[0] + filter[4]*row2[1] + filter[7]*row2[2] + filter[2]*row3[0] + filter[5]*row3[1] + filter[8]*row3[2];
				fp[0]= mfac*row2[1] + fac*sqrt(f1*f1 + f2*f2);
				fp++; row1++; row2++; row3++;
			}
		}
	}
}

static void do_filter3(CompBuf *out, CompBuf *in, float *filter, float fac)
{
	float *row1, *row2, *row3;
	float *fp, mfac= 1.0f-fac;
	int rowlen, x, y, c;
	int pixlen= in->type;
	
	rowlen= in->x;
	
	for(y=0; y<in->y; y++) {
		/* setup rows */
		if(y==0) row1= in->rect;
		else row1= in->rect + pixlen*(y-1)*rowlen;
		
		row2= in->rect + y*pixlen*rowlen;
		
		if(y==in->y-1) row3= row2;
		else row3= row2 + pixlen*rowlen;
		
		fp= out->rect + pixlen*(y)*rowlen;
		
		if(pixlen==1) {
			fp[0]= row2[0];
			fp+= 1;
			
			for(x=2; x<rowlen; x++) {
				fp[0]= mfac*row2[1] + fac*(filter[0]*row1[0] + filter[1]*row1[1] + filter[2]*row1[2] + filter[3]*row2[0] + filter[4]*row2[1] + filter[5]*row2[2] + filter[6]*row3[0] + filter[7]*row3[1] + filter[8]*row3[2]);
				fp++; row1++; row2++; row3++;
			}
			fp[0]= row2[1];
		}
		else if(pixlen==2) {
			fp[0]= row2[0];
			fp[1]= row2[1];
			fp+= 2;
			
			for(x=2; x<rowlen; x++) {
				for(c=0; c<2; c++) {
					fp[0]= mfac*row2[2] + fac*(filter[0]*row1[0] + filter[1]*row1[2] + filter[2]*row1[4] + filter[3]*row2[0] + filter[4]*row2[2] + filter[5]*row2[4] + filter[6]*row3[0] + filter[7]*row3[2] + filter[8]*row3[4]);
					fp++; row1++; row2++; row3++;
				}
			}
			fp[0]= row2[2];
			fp[1]= row2[3];
		}
		else if(pixlen==3) {
			VECCOPY(fp, row2);
			fp+= 3;
			
			for(x=2; x<rowlen; x++) {
				for(c=0; c<3; c++) {
					fp[0]= mfac*row2[3] + fac*(filter[0]*row1[0] + filter[1]*row1[3] + filter[2]*row1[6] + filter[3]*row2[0] + filter[4]*row2[3] + filter[5]*row2[6] + filter[6]*row3[0] + filter[7]*row3[3] + filter[8]*row3[6]);
					fp++; row1++; row2++; row3++;
				}
			}
			VECCOPY(fp, row2+3);
		}
		else {
			QUATCOPY(fp, row2);
			fp+= 4;
			
			for(x=2; x<rowlen; x++) {
				for(c=0; c<4; c++) {
					fp[0]= mfac*row2[4] + fac*(filter[0]*row1[0] + filter[1]*row1[4] + filter[2]*row1[8] + filter[3]*row2[0] + filter[4]*row2[4] + filter[5]*row2[8] + filter[6]*row3[0] + filter[7]*row3[4] + filter[8]*row3[8]);
					fp++; row1++; row2++; row3++;
				}
			}
			QUATCOPY(fp, row2+4);
		}
	}
}


static void node_composit_exec_filter(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	static float soft[9]= {1/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 4/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 1/16.0f};
	float sharp[9]= {-1,-1,-1,-1,9,-1,-1,-1,-1};
	float laplace[9]= {-1/8.0f, -1/8.0f, -1/8.0f, -1/8.0f, 1.0f, -1/8.0f, -1/8.0f, -1/8.0f, -1/8.0f};
	float sobel[9]= {1,2,1,0,0,0,-1,-2,-1};
	float prewitt[9]= {1,1,1,0,0,0,-1,-1,-1};
	float kirsch[9]= {5,5,5,-3,-3,-3,-2,-2,-2};
	float shadow[9]= {1,2,1,0,1,0,-1,-2,-1};
	
	if(out[0]->hasoutput==0) return;
	
	/* stack order in: Image */
	/* stack order out: Image */
	
	if(in[1]->data) {
		/* make output size of first available input image */
		CompBuf *cbuf= in[1]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, cbuf->type, 1); /* allocs */
		
		/* warning note: xof and yof are applied in pixelprocessor, but should be copied otherwise? */
		stackbuf->xof= cbuf->xof;
		stackbuf->yof= cbuf->yof;
		
		switch(node->custom1) {
			case CMP_FILT_SOFT:
				do_filter3(stackbuf, cbuf, soft, in[0]->vec[0]);
				break;
			case CMP_FILT_SHARP:
				do_filter3(stackbuf, cbuf, sharp, in[0]->vec[0]);
				break;
			case CMP_FILT_LAPLACE:
				do_filter3(stackbuf, cbuf, laplace, in[0]->vec[0]);
				break;
			case CMP_FILT_SOBEL:
				do_filter_edge(stackbuf, cbuf, sobel, in[0]->vec[0]);
				break;
			case CMP_FILT_PREWITT:
				do_filter_edge(stackbuf, cbuf, prewitt, in[0]->vec[0]);
				break;
			case CMP_FILT_KIRSCH:
				do_filter_edge(stackbuf, cbuf, kirsch, in[0]->vec[0]);
				break;
			case CMP_FILT_SHADOW:
				do_filter3(stackbuf, cbuf, shadow, in[0]->vec[0]);
				break;
		}
			
		out[0]->data= stackbuf;
		
		generate_preview(node, out[0]->data);
	}
}


bNodeType cmp_node_filter= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_FILTER,
	/* name        */	"Filter",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_OP_FILTER, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */	cmp_node_filter_in,
	/* output sock */	cmp_node_filter_out,
	/* storage     */	"", 
	/* execfunc    */	node_composit_exec_filter,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
	
};


