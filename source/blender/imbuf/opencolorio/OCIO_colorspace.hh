/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_span.hh"
#include "BLI_string_ref.hh"

namespace blender::ocio {

class CPUProcessor;

class ColorSpace {
 public:
  virtual ~ColorSpace() = default;

  /**
   * Global index of the color space within the OpenColorIO configuration.
   * The index is 0-based.
   */
  int index = -1;

  /**
   * Name and description of this space.
   *
   * The name is used to address to this color space from various places of the configuration.
   * The description is used for UI to give better clue what the space is to artists.
   */
  virtual StringRefNull name() const = 0;
  virtual StringRefNull description() const = 0;

  /**
   * Returns true if there is a conversion from this color space to the scene linear.
   */
  virtual bool is_invertible() const = 0;

  /**
   * Check whether this color space matches one of the built-in spaces like scene linear or sRGB
   * (in its standard Blender notation).
   */
  virtual bool is_scene_linear() const = 0;
  virtual bool is_srgb() const = 0;

  /**
   * The color space is a non-color data.
   * Data color spaces do not change values of underlying pixels when converting to other color
   * spaces.
   */
  virtual bool is_data() const = 0;

  /**
   * The color space is display referred rather than scene referred.
   */
  virtual bool is_display_referred() const = 0;

  /*
   * Identifier for colorspaces that works with multiple OpenColorIO configurations,
   * as defined by the ASWF Color Interop Forum.
   */
  virtual StringRefNull interop_id() const = 0;

  /**
   * Quick access to CPU processors that convert color space from the current one to scene linear
   * and vice versa.
   * The call is allowed to be caching from the color space implementation perspective.
   */
  const virtual CPUProcessor *get_to_scene_linear_cpu_processor() const = 0;
  const virtual CPUProcessor *get_from_scene_linear_cpu_processor() const = 0;
};

}  // namespace blender::ocio
