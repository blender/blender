/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup imbuf
 */

#include <array>
#include <type_traits>

#include "BLI_math.h"
#include "BLI_rect.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

namespace blender::imbuf::transform {

struct TransformUserData {
  /** \brief Source image buffer to read from. */
  const ImBuf *src;
  /** \brief Destination image buffer to write to. */
  ImBuf *dst;
  /** \brief UV coordinates at the origin (0,0) in source image space. */
  float start_uv[2];

  /**
   * \brief delta UV coordinates along the source image buffer, when moving a single pixel in the X
   * axis of the dst image buffer.
   */
  float add_x[2];

  /**
   * \brief delta UV coordinate along the source image buffer, when moving a single pixel in the Y
   * axes of the dst image buffer.
   */
  float add_y[2];

  /**
   * \brief Cropping region in source image pixel space.
   */
  rctf src_crop;

  /**
   * \brief Initialize the start_uv, add_x and add_y fields based on the given transform matrix.
   */
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

/**
 * \brief Base class for source discarding.
 *
 * The class decides if a specific uv coordinate from the source buffer should be ignored.
 * This is used to mix multiple images over a single output buffer. Discarded pixels will
 * not change the output buffer.
 */
class BaseDiscard {
 public:
  virtual ~BaseDiscard() = default;

  /**
   * \brief Should the source pixel at the given uv coordinate be discarded.
   */
  virtual bool should_discard(const TransformUserData &user_data, const float uv[2]) = 0;
};

/**
 * \brief Crop uv-coordinates that are outside the user data src_crop rect.
 */
class CropSource : public BaseDiscard {
 public:
  /**
   * \brief Should the source pixel at the given uv coordinate be discarded.
   *
   * Uses user_data.src_crop to determine if the uv coordinate should be skipped.
   */
  bool should_discard(const TransformUserData &user_data, const float uv[2]) override
  {
    return uv[0] < user_data.src_crop.xmin || uv[0] >= user_data.src_crop.xmax ||
           uv[1] < user_data.src_crop.ymin || uv[1] >= user_data.src_crop.ymax;
  }
};

/**
 * \brief Discard that does not discard anything.
 */
class NoDiscard : public BaseDiscard {
 public:
  /**
   * \brief Should the source pixel at the given uv coordinate be discarded.
   *
   * Will never discard any pixels.
   */
  bool should_discard(const TransformUserData &UNUSED(user_data),
                      const float UNUSED(uv[2])) override
  {
    return false;
  }
};

/**
 * \brief Pointer to a pixel to write to in serial.
 */
template<
    /**
     * \brief Kind of buffer.
     * Possible options: float, uchar.
     */
    typename StorageType = float,

    /**
     * \brief Number of channels of a single pixel.
     */
    int NumChannels = 4>
class PixelPointer {
 public:
  static const int ChannelLen = NumChannels;

 private:
  StorageType *pointer;

 public:
  void init_pixel_pointer(const ImBuf *image_buffer, int x, int y)
  {
    const size_t offset = (y * (size_t)image_buffer->x + x) * NumChannels;

    if constexpr (std::is_same_v<StorageType, float>) {
      pointer = image_buffer->rect_float + offset;
    }
    else if constexpr (std::is_same_v<StorageType, uchar>) {
      pointer = const_cast<uchar *>(
          static_cast<const uchar *>(static_cast<const void *>(image_buffer->rect)) + offset);
    }
    else {
      pointer = nullptr;
    }
  }

  /**
   * \brief Get pointer to the current pixel to write to.
   */
  StorageType *get_pointer()
  {
    return pointer;
  }

  void increase_pixel_pointer()
  {
    pointer += NumChannels;
  }
};

/**
 * \brief Wrapping mode for the uv coordinates.
 *
 * Subclasses have the ability to change the UV coordinates when sampling the source buffer.
 */
class BaseUVWrapping {
 public:
  /**
   * \brief modify the given u coordinate.
   */
  virtual float modify_u(const ImBuf *source_buffer, float u) = 0;

