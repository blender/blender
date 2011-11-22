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
	return name;
}

const char *node_math_label(bNode *node)
{
	const char *name;
	RNA_enum_name(node_math_items, node->custom1, &name);
	return name;
}

const char *node_vect_math_label(bNode *node)
{
	const char *name;
	RNA_enum_name(node_vec_math_items, node->custom1, &name);
	return name;
}

const char *node_filter_label(bNode *node)
{
	const char *name;
	RNA_enum_name(node_filter_items, node->custom1, &name);
	return name;
}

/* Returns a list of mapping of some input bNodeStack, GPUNodeStack or bNodeSocket
 * to one or more outputs of the same type.
 * *ntree or (**nsin, **nsout) or (*gnsin, *gnsout) must not be NULL. */
ListBase node_mute_get_links(bNodeTree *ntree, bNode *node, bNodeStack **nsin, bNodeStack **nsout,
                             GPUNodeStack *gnsin, GPUNodeStack *gnsout)
{
	static int types[] = { SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA };
	bNodeLink link = {NULL};
	ListBase ret;
	LinkInOutsMuteNode *lnk;
	int in, out, i;

	ret.first = ret.last = NULL;

	/* Security check! */
	if(!(ntree || (nsin && nsout) || (gnsin && gnsout)))
		return ret;

	/* Connect the first input of each type with first output of the same type. */

	link.fromnode = link.tonode = node;
	for (i=0; i < 3; ++i) {
		/* find input socket */
		for (in=0, link.fromsock=node->inputs.first; link.fromsock; in++, link.fromsock=link.fromsock->next) {
			if (link.fromsock->type==types[i] && (ntree ? nodeCountSocketLinks(ntree, link.fromsock) : nsin ? nsin[in]->hasinput : gnsin[in].hasinput))
				break;
		}
		if (link.fromsock) {
			for (out=0, link.tosock=node->outputs.first; link.tosock; out++, link.tosock=link.tosock->next) {
				if (link.tosock->type==types[i] && (ntree ? nodeCountSocketLinks(ntree, link.tosock) : nsout ? nsout[out]->hasoutput : gnsout[out].hasoutput))
					break;
			}
			if (link.tosock) {
				if(nsin && nsout) {
					lnk = MEM_mallocN(sizeof(LinkInOutsMuteNode), "Muting node: new in to outs link.");
					lnk->in = nsin[in];
					lnk->outs = nsout[out];
					lnk->num_outs = 1;
					BLI_addtail(&ret, lnk);
				}
				else if(gnsin && gnsout) {
					lnk = MEM_mallocN(sizeof(LinkInOutsMuteNode), "Muting node: new in to outs link.");
					lnk->in = &gnsin[in];
					lnk->outs = &gnsout[out];
					lnk->num_outs = 1;
					BLI_addtail(&ret, lnk);
				}
				else {
					lnk = MEM_mallocN(sizeof(LinkInOutsMuteNode), "Muting node: new in to outs link.");
					lnk->in = link.fromsock;
					lnk->outs = link.tosock;
					lnk->num_outs = 1;
					BLI_addtail(&ret, lnk);
				}
			}
		}
	}

	return ret;
}
