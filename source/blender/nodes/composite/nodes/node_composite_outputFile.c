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
#include "BLI_utildefines.h"
#include "BLI_path_util.h"

#include "BKE_context.h"

#include "RNA_access.h"

#include "node_composite_util.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "intern/openexr/openexr_multi.h"


/* **************** OUTPUT FILE ******************** */

/* find unique path */
static bool unique_path_unique_check(void *arg, const char *name)
{
	struct {ListBase *lb; bNodeSocket *sock; } *data = arg;
	bNodeSocket *sock;
	for (sock = data->lb->first; sock; sock = sock->next) {
		if (sock != data->sock) {
			NodeImageMultiFileSocket *sockdata = sock->storage;
			if (STREQ(sockdata->path, name))
				return true;
		}
	}
	return false;
}
void ntreeCompositOutputFileUniquePath(ListBase *list, bNodeSocket *sock, const char defname[], char delim)
{
	NodeImageMultiFileSocket *sockdata;
	struct {ListBase *lb; bNodeSocket *sock; } data;
	data.lb = list;
	data.sock = sock;

	/* See if we are given an empty string */
	if (ELEM(NULL, sock, defname))
		return;

	sockdata = sock->storage;
	BLI_uniquename_cb(unique_path_unique_check, &data, defname, delim, sockdata->path, sizeof(sockdata->path));
}

/* find unique EXR layer */
static bool unique_layer_unique_check(void *arg, const char *name)
{
	struct {ListBase *lb; bNodeSocket *sock; } *data = arg;
	bNodeSocket *sock;
	for (sock = data->lb->first; sock; sock = sock->next) {
		if (sock != data->sock) {
			NodeImageMultiFileSocket *sockdata = sock->storage;
			if (STREQ(sockdata->layer, name))
				return true;
		}
	}
	return false;
}
void ntreeCompositOutputFileUniqueLayer(ListBase *list, bNodeSocket *sock, const char defname[], char delim)
{
	NodeImageMultiFileSocket *sockdata;
	struct {ListBase *lb; bNodeSocket *sock; } data;
	data.lb = list;
	data.sock = sock;

	/* See if we are given an empty string */
	if (ELEM(NULL, sock, defname))
		return;

	sockdata = sock->storage;
	BLI_uniquename_cb(unique_layer_unique_check, &data, defname, delim, sockdata->layer, sizeof(sockdata->layer));
}

bNodeSocket *ntreeCompositOutputFileAddSocket(bNodeTree *ntree, bNode *node, const char *name, ImageFormatData *im_format)
{
	NodeImageMultiFile *nimf = node->storage;
	bNodeSocket *sock = nodeAddStaticSocket(ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, NULL, name);
	
	/* create format data for the input socket */
	NodeImageMultiFileSocket *sockdata = MEM_callocN(sizeof(NodeImageMultiFileSocket), "socket image format");
	sock->storage = sockdata;
	
	BLI_strncpy_utf8(sockdata->path, name, sizeof(sockdata->path));
	ntreeCompositOutputFileUniquePath(&node->inputs, sock, name, '_');
	BLI_strncpy_utf8(sockdata->layer, name, sizeof(sockdata->layer));
	ntreeCompositOutputFileUniqueLayer(&node->inputs, sock, name, '_');
	
	if (im_format) {
		sockdata->format = *im_format;
		if (BKE_imtype_is_movie(sockdata->format.imtype)) {
			sockdata->format.imtype = R_IMF_IMTYPE_OPENEXR;
		}
	}
	else
		BKE_imformat_defaults(&sockdata->format);
	/* use node data format by default */
	sockdata->use_node_format = TRUE;
	
	nimf->active_input = BLI_findindex(&node->inputs, sock);
	
	return sock;
}

int ntreeCompositOutputFileRemoveActiveSocket(bNodeTree *ntree, bNode *node)
{
	NodeImageMultiFile *nimf = node->storage;
	bNodeSocket *sock = BLI_findlink(&node->inputs, nimf->active_input);
	int totinputs = BLI_countlist(&node->inputs);
	
	if (!sock)
		return 0;
	
	if (nimf->active_input == totinputs - 1)
		--nimf->active_input;
	
	/* free format data */
	MEM_freeN(sock->storage);
	
	nodeRemoveSocket(ntree, node, sock);
	return 1;
}

