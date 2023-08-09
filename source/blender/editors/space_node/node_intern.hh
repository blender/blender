/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 */

#pragma once

#include "BLI_compute_context.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "BKE_node.hh"

#include "UI_interface.hh"
#include "UI_view2d.hh"

struct ARegion;
struct NodeInsertOfsData;
struct View2D;
struct bContext;
struct bNode;
struct bNodeLink;
struct bNodeSocket;
struct wmGizmoGroupType;
struct wmKeyConfig;
struct wmWindow;

/* Outside of blender namespace to avoid Python documentation build error with `ctypes`. */
extern "C" {
extern const char *node_context_dir[];
};

namespace blender::ed::asset {
struct AssetItemTree;
}

namespace blender::ed::space_node {
struct NestedTreePreviews;

/** Temporary data used in node link drag modal operator. */
struct bNodeLinkDrag {
  /** Links dragged by the operator. */
  Vector<bNodeLink> links;
  eNodeSocketInOut in_out;

  /** Draw handler for the tooltip icon when dragging a link in empty space. */
  void *draw_handle;

  /** Temporarily stores the last picked link from multi-input socket operator. */
  bNodeLink *last_picked_multi_input_socket_link;

  /**
   * Temporarily stores the last hovered node for multi-input socket operator.
   * Store it to recalculate sorting after it is no longer hovered.
   */
  bNode *last_node_hovered_while_dragging_a_link;

  /**
   * Temporarily stores the currently hovered socket for link swapping to allow reliably swap links
   * even when dragging multiple links at once. `nullptr`, when no socket is hovered.
   */
  bNodeSocket *hovered_socket;

  /* The cursor position, used for drawing a + icon when dragging a node link. */
  std::array<int, 2> cursor;

  /** The node the drag started at. */
  bNode *start_node;
  /** The socket the drag started at. */
  bNodeSocket *start_socket;
  /** The number of links connected to the #start_socket when the drag started. */
  int start_link_count;

  bool swap_links = false;

  /* Data for edge panning */
  View2DEdgePanData pan_data;
};

struct SpaceNode_Runtime {
  float aspect;

  /** Mouse position for drawing socket-less links and adding nodes. */
  float2 cursor;

  /**
   * Indicates that the compositing tree in the space needs to be re-evaluated using the
   * auto-compositing pipeline.
   * Takes priority over the regular compositing.
   */
  bool recalc_auto_compositing;

  /**
   * Indicates that the compositing int the space  tree needs to be re-evaluated using
   * regular compositing pipeline.
   */
  bool recalc_regular_compositing;

  /** Temporary data for modal linking operator. */
  std::unique_ptr<bNodeLinkDrag> linkdrag;

  /* XXX hack for translate_attach op-macros to pass data from transform op to insert_offset op */
  /** Temporary data for node insert offset (in UI called Auto-offset). */
  NodeInsertOfsData *iofsd;

  /**
   * Use this to store data for the displayed node tree. It has an entry for every distinct
   * nested node-group.
   */
  Map<ComputeContextHash, std::unique_ptr<space_node::NestedTreePreviews>>
      tree_previews_per_context;

