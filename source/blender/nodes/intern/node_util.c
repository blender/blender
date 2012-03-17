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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Nathan Letwory.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/intern/node_util.c
 *  \ingroup nodes
 */


#include "DNA_action_types.h"
#include "DNA_node_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_colortools.h"
#include "BKE_node.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "MEM_guardedalloc.h"

#include "node_util.h"

/**** Storage Data ****/

void node_free_curves(bNode *node)
{
	curvemapping_free(node->storage);
}

void node_free_standard_storage(bNode *node)
{
	MEM_freeN(node->storage);
}

void node_copy_curves(bNode *orig_node, bNode *new_node)
{
	new_node->storage= curvemapping_copy(orig_node->storage);
}

void node_copy_standard_storage(bNode *orig_node, bNode *new_node)
{
	new_node->storage= MEM_dupallocN(orig_node->storage);
}

/**** Labels ****/

const char *node_blend_label(bNode *node)
{
	const char *name;
	RNA_enum_name(ramp_blend_items, node->custom1, &name);
	return IFACE_(name);
}

const char *node_math_label(bNode *node)
{
	const char *name;
	RNA_enum_name(node_math_items, node->custom1, &name);
	return IFACE_(name);
}

const char *node_vect_math_label(bNode *node)
{
	const char *name;
	RNA_enum_name(node_vec_math_items, node->custom1, &name);
	return IFACE_(name);
}

const char *node_filter_label(bNode *node)
{
	const char *name;
	RNA_enum_name(node_filter_items, node->custom1, &name);
	return IFACE_(name);
}

ListBase node_internal_connect_default(bNodeTree *ntree, bNode *node)
{
	static int types[] = { SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA };
	bNodeLink *link;
	bNodeSocket *fromsock, *tosock;
	ListBase ret;
	int in, out, i;

	ret.first = ret.last = NULL;

	/* Security check! */
	if(!ntree)
		return ret;

	/* Connect the first input of each type with first output of the same type. */

	for (i=0; i < 3; ++i) {
		/* find input socket */
		for (in=0, fromsock=node->inputs.first; fromsock; in++, fromsock=fromsock->next) {
			if (fromsock->type==types[i] && nodeCountSocketLinks(ntree, fromsock) > 0)
				break;
		}
		if (fromsock) {
			for (out=0, tosock=node->outputs.first; tosock; out++, tosock=tosock->next) {
				if (tosock->type==types[i] && (nodeCountSocketLinks(ntree, tosock) > 0))
					break;
			}
			if (tosock) {
				link = MEM_callocN(sizeof(bNodeLink), "internal node link");
				link->fromnode = node;
				link->fromsock = fromsock;
				link->tonode = node;
				link->tosock = tosock;
				/* internal link is always valid */
				link->flag |= NODE_LINK_VALID;
				BLI_addtail(&ret, link);
			}
		}
	}

	return ret;
}
