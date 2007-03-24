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


/* **************** IMAGE (and RenderResult, multilayer image) ******************** */

static bNodeSocketType cmp_node_rlayers_out[]= {
	{	SOCK_RGBA, 0, "Image",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Z",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Normal",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "UV",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Speed",	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Color",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Diffuse",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Specular",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Shadow",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "AO",			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Reflect",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Refract",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Radio",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "IndexOB",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};


/* note: this function is used for multilayer too, to ensure uniform 
   handling with BKE_image_get_ibuf() */
static CompBuf *node_composit_get_image(RenderData *rd, Image *ima, ImageUser *iuser)
{
	ImBuf *ibuf;
	CompBuf *stackbuf;
	int type;
	
	ibuf= BKE_image_get_ibuf(ima, iuser);
	if(ibuf==NULL)
		return NULL;
	
	if(ibuf->rect_float==NULL)
		IMB_float_from_rect(ibuf);
	
	type= ibuf->channels;
	
	if(rd->scemode & R_COMP_CROP) {
		stackbuf= get_cropped_compbuf(&rd->disprect, ibuf->rect_float, ibuf->x, ibuf->y, type);
	}
	else {
		/* we put imbuf copy on stack, cbuf knows rect is from other ibuf when freed! */
		stackbuf= alloc_compbuf(ibuf->x, ibuf->y, type, 0);
		stackbuf->rect= ibuf->rect_float;
	}
	
	return stackbuf;
};

static CompBuf *node_composit_get_zimage(bNode *node, RenderData *rd)
{
	ImBuf *ibuf= BKE_image_get_ibuf((Image *)node->id, node->storage);
	CompBuf *zbuf= NULL;
	
	if(ibuf && ibuf->zbuf_float) {
		if(rd->scemode & R_COMP_CROP) {
			zbuf= get_cropped_compbuf(&rd->disprect, ibuf->zbuf_float, ibuf->x, ibuf->y, CB_VAL);
		}
		else {
			zbuf= alloc_compbuf(ibuf->x, ibuf->y, CB_VAL, 0);
			zbuf->rect= ibuf->zbuf_float;
		}
	}
	return zbuf;
};

/* check if layer is available, returns pass buffer */
static CompBuf *compbuf_multilayer_get(RenderData *rd, RenderLayer *rl, Image *ima, ImageUser *iuser, int passtype)
{
	RenderPass *rpass;
	short index;
	
	for(index=0, rpass= rl->passes.first; rpass; rpass= rpass->next, index++)
		if(rpass->passtype==passtype)
			break;
	
	if(rpass) {
		CompBuf *cbuf;
		
		iuser->pass= index;
		BKE_image_multilayer_index(ima->rr, iuser);
		cbuf= node_composit_get_image(rd, ima, iuser);
		
		return cbuf;
	}
	return NULL;
};

void outputs_multilayer_get(RenderData *rd, RenderLayer *rl, bNodeStack **out, Image *ima, ImageUser *iuser)
{
	if(out[RRES_OUT_Z]->hasoutput)
		out[RRES_OUT_Z]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_Z);
	if(out[RRES_OUT_VEC]->hasoutput)
		out[RRES_OUT_VEC]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_VECTOR);
	if(out[RRES_OUT_NORMAL]->hasoutput)
		out[RRES_OUT_NORMAL]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_NORMAL);
	if(out[RRES_OUT_UV]->hasoutput)
		out[RRES_OUT_UV]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_UV);
	
	if(out[RRES_OUT_RGBA]->hasoutput)
		out[RRES_OUT_RGBA]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_RGBA);
	if(out[RRES_OUT_DIFF]->hasoutput)
		out[RRES_OUT_DIFF]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_DIFFUSE);
	if(out[RRES_OUT_SPEC]->hasoutput)
		out[RRES_OUT_SPEC]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_SPEC);
	if(out[RRES_OUT_SHADOW]->hasoutput)
		out[RRES_OUT_SHADOW]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_SHADOW);
	if(out[RRES_OUT_AO]->hasoutput)
		out[RRES_OUT_AO]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_AO);
	if(out[RRES_OUT_REFLECT]->hasoutput)
		out[RRES_OUT_REFLECT]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_REFLECT);
	if(out[RRES_OUT_REFRACT]->hasoutput)
		out[RRES_OUT_REFRACT]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_REFRACT);
	if(out[RRES_OUT_RADIO]->hasoutput)
		out[RRES_OUT_RADIO]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_RADIO);
	if(out[RRES_OUT_INDEXOB]->hasoutput)
		out[RRES_OUT_INDEXOB]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_INDEXOB);
	
};


