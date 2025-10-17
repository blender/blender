/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 */

#pragma once

#include "BLI_compute_context.hh"
#include "BLI_enum_flags.hh"
#include "BLI_vector.hh"

#include "BKE_node.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
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
  /** Which side of the links is fixed. */
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

  std::optional<int> frame_identifier_to_highlight;

  /**
   * Indicates that the compositing int the space tree needs to be re-evaluated using
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

  /**
   * Caches the sockets of which nodes can be synced. This can occasionally be expensive to compute
   * because it needs to traverse the tree. Also, we don't want to check whether syncing is
   * necessary for all nodes eagerly but only if a relevant node is visible to the user. The cache
   * is reset when something changes that may affect what nodes need to be synced.
   */
  Map<int, bool> node_can_sync_states;
};

enum NodeResizeDirection {
  NODE_RESIZE_NONE = 0,
  NODE_RESIZE_TOP = (1 << 0),
  NODE_RESIZE_BOTTOM = (1 << 1),
  NODE_RESIZE_RIGHT = (1 << 2),
  NODE_RESIZE_LEFT = (1 << 3),
};
ENUM_OPERATORS(NodeResizeDirection);

/* Nodes draw without DPI - the view zoom is flexible. */
#define BASIS_RAD (0.2f * U.widget_unit)
#define NODE_DYS (U.widget_unit / 2)
#define NODE_DY U.widget_unit
#define NODE_ITEM_SPACING_Y (0.1f * U.widget_unit)
#define NODE_WIDTH(node) (node.width * UI_SCALE_FAC)
#define NODE_HEIGHT(node) (node.height * UI_SCALE_FAC)
#define NODE_MARGIN_X (1.2f * U.widget_unit)
#define NODE_SOCKSIZE (0.25f * U.widget_unit)
#define NODE_MULTI_INPUT_LINK_GAP (0.25f * U.widget_unit)
#define NODE_RESIZE_MARGIN (0.20f * U.widget_unit)
#define NODE_LINK_RESOL 12

/* `space_node.cc` */

/**
 * Transform between View2Ds in the tree path.
 */
float2 space_node_group_offset(const SpaceNode &snode);

int node_get_resize_cursor(NodeResizeDirection directions);

/* `node_draw.cc` */

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

void node_socket_add_tooltip(const bNodeTree &ntree, const bNodeSocket &sock, uiLayout &layout);

/**
 * Update node draw order nodes based on selection: unselected nodes first, then selected,
 * then the active node at the very end. Relative order is kept intact.
 */
void tree_draw_order_update(bNodeTree &ntree);
/** Return the nodes in draw order, with the top nodes at the end. */
Array<bNode *> tree_draw_order_calc_nodes(bNodeTree &ntree);
/** Return the nodes in reverse draw order, with the top nodes at the start. */
Array<bNode *> tree_draw_order_calc_nodes_reversed(bNodeTree &ntree);

void node_set_cursor(wmWindow &win, ARegion &region, SpaceNode &snode, const float2 &cursor);
/* DPI scaled coords */
float2 node_to_view(const float2 &co);
void node_to_updated_rect(const bNode &node, rctf &r_rect);
float2 node_from_view(const float2 &co);

/* `node_ops.cc` */

void node_operatortypes();
void node_keymap(wmKeyConfig *keyconf);

/* `node_select.cc` */

rctf node_frame_rect_inside(const SpaceNode &snode, const bNode &node);
bool node_or_socket_isect_event(const bContext &C, const wmEvent &event);
bNode *node_under_mouse_get(const SpaceNode &snode, const float2 mouse);

bool node_deselect_all(bNodeTree &node_tree);
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

float2 socket_link_connection_location(const bNode &node,
                                       const bNodeSocket &socket,
                                       const bNodeLink &link);

NodeResizeDirection node_get_resize_direction(const SpaceNode &snode,
                                              const bNode *node,
                                              int x,
                                              int y);

/* node socket batched drawing */
void UI_node_socket_draw_cache_flush();
void nodesocket_batch_start();
void nodesocket_batch_end();

void nodelink_batch_start(const SpaceNode &snode);
void nodelink_batch_end(const SpaceNode &snode);

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
void NODE_OT_add_group(wmOperatorType *ot);
void NODE_OT_add_group_asset(wmOperatorType *ot);
void NODE_OT_add_object(wmOperatorType *ot);
void NODE_OT_add_collection(wmOperatorType *ot);
void NODE_OT_add_image(wmOperatorType *ot);
void NODE_OT_add_mask(wmOperatorType *ot);
void NODE_OT_add_material(wmOperatorType *ot);
void NODE_OT_add_color(wmOperatorType *ot);
void NODE_OT_add_import_node(wmOperatorType *ot);
void NODE_OT_swap_group_asset(wmOperatorType *ot);
void NODE_OT_new_node_tree(wmOperatorType *ot);
void NODE_OT_new_compositing_node_group(wmOperatorType *ot);
void NODE_OT_duplicate_compositing_node_group(wmOperatorType *ot);
void NODE_OT_new_compositor_sequencer_node_group(wmOperatorType *operator_type);
void NODE_OT_add_group_input_node(wmOperatorType *ot);

/* `node_group.cc` */

StringRef node_group_idname(const bContext *C);
void NODE_OT_group_make(wmOperatorType *ot);
void NODE_OT_group_insert(wmOperatorType *ot);
void NODE_OT_group_ungroup(wmOperatorType *ot);
void NODE_OT_group_separate(wmOperatorType *ot);
void NODE_OT_group_edit(wmOperatorType *ot);
void NODE_OT_group_enter_exit(wmOperatorType *ot);

void NODE_OT_default_group_width_set(wmOperatorType *ot);

