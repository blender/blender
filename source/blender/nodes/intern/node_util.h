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

#include "BKE_node.h"

#include "MEM_guardedalloc.h"

#include "NOD_socket.h"

#include "GPU_material.h" /* For Shader muting GPU code... */

struct bNodeTree;
struct bNode;

/**** Storage Data ****/

extern void node_free_curves(struct bNode *node);
extern void node_free_standard_storage(struct bNode *node);

extern void node_copy_curves(struct bNode *orig_node, struct bNode *new_node);
extern void node_copy_standard_storage(struct bNode *orig_node, struct bNode *new_node);

/**** Labels ****/

const char *node_blend_label(struct bNode *node);
const char *node_math_label(struct bNode *node);
const char *node_vect_math_label(struct bNode *node);
const char *node_filter_label(struct bNode *node);

ListBase node_internal_connect_default(struct bNodeTree *ntree, struct bNode *node);

#endif

// this is needed for inlining behaviour
#if defined _WIN32
#   define DO_INLINE __inline
#elif defined (__sun) || defined (__sun__)
#   define DO_INLINE
#else
#   define DO_INLINE static inline
#endif

