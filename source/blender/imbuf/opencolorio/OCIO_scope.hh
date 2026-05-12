/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

namespace blender::ocio {

/**
 * Grid line for waveform/parade scopes: a display code value with a
 * pre-formatted label string.
 */
struct ScopeGraticule {
  float value = 0.0f;
  char label[16] = "";
};

/**
 * Information for drawing scopes (waveform/parade/vectorscope).
 */
struct ScopeInfo {
  /** Graticules. */
  Vector<ScopeGraticule> graticules;
  /** Display code value corresponding to 100 nits (SDR reference white).
   * 1.0 for SDR displays, less than 1.0 for HDR displays. */
  float sdr_white_level = 1.0f;
  /** Max luminance in nits from the view transform, 0 if unknown. */
  int view_transform_max_nits = 0;
  /** Display code value corresponding to #view_transform_max_nits. */
  float view_transform_max_nits_value = 1.0f;

  /** Is the scope HDR? */
  bool is_hdr = false;
  /** Gamut mapping from scope display space to Rec.709 linear. */
  blender::float3x3 scope_gamut_to_rec709 = blender::float3x3::identity();
  /** Luminance coefficients to use for scope evaluation (default Rec.709). */
  blender::float3 luma_coefficients = blender::float3(0.2126f, 0.7152f, 0.0722f);
  /** RGB to YCbCr matrix for the vectorscope. */
  blender::float3x3 yuv_matrix = blender::float3x3::identity();
};

}  // namespace blender::ocio
