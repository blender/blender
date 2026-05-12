/* SPDX-FileCopyrightText: 2006-2008 Peter Schlaile < peter [at] schlaile [dot] de >.
 * SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "sequencer_scopes.hh"

namespace blender::ed::vse {

SeqScopes::~SeqScopes()
{
  cleanup();
}

void SeqScopes::cleanup()
{
  histogram.data.reinitialize(0);
  last_ibuf = nullptr;
  last_timeline_frame = 0;
}

static Array<float4> pixels_to_scope_space(const ColormanageProcessor &processor,
                                           int64_t num,
                                           const float *src,
                                           int64_t stride)
{
  Array<float4> result(num, NoInitialization());
  for (int64_t i : result.index_range()) {
    premul_to_straight_v4_v4(result[i], src);
    src += stride;
  }
  processor.apply(&result.data()->x, result.size(), 1, 4, false);
  return result;
}

static Array<float4> pixels_to_scope_space(const ColormanageProcessor &processor,
                                           int64_t num,
                                           const uchar *src,
                                           int64_t stride)
{
  Array<float4> result(num, NoInitialization());
  for (int64_t i : result.index_range()) {
    rgba_uchar_to_float(result[i], src);
    src += stride;
  }
  processor.apply(&result.data()->x, result.size(), 1, 4, false);
  return result;
}

void ScopeHistogram::calc_from_ibuf(const ImBuf *ibuf,
                                    const ColorManagedViewSettings &view_settings,
                                    const ColorManagedDisplaySettings &display_settings)
{
  /* Use scope space so that the histogram matches the waveform/parade encoding,
   * with proper HDR and wide gamut handling. */
  std::optional<ColormanageProcessor> cm_processor =
      ColormanageProcessor::display_processor_for_imbuf(
          ibuf, &view_settings, &display_settings, DISPLAY_SPACE_SCOPE);

  /* Calculate histogram of input image with parallel reduction:
   * process in chunks, and merge their histograms. */
  Array<uint3> counts(NUM_BINS, uint3(0));
  data = threading::parallel_reduce(
      IndexRange(IMB_get_pixel_count(ibuf)),
      16 * 1024,
      counts,
      [&](const IndexRange range, const Array<uint3> &init) {
        Array<uint3> res = init;

        if (ibuf->float_data()) {
          const float *src = ibuf->float_data() + range.first() * 4;
          if (!cm_processor) {
            for ([[maybe_unused]] const int64_t index : range) {
              float4 pixel;
              premul_to_straight_v4_v4(pixel, src);
              res[float_to_bin(pixel.x)].x++;
              res[float_to_bin(pixel.y)].y++;
              res[float_to_bin(pixel.z)].z++;
              src += 4;
            }
          }
          else {
            Array<float4> pixels = pixels_to_scope_space(
                cm_processor.value(), range.size(), src, 4);
            for (const float4 &pixel : pixels) {
              res[float_to_bin(pixel.x)].x++;
              res[float_to_bin(pixel.y)].y++;
              res[float_to_bin(pixel.z)].z++;
            }
          }
        }
        else {
          const uchar *src = ibuf->byte_data() + range.first() * 4;
          if (!cm_processor) {
            for ([[maybe_unused]] const int64_t index : range) {
              float4 pixel;
              rgba_uchar_to_float(pixel, src);
              res[float_to_bin(pixel.x)].x++;
              res[float_to_bin(pixel.y)].y++;
              res[float_to_bin(pixel.z)].z++;
              src += 4;
            }
          }
          else {
            Array<float4> pixels = pixels_to_scope_space(
                cm_processor.value(), range.size(), src, 4);
            for (const float4 &pixel : pixels) {
              res[float_to_bin(pixel.x)].x++;
              res[float_to_bin(pixel.y)].y++;
              res[float_to_bin(pixel.z)].z++;
            }
          }
        }
        return res;
      },
      /* Merge histograms computed per-thread. */
      [&](const Array<uint3> &a, const Array<uint3> &b) {
        BLI_assert(a.size() == b.size());
        Array<uint3> res(a.size());
        for (int i = 0; i < a.size(); i++) {
          res[i] = a[i] + b[i];
        }
        return res;
      });

  max_value = uint3(0);
  for (int64_t i : data.index_range()) {
    max_value = math::max(max_value, data[i]);
  }
}

}  // namespace blender::ed::vse
