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
  /* Byte images just have bins for the 0..255 range. */
  static constexpr int BINS_BYTE = 256;
  /* Float images spread -0.25..+1.25 range over 512 bins. */
  static constexpr int BINS_FLOAT = 512;
  static constexpr float FLOAT_VAL_MIN = -0.25f;
  static constexpr float FLOAT_VAL_MAX = 1.25f;
  Array<uint3> data;
  uint3 max_value;

  void calc_from_ibuf(const ImBuf *ibuf,
                      const ColorManagedViewSettings &view_settings,
                      const ColorManagedDisplaySettings &display_settings);
  bool is_float_hist() const
  {
    return data.size() == BINS_FLOAT;
  }
};

struct SeqScopes : public NonCopyable {
  /* Multiplier to map YUV U,V range (+-0.436, +-0.615) to +-0.5 on both axes. */
  static constexpr float VECSCOPE_U_SCALE = 0.5f / 0.436f;
  static constexpr float VECSCOPE_V_SCALE = 0.5f / 0.615f;

  const ImBuf *reference_ibuf = nullptr;
  ImBuf *zebra_ibuf = nullptr;
  ImBuf *waveform_ibuf = nullptr;
  ImBuf *sep_waveform_ibuf = nullptr;
  ImBuf *vector_ibuf = nullptr;
  ScopeHistogram histogram;

  SeqScopes() = default;
  ~SeqScopes();

  void cleanup();
};

ImBuf *make_waveform_view_from_ibuf(const ImBuf *ibuf);
ImBuf *make_sep_waveform_view_from_ibuf(const ImBuf *ibuf);
ImBuf *make_vectorscope_view_from_ibuf(const ImBuf *ibuf);
ImBuf *make_zebra_view_from_ibuf(const ImBuf *ibuf, float perc);

}  // namespace blender::ed::vse
