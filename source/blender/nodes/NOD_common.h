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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file NOD_common.h
 *  \ingroup nodes
 */

#ifndef __NOD_COMMON_H__
#define __NOD_COMMON_H__

#include "BKE_node.h"

void register_node_type_frame(void);
void register_node_type_reroute(void);

void register_node_type_group_input(void);
void register_node_type_group_output(void);


/* internal functions for editor */
struct bNodeSocket *node_group_find_input_socket(struct bNode *groupnode, const char *identifier);
struct bNodeSocket *node_group_find_output_socket(struct bNode *groupnode, const char *identifier);
void node_group_verify(struct bNodeTree *ntree, struct bNode *node, struct ID *id);

struct bNodeSocket *node_group_input_find_socket(struct bNode *node, const char *identifier);
struct bNodeSocket *node_group_output_find_socket(struct bNode *node, const char *identifier);
void node_group_input_verify(struct bNodeTree *ntree, struct bNode *node, struct ID *id);
void node_group_output_verify(struct bNodeTree *ntree, struct bNode *node, struct ID *id);

#endif  /* __NOD_COMMON_H__ */
