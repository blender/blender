/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 * \brief higher level node drawing for the node editor.
 */

#include <iomanip>

#include "MEM_guardedalloc.h"

#include "DNA_light_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BLI_array.hh"
#include "BLI_bounds.hh"
#include "BLI_convexhull_2d.h"
#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_quaternion.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "BKE_compute_contexts.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node.hh"
#include "BKE_node_enum.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_node_tree_zones.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"
#include "BKE_scene_runtime.hh"
#include "BKE_screen.hh"
#include "BKE_type_conversions.hh"

#include "IMB_imbuf.hh"

#include "DEG_depsgraph.hh"

#include "BLF_api.hh"

#include "BIF_glutil.hh"

#include "GPU_framebuffer.hh"
#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"
#include "GPU_viewport.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_gpencil_legacy.hh"
#include "ED_node.hh"
#include "ED_node_preview.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_viewer_path.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "NOD_geometry_nodes_gizmos.hh"
#include "NOD_geometry_nodes_log.hh"
#include "NOD_node_declaration.hh"
#include "NOD_node_extra_info.hh"
#include "NOD_socket_declarations_geometry.hh"

#include "GEO_fillet_curves.hh"

#include "node_intern.hh" /* own include */

#include <fmt/format.h>
#include <sstream>

namespace geo_log = blender::nodes::geo_eval_log;
using blender::bke::bNodeTreeZone;
using blender::bke::bNodeTreeZones;
using blender::ed::space_node::NestedTreePreviews;
using blender::nodes::NodeExtraInfoRow;

namespace blender::ed::space_node {

/**
 * This is passed to many functions which draw the node editor.
 */
struct TreeDrawContext {
  Main *bmain;
  wmWindow *window;
  Scene *scene;
  ARegion *region;
  Depsgraph *depsgraph;

  /**
   * Whether a viewer node is active in geometry nodes can not be determined by a flag on the node
   * alone. That's because if the node group with the viewer is used multiple times, it's only
   * active in one of these cases.
   * The active node is cached here to avoid doing the more expensive check for every viewer node
   * in the tree.
   */
  const bNode *active_geometry_nodes_viewer = nullptr;
  /**
   * Geometry nodes logs various data during execution. The logged data that corresponds to the
   * currently drawn node tree can be retrieved from the log below.
   */
  Map<const bNodeTreeZone *, geo_log::GeoTreeLog *> geo_log_by_zone;

  NestedTreePreviews *nested_group_infos = nullptr;
  /**
   * True if there is an active compositor using the node tree, false otherwise.
   */
  bool used_by_compositor = false;

  Map<bNodeInstanceKey, timeit::Nanoseconds> *compositor_per_node_execution_time = nullptr;

  /**
   * Label for reroute nodes that is derived from upstream reroute nodes.
   */
  Map<const bNode *, StringRef> reroute_auto_labels;
};

float grid_size_get()
{
  return NODE_GRID_STEP_SIZE;
}

void tree_update(const bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (snode) {
    snode_set_context(*C);

    if (snode->nodetree) {
      id_us_ensure_real(&snode->nodetree->id);
    }
  }
}

/* id is supposed to contain a node tree */
static bNodeTree *node_tree_from_ID(ID *id)
{
  if (id) {
    if (GS(id->name) == ID_NT) {
      return (bNodeTree *)id;
    }
    return bke::node_tree_from_id(id);
  }

  return nullptr;
}

void tag_update_id(ID *id)
{
  bNodeTree *ntree = node_tree_from_ID(id);
  if (id == nullptr || ntree == nullptr) {
    return;
  }

  /* TODO(sergey): With the new dependency graph it should be just enough to only tag ntree itself.
   * All the users of this tree will have update flushed from the tree. */
  DEG_id_tag_update(&ntree->id, 0);

  if (ntree->type == NTREE_SHADER) {
    DEG_id_tag_update(id, 0);

    if (GS(id->name) == ID_MA) {
      WM_main_add_notifier(NC_MATERIAL | ND_SHADING, id);
    }
    else if (GS(id->name) == ID_LA) {
      WM_main_add_notifier(NC_LAMP | ND_LIGHTING, id);
    }
    else if (GS(id->name) == ID_WO) {
      WM_main_add_notifier(NC_WORLD | ND_WORLD, id);
    }
  }
  else if (ntree->type == NTREE_COMPOSIT) {
    WM_main_add_notifier(NC_SCENE | ND_NODES, id);
  }
  else if (ntree->type == NTREE_TEXTURE) {
    DEG_id_tag_update(id, 0);
    WM_main_add_notifier(NC_TEXTURE | ND_NODES, id);
  }
  else if (ntree->type == NTREE_GEOMETRY) {
    WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, id);
  }
  else if (id == &ntree->id) {
    /* Node groups. */
    DEG_id_tag_update(id, 0);
  }
}

static std::string node_socket_get_tooltip(const SpaceNode *snode,
                                           const bNodeTree &ntree,
                                           const bNodeSocket &socket);

static const char *node_socket_get_translation_context(const bNodeSocket &socket)
{
  /* The node is not explicitly defined. */
  if (socket.runtime->declaration == nullptr) {
    return nullptr;
  }

  const std::optional<std::string> &translation_context =
      socket.runtime->declaration->translation_context;

  /* Default context. */
  if (!translation_context.has_value()) {
    return nullptr;
  }

  return translation_context->c_str();
}

static void node_socket_add_tooltip_in_node_editor(const bNodeSocket &sock, uiLayout &layout);

/** Return true when \a a should be behind \a b and false otherwise. */
static bool compare_node_depth(const bNode *a, const bNode *b)
{
  /* These tell if either the node or any of the parent nodes is selected.
   * A selected parent means an unselected node is also in foreground! */
  bool a_select = (a->flag & NODE_SELECT) != 0, b_select = (b->flag & NODE_SELECT) != 0;
  bool a_active = (a->flag & NODE_ACTIVE) != 0, b_active = (b->flag & NODE_ACTIVE) != 0;

  /* If one is an ancestor of the other. */
  /* XXX there might be a better sorting algorithm for stable topological sort,
   * this is O(n^2) worst case. */
  for (bNode *parent = a->parent; parent; parent = parent->parent) {
    /* If B is an ancestor, it is always behind A. */
    if (parent == b) {
      return false;
    }
    /* Any selected ancestor moves the node forward. */
    if (parent->flag & NODE_ACTIVE) {
      a_active = true;
    }
    if (parent->flag & NODE_SELECT) {
      a_select = true;
    }
  }
  for (bNode *parent = b->parent; parent; parent = parent->parent) {
    /* If A is an ancestor, it is always behind B. */
    if (parent == a) {
      return true;
    }
    /* Any selected ancestor moves the node forward. */
    if (parent->flag & NODE_ACTIVE) {
      b_active = true;
    }
    if (parent->flag & NODE_SELECT) {
      b_select = true;
    }
  }

  /* One of the nodes is in the background and the other not. */
  if ((a->flag & NODE_BACKGROUND) && !(b->flag & NODE_BACKGROUND)) {
    return true;
  }
  if ((b->flag & NODE_BACKGROUND) && !(a->flag & NODE_BACKGROUND)) {
    return false;
  }

  /* One has a higher selection state (active > selected > nothing). */
  if (a_active && !b_active) {
    return false;
  }
  if (b_active && !a_active) {
    return true;
  }
  if (!b_select && (a_active || a_select)) {
    return false;
  }
  if (!a_select && (b_active || b_select)) {
    return true;
  }

  return false;
}

void tree_draw_order_update(bNodeTree &ntree)
{
  Array<bNode *> sort_nodes = ntree.all_nodes();
  std::sort(sort_nodes.begin(), sort_nodes.end(), [](bNode *a, bNode *b) {
    return a->ui_order < b->ui_order;
  });
  std::stable_sort(sort_nodes.begin(), sort_nodes.end(), compare_node_depth);
  for (const int i : sort_nodes.index_range()) {
    sort_nodes[i]->ui_order = i;
  }
}

Array<bNode *> tree_draw_order_calc_nodes(bNodeTree &ntree)
{
  Array<bNode *> nodes = ntree.all_nodes();
  if (nodes.is_empty()) {
    return {};
  }
  std::sort(nodes.begin(), nodes.end(), [](const bNode *a, const bNode *b) {
    return a->ui_order < b->ui_order;
  });
  return nodes;
}

Array<bNode *> tree_draw_order_calc_nodes_reversed(bNodeTree &ntree)
{
  Array<bNode *> nodes = ntree.all_nodes();
  if (nodes.is_empty()) {
    return {};
  }
  std::sort(nodes.begin(), nodes.end(), [](const bNode *a, const bNode *b) {
    return a->ui_order > b->ui_order;
  });
  return nodes;
}

static Array<uiBlock *> node_uiblocks_init(const bContext &C, const Span<bNode *> nodes)
{
  Array<uiBlock *> blocks(nodes.size());

  /* Add node uiBlocks in drawing order - prevents events going to overlapping nodes. */
  Scene *scene = CTX_data_scene(&C);
  wmWindow *window = CTX_wm_window(&C);
  ARegion *region = CTX_wm_region(&C);
  for (const int i : nodes.index_range()) {
    const bNode &node = *nodes[i];
    std::string block_name = "node_" + std::string(node.name);
    uiBlock *block = UI_block_begin(&C, scene, window, region, std::move(block_name), UI_EMBOSS);
    blocks[node.index()] = block;
    /* This cancels events for background nodes. */
    UI_block_flag_enable(block, UI_BLOCK_CLIP_EVENTS);
  }

  return blocks;
}

float2 node_to_view(const float2 &co)
{
  return co * UI_SCALE_FAC;
}

static rctf node_to_rect(const bNode &node)
{
  rctf rect{};
  rect.xmin = node.location[0];
  rect.ymin = node.location[1] - node.height;
  rect.xmax = node.location[0] + node.width;
  rect.ymax = node.location[1];
  return rect;
}

void node_to_updated_rect(const bNode &node, rctf &r_rect)
{
  r_rect = node_to_rect(node);
  BLI_rctf_mul(&r_rect, UI_SCALE_FAC);
}

float2 node_from_view(const float2 &co)
{
  return co / UI_SCALE_FAC;
}

static bool is_node_panels_supported(const bNode &node)
{
  return node.declaration() && node.declaration()->use_custom_socket_order;
}

/* Draw UI for options, buttons, and previews. */
static bool node_update_basis_buttons(const bContext &C,
                                      bNodeTree &ntree,
                                      bNode &node,
                                      blender::FunctionRef<nodes::DrawNodeLayoutFn> draw_buttons,
                                      uiBlock &block,
                                      int &dy)
{
  /* Buttons rect? */
  const bool node_options = draw_buttons && (node.flag & NODE_OPTIONS);
  if (!node_options) {
    return false;
  }

  PointerRNA nodeptr = RNA_pointer_create_discrete(&ntree.id, &RNA_Node, &node);

  /* Round the node origin because text contents are always pixel-aligned. */
  const float2 loc = math::round(node_to_view(node.location));

  dy -= NODE_DYS / 4;

  uiLayout *layout = UI_block_layout(&block,
                                     UI_LAYOUT_VERTICAL,
                                     UI_LAYOUT_PANEL,
                                     loc.x + NODE_DYS,
                                     dy,
                                     NODE_WIDTH(node) - NODE_DY,
                                     0,
                                     0,
                                     UI_style_get_dpi());

  if (node.is_muted()) {
    uiLayoutSetActive(layout, false);
  }

  uiLayoutSetContextPointer(layout, "node", &nodeptr);

  draw_buttons(layout, (bContext *)&C, &nodeptr);

  UI_block_align_end(&block);
  int buty;
  UI_block_layout_resolve(&block, nullptr, &buty);

  dy = buty - NODE_DYS / 4;
  return true;
}

const char *node_socket_get_label(const bNodeSocket *socket, const char *panel_label)
{
  /* Get the short label if possible. This is used when grouping sockets under panels,
   * to avoid redundancy in the label. */
  const std::optional<StringRefNull> socket_short_label = bke::nodeSocketShortLabel(socket);
  const char *socket_translation_context = node_socket_get_translation_context(*socket);

  if (socket_short_label.has_value()) {
    return CTX_IFACE_(socket_translation_context, socket_short_label->c_str());
  }

  const StringRefNull socket_label = bke::nodeSocketLabel(socket);
  const char *translated_socket_label = CTX_IFACE_(socket_translation_context,
                                                   socket_label.c_str());

  /* Shorten socket label if it begins with the panel label. */
  if (panel_label) {
    const int len_prefix = strlen(panel_label);
    if (STREQLEN(translated_socket_label, panel_label, len_prefix) &&
        translated_socket_label[len_prefix] == ' ')
    {
      return translated_socket_label + len_prefix + 1;
    }
  }

  /* Full label. */
  return translated_socket_label;
}

static bool node_update_basis_socket(const bContext &C,
                                     bNodeTree &ntree,
                                     bNode &node,
                                     const char *panel_label,
                                     bNodeSocket *input_socket,
                                     bNodeSocket *output_socket,
                                     uiBlock &block,
                                     const int &locx,
                                     int &locy)
{
  if ((!input_socket || !input_socket->is_visible()) &&
      (!output_socket || !output_socket->is_visible()))
  {
    return false;
  }

  const int topy = locy;

  /* Add the half the height of a multi-input socket to cursor Y
   * to account for the increased height of the taller sockets. */
  const bool is_multi_input = (input_socket ? input_socket->flag & SOCK_MULTI_INPUT : false);
  const float multi_input_socket_offset = is_multi_input ?
                                              std::max(input_socket->runtime->total_inputs - 2,
                                                       0) *
                                                  NODE_MULTI_INPUT_LINK_GAP :
                                              0.0f;
  locy -= multi_input_socket_offset * 0.5f;

  uiLayout *layout = UI_block_layout(&block,
                                     UI_LAYOUT_VERTICAL,
                                     UI_LAYOUT_PANEL,
                                     locx + NODE_DYS,
                                     locy,
                                     NODE_WIDTH(node) - NODE_DY,
                                     NODE_DY,
                                     0,
                                     UI_style_get_dpi());

  if (node.is_muted()) {
    uiLayoutSetActive(layout, false);
  }

  uiLayout *row = uiLayoutRow(layout, true);
  PointerRNA nodeptr = RNA_pointer_create_discrete(&ntree.id, &RNA_Node, &node);
  uiLayoutSetContextPointer(row, "node", &nodeptr);

  if (input_socket) {
    /* Context pointers for current node and socket. */
    PointerRNA sockptr = RNA_pointer_create_discrete(&ntree.id, &RNA_NodeSocket, input_socket);
    uiLayoutSetContextPointer(row, "socket", &sockptr);

    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_EXPAND);

    input_socket->typeinfo->draw(
        (bContext *)&C, row, &sockptr, &nodeptr, node_socket_get_label(input_socket, panel_label));
  }
  else {
    /* Context pointers for current node and socket. */
    PointerRNA sockptr = RNA_pointer_create_discrete(&ntree.id, &RNA_NodeSocket, output_socket);
    uiLayoutSetContextPointer(row, "socket", &sockptr);

    /* Align output buttons to the right. */
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);

    output_socket->typeinfo->draw((bContext *)&C,
                                  row,
                                  &sockptr,
                                  &nodeptr,
                                  node_socket_get_label(output_socket, panel_label));
  }

  if (input_socket) {
    node_socket_add_tooltip_in_node_editor(*input_socket, *row);
    /* Round the socket location to stop it from jiggling. */
    input_socket->runtime->location = float2(round(locx), round(locy - NODE_DYS));
  }
  if (output_socket) {
    node_socket_add_tooltip_in_node_editor(*output_socket, *row);
    /* Round the socket location to stop it from jiggling. */
    output_socket->runtime->location = float2(round(locx + NODE_WIDTH(node)),
                                              round(locy - NODE_DYS));
  }

  UI_block_align_end(&block);

  int buty;
  UI_block_layout_resolve(&block, nullptr, &buty);
  /* Ensure minimum socket height in case layout is empty. */
  buty = min_ii(buty, topy - NODE_DY);
  locy = buty - multi_input_socket_offset * 0.5;
  return true;
}

namespace flat_item {

enum class Type {
  Socket,
  Separator,
  Layout,
  PanelHeader,
  PanelContentBegin,
  PanelContentEnd,
};

struct Socket {
  static constexpr Type type = Type::Socket;
  bNodeSocket *input = nullptr;
  bNodeSocket *output = nullptr;
  const nodes::PanelDeclaration *panel_decl = nullptr;
};
struct Separator {
  static constexpr Type type = Type::Separator;
};
struct PanelHeader {
  static constexpr Type type = Type::PanelHeader;
  const nodes::PanelDeclaration *decl;
};
struct PanelContentBegin {
  static constexpr Type type = Type::PanelContentBegin;
  const nodes::PanelDeclaration *decl;
};
struct PanelContentEnd {
  static constexpr Type type = Type::PanelContentEnd;
  const nodes::PanelDeclaration *decl;
};
struct Layout {
  static constexpr Type type = Type::Layout;
  const nodes::LayoutDeclaration *decl;
};

}  // namespace flat_item

struct FlatNodeItem {
  std::variant<flat_item::Socket,
               flat_item::Separator,
               flat_item::PanelHeader,
               flat_item::PanelContentBegin,
               flat_item::PanelContentEnd,
               flat_item::Layout>
      item;

  flat_item::Type type() const
  {
    return std::visit([](auto &&item) { return item.type; }, this->item);
  }
};

static void determine_potentially_visible_panels_recursive(
    const bNode &node, const nodes::PanelDeclaration &panel_decl, MutableSpan<bool> r_result)
{
  bool potentially_visible = false;
  for (const nodes::ItemDeclaration *item_decl : panel_decl.items) {
    if (const auto *socket_decl = dynamic_cast<const nodes::SocketDeclaration *>(item_decl)) {
      const bNodeSocket &socket = node.socket_by_decl(*socket_decl);
      potentially_visible |= socket.is_visible();
    }
    else if (const auto *sub_panel_decl = dynamic_cast<const nodes::PanelDeclaration *>(item_decl))
    {
      determine_potentially_visible_panels_recursive(node, *sub_panel_decl, r_result);
      potentially_visible |= r_result[sub_panel_decl->index];
    }
  }
  r_result[panel_decl.index] = potentially_visible;
}

/**
 * A panel is potentially visible if it contains any socket that is available and not hidden.
 */
static void determine_potentially_visible_panels(const bNode &node, MutableSpan<bool> r_result)
{
  for (const nodes::ItemDeclaration *item_decl : node.declaration()->root_items) {
    if (const auto *panel_decl = dynamic_cast<const nodes::PanelDeclaration *>(item_decl)) {
      determine_potentially_visible_panels_recursive(node, *panel_decl, r_result);
    }
  }
}

static void determine_visible_panels_impl_recursive(const bNode &node,
                                                    const nodes::PanelDeclaration &panel_decl,
                                                    const Span<bool> potentially_visible_states,
                                                    MutableSpan<bool> r_result)
{
  if (!potentially_visible_states[panel_decl.index]) {
    /* This panel does not contain any visible sockets. */
    return;
  }
  r_result[panel_decl.index] = true;
  const bNodePanelState &panel_state = node.panel_states_array[panel_decl.index];
  if (panel_state.is_collapsed()) {
    /* The sub-panels can't be visible if this panel is collapsed. */
    return;
  }
  for (const nodes::ItemDeclaration *item_decl : panel_decl.items) {
    if (const auto *sub_panel_decl = dynamic_cast<const nodes::PanelDeclaration *>(item_decl)) {
      determine_visible_panels_impl_recursive(
          node, *sub_panel_decl, potentially_visible_states, r_result);
    }
  }
}

static void determine_visible_panels_impl(const bNode &node,
                                          const Span<bool> potentially_visible_states,
                                          MutableSpan<bool> r_result)
{
  for (const nodes::ItemDeclaration *item_decl : node.declaration()->root_items) {
    if (const auto *panel_decl = dynamic_cast<const nodes::PanelDeclaration *>(item_decl)) {
      determine_visible_panels_impl_recursive(
          node, *panel_decl, potentially_visible_states, r_result);
    }
  }
}

/**
 * A panel is visible if all of the following are true:
 * - All parent panels are visible and not collapsed.
 * - The panel contains any visible sockets.
 */
static void determine_visible_panels(const bNode &node, MutableSpan<bool> r_visibility_states)
{
  Array<bool> potentially_visible_states(r_visibility_states.size(), false);
  determine_potentially_visible_panels(node, potentially_visible_states);
  determine_visible_panels_impl(node, potentially_visible_states, r_visibility_states);
}

static void add_flat_items_for_socket(bNode &node,
                                      const nodes::SocketDeclaration &socket_decl,
                                      const nodes::PanelDeclaration *panel_decl,
                                      const nodes::SocketDeclaration *prev_socket_decl,
                                      Vector<FlatNodeItem> &r_items)
{
  bNodeSocket &socket = node.socket_by_decl(socket_decl);
  if (!socket.is_visible()) {
    return;
  }
  if (socket_decl.align_with_previous_socket) {
    if (!prev_socket_decl || !node.socket_by_decl(*prev_socket_decl).is_visible()) {
      r_items.append({flat_item::Socket()});
    }
  }
  else {
    r_items.append({flat_item::Socket()});
  }
  flat_item::Socket &item = std::get<flat_item::Socket>(r_items.last().item);
  if (socket_decl.in_out == SOCK_IN) {
    BLI_assert(!item.input);
    item.input = &socket;
  }
  else {
    BLI_assert(!item.output);
    item.output = &socket;
  }
  item.panel_decl = panel_decl;
}

static void add_flat_items_for_separator(Vector<FlatNodeItem> &r_items)
{
  r_items.append({flat_item::Separator()});
}

static void add_flat_items_for_layout(const bNode &node,
                                      const nodes::LayoutDeclaration &layout_decl,
                                      Vector<FlatNodeItem> &r_items)
{
  if (!(node.flag & NODE_OPTIONS)) {
    return;
  }
  r_items.append({flat_item::Layout{&layout_decl}});
}

static void add_flat_items_for_panel(bNode &node,
                                     const nodes::PanelDeclaration &panel_decl,
                                     const Span<bool> panel_visibility,
                                     Vector<FlatNodeItem> &r_items)
{
  if (!panel_visibility[panel_decl.index]) {
    return;
  }
  r_items.append({flat_item::PanelHeader{&panel_decl}});
  const bNodePanelState &panel_state = node.panel_states_array[panel_decl.index];
  if (panel_state.is_collapsed()) {
    return;
  }
  r_items.append({flat_item::PanelContentBegin{&panel_decl}});
  const nodes::SocketDeclaration *prev_socket_decl = nullptr;
  for (const nodes::ItemDeclaration *item_decl : panel_decl.items) {
    if (const auto *socket_decl = dynamic_cast<const nodes::SocketDeclaration *>(item_decl)) {
      add_flat_items_for_socket(node, *socket_decl, &panel_decl, prev_socket_decl, r_items);
      prev_socket_decl = socket_decl;
    }
    else {
      if (const auto *sub_panel_decl = dynamic_cast<const nodes::PanelDeclaration *>(item_decl)) {
        add_flat_items_for_panel(node, *sub_panel_decl, panel_visibility, r_items);
      }
      else if (dynamic_cast<const nodes::SeparatorDeclaration *>(item_decl)) {
        add_flat_items_for_separator(r_items);
      }
      else if (const auto *layout_decl = dynamic_cast<const nodes::LayoutDeclaration *>(item_decl))
      {
        add_flat_items_for_layout(node, *layout_decl, r_items);
      }
      prev_socket_decl = nullptr;
    }
  }
  r_items.append({flat_item::PanelContentEnd{&panel_decl}});
}

