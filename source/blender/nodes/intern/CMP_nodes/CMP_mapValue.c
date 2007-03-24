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

/* **************** MAP VALUE ******************** */
static bNodeSocketType cmp_node_map_value_in[]= {
	{	SOCK_VALUE, 1, "Value",			1.0f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_map_value_out[]= {
	{	SOCK_VALUE, 0, "Value",			1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_map_value(bNode *node, float *out, float *src)
{
	TexMapping *texmap= node->storage;
	
	out[0]= (src[0] + texmap->loc[0])*texmap->size[0];
	if(texmap->flag & TEXMAP_CLIP_MIN)
		if(out[0]<texmap->min[0])
			out[0]= texmap->min[0];
	if(texmap->flag & TEXMAP_CLIP_MAX)
		if(out[0]>texmap->max[0])
			out[0]= texmap->max[0];
}

static void node_composit_exec_map_value(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: valbuf */
	/* stack order out: valbuf */
	if(out[0]->hasoutput==0) return;
	
	/* input no image? then only value operation */
	if(in[0]->data==NULL) {
		do_map_value(node, out[0]->vec, in[0]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1); /* allocs */
		
		composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_map_value, CB_VAL);
		
		out[0]->data= stackbuf;
	}
}

static int node_composit_buts_map_value(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
   if(block) {
      TexMapping *texmap= node->storage;
      short xstart= (short)butr->xmin;
      short dy= (short)(butr->ymax-19.0f);
      short dx= (short)(butr->xmax-butr->xmin)/2;

      uiBlockBeginAlign(block);
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Offs:", xstart, dy, 2*dx, 19, texmap->loc, -1000.0f, 1000.0f, 10, 2, "");
      dy-= 19;
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "Size:", xstart, dy, 2*dx, 19, texmap->size, -1000.0f, 1000.0f, 10, 3, "");
      dy-= 23;
      uiBlockBeginAlign(block);
      uiDefButBitI(block, TOG, TEXMAP_CLIP_MIN, B_NODE_EXEC+node->nr, "Min", xstart, dy, dx, 19, &texmap->flag, 0.0f, 0.0f, 0, 0, "");
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", xstart+dx, dy, dx, 19, texmap->min, -1000.0f, 1000.0f, 10, 2, "");
      dy-= 19;
      uiDefButBitI(block, TOG, TEXMAP_CLIP_MAX, B_NODE_EXEC+node->nr, "Max", xstart, dy, dx, 19, &texmap->flag, 0.0f, 0.0f, 0, 0, "");
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", xstart+dx, dy, dx, 19, texmap->max, -1000.0f, 1000.0f, 10, 2, "");
   }
   return 80;
};

static void node_composit_init_map_value(bNode* node)
{
   node->storage= add_mapping();
}

bNodeType cmp_node_map_value= {
	/* type code   */	CMP_NODE_MAP_VALUE,
	/* name        */	"Map Value",
	/* width+range */	100, 60, 150,
	/* class+opts  */	NODE_CLASS_OP_VECTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_map_value_in,
	/* output sock */	cmp_node_map_value_out,
	/* storage     */	"TexMapping",
	/* execfunc    */	node_composit_exec_map_value,
   /* butfunc     */ node_composit_buts_map_value,
                     node_composit_init_map_value
	
};



