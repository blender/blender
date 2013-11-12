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

#include <limits.h>
#include <string.h>

#include "DNA_action_types.h"
#include "DNA_node_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
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
	if (node->storage) {
		MEM_freeN(node->storage);
	}
}

void node_copy_curves(bNodeTree *UNUSED(dest_ntree), bNode *dest_node, bNode *src_node)
{
	dest_node->storage = curvemapping_copy(src_node->storage);
}

void node_copy_standard_storage(bNodeTree *UNUSED(dest_ntree), bNode *dest_node, bNode *src_node)
{
	dest_node->storage = MEM_dupallocN(src_node->storage);
}

void *node_initexec_curves(bNodeExecContext *UNUSED(context), bNode *node, bNodeInstanceKey UNUSED(key))
{
	curvemapping_initialize(node->storage);
	return NULL;  /* unused return */
}

/**** Labels ****/

void node_blend_label(bNode *node, char *label, int maxlen)
{
	const char *name;
	RNA_enum_name(ramp_blend_items, node->custom1, &name);
	BLI_strncpy(label, IFACE_(name), maxlen);
}

void node_math_label(bNode *node, char *label, int maxlen)
{
	const char *name;
	RNA_enum_name(node_math_items, node->custom1, &name);
	BLI_strncpy(label, IFACE_(name), maxlen);
}

void node_vect_math_label(bNode *node, char *label, int maxlen)
{
	const char *name;
	RNA_enum_name(node_vec_math_items, node->custom1, &name);
	BLI_strncpy(label, IFACE_(name), maxlen);
}

void node_filter_label(bNode *node, char *label, int maxlen)
{
	const char *name;
	RNA_enum_name(node_filter_items, node->custom1, &name);
	BLI_strncpy(label, IFACE_(name), maxlen);
}

void node_update_internal_links_default(bNodeTree *ntree, bNode *node)
{
	bNodeLink *link;
	bNodeSocket *output, *input, *selected;

	/* sanity check */
	if (!ntree)
		return;

	/* use link pointer as a tag for handled sockets (for outputs is unused anyway) */
	for (output = node->outputs.first; output; output = output->next)
		output->link = NULL;
	
	for (link = ntree->links.first; link; link = link->next) {
		output = link->fromsock;
		if (link->fromnode != node || output->link)
			continue;
		output->link = link; /* not really used, just for tagging handled sockets */
		
		/* look for suitable input */
		selected = NULL;
		for (input = node->inputs.first; input; input = input->next) {
			/* only use if same type */
			if (input->type == output->type) {
				if (!selected) {
					selected = input;
				}
				else {
					/* linked inputs preferred */
					if (input->link && !selected->link)
						selected = input;
				}
			}
		}
		
		if (selected) {
			bNodeLink *ilink = MEM_callocN(sizeof(bNodeLink), "internal node link");
			ilink->fromnode = node;
			ilink->fromsock = selected;
			ilink->tonode = node;
			ilink->tosock = output;
			/* internal link is always valid */
			ilink->flag |= NODE_LINK_VALID;
			BLI_addtail(&node->internal_links, ilink);
		}
	}
	
	/* clean up */
	for (output = node->outputs.first; output; output = output->next)
		output->link = NULL;
}

float node_socket_get_float(bNodeTree *ntree, bNode *UNUSED(node), bNodeSocket *sock)
{
	PointerRNA ptr;
	RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
	return RNA_float_get(&ptr, "default_value");
}

void node_socket_set_float(bNodeTree *ntree, bNode *UNUSED(node), bNodeSocket *sock, float value)
{
	PointerRNA ptr;
	RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
	RNA_float_set(&ptr, "default_value", value);
}

void node_socket_get_color(bNodeTree *ntree, bNode *UNUSED(node), bNodeSocket *sock, float *value)
{
	PointerRNA ptr;
	RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
	RNA_float_get_array(&ptr, "default_value", value);
}

void node_socket_set_color(bNodeTree *ntree, bNode *UNUSED(node), bNodeSocket *sock, const float *value)
{
	PointerRNA ptr;
	RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
	RNA_float_set_array(&ptr, "default_value", value);
}

void node_socket_get_vector(bNodeTree *ntree, bNode *UNUSED(node), bNodeSocket *sock, float *value)
{
	PointerRNA ptr;
	RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
	RNA_float_get_array(&ptr, "default_value", value);
}

void node_socket_set_vector(bNodeTree *ntree, bNode *UNUSED(node), bNodeSocket *sock, const float *value)
{
	PointerRNA ptr;
	RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
	RNA_float_set_array(&ptr, "default_value", value);
}
