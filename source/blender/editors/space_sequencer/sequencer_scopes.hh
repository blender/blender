/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_utility_mixins.hh"

struct ColorManagedViewSettings;
struct ColorManagedDisplaySettings;
struct ImBuf;

namespace blender::ed::vse {

struct ScopeHistogram {
  /* LDR (0..1) range is covered by this many bins. */
  static constexpr int BINS_01 = 256;
  /* HDR images extend the range to 0..12 uniformly. */
  static constexpr int BINS_HDR = BINS_01 * 12;
  /* R,G,B counts for each bin. */
  Array<uint3> data;
  /* Maximum R,G,B counts across all bins. */
  uint3 max_value = uint3(0);
  /* Maximum R,G,B bins used. */
  uint3 max_bin = uint3(0);

  void calc_from_ibuf(const ImBuf *ibuf,
                      const ColorManagedViewSettings &view_settings,
                      const ColorManagedDisplaySettings &display_settings);

  static int float_to_bin(float f)
  {
    int bin = int(f * BINS_01);
    return math::clamp(bin, 0, BINS_HDR - 1);
  }

  static float bin_to_float(int bin)
  {
    return float(bin) / (BINS_01 - 1);
  }
};

struct SeqScopes : public NonCopyable {
  /* Multiplier to map YUV U,V range (+-0.436, +-0.615) to +-0.5 on both axes. */
  static constexpr float VECSCOPE_U_SCALE = 0.5f / 0.436f;
  static constexpr float VECSCOPE_V_SCALE = 0.5f / 0.615f;

  const ImBuf *reference_ibuf = nullptr;
  int timeline_frame = 0;
  ImBuf *zebra_ibuf = nullptr;
  ImBuf *waveform_ibuf = nullptr;
  ImBuf *sep_waveform_ibuf = nullptr;
  ImBuf *vector_ibuf = nullptr;
  ScopeHistogram histogram;

  SeqScopes() = default;
  ~SeqScopes();

  void cleanup();
};

ImBuf *make_waveform_view_from_ibuf(const ImBuf *ibuf,
                                    const ColorManagedViewSettings &view_settings,
                                    const ColorManagedDisplaySettings &display_settings);
ImBuf *make_sep_waveform_view_from_ibuf(const ImBuf *ibuf,
                                        const ColorManagedViewSettings &view_settings,
                                        const ColorManagedDisplaySettings &display_settings);
ImBuf *make_vectorscope_view_from_ibuf(const ImBuf *ibuf,
                                       const ColorManagedViewSettings &view_settings,
                                       const ColorManagedDisplaySettings &display_settings);
ImBuf *make_zebra_view_from_ibuf(const ImBuf *ibuf, float perc);

}  // namespace blender::ed::vse
