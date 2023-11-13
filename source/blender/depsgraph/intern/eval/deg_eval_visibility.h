/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

struct Depsgraph;

namespace blender::deg {

struct Depsgraph;
struct IDNode;

/* Evaluate actual node visibility flags based on the current state of object's visibility
 * restriction flags. */
void deg_evaluate_object_node_visibility(::Depsgraph *depsgraph, IDNode *id_node);

/* Update node visibility flags based on actual modifiers mode flags. */
void deg_evaluate_object_modifiers_mode_node_visibility(::Depsgraph *depsgraph, IDNode *id_node);

/* Flush both static and dynamic visibility flags from leaves up to the roots, making it possible
 * to know whether a node has affect on something (potentially) visible. */
void deg_graph_flush_visibility_flags(Depsgraph *graph);
void deg_graph_flush_visibility_flags_if_needed(Depsgraph *graph);

}  // namespace blender::deg