/**
 * Flattens the visible panels, sockets etc. of the node into a list that is then used to draw it.
 */
static Vector<FlatNodeItem> make_flat_node_items(bNode &node)
{
  BLI_assert(is_node_panels_supported(node));
  BLI_assert(node.runtime->panels.size() == node.num_panel_states);

  const int panels_num = node.num_panel_states;
  Array<bool> panel_visibility(panels_num, false);
  determine_visible_panels(node, panel_visibility);

  const nodes::SocketDeclaration *prev_socket_decl = nullptr;

  Vector<FlatNodeItem> items;
  for (const nodes::ItemDeclaration *item_decl : node.declaration()->root_items) {
    if (const auto *socket_decl = dynamic_cast<const nodes::SocketDeclaration *>(item_decl)) {
      add_flat_items_for_socket(node, *socket_decl, nullptr, prev_socket_decl, items);
      prev_socket_decl = socket_decl;
    }
    else {
      if (const auto *panel_decl = dynamic_cast<const nodes::PanelDeclaration *>(item_decl)) {
        add_flat_items_for_panel(node, *panel_decl, panel_visibility, items);
      }
      else if (dynamic_cast<const nodes::SeparatorDeclaration *>(item_decl)) {
        add_flat_items_for_separator(items);
      }
      else if (const auto *layout_decl = dynamic_cast<const nodes::LayoutDeclaration *>(item_decl))
      {
        add_flat_items_for_layout(node, *layout_decl, items);
      }
      prev_socket_decl = nullptr;
    }
  }
  return items;
}

/** Get the height of an empty node body. */
static float get_margin_empty()
{
  return NODE_DYS;
}

/** Get the margin between the node header and the first item. */
static float get_margin_from_top(const Span<FlatNodeItem> items)
{
  const FlatNodeItem &first_item = items[0];
  const flat_item::Type first_item_type = first_item.type();
  switch (first_item_type) {
    case flat_item::Type::Socket:
      return 2 * NODE_ITEM_SPACING_Y;
    case flat_item::Type::Separator:
      return NODE_ITEM_SPACING_Y / 2;
    case flat_item::Type::Layout:
      return 3 * NODE_ITEM_SPACING_Y;
    case flat_item::Type::PanelHeader:
      return 4 * NODE_ITEM_SPACING_Y;
    case flat_item::Type::PanelContentBegin:
    case flat_item::Type::PanelContentEnd:
      break;
  }
  BLI_assert_unreachable();
  return 0;
}

/** Get the margin between the last item and the node bottom. */
static float get_margin_to_bottom(const Span<FlatNodeItem> items)
{
  const FlatNodeItem &last_item = items.last();
  const flat_item::Type last_item_type = last_item.type();
  switch (last_item_type) {
    case flat_item::Type::Socket:
      return 2 * NODE_ITEM_SPACING_Y;
    case flat_item::Type::Separator:
      return NODE_ITEM_SPACING_Y;
    case flat_item::Type::Layout:
      return 5 * NODE_ITEM_SPACING_Y;
    case flat_item::Type::PanelHeader:
      return 4 * NODE_ITEM_SPACING_Y;
    case flat_item::Type::PanelContentBegin:
      break;
    case flat_item::Type::PanelContentEnd:
      return 1 * NODE_ITEM_SPACING_Y;
  }
  BLI_assert_unreachable();
  return 0;
}

/** Get the margin between two consecutive items. */
static float get_margin_between_elements(const Span<FlatNodeItem> items, const int next_index)
{
  BLI_assert(next_index >= 1);
  const FlatNodeItem &prev = items[next_index - 1];
  const FlatNodeItem &next = items[next_index];
  using flat_item::Type;
  const Type prev_type = prev.type();
  const Type next_type = next.type();

  /* Handle all cases explicitly. This simplifies modifying the margins for specific cases
   * without breaking other cases significantly. */
  switch (prev_type) {
    case Type::Socket: {
      switch (next_type) {
        case Type::Socket:
          return NODE_ITEM_SPACING_Y;
        case Type::Separator:
          return 0;
        case Type::Layout:
          return 2 * NODE_ITEM_SPACING_Y;
        case Type::PanelHeader:
          return 3 * NODE_ITEM_SPACING_Y;
        case Type::PanelContentBegin:
          break;
        case Type::PanelContentEnd:
          return 2 * NODE_ITEM_SPACING_Y;
      }
      break;
    }
    case Type::Layout: {
      switch (next_type) {
        case Type::Socket:
          return 2 * NODE_ITEM_SPACING_Y;
        case Type::Separator:
          return 0;
        case Type::Layout:
          return NODE_ITEM_SPACING_Y;
        case Type::PanelHeader:
          return 3 * NODE_ITEM_SPACING_Y;
        case Type::PanelContentBegin:
          break;
        case Type::PanelContentEnd:
          return 2 * NODE_ITEM_SPACING_Y;
      }
      break;
    }
    case Type::Separator: {
      switch (next_type) {
        case Type::Socket:
          return 2 * NODE_ITEM_SPACING_Y;
        case Type::Separator:
          return NODE_ITEM_SPACING_Y;
        case Type::Layout:
          return NODE_ITEM_SPACING_Y;
        case Type::PanelHeader:
          return NODE_ITEM_SPACING_Y;
        case Type::PanelContentBegin:
          break;
        case Type::PanelContentEnd:
          return NODE_ITEM_SPACING_Y;
      }
      break;
    }
    case Type::PanelHeader: {
      switch (next_type) {
        case Type::Socket:
          return 4 * NODE_ITEM_SPACING_Y;
        case Type::Separator:
          return 3 * NODE_ITEM_SPACING_Y;
        case Type::Layout:
          return 3 * NODE_ITEM_SPACING_Y;
        case Type::PanelHeader:
          return 5 * NODE_ITEM_SPACING_Y;
        case Type::PanelContentBegin:
          return 3 * NODE_ITEM_SPACING_Y;
        case Type::PanelContentEnd:
          return 3 * NODE_ITEM_SPACING_Y;
      }
      break;
    }
    case Type::PanelContentBegin: {
      switch (next_type) {
        case Type::Socket:
          return 2 * NODE_ITEM_SPACING_Y;
        case Type::Separator:
          return NODE_ITEM_SPACING_Y;
        case Type::Layout:
          return 2 * NODE_ITEM_SPACING_Y;
        case Type::PanelHeader:
          return 3 * NODE_ITEM_SPACING_Y;
        case Type::PanelContentBegin:
          break;
        case Type::PanelContentEnd:
          return NODE_ITEM_SPACING_Y;
      }
      break;
    }
    case Type::PanelContentEnd: {
      switch (next_type) {
        case Type::Socket:
          return NODE_ITEM_SPACING_Y;
        case Type::Separator:
          return NODE_ITEM_SPACING_Y;
        case Type::Layout:
          return NODE_ITEM_SPACING_Y;
        case Type::PanelHeader:
          return 3 * NODE_ITEM_SPACING_Y;
        case Type::PanelContentBegin:
          break;
        case Type::PanelContentEnd:
          return 0;
      }
      break;
    }
  }
  BLI_assert_unreachable();
  return 0.0f;
}

/** Tags all the sockets in the panel as collapsed and updates their positions. */
static void mark_sockets_collapsed_recursive(bNode &node,
                                             const int node_left_x,
                                             const nodes::PanelDeclaration &visible_panel_decl,
                                             const nodes::PanelDeclaration &panel_decl)
{
  const bke::bNodePanelRuntime &visible_panel_runtime =
      node.runtime->panels[visible_panel_decl.index];
  for (const nodes::ItemDeclaration *item_decl : panel_decl.items) {
    if (const auto *socket_decl = dynamic_cast<const nodes::SocketDeclaration *>(item_decl)) {
      bNodeSocket &socket = node.socket_by_decl(*socket_decl);
      const int socket_x = socket.in_out == SOCK_IN ? node_left_x : node_left_x + NODE_WIDTH(node);
      socket.runtime->location = math::round(
          float2(socket_x, *visible_panel_runtime.header_center_y));
      socket.flag |= SOCK_PANEL_COLLAPSED;
    }
    else if (const auto *sub_panel_decl = dynamic_cast<const nodes::PanelDeclaration *>(item_decl))
    {
      mark_sockets_collapsed_recursive(node, node_left_x, visible_panel_decl, *sub_panel_decl);
    }
  }
}

static void update_collapsed_sockets_recursive(bNode &node,
                                               const int node_left_x,
                                               const nodes::PanelDeclaration &panel_decl)
{
  const bNodePanelState &panel_state = node.panel_states_array[panel_decl.index];
  const bke::bNodePanelRuntime &panel_runtime = node.runtime->panels[panel_decl.index];
  const bool is_open = panel_runtime.header_center_y.has_value() && !panel_state.is_collapsed();
  if (!is_open) {
    mark_sockets_collapsed_recursive(node, node_left_x, panel_decl, panel_decl);
    return;
  }
  for (const nodes::ItemDeclaration *item_decl : panel_decl.items) {
    if (const auto *sub_panel_decl = dynamic_cast<const nodes::PanelDeclaration *>(item_decl)) {
      update_collapsed_sockets_recursive(node, node_left_x, *sub_panel_decl);
    }
  }
}

/**
 * Finds all collapsed sockets and updates them based on the visible parent panel that contains
 * them.
 */
static void update_collapsed_sockets(bNode &node, const int node_left_x)
{
  for (const nodes::ItemDeclaration *item_decl : node.declaration()->root_items) {
    if (const auto *panel_decl = dynamic_cast<const nodes::PanelDeclaration *>(item_decl)) {
      update_collapsed_sockets_recursive(node, node_left_x, *panel_decl);
    }
  }
}

/**
 * Tag the innermost panel that goes to the very end of the node. The background color of that
 * panel is extended to fill the entire rest of the node.
 */
static void tag_final_panel(bNode &node, const Span<FlatNodeItem> items)
{
  const flat_item::PanelContentEnd *final_panel = nullptr;
  for (int item_i = items.size() - 1; item_i >= 0; item_i--) {
    const FlatNodeItem &item = items[item_i];
    if (const auto *panel_item = std::get_if<flat_item::PanelContentEnd>(&item.item)) {
      final_panel = panel_item;
    }
    else {
      break;
    }
  }
  if (final_panel) {
    bke::bNodePanelRuntime &final_panel_runtime = node.runtime->panels[final_panel->decl->index];
    final_panel_runtime.content_extent->fill_node_end = true;
  }
}

/* Advanced drawing with panels and arbitrary input/output ordering. */
static void node_update_basis_from_declaration(
    const bContext &C, bNodeTree &ntree, bNode &node, uiBlock &block, const int locx, int &locy)
{
  BLI_assert(is_node_panels_supported(node));
  BLI_assert(node.runtime->panels.size() == node.num_panel_states);

  /* Reset states. */
  for (bke::bNodePanelRuntime &panel_runtime : node.runtime->panels) {
    panel_runtime.header_center_y.reset();
    panel_runtime.content_extent.reset();
  }
  for (bNodeSocket *socket : node.input_sockets()) {
    socket->flag &= ~SOCK_PANEL_COLLAPSED;
  }
  for (bNodeSocket *socket : node.output_sockets()) {
    socket->flag &= ~SOCK_PANEL_COLLAPSED;
  }

  /* Gather flattened list of items in the node. */
  const Vector<FlatNodeItem> flat_items = make_flat_node_items(node);
  if (flat_items.is_empty()) {
    const float margin = get_margin_empty();
    locy -= margin;
    return;
  }

  for (const int item_i : flat_items.index_range()) {
    /* Apply margins. This should be the only place that applies margins between elements so that
     * it is easy change later on. */
    if (item_i == 0) {
      const float margin = get_margin_from_top(flat_items);
      locy -= margin;
    }
    else {
      const float margin = get_margin_between_elements(flat_items, item_i);
      locy -= margin;
    }

    const FlatNodeItem &item_variant = flat_items[item_i];
    std::visit(
        [&](const auto &item) {
          using ItemT = std::decay_t<decltype(item)>;
          if constexpr (std::is_same_v<ItemT, flat_item::Socket>) {
            bNodeSocket *input_socket = item.input;
            bNodeSocket *output_socket = item.output;
            const nodes::PanelDeclaration *panel_decl = item.panel_decl;
            const char *parent_label = panel_decl ? panel_decl->name.c_str() : "";
            node_update_basis_socket(
                C, ntree, node, parent_label, input_socket, output_socket, block, locx, locy);
          }
          else if constexpr (std::is_same_v<ItemT, flat_item::Layout>) {
            const nodes::LayoutDeclaration &decl = *item.decl;
            /* Round the node origin because text contents are always pixel-aligned. */
            const float2 loc = math::round(node_to_view(node.location));
            uiLayout *layout = UI_block_layout(&block,
                                               UI_LAYOUT_VERTICAL,
                                               UI_LAYOUT_PANEL,
                                               loc.x + NODE_DYS,
                                               locy,
                                               NODE_WIDTH(node) - NODE_DY,
                                               0,
                                               0,
                                               UI_style_get_dpi());
            if (node.is_muted()) {
              uiLayoutSetActive(layout, false);
            }
            PointerRNA node_ptr = RNA_pointer_create_discrete(&ntree.id, &RNA_Node, &node);
            uiLayoutSetContextPointer(layout, "node", &node_ptr);
            decl.draw(layout, const_cast<bContext *>(&C), &node_ptr);
            UI_block_align_end(&block);
            int buty;
            UI_block_layout_resolve(&block, nullptr, &buty);
            locy = buty;
          }
          else if constexpr (std::is_same_v<ItemT, flat_item::Separator>) {
            uiLayout *layout = UI_block_layout(&block,
                                               UI_LAYOUT_VERTICAL,
                                               UI_LAYOUT_PANEL,
                                               locx + NODE_DYS,
                                               locy,
                                               NODE_WIDTH(node) - NODE_DY,
                                               NODE_DY,
                                               0,
                                               UI_style_get_dpi());
            uiItemS_ex(layout, 1.0, LayoutSeparatorType::Line);
            UI_block_layout_resolve(&block, nullptr, nullptr);
          }
          else if constexpr (std::is_same_v<ItemT, flat_item::PanelHeader>) {
            const nodes::PanelDeclaration &node_decl = *item.decl;
            bke::bNodePanelRuntime &panel_runtime = node.runtime->panels[node_decl.index];
            const float panel_header_height = NODE_DYS;
            locy -= panel_header_height / 2;
            panel_runtime.header_center_y = locy;
            locy -= panel_header_height / 2;
          }
          else if constexpr (std::is_same_v<ItemT, flat_item::PanelContentBegin>) {
            const nodes::PanelDeclaration &node_decl = *item.decl;
            bke::bNodePanelRuntime &panel_runtime = node.runtime->panels[node_decl.index];
            panel_runtime.content_extent.emplace();
            panel_runtime.content_extent->max_y = locy;
          }
          else if constexpr (std::is_same_v<ItemT, flat_item::PanelContentEnd>) {
            const nodes::PanelDeclaration &node_decl = *item.decl;
            bke::bNodePanelRuntime &panel_runtime = node.runtime->panels[node_decl.index];
            panel_runtime.content_extent->min_y = locy;
          }
        },
        item_variant.item);
  }

  const float bottom_margin = get_margin_to_bottom(flat_items);
  locy -= bottom_margin;

  update_collapsed_sockets(node, locx);
  tag_final_panel(node, flat_items);
}

/* Conventional drawing in outputs/buttons/inputs order. */
static void node_update_basis_from_socket_lists(
    const bContext &C, bNodeTree &ntree, bNode &node, uiBlock &block, const int locx, int &locy)
{
  /* Space at the top. */
  locy -= NODE_DYS / 2;

  /* Output sockets. */
  bool add_output_space = false;

  for (bNodeSocket *socket : node.output_sockets()) {
    /* Clear flag, conventional drawing does not support panels. */
    socket->flag &= ~SOCK_PANEL_COLLAPSED;

    if (node_update_basis_socket(C, ntree, node, nullptr, nullptr, socket, block, locx, locy)) {
      if (socket->next) {
        locy -= NODE_ITEM_SPACING_Y;
      }
      add_output_space = true;
    }
  }

  if (add_output_space) {
    locy -= NODE_DY / 4;
  }

  const bool add_button_space = node_update_basis_buttons(
      C, ntree, node, node.typeinfo->draw_buttons, block, locy);

  bool add_input_space = false;

  /* Input sockets. */
  for (bNodeSocket *socket : node.input_sockets()) {
    /* Clear flag, conventional drawing does not support panels. */
    socket->flag &= ~SOCK_PANEL_COLLAPSED;

    if (node_update_basis_socket(C, ntree, node, nullptr, socket, nullptr, block, locx, locy)) {
      if (socket->next) {
        locy -= NODE_ITEM_SPACING_Y;
      }
      add_input_space = true;
    }
  }

  /* Little bit of padding at the bottom. */
  if (add_input_space || add_button_space) {
    locy -= NODE_DYS / 2;
  }
}

/**
 * Based on settings and sockets in node, set drawing rect info.
 */
static void node_update_basis(const bContext &C,
                              const TreeDrawContext & /*tree_draw_ctx*/,
                              bNodeTree &ntree,
                              bNode &node,
                              uiBlock &block)
{
  /* Round the node origin because text contents are always pixel-aligned. */
  const float2 loc = math::round(node_to_view(node.location));

  int dy = loc.y;

  /* Header. */
  dy -= NODE_DY;

  if (is_node_panels_supported(node)) {
    node_update_basis_from_declaration(C, ntree, node, block, loc.x, dy);
  }
  else {
    node_update_basis_from_socket_lists(C, ntree, node, block, loc.x, dy);
  }

  node.runtime->draw_bounds.xmin = loc.x;
  node.runtime->draw_bounds.xmax = loc.x + NODE_WIDTH(node);
  node.runtime->draw_bounds.ymax = loc.y;
  node.runtime->draw_bounds.ymin = min_ff(dy, loc.y - 2 * NODE_DY);

  /* Set the block bounds to clip mouse events from underlying nodes.
   * Add a margin for sockets on each side. */
  UI_block_bounds_set_explicit(&block,
                               node.runtime->draw_bounds.xmin - NODE_SOCKSIZE,
                               node.runtime->draw_bounds.ymin,
                               node.runtime->draw_bounds.xmax + NODE_SOCKSIZE,
                               node.runtime->draw_bounds.ymax);
}

/**
 * Based on settings in node, sets drawing rect info.
 */
static void node_update_hidden(bNode &node, uiBlock &block)
{
  int totin = 0, totout = 0;

  /* Round the node origin because text contents are always pixel-aligned. */
  const float2 loc = math::round(node_to_view(node.location));

  /* Calculate minimal radius. */
  for (const bNodeSocket *socket : node.input_sockets()) {
    if (socket->is_visible()) {
      totin++;
    }
  }
  for (const bNodeSocket *socket : node.output_sockets()) {
    if (socket->is_visible()) {
      totout++;
    }
  }

  float hiddenrad = HIDDEN_RAD;
  float tot = std::max(totin, totout);
  if (tot > 4) {
    hiddenrad += 5.0f * float(tot - 4);
  }

  node.runtime->draw_bounds.xmin = loc.x;
  node.runtime->draw_bounds.xmax = loc.x + max_ff(NODE_WIDTH(node), 2 * hiddenrad);
  node.runtime->draw_bounds.ymax = loc.y + (hiddenrad - 0.5f * NODE_DY);
  node.runtime->draw_bounds.ymin = node.runtime->draw_bounds.ymax - 2 * hiddenrad;

  /* Output sockets. */
  float rad = float(M_PI) / (1.0f + float(totout));
  float drad = rad;

  for (bNodeSocket *socket : node.output_sockets()) {
    if (socket->is_visible()) {
      /* Round the socket location to stop it from jiggling. */
      socket->runtime->location = {
          round(node.runtime->draw_bounds.xmax - hiddenrad + sinf(rad) * hiddenrad),
          round(node.runtime->draw_bounds.ymin + hiddenrad + cosf(rad) * hiddenrad)};
      rad += drad;
    }
  }

  /* Input sockets. */
  rad = drad = -float(M_PI) / (1.0f + float(totin));

  for (bNodeSocket *socket : node.input_sockets()) {
    if (socket->is_visible()) {
      /* Round the socket location to stop it from jiggling. */
      socket->runtime->location = {
          round(node.runtime->draw_bounds.xmin + hiddenrad + sinf(rad) * hiddenrad),
          round(node.runtime->draw_bounds.ymin + hiddenrad + cosf(rad) * hiddenrad)};
      rad += drad;
    }
  }

  /* Set the block bounds to clip mouse events from underlying nodes.
   * Add a margin for sockets on each side. */
  UI_block_bounds_set_explicit(&block,
                               node.runtime->draw_bounds.xmin - NODE_SOCKSIZE,
                               node.runtime->draw_bounds.ymin,
                               node.runtime->draw_bounds.xmax + NODE_SOCKSIZE,
                               node.runtime->draw_bounds.ymax);
}

static int node_get_colorid(TreeDrawContext &tree_draw_ctx, const bNode &node)
{
  const int nclass = (node.typeinfo->ui_class == nullptr) ? node.typeinfo->nclass :
                                                            node.typeinfo->ui_class(&node);
  switch (nclass) {
    case NODE_CLASS_INPUT:
      return TH_NODE_INPUT;
    case NODE_CLASS_OUTPUT: {
      if (node.type_legacy == GEO_NODE_VIEWER) {
        return &node == tree_draw_ctx.active_geometry_nodes_viewer ? TH_NODE_OUTPUT : TH_NODE;
      }
      const bool is_output_node = (node.flag & NODE_DO_OUTPUT) ||
                                  (node.type_legacy == CMP_NODE_OUTPUT_FILE);
      return is_output_node ? TH_NODE_OUTPUT : TH_NODE;
    }
    case NODE_CLASS_CONVERTER:
      return TH_NODE_CONVERTER;
    case NODE_CLASS_OP_COLOR:
      return TH_NODE_COLOR;
    case NODE_CLASS_OP_VECTOR:
      return TH_NODE_VECTOR;
    case NODE_CLASS_OP_FILTER:
      return TH_NODE_FILTER;
    case NODE_CLASS_GROUP:
      return TH_NODE_GROUP;
    case NODE_CLASS_INTERFACE:
      return TH_NODE_INTERFACE;
    case NODE_CLASS_MATTE:
      return TH_NODE_MATTE;
    case NODE_CLASS_DISTORT:
      return TH_NODE_DISTORT;
    case NODE_CLASS_TEXTURE:
      return TH_NODE_TEXTURE;
    case NODE_CLASS_SHADER:
      return TH_NODE_SHADER;
    case NODE_CLASS_SCRIPT:
      return TH_NODE_SCRIPT;
    case NODE_CLASS_PATTERN:
      return TH_NODE_PATTERN;
    case NODE_CLASS_LAYOUT:
      return TH_NODE_LAYOUT;
    case NODE_CLASS_GEOMETRY:
      return TH_NODE_GEOMETRY;
    case NODE_CLASS_ATTRIBUTE:
      return TH_NODE_ATTRIBUTE;
    default:
      return TH_NODE;
  }
}

