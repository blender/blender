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

static void rgba_float_to_display_space(ColormanageProcessor *processor,
                                        const ColorSpace *src_colorspace,
                                        MutableSpan<float4> pixels)
{
  IMB_colormanagement_colorspace_to_scene_linear(
      &pixels.data()->x, pixels.size(), 1, 4, src_colorspace, false);
  IMB_colormanagement_processor_apply(processor, &pixels.data()->x, pixels.size(), 1, 4, false);
}

static Array<float4> pixels_to_display_space(ColormanageProcessor *processor,
                                             const ColorSpace *src_colorspace,
                                             int64_t num,
                                             const float *src,
                                             int64_t stride)
{
  Array<float4> result(num, NoInitialization());
  for (int64_t i : result.index_range()) {
    premul_to_straight_v4_v4(result[i], src);
    src += stride;
  }
  rgba_float_to_display_space(processor, src_colorspace, result);
  return result;
}

static Array<float4> pixels_to_display_space(ColormanageProcessor *processor,
                                             const ColorSpace *src_colorspace,
                                             int64_t num,
                                             const uchar *src,
                                             int64_t stride)
{
  Array<float4> result(num, NoInitialization());
  for (int64_t i : result.index_range()) {
    rgba_uchar_to_float(result[i], src);
    src += stride;
  }
  rgba_float_to_display_space(processor, src_colorspace, result);
  return result;
}

void ScopeHistogram::calc_from_ibuf(const ImBuf *ibuf,
                                    const ColorManagedViewSettings &view_settings,
                                    const ColorManagedDisplaySettings &display_settings)
{
  ColormanageProcessor *cm_processor = IMB_colormanagement_display_processor_for_imbuf(
      ibuf, &view_settings, &display_settings);

  const bool is_float = ibuf->float_buffer.data != nullptr;
  const int hist_size = is_float ? BINS_HDR : BINS_01;

  /* Calculate histogram of input image with parallel reduction:
   * process in chunks, and merge their histograms. */
  Array<uint3> counts(hist_size, uint3(0));
  data = threading::parallel_reduce(
      IndexRange(IMB_get_pixel_count(ibuf)),
      16 * 1024,
      counts,
      [&](const IndexRange range, const Array<uint3> &init) {
        Array<uint3> res = init;

        if (is_float) {
          const float *src = ibuf->float_buffer.data + range.first() * 4;
          if (!cm_processor) {
            /* Float image, no color space conversions needed. */
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
            /* Float image, with color space conversions. */
            Array<float4> pixels = pixels_to_display_space(
                cm_processor, ibuf->float_buffer.colorspace, range.size(), src, 4);
            for (const float4 &pixel : pixels) {
              res[float_to_bin(pixel.x)].x++;
              res[float_to_bin(pixel.y)].y++;
              res[float_to_bin(pixel.z)].z++;
            }
          }
        }
        else {
          /* Byte images just use 256 histogram bins, directly indexed by value. */
          const uchar *src = ibuf->byte_buffer.data + range.first() * 4;
          if (!cm_processor) {
            /* Byte image, no color space conversions needed. */
            for ([[maybe_unused]] const int64_t index : range) {
              res[src[0]].x++;
              res[src[1]].y++;
              res[src[2]].z++;
              src += 4;
            }
          }
          else {
            /* Byte image, with color space conversions. */
            Array<float4> pixels = pixels_to_display_space(
                cm_processor, ibuf->byte_buffer.colorspace, range.size(), src, 4);
            for (const float4 &pixel : pixels) {
              uchar pixel_b[4];
              rgba_float_to_uchar(pixel_b, pixel);
              res[pixel_b[0]].x++;
              res[pixel_b[1]].y++;
              res[pixel_b[2]].z++;
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

  if (cm_processor) {
    IMB_colormanagement_processor_free(cm_processor);
  }

  max_value = uint3(0);
  max_bin = uint3(0);
  for (int64_t i : data.index_range()) {
    const uint3 &val = data[i];
    max_value = math::max(max_value, val);
    if (val.x != 0) {
      max_bin.x = i;
    }
    if (val.y != 0) {
      max_bin.y = i;
    }
    if (val.z != 0) {
      max_bin.z = i;
    }
  }
}

}  // namespace blender::ed::vse
