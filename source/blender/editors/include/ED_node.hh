/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_compute_context.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector_set.hh"

#include "BKE_compute_context_cache_fwd.hh"

#include "NOD_geometry_nodes_bundle_signature.hh"
#include "NOD_geometry_nodes_closure_location.hh"
#include "NOD_geometry_nodes_closure_signature.hh"
#include "NOD_nested_node_id.hh"

#include "ED_node_c.hh"

#include "UI_interface_layout.hh"

struct SpaceNode;
struct ARegion;
struct Main;
struct bContext;
struct bNodeSocket;
struct bNodeTree;
struct Object;
struct rcti;
struct rctf;
struct NodesModifierData;
struct uiLayout;

namespace blender::bke {
class bNodeTreeZone;
}

namespace blender::ed::space_node {

void tree_update(const bContext *C);
void tag_update_id(ID *id);

float grid_size_get();

/** Update the active node tree based on the context. */
void snode_set_context(const bContext &C);

VectorSet<bNode *> get_selected_nodes(bNodeTree &node_tree);

/**
 * \param is_new_node: If the node was just inserted, it is allowed to be inserted in a link, even
 * if it is linked already (after link-drag-search).
 */
void node_insert_on_link_flags_set(SpaceNode &snode,
                                   const ARegion &region,
                                   bool attach_enabled,
                                   bool is_new_node);

/**
 * Tag the editor to highlight the frame that currently transformed nodes will be attached to.
 */
void node_insert_on_frame_flag_set(bContext &C, SpaceNode &snode, const int2 &cursor);
void node_insert_on_frame_flag_clear(SpaceNode &snode);

/**
 * Assumes link with #NODE_LINK_INSERT_TARGET set.
 */
void node_insert_on_link_flags(Main &bmain, SpaceNode &snode, bool is_new_node);
void node_insert_on_link_flags_clear(bNodeTree &node_tree);

/**
 * Draw a single node socket at default size.
 */
void node_socket_draw(bNodeSocket *sock, const rcti *rect, const float color[4], float scale);
void node_draw_nodesocket(const rctf *rect,
                          const float color_inner[4],
                          const float color_outline[4],
                          float outline_thickness,
                          int shape,
                          float aspect);

void std_node_socket_colors_get(int socket_type, float *r_color);

/**
 * Find the nested node id of a currently visible node in the root tree.
 */
std::optional<nodes::FoundNestedNodeID> find_nested_node_id_in_root(const SpaceNode &snode,
                                                                    const bNode &node);
std::optional<nodes::FoundNestedNodeID> find_nested_node_id_in_root(
    const bNodeTree &root_tree, const ComputeContext *compute_context, const int node_id);

struct ObjectAndModifier {
  const Object *object;
  const NodesModifierData *nmd;
};
/**
 * Finds the context-modifier for the node editor.
 */
std::optional<ObjectAndModifier> get_modifier_for_node_editor(const SpaceNode &snode);

bool node_editor_is_for_geometry_nodes_modifier(const SpaceNode &snode,
                                                const Object &object,
                                                const NodesModifierData &nmd);

/**
 * Get the compute context for the active context that the user is currently looking at in that
 * node tree.
 */
[[nodiscard]] const ComputeContext *compute_context_for_edittree(
    const SpaceNode &snode, bke::ComputeContextCache &compute_context_cache);

/**
 * Get the active compute context for the given socket in the current edittree.
 */
[[nodiscard]] const ComputeContext *compute_context_for_edittree_socket(
    const SpaceNode &snode,
    bke::ComputeContextCache &compute_context_cache,
    const bNodeSocket &socket);

[[nodiscard]] const ComputeContext *compute_context_for_edittree_node(
    const SpaceNode &snode, bke::ComputeContextCache &compute_context_cache, const bNode &node);

/**
 * Creates a compute context for the given zone. It takes e.g. the current inspection index into
 * account.
 */
[[nodiscard]] const ComputeContext *compute_context_for_zone(
    const bke::bNodeTreeZone &zone,
    bke::ComputeContextCache &compute_context_cache,
    const ComputeContext *parent_compute_context);
[[nodiscard]] const ComputeContext *compute_context_for_zones(
    const Span<const bke::bNodeTreeZone *> zones,
    bke::ComputeContextCache &compute_context_cache,
    const ComputeContext *parent_compute_context);

void ui_template_node_asset_menu_items(uiLayout &layout,
                                       const bContext &C,
                                       StringRef catalog_path,
                                       const NodeAssetMenuOperatorType operator_type);

/** See #SpaceNode_Runtime::node_can_sync_states. */
Map<int, bool> &node_can_sync_cache_get(SpaceNode &snode);

void node_tree_interface_draw(bContext &C, uiLayout &layout, bNodeTree &tree);

const char *node_socket_get_label(const bNodeSocket *socket, const char *panel_label = nullptr);

}  // namespace blender::ed::space_node
