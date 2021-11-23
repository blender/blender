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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spnode
 */

#pragma once

#include "BKE_node.h"
#include "UI_interface.h"
#include "UI_view2d.h"

#include "BLI_vector.hh"
#include "UI_interface.hh"

#include <stddef.h> /* for size_t */

/* internal exports only */

struct ARegion;
struct ARegionType;
struct Main;
struct NodeInsertOfsData;
struct View2D;
struct bContext;
struct bNode;
struct bNodeLink;
struct bNodeSocket;
struct wmGizmoGroupType;
struct wmKeyConfig;
struct wmWindow;

/* temp data to pass on to modal */
struct bNodeLinkDrag {
  /** Links dragged by the operator. */
  blender::Vector<bNodeLink *> links;
  bool from_multi_input_socket;
  int in_out;

  /** Temporarily stores the last picked link from multi-input socket operator. */
  struct bNodeLink *last_picked_multi_input_socket_link;

  /** Temporarily stores the last hovered socket for multi-input socket operator.
   *  Store it to recalculate sorting after it is no longer hovered. */
  struct bNode *last_node_hovered_while_dragging_a_link;

  /* Data for edge panning */
  View2DEdgePanData pan_data;
};

struct SpaceNode_Runtime {
  float aspect;

  /** Mouse position for drawing socket-less links and adding nodes. */
  float cursor[2];

  /** For auto compositing. */
  bool recalc;

  /** Temporary data for modal linking operator. */
  std::unique_ptr<bNodeLinkDrag> linkdrag;

  /* XXX hack for translate_attach op-macros to pass data from transform op to insert_offset op */
  /** Temporary data for node insert offset (in UI called Auto-offset). */
  struct NodeInsertOfsData *iofsd;
};

/* space_node.c */

/* transform between View2Ds in the tree path */
void space_node_group_offset(SpaceNode *snode, float *x, float *y);

/* node_draw.cc */
float node_socket_calculate_height(const bNodeSocket *socket);
void node_link_calculate_multi_input_position(const float socket_x,
                                              const float socket_y,
                                              const int index,
                                              const int total_inputs,
                                              float r[2]);

int node_get_colorid(bNode *node);
void node_draw_extra_info_panel(const SpaceNode *snode, const bNode *node);
int node_get_resize_cursor(int directions);
void node_draw_shadow(const SpaceNode *snode, const bNode *node, float radius, float alpha);
void node_draw_default(const bContext *C,
                       ARegion *region,
                       SpaceNode *snode,
                       bNodeTree *ntree,
                       bNode *node,
                       bNodeInstanceKey key);
void node_draw_sockets(const View2D *v2d,
                       const bContext *C,
                       bNodeTree *ntree,
                       bNode *node,
                       bool draw_outputs,
                       bool select_all);
void node_update_default(const bContext *C, bNodeTree *ntree, bNode *node);
int node_select_area_default(bNode *node, int x, int y);
int node_tweak_area_default(bNode *node, int x, int y);
void node_socket_color_get(const bContext *C,
                           bNodeTree *ntree,
                           PointerRNA *node_ptr,
                           bNodeSocket *sock,
                           float r_color[4]);
void node_update_nodetree(const bContext *C, bNodeTree *ntree);
void node_draw_nodetree(const bContext *C,
                        ARegion *region,
                        SpaceNode *snode,
                        bNodeTree *ntree,
                        bNodeInstanceKey parent_key);
void node_draw_space(const bContext *C, ARegion *region);

void node_set_cursor(wmWindow *win, SpaceNode *snode, float cursor[2]);
/* DPI scaled coords */
void node_to_view(const bNode *node, float x, float y, float *rx, float *ry);
void node_to_updated_rect(const bNode *node, rctf *r_rect);
void node_from_view(const bNode *node, float x, float y, float *rx, float *ry);

/* node_toolbar.c */
void node_toolbar_register(ARegionType *art);

/* node_ops.c */
void node_operatortypes(void);
void node_keymap(wmKeyConfig *keyconf);

/* node_select.c */
void node_deselect_all(SpaceNode *snode);
void node_socket_select(bNode *node, bNodeSocket *sock);
void node_socket_deselect(bNode *node, bNodeSocket *sock, const bool deselect_node);
void node_deselect_all_input_sockets(SpaceNode *snode, const bool deselect_nodes);
void node_deselect_all_output_sockets(SpaceNode *snode, const bool deselect_nodes);
void node_select_single(bContext *C, bNode *node);

