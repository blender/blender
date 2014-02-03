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
struct bNodeType;
struct bNodeSocketType;
struct bNodeTree;
struct bNodeTreeType;
struct ScrArea;
struct Scene;
struct View2D;

typedef enum {
	NODE_TOP    = 1,
	NODE_BOTTOM = 2,
	NODE_LEFT   = 4,
	NODE_RIGHT  = 8
} NodeBorder;

#define NODE_GRID_STEPS     5

/* space_node.c */
int ED_node_tree_path_length(struct SpaceNode *snode);
void ED_node_tree_path_get(struct SpaceNode *snode, char *value);
void ED_node_tree_path_get_fixedbuf(struct SpaceNode *snode, char *value, int max_length);

void ED_node_tree_start(struct SpaceNode *snode, struct bNodeTree *ntree, struct ID *id, struct ID *from);
void ED_node_tree_push(struct SpaceNode *snode, struct bNodeTree *ntree, struct bNode *gnode);
void ED_node_tree_pop(struct SpaceNode *snode);
int ED_node_tree_depth(struct SpaceNode *snode);
struct bNodeTree *ED_node_tree_get(struct SpaceNode *snode, int level);

void ED_node_set_active_viewer_key(struct SpaceNode *snode);

/* drawnode.c */
void ED_node_init_butfuncs(void);
void ED_init_custom_node_type(struct bNodeType *ntype);
void ED_init_custom_node_socket_type(struct bNodeSocketType *stype);
void ED_init_standard_node_socket_type(struct bNodeSocketType *stype);
void ED_init_node_socket_type_virtual(struct bNodeSocketType *stype);
void ED_node_sample_set(const float col[4]);
void ED_node_draw_snap(struct View2D *v2d, const float cent[2], float size, NodeBorder border);

/* node_draw.c */
void ED_node_tree_update(const struct bContext *C);
void ED_node_tag_update_id(struct ID *id);
void ED_node_tag_update_nodetree(struct Main *bmain, struct bNodeTree *ntree);
void ED_node_sort(struct bNodeTree *ntree);
float ED_node_grid_size(void);

/* node_relationships.c */
void ED_node_link_intersect_test(struct ScrArea *sa, int test);
void ED_node_link_insert(struct ScrArea *sa);

/* node_edit.c */
void ED_node_set_tree_type(struct SpaceNode *snode, struct bNodeTreeType *typeinfo);
bool ED_node_is_compositor(struct SpaceNode *snode);
bool ED_node_is_shader(struct SpaceNode *snode);
bool ED_node_is_texture(struct SpaceNode *snode);

void ED_node_shader_default(const struct bContext *C, struct ID *id);
void ED_node_composit_default(const struct bContext *C, struct Scene *scene);
void ED_node_texture_default(const struct bContext *C, struct Tex *tex);
bool ED_node_select_check(ListBase *lb);
void ED_node_post_apply_transform(struct bContext *C, struct bNodeTree *ntree);
void ED_node_set_active(struct Main *bmain, struct bNodeTree *ntree, struct bNode *node);

void ED_node_composite_job(const struct bContext *C, struct bNodeTree *nodetree, struct Scene *scene_owner);

/* node_ops.c */
void ED_operatormacros_node(void);

/* node_view.c */
int ED_space_node_color_sample(struct SpaceNode *snode, struct ARegion *ar, int mval[2], float r_col[3]);

#endif /* __ED_NODE_H__ */

