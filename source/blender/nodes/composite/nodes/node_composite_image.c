/*
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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_image.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** IMAGE (and RenderResult, multilayer image) ******************** */

static bNodeSocketTemplate cmp_node_rlayers_out[]= {
	{	SOCK_RGBA, 0, "Image",					0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 0, "Alpha",					1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 0, "Z",						1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Normal",				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "UV",					1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Speed",				1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Color",					0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Diffuse",				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Specular",				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Shadow",					0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "AO",						0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Reflect",				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Refract",				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Indirect",				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 0, "IndexOB",				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 0, "IndexMA",				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 0, "Mist",					0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Emit",					0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Environment",			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Diffuse Direct",			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Diffuse Indirect",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Diffuse Color",			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Glossy Direct",			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Glossy Indirect",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Glossy Color",			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Transmission Direct",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Transmission Indirect",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Transmission Color",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocket *cmp_node_image_add_render_pass_output(bNodeTree *ntree, bNode *node, int UNUSED(pass), int rres_index)
{
	bNodeSocket *sock;
	
	sock = node_add_output_from_template(ntree, node, &cmp_node_rlayers_out[rres_index]);
	/* for render pass outputs store the pass type index as a lookup key */
	sock->storage = SET_INT_IN_POINTER(rres_index);
	
	return sock;
}

static void cmp_node_image_add_render_pass_outputs(bNodeTree *ntree, bNode *node, int passflag)
{
	if (passflag & SCE_PASS_COMBINED) {
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_COMBINED, RRES_OUT_IMAGE);
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_COMBINED, RRES_OUT_ALPHA);
	}
	
	if (passflag & SCE_PASS_Z)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_Z, RRES_OUT_Z);
	if (passflag & SCE_PASS_NORMAL)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_NORMAL, RRES_OUT_NORMAL);
	if (passflag & SCE_PASS_VECTOR)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_VECTOR, RRES_OUT_VEC);
	if (passflag & SCE_PASS_UV)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_UV, RRES_OUT_UV);
	if (passflag & SCE_PASS_RGBA)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_RGBA, RRES_OUT_RGBA);
	if (passflag & SCE_PASS_DIFFUSE)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_DIFFUSE, RRES_OUT_DIFF);
	if (passflag & SCE_PASS_SPEC)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_SPEC, RRES_OUT_SPEC);
	if (passflag & SCE_PASS_SHADOW)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_SHADOW, RRES_OUT_SHADOW);
	if (passflag & SCE_PASS_AO)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_AO, RRES_OUT_AO);
	if (passflag & SCE_PASS_REFLECT)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_REFLECT, RRES_OUT_REFLECT);
	if (passflag & SCE_PASS_REFRACT)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_REFRACT, RRES_OUT_REFRACT);
	if (passflag & SCE_PASS_INDIRECT)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_INDIRECT, RRES_OUT_INDIRECT);
	if (passflag & SCE_PASS_INDEXOB)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_INDEXOB, RRES_OUT_INDEXOB);
	if (passflag & SCE_PASS_INDEXMA)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_INDEXMA, RRES_OUT_INDEXMA);
	if (passflag & SCE_PASS_MIST)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_MIST, RRES_OUT_MIST);
	if (passflag & SCE_PASS_EMIT)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_EMIT, RRES_OUT_EMIT);
	if (passflag & SCE_PASS_ENVIRONMENT)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_ENVIRONMENT, RRES_OUT_ENV);
	
	if (passflag & SCE_PASS_DIFFUSE_DIRECT)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_DIFFUSE_DIRECT, RRES_OUT_DIFF_DIRECT);
	if (passflag & SCE_PASS_DIFFUSE_INDIRECT)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_DIFFUSE_INDIRECT, RRES_OUT_DIFF_INDIRECT);
	if (passflag & SCE_PASS_DIFFUSE_COLOR)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_DIFFUSE_COLOR, RRES_OUT_DIFF_COLOR);
	
	if (passflag & SCE_PASS_GLOSSY_DIRECT)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_GLOSSY_DIRECT, RRES_OUT_GLOSSY_DIRECT);
	if (passflag & SCE_PASS_GLOSSY_INDIRECT)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_GLOSSY_INDIRECT, RRES_OUT_GLOSSY_INDIRECT);
	if (passflag & SCE_PASS_GLOSSY_COLOR)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_GLOSSY_COLOR, RRES_OUT_GLOSSY_COLOR);
	
	if (passflag & SCE_PASS_TRANSM_DIRECT)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_TRANSM_DIRECT, RRES_OUT_TRANSM_DIRECT);
	if (passflag & SCE_PASS_TRANSM_INDIRECT)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_TRANSM_INDIRECT, RRES_OUT_TRANSM_INDIRECT);
	if (passflag & SCE_PASS_TRANSM_COLOR)
		cmp_node_image_add_render_pass_output(ntree, node, SCE_PASS_TRANSM_COLOR, RRES_OUT_TRANSM_COLOR);
}

