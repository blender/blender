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

/* **************** SET ALPHA ******************** */
static bNodeSocketType cmp_node_setalpha_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Alpha",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_setalpha_out[]= {
	{	SOCK_RGBA, 0, "Image",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_setalpha(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: RGBA image */
	/* stack order in: col, alpha */
	
	/* input no image? then only color operation */
	if(in[0]->data==NULL && in[1]->data==NULL) {
		out[0]->vec[0] = in[0]->vec[0];
		out[0]->vec[1] = in[0]->vec[1];
		out[0]->vec[2] = in[0]->vec[2];
		out[0]->vec[3] = in[1]->vec[0];
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data?in[0]->data:in[1]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); /* allocs */
		
		if(in[1]->data==NULL && in[1]->vec[0]==1.0f) {
			/* pass on image */
			composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_copy_rgb, CB_RGBA);
		}
		else {
			/* send an compbuf or a value to set as alpha - composit2_pixel_processor handles choosing the right one */
			composit2_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, do_copy_a_rgba, CB_RGBA, CB_VAL);
		}
	
		out[0]->data= stackbuf;
	}
}

bNodeType cmp_node_setalpha= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_SETALPHA,
	/* name        */	"Set Alpha",
	/* width+range */	120, 40, 140,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_setalpha_in,
	/* output sock */	cmp_node_setalpha_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_setalpha,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
	
};
