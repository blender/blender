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

/** \file blender/nodes/intern/node_util.h
 *  \ingroup nodes
 */


#ifndef __NODE_UTIL_H__
#define __NODE_UTIL_H__

#include "DNA_listBase.h"

#include "BLI_utildefines.h"

#include "BKE_node.h"

#include "MEM_guardedalloc.h"

#include "NOD_socket.h"

#include "GPU_material.h" /* For Shader muting GPU code... */

#include "RNA_access.h"

struct bNodeTree;
struct bNode;

/* data for initializing node execution */
typedef struct bNodeExecContext {
	struct bNodeInstanceHash *previews;
} bNodeExecContext;

typedef struct bNodeExecData {
	void *data;						/* custom data storage */
	struct bNodePreview *preview;	/* optional preview image */
} bNodeExecData;

/**** Storage Data ****/

extern void node_free_curves(struct bNode *node);
extern void node_free_standard_storage(struct bNode *node);

extern void node_copy_curves(struct bNodeTree *dest_ntree, struct bNode *dest_node, struct bNode *src_node);
extern void node_copy_standard_storage(struct bNodeTree *dest_ntree, struct bNode *dest_node, struct bNode *src_node);
extern void *node_initexec_curves(struct bNodeExecContext *context, struct bNode *node, bNodeInstanceKey key);

/**** Labels ****/

void node_blend_label(struct bNodeTree *ntree, struct bNode *node, char *label, int maxlen);
void node_math_label(struct bNodeTree *ntree, struct bNode *node, char *label, int maxlen);
void node_vect_math_label(struct bNodeTree *ntree, struct bNode *node, char *label, int maxlen);
void node_filter_label(struct bNodeTree *ntree, struct bNode *node, char *label, int maxlen);

void node_update_internal_links_default(struct bNodeTree *ntree, struct bNode *node);

float node_socket_get_float(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *sock);
void node_socket_set_float(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *sock, float value);
void node_socket_get_color(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *sock, float *value);
void node_socket_set_color(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *sock, const float *value);
void node_socket_get_vector(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *sock, float *value);
void node_socket_set_vector(struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *sock, const float *value);

#endif
