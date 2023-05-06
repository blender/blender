/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 Blender Foundation */

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
struct SpaceNode;
struct Tex;
struct View2D;
struct bContext;
struct bNode;
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
ENUM_OPERATORS(NodeBorder, NODE_RIGHT)

#define NODE_GRID_STEP_SIZE U.widget_unit /* Based on the grid nodes snap to. */
#define NODE_EDGE_PAN_INSIDE_PAD 2
#define NODE_EDGE_PAN_OUTSIDE_PAD 0 /* Disable clamping for node panning, use whole screen. */
#define NODE_EDGE_PAN_SPEED_RAMP 1
#define NODE_EDGE_PAN_MAX_SPEED 26 /* In UI units per second, slower than default. */
#define NODE_EDGE_PAN_DELAY 0.5f
#define NODE_EDGE_PAN_ZOOM_INFLUENCE 0.5f

/* clipboard.cc */

void ED_node_clipboard_free(void);

/* space_node.cc */

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

/* drawnode.cc */

void ED_node_init_butfuncs(void);
void ED_init_custom_node_type(struct bNodeType *ntype);
void ED_init_custom_node_socket_type(struct bNodeSocketType *stype);
void ED_init_standard_node_socket_type(struct bNodeSocketType *stype);
void ED_init_node_socket_type_virtual(struct bNodeSocketType *stype);
void ED_node_sample_set(const float col[4]);
void ED_node_draw_snap(
    struct View2D *v2d, const float cent[2], float size, NodeBorder border, unsigned int pos);
void ED_node_type_draw_color(const char *idname, float *r_color);

/* node_draw.cc */

void ED_node_tree_update(const struct bContext *C);
void ED_node_tag_update_id(struct ID *id);

float ED_node_grid_size(void);

/* node_edit.cc */

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

/* node_ops.cc */

void ED_operatormacros_node(void);

/* node_view.cc */

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