static void node_draw_mute_line(const bContext &C,
                                const View2D &v2d,
                                const SpaceNode &snode,
                                const bNode &node)
{
  GPU_blend(GPU_BLEND_ALPHA);

  for (const bNodeLink &link : node.internal_links()) {
    if (!bke::node_link_is_hidden(&link)) {
      node_draw_link_bezier(C, v2d, snode, link, TH_WIRE_INNER, TH_WIRE_INNER, TH_WIRE, false);
    }
  }

  GPU_blend(GPU_BLEND_NONE);
}

static void node_socket_tooltip_set(uiBlock &block,
                                    const int socket_index_in_tree,
                                    const float2 location,
                                    const float2 size)
{
  /* Ideally sockets themselves should be buttons, but they aren't currently. So add an invisible
   * button on top of them for the tooltip. */
  uiBut *but = uiDefIconBut(&block,
                            UI_BTYPE_LABEL,
                            0,
                            ICON_NONE,
                            location.x - size.x / 2.0f,
                            location.y - size.y / 2.0f,
                            size.x,
                            size.y,
                            nullptr,
                            0,
                            0,
                            nullptr);
  UI_but_func_tooltip_set(
      but,
      [](bContext *C, void *argN, const char * /*tip*/) {
        const SpaceNode &snode = *CTX_wm_space_node(C);
        const bNodeTree &ntree = *snode.edittree;
        const int index_in_tree = POINTER_AS_INT(argN);
        ntree.ensure_topology_cache();
        return node_socket_get_tooltip(&snode, ntree, *ntree.all_sockets()[index_in_tree]);
      },
      POINTER_FROM_INT(socket_index_in_tree),
      nullptr);
}

static const float virtual_node_socket_outline_color[4] = {0.5, 0.5, 0.5, 1.0};

static void node_socket_outline_color_get(const bool selected,
                                          const int socket_type,
                                          float r_outline_color[4])
{
  if (selected) {
    UI_GetThemeColor4fv(TH_ACTIVE, r_outline_color);
  }
  else if (socket_type == SOCK_CUSTOM) {
    /* Until there is a better place for per socket color,
     * the outline color for virtual sockets is set here. */
    copy_v4_v4(r_outline_color, virtual_node_socket_outline_color);
  }
  else {
    UI_GetThemeColor4fv(TH_WIRE, r_outline_color);
    r_outline_color[3] = 1.0f;
  }
}

void node_socket_color_get(const bContext &C,
                           const bNodeTree &ntree,
                           PointerRNA &node_ptr,
                           const bNodeSocket &sock,
                           float r_color[4])
{
  if (!sock.typeinfo->draw_color) {
    /* Fallback to the simple variant. If not defined either, fallback to a magenta color. */
    if (sock.typeinfo->draw_color_simple) {
      sock.typeinfo->draw_color_simple(sock.typeinfo, r_color);
    }
    else {
      copy_v4_v4(r_color, float4(1.0f, 0.0f, 1.0f, 1.0f));
    }
    return;
  }

  BLI_assert(RNA_struct_is_a(node_ptr.type, &RNA_Node));
  PointerRNA ptr = RNA_pointer_create_discrete(
      &const_cast<ID &>(ntree.id), &RNA_NodeSocket, &const_cast<bNodeSocket &>(sock));
  sock.typeinfo->draw_color((bContext *)&C, &ptr, &node_ptr, r_color);
}

static void create_inspection_string_for_generic_value(const bNodeSocket &socket,
                                                       const GPointer value,
                                                       fmt::memory_buffer &buf)
{
  auto id_to_inspection_string = [&](const ID *id, const short idcode) {
    fmt::format_to(fmt::appender(buf), "{}", id ? id->name + 2 : TIP_("None"));
    fmt::format_to(fmt::appender(buf), " (");
    fmt::format_to(fmt::appender(buf), "{}", TIP_(BKE_idtype_idcode_to_name(idcode)));
    fmt::format_to(fmt::appender(buf), ")");
  };

  const CPPType &value_type = *value.type();
  const void *buffer = value.get();
  if (value_type.is<Object *>()) {
    id_to_inspection_string(*static_cast<const ID *const *>(buffer), ID_OB);
    return;
  }
  if (value_type.is<Material *>()) {
    id_to_inspection_string(*static_cast<const ID *const *>(buffer), ID_MA);
    return;
  }
  if (value_type.is<Tex *>()) {
    id_to_inspection_string(*static_cast<const ID *const *>(buffer), ID_TE);
    return;
  }
  if (value_type.is<Image *>()) {
    id_to_inspection_string(*static_cast<const ID *const *>(buffer), ID_IM);
    return;
  }
  if (value_type.is<Collection *>()) {
    id_to_inspection_string(*static_cast<const ID *const *>(buffer), ID_GR);
    return;
  }
  if (value_type.is<std::string>()) {
    fmt::format_to(fmt::appender(buf),
                   fmt::runtime(TIP_("{} (String)")),
                   *static_cast<const std::string *>(buffer));
    return;
  }

  const CPPType &socket_type = *socket.typeinfo->base_cpp_type;

  if (socket.type == SOCK_MENU) {
    if (!value_type.is<int>()) {
      return;
    }
    const int item_identifier = *static_cast<const int *>(buffer);
    const auto *socket_storage = socket.default_value_typed<bNodeSocketValueMenu>();
    if (!socket_storage->enum_items) {
      return;
    }
    if (socket_storage->has_conflict()) {
      return;
    }
    const bke::RuntimeNodeEnumItem *enum_item =
        socket_storage->enum_items->find_item_by_identifier(item_identifier);
    if (!enum_item) {
      return;
    }
    fmt::format_to(fmt::appender(buf), fmt::runtime(TIP_("{} (Menu)")), enum_item->name);
    return;
  }

  const bke::DataTypeConversions &convert = bke::get_implicit_type_conversions();
  if (value_type != socket_type) {
    if (!convert.is_convertible(value_type, socket_type)) {
      return;
    }
  }
  BUFFER_FOR_CPP_TYPE_VALUE(socket_type, socket_value);
  /* This will just copy the value if the types are equal. */
  convert.convert_to_uninitialized(value_type, socket_type, buffer, socket_value);
  BLI_SCOPED_DEFER([&]() { socket_type.destruct(socket_value); });

  if (socket_type.is<int>()) {
    fmt::format_to(
        fmt::appender(buf), fmt::runtime(TIP_("{} (Integer)")), *static_cast<int *>(socket_value));
  }
  else if (socket_type.is<float>()) {
    const float float_value = *static_cast<float *>(socket_value);
    /* Above that threshold floats can't represent fractions anymore. */
    if (std::abs(float_value) > (1 << 24)) {
      /* Use higher precision to display correct integer value instead of one that is rounded to
       * fewer significant digits. */
      fmt::format_to(fmt::appender(buf), fmt::runtime(TIP_("{:.10} (Float)")), float_value);
    }
    else {
      fmt::format_to(fmt::appender(buf), fmt::runtime(TIP_("{} (Float)")), float_value);
    }
  }
  else if (socket_type.is<blender::float3>()) {
    const blender::float3 &vector = *static_cast<blender::float3 *>(socket_value);
    fmt::format_to(fmt::appender(buf),
                   fmt::runtime(TIP_("({}, {}, {}) (Vector)")),
                   vector.x,
                   vector.y,
                   vector.z);
  }
  else if (socket_type.is<blender::ColorGeometry4f>()) {
    const blender::ColorGeometry4f &color = *static_cast<blender::ColorGeometry4f *>(socket_value);
    fmt::format_to(fmt::appender(buf),
                   fmt::runtime(TIP_("({}, {}, {}, {}) (Color)")),
                   color.r,
                   color.g,
                   color.b,
                   color.a);
  }
  else if (socket_type.is<math::Quaternion>()) {
    const math::Quaternion &rotation = *static_cast<math::Quaternion *>(socket_value);
    const math::EulerXYZ euler = math::to_euler(rotation);
    fmt::format_to(fmt::appender(buf),
                   ("({}" BLI_STR_UTF8_DEGREE_SIGN ", {}" BLI_STR_UTF8_DEGREE_SIGN
                    ", {}" BLI_STR_UTF8_DEGREE_SIGN ")"),
                   euler.x().degree(),
                   euler.y().degree(),
                   euler.z().degree());
    fmt::format_to(fmt::appender(buf), "{}", TIP_("(Rotation)"));
  }
  else if (socket_type.is<bool>()) {
    fmt::format_to(fmt::appender(buf),
                   fmt::runtime(TIP_("{} (Boolean)")),
                   ((*static_cast<bool *>(socket_value)) ? TIP_("True") : TIP_("False")));
  }
  else if (socket_type.is<float4x4>()) {
    /* Transpose to be able to print row by row. */
    const float4x4 value = math::transpose(*static_cast<const float4x4 *>(socket_value));
    std::stringstream ss;
    ss << value[0] << ",\n";
    ss << value[1] << ",\n";
    ss << value[2] << ",\n";
    ss << value[3] << ",\n";
    buf.append(ss.str());
    fmt::format_to(fmt::appender(buf), "{}", TIP_("(Matrix)"));
  }
}

static void create_inspection_string_for_field_info(const bNodeSocket &socket,
                                                    const geo_log::FieldInfoLog &value_log,
                                                    fmt::memory_buffer &buf)
{
  const CPPType &socket_type = *socket.typeinfo->base_cpp_type;
  const Span<std::string> input_tooltips = value_log.input_tooltips;

  if (input_tooltips.is_empty()) {
    /* Should have been logged as constant value. */
    BLI_assert_unreachable();
    fmt::format_to(fmt::appender(buf), "{}", TIP_("Value has not been logged"));
  }
  else {
    if (socket_type.is<int>()) {
      fmt::format_to(fmt::appender(buf), "{}", TIP_("Integer field based on:"));
    }
    else if (socket_type.is<float>()) {
      fmt::format_to(fmt::appender(buf), "{}", TIP_("Float field based on:"));
    }
    else if (socket_type.is<blender::float3>()) {
      fmt::format_to(fmt::appender(buf), "{}", TIP_("Vector field based on:"));
    }
    else if (socket_type.is<bool>()) {
      fmt::format_to(fmt::appender(buf), "{}", TIP_("Boolean field based on:"));
    }
    else if (socket_type.is<std::string>()) {
      fmt::format_to(fmt::appender(buf), "{}", TIP_("String field based on:"));
    }
    else if (socket_type.is<blender::ColorGeometry4f>()) {
      fmt::format_to(fmt::appender(buf), "{}", TIP_("Color field based on:"));
    }
    else if (socket_type.is<math::Quaternion>()) {
      fmt::format_to(fmt::appender(buf), "{}", TIP_("Rotation field based on:"));
    }
    fmt::format_to(fmt::appender(buf), "\n");

    for (const int i : input_tooltips.index_range()) {
      const blender::StringRefNull tooltip = input_tooltips[i];
      fmt::format_to(fmt::appender(buf), fmt::runtime(TIP_("\u2022 {}")), TIP_(tooltip.c_str()));
      if (i < input_tooltips.size() - 1) {
        fmt::format_to(fmt::appender(buf), ".\n");
      }
    }
  }
}

static void create_inspection_string_for_geometry_info(const geo_log::GeometryInfoLog &value_log,
                                                       fmt::memory_buffer &buf)
{
  auto to_string = [](int value) {
    char str[BLI_STR_FORMAT_INT32_GROUPED_SIZE];
    BLI_str_format_int_grouped(str, value);
    return std::string(str);
  };

  if (value_log.grid_info) {
    const geo_log::GeometryInfoLog::GridInfo &grid_info = *value_log.grid_info;
    fmt::format_to(
        fmt::appender(buf), "{}", grid_info.is_empty ? TIP_("Empty Grid") : TIP_("\u2022 Grid"));
    return;
  }

  Span<bke::GeometryComponent::Type> component_types = value_log.component_types;
  if (component_types.is_empty()) {
    fmt::format_to(fmt::appender(buf), "{}", TIP_("Empty Geometry"));
    return;
  }

  fmt::format_to(fmt::appender(buf), "{}", TIP_("Geometry:"));
  if (!value_log.name.empty()) {
    fmt::format_to(fmt::appender(buf), " \"{}\"", value_log.name);
  }
  fmt::format_to(fmt::appender(buf), "\n");
  for (bke::GeometryComponent::Type type : component_types) {
    switch (type) {
      case bke::GeometryComponent::Type::Mesh: {
        const geo_log::GeometryInfoLog::MeshInfo &mesh_info = *value_log.mesh_info;
        fmt::format_to(fmt::appender(buf),
                       fmt::runtime(TIP_("\u2022 Mesh: {} vertices, {} edges, {} faces")),
                       to_string(mesh_info.verts_num),
                       to_string(mesh_info.edges_num),
                       to_string(mesh_info.faces_num));
        break;
      }
      case bke::GeometryComponent::Type::PointCloud: {
        const geo_log::GeometryInfoLog::PointCloudInfo &pointcloud_info =
            *value_log.pointcloud_info;
        fmt::format_to(fmt::appender(buf),
                       fmt::runtime(TIP_("\u2022 Point Cloud: {} points")),
                       to_string(pointcloud_info.points_num));
        break;
      }
      case bke::GeometryComponent::Type::Curve: {
        const geo_log::GeometryInfoLog::CurveInfo &curve_info = *value_log.curve_info;
        fmt::format_to(fmt::appender(buf),
                       fmt::runtime(TIP_("\u2022 Curve: {} points, {} splines")),
                       to_string(curve_info.points_num),
                       to_string(curve_info.splines_num));
        break;
      }
      case bke::GeometryComponent::Type::Instance: {
        const geo_log::GeometryInfoLog::InstancesInfo &instances_info = *value_log.instances_info;
        fmt::format_to(fmt::appender(buf),
                       fmt::runtime(TIP_("\u2022 Instances: {}")),
                       to_string(instances_info.instances_num));
        break;
      }
      case bke::GeometryComponent::Type::Volume: {
        const geo_log::GeometryInfoLog::VolumeInfo &volume_info = *value_log.volume_info;
        fmt::format_to(fmt::appender(buf),
                       fmt::runtime(TIP_("\u2022 Volume: {} grids")),
                       volume_info.grids_num);
        break;
      }
      case bke::GeometryComponent::Type::Edit: {
        if (value_log.edit_data_info.has_value()) {
          const geo_log::GeometryInfoLog::EditDataInfo &edit_info = *value_log.edit_data_info;
          fmt::format_to(fmt::appender(buf),
                         fmt::runtime(TIP_("\u2022 Edit: {}, {}, {}")),
                         edit_info.has_deformed_positions ? TIP_("positions") :
                                                            TIP_("no positions"),
                         edit_info.has_deform_matrices ? TIP_("matrices") : TIP_("no matrices"),
                         edit_info.gizmo_transforms_num > 0 ? TIP_("gizmos") : TIP_("no gizmos"));
        }
        break;
      }
      case bke::GeometryComponent::Type::GreasePencil: {
        const geo_log::GeometryInfoLog::GreasePencilInfo &grease_pencil_info =
            *value_log.grease_pencil_info;
        fmt::format_to(fmt::appender(buf),
                       fmt::runtime(TIP_("\u2022 Grease Pencil: {} layers")),
                       to_string(grease_pencil_info.layers_num));
        break;
      }
    }
    if (type != component_types.last()) {
      fmt::format_to(fmt::appender(buf), ".\n");
    }
  }
}

static void create_inspection_string_for_geometry_socket(fmt::memory_buffer &buf,
                                                         const nodes::decl::Geometry *socket_decl)
{
  /* If the geometry declaration is null, as is the case for input to group output,
   * or it is an output socket don't show supported types. */
  if (socket_decl == nullptr || socket_decl->in_out == SOCK_OUT) {
    return;
  }

  Span<bke::GeometryComponent::Type> supported_types = socket_decl->supported_types();
  if (supported_types.is_empty()) {
    fmt::format_to(fmt::appender(buf), "{}", TIP_("Supported: All Types"));
    return;
  }

  fmt::format_to(fmt::appender(buf), "{}", TIP_("Supported: "));
  for (bke::GeometryComponent::Type type : supported_types) {
    switch (type) {
      case bke::GeometryComponent::Type::Mesh: {
        fmt::format_to(fmt::appender(buf), "{}", TIP_("Mesh"));
        break;
      }
      case bke::GeometryComponent::Type::PointCloud: {
        fmt::format_to(fmt::appender(buf), "{}", TIP_("Point Cloud"));
        break;
      }
      case bke::GeometryComponent::Type::Curve: {
        fmt::format_to(fmt::appender(buf), "{}", TIP_("Curve"));
        break;
      }
      case bke::GeometryComponent::Type::Instance: {
        fmt::format_to(fmt::appender(buf), "{}", TIP_("Instances"));
        break;
      }
      case bke::GeometryComponent::Type::Volume: {
        fmt::format_to(fmt::appender(buf), "{}", CTX_TIP_(BLT_I18NCONTEXT_ID_ID, "Volume"));
        break;
      }
      case bke::GeometryComponent::Type::Edit: {
        break;
      }
      case bke::GeometryComponent::Type::GreasePencil: {
        fmt::format_to(fmt::appender(buf), "{}", TIP_("Grease Pencil"));
        break;
      }
    }
    if (type != supported_types.last()) {
      fmt::format_to(fmt::appender(buf), ", ");
    }
  }
}

static void create_inspection_string_for_default_socket_value(const bNodeSocket &socket,
                                                              fmt::memory_buffer &buf)
{
  if (!socket.is_input()) {
    return;
  }
  if (socket.is_multi_input()) {
    return;
  }
  if (socket.owner_node().is_reroute()) {
    return;
  }
  const Span<const bNodeSocket *> connected_sockets = socket.directly_linked_sockets();
  if (!connected_sockets.is_empty() && !connected_sockets[0]->owner_node().is_dangling_reroute()) {
    return;
  }
  if (const nodes::SocketDeclaration *socket_decl = socket.runtime->declaration) {
    if (socket_decl->input_field_type == nodes::InputSocketFieldType::Implicit) {
      return;
    }
  }
  if (socket.typeinfo->base_cpp_type == nullptr) {
    return;
  }

  const CPPType &value_type = *socket.typeinfo->base_cpp_type;
  BUFFER_FOR_CPP_TYPE_VALUE(value_type, socket_value);
  socket.typeinfo->get_base_cpp_value(socket.default_value, socket_value);
  create_inspection_string_for_generic_value(socket, GPointer(value_type, socket_value), buf);
  value_type.destruct(socket_value);
}

static std::optional<std::string> create_description_inspection_string(const bNodeSocket &socket)
{
  if (socket.runtime->declaration == nullptr) {
    if (socket.description[0]) {
      return socket.description;
    }
    return std::nullopt;
  }
  const blender::nodes::SocketDeclaration &socket_decl = *socket.runtime->declaration;
  blender::StringRefNull description = socket_decl.description;
  if (description.is_empty()) {
    return std::nullopt;
  }

  return TIP_(description.c_str());
}

static std::optional<std::string> create_log_inspection_string(geo_log::GeoTreeLog *geo_tree_log,
                                                               const bNodeSocket &socket)
{
  if (geo_tree_log == nullptr) {
    return std::nullopt;
  }
  if (socket.typeinfo->base_cpp_type == nullptr) {
    return std::nullopt;
  }

  geo_tree_log->ensure_socket_values();
  geo_log::ValueLog *value_log = geo_tree_log->find_socket_value_log(socket);
  fmt::memory_buffer buf;
  if (const geo_log::GenericValueLog *generic_value_log =
          dynamic_cast<const geo_log::GenericValueLog *>(value_log))
  {
    create_inspection_string_for_generic_value(socket, generic_value_log->value, buf);
  }
  else if (const geo_log::FieldInfoLog *gfield_value_log =
               dynamic_cast<const geo_log::FieldInfoLog *>(value_log))
  {
    create_inspection_string_for_field_info(socket, *gfield_value_log, buf);
  }
  else if (const geo_log::GeometryInfoLog *geo_value_log =
               dynamic_cast<const geo_log::GeometryInfoLog *>(value_log))
  {
    create_inspection_string_for_geometry_info(*geo_value_log, buf);
  }

  std::string str = fmt::to_string(buf);
  if (str.empty()) {
    return std::nullopt;
  }
  return str;
}

static std::optional<std::string> create_declaration_inspection_string(const bNodeSocket &socket)
{
  fmt::memory_buffer buf;
  if (const nodes::decl::Geometry *socket_decl = dynamic_cast<const nodes::decl::Geometry *>(
          socket.runtime->declaration))
  {
    create_inspection_string_for_geometry_socket(buf, socket_decl);
  }

  std::string str = fmt::to_string(buf);
  if (str.empty()) {
    return std::nullopt;
  }
  return str;
}

static geo_log::GeoTreeLog *geo_tree_log_for_socket(const bNodeTree &ntree,
                                                    const bNodeSocket &socket,
                                                    TreeDrawContext &tree_draw_ctx)
{
  const bNodeTreeZones *zones = ntree.zones();
  if (!zones) {
    return nullptr;
  }
  const bNodeTreeZone *zone = zones->get_zone_by_socket(socket);
  return tree_draw_ctx.geo_log_by_zone.lookup_default(zone, nullptr);
}

static Vector<std::string> lines_of_text(std::string text)
{
  Vector<std::string> result;
  std::istringstream text_stream(text);
  for (std::string line; std::getline(text_stream, line);) {
    result.append(line);
  }
  return result;
}

