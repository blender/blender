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


/* **************** VALTORGB ******************** */
static bNodeSocketType cmp_node_valtorgb_in[]= {
	{	SOCK_VALUE, 1, "Fac",			0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_valtorgb_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Alpha",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_colorband_composit(bNode *node, float *out, float *in)
{
	do_colorband(node->storage, in[0], out);
}

static void node_composit_exec_valtorgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: fac */
	/* stack order out: col, alpha */
	
	if(out[0]->hasoutput==0 && out[1]->hasoutput==0) 
		return;
	
	if(node->storage) {
		/* input no image? then only color operation */
		if(in[0]->data==NULL) {
			do_colorband(node->storage, in[0]->vec[0], out[0]->vec);
		}
		else {
			/* make output size of input image */
			CompBuf *cbuf= in[0]->data;
			CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); /* allocs */
			
			composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_colorband_composit, CB_VAL);
			
			out[0]->data= stackbuf;
			
			if(out[1]->hasoutput)
				out[1]->data= valbuf_from_rgbabuf(stackbuf, CHAN_A);

		}
	}
}

static void node_composit_init_valtorgb(bNode* node)
{
   node->storage= add_colorband(1);
}

bNodeType cmp_node_valtorgb= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_VALTORGB,
	/* name        */	"ColorRamp",
	/* width+range */	240, 200, 300,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_valtorgb_in,
	/* output sock */	cmp_node_valtorgb_out,
	/* storage     */	"ColorBand",
	/* execfunc    */	node_composit_exec_valtorgb,
	/* butfunc     */	NULL,
	/* initfunc    */	node_composit_init_valtorgb,
	/* freestoragefunc    */	node_free_standard_storage,
	/* copystoragefunc    */	node_copy_standard_storage,
	/* id          */	NULL
	
};


/* **************** RGBTOBW ******************** */
static bNodeSocketType cmp_node_rgbtobw_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_rgbtobw_out[]= {
	{	SOCK_VALUE, 0, "Val",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_rgbtobw(bNode *node, float *out, float *in)
{
	out[0]= in[0]*0.35f + in[1]*0.45f + in[2]*0.2f;
}

static void node_composit_exec_rgbtobw(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: bw */
	/* stack order in: col */
	
	if(out[0]->hasoutput==0)
		return;
	
	/* input no image? then only color operation */
	if(in[0]->data==NULL) {
		do_rgbtobw(node, out[0]->vec, in[0]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1); /* allocs */
		
		composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_rgbtobw, CB_RGBA);
		
		out[0]->data= stackbuf;
	}
}

bNodeType cmp_node_rgbtobw= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_RGBTOBW,
	/* name        */	"RGB to BW",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_CONVERTOR, 0,
	/* input sock  */	cmp_node_rgbtobw_in,
	/* output sock */	cmp_node_rgbtobw_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_rgbtobw,
	/* butfunc     */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
	
};


