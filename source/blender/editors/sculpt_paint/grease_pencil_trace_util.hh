/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_color_types.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_task.hh"

#include "IMB_imbuf_types.hh"

#ifdef WITH_POTRACE
#  include "potracelib.h"
#endif

namespace blender::bke {
class CurvesGeometry;
}

namespace blender::ed::image_trace {

#ifdef WITH_POTRACE
using Bitmap = potrace_bitmap_t;
using Trace = potrace_state_t;
#else
struct Bitmap;
struct Trace;
#endif

Bitmap *create_bitmap(const int2 &size);
void free_bitmap(Bitmap *bm);

/**
 * ThresholdFn separates foreground/background pixels by turning a color value into a bool.
 * Must accept either a ColorGeometry4f or a ColorGeometry4b.
 *     bool fn(const ColorGeometry4f &color);
 *     bool fn(const ColorGeometry4b &color);
 */
template<typename ThresholdFn> Bitmap *image_to_bitmap(const ImBuf &ibuf, ThresholdFn fn);
ImBuf *bitmap_to_image(const Bitmap &bm);

/* Policy for resolving ambiguity during decomposition of bitmaps into paths. */
enum class TurnPolicy : int8_t {
  /* Prefers to connect foreground pixels. */
  Foreground = 0,
  /* Prefers to connect background pixels. */
  Background = 1,
  /* Always take a left turn. */
  Left = 2,
  /* Always take a right turn. */
  Right = 3,
  /* Prefers to connect minority color in the neighborhood. */
  Minority = 4,
  /* Prefers to connect majority color in the neighborhood. */
  Majority = 5,
  /* Chose direction randomly. */
  Random = 6,
};

struct TraceParams {
  /* Area of the largest path to be ignored. */
  int size_threshold = 2;
  /* Resolves ambiguous turns in path decomposition. */
  TurnPolicy turn_policy = TurnPolicy::Minority;
  /* Corner threshold. */
  float alpha_max = 1.0f;
  /* True to enable curve optimization. */
  bool optimize_curves = true;
  /* Curve optimization tolerance. */
  float optimize_tolerance = 0.2f;
};

/**
 * Trace boundaries in the bitmap.
 */
Trace *trace_bitmap(const TraceParams &params, Bitmap &bm);
void free_trace(Trace *trace);

/**
 * Create curves from trace data.
 * Pixels are interpreted as (x, y, 0) coordinates and transformed.
 */
bke::CurvesGeometry trace_to_curves(const Trace &trace,
                                    StringRef hole_attribute_id,
                                    const float4x4 &transform);
/**
 * Create curves from trace data.
 * Pixels are transformed by the \a pixel_to_position function.
 */
bke::CurvesGeometry trace_to_curves(const Trace &trace,
                                    StringRef hole_attribute_id,
                                    FunctionRef<float3(const int2 &)> pixel_to_position);

/* Inline functions. */

/**
 * Convert an image to a potrace bitmap representing foreground and background regions.
 * \param fn: Function that returns true if the given color is a foreground color.
 */
template<typename ThresholdFn> Bitmap *image_to_bitmap(const ImBuf &ibuf, ThresholdFn fn)
{
#ifdef WITH_POTRACE
  constexpr int BM_WORDSIZE = int(sizeof(potrace_word));
  constexpr int BM_WORDBITS = 8 * BM_WORDSIZE;
  constexpr potrace_word BM_HIBIT = potrace_word(1) << (BM_WORDBITS - 1);

  potrace_bitmap_t *bm = create_bitmap({ibuf.x, ibuf.y});
  const int num_words = bm->dy * bm->h;
  const int words_per_scanline = bm->dy;
  /* Note: bitmap stores one bit per pixel, but can't easily use a BitSpan, because the bit order
   * is reversed in each word (most-significant bit is on the left). */
  MutableSpan<potrace_word> words = {bm->map, num_words};

  if (ibuf.float_buffer.data) {
    const Span<ColorGeometry4f> colors = {
        reinterpret_cast<ColorGeometry4f *>(ibuf.float_buffer.data), ibuf.x * ibuf.y};
    threading::parallel_for(IndexRange(ibuf.y), 4096, [&](const IndexRange range) {
      /* Use callback with the correct color conversion. */
      constexpr bool is_float_color_fn =
          std::is_invocable_r_v<void, ThresholdFn, const ColorGeometry4f &>;
      for (const int y : range) {
        MutableSpan<potrace_word> scanline_words = words.slice(
            IndexRange(words_per_scanline * y, words_per_scanline));
        const Span<ColorGeometry4f> scanline_colors = colors.slice(IndexRange(y * ibuf.x, ibuf.x));
        for (int x = 0; x < ibuf.x; x++) {
          potrace_word &word = scanline_words[x / BM_WORDBITS];
          const potrace_word mask = BM_HIBIT >> (x & (BM_WORDBITS - 1));

          const ColorGeometry4f &fcolor = scanline_colors[x];
          bool is_foreground;
          if constexpr (!is_float_color_fn) {
            is_foreground = fn(
                ColorGeometry4b(fcolor.r * 255, fcolor.g * 255, fcolor.b * 255, fcolor.a * 255));
          }
          else {
            is_foreground = fn(fcolor);
          }

          if (is_foreground) {
            word |= mask;
          }
          else {
            word &= ~mask;
          }
        }
      }
    });
    return bm;
  }

  const Span<ColorGeometry4b> colors = {reinterpret_cast<ColorGeometry4b *>(ibuf.byte_buffer.data),
                                        ibuf.x * ibuf.y};
  threading::parallel_for(IndexRange(ibuf.y), 4096, [&](const IndexRange range) {
    /* Use callback with the correct color conversion. */
    constexpr bool is_float_color_fn =
        std::is_invocable_r_v<void, ThresholdFn, const ColorGeometry4f &>;
    for (const int y : range) {
      MutableSpan<potrace_word> scanline_words = words.slice(
          IndexRange(words_per_scanline * y, words_per_scanline));
      const Span<ColorGeometry4b> scanline_colors = colors.slice(IndexRange(y * ibuf.x, ibuf.x));
      for (uint32_t x = 0; x < ibuf.x; x++) {
        potrace_word &word = scanline_words[x / BM_WORDBITS];
        const potrace_word mask = BM_HIBIT >> (x & (BM_WORDBITS - 1));

        const ColorGeometry4b bcolor = scanline_colors[x];
        bool is_foreground;
        if constexpr (is_float_color_fn) {
          is_foreground = fn(ColorGeometry4f(
              bcolor.r / 255.0f, bcolor.r / 255.0f, bcolor.r / 255.0f, bcolor.r / 255.0f));
        }
        else {
          is_foreground = fn(bcolor);
        }

        if (is_foreground) {
          word |= mask;
        }
        else {
          word &= ~mask;
        }
      }
    }
  });
  return bm;
#else
  UNUSED_VARS(ibuf, fn);
  return nullptr;
#endif
}

}  // namespace blender::ed::image_trace
