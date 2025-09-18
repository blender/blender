/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

namespace blender::ocio {

class CPUProcessor;
class View;

class Display {
 public:
  virtual ~Display() = default;

  /**
   * Global index of the display within the OpenColorIO configuration.
   * The index is 0-based.
   */
  int index = -1;

  /**
   * Name of this display.
   * The name is used to address to this display from various places of the configuration.
   */
  virtual StringRefNull name() const = 0;

  /*
   * Name to display in the user interface.
   */
  virtual StringRefNull ui_name() const = 0;

  /**
   * Description of the display from the OpenColorIO config.
   */
  virtual StringRefNull description() const = 0;

  /**
   * Get default view of this display. */
  virtual const View *get_default_view() const = 0;

  /**
   * Get the view without tonemapping. */
  virtual const View *get_untonemapped_view() const = 0;

  /**
   * Get view with the given name for this display.
   * If the view does not exist nullptr is returned.
   */
  virtual const View *get_view_by_name(StringRefNull name) const = 0;

  /**
   * Get the number of view in this display.
   */
  virtual int get_num_views() const = 0;

  /**
   * Get view with the given index within the display.
   * If the index is invalid nullptr is returned.
   */
  virtual const View *get_view_by_index(int index) const = 0;

  /**
   * Quick access to processors that convert color space from the display to scene linear and vice
   * versa. The call is allowed to be caching from the color space implementation perspective.
   *
   * With #use_display_emulation, rather than converting to the display space, this converts to
   * extended sRGB emulating the display space.
   */
  const virtual CPUProcessor *get_to_scene_linear_cpu_processor(
      bool use_display_emulation) const = 0;
  const virtual CPUProcessor *get_from_scene_linear_cpu_processor(
      bool use_display_emulation) const = 0;

  /**
   * Determine if the display supports HDR.
   */
  virtual bool is_hdr() const = 0;
};

}  // namespace blender::ocio
