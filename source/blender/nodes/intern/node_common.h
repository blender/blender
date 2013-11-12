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
 * Contributor(s): Lukas Toenne.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/intern/node_common.h
 *  \ingroup nodes
 */


#ifndef __NODE_COMMON_H__
#define __NODE_COMMON_H__

#include "DNA_listBase.h"

struct bNodeTree;

void node_group_label(struct bNodeTree *ntree, struct bNode *node, char *label, int maxlen);
int node_group_poll_instance(struct bNode *node, struct bNodeTree *nodetree);

void ntree_update_reroute_nodes(struct bNodeTree *ntree);

#endif
