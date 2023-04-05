/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#include "intern/node/deg_node.h"

struct ID;
struct Main;

namespace blender::deg {

struct Depsgraph;

/* Get type of a node which corresponds to a ID_RECALC_GEOMETRY tag. */
NodeType geometry_tag_to_component(const ID *id);

/* Tag given ID for an update in all registered dependency graphs. */
void id_tag_update(Main *bmain, ID *id, unsigned int flags, eUpdateSource update_source);

/* Tag given ID for an update with in a given dependency graph. */
void graph_id_tag_update(
    Main *bmain, Depsgraph *graph, ID *id, unsigned int flags, eUpdateSource update_source);

/* Tag IDs of the graph for the visibility update tags.
 * Will do nothing if the graph is not tagged for visibility update. */
void graph_tag_ids_for_visible_update(Depsgraph *graph);

}  // namespace blender::deg
