/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct ARegion;
struct ID;
struct Main;
struct Scene;
struct SpaceNode;
struct Tex;
struct View2D;
struct bContext;
struct bNode;
struct bNodeTree;
namespace blender::bke {
struct bNodeTreeType;
struct bNodeType;
struct bNodeSocketType;
}  // namespace blender::bke

#define NODE_GRID_STEP_SIZE (20.0f * UI_SCALE_FAC) /* Based on the grid nodes snap to. */
#define NODE_EDGE_PAN_INSIDE_PAD 2
#define NODE_EDGE_PAN_OUTSIDE_PAD 0 /* Disable clamping for node panning, use whole screen. */
#define NODE_EDGE_PAN_SPEED_RAMP 1
#define NODE_EDGE_PAN_MAX_SPEED 26 /* In UI units per second, slower than default. */
#define NODE_EDGE_PAN_DELAY 0.5f
#define NODE_EDGE_PAN_ZOOM_INFLUENCE 0.5f

/* `clipboard.cc` */

void ED_node_clipboard_free();

/* `space_node.cc` */

void ED_node_cursor_location_get(const SpaceNode *snode, float value[2]);
void ED_node_cursor_location_set(SpaceNode *snode, const float value[2]);

int ED_node_tree_path_length(SpaceNode *snode);
/**
 * \param value: The path output at least the size of `ED_node_tree_path_length(snode) + 1`.
 */
void ED_node_tree_path_get(SpaceNode *snode, char *value);

void ED_node_tree_start(ARegion *region, SpaceNode *snode, bNodeTree *ntree, ID *id, ID *from);
void ED_node_tree_push(ARegion *region, SpaceNode *snode, bNodeTree *ntree, bNode *gnode);
void ED_node_tree_pop(ARegion *region, SpaceNode *snode);
int ED_node_tree_depth(SpaceNode *snode);
bNodeTree *ED_node_tree_get(SpaceNode *snode, int level);

void ED_node_set_active_viewer_key(SpaceNode *snode);

/* `drawnode.cc` */

void ED_node_init_butfuncs();
void ED_init_custom_node_type(blender::bke::bNodeType *ntype);
void ED_init_custom_node_socket_type(blender::bke::bNodeSocketType *stype);
void ED_init_standard_node_socket_type(blender::bke::bNodeSocketType *stype);
void ED_init_node_socket_type_virtual(blender::bke::bNodeSocketType *stype);
void ED_node_sample_set(const float col[4]);
void ED_node_type_draw_color(const char *idname, float *r_color);

/* `node_edit.cc` */

void ED_node_set_tree_type(SpaceNode *snode, blender::bke::bNodeTreeType *typeinfo);
bool ED_node_is_compositor(const SpaceNode *snode);
bool ED_node_is_shader(SpaceNode *snode);
bool ED_node_is_texture(SpaceNode *snode);
bool ED_node_is_geometry(const SpaceNode *snode);
bool ED_node_supports_preview(SpaceNode *snode);

/**
 * Assumes nothing being done in ntree yet, sets the default in/out node.
 * Called from shading buttons or header.
 */
void ED_node_shader_default(const bContext *C, Main *bmain, ID *id);

/**
 * Initializes an empty compositing node tree with default nodes.
 */
void ED_node_composit_default_init(const bContext *C, bNodeTree *ntree);
/**
 * Assumes nothing being done in ntree yet, sets the default in/out node.
 * Called from compositing buttons or header.
 */
void ED_node_composit_default(const bContext *C, Scene *scene);
/**
 * Assumes nothing being done in ntree yet, sets the default in/out node.
 * Called from shading buttons or header.
 */
void ED_node_texture_default(const bContext *C, Tex *tex);
void ED_node_post_apply_transform(bContext *C, bNodeTree *ntree);
void ED_node_set_active(
    Main *bmain, SpaceNode *snode, bNodeTree *ntree, bNode *node, bool *r_active_texture_changed);

/**
 * \param scene_owner: is the owner of the job,
 * we don't use it for anything else currently so could also be a void pointer,
 * but for now keep it an 'Scene' for consistency.
 *
 * \note only call from spaces `refresh` callbacks, not direct! - use with care.
 */
void ED_node_composite_job(const bContext *C, bNodeTree *nodetree, Scene *scene_owner);

/* `node_ops.cc` */

void ED_operatormacros_node();

/* `node_view.cc` */

/**
 * Returns mouse position in image space.
 */
bool ED_space_node_get_position(
    Main *bmain, SpaceNode *snode, ARegion *region, const int mval[2], float fpos[2]);
/**
 * Returns color in linear space, matching #ED_space_image_color_sample().
 * And here we've got recursion in the comments tips...
 */
bool ED_space_node_color_sample(
    Main *bmain, SpaceNode *snode, ARegion *region, const int mval[2], float r_col[3]);
