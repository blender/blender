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

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "intern/openexr/openexr_multi.h"


/* **************** OUTPUT FILE ******************** */

bNodeSocket *ntreeCompositOutputFileAddSocket(bNodeTree *ntree, bNode *node, const char *name, ImageFormatData *im_format)
{
	bNodeSocket *sock = nodeAddSocket(ntree, node, SOCK_IN, name, SOCK_RGBA);
	
	/* create format data for the input socket */
	NodeImageMultiFileSocket *sockdata = MEM_callocN(sizeof(NodeImageMultiFileSocket), "socket image format");
	sock->storage = sockdata;
	sock->struct_type = SOCK_STRUCT_OUTPUT_FILE;
	
	if(im_format) {
		sockdata->format= *im_format;
		if (BKE_imtype_is_movie(sockdata->format.imtype)) {
			sockdata->format.imtype= R_IMF_IMTYPE_OPENEXR;
		}
	}
	/* use node data format by default */
	sockdata->use_node_format = 1;
	
	return sock;
}

int ntreeCompositOutputFileRemoveActiveSocket(bNodeTree *ntree, bNode *node)
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

static void init_output_file(bNodeTree *ntree, bNode* node, bNodeTemplate *ntemp)
{
	RenderData *rd = &ntemp->scene->r;
	NodeImageMultiFile *nimf= MEM_callocN(sizeof(NodeImageMultiFile), "node image multi file");
	node->storage= nimf;

	BLI_strncpy(nimf->base_path, rd->pic, sizeof(nimf->base_path));
	nimf->format = rd->im_format;
	
	/* add one socket by default */
	ntreeCompositOutputFileAddSocket(ntree, node, "Image", &rd->im_format);
}

static void free_output_file(bNode *node)
{
	bNodeSocket *sock;
	
	/* free storage data in sockets */
	for (sock=node->inputs.first; sock; sock=sock->next) {
		MEM_freeN(sock->storage);
	}
	
	MEM_freeN(node->storage);
}

static void copy_output_file(struct bNode *node, struct bNode *target)
{
	bNodeSocket *sock, *newsock;
	
	target->storage = MEM_dupallocN(node->storage);
	
	/* duplicate storage data in sockets */
	for (sock=node->inputs.first, newsock=target->inputs.first; sock && newsock; sock=sock->next, newsock=newsock->next) {
		newsock->storage = MEM_dupallocN(sock->storage);
	}
}

static void update_output_file(bNodeTree *UNUSED(ntree), bNode *node)
{
	bNodeSocket *sock;
	
	/* automatically update the socket type based on linked input */
	for (sock=node->inputs.first; sock; sock=sock->next) {
		if (sock->link) {
			int linktype = sock->link->fromsock->type;
			if (linktype != sock->type)
				nodeSocketSetType(sock, linktype);
		}
	}
}

/* write input data into individual files */
static void exec_output_file_singlelayer(RenderData *rd, bNode *node, bNodeStack **in)
{
	Main *bmain= G.main; /* TODO, have this passed along */
	NodeImageMultiFile *nimf= node->storage;
	bNodeSocket *sock;
	int i;
	int has_preview = 0;
	
	for (sock=node->inputs.first, i=0; sock; sock=sock->next, ++i) {
		if (in[i]->data) {
			NodeImageMultiFileSocket *sockdata = sock->storage;
			ImageFormatData *format = (sockdata->use_node_format ? &nimf->format : &sockdata->format);
			char path[FILE_MAX];
			char filename[FILE_MAX];
			CompBuf *cbuf;
			ImBuf *ibuf;
			
			switch (format->planes) {
			case R_IMF_PLANES_BW:
				cbuf = typecheck_compbuf(in[i]->data, CB_VAL);
				break;
			case R_IMF_PLANES_RGB:
				cbuf = typecheck_compbuf(in[i]->data, CB_VEC3);
				break;
			case R_IMF_PLANES_RGBA:
				cbuf = typecheck_compbuf(in[i]->data, CB_RGBA);
				break;
			}
			
			ibuf = IMB_allocImBuf(cbuf->x, cbuf->y, format->planes, 0);
			ibuf->rect_float = cbuf->rect;
			ibuf->dither = rd->dither_intensity;
			
			if (rd->color_mgt_flag & R_COLOR_MANAGEMENT)
				ibuf->profile = IB_PROFILE_LINEAR_RGB;
			
			/* get full path */
			BLI_join_dirfile(path, FILE_MAX, nimf->base_path, sock->name);
			BKE_makepicstring(filename, path, bmain->name, rd->cfra, format->imtype, (rd->scemode & R_EXTENSION), TRUE);
			
			if(0 == BKE_write_ibuf(ibuf, filename, format))
				printf("Cannot save Node File Output to %s\n", filename);
			else
				printf("Saved: %s\n", filename);
			
			IMB_freeImBuf(ibuf);
			
			/* simply pick the first valid input for preview */
			if (!has_preview) {
				generate_preview(rd, node, cbuf);
				has_preview = 1;
			}
			
			if(in[i]->data != cbuf) 
				free_compbuf(cbuf);
		}
	}
}

