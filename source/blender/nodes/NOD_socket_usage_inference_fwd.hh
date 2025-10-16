/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender::nodes::socket_usage_inference {

class SocketUsageParams;

struct SocketUsage {
  bool is_used = true;
  bool is_visible = true;
};

}  // namespace blender::nodes::socket_usage_inference