static void node_composit_exec_image(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	
	/* image assigned to output */
	/* stack order input sockets: col, alpha */
	if(node->id) {
		RenderData *rd= data;
		Image *ima= (Image *)node->id;
		ImageUser *iuser= (ImageUser *)node->storage;
		CompBuf *stackbuf= NULL;
		
		/* first set the right frame number in iuser */
		BKE_image_user_calc_imanr(iuser, rd->cfra, 0);
		
		/* force a load, we assume iuser index will be set OK anyway */
		if(ima->type==IMA_TYPE_MULTILAYER)
			BKE_image_get_ibuf(ima, iuser);
		
		if(ima->type==IMA_TYPE_MULTILAYER && ima->rr) {
			RenderLayer *rl= BLI_findlink(&ima->rr->layers, iuser->layer);
			
			if(rl) {
				out[0]->data= stackbuf= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_COMBINED);
				
				/* go over all layers */
				outputs_multilayer_get(rd, rl, out, ima, iuser);
			}
		}
		else {
			stackbuf= node_composit_get_image(rd, ima, iuser);

			/* put image on stack */	
			out[0]->data= stackbuf;
			
			if(out[2]->hasoutput)
				out[2]->data= node_composit_get_zimage(node, rd);
		}
		
		/* alpha and preview for both types */
		if(stackbuf) {
			if(out[1]->hasoutput)
				out[1]->data= valbuf_from_rgbabuf(stackbuf, CHAN_A);

			generate_preview(node, stackbuf);
		}
	}	
};

static void node_browse_image_cb(void *ntree_v, void *node_v)
{
   bNodeTree *ntree= ntree_v;
   bNode *node= node_v;

   nodeSetActive(ntree, node);

   if(node->menunr<1) return;
   if(node->menunr==32767) {	/* code for Load New */
      addqueue(curarea->win, UI_BUT_EVENT, B_NODE_LOADIMAGE);
   }
   else {
      if(node->id) node->id->us--;
      node->id= BLI_findlink(&G.main->image, node->menunr-1);
      id_us_plus(node->id);

      BLI_strncpy(node->name, node->id->name+2, 21);

      NodeTagChanged(ntree, node); 
      BKE_image_signal((Image *)node->id, node->storage, IMA_SIGNAL_USER_NEW_IMAGE);
      addqueue(curarea->win, UI_BUT_EVENT, B_NODE_EXEC+node->nr);
   }
   node->menunr= 0;
};

static void node_active_cb(void *ntree_v, void *node_v)
{
   nodeSetActive(ntree_v, node_v);
};

static void node_image_type_cb(void *node_v, void *unused)
{

   allqueue(REDRAWNODE, 1);
};

static char *node_image_type_pup(void)
{
   char *str= MEM_mallocN(256, "image type pup");
   int a;

   str[0]= 0;

   a= sprintf(str, "Image Type %%t|");
   a+= sprintf(str+a, "  Image %%x%d %%i%d|", IMA_SRC_FILE, ICON_IMAGE_DEHLT);
   a+= sprintf(str+a, "  Movie %%x%d %%i%d|", IMA_SRC_MOVIE, ICON_SEQUENCE);
   a+= sprintf(str+a, "  Sequence %%x%d %%i%d|", IMA_SRC_SEQUENCE, ICON_IMAGE_COL);
   a+= sprintf(str+a, "  Generated %%x%d %%i%d", IMA_SRC_GENERATED, ICON_BLANK1);

   return str;
};