static std::optional<std::string> create_multi_input_log_inspection_string(
    const bNodeTree &ntree, const bNodeSocket &socket, TreeDrawContext &tree_draw_ctx)
{
  if (!socket.is_multi_input()) {
    return std::nullopt;
  }

  Vector<std::pair<int, std::string>, 8> numerated_info;

  const Span<const bNodeLink *> connected_links = socket.directly_linked_links();
  for (const int index : connected_links.index_range()) {
    const bNodeLink *link = connected_links[index];
    const int connection_number = index + 1;
    if (!link->is_used()) {
      continue;
    }
    if (!(link->flag & NODE_LINK_VALID)) {
      continue;
    }
    if (link->fromnode->is_dangling_reroute()) {
      continue;
    }
    const bNodeSocket &connected_socket = *link->fromsock;
    geo_log::GeoTreeLog *geo_tree_log = geo_tree_log_for_socket(
        ntree, connected_socket, tree_draw_ctx);
    const std::optional<std::string> input_log = create_log_inspection_string(geo_tree_log,
                                                                              connected_socket);
    if (!input_log.has_value()) {
      continue;
    }
    numerated_info.append({connection_number, std::move(*input_log)});
  }

  if (numerated_info.is_empty()) {
    return std::nullopt;
  }

  fmt::memory_buffer buf;
  for (const std::pair<int, std::string> &info : numerated_info) {
    const Vector<std::string> lines = lines_of_text(info.second);
    fmt::format_to(fmt::appender(buf), "{}", info.first);
    fmt::format_to(fmt::appender(buf), ". ");
    fmt::format_to(fmt::appender(buf), "{}", lines.first());
    for (const std::string &line : lines.as_span().drop_front(1)) {
      fmt::format_to(fmt::appender(buf), "\n  {}", line);
    }
    if (&info != &numerated_info.last()) {
      buf.append(StringRef(".\n"));
    }
  }

  const std::string str = fmt::to_string(buf);
  if (str.empty()) {
    return std::nullopt;
  }

  return str;
}

static std::optional<std::string> create_default_value_inspection_string(const bNodeSocket &socket)
{
  fmt::memory_buffer buf;
  create_inspection_string_for_default_socket_value(socket, buf);

  std::string str = fmt::to_string(buf);
  if (str.empty()) {
    return std::nullopt;
  }
  return str;
}

static const bNodeSocket *target_for_reroute(const bNodeSocket &reroute_output)
{
  const bNodeSocket *output = &reroute_output;
  Set<const bNode *> visited_nodes;
  visited_nodes.add(&reroute_output.owner_node());
  while (true) {
    const Span<const bNodeSocket *> linked_sockets = output->directly_linked_sockets();
    if (linked_sockets.size() != 1) {
      return nullptr;
    }
    const bNode &target_node = linked_sockets[0]->owner_node();
    if (!visited_nodes.add(&target_node)) {
      return nullptr;
    }
    if (!target_node.is_dangling_reroute()) {
      return linked_sockets[0];
    }
    output = target_node.output_sockets()[0];
  }
}

static std::optional<std::string> create_dangling_reroute_inspection_string(
    const bNodeTree &ntree, const bNodeSocket &socket)
{
  if (ntree.type != NTREE_GEOMETRY) {
    return std::nullopt;
  }

  const bNode &node = socket.owner_node();
  if (!node.is_dangling_reroute()) {
    return std::nullopt;
  }

  const bNodeSocket &output_socket = *node.output_sockets()[0];
  const bNodeSocket *target_socket = target_for_reroute(output_socket);

  if (target_socket == nullptr) {
    if (!output_socket.directly_linked_sockets().is_empty()) {
      return TIP_("Dangling reroute is ignored by all targets");
    }
    return std::nullopt;
  }

  if (target_socket->is_multi_input()) {
    return TIP_("Dangling reroute branch is ignored by multi input socket");
  }

  fmt::memory_buffer buf;
  create_inspection_string_for_default_socket_value(*target_socket, buf);
  std::string str = fmt::to_string(buf);
  if (str.empty()) {
    return TIP_("Dangling reroute is ignored");
  }
  fmt::format_to(fmt::appender(buf), ".\n\n");
  fmt::format_to(fmt::appender(buf),
                 "{}",
                 TIP_("Dangling reroute is ignored, default value of target socket is used"));
  return str;
}

static std::string node_socket_get_tooltip(const SpaceNode *snode,
                                           const bNodeTree &ntree,
                                           const bNodeSocket &socket)
{
  TreeDrawContext tree_draw_ctx;
  if (snode != nullptr) {
    if (ntree.type == NTREE_GEOMETRY) {
      tree_draw_ctx.geo_log_by_zone =
          geo_log::GeoModifierLog::get_tree_log_by_zone_for_node_editor(*snode);
    }
  }

  geo_log::GeoTreeLog *geo_tree_log = geo_tree_log_for_socket(ntree, socket, tree_draw_ctx);

  Vector<std::string> inspection_strings;

  if (std::optional<std::string> info = create_description_inspection_string(socket)) {
    inspection_strings.append(std::move(*info));
  }
  if (std::optional<std::string> info = create_log_inspection_string(geo_tree_log, socket)) {
    inspection_strings.append(std::move(*info));
  }
  else if (std::optional<std::string> info = create_dangling_reroute_inspection_string(ntree,
                                                                                       socket))
  {
    inspection_strings.append(std::move(*info));
  }
  else if (std::optional<std::string> info = create_default_value_inspection_string(socket)) {
    inspection_strings.append(std::move(*info));
  }
  else if (std::optional<std::string> info = create_multi_input_log_inspection_string(
               ntree, socket, tree_draw_ctx))
  {
    inspection_strings.append(std::move(*info));
  }
  if (std::optional<std::string> info = create_declaration_inspection_string(socket)) {
    inspection_strings.append(std::move(*info));
  }

  std::stringstream output;
  for (const std::string &info : inspection_strings) {
    output << info;
    if (&info != &inspection_strings.last()) {
      output << ".\n\n";
    }
  }

  if (inspection_strings.is_empty()) {
    const bool is_extend = StringRef(socket.idname) == "NodeSocketVirtual";
    const bNode &node = socket.owner_node();
    if (node.is_reroute()) {
      char reroute_name[MAX_NAME];
      bke::nodeLabel(&ntree, &node, reroute_name, sizeof(reroute_name));
      output << reroute_name;
    }
    else if (is_extend) {
      output << TIP_("Connect a link to create a new socket");
    }
    else {
      output << bke::nodeSocketLabel(&socket);
    }

    if (ntree.type == NTREE_GEOMETRY && !is_extend) {
      output << ".\n\n";
      output << TIP_(
          "Unknown socket value. Either the socket was not used or its value was not logged "
          "during the last evaluation");
    }
  }

  return output.str();
}

static void node_socket_add_tooltip_in_node_editor(const bNodeSocket &sock, uiLayout &layout)
{
  uiLayoutSetTooltipFunc(
      &layout,
      [](bContext *C, void *argN, const char * /*tip*/) {
        const SpaceNode &snode = *CTX_wm_space_node(C);
        const bNodeTree &ntree = *snode.edittree;
        const int index_in_tree = POINTER_AS_INT(argN);
        ntree.ensure_topology_cache();
        return node_socket_get_tooltip(&snode, ntree, *ntree.all_sockets()[index_in_tree]);
      },
      POINTER_FROM_INT(sock.index_in_tree()),
      nullptr,
      nullptr);
}

void node_socket_add_tooltip(const bNodeTree &ntree, const bNodeSocket &sock, uiLayout &layout)
{
  struct SocketTooltipData {
    const bNodeTree *ntree;
    const bNodeSocket *socket;
  };

  SocketTooltipData *data = MEM_cnew<SocketTooltipData>(__func__);
  data->ntree = &ntree;
  data->socket = &sock;

  uiLayoutSetTooltipFunc(
      &layout,
      [](bContext *C, void *argN, const char * /*tip*/) {
        SocketTooltipData *data = static_cast<SocketTooltipData *>(argN);
        const SpaceNode *snode = CTX_wm_space_node(C);
        return node_socket_get_tooltip(snode, *data->ntree, *data->socket);
      },
      data,
      MEM_dupallocN,
      MEM_freeN);
}

#define NODE_SOCKET_OUTLINE U.pixelsize

void node_socket_draw(bNodeSocket *sock, const rcti *rect, const float color[4], float scale)
{
  const float radius = NODE_SOCKSIZE * scale;
  const float2 center = {BLI_rcti_cent_x_fl(rect), BLI_rcti_cent_y_fl(rect)};
  const rctf draw_rect = {
      center.x - radius,
      center.x + radius,
      center.y - radius,
      center.y + radius,
  };
  float outline_color[4] = {0};

  node_draw_nodesocket(&draw_rect,
                       color,
                       outline_color,
                       NODE_SOCKET_OUTLINE * scale,
                       sock->display_shape,
                       1.0 / scale);
}

/** Some elements of the node UI are hidden, when they get too small. */
#define NODE_TREE_SCALE_SMALL 0.2f

/** The node tree scales both with the view and with the UI. */
static float node_tree_view_scale(const SpaceNode &snode)
{
  return (1.0f / snode.runtime->aspect) * UI_SCALE_FAC;
}

/* Some elements of the node tree like labels or node sockets are hardly visible when zoomed
 * out and can slow down the drawing quite a bit.
 * This function can be used to check if it's worth to draw those details and return
 * early. */
static bool draw_node_details(const SpaceNode &snode)
{
  return node_tree_view_scale(snode) > NODE_TREE_SCALE_SMALL * UI_INV_SCALE_FAC;
}

static void node_draw_preview_background(rctf *rect)
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_CHECKER);

  /* Drawing the checkerboard. */
  const float checker_dark = UI_ALPHA_CHECKER_DARK / 255.0f;
  const float checker_light = UI_ALPHA_CHECKER_LIGHT / 255.0f;
  immUniform4f("color1", checker_dark, checker_dark, checker_dark, 1.0f);
  immUniform4f("color2", checker_light, checker_light, checker_light, 1.0f);
  immUniform1i("size", 8);
  immRectf(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);
  immUnbindProgram();
}

/* Not a callback. */
static void node_draw_preview(const Scene *scene, ImBuf *preview, const rctf *prv)
{
  float xrect = BLI_rctf_size_x(prv);
  float yrect = BLI_rctf_size_y(prv);
  float xscale = xrect / float(preview->x);
  float yscale = yrect / float(preview->y);
  float scale;

  /* Uniform scale and offset. */
  rctf draw_rect = *prv;
  if (xscale < yscale) {
    float offset = 0.5f * (yrect - float(preview->y) * xscale);
    draw_rect.ymin += offset;
    draw_rect.ymax -= offset;
    scale = xscale;
  }
  else {
    float offset = 0.5f * (xrect - float(preview->x) * yscale);
    draw_rect.xmin += offset;
    draw_rect.xmax -= offset;
    scale = yscale;
  }

  node_draw_preview_background(&draw_rect);

  GPU_blend(GPU_BLEND_ALPHA);
  /* Pre-multiply graphics. */
  GPU_blend(GPU_BLEND_ALPHA);

  ED_draw_imbuf(preview,
                draw_rect.xmin,
                draw_rect.ymin,
                false,
                &scene->view_settings,
                &scene->display_settings,
                scale,
                scale);

  GPU_blend(GPU_BLEND_NONE);

  float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  const float outline_width = 1.0f;
  draw_rect.xmin -= outline_width;
  draw_rect.xmax += outline_width;
  draw_rect.ymin -= outline_width;
  draw_rect.ymax += outline_width;
  UI_draw_roundbox_4fv(&draw_rect, false, BASIS_RAD / 2, black);
}

/* Common handle function for operator buttons that need to select the node first. */
static void node_toggle_button_cb(bContext *C, void *node_argv, void *op_argv)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &node_tree = *snode.edittree;
  bNode &node = *node_tree.node_by_id(POINTER_AS_INT(node_argv));
  const char *opname = (const char *)op_argv;

  /* Select & activate only the button's node. */
  node_select_single(*C, node);

  WM_operator_name_call(C, opname, WM_OP_INVOKE_DEFAULT, nullptr, nullptr);
}

static void node_draw_shadow(const SpaceNode &snode,
                             const bNode &node,
                             const float radius,
                             const float alpha)
{
  const rctf &rct = node.runtime->draw_bounds;
  UI_draw_roundbox_corner_set(UI_CNR_ALL);

  const float shadow_width = 0.6f * U.widget_unit;
  const float shadow_alpha = 0.5f * alpha;

  ui_draw_dropshadow(&rct, radius, shadow_width, snode.runtime->aspect, shadow_alpha);

  /* Outline emphasis. Slight darkening _inside_ the outline. */
  const float color[4] = {0.0f, 0.0f, 0.0f, 0.4f};
  rctf rect{};
  rect.xmin = rct.xmin - 0.5f;
  rect.xmax = rct.xmax + 0.5f;
  rect.ymin = rct.ymin - 0.5f;
  rect.ymax = rct.ymax + 0.5f;
  UI_draw_roundbox_4fv(&rect, false, radius + 0.5f, color);
}

static void node_draw_socket(const bContext &C,
                             const bNodeTree &ntree,
                             const bNode &node,
                             PointerRNA &node_ptr,
                             uiBlock &block,
                             const bNodeSocket &sock,
                             const float outline_thickness,
                             const bool selected,
                             const float aspect)
{
  const float half_width = NODE_SOCKSIZE;

  const bool multi_socket = (sock.flag & SOCK_MULTI_INPUT) && !(node.flag & NODE_HIDDEN);
  float half_height = multi_socket ? node_socket_calculate_height(sock) : half_width;

  ColorTheme4f socket_color;
  ColorTheme4f outline_color;
  node_socket_color_get(C, ntree, node_ptr, sock, socket_color);
  node_socket_outline_color_get(selected, sock.type, outline_color);

  const float2 socket_location = sock.runtime->location;

  const rctf rect = {
      socket_location.x - half_width,
      socket_location.x + half_width,
      socket_location.y - half_height,
      socket_location.y + half_height,
  };

  node_draw_nodesocket(
      &rect, socket_color, outline_color, outline_thickness, sock.display_shape, aspect);

  node_socket_tooltip_set(
      block, sock.index_in_tree(), socket_location, float2(2.0f * half_width, 2.0f * half_height));
}

static void node_draw_sockets(const bContext &C,
                              uiBlock &block,
                              const SpaceNode &snode,
                              const bNodeTree &ntree,
                              const bNode &node)
{
  if (!draw_node_details(snode)) {
    return;
  }

  if (node.input_sockets().is_empty() && node.output_sockets().is_empty()) {
    return;
  }

  PointerRNA nodeptr = RNA_pointer_create_discrete(
      const_cast<ID *>(&ntree.id), &RNA_Node, const_cast<bNode *>(&node));

  const float outline_thickness = NODE_SOCKET_OUTLINE;

  nodesocket_batch_start();
  /* Input sockets. */
  for (const bNodeSocket *sock : node.input_sockets()) {
    if (!node.is_socket_icon_drawn(*sock)) {
      continue;
    }
    const bool selected = (sock->flag & SELECT);
    node_draw_socket(
        C, ntree, node, nodeptr, block, *sock, outline_thickness, selected, snode.runtime->aspect);
  }

  /* Output sockets. */
  for (const bNodeSocket *sock : node.output_sockets()) {
    if (!node.is_socket_icon_drawn(*sock)) {
      continue;
    }
    const bool selected = (sock->flag & SELECT);
    node_draw_socket(
        C, ntree, node, nodeptr, block, *sock, outline_thickness, selected, snode.runtime->aspect);
  }
  nodesocket_batch_end();
}

static void node_panel_toggle_button_cb(bContext *C, void *panel_state_argv, void *ntree_argv)
{
  Main *bmain = CTX_data_main(C);
  bNodePanelState *panel_state = static_cast<bNodePanelState *>(panel_state_argv);
  bNodeTree *ntree = static_cast<bNodeTree *>(ntree_argv);

  panel_state->flag ^= NODE_PANEL_COLLAPSED;

  BKE_main_ensure_invariants(*bmain, ntree->id);
}

/* Draw panel backgrounds first, so other node elements can be rendered on top. */
static void node_draw_panels_background(const bNode &node)
{
  BLI_assert(is_node_panels_supported(node));

  float panel_color[4];
  UI_GetThemeColor4fv(TH_PANEL_SUB_BACK, panel_color);
  /* Increase contrast in nodes a bit. */
  panel_color[3] *= 1.5f;
  const rctf &draw_bounds = node.runtime->draw_bounds;

  const nodes::PanelDeclaration *final_panel_decl = nullptr;

  const nodes::NodeDeclaration &node_decl = *node.declaration();
  for (const int panel_i : node_decl.panels.index_range()) {
    const nodes::PanelDeclaration &panel_decl = *node_decl.panels[panel_i];
    const bke::bNodePanelRuntime &panel_runtime = node.runtime->panels[panel_i];
    if (!panel_runtime.content_extent.has_value()) {
      continue;
    }
    const rctf content_rect = {draw_bounds.xmin,
                               draw_bounds.xmax,
                               panel_runtime.content_extent->min_y,
                               panel_runtime.content_extent->max_y};
    UI_draw_roundbox_corner_set(UI_CNR_NONE);
    UI_draw_roundbox_4fv(&content_rect, true, BASIS_RAD, panel_color);
    if (panel_runtime.content_extent->fill_node_end) {
      final_panel_decl = &panel_decl;
    }
  }
  if (final_panel_decl) {
    const bke::bNodePanelRuntime &final_panel_runtime =
        node.runtime->panels[final_panel_decl->index];
    const rctf content_rect = {draw_bounds.xmin,
                               draw_bounds.xmax,
                               draw_bounds.ymin,
                               final_panel_runtime.content_extent->min_y};
    UI_draw_roundbox_corner_set(UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT);
    const int repeats = final_panel_decl->depth() + 1;
    for ([[maybe_unused]] const int i : IndexRange(repeats)) {
      UI_draw_roundbox_4fv(&content_rect, true, BASIS_RAD, panel_color);
    }
  }
}

/**
 * Note that this is different from #panel_has_input_affecting_node_output in how it treats output
 * sockets. Within the node UI, the panel should not be grayed out if it has an output socket.
 * However, the sidebar only shows inputs, so output sockets should be ignored.
 */
static bool panel_has_only_inactive_inputs(const bNode &node,
                                           const nodes::PanelDeclaration &panel_decl)
{
  for (const nodes::ItemDeclaration *item_decl : panel_decl.items) {
    if (const auto *socket_decl = dynamic_cast<const nodes::SocketDeclaration *>(item_decl)) {
      if (socket_decl->in_out == SOCK_OUT) {
        return false;
      }
      const bNodeSocket &socket = node.socket_by_decl(*socket_decl);
      if (socket.affects_node_output()) {
        return false;
      }
    }
    else if (const auto *sub_panel_decl = dynamic_cast<const nodes::PanelDeclaration *>(item_decl))
    {
      if (!panel_has_only_inactive_inputs(node, *sub_panel_decl)) {
        return false;
      }
    }
  }
  return true;
}

static void node_draw_panels(bNodeTree &ntree, const bNode &node, uiBlock &block)
{
  BLI_assert(is_node_panels_supported(node));
  const rctf &draw_bounds = node.runtime->draw_bounds;

  const nodes::NodeDeclaration &node_decl = *node.declaration();
  for (const int panel_i : node_decl.panels.index_range()) {
    const nodes::PanelDeclaration &panel_decl = *node_decl.panels[panel_i];
    const bke::bNodePanelRuntime &panel_runtime = node.runtime->panels[panel_i];
    const bNodePanelState &panel_state = node.panel_states_array[panel_i];
    if (!panel_runtime.header_center_y.has_value()) {
      continue;
    }

    const rctf header_rect = {draw_bounds.xmin,
                              draw_bounds.xmax,
                              *panel_runtime.header_center_y - NODE_DYS,
                              *panel_runtime.header_center_y + NODE_DYS};
    UI_block_emboss_set(&block, UI_EMBOSS_NONE);

    /* Collapse/expand icon. */
    const int but_size = U.widget_unit * 0.8f;
    uiDefIconBut(&block,
                 UI_BTYPE_BUT_TOGGLE,
                 0,
                 panel_state.is_collapsed() ? ICON_RIGHTARROW : ICON_DOWNARROW_HLT,
                 draw_bounds.xmin + (NODE_MARGIN_X / 3),
                 *panel_runtime.header_center_y - but_size / 2,
                 but_size,
                 but_size,
                 nullptr,
                 0.0f,
                 0.0f,
                 "");

    /* Panel label. */
    uiBut *label_but = uiDefBut(
        &block,
        UI_BTYPE_LABEL,
        0,
        IFACE_(panel_decl.name.c_str()),
        int(draw_bounds.xmin + NODE_MARGIN_X + 0.4f),
        int(*panel_runtime.header_center_y - NODE_DYS),
        short(draw_bounds.xmax - draw_bounds.xmin - (30.0f * UI_SCALE_FAC)),
        NODE_DY,
        nullptr,
        0,
        0,
        "");

    const bool only_inactive_inputs = panel_has_only_inactive_inputs(node, panel_decl);
    if (node.is_muted() || only_inactive_inputs) {
      UI_but_flag_enable(label_but, UI_BUT_INACTIVE);
    }

    /* Invisible button covering the entire header for collapsing/expanding. */
    const int header_but_margin = NODE_MARGIN_X / 3;
    uiBut *toggle_action_but = uiDefIconBut(
        &block,
        UI_BTYPE_BUT_TOGGLE,
        0,
        ICON_NONE,
        header_rect.xmin + header_but_margin,
        header_rect.ymin,
        std::max(int(header_rect.xmax - header_rect.xmin - 2 * header_but_margin), 0),
        header_rect.ymax - header_rect.ymin,
        nullptr,
        0.0f,
        0.0f,
        panel_decl.description.c_str());
    UI_but_func_pushed_state_set(
        toggle_action_but, [&panel_state](const uiBut &) { return panel_state.is_collapsed(); });
    UI_but_func_set(toggle_action_but,
                    node_panel_toggle_button_cb,
                    const_cast<bNodePanelState *>(&panel_state),
                    &ntree);

    UI_block_emboss_set(&block, UI_EMBOSS);
  }
}

static geo_log::NodeWarningType node_error_highest_priority(Span<geo_log::NodeWarning> warnings)
{
  int highest_priority = 0;
  geo_log::NodeWarningType highest_priority_type = geo_log::NodeWarningType::Info;
  for (const geo_log::NodeWarning &warning : warnings) {
    const int priority = node_warning_type_severity(warning.type);
    if (priority > highest_priority) {
      highest_priority = priority;
      highest_priority_type = warning.type;
    }
  }
  return highest_priority_type;
}

struct NodeErrorsTooltipData {
  Span<geo_log::NodeWarning> warnings;
};

static std::string node_errors_tooltip_fn(bContext * /*C*/, void *argN, const char * /*tip*/)
{
  NodeErrorsTooltipData &data = *(NodeErrorsTooltipData *)argN;

  std::string complete_string;

  for (const geo_log::NodeWarning &warning : data.warnings.drop_back(1)) {
    complete_string += warning.message;
    /* Adding the period is not ideal for multi-line messages, but it is consistent
     * with other tooltip implementations in Blender, so it is added here. */
    complete_string += '.';
    complete_string += '\n';
  }

  /* Let the tooltip system automatically add the last period. */
  complete_string += data.warnings.last().message;

  return complete_string;
}

