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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_sunbeams.c
 *  \ingroup cmpnodes
 */

#include "node_composite_util.h"

static bNodeSocketTemplate inputs[] = {
	{	SOCK_RGBA, 1, N_("Image"),			1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate outputs[] = {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

static void init(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeSunBeams *data = MEM_callocN(sizeof(NodeSunBeams), "sun beams node");

	data->source[0] = 0.5f;
	data->source[1] = 0.5f;

	node->storage = data;
}

void register_node_type_cmp_sunbeams(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_SUNBEAMS, "Sun Beams", NODE_CLASS_OP_FILTER, 0);
	node_type_socket_templates(&ntype, inputs, outputs);
	node_type_init(&ntype, init);
	node_type_storage(&ntype, "NodeSunBeams", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(&ntype);
}