static void cmp_node_image_add_multilayer_outputs(bNodeTree *ntree, bNode *node, RenderLayer *rl)
{
	bNodeSocket *sock;
	RenderPass *rpass;
	int index;
	for (rpass=rl->passes.first, index=0; rpass; rpass=rpass->next, ++index) {
		int type;
		if (rpass->channels == 1)
			type = SOCK_FLOAT;
		else
			type = SOCK_RGBA;
		
		sock = nodeAddSocket(ntree, node, SOCK_OUT, rpass->name, type);
		/* for multilayer image use pass index directly as key */
		sock->storage = SET_INT_IN_POINTER(index);
	}
}

static void cmp_node_image_create_outputs(bNodeTree *ntree, bNode *node)
{
	Image *ima= (Image *)node->id;
	if (ima) {
		ImageUser *iuser= node->storage;
		
		/* make sure ima->type is correct */
		BKE_image_get_ibuf(ima, iuser);
		
		if (ima->rr) {
			RenderLayer *rl= BLI_findlink(&ima->rr->layers, iuser->layer);
			
			if (rl) {
				if (ima->type!=IMA_TYPE_MULTILAYER)
					cmp_node_image_add_render_pass_outputs(ntree, node, rl->passflag);
				else
					cmp_node_image_add_multilayer_outputs(ntree, node, rl);
			}
			else
				cmp_node_image_add_render_pass_outputs(ntree, node, RRES_OUT_IMAGE|RRES_OUT_ALPHA);
		}
		else
			cmp_node_image_add_render_pass_outputs(ntree, node, RRES_OUT_IMAGE|RRES_OUT_ALPHA|RRES_OUT_Z);
	}
	else
		cmp_node_image_add_render_pass_outputs(ntree, node, RRES_OUT_IMAGE|RRES_OUT_ALPHA);
}

static bNodeSocket *cmp_node_image_output_find_match(bNode *UNUSED(node), bNodeSocket *newsock, ListBase *oldsocklist)
{
	bNodeSocket *sock;
	
	for (sock=oldsocklist->first; sock; sock=sock->next)
		if (strcmp(sock->name, newsock->name)==0)
			return sock;
	return NULL;
}

static bNodeSocket *cmp_node_image_output_relink(bNode *node, bNodeSocket *oldsock, int oldindex)
{
	bNodeSocket *sock;
	
	/* first try to find matching socket name */
	for (sock=node->outputs.first; sock; sock=sock->next)
		if (strcmp(sock->name, oldsock->name)==0)
			return sock;
	
	/* no matching name, simply link to same index */
	return BLI_findlink(&node->outputs, oldindex);
}

static void cmp_node_image_sync_output(bNode *UNUSED(node), bNodeSocket *UNUSED(newsock), bNodeSocket *UNUSED(oldsock))
{
	/* pass */
}