  /**
   * \brief modify the given v coordinate.
   */
  virtual float modify_v(const ImBuf *source_buffer, float v) = 0;
};

/**
 * \brief UVWrapping method that does not modify the UV coordinates.
 */
class PassThroughUV : public BaseUVWrapping {
 public:
  float modify_u(const ImBuf *UNUSED(source_buffer), float u) override
  {
    return u;
  }

  float modify_v(const ImBuf *UNUSED(source_buffer), float v) override
  {
    return v;
  }
};

/**
 * \brief UVWrapping method that wrap repeats the UV coordinates.
 */
class WrapRepeatUV : public BaseUVWrapping {
 public:
  float modify_u(const ImBuf *source_buffer, float u) override

  {
    int x = (int)floor(u);
    x = x % source_buffer->x;
    if (x < 0) {
      x += source_buffer->x;
    }
    return x;
  }

  float modify_v(const ImBuf *source_buffer, float v) override
  {
    int y = (int)floor(v);
    y = y % source_buffer->y;
    if (y < 0) {
      y += source_buffer->y;
    }
    return y;
  }
};

/**
 * \brief Read a sample from an image buffer.
 *
 * A sampler can read from an image buffer.
 */
template<
    /** \brief Interpolation mode to use when sampling. */
    eIMBInterpolationFilterMode Filter,

    /** \brief storage type of a single pixel channel (uchar or float). */
    typename StorageType,
    /**
     * \brief number of channels if the image to read.
     *
     * Must match the actual channels of the image buffer that is sampled.
     */
    int NumChannels,
    /**
     * \brief Wrapping method to perform
     *
     * Should be a subclass of BaseUVWrapper
     */
    typename UVWrapping>
class Sampler {
  UVWrapping uv_wrapper;

 public:
  using ChannelType = StorageType;
  static const int ChannelLen = NumChannels;
  using SampleType = std::array<StorageType, NumChannels>;

  void sample(const ImBuf *source, const float u, const float v, SampleType &r_sample)
  {
    if constexpr (Filter == IMB_FILTER_BILINEAR && std::is_same_v<StorageType, float> &&
                  NumChannels == 4) {
      const float wrapped_u = uv_wrapper.modify_u(source, u);
      const float wrapped_v = uv_wrapper.modify_v(source, v);
      bilinear_interpolation_color_fl(source, nullptr, r_sample.data(), wrapped_u, wrapped_v);
    }
    else if constexpr (Filter == IMB_FILTER_NEAREST && std::is_same_v<StorageType, uchar> &&
                       NumChannels == 4) {
      const float wrapped_u = uv_wrapper.modify_u(source, u);
      const float wrapped_v = uv_wrapper.modify_v(source, v);
      nearest_interpolation_color_char(source, r_sample.data(), nullptr, wrapped_u, wrapped_v);
    }
    else if constexpr (Filter == IMB_FILTER_BILINEAR && std::is_same_v<StorageType, uchar> &&
                       NumChannels == 4) {
      const float wrapped_u = uv_wrapper.modify_u(source, u);
      const float wrapped_v = uv_wrapper.modify_v(source, v);
      bilinear_interpolation_color_char(source, r_sample.data(), nullptr, wrapped_u, wrapped_v);
    }
    else if constexpr (Filter == IMB_FILTER_BILINEAR && std::is_same_v<StorageType, float>) {
      if constexpr (std::is_same_v<UVWrapping, WrapRepeatUV>) {
        BLI_bilinear_interpolation_wrap_fl(source->rect_float,
                                           r_sample.data(),
                                           source->x,
                                           source->y,
                                           NumChannels,
                                           u,
                                           v,
                                           true,
                                           true);
      }
      else {
        const float wrapped_u = uv_wrapper.modify_u(source, u);
        const float wrapped_v = uv_wrapper.modify_v(source, v);
        BLI_bilinear_interpolation_fl(source->rect_float,
                                      r_sample.data(),
                                      source->x,
                                      source->y,
                                      NumChannels,
                                      wrapped_u,
                                      wrapped_v);
      }
    }
    else if constexpr (Filter == IMB_FILTER_NEAREST && std::is_same_v<StorageType, float>) {
      const float wrapped_u = uv_wrapper.modify_u(source, u);
      const float wrapped_v = uv_wrapper.modify_v(source, v);
      sample_nearest_float(source, wrapped_u, wrapped_v, r_sample);
    }
    else {
      /* Unsupported sampler. */
      BLI_assert_unreachable();
    }
  }

