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
 * Contributor(s): Campbell Barton, Alfredo de Greef, David Millan Escriva,
 * Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_common.c
 *  \ingroup cmpnodes
 */

#include "DNA_node_types.h"

#include "node_composite_util.h"
#include "NOD_common.h"
#include "node_common.h"
#include "node_exec.h"

#include "BKE_node.h"

#include "RNA_access.h"

void register_node_type_cmp_group(void)
{
	static bNodeType ntype;
	
	/* NB: cannot use sh_node_type_base for node group, because it would map the node type
	 * to the shared NODE_GROUP integer type id.
	 */
	node_type_base_custom(&ntype, "CompositorNodeGroup", "Group", NODE_CLASS_GROUP, NODE_CONST_OUTPUT);
	ntype.type = NODE_GROUP;
	ntype.poll = cmp_node_poll_default;
	ntype.poll_instance = node_group_poll_instance;
	ntype.update_internal_links = node_update_internal_links_default;
	ntype.ext.srna = RNA_struct_find("CompositorNodeGroup");
	BLI_assert(ntype.ext.srna != NULL);
	RNA_struct_blender_type_set(ntype.ext.srna, &ntype);
	
	node_type_socket_templates(&ntype, NULL, NULL);
	node_type_size(&ntype, 120, 60, 400);
	node_type_label(&ntype, node_group_label);
	node_type_update(&ntype, NULL, node_group_verify);

	nodeRegisterType(&ntype);
}

