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
	{	SOCK_VALUE, 0, "Mist",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
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

	if (!(rd->color_mgt_flag & R_COLOR_MANAGEMENT)) {
		int profile = IB_PROFILE_NONE;
		
		/* temporarily set profile to none to not disturb actual */
		SWAP(int, ibuf->profile, profile);
		
		if (ibuf->rect_float != NULL) {
			imb_freerectfloatImBuf(ibuf);
		}
		IMB_float_from_rect(ibuf);
		
		SWAP(int, ibuf->profile, profile);
	}
	
	if (ibuf->rect_float == NULL) {
		IMB_float_from_rect(ibuf);
	}

	type= ibuf->channels;
	
	if(rd->scemode & R_COMP_CROP) {
		stackbuf= get_cropped_compbuf(&rd->disprect, ibuf->rect_float, ibuf->x, ibuf->y, type);
	}
	else {
		/* we put imbuf copy on stack, cbuf knows rect is from other ibuf when freed! */
		stackbuf= alloc_compbuf(ibuf->x, ibuf->y, type, 0);
		stackbuf->rect= ibuf->rect_float;
	}
	
	/*code to respect the premul flag of images; I'm
	  not sure if this is a good idea for multilayer images,
	  since it never worked before for them.
	if (type==CB_RGBA && ima->flag & IMA_DO_PREMUL) {
		//premul the image
		int i;
		float *pixel = stackbuf->rect;
		
		for (i=0; i<stackbuf->x*stackbuf->y; i++, pixel += 4) {
			pixel[0] *= pixel[3];
			pixel[1] *= pixel[3];
			pixel[2] *= pixel[3];
		}
	}
	*/
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
	if(out[RRES_OUT_MIST]->hasoutput)
		out[RRES_OUT_MIST]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_MIST);
	
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
		BKE_image_user_calc_frame(iuser, rd->cfra, 0);
		
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

			if (stackbuf) {
				/*respect image premul option*/
				if (stackbuf->type==CB_RGBA && ima->flag & IMA_DO_PREMUL) {
					int i;
					float *pixel;
			
					/*first duplicate stackbuf->rect, since it's just a pointer
					  to the source imbuf, and we don't want to change that.*/
					stackbuf->rect = MEM_dupallocN(stackbuf->rect);
				
					/*premul the image*/
				
					pixel = stackbuf->rect;
					for (i=0; i<stackbuf->x*stackbuf->y; i++, pixel += 4) {
						pixel[0] *= pixel[3];
						pixel[1] *= pixel[3];
						pixel[2] *= pixel[3];
					}
				}
			
				/* put image on stack */	
				out[0]->data= stackbuf;
			
				if(out[2]->hasoutput)
					out[2]->data= node_composit_get_zimage(node, rd);
			}
		}
		
		/* alpha and preview for both types */
		if(stackbuf) {
			if(out[1]->hasoutput)
				out[1]->data= valbuf_from_rgbabuf(stackbuf, CHAN_A);

			generate_preview(data, node, stackbuf);
		}
	}	
};

static void node_composit_init_image(bNode* node)
{
   ImageUser *iuser= MEM_callocN(sizeof(ImageUser), "node image user");
   node->storage= iuser;
   iuser->frames= 1;
   iuser->sfra= 1;
   iuser->fie_ima= 2;
   iuser->ok= 1;
}

bNodeType cmp_node_image= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_IMAGE,
	/* name        */	"Image",
	/* width+range */	120, 80, 300,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	cmp_node_rlayers_out,
	/* storage     */	"ImageUser",
	/* execfunc    */	node_composit_exec_image,
	/* butfunc     */	NULL,
	/* initfunc    */	node_composit_init_image,
	/* freestoragefunc    */	node_free_standard_storage,
	/* copystoragefunc    */	node_copy_standard_storage,
	/* id          */	NULL
};

/* **************** RENDER RESULT ******************** */

static CompBuf *compbuf_from_pass(RenderData *rd, RenderLayer *rl, int rectx, int recty, int passcode)
{
   float *fp= RE_RenderLayerGetPass(rl, passcode);
   if(fp) {
      CompBuf *buf;
      int buftype= CB_VEC3;

      if(ELEM3(passcode, SCE_PASS_Z, SCE_PASS_INDEXOB, SCE_PASS_MIST))
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
   if(out[RRES_OUT_MIST]->hasoutput)
	   out[RRES_OUT_MIST]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_MIST);

};

static void node_composit_exec_rlayers(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
   Scene *sce= (Scene *)node->id;
   Render *re= (sce)? RE_GetRender(sce->id.name, RE_SLOT_RENDERING): NULL;
   RenderData *rd= data;
   RenderResult *rr= NULL;

   if(re)
	   rr= RE_AcquireResultRead(re);

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

               generate_preview(data, node, stackbuf);
            }
         }
      }
   }

   if(re)
	   RE_ReleaseResult(re);
};


bNodeType cmp_node_rlayers= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_R_LAYERS,
	/* name        */	"Render Layers",
	/* width+range */	150, 100, 300,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	cmp_node_rlayers_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_rlayers,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL

};