  /**
   * Temporary data for node add menu in order to provide longer-term storage for context pointers.
   * Recreated every time the root menu is opened. In the future this will be replaced with an "all
   * libraries" cache in the asset system itself.
   *
   * Stored with a shared pointer so that it can be forward declared.
   */
  std::shared_ptr<asset::AssetItemTree> assets_for_menu;
};

enum NodeResizeDirection {
  NODE_RESIZE_NONE = 0,
  NODE_RESIZE_TOP = (1 << 0),
  NODE_RESIZE_BOTTOM = (1 << 1),
  NODE_RESIZE_RIGHT = (1 << 2),
  NODE_RESIZE_LEFT = (1 << 3),
};
ENUM_OPERATORS(NodeResizeDirection, NODE_RESIZE_LEFT);

/* Nodes draw without DPI - the view zoom is flexible. */
#define HIDDEN_RAD (0.75f * U.widget_unit)
#define BASIS_RAD (0.2f * U.widget_unit)
#define NODE_DYS (U.widget_unit / 2)
#define NODE_DY U.widget_unit
#define NODE_SOCKDY (0.1f * U.widget_unit)
#define NODE_WIDTH(node) (node.width * UI_SCALE_FAC)
#define NODE_HEIGHT(node) (node.height * UI_SCALE_FAC)
#define NODE_MARGIN_X (1.2f * U.widget_unit)
#define NODE_SOCKSIZE (0.25f * U.widget_unit)
#define NODE_SOCKSIZE_DRAW_MULIPLIER 2.25f
#define NODE_SOCK_OUTLINE_SCALE 1.0f
#define NODE_MULTI_INPUT_LINK_GAP (0.25f * U.widget_unit)
#define NODE_RESIZE_MARGIN (0.20f * U.widget_unit)
#define NODE_LINK_RESOL 12

/* `space_node.cc` */

/**
 * Transform between View2Ds in the tree path.
 */
float2 space_node_group_offset(const SpaceNode &snode);

int node_get_resize_cursor(NodeResizeDirection directions);
/**
 * Usual convention here would be #node_socket_get_color(),
 * but that's already used (for setting a color property socket).
 */
void node_socket_color_get(const bContext &C,
                           const bNodeTree &ntree,
                           PointerRNA &node_ptr,
                           const bNodeSocket &sock,
                           float r_color[4]);

/* `node_draw.cc` */

void node_draw_space(const bContext &C, ARegion &region);

void node_socket_add_tooltip(const bNodeTree &ntree, const bNodeSocket &sock, uiLayout &layout);

/**
 * Sort nodes by selection: unselected nodes first, then selected,
 * then the active node at the very end. Relative order is kept intact.
 */
void node_sort(bNodeTree &ntree);

void node_set_cursor(wmWindow &win, SpaceNode &snode, const float2 &cursor);
/* DPI scaled coords */
float2 node_to_view(const bNode &node, const float2 &co);
void node_to_updated_rect(const bNode &node, rctf &r_rect);
float2 node_from_view(const bNode &node, const float2 &co);

/* `node_ops.cc` */

void node_operatortypes();
void node_keymap(wmKeyConfig *keyconf);

/* `node_select.cc` */

rctf node_frame_rect_inside(const SpaceNode &snode, const bNode &node);
bool node_or_socket_isect_event(const bContext &C, const wmEvent &event);

void node_deselect_all(bNodeTree &node_tree);
void node_socket_select(bNode *node, bNodeSocket &sock);
void node_socket_deselect(bNode *node, bNodeSocket &sock, bool deselect_node);
void node_deselect_all_input_sockets(bNodeTree &node_tree, bool deselect_nodes);
void node_deselect_all_output_sockets(bNodeTree &node_tree, bool deselect_nodes);
/**
 * Select nodes that are paired to a selected node.
 */
void node_select_paired(bNodeTree &node_tree);
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

/* `node_view.cc` */

bool space_node_view_flag(
    bContext &C, SpaceNode &snode, ARegion &region, int node_flag, int smooth_viewtx);

void NODE_OT_view_all(wmOperatorType *ot);
void NODE_OT_view_selected(wmOperatorType *ot);

void NODE_OT_backimage_move(wmOperatorType *ot);
void NODE_OT_backimage_zoom(wmOperatorType *ot);
void NODE_OT_backimage_fit(wmOperatorType *ot);
void NODE_OT_backimage_sample(wmOperatorType *ot);

/* `drawnode.cc` */

NodeResizeDirection node_get_resize_direction(const SpaceNode &snode,
                                              const bNode *node,
                                              int x,
                                              int y);

void nodelink_batch_start(SpaceNode &snode);
void nodelink_batch_end(SpaceNode &snode);

/**
 * \note this is used for fake links in groups too.
 */
void node_draw_link(const bContext &C,
                    const View2D &v2d,
                    const SpaceNode &snode,
                    const bNodeLink &link,
                    bool selected);
void node_draw_link_dragged(const bContext &C,
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
                           int th_col3,
                           bool selected);

std::array<float2, 4> node_link_bezier_points_dragged(const SpaceNode &snode,
                                                      const bNodeLink &link);
void node_link_bezier_points_evaluated(const bNodeLink &link,
                                       std::array<float2, NODE_LINK_RESOL + 1> &coords);

std::optional<float2> link_path_intersection(const bNodeLink &link, Span<float2> path);

void draw_nodespace_back_pix(const bContext &C,
                             ARegion &region,
                             SpaceNode &snode,
                             bNodeInstanceKey parent_key);

/* `node_add.cc` */

bNode *add_node(const bContext &C, StringRef idname, const float2 &location);
bNode *add_static_node(const bContext &C, int type, const float2 &location);

void NODE_OT_add_reroute(wmOperatorType *ot);
void NODE_OT_add_search(wmOperatorType *ot);
void NODE_OT_add_group(wmOperatorType *ot);
void NODE_OT_add_group_asset(wmOperatorType *ot);
void NODE_OT_add_object(wmOperatorType *ot);
void NODE_OT_add_collection(wmOperatorType *ot);
void NODE_OT_add_file(wmOperatorType *ot);
void NODE_OT_add_mask(wmOperatorType *ot);
void NODE_OT_new_node_tree(wmOperatorType *ot);

/* `node_group.cc` */

const char *node_group_idname(bContext *C);
void NODE_OT_group_make(wmOperatorType *ot);
void NODE_OT_group_insert(wmOperatorType *ot);
void NODE_OT_group_ungroup(wmOperatorType *ot);
void NODE_OT_group_separate(wmOperatorType *ot);
void NODE_OT_group_edit(wmOperatorType *ot);

/* `node_relationships.cc` */

void update_multi_input_indices_for_removed_links(bNode &node);
bool all_links_muted(const bNodeSocket &socket);

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

wmKeyMap *node_link_modal_keymap(wmKeyConfig *keyconf);

/* `node_edit.cc` */

float2 node_link_calculate_multi_input_position(const float2 &socket_position,
                                                int index,
                                                int total_inputs);

float node_socket_calculate_height(const bNodeSocket &socket);

void snode_set_context(const bContext &C);

bool composite_node_active(bContext *C);
/** Operator poll callback. */
bool composite_node_editable(bContext *C);

bool node_has_hidden_sockets(bNode *node);
void node_set_hidden_sockets(bNode *node, int set);
bool node_is_previewable(const SpaceNode &snode, const bNodeTree &ntree, const bNode &node);
int node_render_changed_exec(bContext *, wmOperator *);
bNodeSocket *node_find_indicated_socket(SpaceNode &snode,
                                        const float2 &cursor,
                                        eNodeSocketInOut in_out);
float node_link_dim_factor(const View2D &v2d, const bNodeLink &link);
bool node_link_is_hidden_or_dimmed(const View2D &v2d, const bNodeLink &link);

void remap_node_pairing(bNodeTree &dst_tree, const Map<const bNode *, bNode *> &node_map);

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
void NODE_OT_deactivate_viewer(wmOperatorType *ot);

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
void NODE_OT_tree_socket_change_subtype(wmOperatorType *ot);
void NODE_OT_tree_socket_move(wmOperatorType *ot);

void NODE_OT_shader_script_update(wmOperatorType *ot);

void NODE_OT_viewer_border(wmOperatorType *ot);
void NODE_OT_clear_viewer_border(wmOperatorType *ot);

void NODE_OT_cryptomatte_layer_add(wmOperatorType *ot);
void NODE_OT_cryptomatte_layer_remove(wmOperatorType *ot);

/* `node_gizmo.cc` */

void NODE_GGT_backdrop_transform(wmGizmoGroupType *gzgt);
void NODE_GGT_backdrop_crop(wmGizmoGroupType *gzgt);
void NODE_GGT_backdrop_sun_beams(wmGizmoGroupType *gzgt);
void NODE_GGT_backdrop_corner_pin(wmGizmoGroupType *gzgt);

/* `node_geometry_attribute_search.cc` */

void node_geometry_add_attribute_search_button(const bContext &C,
                                               const bNode &node,
                                               PointerRNA &socket_ptr,
                                               uiLayout &layout);

/* `node_context_path.cc` */

Vector<ui::ContextPathItem> context_path_for_space_node(const bContext &C);

/* `link_drag_search.cc` */

void invoke_node_link_drag_add_menu(bContext &C,
                                    bNode &node,
                                    bNodeSocket &socket,
                                    const float2 &cursor);

/* `add_node_search.cc` */

void invoke_add_node_search_menu(bContext &C, const float2 &cursor, bool use_transform);

/* `add_menu_assets.cc` */

MenuType add_catalog_assets_menu_type();
MenuType add_root_catalogs_menu_type();

}  // namespace blender::ed::space_node
