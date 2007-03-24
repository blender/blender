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

/* **************** OUTPUT FILE ******************** */
static bNodeSocketType cmp_node_output_file_in[]= {
	{	SOCK_RGBA, 1, "Image",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Z",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_output_file(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* image assigned to output */
	/* stack order input sockets: col, alpha */
	
	if(in[0]->data) {
		RenderData *rd= data;
		NodeImageFile *nif= node->storage;
		if(nif->sfra!=nif->efra && (rd->cfra<nif->sfra || rd->cfra>nif->efra)) {
			return;	/* BAIL OUT RETURN */
		}
		else {
			CompBuf *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
			ImBuf *ibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0, 0);
			char string[256];
			
			ibuf->rect_float= cbuf->rect;
			ibuf->dither= rd->dither_intensity;
			if(in[1]->data) {
				CompBuf *zbuf= in[1]->data;
				if(zbuf->type==CB_VAL && zbuf->x==cbuf->x && zbuf->y==cbuf->y) {
					nif->subimtype|= R_OPENEXR_ZBUF;
					ibuf->zbuf_float= zbuf->rect;
				}
			}
			
			BKE_makepicstring(string, nif->name, rd->cfra, nif->imtype);
			
			if(0 == BKE_write_ibuf(ibuf, string, nif->imtype, nif->subimtype, nif->imtype==R_OPENEXR?nif->codec:nif->quality))
				printf("Cannot save Node File Output to %s\n", string);
			else
				printf("Saved: %s\n", string);
			
			IMB_freeImBuf(ibuf);	
			
			generate_preview(node, cbuf);
			
			if(in[0]->data != cbuf) 
				free_compbuf(cbuf);
		}
	}
}



/* allocate sufficient! */
static void node_imagetype_string(char *str)
{
   str += sprintf(str, "Save Image as: %%t|");
   str += sprintf(str, "Targa %%x%d|", R_TARGA);
   str += sprintf(str, "Targa Raw %%x%d|", R_RAWTGA);
   str += sprintf(str, "PNG %%x%d|", R_PNG);
   str += sprintf(str, "BMP %%x%d|", R_BMP);
   str += sprintf(str, "Jpeg %%x%d|", R_JPEG90);
   str += sprintf(str, "Iris %%x%d|", R_IRIS);
   str += sprintf(str, "Radiance HDR %%x%d|", R_RADHDR);
   str += sprintf(str, "Cineon %%x%d|", R_CINEON);
   str += sprintf(str, "DPX %%x%d|", R_DPX);
   str += sprintf(str, "OpenEXR %%x%d", R_OPENEXR);
}

static int node_composit_buts_output_file(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
   if(block) {
      NodeImageFile *nif= node->storage;
      short x= (short)butr->xmin;
      short y= (short)butr->ymin;
      short w= (short)butr->xmax-butr->xmin;
      char str[320];

      node_imagetype_string(str);

      uiBlockBeginAlign(block);

      uiDefBut(block, TEX, B_NOP, "",
         x, y+60, w, 20, 
         nif->name, 0.0f, 240.0f, 0, 0, "");

      uiDefButS(block, MENU, B_NOP, str,
         x, y+40, w, 20, 
         &nif->imtype, 0.0f, 1.0f, 0, 0, "");

      if(nif->imtype==R_OPENEXR) {
         uiDefButBitS(block, TOG, R_OPENEXR_HALF, B_REDR, "Half",	
            x, y+20, w/2, 20, 
            &nif->subimtype, 0, 0, 0, 0, "");

         uiDefButS(block, MENU,B_NOP, "Codec %t|None %x0|Pxr24 (lossy) %x1|ZIP (lossless) %x2|PIZ (lossless) %x3|RLE (lossless) %x4",  
            x+w/2, y+20, w/2, 20, 
            &nif->codec, 0, 0, 0, 0, "");
      }
      else {
         uiDefButS(block, NUM, B_NOP, "Quality: ",
            x, y+20, w, 20, 
            &nif->quality, 10.0f, 100.0f, 10, 0, "");
      }

      /* start frame, end frame */
      uiDefButI(block, NUM, B_NODE_EXEC+node->nr, "SFra: ", 
         x, y, w/2, 20, 
         &nif->sfra, 1, MAXFRAMEF, 10, 0, "");
      uiDefButI(block, NUM, B_NODE_EXEC+node->nr, "EFra: ", 
         x+w/2, y, w/2, 20, 
         &nif->efra, 1, MAXFRAMEF, 10, 0, "");

   }
   return 80;
}

static void node_composit_init_output_file(bNode *node)
{
   NodeImageFile *nif= MEM_callocN(sizeof(NodeImageFile), "node image file");
   node->storage= nif;
   BLI_strncpy(nif->name, G.scene->r.pic, sizeof(nif->name));
   nif->imtype= G.scene->r.imtype;
   nif->subimtype= G.scene->r.subimtype;
   nif->quality= G.scene->r.quality;
   nif->sfra= G.scene->r.sfra;
   nif->efra= G.scene->r.efra;
};

bNodeType cmp_node_output_file= {
	/* type code   */	CMP_NODE_OUTPUT_FILE,
	/* name        */	"File Output",
	/* width+range */	140, 80, 300,
	/* class+opts  */	NODE_CLASS_OUTPUT, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */	cmp_node_output_file_in,
	/* output sock */	NULL,
	/* storage     */	"NodeImageFile",
	/* execfunc    */	node_composit_exec_output_file,
   /* butfunc     */ node_composit_buts_output_file,
                     node_composit_init_output_file
	
};


