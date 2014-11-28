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

/** \file blender/nodes/composite/nodes/node_composite_keyingscreen.c
 *  \ingroup cmpnodes
 */

#include "DNA_movieclip_types.h"

#include "BLI_math_base.h"
#include "BLI_math_color.h"

#include "node_composite_util.h"

/* **************** Translate  ******************** */

static bNodeSocketTemplate cmp_node_keyingscreen_out[] = {
	{	SOCK_RGBA,  0, "Screen"},
	{	-1, 0, ""	}
};

static void node_composit_init_keyingscreen(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeKeyingScreenData *data;

	data = MEM_callocN(sizeof(NodeKeyingScreenData), "node keyingscreen data");

	node->storage = data;
}

void register_node_type_cmp_keyingscreen(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_KEYINGSCREEN, "Keying Screen", NODE_CLASS_MATTE, 0);
	node_type_socket_templates(&ntype, NULL, cmp_node_keyingscreen_out);
	node_type_init(&ntype, node_composit_init_keyingscreen);
	node_type_storage(&ntype, "NodeKeyingScreenData", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(&ntype);
}