#define NODE_HEADER_ICON_SIZE (0.8f * U.widget_unit)

static void node_add_unsupported_compositor_operation_error_message_button(const bNode &node,
                                                                           uiBlock &block,
                                                                           const rctf &rect,
                                                                           float &icon_offset)
{
  icon_offset -= NODE_HEADER_ICON_SIZE;
  UI_block_emboss_set(&block, UI_EMBOSS_NONE);
  uiDefIconBut(&block,
               UI_BTYPE_BUT,
               0,
               ICON_ERROR,
               icon_offset,
               rect.ymax - NODE_DY,
               NODE_HEADER_ICON_SIZE,
               UI_UNIT_Y,
               nullptr,
               0,
               0,
               TIP_(node.typeinfo->compositor_unsupported_message));
  UI_block_emboss_set(&block, UI_EMBOSS);
}

static void node_add_error_message_button(const TreeDrawContext &tree_draw_ctx,
                                          const bNode &node,
                                          uiBlock &block,
                                          const rctf &rect,
                                          float &icon_offset)
{
  if (tree_draw_ctx.used_by_compositor && node.typeinfo->compositor_unsupported_message) {
    node_add_unsupported_compositor_operation_error_message_button(node, block, rect, icon_offset);
    return;
  }

  geo_log::GeoTreeLog *geo_tree_log = [&]() -> geo_log::GeoTreeLog * {
    const bNodeTreeZones *zones = node.owner_tree().zones();
    if (!zones) {
      return nullptr;
    }
    const bNodeTreeZone *zone = zones->get_zone_by_node(node.identifier);
    if (zone && ELEM(&node, zone->input_node, zone->output_node)) {
      zone = zone->parent_zone;
    }
    return tree_draw_ctx.geo_log_by_zone.lookup_default(zone, nullptr);
  }();

  Span<geo_log::NodeWarning> warnings;
  if (geo_tree_log) {
    geo_log::GeoNodeLog *node_log = geo_tree_log->nodes.lookup_ptr(node.identifier);
    if (node_log != nullptr) {
      warnings = node_log->warnings;
    }
  }
  if (warnings.is_empty()) {
    return;
  }

  const geo_log::NodeWarningType display_type = node_error_highest_priority(warnings);
  NodeErrorsTooltipData *tooltip_data = MEM_new<NodeErrorsTooltipData>(__func__);
  tooltip_data->warnings = warnings;

  icon_offset -= NODE_HEADER_ICON_SIZE;
  UI_block_emboss_set(&block, UI_EMBOSS_NONE);
  uiBut *but = uiDefIconBut(&block,
                            UI_BTYPE_BUT,
                            0,
                            geo_log::node_warning_type_icon(display_type),
                            icon_offset,
                            rect.ymax - NODE_DY,
                            NODE_HEADER_ICON_SIZE,
                            UI_UNIT_Y,
                            nullptr,
                            0,
                            0,
                            nullptr);
  UI_but_func_tooltip_set(but, node_errors_tooltip_fn, tooltip_data, [](void *arg) {
    MEM_delete(static_cast<NodeErrorsTooltipData *>(arg));
  });
  UI_block_emboss_set(&block, UI_EMBOSS);
}

static std::optional<std::chrono::nanoseconds> geo_node_get_execution_time(
    const TreeDrawContext &tree_draw_ctx, const SpaceNode &snode, const bNode &node)
{
  const bNodeTree &ntree = *snode.edittree;

  geo_log::GeoTreeLog *tree_log = [&]() -> geo_log::GeoTreeLog * {
    const bNodeTreeZones *zones = ntree.zones();
    if (!zones) {
      return nullptr;
    }
    const bNodeTreeZone *zone = zones->get_zone_by_node(node.identifier);
    if (zone && ELEM(&node, zone->input_node, zone->output_node)) {
      zone = zone->parent_zone;
    }
    return tree_draw_ctx.geo_log_by_zone.lookup_default(zone, nullptr);
  }();

  if (tree_log == nullptr) {
    return std::nullopt;
  }
  if (node.is_group_output()) {
    return tree_log->execution_time;
  }
  if (node.is_frame()) {
    /* Could be cached in the future if this recursive code turns out to be slow. */
    std::chrono::nanoseconds run_time{0};
    bool found_node = false;

    for (const bNode *tnode : node.direct_children_in_frame()) {
      if (tnode->is_frame()) {
        std::optional<std::chrono::nanoseconds> sub_frame_run_time = geo_node_get_execution_time(
            tree_draw_ctx, snode, *tnode);
        if (sub_frame_run_time.has_value()) {
          run_time += *sub_frame_run_time;
          found_node = true;
        }
      }
      else {
        if (const geo_log::GeoNodeLog *node_log = tree_log->nodes.lookup_ptr_as(tnode->identifier))
        {
          found_node = true;
          run_time += node_log->execution_time;
        }
      }
    }
    if (found_node) {
      return run_time;
    }
    return std::nullopt;
  }
  if (const geo_log::GeoNodeLog *node_log = tree_log->nodes.lookup_ptr(node.identifier)) {
    return node_log->execution_time;
  }
  return std::nullopt;
}

/* Create node key instance, assuming the node comes from the currently edited node tree. */
static bNodeInstanceKey current_node_instance_key(const SpaceNode &snode, const bNode &node)
{
  const bNodeTreePath *path = static_cast<const bNodeTreePath *>(snode.treepath.last);

  /* Some code in this file checks for the non-null elements of the tree path. However, if we did
   * iterate into a node it is expected that there is a tree, and it should be in the path.
   * Otherwise something else went wrong. */
  BLI_assert(path);

  /* Assume that the currently editing tree is the last in the path. */
  BLI_assert(snode.edittree == path->nodetree);

  return bke::node_instance_key(path->parent_key, snode.edittree, &node);
}

static std::optional<std::chrono::nanoseconds> compositor_accumulate_frame_node_execution_time(
    const TreeDrawContext &tree_draw_ctx, const SpaceNode &snode, const bNode &node)
{
  BLI_assert(tree_draw_ctx.compositor_per_node_execution_time);

  timeit::Nanoseconds frame_execution_time(0);
  bool has_any_execution_time = false;

  for (const bNode *current_node : node.direct_children_in_frame()) {
    const bNodeInstanceKey key = current_node_instance_key(snode, *current_node);
    if (const timeit::Nanoseconds *node_execution_time =
            tree_draw_ctx.compositor_per_node_execution_time->lookup_ptr(key))
    {
      frame_execution_time += *node_execution_time;
      has_any_execution_time = true;
    }
  }

  if (!has_any_execution_time) {
    return std::nullopt;
  }

  return frame_execution_time;
}

static std::optional<std::chrono::nanoseconds> compositor_node_get_execution_time(
    const TreeDrawContext &tree_draw_ctx, const SpaceNode &snode, const bNode &node)
{
  BLI_assert(tree_draw_ctx.compositor_per_node_execution_time);

  /* For the frame nodes accumulate execution time of its children. */
  if (node.is_frame()) {
    return compositor_accumulate_frame_node_execution_time(tree_draw_ctx, snode, node);
  }

  /* For other nodes simply lookup execution time.
   * The group node instances have their own entries in the execution times map. */
  const bNodeInstanceKey key = current_node_instance_key(snode, node);
  if (const timeit::Nanoseconds *execution_time =
          tree_draw_ctx.compositor_per_node_execution_time->lookup_ptr(key))
  {
    return *execution_time;
  }

  return std::nullopt;
}

static std::optional<std::chrono::nanoseconds> node_get_execution_time(
    const TreeDrawContext &tree_draw_ctx, const SpaceNode &snode, const bNode &node)
{
  switch (snode.edittree->type) {
    case NTREE_GEOMETRY:
      return geo_node_get_execution_time(tree_draw_ctx, snode, node);
    case NTREE_COMPOSIT:
      return compositor_node_get_execution_time(tree_draw_ctx, snode, node);
    default:
      return std::nullopt;
  }
}

static std::string node_get_execution_time_label(TreeDrawContext &tree_draw_ctx,
                                                 const SpaceNode &snode,
                                                 const bNode &node)
{
  const std::optional<std::chrono::nanoseconds> exec_time = node_get_execution_time(
      tree_draw_ctx, snode, node);

  if (!exec_time.has_value()) {
    return std::string("");
  }

  const uint64_t exec_time_us =
      std::chrono::duration_cast<std::chrono::microseconds>(*exec_time).count();

  /* Don't show time if execution time is 0 microseconds. */
  if (exec_time_us == 0) {
    return std::string("-");
  }
  if (exec_time_us < 100) {
    return std::string("< 0.1 ms");
  }

  int precision = 0;
  /* Show decimal if value is below 1ms */
  if (exec_time_us < 1000) {
    precision = 2;
  }
  else if (exec_time_us < 10000) {
    precision = 1;
  }

  std::stringstream stream;
  stream << std::fixed << std::setprecision(precision) << (exec_time_us / 1000.0f);
  return stream.str() + " ms";
}

struct NamedAttributeTooltipArg {
  Map<StringRefNull, geo_log::NamedAttributeUsage> usage_by_attribute;
};

static std::string named_attribute_tooltip(bContext * /*C*/, void *argN, const char * /*tip*/)
{
  NamedAttributeTooltipArg &arg = *static_cast<NamedAttributeTooltipArg *>(argN);

  fmt::memory_buffer buf;
  fmt::format_to(fmt::appender(buf), "{}", TIP_("Accessed named attributes:"));
  fmt::format_to(fmt::appender(buf), "\n");

  struct NameWithUsage {
    StringRefNull name;
    geo_log::NamedAttributeUsage usage;
  };

  Vector<NameWithUsage> sorted_used_attribute;
  for (auto &&item : arg.usage_by_attribute.items()) {
    sorted_used_attribute.append({item.key, item.value});
  }
  std::sort(sorted_used_attribute.begin(),
            sorted_used_attribute.end(),
            [](const NameWithUsage &a, const NameWithUsage &b) {
              return BLI_strcasecmp_natural(a.name.c_str(), b.name.c_str()) < 0;
            });

  for (const NameWithUsage &attribute : sorted_used_attribute) {
    const StringRefNull name = attribute.name;
    const geo_log::NamedAttributeUsage usage = attribute.usage;
    fmt::format_to(fmt::appender(buf), fmt::runtime(TIP_("  \u2022 \"{}\": ")), name);
    Vector<std::string> usages;
    if ((usage & geo_log::NamedAttributeUsage::Read) != geo_log::NamedAttributeUsage::None) {
      usages.append(TIP_("read"));
    }
    if ((usage & geo_log::NamedAttributeUsage::Write) != geo_log::NamedAttributeUsage::None) {
      usages.append(TIP_("write"));
    }
    if ((usage & geo_log::NamedAttributeUsage::Remove) != geo_log::NamedAttributeUsage::None) {
      usages.append(TIP_("remove"));
    }
    for (const int i : usages.index_range()) {
      fmt::format_to(fmt::appender(buf), "{}", usages[i]);
      if (i < usages.size() - 1) {
        fmt::format_to(fmt::appender(buf), ", ");
      }
    }
    fmt::format_to(fmt::appender(buf), "\n");
  }
  fmt::format_to(fmt::appender(buf), "\n");
  fmt::format_to(
      fmt::appender(buf),
      fmt::runtime(TIP_("Attributes with these names used within the group may conflict with "
                        "existing attributes")));
  return fmt::to_string(buf);
}

static NodeExtraInfoRow row_from_used_named_attribute(
    const Map<StringRefNull, geo_log::NamedAttributeUsage> &usage_by_attribute_name)
{
  const int attributes_num = usage_by_attribute_name.size();

  NodeExtraInfoRow row;
  row.text = std::to_string(attributes_num) +
             (attributes_num == 1 ? RPT_(" Named Attribute") : RPT_(" Named Attributes"));
  row.icon = ICON_SPREADSHEET;
  row.tooltip_fn = named_attribute_tooltip;
  row.tooltip_fn_arg = new NamedAttributeTooltipArg{usage_by_attribute_name};
  row.tooltip_fn_free_arg = [](void *arg) { delete static_cast<NamedAttributeTooltipArg *>(arg); };
  return row;
}

static std::optional<NodeExtraInfoRow> node_get_accessed_attributes_row(
    TreeDrawContext &tree_draw_ctx, const bNode &node)
{
  geo_log::GeoTreeLog *geo_tree_log = [&]() -> geo_log::GeoTreeLog * {
    const bNodeTreeZones *zones = node.owner_tree().zones();
    if (!zones) {
      return nullptr;
    }
    const bNodeTreeZone *zone = zones->get_zone_by_node(node.identifier);
    return tree_draw_ctx.geo_log_by_zone.lookup_default(zone, nullptr);
  }();
  if (geo_tree_log == nullptr) {
    return std::nullopt;
  }
  if (ELEM(node.type_legacy,
           GEO_NODE_STORE_NAMED_ATTRIBUTE,
           GEO_NODE_REMOVE_ATTRIBUTE,
           GEO_NODE_INPUT_NAMED_ATTRIBUTE))
  {
    /* Only show the overlay when the name is passed in from somewhere else. */
    for (const bNodeSocket *socket : node.input_sockets()) {
      if (STREQ(socket->name, "Name")) {
        if (!socket->is_directly_linked()) {
          return std::nullopt;
        }
      }
    }
  }
  geo_tree_log->ensure_used_named_attributes();
  geo_log::GeoNodeLog *node_log = geo_tree_log->nodes.lookup_ptr(node.identifier);
  if (node_log == nullptr) {
    return std::nullopt;
  }
  if (node_log->used_named_attributes.is_empty()) {
    return std::nullopt;
  }
  return row_from_used_named_attribute(node_log->used_named_attributes);
}

static std::optional<NodeExtraInfoRow> node_get_execution_time_label_row(
    TreeDrawContext &tree_draw_ctx, const SpaceNode &snode, const bNode &node)
{
  NodeExtraInfoRow row;
  row.text = node_get_execution_time_label(tree_draw_ctx, snode, node);
  if (row.text.empty()) {
    return std::nullopt;
  }
  row.tooltip = TIP_(
      "The execution time from the node tree's latest evaluation. For frame and group "
      "nodes, the time for all sub-nodes");
  row.icon = ICON_PREVIEW_RANGE;
  return row;
}

static void node_get_compositor_extra_info(TreeDrawContext &tree_draw_ctx,
                                           const SpaceNode &snode,
                                           const bNode &node,
                                           Vector<NodeExtraInfoRow> &rows)
{
  if (snode.overlay.flag & SN_OVERLAY_SHOW_TIMINGS) {
    std::optional<NodeExtraInfoRow> row = node_get_execution_time_label_row(
        tree_draw_ctx, snode, node);
    if (row.has_value()) {
      rows.append(std::move(*row));
    }
  }
}

static void node_get_invalid_links_extra_info(const SpaceNode &snode,
                                              const bNode &node,
                                              Vector<NodeExtraInfoRow> &rows)
{
  const bNodeTree &tree = *snode.edittree;
  const Span<bke::NodeLinkError> link_errors = tree.runtime->link_errors_by_target_node.lookup(
      node.identifier);
  if (link_errors.is_empty()) {
    return;
  }
  NodeExtraInfoRow row;
  row.text = IFACE_("Invalid Link");

  row.tooltip_fn = [](bContext *C, void *arg, const char * /*tip*/) {
    const bNodeTree &tree = *CTX_wm_space_node(C)->edittree;
    const bNode &node = *static_cast<const bNode *>(arg);
    const Span<bke::NodeLinkError> link_errors = tree.runtime->link_errors_by_target_node.lookup(
        node.identifier);
    std::stringstream ss;
    Set<StringRef> already_added_errors;
    for (const int i : link_errors.index_range()) {
      const StringRefNull tooltip = link_errors[i].tooltip;
      if (already_added_errors.add_as(tooltip)) {
        ss << "\u2022 " << tooltip << "\n";
      }
    }
    ss << "\n";
    ss << "Any invalid links are highlighted";
    return ss.str();
  };
  row.tooltip_fn_arg = const_cast<bNode *>(&node);
  row.icon = ICON_ERROR;
  rows.append(std::move(row));
}

static Vector<NodeExtraInfoRow> node_get_extra_info(const bContext &C,
                                                    TreeDrawContext &tree_draw_ctx,
                                                    const SpaceNode &snode,
                                                    const bNode &node)
{
  Vector<NodeExtraInfoRow> rows;

  if (node.typeinfo->get_extra_info) {
    nodes::NodeExtraInfoParams params{rows, node, C};
    node.typeinfo->get_extra_info(params);
  }

  if (node.typeinfo->deprecation_notice) {
    NodeExtraInfoRow row;
    row.text = IFACE_("Deprecated");
    row.icon = ICON_INFO;
    row.tooltip = TIP_(node.typeinfo->deprecation_notice);
    rows.append(std::move(row));
  }

  node_get_invalid_links_extra_info(snode, node, rows);

  if (snode.edittree->type == NTREE_COMPOSIT) {
    node_get_compositor_extra_info(tree_draw_ctx, snode, node, rows);
    return rows;
  }

  if (!(snode.edittree->type == NTREE_GEOMETRY)) {
    /* Currently geometry and compositor nodes are the only nodes to have extra info per nodes. */
    return rows;
  }

  if (snode.overlay.flag & SN_OVERLAY_SHOW_NAMED_ATTRIBUTES) {
    if (std::optional<NodeExtraInfoRow> row = node_get_accessed_attributes_row(tree_draw_ctx,
                                                                               node))
    {
      rows.append(std::move(*row));
    }
  }

  if (snode.overlay.flag & SN_OVERLAY_SHOW_TIMINGS &&
      (ELEM(node.typeinfo->nclass, NODE_CLASS_GEOMETRY, NODE_CLASS_GROUP, NODE_CLASS_ATTRIBUTE) ||
       ELEM(node.type_legacy,
            NODE_FRAME,
            NODE_GROUP_OUTPUT,
            GEO_NODE_SIMULATION_OUTPUT,
            GEO_NODE_REPEAT_OUTPUT,
            GEO_NODE_FOREACH_GEOMETRY_ELEMENT_OUTPUT)))
  {
    std::optional<NodeExtraInfoRow> row = node_get_execution_time_label_row(
        tree_draw_ctx, snode, node);
    if (row.has_value()) {
      rows.append(std::move(*row));
    }
  }

  geo_log::GeoTreeLog *tree_log = [&]() -> geo_log::GeoTreeLog * {
    const bNodeTreeZones *tree_zones = node.owner_tree().zones();
    if (!tree_zones) {
      return nullptr;
    }
    const bNodeTreeZone *zone = tree_zones->get_zone_by_node(node.identifier);
    return tree_draw_ctx.geo_log_by_zone.lookup_default(zone, nullptr);
  }();

  if (tree_log) {
    tree_log->ensure_debug_messages();
    const geo_log::GeoNodeLog *node_log = tree_log->nodes.lookup_ptr(node.identifier);
    if (node_log != nullptr) {
      for (const StringRef message : node_log->debug_messages) {
        NodeExtraInfoRow row;
        row.text = message;
        row.icon = ICON_INFO;
        rows.append(std::move(row));
      }
    }
  }

  return rows;
}

static void node_draw_extra_info_row(const bNode &node,
                                     uiBlock &block,
                                     const rctf &rect,
                                     const int row,
                                     const NodeExtraInfoRow &extra_info_row)
{
  const float but_icon_left = rect.xmin + 6.0f * UI_SCALE_FAC;
  const float but_icon_width = NODE_HEADER_ICON_SIZE * 0.8f;
  const float but_icon_right = but_icon_left + but_icon_width;

  UI_block_emboss_set(&block, UI_EMBOSS_NONE);
  uiBut *but_icon = uiDefIconBut(&block,
                                 UI_BTYPE_BUT,
                                 0,
                                 extra_info_row.icon,
                                 int(but_icon_left),
                                 int(rect.ymin + row * (20.0f * UI_SCALE_FAC)),
                                 but_icon_width,
                                 UI_UNIT_Y,
                                 nullptr,
                                 0,
                                 0,
                                 extra_info_row.tooltip);
  if (extra_info_row.tooltip_fn != nullptr) {
    UI_but_func_tooltip_set(but_icon,
                            extra_info_row.tooltip_fn,
                            extra_info_row.tooltip_fn_arg,
                            extra_info_row.tooltip_fn_free_arg);
  }
  UI_block_emboss_set(&block, UI_EMBOSS);

  const float but_text_left = but_icon_right + 6.0f * UI_SCALE_FAC;
  const float but_text_right = rect.xmax;
  const float but_text_width = but_text_right - but_text_left;

  uiBut *but_text = uiDefBut(&block,
                             UI_BTYPE_LABEL,
                             0,
                             extra_info_row.text.c_str(),
                             int(but_text_left),
                             int(rect.ymin + row * (20.0f * UI_SCALE_FAC)),
                             short(but_text_width),
                             NODE_DY,
                             nullptr,
                             0,
                             0,
                             extra_info_row.tooltip);

  if (extra_info_row.tooltip_fn != nullptr) {
    /* Don't pass tooltip free function because it's already used on the uiBut above. */
    UI_but_func_tooltip_set(
        but_text, extra_info_row.tooltip_fn, extra_info_row.tooltip_fn_arg, nullptr);
  }

  if (node.is_muted()) {
    UI_but_flag_enable(but_text, UI_BUT_INACTIVE);
    UI_but_flag_enable(but_icon, UI_BUT_INACTIVE);
  }
}

static void node_draw_extra_info_panel_back(const bNode &node, const rctf &extra_info_rect)
{
  const rctf &node_rect = node.runtime->draw_bounds;
  rctf panel_back_rect = extra_info_rect;
  /* Extend the panel behind hidden nodes to accommodate the large rounded corners. */
  if (node.flag & NODE_HIDDEN) {
    panel_back_rect.ymin = BLI_rctf_cent_y(&node_rect);
  }

  ColorTheme4f color;
  if (node.is_muted()) {
    UI_GetThemeColorBlend4f(TH_BACK, TH_NODE, 0.2f, color);
  }
  else {
    UI_GetThemeColorBlend4f(TH_BACK, TH_NODE, 0.75f, color);
  }
  color.a -= 0.35f;

  ColorTheme4f color_outline;
  UI_GetThemeColorBlendShade4fv(TH_BACK, TH_NODE, 0.4f, -20, color_outline);

  const float outline_width = U.pixelsize;
  BLI_rctf_pad(&panel_back_rect, outline_width, outline_width);

  UI_draw_roundbox_corner_set(UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT);
  UI_draw_roundbox_4fv_ex(
      &panel_back_rect, color, nullptr, 0.0f, color_outline, outline_width, BASIS_RAD);
}

