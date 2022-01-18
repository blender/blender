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

#include "BLI_math_vec_types.hh"
#include "BLI_vector.hh"

#include "BKE_node.h"

#include "UI_interface.h"
#include "UI_interface.hh"
#include "UI_view2d.h"

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

/** Temporary data used in node link drag modal operator. */
struct bNodeLinkDrag {
  /** Links dragged by the operator. */
  blender::Vector<bNodeLink *> links;
  bool from_multi_input_socket;
  eNodeSocketInOut in_out;

  /** Draw handler for the "+" icon when dragging a link in empty space. */
  void *draw_handle;

  /** Temporarily stores the last picked link from multi-input socket operator. */
  bNodeLink *last_picked_multi_input_socket_link;

  /**
   * Temporarily stores the last hovered socket for multi-input socket operator.
   * Store it to recalculate sorting after it is no longer hovered.
   */
  bNode *last_node_hovered_while_dragging_a_link;

  /* The cursor position, used for drawing a + icon when dragging a node link. */
  std::array<int, 2> cursor;

  /** The node the drag started at. */
  bNode *start_node;
  /** The socket the drag started at. */
  bNodeSocket *start_socket;
  /** The number of links connected to the #start_socket when the drag started. */
  int start_link_count;

  /* Data for edge panning */
  View2DEdgePanData pan_data;
};

struct SpaceNode_Runtime {
  float aspect;

  /** Mouse position for drawing socket-less links and adding nodes. */
  blender::float2 cursor;

  /** For auto compositing. */
  bool recalc;

  /** Temporary data for modal linking operator. */
  std::unique_ptr<bNodeLinkDrag> linkdrag;

  /* XXX hack for translate_attach op-macros to pass data from transform op to insert_offset op */
  /** Temporary data for node insert offset (in UI called Auto-offset). */
  struct NodeInsertOfsData *iofsd;
};

enum NodeResizeDirection {
  NODE_RESIZE_NONE = 0,
  NODE_RESIZE_TOP = (1 << 0),
  NODE_RESIZE_BOTTOM = (1 << 1),
  NODE_RESIZE_RIGHT = (1 << 2),
  NODE_RESIZE_LEFT = (1 << 3),
};
ENUM_OPERATORS(NodeResizeDirection, NODE_RESIZE_LEFT);

/**
 * Transform between View2Ds in the tree path.
 */
blender::float2 space_node_group_offset(const SpaceNode &snode);

float node_socket_calculate_height(const bNodeSocket &socket);
blender::float2 node_link_calculate_multi_input_position(const blender::float2 &socket_position,
                                                         int index,
                                                         int total_inputs);

int node_get_resize_cursor(NodeResizeDirection directions);
NodeResizeDirection node_get_resize_direction(const bNode *node, int x, int y);
/**
 * Usual convention here would be #node_socket_get_color(),
 * but that's already used (for setting a color property socket).
 */
void node_socket_color_get(const bContext &C,
                           const bNodeTree &ntree,
                           PointerRNA &node_ptr,
                           const bNodeSocket &sock,
                           float r_color[4]);
void node_draw_space(const bContext &C, ARegion &region);

/**
 * Sort nodes by selection: unselected nodes first, then selected,
 * then the active node at the very end. Relative order is kept intact.
 */
void node_sort(bNodeTree &ntree);

void node_set_cursor(wmWindow &win, SpaceNode &snode, const blender::float2 &cursor);
/* DPI scaled coords */
blender::float2 node_to_view(const bNode &node, const blender::float2 &co);
void node_to_updated_rect(const bNode &node, rctf &r_rect);
blender::float2 node_from_view(const bNode &node, const blender::float2 &co);

void node_operatortypes();
void node_keymap(wmKeyConfig *keyconf);

void node_deselect_all(SpaceNode &snode);
void node_socket_select(bNode *node, bNodeSocket &sock);
void node_socket_deselect(bNode *node, bNodeSocket &sock, bool deselect_node);
void node_deselect_all_input_sockets(SpaceNode &snode, bool deselect_nodes);
void node_deselect_all_output_sockets(SpaceNode &snode, bool deselect_nodes);
void node_select_single(bContext &C, bNode &node);

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

bool space_node_view_flag(
    bContext &C, SpaceNode &snode, ARegion &region, int node_flag, int smooth_viewtx);

void NODE_OT_view_all(wmOperatorType *ot);
void NODE_OT_view_selected(wmOperatorType *ot);
void NODE_OT_geometry_node_view_legacy(wmOperatorType *ot);

void NODE_OT_backimage_move(wmOperatorType *ot);
void NODE_OT_backimage_zoom(wmOperatorType *ot);
void NODE_OT_backimage_fit(wmOperatorType *ot);
void NODE_OT_backimage_sample(wmOperatorType *ot);

void nodelink_batch_start(SpaceNode &snode);
void nodelink_batch_end(SpaceNode &snode);

/**
 * \note this is used for fake links in groups too.
 */
void node_draw_link(const bContext &C,
                    const View2D &v2d,
                    const SpaceNode &snode,
                    const bNodeLink &link);
/**
 * Don't do shadows if th_col3 is -1.
 */
void node_draw_link_bezier(const bContext &C,
                           const View2D &v2d,
                           const SpaceNode &snode,
                           const bNodeLink &link,
                           int th_col1,
                           int th_col2,
                           int th_col3);
