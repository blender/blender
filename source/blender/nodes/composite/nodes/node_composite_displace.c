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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_displace.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** Displace  ******************** */

static bNodeSocketTemplate cmp_node_displace_in[]= {
	{	SOCK_RGBA, 1, N_("Image"),			1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_VECTOR, 1, N_("Vector"),			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_TRANSLATION},
	{	SOCK_FLOAT, 1, N_("X Scale"),				0.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, N_("Y Scale"),				0.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f, PROP_FACTOR},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_displace_out[]= {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

/* minimum distance (in pixels) a pixel has to be displaced
 * in order to take effect */
#define DISPLACE_EPSILON	0.01f

static void do_displace(bNode *node, CompBuf *stackbuf, CompBuf *cbuf, CompBuf *vecbuf, float *UNUSED(veccol), CompBuf *xbuf,  CompBuf *ybuf, float *xscale, float *yscale)
{
	ImBuf *ibuf;
	int x, y;
	float p_dx, p_dy;	/* main displacement in pixel space */
	float d_dx, d_dy;
	float dxt, dyt;
	float u, v;
	float xs, ys;
	float vec[3], vecdx[3], vecdy[3];
	float col[3];
	
	ibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0);
	ibuf->rect_float= cbuf->rect;
	
	for (y=0; y < stackbuf->y; y++) {
		for (x=0; x < stackbuf->x; x++) {
			/* calc pixel coordinates */
			qd_getPixel(vecbuf, x-vecbuf->xof, y-vecbuf->yof, vec);
			
			if (xbuf)
				qd_getPixel(xbuf, x-xbuf->xof, y-xbuf->yof, &xs);
			else
				xs = xscale[0];
			
			if (ybuf)
				qd_getPixel(ybuf, x-ybuf->xof, y-ybuf->yof, &ys);
			else
				ys = yscale[0];

			/* clamp x and y displacement to triple image resolution - 
			 * to prevent hangs from huge values mistakenly plugged in eg. z buffers */
			CLAMP(xs, -stackbuf->x*4, stackbuf->x*4);
			CLAMP(ys, -stackbuf->y*4, stackbuf->y*4);
			
			p_dx = vec[0] * xs;
			p_dy = vec[1] * ys;
			
			/* if no displacement, then just copy this pixel */
			if (fabsf(p_dx) < DISPLACE_EPSILON && fabsf(p_dy) < DISPLACE_EPSILON) {
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
			d_dx = vecdx[0] * xs;
			d_dy = vecdy[1] * ys;

			/* clamp derivatives to minimum displacement distance in UV space */
			dxt = p_dx - d_dx;
			dyt = p_dy - d_dy;

			dxt = signf(dxt)*maxf(fabsf(dxt), DISPLACE_EPSILON)/(float)stackbuf->x;
			dyt = signf(dyt)*maxf(fabsf(dyt), DISPLACE_EPSILON)/(float)stackbuf->y;
			
			ibuf_sample(ibuf, u, v, dxt, dyt, col);
			qd_setPixel(stackbuf, x, y, col);
			
			if (node->exec & NODE_BREAK) break;
		}
		
		if (node->exec & NODE_BREAK) break;
	}
	IMB_freeImBuf(ibuf);
	
	
/* simple method for reference, linear interpolation */
/*	
	int x, y;
	float dx, dy;
	float u, v;
	float vec[3];
	float col[3];
	
	for (y=0; y < stackbuf->y; y++) {
		for (x=0; x < stackbuf->x; x++) {
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


static void node_composit_exec_displace(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	if (out[0]->hasoutput==0)
		return;
	
	if (in[0]->data && in[1]->data) {
		CompBuf *cbuf= in[0]->data;
		CompBuf *vecbuf= in[1]->data;
		CompBuf *xbuf= in[2]->data;
		CompBuf *ybuf= in[3]->data;
		CompBuf *stackbuf;
		
		cbuf= typecheck_compbuf(cbuf, CB_RGBA);
		vecbuf= typecheck_compbuf(vecbuf, CB_VEC3);
		xbuf= typecheck_compbuf(xbuf, CB_VAL);
		ybuf= typecheck_compbuf(ybuf, CB_VAL);
		
		stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); /* allocs */

		do_displace(node, stackbuf, cbuf, vecbuf, in[1]->vec, xbuf, ybuf, in[2]->vec, in[3]->vec);
		
		out[0]->data= stackbuf;
		
		
		if (cbuf!=in[0]->data)
			free_compbuf(cbuf);
		if (vecbuf!=in[1]->data)
			free_compbuf(vecbuf);
	}
}

void register_node_type_cmp_displace(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_DISPLACE, "Displace", NODE_CLASS_DISTORT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_displace_in, cmp_node_displace_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_exec(&ntype, node_composit_exec_displace);

	nodeRegisterType(ttype, &ntype);
}