/* XXX make this into a generic socket verification function for dynamic socket replacement (multilayer, groups, static templates) */
static void cmp_node_image_verify_outputs(bNodeTree *ntree, bNode *node)
{
	bNodeSocket *newsock, *oldsock, *oldsock_next;
	ListBase oldsocklist;
	int oldindex;
	bNodeLink *link;
	
	/* store current nodes in oldsocklist, then clear socket list */
	oldsocklist = node->outputs;
	node->outputs.first = node->outputs.last = NULL;
	
	/* XXX make callback */
	cmp_node_image_create_outputs(ntree, node);
	/* flag all new sockets as dynamic, to prevent removal by socket verification function */
	for (newsock=node->outputs.first; newsock; newsock=newsock->next)
		newsock->flag |= SOCK_DYNAMIC;
	
	for (newsock=node->outputs.first; newsock; newsock=newsock->next) {
		/* XXX make callback */
		oldsock = cmp_node_image_output_find_match(node, newsock, &oldsocklist);
		if (oldsock) {
			/* XXX make callback */
			cmp_node_image_sync_output(node, newsock, oldsock);
		}
	}
	
	/* move links to new socket */
	for (oldsock=oldsocklist.first, oldindex=0; oldsock; oldsock=oldsock->next, ++oldindex) {
		newsock = cmp_node_image_output_relink(node, oldsock, oldindex);
		
		if (newsock) {
			for (link=ntree->links.first; link; link=link->next) {
				if (link->fromsock == oldsock)
					link->fromsock = newsock;
			}
		}
	}
	
	/* delete old sockets
	 * XXX oldsock is not actually in the node->outputs list any more,
	 * but the nodeRemoveSocket function works anyway. In future this
	 * should become part of the core code, so can take care of this behavior.
	 */
	for (oldsock=oldsocklist.first; oldsock; oldsock=oldsock_next) {
		oldsock_next = oldsock->next;
		nodeRemoveSocket(ntree, node, oldsock);
	}
}

static void cmp_node_image_update(bNodeTree *ntree, bNode *node)
{
	/* avoid unnecessary updates, only changes to the image/image user data are of interest */
	if (node->update & NODE_UPDATE_ID)
		cmp_node_image_verify_outputs(ntree, node);
}

/* float buffer from the image with matching color management */
float *node_composit_get_float_buffer(RenderData *rd, ImBuf *ibuf, int *alloc)
{
	float *rect;
	int predivide= (ibuf->flags & IB_cm_predivide);

	*alloc= FALSE;

	if (rd->color_mgt_flag & R_COLOR_MANAGEMENT) {
		if (ibuf->profile != IB_PROFILE_NONE) {
			rect= ibuf->rect_float;
		}
		else {
			rect= MEM_mapallocN(sizeof(float) * 4 * ibuf->x * ibuf->y, "node_composit_get_image");

			IMB_buffer_float_from_float(rect, ibuf->rect_float,
				4, IB_PROFILE_LINEAR_RGB, IB_PROFILE_SRGB, predivide,
				ibuf->x, ibuf->y, ibuf->x, ibuf->x);

			*alloc= TRUE;
		}
	}
	else {
		if (ibuf->profile == IB_PROFILE_NONE) {
			rect= ibuf->rect_float;
		}
		else {
			rect= MEM_mapallocN(sizeof(float) * 4 * ibuf->x * ibuf->y, "node_composit_get_image");

			IMB_buffer_float_from_float(rect, ibuf->rect_float,
				4, IB_PROFILE_SRGB, IB_PROFILE_LINEAR_RGB, predivide,
				ibuf->x, ibuf->y, ibuf->x, ibuf->x);

			*alloc= TRUE;
		}
	}

	return rect;
}

/* note: this function is used for multilayer too, to ensure uniform 
   handling with BKE_image_get_ibuf() */