/* copy from buttons_shading.c */
static char *layer_menu(RenderResult *rr)
{
   RenderLayer *rl;
   int len= 40 + 40*BLI_countlist(&rr->layers);
   short a, nr;
   char *str= MEM_callocN(len, "menu layers");

   strcpy(str, "Layer %t");
   a= strlen(str);
   for(nr=0, rl= rr->layers.first; rl; rl= rl->next, nr++) {
      a+= sprintf(str+a, "|%s %%x%d", rl->name, nr);
   }

   return str;
};

static void image_layer_cb(void *ima_v, void *iuser_v)
{

   ntreeCompositForceHidden(G.scene->nodetree);
   BKE_image_multilayer_index(ima_v, iuser_v);
   allqueue(REDRAWNODE, 0);
};

static int node_composit_buts_image(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
   ImageUser *iuser= node->storage;

   if(block) {
      uiBut *bt;
      short dy= (short)butr->ymax-19;
      char *strp;

      uiBlockBeginAlign(block);
      uiBlockSetCol(block, TH_BUT_SETTING2);

      /* browse button */
      IMAnames_to_pupstring(&strp, NULL, "LOAD NEW %x32767", &(G.main->image), NULL, NULL);
      node->menunr= 0;
      bt= uiDefButS(block, MENU, B_NOP, strp, 
         butr->xmin, dy, 19, 19, 
         &node->menunr, 0, 0, 0, 0, "Browses existing choices");
      uiButSetFunc(bt, node_browse_image_cb, ntree, node);
      if(strp) MEM_freeN(strp);

      /* Add New button */
      if(node->id==NULL) {
         bt= uiDefBut(block, BUT, B_NODE_LOADIMAGE, "Load New",
            butr->xmin+19, dy, (short)(butr->xmax-butr->xmin-19.0f), 19, 
            NULL, 0.0, 0.0, 0, 0, "Add new Image");
         uiButSetFunc(bt, node_active_cb, ntree, node);
         uiBlockSetCol(block, TH_AUTO);
      }
      else {
         /* name button + type */
         Image *ima= (Image *)node->id;
         short xmin= (short)butr->xmin, xmax= (short)butr->xmax;
         short width= xmax - xmin - 45;
         short icon= ICON_IMAGE_DEHLT;

         if(ima->source==IMA_SRC_MOVIE) icon= ICON_SEQUENCE;
         else if(ima->source==IMA_SRC_SEQUENCE) icon= ICON_IMAGE_COL;
         else if(ima->source==IMA_SRC_GENERATED) icon= ICON_BLANK1;

         bt= uiDefBut(block, TEX, B_NOP, "IM:",
            xmin+19, dy, width, 19, 
            node->id->name+2, 0.0, 19.0, 0, 0, "Image name");
         uiButSetFunc(bt, node_ID_title_cb, node, NULL);

         /* buffer type option */
         strp= node_image_type_pup();
         bt= uiDefIconTextButS(block, MENU, B_NOP, icon, strp,
            xmax-26, dy, 26, 19, 
            &ima->source, 0.0, 19.0, 0, 0, "Image type");
         uiButSetFunc(bt, node_image_type_cb, node, ima);
         MEM_freeN(strp);

         if( ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE) ) {
            width= (xmax-xmin)/2;

            dy-= 19;
            uiDefButI(block, NUM, B_NODE_EXEC+node->nr, "Frs:",
               xmin, dy, width, 19, 
               &iuser->frames, 0.0, 10000.0, 0, 0, "Amount of images used in animation");
            uiDefButI(block, NUM, B_NODE_EXEC+node->nr, "SFra:",
               xmin+width, dy, width, 19, 
               &iuser->sfra, 1.0, 10000.0, 0, 0, "Start frame of animation");
            dy-= 19;
            uiDefButI(block, NUM, B_NODE_EXEC+node->nr, "Offs:",
               xmin, dy, width, 19, 
               &iuser->offset, 0.0, 10000.0, 0, 0, "Offsets the number of the frame to use in the animation");
            uiDefButS(block, TOG, B_NODE_EXEC+node->nr, "Cycl",
               xmin+width, dy, width-20, 19, 
               &iuser->cycl, 0.0, 0.0, 0, 0, "Make animation go cyclic");
            uiDefIconButBitS(block, TOG, IMA_ANIM_ALWAYS, B_NODE_EXEC+node->nr, ICON_AUTO,
               xmax-20, dy, 20, 19, 
               &iuser->flag, 0.0, 0.0, 0, 0, "Always refresh Image on frame changes");
         }
         if( ima->type==IMA_TYPE_MULTILAYER && ima->rr) {
            RenderLayer *rl= BLI_findlink(&ima->rr->layers, iuser->layer);
            if(rl) {
               width= (xmax-xmin);
               dy-= 19;
               strp= layer_menu(ima->rr);
               bt= uiDefButS(block, MENU, B_NODE_EXEC+node->nr, strp,
                  xmin, dy, width, 19, 
                  &iuser->layer, 0.0, 10000.0, 0, 0, "Layer");
               uiButSetFunc(bt, image_layer_cb, ima, node->storage);
               MEM_freeN(strp);
            }
         }
      }

   }	
   if(node->id) {
      Image *ima= (Image *)node->id;
      int retval= 19;

      /* for each draw we test for anim refresh event */
      if(iuser->flag & IMA_ANIM_REFRESHED) {
         iuser->flag &= ~IMA_ANIM_REFRESHED;
         addqueue(curarea->win, UI_BUT_EVENT, B_NODE_EXEC+node->nr);
      }

      if( ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE) )
         retval+= 38;
      if( ima->type==IMA_TYPE_MULTILAYER)
         retval+= 19;
      return retval;
   }
   else
      return 19;
};

