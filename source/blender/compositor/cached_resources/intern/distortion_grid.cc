/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>

#include "BLI_hash.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "BKE_movieclip.hh"
#include "BKE_tracking.hh"

#include "COM_context.hh"
#include "COM_distortion_grid.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * Distortion Grid Key.
 */

DistortionGridKey::DistortionGridKey(const MovieTrackingCamera &camera,
                                     int2 size,
                                     DistortionType type,
                                     int2 calibration_size)
    : camera(camera), size(size), type(type), calibration_size(calibration_size)
{
}

uint64_t DistortionGridKey::hash() const
{
  return get_default_hash(
      BKE_tracking_camera_distortion_hash(&camera), size, type, calibration_size);
}

bool operator==(const DistortionGridKey &a, const DistortionGridKey &b)
{
  return BKE_tracking_camera_distortion_equal(&a.camera, &b.camera) && a.size == b.size &&
         a.type == b.type && a.calibration_size == b.calibration_size;
}

/* --------------------------------------------------------------------
 * Distortion Grid.
 */

/* Reduces the given function in parallel over the given range, the reduction function should have
 * the given identity value. The given function gets as arguments the index of the element of the
 * range as well as a reference to the value where the result should be accumulated, while the
 * reduction function gets a reference to two values and returns their reduction. */
template<typename Value, typename Function, typename Reduction>
static Value parallel_reduce(const int range,
                             const Value &identity,
                             const Function &function,
                             const Reduction &reduction)
{
  return threading::parallel_reduce(
      IndexRange(range),
      32,
      identity,
      [&](const IndexRange sub_range, const Value &initial_value) {
        Value result = initial_value;
        for (const int64_t i : sub_range) {
          function(i, result);
        }
        return result;
      },
      reduction);
}

/* Given the domain of an image, compute its domain after distortion by the given distortion
 * parameters. The data window of the domain will likely grow or shrink depending on the
 * distortion, while the display window will stay the same. */
static Domain compute_output_domain(MovieDistortion *distortion,
                                    const int2 &calibration_size,
                                    const DistortionType &type,
                                    const Domain &domain)
{
  auto distortion_function = [&](const float2 &coordinates) {
    /* We are looping over the data space, so transfer to the display space by subtracting the data
     * offset. Finally, transform to the calibration space since this is what the distortion
     * functions expect. */
    const float2 display_coordinates = coordinates - float2(domain.data_offset);
    const float2 normalized_coordinates = display_coordinates / float2(domain.display_size);
    const float2 calibrated_coordinates = normalized_coordinates * float2(calibration_size);

    float2 distorted_coordinates;
    if (type == DistortionType::Undistort) {
      BKE_tracking_distortion_undistort_v2(
          distortion, calibrated_coordinates, distorted_coordinates);
    }
    else {
      BKE_tracking_distortion_distort_v2(
          distortion, calibrated_coordinates, distorted_coordinates);
    }

    /* Undo the space transformations into the data space and finally into the normalized sampling
     * coordinates. */
    const float2 distorted_normalized_coordinates = distorted_coordinates /
                                                    float2(calibration_size);
    const float2 distorted_display_coordinates = distorted_normalized_coordinates *
                                                 float2(domain.display_size);
    return distorted_display_coordinates;
  };

  /* Maximum distorted x location along the right edge of the image. */
  const float maximum_x = parallel_reduce(
      domain.data_size.y + 1,
      std::numeric_limits<float>::lowest(),
      [&](const int i, float &accumulated_value) {
        const float2 position = float2(domain.data_size.x, i);
        accumulated_value = math::max(accumulated_value, distortion_function(position).x);
      },
      math::max<float>);

  /* Minimum distorted x location along the left edge of the image. */
  const float minimum_x = parallel_reduce(
      domain.data_size.y + 1,
      std::numeric_limits<float>::max(),
      [&](const int i, float &accumulated_value) {
        const float2 position = float2(0.0f, i);
        accumulated_value = math::min(accumulated_value, distortion_function(position).x);
      },
      math::min<float>);

  /* Minimum distorted y location along the bottom edge of the image. */
  const float minimum_y = parallel_reduce(
      domain.data_size.x + 1,
      std::numeric_limits<float>::max(),
      [&](const int i, float &accumulated_value) {
        const float2 position = float2(i, 0.0f);
        accumulated_value = math::min(accumulated_value, distortion_function(position).y);
      },
      math::min<float>);

  /* Maximum distorted y location along the top edge of the image. */
  const float maximum_y = parallel_reduce(
      domain.data_size.x + 1,
      std::numeric_limits<float>::lowest(),
      [&](const int i, float &accumulated_value) {
        const float2 position = float2(i, domain.data_size.y);
        accumulated_value = math::max(accumulated_value, distortion_function(position).y);
      },
      math::max<float>);

  /* Compute the deltas from the image edges to the maximum/minimum distorted location along the
   * direction of that edge. */
  const float2 lower_left_delta = float2(0.0f) - float2(minimum_x, minimum_y);
  const float2 upper_right_delta = float2(maximum_x, maximum_y) - float2(domain.data_size);

  /* Rounds the deltas away from zero and clamp to the size to avoid excessive sizes in case of
   * extreme distortion. */
  const int2 lower_left_offset = math::min(domain.data_size, int2(math::ceil(lower_left_delta)));
  const int2 upper_right_offset = math::min(domain.data_size, int2(math::ceil(upper_right_delta)));

  /* Grow/Shrink the domain of the image according to the offsets and set the data offset
   * accordingly. */
  Domain output_domain = domain;
  output_domain.data_size = domain.data_size + lower_left_offset + upper_right_offset;
  output_domain.data_offset = lower_left_offset;
  return output_domain;
}

