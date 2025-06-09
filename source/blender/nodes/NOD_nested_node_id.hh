/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender::nodes {

/**
 * Utility struct to store information about a nested node id. Also see #bNestedNodeRef.
 * Sometimes these IDs can only be used when they are at the top level and not within zones.
 */
struct FoundNestedNodeID {
  int id;
  bool is_in_simulation = false;
  bool is_in_loop = false;
  bool is_in_closure = false;
};

}  // namespace blender::nodes
