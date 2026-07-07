/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

namespace blender::ocio {

class ColorSpace;

enum class Gamut {
  Unknown,
  Rec709,  /* sRGB primaries + D65 white point. */
  P3D65,   /* DCI-P3 primaries + D65 white point. */
  Rec2020, /* Rec.2020 primaries + D65 white point */
};

enum class TransferFunction {
  Unknown,
  sRGB,         /* Piecewise sRGB */
  ExtendedsRGB, /* Piecewise sRGB, unclipped for wide gamut */
  Gamma18,      /* Pure Gamma 1.8 */
  Gamma22,      /* Pure Gamma 2.2 */
  Gamma24,      /* Pure Gamma 2.4 */
  Gamma26,      /* Pure Gamma 2.6 */
  PQ,           /* PQ from Rec.2100 */
  HLG,          /* HLG from Rec.2100 */
};

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

  /**
   * Description of the view from the OpenColorIO config.
   */
  virtual StringRefNull description() const = 0;

  /**
   * Does this view transform output HDR colors?
   */
  virtual bool is_hdr() const = 0;

  /**
   * Does this view transform support display emulation?
   */
  virtual bool support_emulation() const = 0;

  /**
   * Gamut of the display colorspace.
   */
  virtual Gamut gamut() const = 0;

  /**
   * Transfer function of the display colorspace.
   */
  virtual TransferFunction transfer_function() const = 0;

  /**
   * Display colorspace that this view transform transforms into.
   * Not guaranteed to be display referred.
   */
  virtual const ColorSpace *display_colorspace() const = 0;
};

}  // namespace blender::ocio
