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
 * Contributor(s): Bob Holcomb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_channelMatte.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* ******************* Channel Matte Node ********************************* */
static bNodeSocketTemplate cmp_node_channel_matte_in[] = {
	{SOCK_RGBA, 1, N_("Image"), 1.0f, 1.0f, 1.0f, 1.0f},
	{-1, 0, ""}
};

static bNodeSocketTemplate cmp_node_channel_matte_out[] = {
	{SOCK_RGBA, 0, N_("Image")},
	{SOCK_FLOAT, 0, N_("Matte")},
	{-1, 0, ""}
};

static void node_composit_init_channel_matte(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeChroma *c = MEM_callocN(sizeof(NodeChroma), "node chroma");
	node->storage = c;
	c->t1 = 1.0f;
	c->t2 = 0.0f;
	c->t3 = 0.0f;
	c->fsize = 0.0f;
	c->fstrength = 0.0f;
	c->algorithm = 1; /*max channel limiting */
	c->channel = 1; /* limit by red */
	node->custom1 = 1; /* RGB channel */
	node->custom2 = 2; /* Green Channel */
}

void register_node_type_cmp_channel_matte(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_CHANNEL_MATTE, "Channel Key", NODE_CLASS_MATTE, NODE_PREVIEW);
	node_type_socket_templates(&ntype, cmp_node_channel_matte_in, cmp_node_channel_matte_out);
	node_type_init(&ntype, node_composit_init_channel_matte);
	node_type_storage(&ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(&ntype);
}