/* `node_relationships.cc` */

void update_multi_input_indices_for_removed_links(bNode &node);
bool all_links_muted(const bNodeSocket &socket);
/** Get the "main" socket based on the node declaration or an heuristic. */
bNodeSocket *get_main_socket(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out);

void NODE_OT_link(wmOperatorType *ot);
void NODE_OT_link_make(wmOperatorType *ot);
void NODE_OT_links_cut(wmOperatorType *ot);
void NODE_OT_links_detach(wmOperatorType *ot);
void NODE_OT_links_mute(wmOperatorType *ot);

void NODE_OT_parent_set(wmOperatorType *ot);
void NODE_OT_join(wmOperatorType *ot);
void NODE_OT_attach(wmOperatorType *ot);
void NODE_OT_detach(wmOperatorType *ot);
void NODE_OT_join_nodes(wmOperatorType *ot);

void NODE_OT_link_viewer(wmOperatorType *ot);

void NODE_OT_insert_offset(wmOperatorType *ot);

wmKeyMap *node_link_modal_keymap(wmKeyConfig *keyconf);
wmKeyMap *node_resize_modal_keymap(wmKeyConfig *keyconf);

/* `node_edit.cc` */

float2 node_link_calculate_multi_input_position(const float2 &socket_position,
                                                int index,
                                                int total_inputs);

float node_socket_calculate_height(const bNodeSocket &socket);

bool composite_node_active(bContext *C);
/** Operator poll callback. */
bool composite_node_editable(bContext *C);

bool node_has_hidden_sockets(bNode *node);
void node_set_hidden_sockets(bNode *node, int set);
bool node_is_previewable(const SpaceNode &snode, const bNodeTree &ntree, const bNode &node);
wmOperatorStatus node_render_changed_exec(bContext *, wmOperator *);
bNodeSocket *node_find_indicated_socket(SpaceNode &snode,
                                        ARegion &region,
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
void NODE_OT_collapse_toggle(wmOperatorType *ot);
void NODE_OT_hide_socket_toggle(wmOperatorType *ot);
void NODE_OT_preview_toggle(wmOperatorType *ot);
void NODE_OT_options_toggle(wmOperatorType *ot);
void NODE_OT_node_copy_color(wmOperatorType *ot);
void NODE_OT_deactivate_viewer(wmOperatorType *ot);
void NODE_OT_activate_viewer(wmOperatorType *ot);
void NODE_OT_toggle_viewer(wmOperatorType *ot);
void NODE_OT_test_inlining_shader_nodes(wmOperatorType *ot);

void NODE_OT_read_viewlayers(wmOperatorType *ot);
void NODE_OT_render_changed(wmOperatorType *ot);

/**
 * \note clipboard_cut is a simple macro of copy + delete.
 */
void NODE_OT_clipboard_copy(wmOperatorType *ot);
void NODE_OT_clipboard_paste(wmOperatorType *ot);

void NODE_OT_shader_script_update(wmOperatorType *ot);

void NODE_OT_viewer_border(wmOperatorType *ot);
void NODE_OT_clear_viewer_border(wmOperatorType *ot);

void NODE_OT_cryptomatte_layer_add(wmOperatorType *ot);
void NODE_OT_cryptomatte_layer_remove(wmOperatorType *ot);

/* `node_gizmo.cc` */

void NODE_GGT_backdrop_transform(wmGizmoGroupType *gzgt);
void NODE_GGT_backdrop_crop(wmGizmoGroupType *gzgt);
void NODE_GGT_backdrop_glare(wmGizmoGroupType *gzgt);
void NODE_GGT_backdrop_corner_pin(wmGizmoGroupType *gzgt);
void NODE_GGT_backdrop_box_mask(wmGizmoGroupType *gzgt);
void NODE_GGT_backdrop_ellipse_mask(wmGizmoGroupType *gzgt);
void NODE_GGT_backdrop_split(wmGizmoGroupType *gzgt);

/* `node_geometry_attribute_search.cc` */

void node_geometry_add_attribute_search_button(const bContext &C,
                                               const bNode &node,
                                               PointerRNA &socket_ptr,
                                               uiLayout &layout,
                                               StringRef placeholder = "");

/* `node_geometry_layer_search.cc` */

void node_geometry_add_layer_search_button(const bContext &C,
                                           const bNode &node,
                                           PointerRNA &socket_ptr,
                                           uiLayout &layout,
                                           StringRef placeholder = "");
/* `node_geometry_volume_grid_search.cc` */

void node_geometry_add_volume_grid_search_button(const bContext &C,
                                                 const bNode &node,
                                                 PointerRNA &socket_ptr,
                                                 uiLayout &layout,
                                                 StringRef placeholder = "");

/* `node_context_path.cc` */

Vector<ui::ContextPathItem> context_path_for_space_node(const bContext &C);

/* `link_drag_search.cc` */

void invoke_node_link_drag_add_menu(bContext &C,
                                    bNode &node,
                                    bNodeSocket &socket,
                                    const float2 &cursor);

/* `add_menu_assets.cc` */

MenuType catalog_assets_menu_type();
MenuType unassigned_assets_menu_type();
MenuType add_root_catalogs_menu_type();

MenuType swap_root_catalogs_menu_type();

/* `node_sync_sockets.cc` */

void NODE_OT_sockets_sync(wmOperatorType *ot);

/* node_socket_tooltip.cc */

void build_socket_tooltip(uiTooltipData &tip_data,
                          bContext &C,
                          uiBut *but,
                          const bNodeTree &tree,
                          const bNodeSocket &socket);

/** node_tree_interface_ui.cc */

void node_tree_interface_panel_register(ARegionType *art);

}  // namespace blender::ed::space_node
