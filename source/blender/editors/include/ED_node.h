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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_node.h
 *  \ingroup editors
 */

#ifndef __ED_NODE_H__
#define __ED_NODE_H__

struct ID;
struct Main;
struct Material;
struct Scene;
struct Tex;
struct bContext;
struct bNodeTree;
struct bNode;
struct bNodeTree;
struct ScrArea;
struct View2D;

typedef enum {
	NODE_TOP    = 1,
	NODE_BOTTOM = 2,
	NODE_LEFT   = 4,
	NODE_RIGHT  = 8
} NodeBorder;

/* drawnode.c */
void ED_init_node_butfuncs(void);

void drawnodesnap(struct View2D *v2d, const float cent[2], float size, NodeBorder border);

/* node_draw.c */
void ED_node_tree_update(struct SpaceNode *snode, struct Scene *scene);
void ED_node_changed_update(struct ID *id, struct bNode *node);
void ED_node_generic_update(struct Main *bmain, struct bNodeTree *ntree, struct bNode *node);
void ED_node_sort(struct bNodeTree *ntree);

/* node_edit.c */
void ED_node_shader_default(struct Scene *scene, struct ID *id);
void ED_node_composit_default(struct Scene *sce);
void ED_node_texture_default(struct Tex *tex);
void ED_node_link_intersect_test(struct ScrArea *sa, int test);
void ED_node_link_insert(struct ScrArea *sa);

void ED_node_post_apply_transform(struct bContext *C, struct bNodeTree *ntree);

void ED_node_set_active(struct Main *bmain, struct bNodeTree *ntree, struct bNode *node);

void ED_node_sample_set(const float col[4]);

/* node ops.c */
void ED_operatormacros_node(void);

#endif /* __ED_NODE_H__ */

