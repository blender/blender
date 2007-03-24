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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../SHD_util.h"

/* **************** MAPPING  ******************** */
static bNodeSocketType sh_node_mapping_in[]= {
	{	SOCK_VECTOR, 1, "Vector",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType sh_node_mapping_out[]= {
	{	SOCK_VECTOR, 0, "Vector",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

/* do the regular mapping options for blender textures */
static void node_shader_exec_mapping(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	TexMapping *texmap= node->storage;
	float *vec= out[0]->vec;
	
	/* stack order input:  vector */
	/* stack order output: vector */
	nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	Mat4MulVecfl(texmap->mat, vec);
	
	if(texmap->flag & TEXMAP_CLIP_MIN) {
		if(vec[0]<texmap->min[0]) vec[0]= texmap->min[0];
		if(vec[1]<texmap->min[1]) vec[1]= texmap->min[1];
		if(vec[2]<texmap->min[2]) vec[2]= texmap->min[2];
	}
	if(texmap->flag & TEXMAP_CLIP_MAX) {
		if(vec[0]>texmap->max[0]) vec[0]= texmap->max[0];
		if(vec[1]>texmap->max[1]) vec[1]= texmap->max[1];
		if(vec[2]>texmap->max[2]) vec[2]= texmap->max[2];
	}
}

static int node_shader_buts_mapping(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
   if(block) {
      TexMapping *texmap= node->storage;
      short dx= (short)((butr->xmax-butr->xmin)/7.0f);
      short dy= (short)(butr->ymax-19);

      uiBlockSetFunc(block, node_texmap_cb, texmap, NULL);	/* all buttons get this */

      uiBlockBeginAlign(block);
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+dx, dy, 2*dx, 19, texmap->loc, -1000.0f, 1000.0f, 10, 2, "");
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+3*dx, dy, 2*dx, 19, texmap->loc+1, -1000.0f, 1000.0f, 10, 2, "");
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+5*dx, dy, 2*dx, 19, texmap->loc+2, -1000.0f, 1000.0f, 10, 2, "");
      dy-= 19;
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+dx, dy, 2*dx, 19, texmap->rot, -1000.0f, 1000.0f, 1000, 1, "");
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+3*dx, dy, 2*dx, 19, texmap->rot+1, -1000.0f, 1000.0f, 1000, 1, "");
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+5*dx, dy, 2*dx, 19, texmap->rot+2, -1000.0f, 1000.0f, 1000, 1, "");
      dy-= 19;
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+dx, dy, 2*dx, 19, texmap->size, -1000.0f, 1000.0f, 10, 2, "");
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+3*dx, dy, 2*dx, 19, texmap->size+1, -1000.0f, 1000.0f, 10, 2, "");
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+5*dx, dy, 2*dx, 19, texmap->size+2, -1000.0f, 1000.0f, 10, 2, "");
      dy-= 25;
      uiBlockBeginAlign(block);
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+dx, dy, 2*dx, 19, texmap->min, -10.0f, 10.0f, 100, 2, "");
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+3*dx, dy, 2*dx, 19, texmap->min+1, -10.0f, 10.0f, 100, 2, "");
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+5*dx, dy, 2*dx, 19, texmap->min+2, -10.0f, 10.0f, 100, 2, "");
      dy-= 19;
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+dx, dy, 2*dx, 19, texmap->max, -10.0f, 10.0f, 10, 2, "");
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+3*dx, dy, 2*dx, 19, texmap->max+1, -10.0f, 10.0f, 10, 2, "");
      uiDefButF(block, NUM, B_NODE_EXEC+node->nr, "", butr->xmin+5*dx, dy, 2*dx, 19, texmap->max+2, -10.0f, 10.0f, 10, 2, "");
      uiBlockEndAlign(block);

      /* labels/options */

      dy= (short)(butr->ymax-19);
      uiDefBut(block, LABEL, B_NOP, "Loc", butr->xmin, dy, dx, 19, NULL, 0.0f, 0.0f, 0, 0, "");
      dy-= 19;
      uiDefBut(block, LABEL, B_NOP, "Rot", butr->xmin, dy, dx, 19, NULL, 0.0f, 0.0f, 0, 0, "");
      dy-= 19;
      uiDefBut(block, LABEL, B_NOP, "Size", butr->xmin, dy, dx, 19, NULL, 0.0f, 0.0f, 0, 0, "");
      dy-= 25;
      uiDefButBitI(block, TOG, TEXMAP_CLIP_MIN, B_NODE_EXEC+node->nr, "Min", butr->xmin, dy, dx-4, 19, &texmap->flag, 0.0f, 0.0f, 0, 0, "");
      dy-= 19;
      uiDefButBitI(block, TOG, TEXMAP_CLIP_MAX, B_NODE_EXEC+node->nr, "Max", butr->xmin, dy, dx-4, 19, &texmap->flag, 0.0f, 0.0f, 0, 0, "");

   }	
   return 5*19 + 6;
}

static void node_shader_init_mapping(bNode *node)
{
   node->storage= add_mapping();
}

bNodeType sh_node_mapping= {
	/* type code   */	SH_NODE_MAPPING,
	/* name        */	"Mapping",
	/* width+range */	240, 160, 320,
	/* class+opts  */	NODE_CLASS_OP_VECTOR, NODE_OPTIONS,
	/* input sock  */	sh_node_mapping_in,
	/* output sock */	sh_node_mapping_out,
	/* storage     */	"TexMapping",
	/* execfunc    */	node_shader_exec_mapping,
   /* butfunc     */ node_shader_buts_mapping,
   /* initfunc    */ node_shader_init_mapping
	
};

