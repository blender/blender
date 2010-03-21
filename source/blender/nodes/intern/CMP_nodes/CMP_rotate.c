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

/* only supports RGBA nodes now */
static void node_composit_exec_rotate(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	
	if(out[0]->hasoutput==0)
		return;
	
	if(in[0]->data) {
		CompBuf *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1);	/* note, this returns zero'd image */
		float rad, u, v, s, c, centx, centy, miny, maxy, minx, maxx;
		int x, y, yo, xo;
      ImBuf *ibuf, *obuf;
	
		rad= (M_PI*in[1]->vec[0])/180.0f;
		
		s= sin(rad);
		c= cos(rad);
		centx= cbuf->x/2;
		centy= cbuf->y/2;
		
		minx= -centx;
		maxx= -centx + (float)cbuf->x;
		miny= -centy;
		maxy= -centy + (float)cbuf->y;
		

      ibuf=IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0, 0);
      obuf=IMB_allocImBuf(stackbuf->x, stackbuf->y, 32, 0, 0);

      if(ibuf){
         ibuf->rect_float=cbuf->rect;
         obuf->rect_float=stackbuf->rect;

		   for(y=miny; y<maxy; y++) {
			   yo= y+(int)centy;
		      
            for(x=minx; x<maxx;x++) {
               u=c*x + y*s + centx;
               v=-s*x + c*y + centy;
               xo= x+(int)centx;

               switch(node->custom1) {
                  case 0:
                     neareast_interpolation(ibuf, obuf, u, v, xo, yo);
                     break ;
                  case 1:
                     bilinear_interpolation(ibuf, obuf, u, v, xo, yo);
                     break;
                  case 2:
                     bicubic_interpolation(ibuf, obuf, u, v, xo, yo);
               }
               
            }
			}
        
         /* rotate offset vector too, but why negative rad, ehh?? Has to be replaced with [3][3] matrix once (ton) */
		   s= sin(-rad);
		   c= cos(-rad);
		   centx= (float)cbuf->xof; centy= (float)cbuf->yof;
		   stackbuf->xof= (int)( c*centx + s*centy);
		   stackbuf->yof= (int)(-s*centx + c*centy);
		}
		
		/* pass on output and free */
		out[0]->data= stackbuf;
		if(cbuf!=in[0]->data)
			free_compbuf(cbuf);
	}
}

static void node_composit_init_rotate(bNode *node)
{
   node->custom1= 1; /* Bilinear Filter*/
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
	/* initfunc    */	node_composit_init_rotate,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
};