static void node_draw_extra_info_panel(const bContext &C,
                                       TreeDrawContext &tree_draw_ctx,
                                       const SpaceNode &snode,
                                       const bNode &node,
                                       ImBuf *preview,
                                       uiBlock &block)
{
  const Scene *scene = CTX_data_scene(&C);
  if (!(snode.overlay.flag & SN_OVERLAY_SHOW_OVERLAYS)) {
    return;
  }
  if (preview && !(preview->x > 0 && preview->y > 0)) {
    /* If the preview has an non-drawable size, just don't draw it. */
    preview = nullptr;
  }
  Vector<NodeExtraInfoRow> extra_info_rows = node_get_extra_info(C, tree_draw_ctx, snode, node);
  if (extra_info_rows.is_empty() && !preview) {
    return;
  }

  const rctf &rct = node.runtime->draw_bounds;
  rctf extra_info_rect;

  if (node.is_frame()) {
    extra_info_rect.xmin = rct.xmin;
    extra_info_rect.xmax = rct.xmin + 95.0f * UI_SCALE_FAC;
    extra_info_rect.ymin = rct.ymin + 2.0f * UI_SCALE_FAC;
    extra_info_rect.ymax = rct.ymin + 2.0f * UI_SCALE_FAC;
  }
  else {
    const float padding = 3.0f * UI_SCALE_FAC;

    extra_info_rect.xmin = rct.xmin + padding;
    extra_info_rect.xmax = rct.xmax - padding;
    extra_info_rect.ymin = rct.ymax;
    extra_info_rect.ymax = rct.ymax + extra_info_rows.size() * (20.0f * UI_SCALE_FAC);

    float preview_height = 0.0f;
    rctf preview_rect;
    if (preview) {
      const float width = BLI_rctf_size_x(&extra_info_rect);
      if (preview->x > preview->y) {
        preview_height = (width - 2.0f * padding) * float(preview->y) / float(preview->x) +
                         2.0f * padding;
        preview_rect.ymin = extra_info_rect.ymin + padding;
        preview_rect.ymax = extra_info_rect.ymin + preview_height - padding;
        preview_rect.xmin = extra_info_rect.xmin + padding;
        preview_rect.xmax = extra_info_rect.xmax - padding;
        extra_info_rect.ymax += preview_height;
      }
      else {
        preview_height = width;
        const float preview_width = (width - 2.0f * padding) * float(preview->x) /
                                        float(preview->y) +
                                    2.0f * padding;
        preview_rect.ymin = extra_info_rect.ymin + padding;
        preview_rect.ymax = extra_info_rect.ymin + preview_height - padding;
        preview_rect.xmin = extra_info_rect.xmin + padding + (width - preview_width) / 2;
        preview_rect.xmax = extra_info_rect.xmax - padding - (width - preview_width) / 2;
        extra_info_rect.ymax += preview_height;
      }
    }

    node_draw_extra_info_panel_back(node, extra_info_rect);

    if (preview) {
      node_draw_preview(scene, preview, &preview_rect);
    }

    /* Resize the rect to draw the textual infos on top of the preview. */
    extra_info_rect.ymin += preview_height;
  }

  for (int row : extra_info_rows.index_range()) {
    node_draw_extra_info_row(node, block, extra_info_rect, row, extra_info_rows[row]);
  }
}

static short get_viewer_shortcut_icon(const bNode &node)
{
  BLI_assert(node.is_type("CompositorNodeViewer"));
  switch (node.custom1) {
    case NODE_VIEWER_SHORTCUT_NONE:
      /* No change by default. */
      return node.typeinfo->ui_icon;
    case NODE_VIEWER_SHORCTUT_SLOT_1:
      return ICON_EVENT_ONEKEY;
    case NODE_VIEWER_SHORCTUT_SLOT_2:
      return ICON_EVENT_TWOKEY;
    case NODE_VIEWER_SHORCTUT_SLOT_3:
      return ICON_EVENT_THREEKEY;
    case NODE_VIEWER_SHORCTUT_SLOT_4:
      return ICON_EVENT_FOURKEY;
    case NODE_VIEWER_SHORCTUT_SLOT_5:
      return ICON_EVENT_FIVEKEY;
    case NODE_VIEWER_SHORCTUT_SLOT_6:
      return ICON_EVENT_SIXKEY;
    case NODE_VIEWER_SHORCTUT_SLOT_7:
      return ICON_EVENT_SEVENKEY;
    case NODE_VIEWER_SHORCTUT_SLOT_8:
      return ICON_EVENT_EIGHTKEY;
    case NODE_VIEWER_SHORCTUT_SLOT_9:
      return ICON_EVENT_NINEKEY;
  }

  return node.typeinfo->ui_icon;
}

static void node_draw_basis(const bContext &C,
                            TreeDrawContext &tree_draw_ctx,
                            const View2D &v2d,
                            const SpaceNode &snode,
                            bNodeTree &ntree,
                            const bNode &node,
                            uiBlock &block,
                            bNodeInstanceKey key)
{
  const float iconbutw = NODE_HEADER_ICON_SIZE;
  const bool show_preview = (snode.overlay.flag & SN_OVERLAY_SHOW_OVERLAYS) &&
                            (snode.overlay.flag & SN_OVERLAY_SHOW_PREVIEWS) &&
                            (node.flag & NODE_PREVIEW) &&
                            (USER_EXPERIMENTAL_TEST(&U, use_shader_node_previews) ||
                             ntree.type != NTREE_SHADER);

  /* Skip if out of view. */
  rctf rect_with_preview = node.runtime->draw_bounds;
  if (show_preview) {
    rect_with_preview.ymax += NODE_WIDTH(node);
  }
  if (BLI_rctf_isect(&rect_with_preview, &v2d.cur, nullptr) == false) {
    UI_block_end_ex(&C,
                    tree_draw_ctx.bmain,
                    tree_draw_ctx.window,
                    tree_draw_ctx.scene,
                    tree_draw_ctx.region,
                    tree_draw_ctx.depsgraph,
                    &block);
    return;
  }

  /* Shadow. */
  if (!bke::all_zone_node_types().contains(node.type_legacy)) {
    node_draw_shadow(snode, node, BASIS_RAD, 1.0f);
  }

  const rctf &rct = node.runtime->draw_bounds;
  float color[4];
  int color_id = node_get_colorid(tree_draw_ctx, node);

  GPU_line_width(1.0f);

  /* Overlay atop the node. */
  {
    bool drawn_with_previews = false;

    if (show_preview) {
      bke::bNodeInstanceHash *previews_compo = static_cast<bke::bNodeInstanceHash *>(
          CTX_data_pointer_get(&C, "node_previews").data);
      NestedTreePreviews *previews_shader = tree_draw_ctx.nested_group_infos;

      if (previews_shader) {
        ImBuf *preview = node_preview_acquire_ibuf(ntree, *previews_shader, node);
        node_draw_extra_info_panel(C, tree_draw_ctx, snode, node, preview, block);
        node_release_preview_ibuf(*previews_shader);
        drawn_with_previews = true;
      }
      else if (previews_compo) {
        bNodePreview *preview_compositor = static_cast<bNodePreview *>(
            bke::node_instance_hash_lookup(previews_compo, key));
        if (preview_compositor) {
          node_draw_extra_info_panel(
              C, tree_draw_ctx, snode, node, preview_compositor->ibuf, block);
          drawn_with_previews = true;
        }
      }
    }

    if (drawn_with_previews == false) {
      node_draw_extra_info_panel(C, tree_draw_ctx, snode, node, nullptr, block);
    }
  }

  const float padding = 0.5f;
  const float corner_radius = BASIS_RAD + padding;
  /* Header. */
  {
    /* Add some padding to prevent transparent gaps with the outline. */
    const rctf rect = {
        rct.xmin - padding,
        rct.xmax + padding,
        rct.ymax - NODE_DY - padding,
        rct.ymax + padding,
    };

    float color_header[4];

    /* Muted nodes get a mix of the background with the node color. */
    if (node.is_muted()) {
      UI_GetThemeColorBlend4f(TH_BACK, color_id, 0.1f, color_header);
    }
    else {
      UI_GetThemeColorBlend4f(TH_NODE, color_id, 0.4f, color_header);
    }

    UI_draw_roundbox_corner_set(UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT);
    UI_draw_roundbox_4fv(&rect, true, corner_radius, color_header);
  }

  /* Show/hide icons. */
  float iconofs = rct.xmax - 0.35f * U.widget_unit;

  /* Group edit. This icon should be the first for the node groups. Note that we intentionally
   * don't check for NODE_GROUP_CUSTOM here. */
  if (node.type_legacy == NODE_GROUP) {
    iconofs -= iconbutw;
    UI_block_emboss_set(&block, UI_EMBOSS_NONE);
    uiBut *but = uiDefIconBut(&block,
                              UI_BTYPE_BUT_TOGGLE,
                              0,
                              ICON_NODETREE,
                              iconofs,
                              rct.ymax - NODE_DY,
                              iconbutw,
                              UI_UNIT_Y,
                              nullptr,
                              0,
                              0,
                              "");
    UI_but_func_set(but,
                    node_toggle_button_cb,
                    POINTER_FROM_INT(node.identifier),
                    (void *)"NODE_OT_group_edit");
    if (node.id) {
      UI_but_icon_indicator_number_set(but, ID_REAL_USERS(node.id));
    }
    UI_block_emboss_set(&block, UI_EMBOSS);
  }
  /* Preview. */
  if (node_is_previewable(snode, ntree, node)) {
    const bool is_active = node.flag & NODE_PREVIEW;
    iconofs -= iconbutw;
    UI_block_emboss_set(&block, UI_EMBOSS_NONE);
    uiBut *but = uiDefIconBut(&block,
                              UI_BTYPE_BUT_TOGGLE,
                              0,
                              is_active ? ICON_HIDE_OFF : ICON_HIDE_ON,
                              iconofs,
                              rct.ymax - NODE_DY,
                              iconbutw,
                              UI_UNIT_Y,
                              nullptr,
                              0,
                              0,
                              "");
    UI_but_func_set(but,
                    node_toggle_button_cb,
                    POINTER_FROM_INT(node.identifier),
                    (void *)"NODE_OT_preview_toggle");
    UI_block_emboss_set(&block, UI_EMBOSS);
  }
  if (ELEM(node.type_legacy, NODE_CUSTOM, NODE_CUSTOM_GROUP) &&
      node.typeinfo->ui_icon != ICON_NONE)
  {
    iconofs -= iconbutw;
    UI_block_emboss_set(&block, UI_EMBOSS_NONE);
    uiDefIconBut(&block,
                 UI_BTYPE_BUT,
                 0,
                 node.typeinfo->ui_icon,
                 iconofs,
                 rct.ymax - NODE_DY,
                 iconbutw,
                 UI_UNIT_Y,
                 nullptr,
                 0,
                 0,
                 "");
    UI_block_emboss_set(&block, UI_EMBOSS);
  }
  if (node.type_legacy == GEO_NODE_VIEWER) {
    const bool is_active = &node == tree_draw_ctx.active_geometry_nodes_viewer;
    iconofs -= iconbutw;
    UI_block_emboss_set(&block, UI_EMBOSS_NONE);
    uiBut *but = uiDefIconBut(&block,
                              UI_BTYPE_BUT,
                              0,
                              is_active ? ICON_HIDE_OFF : ICON_HIDE_ON,
                              iconofs,
                              rct.ymax - NODE_DY,
                              iconbutw,
                              UI_UNIT_Y,
                              nullptr,
                              0,
                              0,
                              "");
    /* Selection implicitly activates the node. */
    const char *operator_idname = is_active ? "NODE_OT_deactivate_viewer" : "NODE_OT_select";
    UI_but_func_set(
        but, node_toggle_button_cb, POINTER_FROM_INT(node.identifier), (void *)operator_idname);
    UI_block_emboss_set(&block, UI_EMBOSS);
  }
  /* Viewer node shortcuts. */
  if (node.is_type("CompositorNodeViewer")) {
    short shortcut_icon = get_viewer_shortcut_icon(node);
    iconofs -= iconbutw;
    UI_block_emboss_set(&block, UI_EMBOSS_NONE);
    uiDefIconBut(&block,
                 UI_BTYPE_BUT,
                 0,
                 shortcut_icon,
                 iconofs,
                 rct.ymax - NODE_DY,
                 iconbutw,
                 UI_UNIT_Y,
                 nullptr,
                 0,
                 0,
                 "");
    UI_block_emboss_set(&block, UI_EMBOSS);
  }

  node_add_error_message_button(tree_draw_ctx, node, block, rct, iconofs);

  /* Title. */
  if (node.flag & SELECT) {
    UI_GetThemeColor4fv(TH_SELECT, color);
  }
  else {
    UI_GetThemeColorBlendShade4fv(TH_SELECT, color_id, 0.4f, 10, color);
  }

  /* Collapse/expand icon. */
  {
    const int but_size = U.widget_unit * 0.8f;
    UI_block_emboss_set(&block, UI_EMBOSS_NONE);

    uiBut *but = uiDefIconBut(&block,
                              UI_BTYPE_BUT_TOGGLE,
                              0,
                              ICON_DOWNARROW_HLT,
                              rct.xmin + (NODE_MARGIN_X / 3),
                              rct.ymax - NODE_DY / 2.2f - but_size / 2,
                              but_size,
                              but_size,
                              nullptr,
                              0.0f,
                              0.0f,
                              "");

    UI_but_func_set(but,
                    node_toggle_button_cb,
                    POINTER_FROM_INT(node.identifier),
                    (void *)"NODE_OT_hide_toggle");
    UI_block_emboss_set(&block, UI_EMBOSS);
  }

  char showname[128];
  bke::nodeLabel(&ntree, &node, showname, sizeof(showname));

  uiBut *but = uiDefBut(&block,
                        UI_BTYPE_LABEL,
                        0,
                        showname,
                        int(rct.xmin + NODE_MARGIN_X + 0.4f),
                        int(rct.ymax - NODE_DY),
                        short(iconofs - rct.xmin - NODE_MARGIN_X),
                        NODE_DY,
                        nullptr,
                        0,
                        0,
                        TIP_(node.typeinfo->ui_description.c_str()));
  UI_but_func_tooltip_set(
      but,
      [](bContext * /*C*/, void *arg, const char *tip) -> std::string {
        const bNode &node = *static_cast<const bNode *>(arg);
        if (node.typeinfo->ui_description_fn) {
          return node.typeinfo->ui_description_fn(node);
        }
        return StringRef(tip);
      },
      const_cast<bNode *>(&node),
      nullptr);

  if (node.is_muted()) {
    UI_but_flag_enable(but, UI_BUT_INACTIVE);
  }

  /* Wire across the node when muted/disabled. */
  if (node.is_muted()) {
    node_draw_mute_line(C, v2d, snode, node);
  }

  /* Body. */
  const float outline_width = U.pixelsize;
  {
    /* Use warning color to indicate undefined types. */
    if (bke::node_type_is_undefined(&node)) {
      UI_GetThemeColorBlend4f(TH_REDALERT, TH_NODE, 0.4f, color);
    }
    /* Muted nodes get a mix of the background with the node color. */
    else if (node.is_muted()) {
      UI_GetThemeColorBlend4f(TH_BACK, TH_NODE, 0.2f, color);
    }
    else if (node.flag & NODE_CUSTOM_COLOR) {
      rgba_float_args_set(color, node.color[0], node.color[1], node.color[2], 1.0f);
    }
    else {
      UI_GetThemeColor4fv(TH_NODE, color);
    }

    /* Draw selected nodes fully opaque. */
    if (node.flag & SELECT) {
      color[3] = 1.0f;
    }

    /* Draw muted nodes slightly transparent so the wires inside are visible. */
    if (node.is_muted()) {
      color[3] -= 0.2f;
    }

    /* Add some padding to prevent transparent gaps with the outline. */
    const rctf rect = {
        rct.xmin - padding,
        rct.xmax + padding,
        rct.ymin - padding,
        rct.ymax - (NODE_DY + outline_width) + padding,
    };

    UI_draw_roundbox_corner_set(UI_CNR_BOTTOM_LEFT | UI_CNR_BOTTOM_RIGHT);
    UI_draw_roundbox_4fv(&rect, true, corner_radius, color);

    if (is_node_panels_supported(node)) {
      node_draw_panels_background(node);
    }
  }

  /* Header underline. */
  {
    float color_underline[4];

    if (node.is_muted()) {
      UI_GetThemeColorBlend4f(TH_BACK, color_id, 0.05f, color_underline);
      color_underline[3] = 1.0f;
    }
    else {
      UI_GetThemeColorBlend4f(TH_BACK, color_id, 0.2f, color_underline);
    }

    const rctf rect = {
        rct.xmin,
        rct.xmax,
        rct.ymax - (NODE_DY + outline_width),
        rct.ymax - NODE_DY,
    };

    UI_draw_roundbox_corner_set(UI_CNR_NONE);
    UI_draw_roundbox_4fv(&rect, true, 0.0f, color_underline);
  }

  /* Outline. */
  {
    const rctf rect = {
        rct.xmin - outline_width,
        rct.xmax + outline_width,
        rct.ymin - outline_width,
        rct.ymax + outline_width,
    };

    /* Color the outline according to active, selected, or undefined status. */
    float color_outline[4];

    if (node.flag & SELECT) {
      UI_GetThemeColor4fv((node.flag & NODE_ACTIVE) ? TH_ACTIVE : TH_SELECT, color_outline);
    }
    else if (bke::node_type_is_undefined(&node)) {
      UI_GetThemeColor4fv(TH_REDALERT, color_outline);
    }
    else if (const bke::bNodeZoneType *zone_type = bke::zone_type_by_node_type(node.type_legacy)) {
      UI_GetThemeColor4fv(zone_type->theme_id, color_outline);
      color_outline[3] = 1.0f;
    }
    else {
      UI_GetThemeColorBlendShade4fv(TH_BACK, TH_NODE, 0.4f, -20, color_outline);
    }

    UI_draw_roundbox_corner_set(UI_CNR_ALL);
    UI_draw_roundbox_4fv(&rect, false, BASIS_RAD + outline_width, color_outline);
  }

  /* Skip slow socket drawing if zoom is small. */
  if (draw_node_details(snode)) {
    node_draw_sockets(C, block, snode, ntree, node);
  }

  if (is_node_panels_supported(node)) {
    node_draw_panels(ntree, node, block);
  }

  UI_block_end_ex(&C,
                  tree_draw_ctx.bmain,
                  tree_draw_ctx.window,
                  tree_draw_ctx.scene,
                  tree_draw_ctx.region,
                  tree_draw_ctx.depsgraph,
                  &block);
  UI_block_draw(&C, &block);
}

static void node_draw_hidden(const bContext &C,
                             TreeDrawContext &tree_draw_ctx,
                             const View2D &v2d,
                             const SpaceNode &snode,
                             bNodeTree &ntree,
                             bNode &node,
                             uiBlock &block)
{
  const rctf &rct = node.runtime->draw_bounds;
  float centy = BLI_rctf_cent_y(&rct);
  float hiddenrad = BLI_rctf_size_y(&rct) / 2.0f;

  float scale;
  UI_view2d_scale_get(&v2d, &scale, nullptr);

  const int color_id = node_get_colorid(tree_draw_ctx, node);

  node_draw_extra_info_panel(C, tree_draw_ctx, snode, node, nullptr, block);

  /* Shadow. */
  node_draw_shadow(snode, node, hiddenrad, 1.0f);

  /* Wire across the node when muted/disabled. */
  if (node.is_muted()) {
    node_draw_mute_line(C, v2d, snode, node);
  }

  /* Body. */
  float color[4];
  {
    if (bke::node_type_is_undefined(&node)) {
      /* Use warning color to indicate undefined types. */
      UI_GetThemeColorBlend4f(TH_REDALERT, TH_NODE, 0.4f, color);
    }
    else if (node.is_muted()) {
      /* Muted nodes get a mix of the background with the node color. */
      UI_GetThemeColorBlendShade4fv(TH_BACK, color_id, 0.1f, 0, color);
    }
    else if (node.flag & NODE_CUSTOM_COLOR) {
      rgba_float_args_set(color, node.color[0], node.color[1], node.color[2], 1.0f);
    }
    else {
      UI_GetThemeColorBlend4f(TH_NODE, color_id, 0.4f, color);
    }

    /* Draw selected nodes fully opaque. */
    if (node.flag & SELECT) {
      color[3] = 1.0f;
    }

    /* Draw muted nodes slightly transparent so the wires inside are visible. */
    if (node.is_muted()) {
      color[3] -= 0.2f;
    }

    /* Add some padding to prevent transparent gaps with the outline. */
    const float padding = 0.5f;
    const rctf rect = {
        rct.xmin - padding,
        rct.xmax + padding,
        rct.ymin - padding,
        rct.ymax + padding,
    };

    UI_draw_roundbox_4fv(&rect, true, hiddenrad + padding, color);
  }

  /* Title. */
  if (node.flag & SELECT) {
    UI_GetThemeColor4fv(TH_SELECT, color);
  }
  else {
    UI_GetThemeColorBlendShade4fv(TH_SELECT, color_id, 0.4f, 10, color);
  }

  /* Collapse/expand icon. */
  {
    const int but_size = U.widget_unit * 1.0f;
    UI_block_emboss_set(&block, UI_EMBOSS_NONE);

    uiBut *but = uiDefIconBut(&block,
                              UI_BTYPE_BUT_TOGGLE,
                              0,
                              ICON_RIGHTARROW,
                              rct.xmin + (NODE_MARGIN_X / 3),
                              centy - but_size / 2,
                              but_size,
                              but_size,
                              nullptr,
                              0.0f,
                              0.0f,
                              "");

    UI_but_func_set(but,
                    node_toggle_button_cb,
                    POINTER_FROM_INT(node.identifier),
                    (void *)"NODE_OT_hide_toggle");
    UI_block_emboss_set(&block, UI_EMBOSS);
  }

  char showname[128];
  bke::nodeLabel(&ntree, &node, showname, sizeof(showname));

  uiBut *but = uiDefBut(&block,
                        UI_BTYPE_LABEL,
                        0,
                        showname,
                        round_fl_to_int(rct.xmin + NODE_MARGIN_X),
                        round_fl_to_int(centy - NODE_DY * 0.5f),
                        short(BLI_rctf_size_x(&rct) - (2 * U.widget_unit)),
                        NODE_DY,
                        nullptr,
                        0,
                        0,
                        TIP_(node.typeinfo->ui_description.c_str()));

  /* Outline. */
  {
    const float outline_width = U.pixelsize;
    const rctf rect = {
        rct.xmin - outline_width,
        rct.xmax + outline_width,
        rct.ymin - outline_width,
        rct.ymax + outline_width,
    };

    /* Color the outline according to active, selected, or undefined status. */
    float color_outline[4];

    if (node.flag & SELECT) {
      UI_GetThemeColor4fv((node.flag & NODE_ACTIVE) ? TH_ACTIVE : TH_SELECT, color_outline);
    }
    else if (bke::node_type_is_undefined(&node)) {
      UI_GetThemeColor4fv(TH_REDALERT, color_outline);
    }
    else {
      UI_GetThemeColorBlendShade4fv(TH_BACK, TH_NODE, 0.4f, -20, color_outline);
    }

    UI_draw_roundbox_corner_set(UI_CNR_ALL);
    UI_draw_roundbox_4fv(&rect, false, hiddenrad + outline_width, color_outline);
  }

  if (node.is_muted()) {
    UI_but_flag_enable(but, UI_BUT_INACTIVE);
  }

  /* Scale widget thing. */
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  GPU_blend(GPU_BLEND_ALPHA);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformThemeColorShadeAlpha(TH_TEXT, -40, -180);
  float dx = 0.5f * U.widget_unit;
  const float dx2 = 0.15f * U.widget_unit * snode.runtime->aspect;
  const float dy = 0.2f * U.widget_unit;

  immBegin(GPU_PRIM_LINES, 4);
  immVertex2f(pos, rct.xmax - dx, centy - dy);
  immVertex2f(pos, rct.xmax - dx, centy + dy);

  immVertex2f(pos, rct.xmax - dx - dx2, centy - dy);
  immVertex2f(pos, rct.xmax - dx - dx2, centy + dy);
  immEnd();

  immUniformThemeColorShadeAlpha(TH_TEXT, 0, -180);
  dx -= snode.runtime->aspect;

  immBegin(GPU_PRIM_LINES, 4);
  immVertex2f(pos, rct.xmax - dx, centy - dy);
  immVertex2f(pos, rct.xmax - dx, centy + dy);

  immVertex2f(pos, rct.xmax - dx - dx2, centy - dy);
  immVertex2f(pos, rct.xmax - dx - dx2, centy + dy);
  immEnd();

  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);

  node_draw_sockets(C, block, snode, ntree, node);

  UI_block_end_ex(&C,
                  tree_draw_ctx.bmain,
                  tree_draw_ctx.window,
                  tree_draw_ctx.scene,
                  tree_draw_ctx.region,
                  tree_draw_ctx.depsgraph,
                  &block);
  UI_block_draw(&C, &block);
}

