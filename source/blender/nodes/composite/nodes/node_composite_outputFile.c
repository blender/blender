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

/** \file blender/nodes/composite/nodes/node_composite_outputFile.c
 *  \ingroup cmpnodes
 */


#include <string.h>
#include "BLI_path_util.h"

#include "BKE_utildefines.h"

#include "node_composite_util.h"

/* **************** OUTPUT FILE ******************** */
static bNodeSocketTemplate cmp_node_output_file_in[]= {
	{	SOCK_RGBA, 1, "Image",		0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 1, "Z",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{	-1, 0, ""	}
};

static void node_composit_exec_output_file(void *data, bNode *node, bNodeStack **in, bNodeStack **UNUSED(out))
{
	/* image assigned to output */
	/* stack order input sockets: col, alpha */
	
	if(in[0]->data) {
		RenderData *rd= data;
		NodeImageFile *nif= node->storage;
		if(nif->sfra!=nif->efra && (rd->cfra<nif->sfra || rd->cfra>nif->efra)) {
			return;	/* BAIL OUT RETURN */
		}
		else if (!G.rendering) {
			/* only output files when rendering a sequence -
			 * otherwise, it overwrites the output files just 
			 * scrubbing through the timeline when the compositor updates */
			return;
		} else {
			Main *bmain= G.main; /* TODO, have this passed along */
			CompBuf *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
			ImBuf *ibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0);
			char string[FILE_MAX];
			
			ibuf->rect_float= cbuf->rect;
			ibuf->dither= rd->dither_intensity;
			
			if (rd->color_mgt_flag & R_COLOR_MANAGEMENT)
				ibuf->profile = IB_PROFILE_LINEAR_RGB;
			
			if(in[1]->data) {
				CompBuf *zbuf= in[1]->data;
				if(zbuf->type==CB_VAL && zbuf->x==cbuf->x && zbuf->y==cbuf->y) {
					nif->im_format.flag |= R_IMF_FLAG_ZBUF;
					ibuf->zbuf_float= zbuf->rect;
				}
			}
			
			BKE_makepicstring(string, nif->name, bmain->name, rd->cfra, nif->im_format.imtype, (rd->scemode & R_EXTENSION), TRUE);
			
			if(0 == BKE_write_ibuf(ibuf, string, &nif->im_format))
				printf("Cannot save Node File Output to %s\n", string);
			else
				printf("Saved: %s\n", string);
			
			IMB_freeImBuf(ibuf);	
			
			generate_preview(data, node, cbuf);
			
			if(in[0]->data != cbuf) 
				free_compbuf(cbuf);
		}
	}
}

static void node_composit_mute_output_file(void *UNUSED(data), int UNUSED(thread),
										   struct bNode *UNUSED(node), void *UNUSED(nodedata),
										   struct bNodeStack **UNUSED(in), struct bNodeStack **UNUSED(out))
{
	/* nothing to do here */
}

static void node_composit_init_output_file(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *ntemp)
{
	RenderData *rd = &ntemp->scene->r;
	NodeImageFile *nif= MEM_callocN(sizeof(NodeImageFile), "node image file");
	node->storage= nif;

	BLI_strncpy(nif->name, rd->pic, sizeof(nif->name));
	nif->im_format= rd->im_format;
	if (BKE_imtype_is_movie(nif->im_format.imtype)) {
		nif->im_format.imtype= R_IMF_IMTYPE_OPENEXR;
	}
	nif->sfra= rd->sfra;
	nif->efra= rd->efra;
}

void register_node_type_cmp_output_file(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_OUTPUT_FILE, "File Output", NODE_CLASS_OUTPUT, NODE_PREVIEW|NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_output_file_in, NULL);
	node_type_size(&ntype, 140, 80, 300);
	node_type_init(&ntype, node_composit_init_output_file);
	node_type_storage(&ntype, "NodeImageFile", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_output_file);
	node_type_mute(&ntype, node_composit_mute_output_file, NULL);

	nodeRegisterType(ttype, &ntype);
}


/* =============================================================================== */


void ntreeCompositOutputMultiFileAddSocket(bNodeTree *ntree, bNode *node, ImageFormatData *im_format)
{
	bNodeSocket *sock = nodeAddSocket(ntree, node, SOCK_IN, "", SOCK_RGBA);
	
	/* create format data for the input socket */
	NodeImageMultiFileSocket *sockdata = MEM_callocN(sizeof(NodeImageMultiFileSocket), "socket image format");
	sock->storage = sockdata;
	sock->struct_type = SOCK_STRUCT_OUTPUT_MULTI_FILE;
	
	if(im_format) {
		sockdata->format= *im_format;
		if (BKE_imtype_is_movie(sockdata->format.imtype)) {
			sockdata->format.imtype= R_IMF_IMTYPE_OPENEXR;
		}
	}
	/* use render data format by default */
	sockdata->use_render_format = 1;
}

