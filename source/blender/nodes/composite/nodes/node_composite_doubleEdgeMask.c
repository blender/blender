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
 * Contributor(s): Peter Larabell.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_doubleEdgeMask.c
 *  \ingroup cmpnodes
 */
#include "node_composite_util.h"
/* **************** Double Edge Mask ******************** */


static bNodeSocketTemplate cmp_node_doubleedgemask_in[] = {
	{ SOCK_FLOAT, 1, "Inner Mask", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f, PROP_NONE},	// inner mask socket definition
	{ SOCK_FLOAT, 1, "Outer Mask", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f, PROP_NONE},	// outer mask socket definition
	{ -1, 0, ""	}																	// input socket array terminator
};
static bNodeSocketTemplate cmp_node_doubleedgemask_out[] = {
	{ SOCK_FLOAT, 0, "Mask"},		// output socket definition
	{ -1, 0, "" }					// output socket array terminator
};

void register_node_type_cmp_doubleedgemask(void)
{
	static bNodeType ntype;	// allocate a node type data structure

	cmp_node_type_base(&ntype, CMP_NODE_DOUBLEEDGEMASK, "Double Edge Mask", NODE_CLASS_MATTE, 0);
	node_type_socket_templates(&ntype, cmp_node_doubleedgemask_in, cmp_node_doubleedgemask_out);
	node_type_socket_templates(&ntype, cmp_node_doubleedgemask_in, cmp_node_doubleedgemask_out);

	nodeRegisterType(&ntype);
}