DistortionGrid::DistortionGrid(Context &context,
                               MovieClip *movie_clip,
                               Domain domain,
                               DistortionType type,
                               int2 calibration_size)
    : result(context.create_result(ResultType::Float2, ResultPrecision::Full))
{
  MovieDistortion *distortion = BKE_tracking_distortion_new(
      &movie_clip->tracking, calibration_size.x, calibration_size.y);

  const Domain output_domain = compute_output_domain(distortion, calibration_size, type, domain);
  this->result.allocate_texture(output_domain, false, ResultStorageType::CPU);

  parallel_for(this->result.domain().data_size, [&](const int2 texel) {
    /* We are looping over the data space, so transfer to the display space by subtracting the data
     * offset. Add 0.5 to distort at the pixel centers. Finally, transform to the calibration space
     * since this is what the distortion functions expect. */
    const float2 display_coordinates = float2(texel - output_domain.data_offset) + 0.5f;
    const float2 normalized_coordinates = display_coordinates / float2(domain.display_size);
    const float2 calibrated_coordinates = normalized_coordinates * float2(calibration_size);

    /* Notice that if we are undistorting the image, we need to distort the coordinates space and
     * vice versa, hence the inverted condition. */
    float2 distorted_coordinates;
    if (type == DistortionType::Undistort) {
      BKE_tracking_distortion_distort_v2(
          distortion, calibrated_coordinates, distorted_coordinates);
    }
    else {
      BKE_tracking_distortion_undistort_v2(
          distortion, calibrated_coordinates, distorted_coordinates);
    }

    /* Undo the space transformations into the data space and finally into the normalized sampling
     * coordinates. */
    const float2 distorted_normalized_coordinates = distorted_coordinates /
                                                    float2(calibration_size);
    const float2 distorted_display_coordinates = distorted_normalized_coordinates *
                                                 float2(domain.display_size);
    const float2 distorted_data_coordinates = distorted_display_coordinates +
                                              float2(domain.data_offset);
    const float2 sampling_coordinates = distorted_data_coordinates / float2(domain.data_size);
    this->result.store_pixel(texel, sampling_coordinates);
  });

  BKE_tracking_distortion_free(distortion);

  if (context.use_gpu()) {
    const Result gpu_result = this->result.upload_to_gpu(false);
    this->result.release();
    this->result = gpu_result;
  }
}

DistortionGrid::~DistortionGrid()
{
  this->result.release();
}

/* --------------------------------------------------------------------
 * Distortion Grid Container.
 */

void DistortionGridContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

static int2 get_movie_clip_size(MovieClip *movie_clip, int frame_number)
{
  MovieClipUser user = {};
  BKE_movieclip_user_set_frame(&user, frame_number);

  int2 size;
  BKE_movieclip_get_size(movie_clip, &user, &size.x, &size.y);

  return size;
}

Result &DistortionGridContainer::get(
    Context &context, MovieClip *movie_clip, Domain domain, DistortionType type, int frame_number)
{
  const int2 calibration_size = get_movie_clip_size(movie_clip, frame_number);

  const DistortionGridKey key(
      movie_clip->tracking.camera, domain.data_size, type, calibration_size);

  auto &distortion_grid = *map_.lookup_or_add_cb(key, [&]() {
    return std::make_unique<DistortionGrid>(context, movie_clip, domain, type, calibration_size);
  });

  distortion_grid.needed = true;
  return distortion_grid.result;
}

}  // namespace blender::compositor