int ntreeCompositOutputMultiFileRemoveActiveSocket(bNodeTree *ntree, bNode *node)
{
	NodeImageMultiFile *nimf = node->storage;
	bNodeSocket *sock = BLI_findlink(&node->inputs, nimf->active_input);
	
	if (!sock)
		return 0;
	
	/* free format data */
	MEM_freeN(sock->storage);
	
	nodeRemoveSocket(ntree, node, sock);
	return 1;
}

static void init_output_multi_file(bNodeTree *ntree, bNode* node, bNodeTemplate *ntemp)
{
	RenderData *rd = &ntemp->scene->r;
	NodeImageMultiFile *nimf= MEM_callocN(sizeof(NodeImageMultiFile), "node image multi file");
	node->storage= nimf;

	BLI_strncpy(nimf->base_path, rd->pic, sizeof(nimf->base_path));
	
	/* add one socket by default */
	ntreeCompositOutputMultiFileAddSocket(ntree, node, &rd->im_format);
}

void free_output_multi_file(bNode *node)
{
	bNodeSocket *sock;
	
	/* free storage data in sockets */
	for (sock=node->inputs.first; sock; sock=sock->next) {
		MEM_freeN(sock->storage);
	}
	
	MEM_freeN(node->storage);
}

void copy_output_multi_file(struct bNode *node, struct bNode *target)
{
	bNodeSocket *sock, *newsock;
	
	target->storage = MEM_dupallocN(node->storage);
	
	/* duplicate storage data in sockets */
	for (sock=node->inputs.first, newsock=target->inputs.first; sock && newsock; sock=sock->next, newsock=newsock->next) {
		newsock->storage = MEM_dupallocN(sock->storage);
	}
}

static void exec_output_multi_file(void *data, bNode *node, bNodeStack **in, bNodeStack **UNUSED(out))
{
	RenderData *rd= data;
	NodeImageMultiFile *nimf= node->storage;
	bNodeSocket *sock;
	int i;
	
	for (sock=node->inputs.first, i=0; sock; sock=sock->next, ++i) {
		if (!in[i]->data)
			continue;
		
		if (!G.rendering) {
			/* only output files when rendering a sequence -
			 * otherwise, it overwrites the output files just 
			 * scrubbing through the timeline when the compositor updates */
			return;
		} else {
			Main *bmain= G.main; /* TODO, have this passed along */
			NodeImageMultiFileSocket *sockdata = sock->storage;
			CompBuf *cbuf= typecheck_compbuf(in[i]->data, CB_RGBA);
			ImBuf *ibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0);
			ImageFormatData *format = (sockdata->use_render_format ? &rd->im_format : &sockdata->format);
			char path[FILE_MAX];
			char string[FILE_MAX];
			
			ibuf->rect_float= cbuf->rect;
			ibuf->dither= rd->dither_intensity;
			
			if (rd->color_mgt_flag & R_COLOR_MANAGEMENT)
				ibuf->profile = IB_PROFILE_LINEAR_RGB;
			
			/* get full path */
			BLI_join_dirfile(path, FILE_MAX, nimf->base_path, sock->name);
			
			BKE_makepicstring(string, path, bmain->name, rd->cfra, format->imtype, (rd->scemode & R_EXTENSION), TRUE);
			
			if(0 == BKE_write_ibuf(ibuf, string, format))
				printf("Cannot save Node File Output to %s\n", string);
			else
				printf("Saved: %s\n", string);
			
			IMB_freeImBuf(ibuf);	
			
			#if 0	/* XXX not used yet */
			generate_preview(data, node, cbuf);
			#endif
			
			if(in[i]->data != cbuf) 
				free_compbuf(cbuf);
		}
	}
}

static void mute_output_multi_file(void *UNUSED(data), int UNUSED(thread),
										   struct bNode *UNUSED(node), void *UNUSED(nodedata),
										   struct bNodeStack **UNUSED(in), struct bNodeStack **UNUSED(out))
{
	/* nothing to do here */
}

void register_node_type_cmp_output_multi_file(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_OUTPUT_MULTI_FILE, "Multi File Output", NODE_CLASS_OUTPUT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, NULL, NULL);
	node_type_size(&ntype, 140, 80, 300);
	node_type_init(&ntype, init_output_multi_file);
	node_type_storage(&ntype, "NodeImageMultiFile", free_output_multi_file, copy_output_multi_file);
	node_type_exec(&ntype, exec_output_multi_file);
	node_type_mute(&ntype, mute_output_multi_file, NULL);

	nodeRegisterType(ttype, &ntype);
}