 private:
  void sample_nearest_float(const ImBuf *source,
                            const float u,
                            const float v,
                            SampleType &r_sample)
  {
    BLI_STATIC_ASSERT(std::is_same_v<StorageType, float>);

    /* ImBuf in must have a valid rect or rect_float, assume this is already checked */
    int x1 = (int)(u);
    int y1 = (int)(v);

    /* Break when sample outside image is requested. */
    if (x1 < 0 || x1 >= source->x || y1 < 0 || y1 >= source->y) {
      for (int i = 0; i < NumChannels; i++) {
        r_sample[i] = 0.0f;
      }
      return;
    }

    const size_t offset = ((size_t)source->x * y1 + x1) * NumChannels;
    const float *dataF = source->rect_float + offset;
    for (int i = 0; i < NumChannels; i++) {
      r_sample[i] = dataF[i];
    }
  }
};

/**
 * \brief Change the number of channels and store it.
 *
 * Template class to convert and store a sample in a PixelPointer.
 * It supports:
 * - 4 channel uchar -> 4 channel uchar.
 * - 4 channel float -> 4 channel float.
 * - 3 channel float -> 4 channel float.
 * - 2 channel float -> 4 channel float.
 * - 1 channel float -> 4 channel float.
 */
template<typename StorageType, int SourceNumChannels, int DestinationNumChannels>
class ChannelConverter {
 public:
  using SampleType = std::array<StorageType, SourceNumChannels>;
  using PixelType = PixelPointer<StorageType, DestinationNumChannels>;

  /**
   * \brief Convert the number of channels of the given sample to match the pixel pointer and store
   * it at the location the pixel_pointer points at.
   */
  void convert_and_store(const SampleType &sample, PixelType &pixel_pointer)
  {
    if constexpr (std::is_same_v<StorageType, uchar>) {
      BLI_STATIC_ASSERT(SourceNumChannels == 4, "Unsigned chars always have 4 channels.");
      BLI_STATIC_ASSERT(DestinationNumChannels == 4, "Unsigned chars always have 4 channels.");

      copy_v4_v4_uchar(pixel_pointer.get_pointer(), sample.data());
    }
    else if constexpr (std::is_same_v<StorageType, float> && SourceNumChannels == 4 &&
                       DestinationNumChannels == 4) {
      copy_v4_v4(pixel_pointer.get_pointer(), sample.data());
    }
    else if constexpr (std::is_same_v<StorageType, float> && SourceNumChannels == 3 &&
                       DestinationNumChannels == 4) {
      copy_v4_fl4(pixel_pointer.get_pointer(), sample[0], sample[1], sample[2], 1.0f);
    }
    else if constexpr (std::is_same_v<StorageType, float> && SourceNumChannels == 2 &&
                       DestinationNumChannels == 4) {
      copy_v4_fl4(pixel_pointer.get_pointer(), sample[0], sample[1], 0.0f, 1.0f);
    }
    else if constexpr (std::is_same_v<StorageType, float> && SourceNumChannels == 1 &&
                       DestinationNumChannels == 4) {
      copy_v4_fl4(pixel_pointer.get_pointer(), sample[0], sample[0], sample[0], 1.0f);
    }
    else {
      BLI_assert_unreachable();
    }
  }
};

/**
 * \brief Processor for a scanline.
 */
template<
    /**
     * \brief Discard function to use.
     *
     * \attention Should be a subclass of BaseDiscard.
     */
    typename Discard,

    /**
     * \brief Color interpolation function to read from the source buffer.
     */
    typename Sampler,

    /**
     * \brief Kernel to store to the destination buffer.
     * Should be an PixelPointer
     */
    typename OutputPixelPointer>
class ScanlineProcessor {
  Discard discarder;
  OutputPixelPointer output;
  Sampler sampler;

