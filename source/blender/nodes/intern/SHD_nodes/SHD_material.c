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

/* **************** MATERIAL ******************** */

static bNodeSocketType sh_node_material_in[]= {
	{	SOCK_RGBA, 1, "Color",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Spec",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Refl",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 1, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

/* output socket defines */
#define MAT_OUT_COLOR	0
#define MAT_OUT_ALPHA	1
#define MAT_OUT_NORMAL	2

static bNodeSocketType sh_node_material_out[]= {
	{	SOCK_RGBA, 0, "Color",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_material(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(data && node->id) {
		ShadeResult shrnode;
		ShadeInput *shi;
		ShaderCallData *shcd= data;
		float col[4];
		
		shi= shcd->shi;
		shi->mat= (Material *)node->id;
		
		/* copy all relevant material vars, note, keep this synced with render_types.h */
		memcpy(&shi->r, &shi->mat->r, 23*sizeof(float));
		shi->har= shi->mat->har;
		
		/* write values */
		if(in[MAT_IN_COLOR]->hasinput)
			nodestack_get_vec(&shi->r, SOCK_VECTOR, in[MAT_IN_COLOR]);
		
		if(in[MAT_IN_SPEC]->hasinput)
			nodestack_get_vec(&shi->specr, SOCK_VECTOR, in[MAT_IN_SPEC]);
		
		if(in[MAT_IN_REFL]->hasinput)
			nodestack_get_vec(&shi->refl, SOCK_VALUE, in[MAT_IN_REFL]);
		
		/* retrieve normal */
		if(in[MAT_IN_NORMAL]->hasinput) {
			nodestack_get_vec(shi->vn, SOCK_VECTOR, in[MAT_IN_NORMAL]);
			Normalise(shi->vn);
		}
		else
			VECCOPY(shi->vn, shi->vno);
		
		/* custom option to flip normal */
		if(node->custom1 & SH_NODE_MAT_NEG) {
			shi->vn[0]= -shi->vn[0];
			shi->vn[1]= -shi->vn[1];
			shi->vn[2]= -shi->vn[2];
		}
		
		node_shader_lamp_loop(shi, &shrnode);	/* clears shrnode */
		
		/* write to outputs */
		if(node->custom1 & SH_NODE_MAT_DIFF) {
			VECCOPY(col, shrnode.combined);
			if(!(node->custom1 & SH_NODE_MAT_SPEC)) {
				VecSubf(col, col, shrnode.spec);
			}
		}
		else if(node->custom1 & SH_NODE_MAT_SPEC) {
			VECCOPY(col, shrnode.spec);
		}
		else
			col[0]= col[1]= col[2]= 0.0f;
		
		col[3]= shrnode.alpha;
		
		if(shi->do_preview)
			nodeAddToPreview(node, col, shi->xs, shi->ys);
		
		VECCOPY(out[MAT_OUT_COLOR]->vec, col);
		out[MAT_OUT_ALPHA]->vec[0]= shrnode.alpha;
		
		if(node->custom1 & SH_NODE_MAT_NEG) {
			shi->vn[0]= -shi->vn[0];
			shi->vn[1]= -shi->vn[1];
			shi->vn[2]= -shi->vn[2];
		}
		
		VECCOPY(out[MAT_OUT_NORMAL]->vec, shi->vn);
		
		/* copy passes, now just active node */
		if(node->flag & NODE_ACTIVE_ID)
			*(shcd->shr)= shrnode;
	}
}

static void node_mat_alone_cb(void *node_v, void *unused)
{
   bNode *node= node_v;

   node->id= (ID *)copy_material((Material *)node->id);

   BIF_undo_push("Single user material");
   allqueue(REDRAWBUTSSHADING, 0);
   allqueue(REDRAWNODE, 0);
   allqueue(REDRAWOOPS, 0);
}

static void node_browse_mat_cb(void *ntree_v, void *node_v)
{
   bNodeTree *ntree= ntree_v;
   bNode *node= node_v;

   if(node->menunr<1) return;

   if(node->menunr==32767) {	/* code for Add New */
      if(node->id) {
         /* make copy, but make sure it doesnt have the node tag nor nodes */
         Material *ma= (Material *)node->id;
         ma->id.us--;
         ma= copy_material(ma);
         ma->use_nodes= 0;
         if(ma->nodetree) {
            ntreeFreeTree(ma->nodetree);
            MEM_freeN(ma->nodetree);
         }
         ma->nodetree= NULL;
         node->id= (ID *)ma;
      }
      else node->id= (ID *)add_material("MatNode");
   }
   else {
      if(node->id) node->id->us--;
      node->id= BLI_findlink(&G.main->mat, node->menunr-1);
      id_us_plus(node->id);
   }
   BLI_strncpy(node->name, node->id->name+2, 21);

   nodeSetActive(ntree, node);

   allqueue(REDRAWBUTSSHADING, 0);
   allqueue(REDRAWNODE, 0);
   BIF_preview_changed(ID_MA);

   node->menunr= 0;
}

static void node_new_mat_cb(void *ntree_v, void *node_v)
{
   bNodeTree *ntree= ntree_v;
   bNode *node= node_v;

   node->id= (ID *)add_material("MatNode");
   BLI_strncpy(node->name, node->id->name+2, 21);

   nodeSetActive(ntree, node);

   allqueue(REDRAWBUTSSHADING, 0);
   allqueue(REDRAWNODE, 0);
   BIF_preview_changed(ID_MA);

}
static int node_shader_buts_material(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
   if(block) {
      uiBut *bt;
      short dx= (short)((butr->xmax-butr->xmin)/3.0f), has_us= (node->id && node->id->us>1);
      short dy= (short)butr->ymin;
      char *strp;

      /* WATCH IT: we use this callback in material buttons, but then only want first row */
      if(butr->ymax-butr->ymin > 21.0f) dy+= 19;

      uiBlockBeginAlign(block);
      if(node->id==NULL) uiBlockSetCol(block, TH_REDALERT);
      else if(has_us) uiBlockSetCol(block, TH_BUT_SETTING1);
      else uiBlockSetCol(block, TH_BUT_SETTING2);

      /* browse button */
      IDnames_to_pupstring(&strp, NULL, "ADD NEW %x32767", &(G.main->mat), NULL, NULL);
      node->menunr= 0;
      bt= uiDefButS(block, MENU, B_NOP, strp, 
         butr->xmin, dy, 19, 19, 
         &node->menunr, 0, 0, 0, 0, "Browses existing choices or adds NEW");
      uiButSetFunc(bt, node_browse_mat_cb, ntree, node);
      if(strp) MEM_freeN(strp);

      /* Add New button */
      if(node->id==NULL) {
         bt= uiDefBut(block, BUT, B_NOP, "Add New",
            butr->xmin+19, dy, (short)(butr->xmax-butr->xmin-19.0f), 19, 
            NULL, 0.0, 0.0, 0, 0, "Add new Material");
         uiButSetFunc(bt, node_new_mat_cb, ntree, node);
         uiBlockSetCol(block, TH_AUTO);
      }
      else {
         /* name button */
         short width= (short)(butr->xmax-butr->xmin-19.0f - (has_us?19.0f:0.0f));
         bt= uiDefBut(block, TEX, B_NOP, "MA:",
            butr->xmin+19, dy, width, 19, 
            node->id->name+2, 0.0, 19.0, 0, 0, "Material name");
         uiButSetFunc(bt, node_ID_title_cb, node, NULL);

         /* user amount */
         if(has_us) {
            char str1[32];
            sprintf(str1, "%d", node->id->us);
            bt= uiDefBut(block, BUT, B_NOP, str1, 
               butr->xmax-19, dy, 19, 19, 
               NULL, 0, 0, 0, 0, "Displays number of users. Click to make a single-user copy.");
            uiButSetFunc(bt, node_mat_alone_cb, node, NULL);
         }

         /* WATCH IT: we use this callback in material buttons, but then only want first row */
         if(butr->ymax-butr->ymin > 21.0f) {
            /* node options */
            uiBlockSetCol(block, TH_AUTO);
            uiDefButBitS(block, TOG, SH_NODE_MAT_DIFF, B_NODE_EXEC+node->nr, "Diff",
               butr->xmin, butr->ymin, dx, 19, 
               &node->custom1, 0, 0, 0, 0, "Material Node outputs Diffuse");
            uiDefButBitS(block, TOG, SH_NODE_MAT_SPEC, B_NODE_EXEC+node->nr, "Spec",
               butr->xmin+dx, butr->ymin, dx, 19, 
               &node->custom1, 0, 0, 0, 0, "Material Node outputs Specular");
            uiDefButBitS(block, TOG, SH_NODE_MAT_NEG, B_NODE_EXEC+node->nr, "Neg Normal",
               butr->xmax-dx, butr->ymin, dx, 19,
               &node->custom1, 0, 0, 0, 0, "Material Node uses inverted Normal");
         }
      }
      uiBlockEndAlign(block);
   }	
   return 38;
}

static void node_shader_init_material(bNode* node)
{
   node->custom1= SH_NODE_MAT_DIFF|SH_NODE_MAT_SPEC;
}


bNodeType sh_node_material= {
	/* type code   */	SH_NODE_MATERIAL,
	/* name        */	"Material",
	/* width+range */	120, 80, 240,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_OPTIONS|NODE_PREVIEW,
	/* input sock  */	sh_node_material_in,
	/* output sock */	sh_node_material_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_material,
   /* butfunc     */ node_shader_buts_material,
   /* initfunc    */ node_shader_init_material
	
};

