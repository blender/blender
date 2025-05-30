/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender::nodes {

/** These values are also written to .blend files, so don't change them lightly. */
enum class NodeWarningType {
  Error = 0,
  Warning = 1,
  Info = 2,
};

int node_warning_type_icon(NodeWarningType type);
int node_warning_type_severity(NodeWarningType type);

}  // namespace blender::nodes
