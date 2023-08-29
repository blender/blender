/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "BKE_viewer_path.h"

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
void activate_geometry_node(Main &bmain, SpaceNode &snode, bNode &node);

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
  blender::StringRefNull modifier_name;
  /* Contains only group node and simulation zone elements. */
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

/**
 * Checks if the node referenced by the viewer and its entire context is still active, i.e. some
 * editor is showing it.
 */
bool is_active_geometry_nodes_viewer(const bContext &C, const ViewerPath &viewer_path);

}  // namespace blender::ed::viewer_path