int node_get_resize_cursor(NodeResizeDirection directions)
{
  if (directions == 0) {
    return WM_CURSOR_DEFAULT;
  }
  if ((directions & ~(NODE_RESIZE_TOP | NODE_RESIZE_BOTTOM)) == 0) {
    return WM_CURSOR_Y_MOVE;
  }
  if ((directions & ~(NODE_RESIZE_RIGHT | NODE_RESIZE_LEFT)) == 0) {
    return WM_CURSOR_X_MOVE;
  }
  return WM_CURSOR_EDIT;
}

static const bNode *find_node_under_cursor(SpaceNode &snode, const float2 &cursor)
{
  for (const bNode *node : tree_draw_order_calc_nodes_reversed(*snode.edittree)) {
    if (BLI_rctf_isect_pt(&node->runtime->draw_bounds, cursor[0], cursor[1])) {
      return node;
    }
  }
  return nullptr;
}

void node_set_cursor(wmWindow &win, ARegion &region, SpaceNode &snode, const float2 &cursor)
{
  const bNodeTree *ntree = snode.edittree;
  if (ntree == nullptr) {
    WM_cursor_set(&win, WM_CURSOR_DEFAULT);
    return;
  }
  if (node_find_indicated_socket(snode, region, cursor, SOCK_IN | SOCK_OUT)) {
    WM_cursor_set(&win, WM_CURSOR_DEFAULT);
    return;
  }
  const bNode *node = find_node_under_cursor(snode, cursor);
  if (!node) {
    WM_cursor_set(&win, WM_CURSOR_DEFAULT);
    return;
  }
  const NodeResizeDirection dir = node_get_resize_direction(snode, node, cursor[0], cursor[1]);
  if (node->is_frame() && dir == NODE_RESIZE_NONE) {
    /* Indicate that frame nodes can be moved/selected on their borders. */
    const rctf frame_inside = node_frame_rect_inside(snode, *node);
    if (!BLI_rctf_isect_pt(&frame_inside, cursor[0], cursor[1])) {
      WM_cursor_set(&win, WM_CURSOR_NSEW_SCROLL);
      return;
    }
    WM_cursor_set(&win, WM_CURSOR_DEFAULT);
    return;
  }

  WM_cursor_set(&win, node_get_resize_cursor(dir));
}

static void count_multi_input_socket_links(bNodeTree &ntree, SpaceNode &snode)
{
  for (bNode *node : ntree.all_nodes()) {
    for (bNodeSocket *socket : node->input_sockets()) {
      if (socket->is_multi_input()) {
        socket->runtime->total_inputs = socket->directly_linked_links().size();
      }
    }
  }
  /* Count temporary links going into this socket. */
  if (snode.runtime->linkdrag) {
    for (const bNodeLink &link : snode.runtime->linkdrag->links) {
      if (link.tosock && (link.tosock->flag & SOCK_MULTI_INPUT)) {
        link.tosock->runtime->total_inputs++;
      }
    }
  }
}

static float frame_node_label_height(const NodeFrame &frame_data)
{
  return frame_data.label_size * UI_SCALE_FAC;
}

#define NODE_FRAME_MARGIN (1.5f * U.widget_unit)

/**
 * Does a bounding box update by iterating over all children.
 * Not ideal to do this in every draw call, but doing as transform callback doesn't work,
 * since the frame node automatic size depends on the size of each node which is only calculated
 * while drawing.
 */
static rctf calc_node_frame_dimensions(bNode &node)
{
  if (!node.is_frame()) {
    return node.runtime->draw_bounds;
  }

  NodeFrame *data = (NodeFrame *)node.storage;

  const float margin = NODE_FRAME_MARGIN;
  const float has_label = node.label[0] != '\0';

  const float label_height = frame_node_label_height(*data);
  /* Add an additional 25% to account for the glyphs descender.
   * This works well in most cases. */
  const float margin_top = 0.5f * margin + (has_label ? 1.25f * label_height : 0.5f * margin);

  /* Initialize rect from current frame size. */
  rctf rect;
  node_to_updated_rect(node, rect);

  /* Frame can be resized manually only if shrinking is disabled or no children are attached. */
  data->flag |= NODE_FRAME_RESIZEABLE;
  /* For shrinking bounding box, initialize the rect from first child node. */
  bool bbinit = (data->flag & NODE_FRAME_SHRINK);
  /* Fit bounding box to all children. */
  for (bNode *child : node.direct_children_in_frame()) {
    /* Add margin to node rect. */
    rctf noderect = calc_node_frame_dimensions(*child);
    noderect.xmin -= margin;
    noderect.xmax += margin;
    noderect.ymin -= margin;
    noderect.ymax += margin_top;

    /* First child initializes frame. */
    if (bbinit) {
      bbinit = false;
      rect = noderect;
      data->flag &= ~NODE_FRAME_RESIZEABLE;
    }
    else {
      BLI_rctf_union(&rect, &noderect);
    }
  }

  /* Now adjust the frame size from view-space bounding box. */
  const float2 min = node_from_view({rect.xmin, rect.ymin});
  const float2 max = node_from_view({rect.xmax, rect.ymax});
  node.location[0] = min.x;
  node.location[1] = max.y;
  node.width = max.x - min.x;
  node.height = max.y - min.y;

  node.runtime->draw_bounds = rect;
  return rect;
}

static void reroute_node_prepare_for_draw(bNode &node)
{
  const float2 loc = node_to_view(node.location);

  /* When the node is hidden, the input and output socket are both in the same place. */
  node.input_socket(0).runtime->location = loc;
  node.output_socket(0).runtime->location = loc;

  const float radius = NODE_SOCKSIZE;
  node.width = radius * 2;
  node.runtime->draw_bounds.xmin = loc.x - radius;
  node.runtime->draw_bounds.xmax = loc.x + radius;
  node.runtime->draw_bounds.ymax = loc.y + radius;
  node.runtime->draw_bounds.ymin = loc.y - radius;
}

static void node_update_nodetree(const bContext &C,
                                 TreeDrawContext &tree_draw_ctx,
                                 bNodeTree &ntree,
                                 Span<bNode *> nodes,
                                 Span<uiBlock *> blocks)
{
  /* Make sure socket "used" tags are correct, for displaying value buttons. */
  SpaceNode *snode = CTX_wm_space_node(&C);

  count_multi_input_socket_links(ntree, *snode);

  for (const int i : nodes.index_range()) {
    bNode &node = *nodes[i];
    uiBlock &block = *blocks[node.index()];
    if (node.is_frame()) {
      /* Frame sizes are calculated after all other nodes have calculating their #draw_bounds. */
      continue;
    }

    if (node.is_reroute()) {
      reroute_node_prepare_for_draw(node);
    }
    else {
      if (node.flag & NODE_HIDDEN) {
        node_update_hidden(node, block);
      }
      else {
        node_update_basis(C, tree_draw_ctx, ntree, node, block);
      }
    }
  }

  /* Now calculate the size of frame nodes, which can depend on the size of other nodes. */
  for (bNode *frame : ntree.root_frames()) {
    calc_node_frame_dimensions(*frame);
  }
}

static void frame_node_draw_label(TreeDrawContext &tree_draw_ctx,
                                  const bNodeTree &ntree,
                                  const bNode &node,
                                  const SpaceNode &snode)
{
  const float aspect = snode.runtime->aspect;
  /* XXX font id is crap design */
  const int fontid = UI_style_get()->widget.uifont_id;
  const NodeFrame *data = (const NodeFrame *)node.storage;
  const float font_size = data->label_size / aspect;

  char label[MAX_NAME];
  bke::nodeLabel(&ntree, &node, label, sizeof(label));

  BLF_enable(fontid, BLF_ASPECT);
  BLF_aspect(fontid, aspect, aspect, 1.0f);
  BLF_size(fontid, font_size * UI_SCALE_FAC);

  /* Title color. */
  int color_id = node_get_colorid(tree_draw_ctx, node);
  uchar color[3];
  UI_GetThemeColorBlendShade3ubv(TH_TEXT, color_id, 0.4f, 10, color);
  BLF_color3ubv(fontid, color);

  const float margin = NODE_FRAME_MARGIN;
  const float width = BLF_width(fontid, label, sizeof(label));
  const int label_height = frame_node_label_height(*data);

  const rctf &rct = node.runtime->draw_bounds;
  const float label_x = BLI_rctf_cent_x(&rct) - (0.5f * width);
  const float label_y = rct.ymax - label_height - (0.5f * margin);

  /* Label. */
  const bool has_label = node.label[0] != '\0';
  if (has_label) {
    BLF_position(fontid, label_x, label_y, 0);
    BLF_draw(fontid, label, sizeof(label));
  }

  /* Draw text body. */
  if (node.id) {
    const Text *text = (const Text *)node.id;
    const int line_height_max = BLF_height_max(fontid);
    const float line_spacing = (line_height_max * aspect);
    const float line_width = (BLI_rctf_size_x(&rct) - 2 * margin) / aspect;

    const float x = rct.xmin + margin;
    float y = rct.ymax - label_height - (has_label ? line_spacing + margin : 0);

    const int y_min = rct.ymin + margin;

    BLF_enable(fontid, BLF_CLIPPING | BLF_WORD_WRAP);
    BLF_clipping(fontid, rct.xmin, rct.ymin + margin, rct.xmax, rct.ymax);

    BLF_wordwrap(fontid, line_width);

    LISTBASE_FOREACH (const TextLine *, line, &text->lines) {
      if (line->line[0]) {
        BLF_position(fontid, x, y, 0);
        ResultBLF info;
        BLF_draw(fontid, line->line, line->len, &info);
        y -= line_spacing * info.lines;
      }
      else {
        y -= line_spacing;
      }
      if (y < y_min) {
        break;
      }
    }

    BLF_disable(fontid, BLF_CLIPPING | BLF_WORD_WRAP);
  }

  BLF_disable(fontid, BLF_ASPECT);
}

static void frame_node_draw_background(const ARegion &region,
                                       const SpaceNode &snode,
                                       const bNode &node)
{
  /* Skip if out of view. */
  if (BLI_rctf_isect(&node.runtime->draw_bounds, &region.v2d.cur, nullptr) == false) {
    return;
  }

  float color[4];
  UI_GetThemeColor4fv(TH_NODE_FRAME, color);
  const float alpha = color[3];

  node_draw_shadow(snode, node, BASIS_RAD, alpha);

  if (node.flag & NODE_CUSTOM_COLOR) {
    rgba_float_args_set(color, node.color[0], node.color[1], node.color[2], alpha);
  }
  else {
    UI_GetThemeColor4fv(TH_NODE_FRAME, color);
  }

  const rctf &rct = node.runtime->draw_bounds;
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_4fv(&rct, true, BASIS_RAD, color);

  /* Outline active and selected emphasis. */
  if (node.flag & SELECT) {
    if (node.flag & NODE_ACTIVE) {
      UI_GetThemeColorShadeAlpha4fv(TH_ACTIVE, 0, -40, color);
    }
    else {
      UI_GetThemeColorShadeAlpha4fv(TH_SELECT, 0, -40, color);
    }

    UI_draw_roundbox_aa(&rct, false, BASIS_RAD, color);
  }
}

static void frame_node_draw_overlay(const bContext &C,
                                    TreeDrawContext &tree_draw_ctx,
                                    const ARegion &region,
                                    const SpaceNode &snode,
                                    const bNodeTree &ntree,
                                    const bNode &node,
                                    uiBlock &block)
{
  /* Skip if out of view. */
  if (BLI_rctf_isect(&node.runtime->draw_bounds, &region.v2d.cur, nullptr) == false) {
    UI_block_end_ex(&C,
                    tree_draw_ctx.bmain,
                    tree_draw_ctx.window,
                    tree_draw_ctx.scene,
                    tree_draw_ctx.region,
                    tree_draw_ctx.depsgraph,
                    &block);
    return;
  }

  /* Label and text. */
  frame_node_draw_label(tree_draw_ctx, ntree, node, snode);

  node_draw_extra_info_panel(C, tree_draw_ctx, snode, node, nullptr, block);

  UI_block_end_ex(&C,
                  tree_draw_ctx.bmain,
                  tree_draw_ctx.window,
                  tree_draw_ctx.scene,
                  tree_draw_ctx.region,
                  tree_draw_ctx.depsgraph,
                  &block);
  UI_block_draw(&C, &block);
}

static Set<const bNodeSocket *> find_sockets_on_active_gizmo_paths(const bContext &C,
                                                                   const SpaceNode &snode)
{
  const std::optional<ed::space_node::ObjectAndModifier> object_and_modifier =
      ed::space_node::get_modifier_for_node_editor(snode);
  if (!object_and_modifier) {
    return {};
  }
  snode.edittree->ensure_topology_cache();

  /* Compute the compute context hash for the current node tree path. */
  std::optional<ComputeContextHash> current_compute_context_hash =
      [&]() -> std::optional<ComputeContextHash> {
    ComputeContextBuilder compute_context_builder;
    compute_context_builder.push<bke::ModifierComputeContext>(
        object_and_modifier->nmd->modifier.name);
    if (!ed::space_node::push_compute_context_for_tree_path(snode, compute_context_builder)) {
      return std::nullopt;
    }
    return compute_context_builder.current()->hash();
  }();
  if (!current_compute_context_hash) {
    return {};
  }

  Set<const bNodeSocket *> sockets_on_gizmo_paths;

  ComputeContextBuilder compute_context_builder;
  nodes::gizmos::foreach_active_gizmo(
      C,
      compute_context_builder,
      [&](const Object &gizmo_object,
          const NodesModifierData &gizmo_nmd,
          const ComputeContext &gizmo_context,
          const bNode &gizmo_node,
          const bNodeSocket &gizmo_socket) {
        if (&gizmo_object != object_and_modifier->object) {
          return;
        }
        if (&gizmo_nmd != object_and_modifier->nmd) {
          return;
        }
        nodes::gizmos::foreach_socket_on_gizmo_path(
            gizmo_context,
            gizmo_node,
            gizmo_socket,
            [&](const ComputeContext &compute_context,
                const bNodeSocket &socket,
                const nodes::inverse_eval::ElemVariant & /*elem*/) {
              if (compute_context.hash() == *current_compute_context_hash) {
                sockets_on_gizmo_paths.add(&socket);
              }
            });
      });

  return sockets_on_gizmo_paths;
}

/**
 * Returns the reroute node linked to the input of the given reroute, if there is one.
 */
static const bNode *reroute_node_get_linked_reroute(const bNode &reroute)
{
  BLI_assert(reroute.is_reroute());

  const bNodeSocket *input_socket = reroute.input_sockets().first();
  if (input_socket->directly_linked_links().is_empty()) {
    return nullptr;
  }
  const bNodeLink *input_link = input_socket->directly_linked_links().first();
  const bNode *from_node = input_link->fromnode;
  return from_node->is_reroute() ? from_node : nullptr;
}

/**
 * The auto label overlay displays a label on reroute nodes based on the user-defined label of a
 * linked reroute upstream.
 */
static StringRef reroute_node_get_auto_label(TreeDrawContext &tree_draw_ctx,
                                             const bNode &src_reroute)
{
  BLI_assert(src_reroute.is_reroute());

  if (src_reroute.label[0] != '\0') {
    return src_reroute.label;
  }

  Map<const bNode *, StringRef> &reroute_auto_labels = tree_draw_ctx.reroute_auto_labels;

  StringRef label;
  Vector<const bNode *> reroute_path;

  /* Traverse reroute path backwards until label, non-reroute node or link-cycle is found. */
  for (const bNode *reroute = &src_reroute; reroute;
       reroute = reroute_node_get_linked_reroute(*reroute))
  {
    reroute_path.append(reroute);
    if (const StringRef *label_ptr = reroute_auto_labels.lookup_ptr(reroute)) {
      label = *label_ptr;
      break;
    }
    if (reroute->label[0] != '\0') {
      label = reroute->label;
      break;
    }
    /* This makes sure that the loop eventually ends even if there are link-cycles. */
    reroute_auto_labels.add(reroute, "");
  }

  /* Remember the label for each node on the path to avoid recomputing it. */
  for (const bNode *reroute : reroute_path) {
    reroute_auto_labels.add_overwrite(reroute, label);
  }

  return label;
}

static void reroute_node_draw_body(const bContext &C,
                                   const SpaceNode &snode,
                                   const bNodeTree &ntree,
                                   const bNode &node,
                                   uiBlock &block,
                                   const bool selected)
{
  BLI_assert(node.is_reroute());

  bNodeSocket &sock = *static_cast<bNodeSocket *>(node.inputs.first);

  PointerRNA nodeptr = RNA_pointer_create_discrete(
      const_cast<ID *>(&ntree.id), &RNA_Node, const_cast<bNode *>(&node));

  ColorTheme4f socket_color;
  ColorTheme4f outline_color;

  node_socket_color_get(C, ntree, nodeptr, sock, socket_color);
  node_socket_outline_color_get(selected, sock.type, outline_color);

  node_draw_nodesocket(&node.runtime->draw_bounds,
                       socket_color,
                       outline_color,
                       NODE_SOCKET_OUTLINE,
                       sock.display_shape,
                       snode.runtime->aspect);

  const float2 location = float2(BLI_rctf_cent_x(&node.runtime->draw_bounds),
                                 BLI_rctf_cent_y(&node.runtime->draw_bounds));
  const float2 size = float2(BLI_rctf_size_x(&node.runtime->draw_bounds),
                             BLI_rctf_size_y(&node.runtime->draw_bounds));
  node_socket_tooltip_set(block, sock.index_in_tree(), location, size);
}

static void reroute_node_draw_label(TreeDrawContext &tree_draw_ctx,
                                    const SpaceNode &snode,
                                    const bNode &node,
                                    uiBlock &block)
{
  const bool has_label = node.label[0] != '\0';
  const bool use_auto_label = !has_label && (snode.overlay.flag & SN_OVERLAY_SHOW_OVERLAYS) &&
                              (snode.overlay.flag & SN_OVERLAY_SHOW_REROUTE_AUTO_LABELS);

  if (!has_label && !use_auto_label) {
    return;
  }

  /* Don't show the automatic label, when being zoomed out. */
  if (!has_label && !draw_node_details(snode)) {
    return;
  }

  const StringRef text = has_label ? node.label : reroute_node_get_auto_label(tree_draw_ctx, node);
  if (text.is_empty()) {
    return;
  }

  const short width = 512;
  const int x = BLI_rctf_cent_x(&node.runtime->draw_bounds) - (width / 2);
  const int y = node.runtime->draw_bounds.ymax;

  uiBut *label_but = uiDefBut(
      &block, UI_BTYPE_LABEL, 0, text, x, y, width, NODE_DY, nullptr, 0, 0, nullptr);

  UI_but_drawflag_disable(label_but, UI_BUT_TEXT_LEFT);

  if (use_auto_label && !(node.flag & NODE_SELECT)) {
    UI_but_flag_enable(label_but, UI_BUT_INACTIVE);
  }
}

static void reroute_node_draw(const bContext &C,
                              TreeDrawContext &tree_draw_ctx,
                              ARegion &region,
                              const SpaceNode &snode,
                              bNodeTree &ntree,
                              const bNode &node,
                              uiBlock &block)
{
  const rctf &rct = node.runtime->draw_bounds;
  const View2D &v2d = region.v2d;

  /* Skip if out of view. */
  if (rct.xmax < v2d.cur.xmin || rct.xmin > v2d.cur.xmax || rct.ymax < v2d.cur.ymin ||
      node.runtime->draw_bounds.ymin > v2d.cur.ymax)
  {
    UI_block_end_ex(&C,
                    tree_draw_ctx.bmain,
                    tree_draw_ctx.window,
                    tree_draw_ctx.scene,
                    tree_draw_ctx.region,
                    tree_draw_ctx.depsgraph,
                    &block);
    return;
  }

  if (draw_node_details(snode)) {
    reroute_node_draw_label(tree_draw_ctx, snode, node, block);
  }

  /* Only draw the input socket, since all sockets are at the same location. */
  const bool selected = node.flag & NODE_SELECT;
  reroute_node_draw_body(C, snode, ntree, node, block, selected);

  UI_block_end_ex(&C,
                  tree_draw_ctx.bmain,
                  tree_draw_ctx.window,
                  tree_draw_ctx.scene,
                  tree_draw_ctx.region,
                  tree_draw_ctx.depsgraph,
                  &block);
  UI_block_draw(&C, &block);
}

static void node_draw(const bContext &C,
                      TreeDrawContext &tree_draw_ctx,
                      ARegion &region,
                      const SpaceNode &snode,
                      bNodeTree &ntree,
                      bNode &node,
                      uiBlock &block,
                      bNodeInstanceKey key)
{
  if (node.is_frame()) {
    /* Should have been drawn before already. */
    BLI_assert_unreachable();
  }
  else if (node.is_reroute()) {
    reroute_node_draw(C, tree_draw_ctx, region, snode, ntree, node, block);
  }
  else {
    const View2D &v2d = region.v2d;
    if (node.flag & NODE_HIDDEN) {
      node_draw_hidden(C, tree_draw_ctx, v2d, snode, ntree, node, block);
    }
    else {
      node_draw_basis(C, tree_draw_ctx, v2d, snode, ntree, node, block, key);
    }
  }
}

