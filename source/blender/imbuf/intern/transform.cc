/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup imbuf
 */

#include <array>
#include <type_traits>

#include "BLI_math.h"
#include "BLI_math_color_blend.h"
#include "BLI_math_matrix.hh"
#include "BLI_rect.h"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

namespace blender::imbuf::transform {

struct TransformUserData {
  /** \brief Source image buffer to read from. */
  const ImBuf *src;
  /** \brief Destination image buffer to write to. */
  ImBuf *dst;
  /** \brief UV coordinates at the origin (0,0) in source image space. */
  double2 start_uv;

  /**
   * \brief delta UV coordinates along the source image buffer, when moving a single pixel in the X
   * axis of the dst image buffer.
   */
  double2 add_x;

  /**
   * \brief delta UV coordinate along the source image buffer, when moving a single pixel in the Y
   * axes of the dst image buffer.
   */
  double2 add_y;

  struct {
    /**
     * Contains per sub-sample a delta to be added to the uv of the source image buffer.
     */
    Vector<double2, 9> delta_uvs;
  } subsampling;

  struct {
    IndexRange x_range;
    IndexRange y_range;
  } destination_region;

  /**
   * \brief Cropping region in source image pixel space.
   */
  rctf src_crop;

  /**
   * \brief Initialize the start_uv, add_x and add_y fields based on the given transform matrix.
   */
  void init(const float4x4 &transform_matrix,
            const int num_subsamples,
            const bool do_crop_destination_region)
  {
    init_start_uv(transform_matrix);
    init_add_x(transform_matrix);
    init_add_y(transform_matrix);
    init_subsampling(num_subsamples);
    init_destination_region(transform_matrix, do_crop_destination_region);
  }

 private:
  void init_start_uv(const float4x4 &transform_matrix)
  {
    start_uv = double2(transform_matrix.location().xy());
  }

  void init_add_x(const float4x4 &transform_matrix)
  {
    const double width = src->x;
    add_x = double2(transform_matrix.x_axis()) * width + double2(transform_matrix.location());
    add_x = (add_x - start_uv) * (1.0 / width);
  }

  void init_add_y(const float4x4 &transform_matrix)
  {
    const double height = src->y;
    add_y = double2(transform_matrix.y_axis()) * height + double2(transform_matrix.location());
    add_y = (add_y - start_uv) * (1.0 / height);
  }

  void init_subsampling(const int num_subsamples)
  {
    double2 subsample_add_x = add_x / num_subsamples;
    double2 subsample_add_y = add_y / num_subsamples;
    double2 offset_x = -add_x * 0.5 + subsample_add_x * 0.5;
    double2 offset_y = -add_y * 0.5 + subsample_add_y * 0.5;

    for (int y : IndexRange(0, num_subsamples)) {
      for (int x : IndexRange(0, num_subsamples)) {
        double2 delta_uv = -offset_x - offset_y;
        delta_uv += x * subsample_add_x;
        delta_uv += y * subsample_add_y;
        subsampling.delta_uvs.append(delta_uv);
      }
    }
  }