static CompBuf *node_composit_get_image(RenderData *rd, Image *ima, ImageUser *iuser)
{
	ImBuf *ibuf;
	CompBuf *stackbuf;
	int type;

	float *rect;
	int alloc= FALSE;

	ibuf= BKE_image_get_ibuf(ima, iuser);
	if (ibuf==NULL || (ibuf->rect==NULL && ibuf->rect_float==NULL)) {
		return NULL;
	}

	if (ibuf->rect_float == NULL) {
		IMB_float_from_rect(ibuf);
	}

	/* now we need a float buffer from the image with matching color management */
	/* XXX weak code, multilayer is excluded from this */
	if (ibuf->channels == 4 && ima->rr==NULL) {
		rect= node_composit_get_float_buffer(rd, ibuf, &alloc);
	}
	else {
		/* non-rgba passes can't use color profiles */
		rect= ibuf->rect_float;
	}
	/* done coercing into the correct color management */


	type= ibuf->channels;
	
	if (rd->scemode & R_COMP_CROP) {
		stackbuf= get_cropped_compbuf(&rd->disprect, rect, ibuf->x, ibuf->y, type);
		if (alloc)
			MEM_freeN(rect);
	}
	else {
		/* we put imbuf copy on stack, cbuf knows rect is from other ibuf when freed! */
		stackbuf= alloc_compbuf(ibuf->x, ibuf->y, type, FALSE);
		stackbuf->rect= rect;
		stackbuf->malloc= alloc;
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
}

static CompBuf *node_composit_get_zimage(bNode *node, RenderData *rd)
{
	ImBuf *ibuf= BKE_image_get_ibuf((Image *)node->id, node->storage);
	CompBuf *zbuf= NULL;
	
	if (ibuf && ibuf->zbuf_float) {
		if (rd->scemode & R_COMP_CROP) {
			zbuf= get_cropped_compbuf(&rd->disprect, ibuf->zbuf_float, ibuf->x, ibuf->y, CB_VAL);
		}
		else {
			zbuf= alloc_compbuf(ibuf->x, ibuf->y, CB_VAL, 0);
			zbuf->rect= ibuf->zbuf_float;
		}
	}
	return zbuf;
}

/* check if layer is available, returns pass buffer */
static CompBuf *compbuf_multilayer_get(RenderData *rd, RenderLayer *rl, Image *ima, ImageUser *iuser, int passindex)
{
	RenderPass *rpass = BLI_findlink(&rl->passes, passindex);
	if (rpass) {
		CompBuf *cbuf;
		
		iuser->pass = passindex;
		BKE_image_multilayer_index(ima->rr, iuser);
		cbuf = node_composit_get_image(rd, ima, iuser);
		
		return cbuf;
	}
	return NULL;
}

static void node_composit_exec_image(void *data, bNode *node, bNodeStack **UNUSED(in), bNodeStack **out)
{
	
	/* image assigned to output */
	/* stack order input sockets: col, alpha */
	if (node->id) {
		RenderData *rd= data;
		Image *ima= (Image *)node->id;
		ImageUser *iuser= (ImageUser *)node->storage;
		
		/* first set the right frame number in iuser */
		BKE_image_user_calc_frame(iuser, rd->cfra, 0);
		
		/* force a load, we assume iuser index will be set OK anyway */
		if (ima->type==IMA_TYPE_MULTILAYER)
			BKE_image_get_ibuf(ima, iuser);
		
		if (ima->type==IMA_TYPE_MULTILAYER && ima->rr) {
			RenderLayer *rl= BLI_findlink(&ima->rr->layers, iuser->layer);
			
			if (rl) {
				bNodeSocket *sock;
				int out_index;
				CompBuf *combinedbuf= NULL, *firstbuf= NULL;
				
				for (sock=node->outputs.first, out_index=0; sock; sock=sock->next, ++out_index) {
					int passindex = GET_INT_FROM_POINTER(sock->storage);
					if (out[out_index]->hasoutput) {
						CompBuf *stackbuf = out[out_index]->data = compbuf_multilayer_get(rd, rl, ima, iuser, passindex);
						if (stackbuf) {
							/* preview policy: take first 'Combined' pass if available,
							 * otherwise just use the first layer.
							 */
							if (!firstbuf)
								firstbuf = stackbuf;
							if (!combinedbuf &&
							    (strcmp(sock->name, "Combined")==0 || strcmp(sock->name, "Image")==0))
								combinedbuf = stackbuf;
						}
					}
				}
				
				/* preview */
				if (combinedbuf)
					generate_preview(data, node, combinedbuf);
				else if (firstbuf)
					generate_preview(data, node, firstbuf);
			}
		}
		else {
			CompBuf *stackbuf = node_composit_get_image(rd, ima, iuser);
			if (stackbuf) {
				int num_outputs = BLI_countlist(&node->outputs);
				
				/*respect image premul option*/
				if (stackbuf->type==CB_RGBA && ima->flag & IMA_DO_PREMUL) {
					int i;
					float *pixel;
			
					/*first duplicate stackbuf->rect, since it's just a pointer
					  to the source imbuf, and we don't want to change that.*/
					stackbuf->rect = MEM_dupallocN(stackbuf->rect);
					
					/* since stackbuf now has allocated memory, rather than just a pointer,
					 * mark it as allocated so it can be freed properly */
					stackbuf->malloc=1;
					
					/*premul the image*/
					pixel = stackbuf->rect;
					for (i=0; i<stackbuf->x*stackbuf->y; i++, pixel += 4) {
						pixel[0] *= pixel[3];
						pixel[1] *= pixel[3];
						pixel[2] *= pixel[3];
					}
				}
			
				/* put image on stack */
				if (num_outputs > 0)
					out[0]->data= stackbuf;
				
				/* alpha output */
				if (num_outputs > 1 && out[1]->hasoutput)
					out[1]->data= valbuf_from_rgbabuf(stackbuf, CHAN_A);
				
				/* Z output */
				if (num_outputs > 2 && out[2]->hasoutput)
					out[2]->data= node_composit_get_zimage(node, rd);
				
				/* preview */
				generate_preview(data, node, stackbuf);
			}
		}
	}	
}

static void node_composit_init_image(bNodeTree *ntree, bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	ImageUser *iuser= MEM_callocN(sizeof(ImageUser), "node image user");
	node->storage= iuser;
	iuser->frames= 1;
	iuser->sfra= 1;
	iuser->fie_ima= 2;
	iuser->ok= 1;
	
	/* setup initial outputs */
	cmp_node_image_verify_outputs(ntree, node);
}

void register_node_type_cmp_image(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_IMAGE, "Image", NODE_CLASS_INPUT, NODE_PREVIEW|NODE_OPTIONS);
	node_type_size(&ntype, 120, 80, 300);
	node_type_init(&ntype, node_composit_init_image);
	node_type_storage(&ntype, "ImageUser", node_free_standard_storage, node_copy_standard_storage);
	node_type_update(&ntype, cmp_node_image_update, NULL);
	node_type_exec(&ntype, node_composit_exec_image);

	nodeRegisterType(ttype, &ntype);
}


