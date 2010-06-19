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


/* **************** Displace  ******************** */

static bNodeSocketType cmp_node_displace_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 1, "Vector",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "X Scale",				0.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
	{	SOCK_VALUE, 1, "Y Scale",				0.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_displace_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

/* minimum distance (in pixels) a pixel has to be displaced
 * in order to take effect */
#define DISPLACE_EPSILON	0.01

static void do_displace(CompBuf *stackbuf, CompBuf *cbuf, CompBuf *vecbuf, float *veccol, float *xscale, float *yscale)
{
	ImBuf *ibuf;
	int x, y;
	float p_dx, p_dy;	/* main displacement in pixel space */
	float d_dx, d_dy;
	float dxt, dyt;
	float u, v;
	float vec[3], vecdx[3], vecdy[3];
	float col[3];
	
	ibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0, 0);
	ibuf->rect_float= cbuf->rect;
	
	for(y=0; y < stackbuf->y; y++) {
		for(x=0; x < stackbuf->x; x++) {
			/* calc pixel coordinates */
			qd_getPixel(vecbuf, x-vecbuf->xof, y-vecbuf->yof, vec);
			p_dx = vec[0] * xscale[0];
			p_dy = vec[1] * yscale[0];
			
			/* if no displacement, then just copy this pixel */
			if (p_dx < DISPLACE_EPSILON && p_dy < DISPLACE_EPSILON) {
				qd_getPixel(cbuf, x-cbuf->xof, y-cbuf->yof, col);
				qd_setPixel(stackbuf, x, y, col);
				continue;
			}
			
			/* displaced pixel in uv coords, for image sampling */
			u = (x - cbuf->xof - p_dx + 0.5f) / (float)stackbuf->x;
			v = (y - cbuf->yof - p_dy + 0.5f) / (float)stackbuf->y;
			
			
			/* calc derivatives */
			qd_getPixel(vecbuf, x-vecbuf->xof+1, y-vecbuf->yof, vecdx);
			qd_getPixel(vecbuf, x-vecbuf->xof, y-vecbuf->yof+1, vecdy);
			d_dx = vecdx[0] * xscale[0];
			d_dy = vecdy[0] * yscale[0];
			
			/* clamp derivatives to minimum displacement distance in UV space */
			dxt = MAX2(p_dx - d_dx, DISPLACE_EPSILON)/(float)stackbuf->x;
			dyt = MAX2(p_dy - d_dy, DISPLACE_EPSILON)/(float)stackbuf->y;
			
			ibuf_sample(ibuf, u, v, dxt, dyt, col);
			qd_setPixel(stackbuf, x, y, col);
		}
	}
	IMB_freeImBuf(ibuf);
	
	
/* simple method for reference, linear interpolation */
/*	
	int x, y;
	float dx, dy;
	float u, v;
	float vec[3];
	float col[3];
	
	for(y=0; y < stackbuf->y; y++) {
		for(x=0; x < stackbuf->x; x++) {
			qd_getPixel(vecbuf, x, y, vec);
			
			dx = vec[0] * (xscale[0]);
			dy = vec[1] * (yscale[0]);
			
			u = (x - dx + 0.5f) / (float)stackbuf->x;
			v = (y - dy + 0.5f) / (float)stackbuf->y;
			
			qd_getPixelLerp(cbuf, u*cbuf->x - 0.5f, v*cbuf->y - 0.5f, col);
			qd_setPixel(stackbuf, x, y, col);
		}
	}
*/
}


static void node_composit_exec_displace(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(out[0]->hasoutput==0)
		return;
	
	if(in[0]->data && in[1]->data) {
		CompBuf *cbuf= in[0]->data;
		CompBuf *vecbuf= in[1]->data;
		CompBuf *stackbuf;
		
		cbuf= typecheck_compbuf(cbuf, CB_RGBA);
		vecbuf= typecheck_compbuf(vecbuf, CB_VEC3);
		stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); /* allocs */

		do_displace(stackbuf, cbuf, vecbuf, in[1]->vec, in[2]->vec, in[3]->vec);
		
		out[0]->data= stackbuf;
		
		
		if(cbuf!=in[0]->data)
			free_compbuf(cbuf);
		if(vecbuf!=in[1]->data)
			free_compbuf(vecbuf);
	}
}

bNodeType cmp_node_displace= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_DISPLACE,
	/* name        */	"Displace",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_DISTORT, NODE_OPTIONS,
	/* input sock  */	cmp_node_displace_in,
	/* output sock */	cmp_node_displace_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_displace, 
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
};

