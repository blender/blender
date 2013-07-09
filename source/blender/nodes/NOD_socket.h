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
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file NOD_socket.h
 *  \ingroup nodes
 */


#ifndef __NOD_SOCKET_H__
#define __NOD_SOCKET_H__

#include "DNA_listBase.h"

#include "BLI_utildefines.h"

#include "BKE_node.h"

#include "RNA_types.h"

struct bNodeTree;
struct bNode;
struct bNodeStack;

struct bNodeSocket *node_add_socket_from_template(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocketTemplate *stemp, int in_out);

void node_verify_socket_templates(struct bNodeTree *ntree, struct bNode *node);

void node_socket_init_default_value(struct bNodeSocket *sock);
void node_socket_copy_default_value(struct bNodeSocket *to, struct bNodeSocket *from);
void register_standard_node_socket_types(void);

#endif
