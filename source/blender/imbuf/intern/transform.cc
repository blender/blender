/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup imbuf
 */

#include "BLI_math.h"
#include "BLI_rect.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

namespace blender::imbuf::transform {

struct TransformUserData {
  const ImBuf *src;
  ImBuf *dst;
  float start_uv[2];
  float add_x[2];
  float add_y[2];
  rctf src_crop;

  void init(const float transform_matrix[4][4])
  {
    init_start_uv(transform_matrix);
    init_add_x(transform_matrix);
    init_add_y(transform_matrix);
  }

 private:
  void init_start_uv(const float transform_matrix[4][4])
  {
    float start_uv_v3[3];
    float orig[3];
    zero_v3(orig);
    mul_v3_m4v3(start_uv_v3, transform_matrix, orig);
    copy_v2_v2(start_uv, start_uv_v3);
  }

  void init_add_x(const float transform_matrix[4][4])
  {
    const int width = src->x;
    float add_x_v3[3];
    float uv_max_x[3];
    zero_v3(uv_max_x);
    uv_max_x[0] = width;
    uv_max_x[1] = 0.0f;
    mul_v3_m4v3(add_x_v3, transform_matrix, uv_max_x);
    sub_v2_v2(add_x_v3, start_uv);
    mul_v2_fl(add_x_v3, 1.0f / width);
    copy_v2_v2(add_x, add_x_v3);
  }

  void init_add_y(const float transform_matrix[4][4])
  {
    const int height = src->y;
    float add_y_v3[3];
    float uv_max_y[3];
    zero_v3(uv_max_y);
    uv_max_y[0] = 0.0f;
    uv_max_y[1] = height;
    mul_v3_m4v3(add_y_v3, transform_matrix, uv_max_y);
    sub_v2_v2(add_y_v3, start_uv);
    mul_v2_fl(add_y_v3, 1.0f / height);
    copy_v2_v2(add_y, add_y_v3);
  }
};

template<eIMBTransformMode Mode, InterpolationColorFunction ColorInterpolation, int ChannelLen = 4>
class ScanlineProcessor {
 private:
  void pixel_from_buffer(const struct ImBuf *ibuf, unsigned char **outI, float **outF, int y) const

  {
    const size_t offset = ((size_t)ibuf->x) * y * ChannelLen;

    if (ibuf->rect) {
      *outI = (unsigned char *)ibuf->rect + offset;
    }

    if (ibuf->rect_float) {
      *outF = ibuf->rect_float + offset;
    }
  }

 public:
  void process(const TransformUserData *user_data, int scanline)
  {
    const int width = user_data->dst->x;

    float uv[2];
    madd_v2_v2v2fl(uv, user_data->start_uv, user_data->add_y, scanline);

    unsigned char *outI = nullptr;
    float *outF = nullptr;
    pixel_from_buffer(user_data->dst, &outI, &outF, scanline);

    for (int xi = 0; xi < width; xi++) {
      if constexpr (Mode == IMB_TRANSFORM_MODE_CROP_SRC) {
        if (uv[0] >= user_data->src_crop.xmin && uv[0] < user_data->src_crop.xmax &&
            uv[1] >= user_data->src_crop.ymin && uv[1] < user_data->src_crop.ymax) {
          ColorInterpolation(user_data->src, outI, outF, uv[0], uv[1]);
        }
      }
      else {
        ColorInterpolation(user_data->src, outI, outF, uv[0], uv[1]);
      }
      add_v2_v2(uv, user_data->add_x);
      if (outI) {
        outI += ChannelLen;
      }
      if (outF) {
        outF += ChannelLen;
      }
    }
  }
};

template<typename Processor> void transform_scanline_function(void *custom_data, int scanline)
{
  const TransformUserData *user_data = static_cast<const TransformUserData *>(custom_data);
  Processor processor;
  processor.process(user_data, scanline);
}

template<InterpolationColorFunction DefaultFunction, InterpolationColorFunction WrapRepeatFunction>
ScanlineThreadFunc get_scanline_function(const eIMBTransformMode mode)

{
  switch (mode) {
    case IMB_TRANSFORM_MODE_REGULAR:
      return transform_scanline_function<
          ScanlineProcessor<IMB_TRANSFORM_MODE_REGULAR, DefaultFunction>>;
    case IMB_TRANSFORM_MODE_CROP_SRC:
      return transform_scanline_function<
          ScanlineProcessor<IMB_TRANSFORM_MODE_CROP_SRC, DefaultFunction>>;
    case IMB_TRANSFORM_MODE_WRAP_REPEAT:
      return transform_scanline_function<
          ScanlineProcessor<IMB_TRANSFORM_MODE_WRAP_REPEAT, WrapRepeatFunction>>;
  }

  BLI_assert_unreachable();
  return nullptr;
}

template<eIMBInterpolationFilterMode Filter>
static void transform(TransformUserData *user_data, const eIMBTransformMode mode)
{
  ScanlineThreadFunc scanline_func = nullptr;

  if (user_data->dst->rect_float) {
    constexpr InterpolationColorFunction interpolation_function =
        Filter == IMB_FILTER_NEAREST ? nearest_interpolation_color_fl :
                                       bilinear_interpolation_color_fl;
    scanline_func =
        get_scanline_function<interpolation_function, nearest_interpolation_color_wrap>(mode);
  }
  else if (user_data->dst->rect) {
    constexpr InterpolationColorFunction interpolation_function =
        Filter == IMB_FILTER_NEAREST ? nearest_interpolation_color_char :
                                       bilinear_interpolation_color_char;
    scanline_func =
        get_scanline_function<interpolation_function, nearest_interpolation_color_wrap>(mode);
  }

  if (scanline_func != nullptr) {
    IMB_processor_apply_threaded_scanlines(user_data->dst->y, scanline_func, user_data);
  }
}

}  // namespace blender::imbuf::transform

extern "C" {

using namespace blender::imbuf::transform;

void IMB_transform(const struct ImBuf *src,
                   struct ImBuf *dst,
                   const eIMBTransformMode mode,
                   const eIMBInterpolationFilterMode filter,
                   const float transform_matrix[4][4],
                   const struct rctf *src_crop)
{
  BLI_assert_msg(mode != IMB_TRANSFORM_MODE_CROP_SRC || src_crop != nullptr,
                 "No source crop rect given, but crop source is requested. Or source crop rect "
                 "was given, but crop source was not requested.");

  TransformUserData user_data;
  user_data.src = src;
  user_data.dst = dst;
  if (mode == IMB_TRANSFORM_MODE_CROP_SRC) {
    user_data.src_crop = *src_crop;
  }
  user_data.init(transform_matrix);

  if (filter == IMB_FILTER_NEAREST) {
    transform<IMB_FILTER_NEAREST>(&user_data, mode);
  }
  else {
    transform<IMB_FILTER_BILINEAR>(&user_data, mode);
  }
}
}