static void add_rect_corner_positions(Vector<float2> &positions, const rctf &rect)
{
  positions.append({rect.xmin, rect.ymin});
  positions.append({rect.xmin, rect.ymax});
  positions.append({rect.xmax, rect.ymin});
  positions.append({rect.xmax, rect.ymax});
}

static void find_bounds_by_zone_recursive(const SpaceNode &snode,
                                          const bNodeTreeZone &zone,
                                          const Span<const bNodeTreeZone *> all_zones,
                                          MutableSpan<Vector<float2>> r_bounds_by_zone)
{
  const float node_padding = UI_UNIT_X;
  const float zone_padding = 0.3f * UI_UNIT_X;

  Vector<float2> &bounds = r_bounds_by_zone[zone.index];
  if (!bounds.is_empty()) {
    return;
  }

  Vector<float2> possible_bounds;
  for (const bNodeTreeZone *child_zone : zone.child_zones) {
    find_bounds_by_zone_recursive(snode, *child_zone, all_zones, r_bounds_by_zone);
    const Span<float2> child_bounds = r_bounds_by_zone[child_zone->index];
    for (const float2 &pos : child_bounds) {
      rctf rect;
      BLI_rctf_init_pt_radius(&rect, pos, zone_padding);
      add_rect_corner_positions(possible_bounds, rect);
    }
  }
  for (const bNode *child_node : zone.child_nodes) {
    rctf rect = child_node->runtime->draw_bounds;
    BLI_rctf_pad(&rect, node_padding, node_padding);
    add_rect_corner_positions(possible_bounds, rect);
  }
  if (zone.input_node) {
    const rctf &draw_bounds = zone.input_node->runtime->draw_bounds;
    rctf rect = draw_bounds;
    BLI_rctf_pad(&rect, node_padding, node_padding);
    rect.xmin = math::interpolate(draw_bounds.xmin, draw_bounds.xmax, 0.25f);
    add_rect_corner_positions(possible_bounds, rect);
  }
  if (zone.output_node) {
    const rctf &draw_bounds = zone.output_node->runtime->draw_bounds;
    rctf rect = draw_bounds;
    BLI_rctf_pad(&rect, node_padding, node_padding);
    rect.xmax = math::interpolate(draw_bounds.xmin, draw_bounds.xmax, 0.75f);
    add_rect_corner_positions(possible_bounds, rect);
  }

  if (snode.runtime->linkdrag) {
    for (const bNodeLink &link : snode.runtime->linkdrag->links) {
      if (link.fromnode == nullptr) {
        continue;
      }
      if (zone.contains_node_recursively(*link.fromnode) && zone.output_node != link.fromnode) {
        const float2 pos = node_link_bezier_points_dragged(snode, link)[3];
        rctf rect;
        BLI_rctf_init_pt_radius(&rect, pos, node_padding);
        add_rect_corner_positions(possible_bounds, rect);
      }
    }
  }

  Vector<int> convex_indices(possible_bounds.size());
  const int convex_positions_num = BLI_convexhull_2d(
      reinterpret_cast<float(*)[2]>(possible_bounds.data()),
      possible_bounds.size(),
      convex_indices.data());
  convex_indices.resize(convex_positions_num);

  for (const int i : convex_indices) {
    bounds.append(possible_bounds[i]);
  }
}

static void node_draw_zones_and_frames(const bContext &C,
                                       TreeDrawContext &tree_draw_ctx,
                                       const ARegion &region,
                                       const SpaceNode &snode,
                                       const bNodeTree &ntree,
                                       Span<uiBlock *> blocks)
{
  const bNodeTreeZones *zones = ntree.zones();
  const int zones_num = zones ? zones->zones.size() : 0;

  Array<Vector<float2>> bounds_by_zone(zones_num);
  Array<bke::CurvesGeometry> fillet_curve_by_zone(zones_num);
  /* Bounding box area of zones is used to determine draw order. */
  Array<float> bounding_box_width_by_zone(zones_num);

  for (const int zone_i : IndexRange(zones_num)) {
    const bNodeTreeZone &zone = *zones->zones[zone_i];

    find_bounds_by_zone_recursive(snode, zone, zones->zones, bounds_by_zone);
    const Span<float2> boundary_positions = bounds_by_zone[zone_i];
    const int boundary_positions_num = boundary_positions.size();

    const Bounds<float2> bounding_box = *bounds::min_max(boundary_positions);
    const float bounding_box_width = bounding_box.max.x - bounding_box.min.x;
    bounding_box_width_by_zone[zone_i] = bounding_box_width;

    bke::CurvesGeometry boundary_curve(boundary_positions_num, 1);
    boundary_curve.cyclic_for_write().first() = true;
    boundary_curve.fill_curve_types(CURVE_TYPE_POLY);
    MutableSpan<float3> boundary_curve_positions = boundary_curve.positions_for_write();
    boundary_curve.offsets_for_write().copy_from({0, boundary_positions_num});
    for (const int i : boundary_positions.index_range()) {
      boundary_curve_positions[i] = float3(boundary_positions[i], 0.0f);
    }

    fillet_curve_by_zone[zone_i] = geometry::fillet_curves_poly(
        boundary_curve,
        IndexRange(1),
        VArray<float>::ForSingle(BASIS_RAD, boundary_positions_num),
        VArray<int>::ForSingle(5, boundary_positions_num),
        true,
        {});
  }

  const View2D &v2d = region.v2d;
  float scale;
  UI_view2d_scale_get(&v2d, &scale, nullptr);
  float line_width = 1.0f * scale;
  float viewport[4] = {};
  GPU_viewport_size_get_f(viewport);

  const auto get_theme_id = [&](const int zone_i) {
    const bNode *node = zones->zones[zone_i]->output_node;
    return bke::zone_type_by_node_type(node->type_legacy)->theme_id;
  };

  const uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  using ZoneOrNode = std::variant<const bNodeTreeZone *, const bNode *>;
  Vector<ZoneOrNode> draw_order;
  for (const int zone_i : IndexRange(zones_num)) {
    draw_order.append(zones->zones[zone_i]);
  }
  for (const bNode *node : ntree.all_nodes()) {
    if (node->flag & NODE_BACKGROUND) {
      draw_order.append(node);
    }
  }
  auto get_zone_or_node_width = [&](const ZoneOrNode &zone_or_node) {
    if (const bNodeTreeZone *const *zone_p = std::get_if<const bNodeTreeZone *>(&zone_or_node)) {
      const bNodeTreeZone &zone = **zone_p;
      return bounding_box_width_by_zone[zone.index];
    }
    if (const bNode *const *node_p = std::get_if<const bNode *>(&zone_or_node)) {
      const bNode &node = **node_p;
      return BLI_rctf_size_x(&node.runtime->draw_bounds);
    }
    BLI_assert_unreachable();
    return 0.0f;
  };
  std::sort(draw_order.begin(), draw_order.end(), [&](const ZoneOrNode &a, const ZoneOrNode &b) {
    /* Draw zones with smaller bounding box on top to make them visible. */
    return get_zone_or_node_width(a) > get_zone_or_node_width(b);
  });

  for (const ZoneOrNode &zone_or_node : draw_order) {
    if (const bNodeTreeZone *const *zone_p = std::get_if<const bNodeTreeZone *>(&zone_or_node)) {
      const bNodeTreeZone &zone = **zone_p;
      const int zone_i = zone.index;
      float zone_color[4];
      UI_GetThemeColor4fv(get_theme_id(zone_i), zone_color);
      if (zone_color[3] == 0.0f) {
        continue;
      }
      const Span<float3> fillet_boundary_positions = fillet_curve_by_zone[zone_i].positions();
      /* Draw the background. */
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      immUniformThemeColorBlend(TH_BACK, get_theme_id(zone_i), zone_color[3]);

      immBegin(GPU_PRIM_TRI_FAN, fillet_boundary_positions.size() + 1);
      for (const float3 &p : fillet_boundary_positions) {
        immVertex3fv(pos, p);
      }
      immVertex3fv(pos, fillet_boundary_positions[0]);
      immEnd();

      immUnbindProgram();
    }
    if (const bNode *const *node_p = std::get_if<const bNode *>(&zone_or_node)) {
      const bNode &node = **node_p;
      frame_node_draw_background(region, snode, node);
    }
  }

  GPU_blend(GPU_BLEND_ALPHA);

  /* Draw all the contour lines after to prevent them from getting hidden by overlapping zones. */
  for (const ZoneOrNode &zone_or_node : draw_order) {
    const bNodeTreeZone *const *zone_p = std::get_if<const bNodeTreeZone *>(&zone_or_node);
    if (!zone_p) {
      continue;
    }
    const bNodeTreeZone &zone = **zone_p;
    const int zone_i = zone.index;
    const Span<float3> fillet_boundary_positions = fillet_curve_by_zone[zone_i].positions();
    /* Draw the contour lines. */
    immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);

    immUniform2fv("viewportSize", &viewport[2]);
    immUniform1f("lineWidth", line_width * U.pixelsize);

    immUniformThemeColorAlpha(get_theme_id(zone_i), 1.0f);
    immBegin(GPU_PRIM_LINE_STRIP, fillet_boundary_positions.size() + 1);
    for (const float3 &p : fillet_boundary_positions) {
      immVertex3fv(pos, p);
    }
    immVertex3fv(pos, fillet_boundary_positions[0]);
    immEnd();

    immUnbindProgram();
  }

  GPU_blend(GPU_BLEND_NONE);

  /* Draw text on frame nodes. */
  for (const ZoneOrNode &zone_or_node : draw_order) {
    if (const bNode *const *node_p = std::get_if<const bNode *>(&zone_or_node)) {
      const bNode &node = **node_p;
      frame_node_draw_overlay(C, tree_draw_ctx, region, snode, ntree, node, *blocks[node.index()]);
    }
  }
}

#define USE_DRAW_TOT_UPDATE

static void node_draw_nodetree(const bContext &C,
                               TreeDrawContext &tree_draw_ctx,
                               ARegion &region,
                               SpaceNode &snode,
                               bNodeTree &ntree,
                               Span<bNode *> nodes,
                               Span<uiBlock *> blocks,
                               bNodeInstanceKey parent_key)
{
#ifdef USE_DRAW_TOT_UPDATE
  BLI_rctf_init_minmax(&region.v2d.tot);
#endif

  for (const int i : nodes.index_range()) {
#ifdef USE_DRAW_TOT_UPDATE
    /* Unrelated to background nodes, update the v2d->tot,
     * can be anywhere before we draw the scroll bars. */
    BLI_rctf_union(&region.v2d.tot, &nodes[i]->runtime->draw_bounds);
#endif
  }

  /* Node lines. */
  GPU_blend(GPU_BLEND_ALPHA);
  nodelink_batch_start(snode);

  for (const bNodeLink *link : ntree.all_links()) {
    if (!bke::node_link_is_hidden(link) && !bke::node_link_is_selected(link)) {
      node_draw_link(C, region.v2d, snode, *link, false);
    }
  }

  /* Draw selected node links after the unselected ones, so they are shown on top. */
  for (const bNodeLink *link : ntree.all_links()) {
    if (!bke::node_link_is_hidden(link) && bke::node_link_is_selected(link)) {
      node_draw_link(C, region.v2d, snode, *link, true);
    }
  }

  nodelink_batch_end(snode);
  GPU_blend(GPU_BLEND_NONE);

  /* Draw foreground nodes, last nodes in front. */
  for (const int i : nodes.index_range()) {
    bNode &node = *nodes[i];
    if (node.flag & NODE_BACKGROUND) {
      /* Background nodes are drawn before mixed with zones already. */
      continue;
    }

    const bNodeInstanceKey key = bke::node_instance_key(parent_key, &ntree, &node);
    node_draw(C, tree_draw_ctx, region, snode, ntree, node, *blocks[node.index()], key);
  }
}

/* Draw the breadcrumb on the top of the editor. */
static void draw_tree_path(const bContext &C, ARegion &region)
{
  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(&region);

  const rcti *rect = ED_region_visible_rect(&region);

  const uiStyle *style = UI_style_get_dpi();
  const float padding_x = 16 * UI_SCALE_FAC;
  const int x = rect->xmin + padding_x;
  const int y = region.winy - UI_UNIT_Y * 0.6f;
  const int width = BLI_rcti_size_x(rect) - 2 * padding_x;

  uiBlock *block = UI_block_begin(&C, &region, __func__, UI_EMBOSS_NONE);
  uiLayout *layout = UI_block_layout(
      block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, x, y, width, 1, 0, style);

  const Vector<ui::ContextPathItem> context_path = ed::space_node::context_path_for_space_node(C);
  ui::template_breadcrumbs(*layout, context_path);

  UI_block_layout_resolve(block, nullptr, nullptr);
  UI_block_end(&C, block);
  UI_block_draw(&C, block);

  GPU_matrix_pop_projection();
}

static void snode_setup_v2d(SpaceNode &snode, ARegion &region, const float2 &center)
{
  View2D &v2d = region.v2d;

  /* Shift view to node tree center. */
  UI_view2d_center_set(&v2d, center[0], center[1]);
  UI_view2d_view_ortho(&v2d);

  snode.runtime->aspect = BLI_rctf_size_x(&v2d.cur) / float(region.winx);
}

/* Similar to DRW_is_viewport_compositor_enabled() in `draw_manager.cc` but checks all 3D views. */
static bool compositor_is_in_use(const bContext &context)
{
  const Scene *scene = CTX_data_scene(&context);
  if (!scene->use_nodes) {
    return false;
  }

  if (!scene->nodetree) {
    return false;
  }

  wmWindowManager *wm = CTX_wm_manager(&context);
  LISTBASE_FOREACH (const wmWindow *, win, &wm->windows) {
    const bScreen *screen = WM_window_get_active_screen(win);
    LISTBASE_FOREACH (const ScrArea *, area, &screen->areabase) {
      const SpaceLink &space = *static_cast<const SpaceLink *>(area->spacedata.first);
      if (space.spacetype == SPACE_VIEW3D) {
        const View3D &view_3d = reinterpret_cast<const View3D &>(space);

        if (view_3d.shading.use_compositor == V3D_SHADING_USE_COMPOSITOR_DISABLED) {
          continue;
        }

        if (!(view_3d.shading.type >= OB_MATERIAL)) {
          continue;
        }

        return true;
      }
    }
  }

  return false;
}

static void draw_nodetree(const bContext &C,
                          ARegion &region,
                          bNodeTree &ntree,
                          bNodeInstanceKey parent_key)
{
  SpaceNode *snode = CTX_wm_space_node(&C);
  ntree.ensure_topology_cache();

  Array<bNode *> nodes = tree_draw_order_calc_nodes(ntree);

  Array<uiBlock *> blocks = node_uiblocks_init(C, nodes);

  TreeDrawContext tree_draw_ctx;
  tree_draw_ctx.bmain = CTX_data_main(&C);
  tree_draw_ctx.window = CTX_wm_window(&C);
  tree_draw_ctx.scene = CTX_data_scene(&C);
  tree_draw_ctx.region = CTX_wm_region(&C);
  tree_draw_ctx.depsgraph = CTX_data_depsgraph_pointer(&C);

  BLI_SCOPED_DEFER([&]() { ntree.runtime->sockets_on_active_gizmo_paths.clear(); });
  if (ntree.type == NTREE_GEOMETRY) {
    tree_draw_ctx.geo_log_by_zone = geo_log::GeoModifierLog::get_tree_log_by_zone_for_node_editor(
        *snode);
    for (geo_log::GeoTreeLog *log : tree_draw_ctx.geo_log_by_zone.values()) {
      log->ensure_node_warnings(&ntree);
      log->ensure_execution_times();
    }
    const WorkSpace *workspace = CTX_wm_workspace(&C);
    tree_draw_ctx.active_geometry_nodes_viewer = viewer_path::find_geometry_nodes_viewer(
        workspace->viewer_path, *snode);

    /* This set of socket is used when drawing links to determine which links should use the
     * special gizmo drawing. */
    ntree.runtime->sockets_on_active_gizmo_paths = find_sockets_on_active_gizmo_paths(C, *snode);
  }
  else if (ntree.type == NTREE_COMPOSIT) {
    const Scene *scene = CTX_data_scene(&C);
    tree_draw_ctx.used_by_compositor = compositor_is_in_use(C);
    tree_draw_ctx.compositor_per_node_execution_time =
        &scene->runtime->compositor.per_node_execution_time;
  }
  else if (ntree.type == NTREE_SHADER && USER_EXPERIMENTAL_TEST(&U, use_shader_node_previews) &&
           BKE_scene_uses_shader_previews(CTX_data_scene(&C)) &&
           snode->overlay.flag & SN_OVERLAY_SHOW_OVERLAYS &&
           snode->overlay.flag & SN_OVERLAY_SHOW_PREVIEWS)
  {
    tree_draw_ctx.nested_group_infos = get_nested_previews(C, *snode);
  }

  node_update_nodetree(C, tree_draw_ctx, ntree, nodes, blocks);
  node_draw_zones_and_frames(C, tree_draw_ctx, region, *snode, ntree, blocks);
  node_draw_nodetree(C, tree_draw_ctx, region, *snode, ntree, nodes, blocks, parent_key);
}

/**
 * Make the background slightly brighter to indicate that users are inside a node-group.
 */
static void draw_background_color(const SpaceNode &snode)
{
  const int max_tree_length = 3;
  const float bright_factor = 0.25f;

  /* We ignore the first element of the path since it is the top-most tree and it doesn't need to
   * be brighter. We also set a cap to how many levels we want to set apart, to avoid the
   * background from getting too bright. */
  const int clamped_tree_path_length = BLI_listbase_count_at_most(&snode.treepath,
                                                                  max_tree_length);
  const int depth = max_ii(0, clamped_tree_path_length - 1);

  float color[3];
  UI_GetThemeColor3fv(TH_BACK, color);
  mul_v3_fl(color, 1.0f + bright_factor * depth);
  GPU_clear_color(color[0], color[1], color[2], 1.0);
}

void node_draw_space(const bContext &C, ARegion &region)
{
  wmWindow *win = CTX_wm_window(&C);
  SpaceNode &snode = *CTX_wm_space_node(&C);
  View2D &v2d = region.v2d;

  /* Setup off-screen buffers. */
  GPUViewport *viewport = WM_draw_region_get_viewport(&region);

  GPUFrameBuffer *framebuffer_overlay = GPU_viewport_framebuffer_overlay_get(viewport);
  GPU_framebuffer_bind_no_srgb(framebuffer_overlay);

  UI_view2d_view_ortho(&v2d);
  draw_background_color(snode);
  GPU_depth_test(GPU_DEPTH_NONE);
  GPU_scissor_test(true);

  /* XXX `snode->runtime->cursor` set in coordinate-space for placing new nodes,
   * used for drawing noodles too. */
  UI_view2d_region_to_view(&region.v2d,
                           win->eventstate->xy[0] - region.winrct.xmin,
                           win->eventstate->xy[1] - region.winrct.ymin,
                           &snode.runtime->cursor[0],
                           &snode.runtime->cursor[1]);
  snode.runtime->cursor[0] /= UI_SCALE_FAC;
  snode.runtime->cursor[1] /= UI_SCALE_FAC;

  ED_region_draw_cb_draw(&C, &region, REGION_DRAW_PRE_VIEW);

  /* Only set once. */
  GPU_blend(GPU_BLEND_ALPHA);

  /* Nodes. */
  snode_set_context(C);

  const int grid_levels = UI_GetThemeValueType(TH_NODE_GRID_LEVELS, SPACE_NODE);
  UI_view2d_dot_grid_draw(&v2d, TH_GRID, NODE_GRID_STEP_SIZE, grid_levels);

  /* Draw parent node trees. */
  if (snode.treepath.last) {
    bNodeTreePath *path = (bNodeTreePath *)snode.treepath.last;

    /* Update tree path name (drawn in the bottom left). */
    ID *name_id = (path->nodetree && path->nodetree != snode.nodetree) ? &path->nodetree->id :
                                                                         snode.id;

    if (name_id && UNLIKELY(!STREQ(path->display_name, name_id->name + 2))) {
      STRNCPY(path->display_name, name_id->name + 2);
    }

    /* Current View2D center, will be set temporarily for parent node trees. */
    float2 center;
    UI_view2d_center_get(&v2d, &center.x, &center.y);

    /* Store new view center in path and current edit tree. */
    copy_v2_v2(path->view_center, center);
    if (snode.edittree) {
      copy_v2_v2(snode.edittree->view_center, center);
    }

    /* Top-level edit tree. */
    bNodeTree *ntree = path->nodetree;
    if (ntree) {
      snode_setup_v2d(snode, region, center);

      /* Backdrop. */
      draw_nodespace_back_pix(C, region, snode, path->parent_key);

      {
        float original_proj[4][4];
        GPU_matrix_projection_get(original_proj);

        GPU_matrix_push();
        GPU_matrix_identity_set();

        wmOrtho2_pixelspace(region.winx, region.winy);

        WM_gizmomap_draw(region.runtime->gizmo_map, &C, WM_GIZMOMAP_DRAWSTEP_2D);

        GPU_matrix_pop();
        GPU_matrix_projection_set(original_proj);
      }

      draw_nodetree(C, region, *ntree, path->parent_key);
    }

    /* Temporary links. */
    GPU_blend(GPU_BLEND_ALPHA);
    GPU_line_smooth(true);
    if (snode.runtime->linkdrag) {
      for (const bNodeLink &link : snode.runtime->linkdrag->links) {
        node_draw_link_dragged(C, v2d, snode, link);
      }
    }
    GPU_line_smooth(false);
    GPU_blend(GPU_BLEND_NONE);

    if (snode.overlay.flag & SN_OVERLAY_SHOW_OVERLAYS && snode.flag & SNODE_SHOW_GPENCIL) {
      /* Draw grease-pencil annotations. */
      ED_annotation_draw_view2d(&C, true);
    }
  }
  else {

    /* Backdrop. */
    draw_nodespace_back_pix(C, region, snode, bke::NODE_INSTANCE_KEY_NONE);
  }

  ED_region_draw_cb_draw(&C, &region, REGION_DRAW_POST_VIEW);

  /* Reset view matrix. */
  UI_view2d_view_restore(&C);

  if (snode.overlay.flag & SN_OVERLAY_SHOW_OVERLAYS) {
    if (snode.flag & SNODE_SHOW_GPENCIL && snode.treepath.last) {
      /* Draw grease-pencil (screen strokes, and also paint-buffer). */
      ED_annotation_draw_view2d(&C, false);
    }

    /* Draw context path. */
    if (snode.overlay.flag & SN_OVERLAY_SHOW_PATH && snode.edittree) {
      draw_tree_path(C, region);
    }
  }

  /* Scrollers. */
  UI_view2d_scrollers_draw(&v2d, nullptr);
}

}  // namespace blender::ed::space_node