/* **************** RENDER RESULT ******************** */

static CompBuf *compbuf_from_pass(RenderData *rd, RenderLayer *rl, int rectx, int recty, int passcode)
{
	float *fp= RE_RenderLayerGetPass(rl, passcode);
	if (fp) {
		CompBuf *buf;
		int buftype= CB_VEC3;

		if (ELEM4(passcode, SCE_PASS_Z, SCE_PASS_INDEXOB, SCE_PASS_MIST, SCE_PASS_INDEXMA))
			buftype= CB_VAL;
		else if (passcode==SCE_PASS_VECTOR)
			buftype= CB_VEC4;
		else if (ELEM(passcode, SCE_PASS_COMBINED, SCE_PASS_RGBA))
			buftype= CB_RGBA;

		if (rd->scemode & R_COMP_CROP)
			buf= get_cropped_compbuf(&rd->disprect, fp, rectx, recty, buftype);
		else {
			buf= alloc_compbuf(rectx, recty, buftype, 0);
			buf->rect= fp;
		}
		return buf;
	}
	return NULL;
}

static void node_composit_rlayers_out(RenderData *rd, RenderLayer *rl, bNodeStack **out, int rectx, int recty)
{
	if (out[RRES_OUT_Z]->hasoutput)
		out[RRES_OUT_Z]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_Z);
	if (out[RRES_OUT_VEC]->hasoutput)
		out[RRES_OUT_VEC]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_VECTOR);
	if (out[RRES_OUT_NORMAL]->hasoutput)
		out[RRES_OUT_NORMAL]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_NORMAL);
	if (out[RRES_OUT_UV]->hasoutput)
		out[RRES_OUT_UV]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_UV);

	if (out[RRES_OUT_RGBA]->hasoutput)
		out[RRES_OUT_RGBA]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_RGBA);
	if (out[RRES_OUT_DIFF]->hasoutput)
		out[RRES_OUT_DIFF]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_DIFFUSE);
	if (out[RRES_OUT_SPEC]->hasoutput)
		out[RRES_OUT_SPEC]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_SPEC);
	if (out[RRES_OUT_SHADOW]->hasoutput)
		out[RRES_OUT_SHADOW]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_SHADOW);
	if (out[RRES_OUT_AO]->hasoutput)
		out[RRES_OUT_AO]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_AO);
	if (out[RRES_OUT_REFLECT]->hasoutput)
		out[RRES_OUT_REFLECT]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_REFLECT);
	if (out[RRES_OUT_REFRACT]->hasoutput)
		out[RRES_OUT_REFRACT]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_REFRACT);
	if (out[RRES_OUT_INDIRECT]->hasoutput)
		out[RRES_OUT_INDIRECT]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_INDIRECT);
	if (out[RRES_OUT_INDEXOB]->hasoutput)
		out[RRES_OUT_INDEXOB]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_INDEXOB);
	if (out[RRES_OUT_INDEXMA]->hasoutput)
		out[RRES_OUT_INDEXMA]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_INDEXMA);
	if (out[RRES_OUT_MIST]->hasoutput)
		out[RRES_OUT_MIST]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_MIST);
	if (out[RRES_OUT_EMIT]->hasoutput)
		out[RRES_OUT_EMIT]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_EMIT);
	if (out[RRES_OUT_ENV]->hasoutput)
		out[RRES_OUT_ENV]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_ENVIRONMENT);
	if (out[RRES_OUT_DIFF_DIRECT]->hasoutput)
		out[RRES_OUT_DIFF_DIRECT]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_DIFFUSE_DIRECT);
	if (out[RRES_OUT_DIFF_INDIRECT]->hasoutput)
		out[RRES_OUT_DIFF_INDIRECT]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_DIFFUSE_INDIRECT);
	if (out[RRES_OUT_DIFF_COLOR]->hasoutput)
		out[RRES_OUT_DIFF_COLOR]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_DIFFUSE_COLOR);
	if (out[RRES_OUT_GLOSSY_DIRECT]->hasoutput)
		out[RRES_OUT_GLOSSY_DIRECT]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_GLOSSY_DIRECT);
	if (out[RRES_OUT_GLOSSY_INDIRECT]->hasoutput)
		out[RRES_OUT_GLOSSY_INDIRECT]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_GLOSSY_INDIRECT);
	if (out[RRES_OUT_GLOSSY_COLOR]->hasoutput)
		out[RRES_OUT_GLOSSY_COLOR]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_GLOSSY_COLOR);
	if (out[RRES_OUT_TRANSM_DIRECT]->hasoutput)
		out[RRES_OUT_TRANSM_DIRECT]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_TRANSM_DIRECT);
	if (out[RRES_OUT_TRANSM_INDIRECT]->hasoutput)
		out[RRES_OUT_TRANSM_INDIRECT]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_TRANSM_INDIRECT);
	if (out[RRES_OUT_TRANSM_COLOR]->hasoutput)
		out[RRES_OUT_TRANSM_COLOR]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_TRANSM_COLOR);
}