void ntreeCompositOutputFileSetPath(bNode *node, bNodeSocket *sock, const char *name)
{
	NodeImageMultiFileSocket *sockdata = sock->storage;
	BLI_strncpy_utf8(sockdata->path, name, sizeof(sockdata->path));
	ntreeCompositOutputFileUniquePath(&node->inputs, sock, name, '_');
}

void ntreeCompositOutputFileSetLayer(bNode *node, bNodeSocket *sock, const char *name)
{
	NodeImageMultiFileSocket *sockdata = sock->storage;
	BLI_strncpy_utf8(sockdata->layer, name, sizeof(sockdata->layer));
	ntreeCompositOutputFileUniqueLayer(&node->inputs, sock, name, '_');
}

/* XXX uses initfunc_api callback, regular initfunc does not support context yet */
static void init_output_file(const bContext *C, PointerRNA *ptr)
{
	Scene *scene = CTX_data_scene(C);
	bNodeTree *ntree = ptr->id.data;
	bNode *node = ptr->data;
	NodeImageMultiFile *nimf = MEM_callocN(sizeof(NodeImageMultiFile), "node image multi file");
	ImageFormatData *format = NULL;
	node->storage = nimf;
	
	if (scene) {
		RenderData *rd = &scene->r;

		BLI_strncpy(nimf->base_path, rd->pic, sizeof(nimf->base_path));
		nimf->format = rd->im_format;
		if (BKE_imtype_is_movie(nimf->format.imtype)) {
			nimf->format.imtype = R_IMF_IMTYPE_OPENEXR;
		}
		
		format = &nimf->format;
	}
	else
		BKE_imformat_defaults(&nimf->format);
	
	/* add one socket by default */
	ntreeCompositOutputFileAddSocket(ntree, node, "Image", format);
}

static void free_output_file(bNode *node)
{
	bNodeSocket *sock;
	
	/* free storage data in sockets */
	for (sock = node->inputs.first; sock; sock = sock->next) {
		MEM_freeN(sock->storage);
	}
	
	MEM_freeN(node->storage);
}

static void copy_output_file(bNodeTree *UNUSED(dest_ntree), bNode *dest_node, bNode *src_node)
{
	bNodeSocket *src_sock, *dest_sock;
	
	dest_node->storage = MEM_dupallocN(src_node->storage);
	
	/* duplicate storage data in sockets */
	for (src_sock = src_node->inputs.first, dest_sock = dest_node->inputs.first; src_sock && dest_sock; src_sock = src_sock->next, dest_sock = dest_sock->next) {
		dest_sock->storage = MEM_dupallocN(src_sock->storage);
	}
}

static void update_output_file(bNodeTree *ntree, bNode *node)
{
	bNodeSocket *sock, *sock_next;
	PointerRNA ptr;
	
	/* XXX fix for #36706: remove invalid sockets added with bpy API.
	 * This is not ideal, but prevents crashes from missing storage.
	 * FileOutput node needs a redesign to support this properly.
	 */
	for (sock = node->inputs.first; sock; sock = sock_next) {
		sock_next = sock->next;
		if (sock->storage == NULL) {
			nodeRemoveSocket(ntree, node, sock);
		}
	}
	for (sock = node->outputs.first; sock; sock = sock_next) {
		sock_next = sock->next;
		nodeRemoveSocket(ntree, node, sock);
	}
	
	cmp_node_update_default(ntree, node);
	
	/* automatically update the socket type based on linked input */
	for (sock = node->inputs.first; sock; sock = sock->next) {
		if (sock->link) {
			RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
			RNA_enum_set(&ptr, "type", sock->link->fromsock->type);
		}
	}
}

void register_node_type_cmp_output_file(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_OUTPUT_FILE, "File Output", NODE_CLASS_OUTPUT, NODE_PREVIEW);
	node_type_socket_templates(&ntype, NULL, NULL);
	ntype.initfunc_api = init_output_file;
	node_type_storage(&ntype, "NodeImageMultiFile", free_output_file, copy_output_file);
	node_type_update(&ntype, update_output_file, NULL);

	nodeRegisterType(&ntype);
}
