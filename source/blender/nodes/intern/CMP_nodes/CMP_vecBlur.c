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


/* **************** VECTOR BLUR ******************** */
static bNodeSocketType cmp_node_vecblur_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Z",			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 1, "Speed",			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_vecblur_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};



static void node_composit_exec_vecblur(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	NodeBlurData *nbd= node->storage;
	CompBuf *new, *img= in[0]->data, *vecbuf= in[2]->data, *zbuf= in[1]->data;
	
	if(img==NULL || vecbuf==NULL || zbuf==NULL || out[0]->hasoutput==0)
		return;
	if(vecbuf->x!=img->x || vecbuf->y!=img->y) {
		printf("ERROR: cannot do different sized vecbuf yet\n");
		return;
	}
	if(vecbuf->type!=CB_VEC4) {
		printf("ERROR: input should be vecbuf\n");
		return;
	}
	if(zbuf->type!=CB_VAL) {
		printf("ERROR: input should be zbuf\n");
		return;
	}
	if(zbuf->x!=img->x || zbuf->y!=img->y) {
		printf("ERROR: cannot do different sized zbuf yet\n");
		return;
	}
	
	/* allow the input image to be of another type */
	img= typecheck_compbuf(in[0]->data, CB_RGBA);

	new= dupalloc_compbuf(img);
	
	/* call special zbuffer version */
	RE_zbuf_accumulate_vecblur(nbd, img->x, img->y, new->rect, img->rect, vecbuf->rect, zbuf->rect);
	
	out[0]->data= new;
	
	if(img!=in[0]->data)
		free_compbuf(img);
}

static int node_composit_buts_vecblur(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
   if(block) {
      NodeBlurData *nbd= node->storage;
      short dy= butr->ymin;
      short dx= (butr->xmax-butr->xmin);

      uiBlockBeginAlign(block);
      uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "Samples:",
         butr->xmin, dy+57, dx, 19, 
         &nbd->samples, 1, 256, 0, 0, "Amount of samples");
      uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "MinSpeed:",
         butr->xmin, dy+38, dx, 19, 
         &nbd->minspeed, 0, 1024, 0, 0, "Minimum speed for a pixel to be blurred, used to separate background from foreground");
      uiDefButS(block, NUM, B_NODE_EXEC+node->nr, "MaxSpeed:",
         butr->xmin, dy+19, dx, 19, 
         &nbd->maxspeed, 0, 1024, 0, 0, "If not zero, maximum speed in pixels");
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "BlurFac:",
         butr->xmin, dy, dx, 19, 
         &nbd->fac, 0.0f, 2.0f, 10, 2, "Scaling factor for motion vectors, actually 'shutter speed' in frames");
   }
   return 76;
}

static void node_composit_init_vecblur(bNode* node)
{
   NodeBlurData *nbd= MEM_callocN(sizeof(NodeBlurData), "node blur data");
   node->storage= nbd;
   nbd->samples= 32;
   nbd->fac= 1.0f;
};


/* custom1: itterations, custom2: maxspeed (0 = nolimit) */
bNodeType cmp_node_vecblur= {
	/* type code   */	CMP_NODE_VECBLUR,
	/* name        */	"Vector Blur",
	/* width+range */	120, 80, 200,
	/* class+opts  */	NODE_CLASS_OP_FILTER, NODE_OPTIONS,
	/* input sock  */	cmp_node_vecblur_in,
	/* output sock */	cmp_node_vecblur_out,
	/* storage     */	"NodeBlurData",
   /* execfunc    */	node_composit_exec_vecblur,
   /* butfunc     */ node_composit_buts_vecblur,
                     node_composit_init_vecblur	
};
