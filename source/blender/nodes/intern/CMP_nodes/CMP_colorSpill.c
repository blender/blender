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
 * Contributor(s): Bob Holcomb, Xavier Thomas
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include "../CMP_util.h"

#define avg(a,b) ((a+b)/2)

/* ******************* Color Spill Supression ********************************* */
static bNodeSocketType cmp_node_color_spill_in[]={
	{SOCK_RGBA,1,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static bNodeSocketType cmp_node_color_spill_out[]={
	{SOCK_RGBA,0,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static void do_simple_spillmap_red(bNode *node, float* out, float *in)
{
   NodeColorspill *ncs;
	ncs=node->storage;
	out[0]=in[0]-( ncs->limscale * in[ncs->limchan] );
}

static void do_simple_spillmap_green(bNode *node, float* out, float *in)
{
	NodeColorspill *ncs;
	ncs=node->storage;
	out[0]=in[1]-( ncs->limscale * in[ncs->limchan] );
}

static void do_simple_spillmap_blue(bNode *node, float* out, float *in)
{
	NodeColorspill *ncs;
	ncs=node->storage;
	out[0]=in[2]-( ncs->limscale * in[ncs->limchan] );
}

static void do_average_spillmap_red(bNode *node, float* out, float *in)
{
	NodeColorspill *ncs;
	ncs=node->storage;
	out[0]=in[0]-(ncs->limscale * avg(in[1], in[2]) );
}

static void do_average_spillmap_green(bNode *node, float* out, float *in)
{
	NodeColorspill *ncs;
	ncs=node->storage;
	out[0]=in[1]-(ncs->limscale * avg(in[0], in[2]) );
}

static void do_average_spillmap_blue(bNode *node, float* out, float *in)
{
	NodeColorspill *ncs;
	ncs=node->storage;
	out[0]=in[2]-(ncs->limscale * avg(in[0], in[1]) );
}

static void do_apply_spillmap_red(bNode *node, float* out, float *in, float *map)
{	
	NodeColorspill *ncs;
	ncs=node->storage;
	if(map[0]>0) {
		out[0]=in[0]-(ncs->uspillr*map[0]);
		out[1]=in[1]+(ncs->uspillg*map[0]);
		out[2]=in[2]+(ncs->uspillb*map[0]);
	}
	else {
		out[0]=in[0];
		out[1]=in[1];
		out[2]=in[2];
	}
}

static void do_apply_spillmap_green(bNode *node, float* out, float *in, float *map)
{
	NodeColorspill *ncs;
	ncs=node->storage;
	if(map[0]>0) {
		out[0]=in[0]+(ncs->uspillr*map[0]);
		out[1]=in[1]-(ncs->uspillg*map[0]);
		out[2]=in[2]+(ncs->uspillb*map[0]);
	 }
	else {
		out[0]=in[0];
		out[1]=in[1];
		out[2]=in[2];
	}
}

static void do_apply_spillmap_blue(bNode *node, float* out, float *in, float *map)
{
	NodeColorspill *ncs;
	ncs=node->storage;
	if(map[0]>0) {
		out[0]=in[0]+(ncs->uspillr*map[0]);
		out[1]=in[1]+(ncs->uspillg*map[0]);
		out[2]=in[2]-(ncs->uspillb*map[0]);
	 }
	else {
		out[0]=in[0];
		out[1]=in[1];
		out[2]=in[2];
	}
}

static void node_composit_exec_color_spill(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/*
	Originally based on the information from the book "The Art and Science of Digital Composition" and 
	discussions from vfxtalk.com.*/
	CompBuf *cbuf;
	CompBuf *rgbbuf;
	CompBuf *spillmap;
	NodeColorspill *ncs;
	ncs=node->storage;
	
	if(out[0]->hasoutput==0 || in[0]->hasinput==0) return;
	if(in[0]->data==NULL) return;
	
	cbuf=typecheck_compbuf(in[0]->data, CB_RGBA);
	spillmap=alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1);
	rgbbuf=dupalloc_compbuf(cbuf);

	switch(node->custom1)
	{
	case 1:  /*red spill*/
		{
			switch(node->custom2)
			{
				case 0: /* simple limit */
				{
					composit1_pixel_processor(node, spillmap, cbuf, in[0]->vec, do_simple_spillmap_red, CB_RGBA);
					break;
				}
				case 1: /* average limit */
				{
					composit1_pixel_processor(node, spillmap, cbuf, in[0]->vec, do_average_spillmap_red, CB_RGBA);
					break;
				}
			}
			if(ncs->unspill==0) {
				ncs->uspillr=1.0f;
				ncs->uspillg=0.0f;
				ncs->uspillb=0.0f;
			}
			composit2_pixel_processor(node, rgbbuf, cbuf, in[0]->vec, spillmap, NULL, do_apply_spillmap_red, CB_RGBA, CB_VAL);
			break;
		}
	case 2: /*green spill*/
		{
			switch(node->custom2)
			{
				case 0: /* simple limit */
				{
					composit1_pixel_processor(node, spillmap, cbuf, in[0]->vec, do_simple_spillmap_green, CB_RGBA);
					break;
				}
				case 1: /* average limit */
				{
					composit1_pixel_processor(node, spillmap, cbuf, in[0]->vec, do_average_spillmap_green, CB_RGBA);
					break;
				}
			}
			if(ncs->unspill==0) {
				ncs->uspillr=0.0f;
				ncs->uspillg=1.0f;
				ncs->uspillb=0.0f;
			}
			composit2_pixel_processor(node, rgbbuf, cbuf, in[0]->vec, spillmap, NULL, do_apply_spillmap_green, CB_RGBA, CB_VAL);
			break;
		}
	case 3: /*blue spill*/
		{
			switch(node->custom2)
			{
				case 0: /* simple limit */
				{
					composit1_pixel_processor(node, spillmap, cbuf, in[0]->vec, do_simple_spillmap_blue, CB_RGBA);
					break;
				}
				case 1: /* average limit */
				{
					composit1_pixel_processor(node, spillmap, cbuf, in[0]->vec, do_average_spillmap_blue, CB_RGBA);
					break;
				}
			}
			if(ncs->unspill==0) {
				ncs->uspillr=0.0f;
				ncs->uspillg=0.0f;
				ncs->uspillb=1.0f;
			}
			composit2_pixel_processor(node, rgbbuf, cbuf, in[0]->vec, spillmap, NULL, do_apply_spillmap_blue, CB_RGBA, CB_VAL);
			break;
		}
	default:
		break;
	}

	out[0]->data=rgbbuf;

	if(cbuf!=in[0]->data)
		free_compbuf(cbuf);
   
   free_compbuf(spillmap);
}

static void node_composit_init_color_spill(bNode *node)
{
   NodeColorspill *ncs= MEM_callocN(sizeof(NodeColorspill), "node colorspill");
   node->storage=ncs;
   node->custom1= 2; /* green channel */
   node->custom2= 0; /* simple limit algo*/
   ncs->limchan= 0;  /* limit by red */
   ncs->limscale= 1.0f; /* limit scaling factor */
   ncs->unspill=0;   /* do not use unspill */
}

bNodeType cmp_node_color_spill={
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_COLOR_SPILL,
	/* name        */	"Color Spill",
	/* width+range */	140, 80, 200,
	/* class+opts  */	NODE_CLASS_MATTE, NODE_OPTIONS,
	/* input sock  */	cmp_node_color_spill_in,
	/* output sock */	cmp_node_color_spill_out,
	/* storage     */	"NodeColorspill",
	/* execfunc    */	node_composit_exec_color_spill,
	/* butfunc     */	NULL,
	/* initfunc    */	node_composit_init_color_spill,
	/* freestoragefunc    */	node_free_standard_storage,
	/* copystoragefunc    */	node_copy_standard_storage,
	/* id          */	NULL
};