static void node_composit_init_image(bNode* node)
{
   ImageUser *iuser= MEM_callocN(sizeof(ImageUser), "node image user");
   node->storage= iuser;
   iuser->sfra= 1;
   iuser->fie_ima= 2;
   iuser->ok= 1;
}

bNodeType cmp_node_image= {
   /* type code   */	CMP_NODE_IMAGE,
   /* name        */	"Image",
   /* width+range */	120, 80, 300,
   /* class+opts  */	NODE_CLASS_INPUT, NODE_PREVIEW|NODE_OPTIONS,
   /* input sock  */	NULL,
   /* output sock */	cmp_node_rlayers_out,
   /* storage     */	"ImageUser",
   /* execfunc    */	node_composit_exec_image,
   /* butfunc     */ node_composit_buts_image,
                     node_composit_init_image
};

/* **************** RENDER RESULT ******************** */

static CompBuf *compbuf_from_pass(RenderData *rd, RenderLayer *rl, int rectx, int recty, int passcode)
{
   float *fp= RE_RenderLayerGetPass(rl, passcode);
   if(fp) {
      CompBuf *buf;
      int buftype= CB_VEC3;

      if(ELEM(passcode, SCE_PASS_Z, SCE_PASS_INDEXOB))
         buftype= CB_VAL;
      else if(passcode==SCE_PASS_VECTOR)
         buftype= CB_VEC4;
      else if(ELEM(passcode, SCE_PASS_COMBINED, SCE_PASS_RGBA))
         buftype= CB_RGBA;

      if(rd->scemode & R_COMP_CROP)
         buf= get_cropped_compbuf(&rd->disprect, fp, rectx, recty, buftype);
      else {
         buf= alloc_compbuf(rectx, recty, buftype, 0);
         buf->rect= fp;
      }
      return buf;
   }
   return NULL;
};