  void init_destination_region(const float4x4 &transform_matrix,
                               const bool do_crop_destination_region)
  {
    if (!do_crop_destination_region) {
      destination_region.x_range = IndexRange(dst->x);
      destination_region.y_range = IndexRange(dst->y);
      return;
    }

    /* Transform the src_crop to the destination buffer with a margin. */
    const int2 margin(2);
    rcti rect;
    BLI_rcti_init_minmax(&rect);
    float4x4 inverse = math::invert(transform_matrix);
    for (const int2 &src_coords : {
             int2(src_crop.xmin, src_crop.ymin),
             int2(src_crop.xmax, src_crop.ymin),
             int2(src_crop.xmin, src_crop.ymax),
             int2(src_crop.xmax, src_crop.ymax),
         })
    {
      float3 dst_co = math::transform_point(inverse, float3(src_coords.x, src_coords.y, 0.0f));
      BLI_rcti_do_minmax_v(&rect, int2(dst_co) + margin);
      BLI_rcti_do_minmax_v(&rect, int2(dst_co) - margin);
    }

    /* Clamp rect to fit inside the image buffer. */
    rcti dest_rect;
    BLI_rcti_init(&dest_rect, 0, dst->x, 0, dst->y);
    BLI_rcti_isect(&rect, &dest_rect, &rect);
    destination_region.x_range = IndexRange(rect.xmin, BLI_rcti_size_x(&rect));
    destination_region.y_range = IndexRange(rect.ymin, BLI_rcti_size_y(&rect));
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
  virtual bool should_discard(const TransformUserData &user_data, const double2 &uv) = 0;
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
  bool should_discard(const TransformUserData &user_data, const double2 &uv) override
  {
    return uv.x < user_data.src_crop.xmin || uv.x >= user_data.src_crop.xmax ||
           uv.y < user_data.src_crop.ymin || uv.y >= user_data.src_crop.ymax;
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
  bool should_discard(const TransformUserData & /*user_data*/, const double2 & /*uv*/) override
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
  void init_pixel_pointer(const ImBuf *image_buffer, int2 start_coordinate)
  {
    const size_t offset = (start_coordinate.y * size_t(image_buffer->x) + start_coordinate.x) *
                          NumChannels;

    if constexpr (std::is_same_v<StorageType, float>) {
      pointer = image_buffer->float_buffer.data + offset;
    }
    else if constexpr (std::is_same_v<StorageType, uchar>) {
      pointer = const_cast<uchar *>(
          static_cast<const uchar *>(static_cast<const void *>(image_buffer->byte_buffer.data)) +
          offset);
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
  virtual double modify_u(const ImBuf *source_buffer, double u) = 0;

  /**
   * \brief modify the given v coordinate.
   */
  virtual double modify_v(const ImBuf *source_buffer, double v) = 0;

  /**
   * \brief modify the given uv coordinate.
   */
  double2 modify_uv(const ImBuf *source_buffer, const double2 &uv)
  {
    return double2(modify_u(source_buffer, uv.x), modify_v(source_buffer, uv.y));
  }
};

/**
 * \brief UVWrapping method that does not modify the UV coordinates.
 */
class PassThroughUV : public BaseUVWrapping {
 public:
  double modify_u(const ImBuf * /*source_buffer*/, double u) override
  {
    return u;
  }

  double modify_v(const ImBuf * /*source_buffer*/, double v) override
  {
    return v;
  }
};

/**
 * \brief UVWrapping method that wrap repeats the UV coordinates.
 */
class WrapRepeatUV : public BaseUVWrapping {
 public:
  double modify_u(const ImBuf *source_buffer, double u) override

  {
    int x = int(floor(u));
    x = x % source_buffer->x;
    if (x < 0) {
      x += source_buffer->x;
    }
    return x;
  }

  double modify_v(const ImBuf *source_buffer, double v) override
  {
    int y = int(floor(v));
    y = y % source_buffer->y;
    if (y < 0) {
      y += source_buffer->y;
    }
    return y;
  }
};

// TODO: should we use math_vectors for this.
template<typename StorageType, int NumChannels>
class Pixel : public std::array<StorageType, NumChannels> {
 public:
  void clear()
  {
    for (int channel_index : IndexRange(NumChannels)) {
      (*this)[channel_index] = 0;
    }
  }

  void add_subsample(const Pixel<StorageType, NumChannels> other, int sample_number)
  {
    BLI_STATIC_ASSERT((std::is_same_v<StorageType, uchar>) || (std::is_same_v<StorageType, float>),
                      "Only uchar and float channels supported.");

    float factor = 1.0 / (sample_number + 1);
    if constexpr (std::is_same_v<StorageType, uchar>) {
      BLI_STATIC_ASSERT(NumChannels == 4, "Pixels using uchar requires to have 4 channels.");
      blend_color_interpolate_byte(this->data(), this->data(), other.data(), factor);
    }
    else if constexpr (std::is_same_v<StorageType, float> && NumChannels == 4) {
      blend_color_interpolate_float(this->data(), this->data(), other.data(), factor);
    }
    else if constexpr (std::is_same_v<StorageType, float>) {
      for (int channel_index : IndexRange(NumChannels)) {
        (*this)[channel_index] = (*this)[channel_index] * (1.0 - factor) +
                                 other[channel_index] * factor;
      }
    }
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
  using SampleType = Pixel<StorageType, NumChannels>;

  void sample(const ImBuf *source, const double2 &uv, SampleType &r_sample)
  {
    if constexpr (Filter == IMB_FILTER_BILINEAR && std::is_same_v<StorageType, float> &&
                  NumChannels == 4)
    {
      const double2 wrapped_uv = uv_wrapper.modify_uv(source, uv);
      bilinear_interpolation_color_fl(source, nullptr, r_sample.data(), UNPACK2(wrapped_uv));
    }
    else if constexpr (Filter == IMB_FILTER_NEAREST && std::is_same_v<StorageType, uchar> &&
                       NumChannels == 4)
    {
      const double2 wrapped_uv = uv_wrapper.modify_uv(source, uv);
      nearest_interpolation_color_char(source, r_sample.data(), nullptr, UNPACK2(wrapped_uv));
    }
    else if constexpr (Filter == IMB_FILTER_BILINEAR && std::is_same_v<StorageType, uchar> &&
                       NumChannels == 4)
    {
      const double2 wrapped_uv = uv_wrapper.modify_uv(source, uv);
      bilinear_interpolation_color_char(source, r_sample.data(), nullptr, UNPACK2(wrapped_uv));
    }
    else if constexpr (Filter == IMB_FILTER_BILINEAR && std::is_same_v<StorageType, float>) {
      if constexpr (std::is_same_v<UVWrapping, WrapRepeatUV>) {
        BLI_bilinear_interpolation_wrap_fl(source->float_buffer.data,
                                           r_sample.data(),
                                           source->x,
                                           source->y,
                                           NumChannels,
                                           UNPACK2(uv),
                                           true,
                                           true);
      }
      else {
        const double2 wrapped_uv = uv_wrapper.modify_uv(source, uv);
        BLI_bilinear_interpolation_fl(source->float_buffer.data,
                                      r_sample.data(),
                                      source->x,
                                      source->y,
                                      NumChannels,
                                      UNPACK2(wrapped_uv));
      }
    }
    else if constexpr (Filter == IMB_FILTER_NEAREST && std::is_same_v<StorageType, float>) {
      const double2 wrapped_uv = uv_wrapper.modify_uv(source, uv);
      sample_nearest_float(source, wrapped_uv, r_sample);
    }
    else {
      /* Unsupported sampler. */
      BLI_assert_unreachable();
    }
  }

 private:
  void sample_nearest_float(const ImBuf *source, const double2 &uv, SampleType &r_sample)
  {
    BLI_STATIC_ASSERT(std::is_same_v<StorageType, float>);

    /* ImBuf in must have a valid rect or rect_float, assume this is already checked */
    int x1 = int(uv.x);
    int y1 = int(uv.y);

    /* Break when sample outside image is requested. */
    if (x1 < 0 || x1 >= source->x || y1 < 0 || y1 >= source->y) {
      for (int i = 0; i < NumChannels; i++) {
        r_sample[i] = 0.0f;
      }
      return;
    }

    const size_t offset = (size_t(source->x) * y1 + x1) * NumChannels;
    const float *dataF = source->float_buffer.data + offset;
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
  using SampleType = Pixel<StorageType, SourceNumChannels>;
  using PixelType = PixelPointer<StorageType, DestinationNumChannels>;

  /**
   * \brief Convert the number of channels of the given sample to match the pixel pointer and
   * store it at the location the pixel_pointer points at.
   */
  void convert_and_store(const SampleType &sample, PixelType &pixel_pointer)
  {
    if constexpr (std::is_same_v<StorageType, uchar>) {
      BLI_STATIC_ASSERT(SourceNumChannels == 4, "Unsigned chars always have 4 channels.");
      BLI_STATIC_ASSERT(DestinationNumChannels == 4, "Unsigned chars always have 4 channels.");

      copy_v4_v4_uchar(pixel_pointer.get_pointer(), sample.data());
    }
    else if constexpr (std::is_same_v<StorageType, float> && SourceNumChannels == 4 &&
                       DestinationNumChannels == 4)
    {
      copy_v4_v4(pixel_pointer.get_pointer(), sample.data());
    }
    else if constexpr (std::is_same_v<StorageType, float> && SourceNumChannels == 3 &&
                       DestinationNumChannels == 4)
    {
      copy_v4_fl4(pixel_pointer.get_pointer(), sample[0], sample[1], sample[2], 1.0f);
    }
    else if constexpr (std::is_same_v<StorageType, float> && SourceNumChannels == 2 &&
                       DestinationNumChannels == 4)
    {
      copy_v4_fl4(pixel_pointer.get_pointer(), sample[0], sample[1], 0.0f, 1.0f);
    }
    else if constexpr (std::is_same_v<StorageType, float> && SourceNumChannels == 1 &&
                       DestinationNumChannels == 4)
    {
      copy_v4_fl4(pixel_pointer.get_pointer(), sample[0], sample[0], sample[0], 1.0f);
    }
    else {
      BLI_assert_unreachable();
    }
  }

  void mix_and_store(const SampleType &sample, PixelType &pixel_pointer, const float mix_factor)
  {
    if constexpr (std::is_same_v<StorageType, uchar>) {
      BLI_STATIC_ASSERT(SourceNumChannels == 4, "Unsigned chars always have 4 channels.");
      BLI_STATIC_ASSERT(DestinationNumChannels == 4, "Unsigned chars always have 4 channels.");
      blend_color_interpolate_byte(
          pixel_pointer.get_pointer(), pixel_pointer.get_pointer(), sample.data(), mix_factor);
    }
    else if constexpr (std::is_same_v<StorageType, float> && SourceNumChannels == 4 &&
                       DestinationNumChannels == 4)
    {
      blend_color_interpolate_float(
          pixel_pointer.get_pointer(), pixel_pointer.get_pointer(), sample.data(), mix_factor);
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
   * \brief Channels sizzling logic to convert between the input image buffer and the output
   * image buffer.
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
    if (user_data->subsampling.delta_uvs.size() > 1) {
      process_with_subsampling(user_data, scanline);
    }
    else {
      process_one_sample_per_pixel(user_data, scanline);
    }
  }

 private:
  void process_one_sample_per_pixel(const TransformUserData *user_data, int scanline)
  {
    double2 uv = user_data->start_uv +
                 user_data->destination_region.x_range.first() * user_data->add_x +
                 user_data->add_y * scanline;

    output.init_pixel_pointer(user_data->dst,
                              int2(user_data->destination_region.x_range.first(), scanline));
    for (int xi : user_data->destination_region.x_range) {
      UNUSED_VARS(xi);
      if (!discarder.should_discard(*user_data, uv)) {
        typename Sampler::SampleType sample;
        sampler.sample(user_data->src, uv, sample);
        channel_converter.convert_and_store(sample, output);
      }

      uv += user_data->add_x;
      output.increase_pixel_pointer();
    }
  }

  void process_with_subsampling(const TransformUserData *user_data, int scanline)
  {
    double2 uv = user_data->start_uv +
                 user_data->destination_region.x_range.first() * user_data->add_x +
                 user_data->add_y * scanline;

    output.init_pixel_pointer(user_data->dst,
                              int2(user_data->destination_region.x_range.first(), scanline));
    for (int xi : user_data->destination_region.x_range) {
      UNUSED_VARS(xi);
      typename Sampler::SampleType sample;
      sample.clear();
      int num_subsamples_added = 0;

      for (const double2 &delta_uv : user_data->subsampling.delta_uvs) {
        const double2 subsample_uv = uv + delta_uv;
        if (!discarder.should_discard(*user_data, subsample_uv)) {
          typename Sampler::SampleType sub_sample;
          sampler.sample(user_data->src, subsample_uv, sub_sample);
          sample.add_subsample(sub_sample, num_subsamples_added);
          num_subsamples_added += 1;
        }
      }

      if (num_subsamples_added != 0) {
        const float mix_weight = float(num_subsamples_added) /
                                 user_data->subsampling.delta_uvs.size();
        channel_converter.mix_and_store(sample, output, mix_weight);
      }
      uv += user_data->add_x;
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

  if (user_data->dst->float_buffer.data && user_data->src->float_buffer.data) {
    scanline_func = get_scanline_function<Filter>(user_data, mode);
  }
  else if (user_data->dst->byte_buffer.data && user_data->src->byte_buffer.data) {
    /* Number of channels is always 4 when using uchar buffers (sRGB + straight alpha). */
    scanline_func = get_scanline_function<Filter, uchar, 4, 4>(mode);
  }

  if (scanline_func != nullptr) {
    threading::parallel_for(user_data->destination_region.y_range, 8, [&](IndexRange range) {
      for (int scanline : range) {
        scanline_func(user_data, scanline);
      }
    });
  }
}

}  // namespace blender::imbuf::transform

extern "C" {

using namespace blender::imbuf::transform;

void IMB_transform(const struct ImBuf *src,
                   struct ImBuf *dst,
                   const eIMBTransformMode mode,
                   const eIMBInterpolationFilterMode filter,
                   const int num_subsamples,
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
  user_data.init(blender::float4x4(transform_matrix),
                 num_subsamples,
                 ELEM(mode, IMB_TRANSFORM_MODE_CROP_SRC));

  if (filter == IMB_FILTER_NEAREST) {
    transform_threaded<IMB_FILTER_NEAREST>(&user_data, mode);
  }
  else {
    transform_threaded<IMB_FILTER_BILINEAR>(&user_data, mode);
  }
}
}
