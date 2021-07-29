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

/** \file blender/nodes/composite/nodes/node_composite_scale.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** Scale  ******************** */

static bNodeSocketTemplate cmp_node_scale_in[] = {
	{   SOCK_RGBA, 1, N_("Image"),          1.0f, 1.0f, 1.0f, 1.0f},
	{   SOCK_FLOAT, 1, N_("X"),             1.0f, 0.0f, 0.0f, 0.0f, 0.0001f, CMP_SCALE_MAX, PROP_NONE},
	{   SOCK_FLOAT, 1, N_("Y"),             1.0f, 0.0f, 0.0f, 0.0f, 0.0001f, CMP_SCALE_MAX, PROP_NONE},
	{   -1, 0, ""   }
};
static bNodeSocketTemplate cmp_node_scale_out[] = {
	{   SOCK_RGBA, 0, N_("Image")},
	{   -1, 0, ""   }
};

static void node_composite_update_scale(bNodeTree *UNUSED(ntree), bNode *node)
{
	bNodeSocket *sock;
	bool use_xy_scale = ELEM(node->custom1, CMP_SCALE_RELATIVE, CMP_SCALE_ABSOLUTE);

	/* Only show X/Y scale factor inputs for modes using them! */
	for (sock = node->inputs.first; sock; sock = sock->next) {
		if (STREQ(sock->name, "X") || STREQ(sock->name, "Y")) {
			if (use_xy_scale) {
				sock->flag &= ~SOCK_UNAVAIL;
			}
			else {
				sock->flag |= SOCK_UNAVAIL;
			}
		}
	}
}

void register_node_type_cmp_scale(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_SCALE, "Scale", NODE_CLASS_DISTORT, 0);
	node_type_socket_templates(&ntype, cmp_node_scale_in, cmp_node_scale_out);
	node_type_update(&ntype, node_composite_update_scale, NULL);

	nodeRegisterType(&ntype);
}