void node_composit_rlayers_out(RenderData *rd, RenderLayer *rl, bNodeStack **out, int rectx, int recty)
{
   if(out[RRES_OUT_Z]->hasoutput)
      out[RRES_OUT_Z]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_Z);
   if(out[RRES_OUT_VEC]->hasoutput)
      out[RRES_OUT_VEC]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_VECTOR);
   if(out[RRES_OUT_NORMAL]->hasoutput)
      out[RRES_OUT_NORMAL]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_NORMAL);
   if(out[RRES_OUT_UV]->hasoutput)
      out[RRES_OUT_UV]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_UV);

   if(out[RRES_OUT_RGBA]->hasoutput)
      out[RRES_OUT_RGBA]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_RGBA);
   if(out[RRES_OUT_DIFF]->hasoutput)
      out[RRES_OUT_DIFF]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_DIFFUSE);
   if(out[RRES_OUT_SPEC]->hasoutput)
      out[RRES_OUT_SPEC]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_SPEC);
   if(out[RRES_OUT_SHADOW]->hasoutput)
      out[RRES_OUT_SHADOW]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_SHADOW);
   if(out[RRES_OUT_AO]->hasoutput)
      out[RRES_OUT_AO]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_AO);
   if(out[RRES_OUT_REFLECT]->hasoutput)
      out[RRES_OUT_REFLECT]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_REFLECT);
   if(out[RRES_OUT_REFRACT]->hasoutput)
      out[RRES_OUT_REFRACT]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_REFRACT);
   if(out[RRES_OUT_RADIO]->hasoutput)
      out[RRES_OUT_RADIO]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_RADIO);
   if(out[RRES_OUT_INDEXOB]->hasoutput)
      out[RRES_OUT_INDEXOB]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_INDEXOB);

};

static void node_composit_exec_rlayers(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
   Scene *sce= node->id?(Scene *)node->id:G.scene; /* G.scene is WEAK! */
   RenderData *rd= data;
   RenderResult *rr;

   rr= RE_GetResult(RE_GetRender(sce->id.name));

   if(rr) {
      SceneRenderLayer *srl= BLI_findlink(&sce->r.layers, node->custom1);
      if(srl) {
         RenderLayer *rl= RE_GetRenderLayer(rr, srl->name);
         if(rl && rl->rectf) {
            CompBuf *stackbuf;

            /* we put render rect on stack, cbuf knows rect is from other ibuf when freed! */
            if(rd->scemode & R_COMP_CROP)
               stackbuf= get_cropped_compbuf(&rd->disprect, rl->rectf, rr->rectx, rr->recty, CB_RGBA);
            else {
               stackbuf= alloc_compbuf(rr->rectx, rr->recty, CB_RGBA, 0);
               stackbuf->rect= rl->rectf;
            }
            if(stackbuf==NULL) {
               printf("Error; Preview Panel in UV Window returns zero sized image\n");
            }
            else {
               stackbuf->xof= rr->xof;
               stackbuf->yof= rr->yof;

               /* put on stack */	
               out[RRES_OUT_IMAGE]->data= stackbuf;

               if(out[RRES_OUT_ALPHA]->hasoutput)
                  out[RRES_OUT_ALPHA]->data= valbuf_from_rgbabuf(stackbuf, CHAN_A);

               node_composit_rlayers_out(rd, rl, out, rr->rectx, rr->recty);

               generate_preview(node, stackbuf);
            }
         }
      }
   }	
};

/* if we use render layers from other scene, we make a nice title */
static void set_render_layers_title(void *node_v, void *unused)
{
   bNode *node= node_v;
   Scene *sce;
   SceneRenderLayer *srl;
   char str[64];

   if(node->id) {
      BLI_strncpy(str, node->id->name+2, 21);
      strcat(str, "|");
      sce= (Scene *)node->id;
   }
   else {
      str[0]= 0;
      sce= G.scene;
   }
   srl= BLI_findlink(&sce->r.layers, node->custom1);
   if(srl==NULL) {
      node->custom1= 0;
      srl= sce->r.layers.first;
   }

   strcat(str, srl->name);
   BLI_strncpy(node->name, str, 32);
};

