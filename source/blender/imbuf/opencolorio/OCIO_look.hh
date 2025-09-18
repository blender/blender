/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

namespace blender::ocio {

class Look {
 public:
  virtual ~Look() = default;

  /**
   * Global index of the look within the OpenColorIO configuration.
   * The index is 0-based.
   *
   * NOTE: The implementation ensures None as a look. It has index of 0. This makes it so looks in
   * the OpenColorIO configurations are to be offset by 1 from this index.
   */
  int index = -1;

  /**
   * The look is known to not perform any actual color space conversion.
   */
  bool is_noop = false;

  /**
   * Name of this look.
   * The name is used to address to this look from various places of the configuration.
   */
  virtual StringRefNull name() const = 0;

  /**
   * Name of the look presented in the interface.
   * It is typically derived from the OpenColorIO's look name by stripping the view name prefix/
   */
  virtual StringRefNull ui_name() const = 0;

  /**
   * Description of the look from the OpenColorIO config.
   */
  virtual StringRefNull description() const = 0;

  /**
   * When not empty the look is specific to the view with the given name.
   */
  virtual StringRefNull view() const = 0;

  /**
   * process_space defines the color space the image required to be in for the math to apply
   * correctly.
   */
  virtual StringRefNull process_space() const = 0;
};

}  // namespace blender::ocio
