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
 * Contributor(s): Bob Holcomb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../CMP_util.h"

/* ******************* Color Key ********************************************************** */
static bNodeSocketType cmp_node_color_in[]={
	{SOCK_RGBA,1,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{SOCK_RGBA,1,"Key Color", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static bNodeSocketType cmp_node_color_out[]={
	{SOCK_RGBA,0,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{SOCK_VALUE,0,"Matte",0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static void do_color_key(bNode *node, float *out, float *in)
{
	NodeChroma *c;
	c=node->storage;


   VECCOPY(out, in);

   if(fabs(in[0]-c->key[0]) < c->t1 &&
      fabs(in[1]-c->key[1]) < c->t2 &&
      fabs(in[2]-c->key[2]) < c->t3) 
   {
      out[3]=0.0; /*make transparent*/
   }

	else { /*pixel is outside key color */
		out[3]=in[3]; /* make pixel just as transparent as it was before */
	}
}

static void node_composit_exec_color_matte(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *cbuf;
	CompBuf *colorbuf;
	NodeChroma *c;
	
	if(in[0]->hasinput==0) return;
	if(in[0]->data==NULL) return;
	if(out[0]->hasoutput==0 && out[1]->hasoutput==0) return;
	
	cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
	
	colorbuf= dupalloc_compbuf(cbuf);
	
	c=node->storage;
	
	/*convert rgbbuf to hsv*/
	composit1_pixel_processor(node, colorbuf, cbuf, in[0]->vec, do_rgba_to_hsva, CB_RGBA);
	
   /*convert key to hsv*/
	do_rgba_to_hsva(node, c->key, in[1]->vec);
	

	/*per pixel color key*/
	composit1_pixel_processor(node, colorbuf, colorbuf, in[0]->vec, do_color_key, CB_RGBA);
	
	/*convert back*/
	composit1_pixel_processor(node, colorbuf, colorbuf, in[0]->vec, do_hsva_to_rgba, CB_RGBA);
	
	out[0]->data= colorbuf;
	if(out[1]->hasoutput)
		out[1]->data= valbuf_from_rgbabuf(colorbuf, CHAN_A);
	
	generate_preview(data, node, colorbuf);

	if(cbuf!=in[0]->data)
		free_compbuf(cbuf);
};

static void node_composit_init_color_matte(bNode *node)
{
   NodeChroma *c= MEM_callocN(sizeof(NodeChroma), "node color");
   node->storage= c;
   c->t1= 0.01f;
   c->t2= 0.1f;
   c->t3= 0.1f;
   c->fsize= 0.0f;
   c->fstrength= 1.0f;
};

bNodeType cmp_node_color_matte={
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_COLOR_MATTE,
	/* name        */	"Color Key",
	/* width+range */	200, 80, 300,
	/* class+opts  */	NODE_CLASS_MATTE, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */	cmp_node_color_in,
	/* output sock */	cmp_node_color_out,
	/* storage     */	"NodeChroma",
	/* execfunc    */	node_composit_exec_color_matte,
	/* butfunc     */	NULL,
	/* initfunc    */	node_composit_init_color_matte,
	/* freestoragefunc    */	node_free_standard_storage,
	/* copystoragefunc    */	node_copy_standard_storage,
	/* id          */	NULL
};