void NODE_OT_select(wmOperatorType *ot);
void NODE_OT_select_all(wmOperatorType *ot);
void NODE_OT_select_linked_to(wmOperatorType *ot);
void NODE_OT_select_linked_from(wmOperatorType *ot);
void NODE_OT_select_box(wmOperatorType *ot);
void NODE_OT_select_circle(wmOperatorType *ot);
void NODE_OT_select_lasso(wmOperatorType *ot);
void NODE_OT_select_grouped(wmOperatorType *ot);
void NODE_OT_select_same_type_step(wmOperatorType *ot);
void NODE_OT_find_node(wmOperatorType *ot);

/* node_view.c */
int space_node_view_flag(
    bContext *C, SpaceNode *snode, ARegion *region, const int node_flag, const int smooth_viewtx);

void NODE_OT_view_all(wmOperatorType *ot);
void NODE_OT_view_selected(wmOperatorType *ot);
void NODE_OT_geometry_node_view_legacy(wmOperatorType *ot);

void NODE_OT_backimage_move(wmOperatorType *ot);
void NODE_OT_backimage_zoom(wmOperatorType *ot);
void NODE_OT_backimage_fit(wmOperatorType *ot);
void NODE_OT_backimage_sample(wmOperatorType *ot);

/* drawnode.c */
void nodelink_batch_start(SpaceNode *snode);
void nodelink_batch_end(SpaceNode *snode);

void node_draw_link(const bContext *C,
                    const View2D *v2d,
                    const SpaceNode *snode,
                    const bNodeLink *link);
void node_draw_link_bezier(const bContext *C,
                           const View2D *v2d,
                           const SpaceNode *snode,
                           const bNodeLink *link,
                           int th_col1,
                           int th_col2,
                           int th_col3);
bool node_link_bezier_points(const View2D *v2d,
                             const SpaceNode *snode,
                             const bNodeLink *link,
                             float coord_array[][2],
                             const int resol);
bool node_link_bezier_handles(const View2D *v2d,
                              const SpaceNode *snode,
                              const bNodeLink *link,
                              float vec[4][2]);
void draw_nodespace_back_pix(const bContext *C,
                             ARegion *region,
                             SpaceNode *snode,
                             bNodeInstanceKey parent_key);

/* node_add.c */
bNode *node_add_node(const bContext *C, const char *idname, int type, float locx, float locy);
void NODE_OT_add_reroute(wmOperatorType *ot);
void NODE_OT_add_group(wmOperatorType *ot);
void NODE_OT_add_object(wmOperatorType *ot);
void NODE_OT_add_collection(wmOperatorType *ot);
void NODE_OT_add_texture(wmOperatorType *ot);
void NODE_OT_add_file(wmOperatorType *ot);
void NODE_OT_add_mask(wmOperatorType *ot);
void NODE_OT_new_node_tree(wmOperatorType *ot);

/* node_group.c */
const char *node_group_idname(bContext *C);
void NODE_OT_group_make(wmOperatorType *ot);
void NODE_OT_group_insert(wmOperatorType *ot);
void NODE_OT_group_ungroup(wmOperatorType *ot);
void NODE_OT_group_separate(wmOperatorType *ot);
void NODE_OT_group_edit(wmOperatorType *ot);

/* node_relationships.c */
void sort_multi_input_socket_links(SpaceNode *snode,
                                   bNode *node,
                                   bNodeLink *drag_link,
                                   float cursor[2]);
bool node_connected_to_output(Main *bmain, bNodeTree *ntree, bNode *node);

void NODE_OT_link(wmOperatorType *ot);
void NODE_OT_link_make(wmOperatorType *ot);
void NODE_OT_links_cut(wmOperatorType *ot);
void NODE_OT_links_detach(wmOperatorType *ot);
void NODE_OT_links_mute(wmOperatorType *ot);

void NODE_OT_parent_set(wmOperatorType *ot);
void NODE_OT_join(wmOperatorType *ot);
void NODE_OT_attach(wmOperatorType *ot);
void NODE_OT_detach(wmOperatorType *ot);

void NODE_OT_link_viewer(wmOperatorType *ot);

void NODE_OT_insert_offset(wmOperatorType *ot);

/* node_edit.c */
void snode_notify(bContext *C, SpaceNode *snode);
void snode_dag_update(bContext *C, SpaceNode *snode);
void snode_set_context(const bContext *C);