/** If v2d not nullptr, it clips and returns 0 if not visible. */
bool node_link_bezier_points(const View2D *v2d,
                             const SpaceNode *snode,
                             const bNodeLink &link,
                             float coord_array[][2],
                             int resol);
/**
 * Return quadratic beziers points for a given nodelink and clip if v2d is not nullptr.
 */
bool node_link_bezier_handles(const View2D *v2d,
                              const SpaceNode *snode,
                              const bNodeLink &ink,
                              float vec[4][2]);
void draw_nodespace_back_pix(const bContext &C,
                             ARegion &region,
                             SpaceNode &snode,
                             bNodeInstanceKey parent_key);

void node_select_all(ListBase *lb, int action);

/**
 * XXX Does some additional initialization on top of #nodeAddNode
 * Can be used with both custom and static nodes,
 * if `idname == nullptr` the static int type will be used instead.
 */
bNode *node_add_node(const bContext &C, const char *idname, int type, float locx, float locy);
void NODE_OT_add_reroute(wmOperatorType *ot);
void NODE_OT_add_group(wmOperatorType *ot);
void NODE_OT_add_object(wmOperatorType *ot);
void NODE_OT_add_collection(wmOperatorType *ot);
void NODE_OT_add_texture(wmOperatorType *ot);
void NODE_OT_add_file(wmOperatorType *ot);
void NODE_OT_add_mask(wmOperatorType *ot);
void NODE_OT_new_node_tree(wmOperatorType *ot);

const char *node_group_idname(bContext *C);
void NODE_OT_group_make(wmOperatorType *ot);
void NODE_OT_group_insert(wmOperatorType *ot);
void NODE_OT_group_ungroup(wmOperatorType *ot);
void NODE_OT_group_separate(wmOperatorType *ot);
void NODE_OT_group_edit(wmOperatorType *ot);

void sort_multi_input_socket_links(SpaceNode &snode,
                                   bNode &node,
                                   bNodeLink *drag_link,
                                   const blender::float2 *cursor);

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

void snode_set_context(const bContext &C);

bool composite_node_active(bContext *C);
/** Operator poll callback. */
bool composite_node_editable(bContext *C);

bool node_has_hidden_sockets(bNode *node);
void node_set_hidden_sockets(SpaceNode *snode, bNode *node, int set);
int node_render_changed_exec(bContext *, wmOperator *);
/** Type is #SOCK_IN and/or #SOCK_OUT. */
bool node_find_indicated_socket(SpaceNode &snode,
                                bNode **nodep,
                                bNodeSocket **sockp,
                                const blender::float2 &cursor,
                                eNodeSocketInOut in_out);
float node_link_dim_factor(const View2D &v2d, const bNodeLink &link);
bool node_link_is_hidden_or_dimmed(const View2D &v2d, const bNodeLink &link);

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

/**
 * \note clipboard_cut is a simple macro of copy + delete.
 */
void NODE_OT_clipboard_copy(wmOperatorType *ot);
void NODE_OT_clipboard_paste(wmOperatorType *ot);

void NODE_OT_tree_socket_add(wmOperatorType *ot);
void NODE_OT_tree_socket_remove(wmOperatorType *ot);
void NODE_OT_tree_socket_change_type(wmOperatorType *ot);
void NODE_OT_tree_socket_move(wmOperatorType *ot);

void NODE_OT_shader_script_update(wmOperatorType *ot);

void NODE_OT_viewer_border(wmOperatorType *ot);
void NODE_OT_clear_viewer_border(wmOperatorType *ot);

void NODE_GGT_backdrop_transform(wmGizmoGroupType *gzgt);
void NODE_GGT_backdrop_crop(wmGizmoGroupType *gzgt);
void NODE_GGT_backdrop_sun_beams(wmGizmoGroupType *gzgt);
void NODE_GGT_backdrop_corner_pin(wmGizmoGroupType *gzgt);

void NODE_OT_cryptomatte_layer_add(wmOperatorType *ot);
void NODE_OT_cryptomatte_layer_remove(wmOperatorType *ot);

void node_geometry_add_attribute_search_button(const bContext &C,
                                               const bNodeTree &node_tree,
                                               const bNode &node,
                                               PointerRNA &socket_ptr,
                                               uiLayout &layout);

extern const char *node_context_dir[];

/* Nodes draw without dpi - the view zoom is flexible. */
#define HIDDEN_RAD (0.75f * U.widget_unit)
#define BASIS_RAD (0.2f * U.widget_unit)
#define NODE_DYS (U.widget_unit / 2)
#define NODE_DY U.widget_unit
#define NODE_SOCKDY (0.1f * U.widget_unit)
#define NODE_WIDTH(node) (node.width * UI_DPI_FAC)
#define NODE_HEIGHT(node) (node.height * UI_DPI_FAC)
#define NODE_MARGIN_X (1.2f * U.widget_unit)
#define NODE_SOCKSIZE (0.25f * U.widget_unit)
#define NODE_MULTI_INPUT_LINK_GAP (0.25f * U.widget_unit)
#define NODE_RESIZE_MARGIN (0.20f * U.widget_unit)
#define NODE_LINK_RESOL 12

namespace blender::ed::space_node {

Vector<ui::ContextPathItem> context_path_for_space_node(const bContext &C);

void invoke_node_link_drag_add_menu(bContext &C,
                                    bNode &node,
                                    bNodeSocket &socket,
                                    const float2 &cursor);

}  // namespace blender::ed::space_node
