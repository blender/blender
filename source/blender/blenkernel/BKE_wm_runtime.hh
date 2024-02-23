/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_windowmanager_types.h"

namespace blender::bke {

class WindowManagerRuntime {
 public:
  /** Indicates whether interface is locked for user interaction. */
  bool is_interface_locked = false;

  /** Information and error reports. */
  ReportList reports;

  WindowManagerRuntime();
  ~WindowManagerRuntime();
};

}  // namespace blender::bke