  /**
   * \brief Channels sizzling logic to convert between the input image buffer and the output image
   * buffer.
   */
  ChannelConverter<typename Sampler::ChannelType,
                   Sampler::ChannelLen,
                   OutputPixelPointer::ChannelLen>
      channel_converter;

 public:
  /**
   * \brief Inner loop of the transformations, processing a full scanline.
   */
  void process(const TransformUserData *user_data, int scanline)
  {
    const int width = user_data->dst->x;

    float uv[2];
    madd_v2_v2v2fl(uv, user_data->start_uv, user_data->add_y, scanline);

    output.init_pixel_pointer(user_data->dst, 0, scanline);
    for (int xi = 0; xi < width; xi++) {
      if (!discarder.should_discard(*user_data, uv)) {
        typename Sampler::SampleType sample;
        sampler.sample(user_data->src, uv[0], uv[1], sample);
        channel_converter.convert_and_store(sample, output);
      }

      add_v2_v2(uv, user_data->add_x);
      output.increase_pixel_pointer();
    }
  }
};

/**
 * \brief callback function for threaded transformation.
 */
template<typename Processor> void transform_scanline_function(void *custom_data, int scanline)
{
  const TransformUserData *user_data = static_cast<const TransformUserData *>(custom_data);
  Processor processor;
  processor.process(user_data, scanline);
}

template<eIMBInterpolationFilterMode Filter,
         typename StorageType,
         int SourceNumChannels,
         int DestinationNumChannels>
ScanlineThreadFunc get_scanline_function(const eIMBTransformMode mode)

{
  switch (mode) {
    case IMB_TRANSFORM_MODE_REGULAR:
      return transform_scanline_function<
          ScanlineProcessor<NoDiscard,
                            Sampler<Filter, StorageType, SourceNumChannels, PassThroughUV>,
                            PixelPointer<StorageType, DestinationNumChannels>>>;
    case IMB_TRANSFORM_MODE_CROP_SRC:
      return transform_scanline_function<
          ScanlineProcessor<CropSource,
                            Sampler<Filter, StorageType, SourceNumChannels, PassThroughUV>,
                            PixelPointer<StorageType, DestinationNumChannels>>>;
    case IMB_TRANSFORM_MODE_WRAP_REPEAT:
      return transform_scanline_function<
          ScanlineProcessor<NoDiscard,
                            Sampler<Filter, StorageType, SourceNumChannels, WrapRepeatUV>,
                            PixelPointer<StorageType, DestinationNumChannels>>>;
  }

  BLI_assert_unreachable();
  return nullptr;
}

template<eIMBInterpolationFilterMode Filter>
ScanlineThreadFunc get_scanline_function(const TransformUserData *user_data,
                                         const eIMBTransformMode mode)
{
  const ImBuf *src = user_data->src;
  const ImBuf *dst = user_data->dst;

  if (src->channels == 4 && dst->channels == 4) {
    return get_scanline_function<Filter, float, 4, 4>(mode);
  }
  if (src->channels == 3 && dst->channels == 4) {
    return get_scanline_function<Filter, float, 3, 4>(mode);
  }
  if (src->channels == 2 && dst->channels == 4) {
    return get_scanline_function<Filter, float, 2, 4>(mode);
  }
  if (src->channels == 1 && dst->channels == 4) {
    return get_scanline_function<Filter, float, 1, 4>(mode);
  }
  return nullptr;
}

template<eIMBInterpolationFilterMode Filter>
static void transform_threaded(TransformUserData *user_data, const eIMBTransformMode mode)
{
  ScanlineThreadFunc scanline_func = nullptr;

  if (user_data->dst->rect_float && user_data->src->rect_float) {
    scanline_func = get_scanline_function<Filter>(user_data, mode);
  }
  else if (user_data->dst->rect && user_data->src->rect) {
    /* Number of channels is always 4 when using uchar buffers (sRGB + straight alpha). */
    scanline_func = get_scanline_function<Filter, uchar, 4, 4>(mode);
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
    transform_threaded<IMB_FILTER_NEAREST>(&user_data, mode);
  }
  else {
    transform_threaded<IMB_FILTER_BILINEAR>(&user_data, mode);
  }
}
}
