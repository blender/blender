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

/* **************** Rotate  ******************** */

static bNodeSocketType cmp_node_rotate_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Degr",			0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_rotate_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

/* function assumes out to be zero'ed, only does RGBA */
static void bilinear_interpolation_rotate(CompBuf *in, float *out, float u, float v)
{
	float *row1, *row2, *row3, *row4, a, b;
	float a_b, ma_b, a_mb, ma_mb;
	float empty[4]= {0.0f, 0.0f, 0.0f, 0.0f};
	int y1, y2, x1, x2;

	x1= (int)floor(u);
	x2= (int)ceil(u);
	y1= (int)floor(v);
	y2= (int)ceil(v);

	/* sample area entirely outside image? */
	if(x2<0 || x1>in->x-1 || y2<0 || y1>in->y-1)
		return;
	
	/* sample including outside of edges of image */
	if(x1<0 || y1<0) row1= empty;
	else row1= in->rect + in->x * y1 * in->type + in->type*x1;
	
	if(x1<0 || y2>in->y-1) row2= empty;
	else row2= in->rect + in->x * y2 * in->type + in->type*x1;
	
	if(x2>in->x-1 || y1<0) row3= empty;
	else row3= in->rect + in->x * y1 * in->type + in->type*x2;
	
	if(x2>in->x-1 || y2>in->y-1) row4= empty;
	else row4= in->rect + in->x * y2 * in->type + in->type*x2;
	
	a= u-floor(u);
	b= v-floor(v);
	a_b= a*b; ma_b= (1.0f-a)*b; a_mb= a*(1.0f-b); ma_mb= (1.0f-a)*(1.0f-b);
	
	out[0]= ma_mb*row1[0] + a_mb*row3[0] + ma_b*row2[0]+ a_b*row4[0];
	out[1]= ma_mb*row1[1] + a_mb*row3[1] + ma_b*row2[1]+ a_b*row4[1];
	out[2]= ma_mb*row1[2] + a_mb*row3[2] + ma_b*row2[2]+ a_b*row4[2];
	out[3]= ma_mb*row1[3] + a_mb*row3[3] + ma_b*row2[3]+ a_b*row4[3];
}

/* only supports RGBA nodes now */
static void node_composit_exec_rotate(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	
	if(out[0]->hasoutput==0)
		return;
	
	if(in[0]->data) {
		CompBuf *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1);	/* note, this returns zero'd image */
		float *ofp, rad, u, v, s, c, centx, centy, miny, maxy, minx, maxx;
		int x, y, yo;
	
		rad= (M_PI*in[1]->vec[0])/180.0f;
		
		s= sin(rad);
		c= cos(rad);
		centx= cbuf->x/2;
		centy= cbuf->y/2;
		
		minx= -centx;
		maxx= -centx + (float)cbuf->x;
		miny= -centy;
		maxy= -centy + (float)cbuf->y;
		
		for(y=miny; y<maxy; y++) {
			yo= y+(int)centy;
			ofp= stackbuf->rect + 4*yo*stackbuf->x;
			
			for(x=minx; x<maxx; x++, ofp+=4) {
				u= c*x + y*s + centx;
				v= -s*x + c*y + centy;
				
				bilinear_interpolation_rotate(cbuf, ofp, u, v);
			}
		}
		/* rotate offset vector too, but why negative rad, ehh?? Has to be replaced with [3][3] matrix once (ton) */
		s= sin(-rad);
		c= cos(-rad);
		centx= (float)cbuf->xof; centy= (float)cbuf->yof;
		stackbuf->xof= (int)( c*centx + s*centy);
		stackbuf->yof= (int)(-s*centx + c*centy);
		
		/* pass on output and free */
		out[0]->data= stackbuf;
		if(cbuf!=in[0]->data)
			free_compbuf(cbuf);
		
	}
}

bNodeType cmp_node_rotate= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_ROTATE,
	/* name        */	"Rotate",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_DISTORT, NODE_OPTIONS,
	/* input sock  */	cmp_node_rotate_in,
	/* output sock */	cmp_node_rotate_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_rotate,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
};
