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

/** \file blender/nodes/composite/nodes/node_composite_keying.c
 *  \ingroup cmpnodes
 */

#include "BLF_translation.h"

#include "DNA_movieclip_types.h"

#include "BKE_movieclip.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_color.h"
#include "BLI_voronoi.h"

#include "node_composite_util.h"

/* **************** Translate  ******************** */

static bNodeSocketTemplate cmp_node_keying_in[] = {
	{	SOCK_RGBA,  1, "Image",	        0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA,  1, "Key Color",     1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_FLOAT, 1, "Garbage Matte", 0.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_keying_out[] = {
	{	SOCK_RGBA,  0, "Image"},
	{	SOCK_FLOAT, 0, "Matte"},
	{	SOCK_FLOAT, 0, "Edges"},
	{	-1, 0, ""	}
};

static void exec(void *UNUSED(data), bNode *UNUSED(node), bNodeStack **UNUSED(in), bNodeStack **UNUSED(out))
{
}

static void node_composit_init_keying(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeKeyingData *data;

	data = MEM_callocN(sizeof(NodeKeyingData), "node keying data");

	data->screen_balance = 0.5f;
	data->despill_factor = 1.0f;
	data->edge_kernel_radius = 3;
	data->edge_kernel_tolerance = 0.1f;
	data->clip_white = 1.0f;
	data->clip_black = 0.0f;
	data->clip_white = 1.0f;

	node->storage = data;
}

void register_node_type_cmp_keying(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_KEYING, "Keying", NODE_CLASS_MATTE, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_keying_in, cmp_node_keying_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_init(&ntype, node_composit_init_keying);
	node_type_storage(&ntype, "NodeKeyingData", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, exec);

	nodeRegisterType(ttype, &ntype);
}