static char *scene_layer_menu(Scene *sce)
{
   SceneRenderLayer *srl;
   int len= 40 + 40*BLI_countlist(&sce->r.layers);
   short a, nr;
   char *str= MEM_callocN(len, "menu layers");

   strcpy(str, "Active Layer %t");
   a= strlen(str);
   for(nr=0, srl= sce->r.layers.first; srl; srl= srl->next, nr++) {
      a+= sprintf(str+a, "|%s %%x%d", srl->name, nr);
   }

   return str;
};

static void node_browse_scene_cb(void *ntree_v, void *node_v)
{
   bNodeTree *ntree= ntree_v;
   bNode *node= node_v;
   Scene *sce;

   if(node->menunr<1) return;

   if(node->id) {
      node->id->us--;
      node->id= NULL;
   }
   sce= BLI_findlink(&G.main->scene, node->menunr-1);
   if(sce!=G.scene) {
      node->id= &sce->id;
      id_us_plus(node->id);
   }

   set_render_layers_title(node, NULL);
   nodeSetActive(ntree, node);

   allqueue(REDRAWBUTSSHADING, 0);
   allqueue(REDRAWNODE, 0);
   NodeTagChanged(ntree, node); 

   node->menunr= 0;
};

static int node_composit_buts_renderlayers(uiBlock *block, bNodeTree *ntree, bNode *node, rctf *butr)
{
   if(block) {
      uiBut *bt;
      char *strp;

      /* browse button scene */
      uiBlockBeginAlign(block);
      IDnames_to_pupstring(&strp, NULL, "", &(G.main->scene), NULL, NULL);
      node->menunr= 0;
      bt= uiDefButS(block, MENU, B_NOP, strp, 
         butr->xmin, butr->ymin, 20, 19, 
         &node->menunr, 0, 0, 0, 0, "Browse Scene to use RenderLayer from");
      uiButSetFunc(bt, node_browse_scene_cb, ntree, node);
      if(strp) MEM_freeN(strp);

      /* browse button layer */
      strp= scene_layer_menu(node->id?(Scene *)node->id:G.scene);
      if(node->id)
         bt= uiDefIconTextButS(block, MENU, B_NODE_EXEC+node->nr, ICON_SCENE_DEHLT, strp, 
         butr->xmin+20, butr->ymin, (butr->xmax-butr->xmin)-40, 19, 
         &node->custom1, 0, 0, 0, 0, "Choose Render Layer");
      else
         bt= uiDefButS(block, MENU, B_NODE_EXEC+node->nr, strp, 
         butr->xmin+20, butr->ymin, (butr->xmax-butr->xmin)-40, 19, 
         &node->custom1, 0, 0, 0, 0, "Choose Render Layer");
      uiButSetFunc(bt, set_render_layers_title, node, NULL);
      MEM_freeN(strp);

      /* re-render */
      /* uses custom2, not the best implementation of the world... but we need it to work now :) */
      bt= uiDefIconButS(block, TOG, B_NODE_EXEC+node->nr, ICON_SCENE, 
         butr->xmax-20, butr->ymin, 20, 19, 
         &node->custom2, 0, 0, 0, 0, "Re-render this Layer");

   }
   return 19;
};


bNodeType cmp_node_rlayers= {
   /* type code   */	CMP_NODE_R_LAYERS,
   /* name        */	"Render Layers",
   /* width+range */	150, 100, 300,
   /* class+opts  */	NODE_CLASS_INPUT, NODE_PREVIEW|NODE_OPTIONS,
   /* input sock  */	NULL,
   /* output sock */	cmp_node_rlayers_out,
   /* storage     */	"",
   /* execfunc    */	node_composit_exec_rlayers,
   /* butfunc     */ node_composit_buts_renderlayers,
                     NULL

};