/* write input data into layers */
static void exec_output_file_multilayer(RenderData *rd, bNode *node, bNodeStack **in)
{
	Main *bmain= G.main; /* TODO, have this passed along */
	NodeImageMultiFile *nimf= node->storage;
	void *exrhandle= IMB_exr_get_handle();
	char filename[FILE_MAX];
	bNodeSocket *sock;
	int i;
	/* Must have consistent pixel size for exr file, simply take the first valid input size. */
	int rectx = -1;
	int recty = -1;
	int has_preview = 0;
	
	BKE_makepicstring(filename, nimf->base_path, bmain->name, rd->cfra, R_IMF_IMTYPE_MULTILAYER, (rd->scemode & R_EXTENSION), TRUE);
	BLI_make_existing_file(filename);
	
	for (sock=node->inputs.first, i=0; sock; sock=sock->next, ++i) {
		if (in[i]->data) {
			CompBuf *cbuf = in[i]->data;
			char layname[EXR_LAY_MAXNAME];
			char channelname[EXR_PASS_MAXNAME];
			char *channelname_ext;
			
			if (cbuf->rect_procedural) {
				printf("Error writing multilayer EXR: Procedural buffer not supported\n");
				continue;
			}
			if (rectx < 0) {
				rectx = cbuf->x;
				recty = cbuf->y;
			}
			else if (cbuf->x != rectx || cbuf->y != recty) {
				printf("Error: Multilayer EXR output node %s expects same resolution for all input buffers. Layer %s skipped.\n", node->name, sock->name);
				continue;
			}
			
			BLI_strncpy(layname, sock->name, sizeof(layname));
			BLI_strncpy(channelname, sock->name, sizeof(channelname)-2);
			channelname_ext = channelname + strlen(channelname);
			
			/* create channels */
			switch (cbuf->type) {
			case CB_VAL:
				strcpy(channelname_ext, ".V");
				IMB_exr_add_channel(exrhandle, layname, channelname, 1, rectx, cbuf->rect);
				break;
			case CB_VEC2:
				strcpy(channelname_ext, ".X");
				IMB_exr_add_channel(exrhandle, layname, channelname, 2, 2*rectx, cbuf->rect);
				strcpy(channelname_ext, ".Y");
				IMB_exr_add_channel(exrhandle, layname, channelname, 2, 2*rectx, cbuf->rect+1);
				break;
			case CB_VEC3:
				strcpy(channelname_ext, ".X");
				IMB_exr_add_channel(exrhandle, layname, channelname, 3, 3*rectx, cbuf->rect);
				strcpy(channelname_ext, ".Y");
				IMB_exr_add_channel(exrhandle, layname, channelname, 3, 3*rectx, cbuf->rect+1);
				strcpy(channelname_ext, ".Z");
				IMB_exr_add_channel(exrhandle, layname, channelname, 3, 3*rectx, cbuf->rect+2);
				break;
			case CB_RGBA:
				strcpy(channelname_ext, ".R");
				IMB_exr_add_channel(exrhandle, layname, channelname, 4, 4*rectx, cbuf->rect);
				strcpy(channelname_ext, ".G");
				IMB_exr_add_channel(exrhandle, layname, channelname, 4, 4*rectx, cbuf->rect+1);
				strcpy(channelname_ext, ".B");
				IMB_exr_add_channel(exrhandle, layname, channelname, 4, 4*rectx, cbuf->rect+2);
				strcpy(channelname_ext, ".A");
				IMB_exr_add_channel(exrhandle, layname, channelname, 4, 4*rectx, cbuf->rect+3);
				break;
			}
			
			/* simply pick the first valid input for preview */
			if (!has_preview) {
				generate_preview(rd, node, cbuf);
				has_preview = 1;
			}
		}
	}
	
	/* when the filename has no permissions, this can fail */
	if(IMB_exr_begin_write(exrhandle, filename, rectx, recty, nimf->format.compress)) {
		IMB_exr_write_channels(exrhandle);
	}
	else {
		/* TODO, get the error from openexr's exception */
		/* XXX nice way to do report? */
		printf("Error Writing Render Result, see console\n");
	}
	
	IMB_exr_close(exrhandle);
}

static void exec_output_file(void *data, bNode *node, bNodeStack **in, bNodeStack **UNUSED(out))
{
	RenderData *rd= data;
	NodeImageMultiFile *nimf= node->storage;
	
	if (!G.rendering) {
		/* only output files when rendering a sequence -
		 * otherwise, it overwrites the output files just 
		 * scrubbing through the timeline when the compositor updates */
		return;
	}
	
	if (nimf->format.imtype==R_IMF_IMTYPE_MULTILAYER)
		exec_output_file_multilayer(rd, node, in);
	else
		exec_output_file_singlelayer(rd, node, in);
}

void register_node_type_cmp_output_file(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_OUTPUT_FILE, "File Output", NODE_CLASS_OUTPUT, NODE_OPTIONS|NODE_PREVIEW);
	node_type_socket_templates(&ntype, NULL, NULL);
	node_type_size(&ntype, 140, 80, 300);
	node_type_init(&ntype, init_output_file);
	node_type_storage(&ntype, "NodeImageMultiFile", free_output_file, copy_output_file);
	node_type_update(&ntype, update_output_file, NULL);
	node_type_exec(&ntype, exec_output_file);

	nodeRegisterType(ttype, &ntype);
}
