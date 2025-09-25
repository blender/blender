/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_compute_context.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "BKE_compute_context_cache_fwd.hh"

#include "DNA_viewer_path_types.h"

struct Main;
struct SpaceNode;
struct bNode;
struct bContext;
struct Object;

namespace blender::ed::viewer_path {

/**
 * Activates the given node in the context provided by the editor. This indirectly updates all
 * non-pinned viewer paths in other editors (spreadsheet and 3d view).
 */
void activate_geometry_node(Main &bmain,
                            SpaceNode &snode,
                            bNode &node,
                            std::optional<int> item_identifier = std::nullopt);

/**
 * Returns the object referenced by the viewer path. This only returns something if the viewer path
 * *only* contains the object and nothing more.
 */
Object *parse_object_only(const ViewerPath &viewer_path);

/**
 * Represents a parsed #ViewerPath for easier consumption.
 */
struct ViewerPathForGeometryNodesViewer {
  Object *object;
  /** #ModifierData.persistent_uid. */
  int modifier_uid;
  /** Contains only group node and simulation zone elements. */
  blender::Vector<const ViewerPathElem *> node_path;
  int32_t viewer_node_id;
};

/**
 * Parses a #ViewerPath into a #ViewerPathForGeometryNodesViewer or returns none if that does not
 * work.
 */
std::optional<ViewerPathForGeometryNodesViewer> parse_geometry_nodes_viewer(
    const ViewerPath &viewer_path);

/**
 * Finds the node referenced by the #ViewerPath within the provided editor. If no node is
 * referenced, null is returned. When two different editors show the same node group but in a
 * different context, it's possible that the same node is active in one editor but not the other.
 */
bNode *find_geometry_nodes_viewer(const ViewerPath &viewer_path, SpaceNode &snode);

/**
 * Checks if the node referenced by the viewer path and its entire context still exists. The node
 * does not have to be visible for this to return true.
 */
bool exists_geometry_nodes_viewer(const ViewerPathForGeometryNodesViewer &parsed_viewer_path);

enum class UpdateActiveGeometryNodesViewerResult {
  StillActive,
  Updated,
  NotActive,
};

/**
 * Checks if the node referenced by the viewer and its entire context is still active, i.e. some
 * editor is showing it. If not, the viewer path might be updated in minor ways (like changing the
 * repeat zone iteration).
 */
UpdateActiveGeometryNodesViewerResult update_active_geometry_nodes_viewer(const bContext &C,
                                                                          ViewerPath &viewer_path);

/**
 * Some viewer path elements correspond to compute-contexts. This function converts from the viewer
 * path element to the corresponding compute context if possible.
 *
 * \return The corresponding compute context or null.
 */
[[nodiscard]] const ComputeContext *compute_context_for_viewer_path_elem(
    const ViewerPathElem &elem,
    bke::ComputeContextCache &compute_context_cache,
    const ComputeContext *parent_compute_context);

/**
 * The inverse of #compute_context_for_viewer_path_elem. It helps to create a viewer path (which
 * can be stored in .blend files) from a compute context.
 */
[[nodiscard]] ViewerPathElem *viewer_path_elem_for_compute_context(
    const ComputeContext &compute_context);

}  // namespace blender::ed::viewer_path