static void node_composit_exec_rlayers(void *data, bNode *node, bNodeStack **UNUSED(in), bNodeStack **out)
{
	Scene *sce= (Scene *)node->id;
	Render *re= (sce)? RE_GetRender(sce->id.name): NULL;
	RenderData *rd= data;
	RenderResult *rr= NULL;

	if (re)
		rr= RE_AcquireResultRead(re);

	if (rr) {
		SceneRenderLayer *srl= BLI_findlink(&sce->r.layers, node->custom1);
		if (srl) {
			RenderLayer *rl= RE_GetRenderLayer(rr, srl->name);
			if (rl && rl->rectf) {
				CompBuf *stackbuf;

				/* we put render rect on stack, cbuf knows rect is from other ibuf when freed! */
				if (rd->scemode & R_COMP_CROP)
					stackbuf= get_cropped_compbuf(&rd->disprect, rl->rectf, rr->rectx, rr->recty, CB_RGBA);
				else {
					stackbuf= alloc_compbuf(rr->rectx, rr->recty, CB_RGBA, 0);
					stackbuf->rect= rl->rectf;
				}
				if (stackbuf==NULL) {
					printf("Error; Preview Panel in UV Window returns zero sized image\n");
				}
				else {
					stackbuf->xof= rr->xof;
					stackbuf->yof= rr->yof;

					/* put on stack */
					out[RRES_OUT_IMAGE]->data= stackbuf;

					if (out[RRES_OUT_ALPHA]->hasoutput)
						out[RRES_OUT_ALPHA]->data= valbuf_from_rgbabuf(stackbuf, CHAN_A);

					node_composit_rlayers_out(rd, rl, out, rr->rectx, rr->recty);

					generate_preview(data, node, stackbuf);
				}
			}
		}
	}

	if (re)
		RE_ReleaseResult(re);
}


void register_node_type_cmp_rlayers(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_R_LAYERS, "Render Layers", NODE_CLASS_INPUT, NODE_PREVIEW|NODE_OPTIONS);
	node_type_socket_templates(&ntype, NULL, cmp_node_rlayers_out);
	node_type_size(&ntype, 150, 100, 300);
	node_type_exec(&ntype, node_composit_exec_rlayers);

	nodeRegisterType(ttype, &ntype);
}
