/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

namespace blender::ocio {

class View {
 public:
  virtual ~View() = default;

  /**
   * Index of the view within the display that owns it.
   * The index is 0-based.
   */
  int index = -1;

  /**
   * Name of this view.
   * The name is used to address to this view from various places of the configuration.
   */
  virtual StringRefNull name() const = 0;
};

}  // namespace blender::ocio