void snode_update(SpaceNode *snode, bNode *node);
bool composite_node_active(bContext *C);
bool composite_node_editable(bContext *C);

bool node_has_hidden_sockets(bNode *node);
void node_set_hidden_sockets(SpaceNode *snode, bNode *node, int set);
int node_render_changed_exec(bContext *, wmOperator *);
bool node_find_indicated_socket(
    SpaceNode *snode, bNode **nodep, bNodeSocket **sockp, const float cursor[2], int in_out);
float node_link_dim_factor(const View2D *v2d, const bNodeLink *link);
bool node_link_is_hidden_or_dimmed(const View2D *v2d, const bNodeLink *link);

void NODE_OT_duplicate(wmOperatorType *ot);
void NODE_OT_delete(wmOperatorType *ot);
void NODE_OT_delete_reconnect(wmOperatorType *ot);
void NODE_OT_resize(wmOperatorType *ot);

void NODE_OT_mute_toggle(wmOperatorType *ot);
void NODE_OT_hide_toggle(wmOperatorType *ot);
void NODE_OT_hide_socket_toggle(wmOperatorType *ot);
void NODE_OT_preview_toggle(wmOperatorType *ot);
void NODE_OT_options_toggle(wmOperatorType *ot);
void NODE_OT_node_copy_color(wmOperatorType *ot);

void NODE_OT_read_viewlayers(wmOperatorType *ot);
void NODE_OT_render_changed(wmOperatorType *ot);

void NODE_OT_output_file_add_socket(wmOperatorType *ot);
void NODE_OT_output_file_remove_active_socket(wmOperatorType *ot);
void NODE_OT_output_file_move_active_socket(wmOperatorType *ot);

void NODE_OT_switch_view_update(wmOperatorType *ot);

/* NOTE: clipboard_cut is a simple macro of copy + delete. */
void NODE_OT_clipboard_copy(wmOperatorType *ot);
void NODE_OT_clipboard_paste(wmOperatorType *ot);

void NODE_OT_tree_socket_add(wmOperatorType *ot);
void NODE_OT_tree_socket_remove(wmOperatorType *ot);
void NODE_OT_tree_socket_change_type(wmOperatorType *ot);
void NODE_OT_tree_socket_move(wmOperatorType *ot);

void NODE_OT_shader_script_update(wmOperatorType *ot);

void NODE_OT_viewer_border(wmOperatorType *ot);
void NODE_OT_clear_viewer_border(wmOperatorType *ot);

/* node_widgets.c */
void NODE_GGT_backdrop_transform(wmGizmoGroupType *gzgt);
void NODE_GGT_backdrop_crop(wmGizmoGroupType *gzgt);
void NODE_GGT_backdrop_sun_beams(wmGizmoGroupType *gzgt);
void NODE_GGT_backdrop_corner_pin(wmGizmoGroupType *gzgt);

void NODE_OT_cryptomatte_layer_add(wmOperatorType *ot);
void NODE_OT_cryptomatte_layer_remove(wmOperatorType *ot);

/* node_geometry_attribute_search.cc */
void node_geometry_add_attribute_search_button(const bContext *C,
                                               const bNodeTree *node_tree,
                                               const bNode *node,
                                               PointerRNA *socket_ptr,
                                               uiLayout *layout);

extern const char *node_context_dir[];

/* XXXXXX */

/* Nodes draw without dpi - the view zoom is flexible. */
#define HIDDEN_RAD (0.75f * U.widget_unit)
#define BASIS_RAD (0.2f * U.widget_unit)
#define NODE_DYS (U.widget_unit / 2)
#define NODE_DY U.widget_unit
#define NODE_SOCKDY (0.1f * U.widget_unit)
#define NODE_WIDTH(node) (node->width * UI_DPI_FAC)
#define NODE_HEIGHT(node) (node->height * UI_DPI_FAC)
#define NODE_MARGIN_X (1.2f * U.widget_unit)
#define NODE_SOCKSIZE (0.25f * U.widget_unit)
#define NODE_MULTI_INPUT_LINK_GAP (0.25f * U.widget_unit)
#define NODE_RESIZE_MARGIN (0.20f * U.widget_unit)
#define NODE_LINK_RESOL 12

namespace blender::ed::space_node {
Vector<ui::ContextPathItem> context_path_for_space_node(const bContext &C);
}
