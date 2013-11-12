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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_moviedistortion.c
 *  \ingroup cmpnodes
 */

#include "node_composite_util.h"

#include "BKE_context.h"

#include "RNA_access.h"

/* **************** Translate  ******************** */

static bNodeSocketTemplate cmp_node_moviedistortion_in[] = {
	{	SOCK_RGBA, 1, N_("Image"),			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_moviedistortion_out[] = {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

static void label(bNodeTree *UNUSED(ntree), bNode *node, char *label, int maxlen)
{
	if (node->custom1 == 0)
		BLI_strncpy(label, IFACE_("Undistortion"), maxlen);
	else
		BLI_strncpy(label, IFACE_("Distortion"), maxlen);
}

static void init(const bContext *C, PointerRNA *ptr)
{
	bNode *node = ptr->data;
	Scene *scene = CTX_data_scene(C);
	
	node->id = (ID *)scene->clip;
}

static void storage_free(bNode *node)
{
	if (node->storage)
		BKE_tracking_distortion_free(node->storage);

	node->storage = NULL;
}

static void storage_copy(bNodeTree *UNUSED(dest_ntree), bNode *dest_node, bNode *src_node)
{
	if (src_node->storage)
		dest_node->storage = BKE_tracking_distortion_copy(src_node->storage);
}

void register_node_type_cmp_moviedistortion(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_MOVIEDISTORTION, "Movie Distortion", NODE_CLASS_DISTORT, 0);
	node_type_socket_templates(&ntype, cmp_node_moviedistortion_in, cmp_node_moviedistortion_out);
	node_type_label(&ntype, label);

	ntype.initfunc_api = init;
	node_type_storage(&ntype, NULL, storage_free, storage_copy);

	nodeRegisterType(&ntype);
}
