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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/node/deg_node.h"

struct ID;
struct Main;

namespace blender {
namespace deg {

struct Depsgraph;

/* Get type of a node which corresponds to a ID_RECALC_GEOMETRY tag.  */
NodeType geometry_tag_to_component(const ID *id);

/* Tag given ID for an update in all registered dependency graphs. */
void id_tag_update(Main *bmain, ID *id, int flag, eUpdateSource update_source);

/* Tag given ID for an update with in a given dependency graph. */
void graph_id_tag_update(
    Main *bmain, Depsgraph *graph, ID *id, int flag, eUpdateSource update_source);

}  // namespace deg
}  // namespace blender
