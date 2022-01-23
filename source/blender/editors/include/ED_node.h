/*
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
 */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct Main;
struct Scene;
struct ScrArea;
struct SpaceNode;
struct Tex;
struct View2D;
struct bContext;
struct bNode;
struct bNodeSocket;
struct bNodeSocketType;
struct bNodeTree;
struct bNodeTreeType;
struct bNodeType;

typedef enum {
  NODE_TOP = 1,
  NODE_BOTTOM = 2,
  NODE_LEFT = 4,
  NODE_RIGHT = 8,
} NodeBorder;

#define NODE_GRID_STEP_SIZE 10
#define NODE_EDGE_PAN_INSIDE_PAD 2
#define NODE_EDGE_PAN_OUTSIDE_PAD 0 /* Disable clamping for node panning, use whole screen. */
#define NODE_EDGE_PAN_SPEED_RAMP 1
#define NODE_EDGE_PAN_MAX_SPEED 26 /* In UI units per second, slower than default. */
#define NODE_EDGE_PAN_DELAY 0.5f
#define NODE_EDGE_PAN_ZOOM_INFLUENCE 0.5f

/* space_node.c */

void ED_node_cursor_location_get(const struct SpaceNode *snode, float value[2]);
void ED_node_cursor_location_set(struct SpaceNode *snode, const float value[2]);

int ED_node_tree_path_length(struct SpaceNode *snode);
void ED_node_tree_path_get(struct SpaceNode *snode, char *value);

void ED_node_tree_start(struct SpaceNode *snode,
                        struct bNodeTree *ntree,
                        struct ID *id,
                        struct ID *from);
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
void ED_node_draw_snap(
    struct View2D *v2d, const float cent[2], float size, NodeBorder border, unsigned int pos);

/* node_draw.cc */

/**
 * Draw a single node socket at default size.
 * \note this is only called from external code, internally #node_socket_draw_nested() is used for
 *       optimized drawing of multiple/all sockets of a node.
 */
void ED_node_socket_draw(struct bNodeSocket *sock,
                         const struct rcti *rect,
                         const float color[4],
                         float scale);
void ED_node_tree_update(const struct bContext *C);
void ED_node_tag_update_id(struct ID *id);

float ED_node_grid_size(void);

/* node_relationships.c */

/**
 * Test == 0, clear all intersect flags.
 */
void ED_node_link_intersect_test(struct ScrArea *area, int test);
/**
 * Assumes link with #NODE_LINKFLAG_HILITE set.
 */
void ED_node_link_insert(struct Main *bmain, struct ScrArea *area);

/* node_edit.c */

void ED_node_set_tree_type(struct SpaceNode *snode, struct bNodeTreeType *typeinfo);
bool ED_node_is_compositor(struct SpaceNode *snode);
bool ED_node_is_shader(struct SpaceNode *snode);
bool ED_node_is_texture(struct SpaceNode *snode);
bool ED_node_is_geometry(struct SpaceNode *snode);

/**
 * Assumes nothing being done in ntree yet, sets the default in/out node.
 * Called from shading buttons or header.
 */
void ED_node_shader_default(const struct bContext *C, struct ID *id);
/**
 * Assumes nothing being done in ntree yet, sets the default in/out node.
 * Called from shading buttons or header.
 */
void ED_node_composit_default(const struct bContext *C, struct Scene *scene);
/**
 * Assumes nothing being done in ntree yet, sets the default in/out node.
 * Called from shading buttons or header.
 */
void ED_node_texture_default(const struct bContext *C, struct Tex *tex);
void ED_node_post_apply_transform(struct bContext *C, struct bNodeTree *ntree);
void ED_node_set_active(struct Main *bmain,
                        struct SpaceNode *snode,
                        struct bNodeTree *ntree,
                        struct bNode *node,
                        bool *r_active_texture_changed);

/**
 * Call after one or more node trees have been changed and tagged accordingly.
 *
 * This function will make sure that other parts of Blender update accordingly. For example, if the
 * node group interface changed, parent node groups have to be updated as well.
 *
 * Additionally, this will send notifiers and tag the depsgraph based on the changes. Depsgraph
 * relation updates have to be triggered by the caller.
 *
 * \param C: Context if available. This can be null.
 * \param bmain: Main whose data-blocks should be updated based on the changes.
 * \param ntree: Under some circumstances the caller knows that only one node tree has
 *   changed since the last update. In this case the function may be able to skip scanning #bmain
 *   for other things that have to be changed. It may still scan #bmain if the interface of the
 *   node tree has changed.
 */
void ED_node_tree_propagate_change(const struct bContext *C,
                                   struct Main *bmain,
                                   struct bNodeTree *ntree);

/**
 * \param scene_owner: is the owner of the job,
 * we don't use it for anything else currently so could also be a void pointer,
 * but for now keep it an 'Scene' for consistency.
 *
 * \note only call from spaces `refresh` callbacks, not direct! - use with care.
 */
void ED_node_composite_job(const struct bContext *C,
                           struct bNodeTree *nodetree,
                           struct Scene *scene_owner);

/* node_ops.c */

void ED_operatormacros_node(void);

/* node_view.c */
/**
 * Returns mouse position in image space.
 */
bool ED_space_node_get_position(struct Main *bmain,
                                struct SpaceNode *snode,
                                struct ARegion *region,
                                const int mval[2],
                                float fpos[2]);
/**
 * Returns color in linear space, matching #ED_space_image_color_sample().
 * And here we've got recursion in the comments tips...
 */
bool ED_space_node_color_sample(struct Main *bmain,
                                struct SpaceNode *snode,
                                struct ARegion *region,
                                const int mval[2],
                                float r_col[3]);

#ifdef __cplusplus
}
#endif
