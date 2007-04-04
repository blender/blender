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


/* ******************* Color Spill Supression ********************************* */
static bNodeSocketType cmp_node_color_spill_in[]={
	{SOCK_RGBA,1,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static bNodeSocketType cmp_node_color_spill_out[]={
	{SOCK_RGBA,0,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static void do_reduce_red(bNode *node, float* out, float *in)
{
	NodeChroma *c;
	c=node->storage;
	
	if(in[0] > in[1] && in[0] > in[2]) {
		out[0]=((in[1]+in[2])/2)*(1-c->t1);
	}
}

static void do_reduce_green(bNode *node, float* out, float *in)
{
	NodeChroma *c;
	c=node->storage;
	
	if(in[1] > in[0] && in[1] > in[2]) {
		out[1]=((in[0]+in[2])/2)*(1-c->t1);
	}
}

static void do_reduce_blue(bNode *node, float* out, float *in)
{
	NodeChroma *c;
	c=node->storage;
	
	if(in[2] > in[1] && in[2] > in[1]) {
		out[2]=((in[1]+in[0])/2)*(1-c->t1);
	}
}

static void node_composit_exec_color_spill(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/*
	Originally based on the information from the book "The Art and Science of Digital Composition" and 
	discussions from vfxtalk.com.*/
	CompBuf *cbuf;
	CompBuf *rgbbuf;
	
	if(out[0]->hasoutput==0 || in[0]->hasinput==0) return;
	if(in[0]->data==NULL) return;
	
	cbuf=typecheck_compbuf(in[0]->data, CB_RGBA);
	rgbbuf=dupalloc_compbuf(cbuf);

	switch(node->custom1)
	{
	case 1:  /*red spill*/
		{
			composit1_pixel_processor(node, rgbbuf, cbuf, in[1]->vec, do_reduce_red, CB_RGBA);
			break;
		}
	case 2: /*green spill*/
		{
			composit1_pixel_processor(node, rgbbuf, cbuf, in[1]->vec, do_reduce_green, CB_RGBA);
			break;
		}
	case 3: /*blue spill*/
		{
			composit1_pixel_processor(node, rgbbuf, cbuf, in[1]->vec, do_reduce_blue, CB_RGBA);
			break;
		}
	default:
		break;
	}

	out[0]->data=rgbbuf;

	if(cbuf!=in[0]->data)
		free_compbuf(cbuf);
}

static void node_composit_init_color_spill(bNode *node)
{
   NodeChroma *c= MEM_callocN(sizeof(NodeChroma), "node chroma");
   node->storage=c;
   c->t1= 0.0f;
   c->t2= 0.0f;
   c->t3= 0.0f;
   c->fsize= 0.0f;
   c->fstrength= 0.0f;
   node->custom1= 2; /* green channel */
}

bNodeType cmp_node_color_spill={
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_COLOR_SPILL,
	/* name        */	"Color Spill",
	/* width+range */	140, 80, 200,
	/* class+opts  */	NODE_CLASS_MATTE, NODE_OPTIONS,
	/* input sock  */	cmp_node_color_spill_in,
	/* output sock */	cmp_node_color_spill_out,
	/* storage     */	"NodeChroma",
	/* execfunc    */	node_composit_exec_color_spill,
	/* butfunc     */	NULL,
	/* initfunc    */	node_composit_init_color_spill,
	/* freestoragefunc    */	node_free_standard_storage,
	/* copystoragefunc    */	node_copy_standard_storage,
	/* id          */	NULL
};

