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


/* **************** NORMAL  ******************** */
static bNodeSocketType cmp_node_normal_in[]= {
	{	SOCK_VECTOR, 1, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType cmp_node_normal_out[]= {
	{	SOCK_VECTOR, 0, "Normal",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_VALUE, 0, "Dot",		1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_normal(bNode *node, float *out, float *in)
{
	bNodeSocket *sock= node->outputs.first;
	float *nor= sock->ns.vec;
	
	/* render normals point inside... the widget points outside */
	out[0]= -INPR(nor, in);
}

/* generates normal, does dot product */
static void node_composit_exec_normal(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	bNodeSocket *sock= node->outputs.first;
	/* stack order input:  normal */
	/* stack order output: normal, value */
	
	/* input no image? then only vector op */
	if(in[0]->data==NULL) {
		VECCOPY(out[0]->vec, sock->ns.vec);
		/* render normals point inside... the widget points outside */
		out[1]->vec[0]= -INPR(out[0]->vec, in[0]->vec);
	}
	else if(out[1]->hasoutput) {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1); /* allocs */
		
		composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_normal, CB_VEC3);
		
		out[1]->data= stackbuf;
	}
	
	
}

bNodeType cmp_node_normal= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_NORMAL,
	/* name        */	"Normal",
	/* width+range */	100, 60, 200,
	/* class+opts  */	NODE_CLASS_OP_VECTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_normal_in,
	/* output sock */	cmp_node_normal_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_normal,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
	
};

