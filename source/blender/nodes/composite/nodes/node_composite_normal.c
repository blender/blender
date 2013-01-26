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

/** \file blender/nodes/composite/nodes/node_composite_normal.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** NORMAL  ******************** */
static bNodeSocketTemplate cmp_node_normal_in[] = {
	{	SOCK_VECTOR, 1, N_("Normal"),	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_DIRECTION},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_normal_out[] = {
	{	SOCK_VECTOR, 0, N_("Normal"),	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_DIRECTION},
	{	SOCK_FLOAT, 0, N_("Dot")},
	{	-1, 0, ""	}
};

static void init(bNodeTree *UNUSED(ntree), bNode *node, bNodeTemplate *UNUSED(ntemp))
{
	bNodeSocket *sock= node->outputs.first;
	float *nor= ((bNodeSocketValueVector*)sock->default_value)->value;
	
	nor[0] = 0.0f;
	nor[1] = 0.0f;
	nor[2] = 1.0f;
}

void register_node_type_cmp_normal(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_NORMAL, "Normal", NODE_CLASS_OP_VECTOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_normal_in, cmp_node_normal_out);
	node_type_init(&ntype, init);
	node_type_size(&ntype, 100, 60, 200);

	nodeRegisterType(ttype, &ntype);
}
