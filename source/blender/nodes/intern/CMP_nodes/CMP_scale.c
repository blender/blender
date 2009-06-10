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

/* **************** Scale  ******************** */

#define CMP_SCALE_MAX	12000

static bNodeSocketType cmp_node_scale_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "X",				1.0f, 0.0f, 0.0f, 0.0f, 0.0001f, CMP_SCALE_MAX},
	{	SOCK_VALUE, 1, "Y",				1.0f, 0.0f, 0.0f, 0.0f, 0.0001f, CMP_SCALE_MAX},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_scale_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

/* only supports RGBA nodes now */
/* node->custom1 stores if input values are absolute or relative scale */
static void node_composit_exec_scale(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(out[0]->hasoutput==0)
		return;
	
	if(in[0]->data) {
		RenderData *rd= data;
		CompBuf *stackbuf, *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
		ImBuf *ibuf;
		int newx, newy;
		
		if(node->custom1==CMP_SCALE_RELATIVE) {
			newx= MAX2((int)(in[1]->vec[0]*cbuf->x), 1);
			newy= MAX2((int)(in[2]->vec[0]*cbuf->y), 1);
		}
		else if(node->custom1==CMP_SCALE_SCENEPERCENT) {
			newx = cbuf->x * (rd->size / 100.0f);
			newy = cbuf->y * (rd->size / 100.0f);
		} else {	/* CMP_SCALE_ABSOLUTE */
			newx= MAX2((int)in[1]->vec[0], 1);
			newy= MAX2((int)in[2]->vec[0], 1);
		}
		newx= MIN2(newx, CMP_SCALE_MAX);
		newy= MIN2(newy, CMP_SCALE_MAX);

		ibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0, 0);
		if(ibuf) {
			ibuf->rect_float= cbuf->rect;
			IMB_scaleImBuf(ibuf, newx, newy);
			
			if(ibuf->rect_float == cbuf->rect) {
				/* no scaling happened. */
				stackbuf= pass_on_compbuf(in[0]->data);
			}
			else {
				stackbuf= alloc_compbuf(newx, newy, CB_RGBA, 0);
				stackbuf->rect= ibuf->rect_float;
				stackbuf->malloc= 1;
			}

			ibuf->rect_float= NULL;
			ibuf->mall &= ~IB_rectfloat;
			IMB_freeImBuf(ibuf);
			
			/* also do the translation vector */
			stackbuf->xof = (int)(((float)newx/(float)cbuf->x) * (float)cbuf->xof);
			stackbuf->yof = (int)(((float)newy/(float)cbuf->y) * (float)cbuf->yof);
		}
		else {
			stackbuf= dupalloc_compbuf(cbuf);
			printf("Scaling to %dx%d failed\n", newx, newy);
		}
		
		out[0]->data= stackbuf;
		if(cbuf!=in[0]->data)
			free_compbuf(cbuf);
	}
};

bNodeType cmp_node_scale= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_SCALE,
	/* name        */	"Scale",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_DISTORT, NODE_OPTIONS,
	/* input sock  */	cmp_node_scale_in,
	/* output sock */	cmp_node_scale_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_scale,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
};






