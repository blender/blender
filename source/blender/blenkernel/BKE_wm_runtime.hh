/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender::bke {

class WindowManagerRuntime {
 public:
  /** Indicates whether interface is locked for user interaction. */
  bool is_interface_locked = false;
};

}  // namespace blender::bke
