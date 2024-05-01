/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cmath>
#include <cstring>
#include <memory>

#include "BLI_array.hh"
#include "BLI_index_range.hh"
#include "BLI_math_base.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "COM_VectorBlurOperation.h"

/* This is identical to the compositor implementation in compositor_motion_blur_info.hh and its
 * related files with the necessary adjustments to make it work for the CPU. */

#define MOTION_BLUR_TILE_SIZE 32
#define DEPTH_SCALE 100.0f

namespace blender::compositor {

VectorBlurOperation::VectorBlurOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  settings_ = nullptr;
}

/* Returns the input velocity that has the larger magnitude. */
static float2 max_velocity(const float2 &a, const float2 &b)
{
  return math::length_squared(a) > math::length_squared(b) ? a : b;
}

/* Identical to motion_blur_tile_indirection_pack_payload, encodes the value and its texel such
 * that the integer length of the value is encoded in the most significant bits, then the x value
 * of the texel are encoded in the middle bits, then the y value of the texel is stored in the
 * least significant bits. */
static uint32_t velocity_atomic_max_value(const float2 &value, const int2 &texel)
{
  const uint32_t length_bits = math::min(uint32_t(math::ceil(math::length(value))), 0x3FFFu);
  return (length_bits << 18u) | ((texel.x & 0x1FFu) << 9u) | (texel.y & 0x1FFu);
}

/* Returns the input velocity that has the larger integer magnitude, and if equal the larger x
 * texel coordinates, and if equal, the larger y texel coordinates. It might be weird that we use
 * an approximate comparison, but this is used for compatibility with the GPU code, which uses
 * atomic integer operations, hence the limited precision. See  velocity_atomic_max_value for more
 * information. */
static float2 max_velocity_approximate(const float2 &a,
                                       const float2 &b,
                                       const int2 &a_texel,
                                       const int2 &b_texel)
{
  return velocity_atomic_max_value(a, a_texel) > velocity_atomic_max_value(b, b_texel) ? a : b;
}

/* Reduces each 32x32 block of velocity pixels into a single velocity whose magnitude is largest.
 * Each of the previous and next velocities are reduces independently. */
static MemoryBuffer compute_max_tile_velocity(MemoryBuffer *velocity_buffer)
{
  const int2 tile_size = int2(MOTION_BLUR_TILE_SIZE);
  const int2 velocity_size = int2(velocity_buffer->get_width(), velocity_buffer->get_height());
  const int2 tiles_count = math::divide_ceil(velocity_size, tile_size);
  MemoryBuffer output(DataType::Color, tiles_count.x, tiles_count.y);

  threading::parallel_for(IndexRange(tiles_count.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(tiles_count.x)) {
        const int2 texel = int2(x, y);

        float2 max_previous_velocity = float2(0.0f);
        float2 max_next_velocity = float2(0.0f);

        for (int j = 0; j < tile_size.y; j++) {
          for (int i = 0; i < tile_size.x; i++) {
            int2 sub_texel = texel * tile_size + int2(i, j);
            const float4 velocity = velocity_buffer->get_elem_clamped(sub_texel.x, sub_texel.y);
            max_previous_velocity = max_velocity(velocity.xy(), max_previous_velocity);
            max_next_velocity = max_velocity(velocity.zw(), max_next_velocity);
          }
        }

        const float4 max_velocity = float4(max_previous_velocity, max_next_velocity);
        copy_v4_v4(output.get_elem(texel.x, texel.y), max_velocity);
      }
    }
  });

  return output;
}

struct MotionRect {
  int2 bottom_left;
  int2 extent;
};

static MotionRect compute_motion_rect(int2 tile, float2 motion, int2 size)
{
  /* `ceil()` to number of tile touched. */
  int2 point1 = tile + int2(math::sign(motion) *
                            math::ceil(math::abs(motion) / float(MOTION_BLUR_TILE_SIZE)));
  int2 point2 = tile;

  int2 max_point = math::max(point1, point2);
  int2 min_point = math::min(point1, point2);
  /* Clamp to bounds. */
  max_point = math::min(max_point, size - 1);
  min_point = math::max(min_point, int2(0));

  MotionRect rect;
  rect.bottom_left = min_point;
  rect.extent = 1 + max_point - min_point;
  return rect;
}

struct MotionLine {
  /** Origin of the line. */
  float2 origin;
  /** Normal to the line direction. */
  float2 normal;
};

static MotionLine compute_motion_line(int2 tile, float2 motion)
{
  float magnitude = math::length(motion);
  float2 dir = magnitude != 0.0f ? motion / magnitude : motion;

  MotionLine line;
  line.origin = float2(tile);
  /* Rotate 90 degrees counter-clockwise. */
  line.normal = float2(-dir.y, dir.x);
  return line;
}

static bool is_inside_motion_line(int2 tile, MotionLine motion_line)
{
  /* NOTE: Everything in is tile unit. */
  float distance_to_line = math::dot(motion_line.normal, motion_line.origin - float2(tile));
  /* In order to be conservative and for simplicity, we use the tiles bounding circles.
   * Consider that both the tile and the line have bounding radius of M_SQRT1_2. */
  return math::abs(distance_to_line) < math::numbers::sqrt2_v<float>;
}

/* The max tile velocity image computes the maximum within 32x32 blocks, while the velocity can
 * in fact extend beyond such a small block. So we dilate the max blocks by taking the maximum
 * along the path of each of the max velocity tiles. Since the shader uses custom max atomics,
 * the output will be an indirection buffer that points to a particular tile in the original max
 * tile velocity image. This is done as a form of performance optimization, see the shader for
 * more information. */
static MemoryBuffer dilate_max_velocity(MemoryBuffer &max_tile_velocity, float shutter_speed)
{
  const int2 size = int2(max_tile_velocity.get_width(), max_tile_velocity.get_height());
  MemoryBuffer output(DataType::Color, size.x, size.y);
  const float4 zero_value = float4(0.0f);
  output.fill(output.get_rect(), zero_value);

  for (const int64_t y : IndexRange(size.y)) {
    for (const int64_t x : IndexRange(size.x)) {
      const int2 src_tile = int2(x, y);

      float4 max_motion = float4(max_tile_velocity.get_elem(x, y)) *
                          float4(float2(shutter_speed), float2(-shutter_speed));

      {
        /* Rectangular area (in tiles) where the motion vector spreads. */
        MotionRect motion_rect = compute_motion_rect(src_tile, max_motion.xy(), size);
        MotionLine motion_line = compute_motion_line(src_tile, max_motion.xy());
        /* Do a conservative rasterization of the line of the motion vector line. */
        for (int j = 0; j < motion_rect.extent.y; j++) {
          for (int i = 0; i < motion_rect.extent.x; i++) {
            int2 tile = motion_rect.bottom_left + int2(i, j);
            if (is_inside_motion_line(tile, motion_line)) {
              float *pixel = output.get_elem(tile.x, tile.y);
              copy_v2_v2(pixel + 2,
                         max_velocity_approximate(pixel + 2, max_motion.zw(), tile, src_tile));
              copy_v2_v2(pixel, max_velocity_approximate(pixel, max_motion.xy(), tile, src_tile));
            }
          }
        }
      }

      {
        /* Rectangular area (in tiles) where the motion vector spreads. */
        MotionRect motion_rect = compute_motion_rect(src_tile, max_motion.zw(), size);
        MotionLine motion_line = compute_motion_line(src_tile, max_motion.zw());
        /* Do a conservative rasterization of the line of the motion vector line. */
        for (int j = 0; j < motion_rect.extent.y; j++) {
          for (int i = 0; i < motion_rect.extent.x; i++) {
            int2 tile = motion_rect.bottom_left + int2(i, j);
            if (is_inside_motion_line(tile, motion_line)) {
              float *pixel = output.get_elem(tile.x, tile.y);
              copy_v2_v2(pixel, max_velocity_approximate(pixel, max_motion.xy(), tile, src_tile));
              copy_v2_v2(pixel + 2,
                         max_velocity_approximate(pixel + 2, max_motion.zw(), tile, src_tile));
            }
          }
        }
      }
    }
  }

  return output;
}

/* Interleaved gradient noise by Jorge Jimenez
 * http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare. */
static float interleaved_gradient_noise(int2 p)
{
  return math::fract(52.9829189f * math::fract(0.06711056f * p.x + 0.00583715f * p.y));
}

static float2 spread_compare(float center_motion_length,
                             float sample_motion_length,
                             float offset_length)
{
  return math::clamp(
      float2(center_motion_length, sample_motion_length) - offset_length + 1.0f, 0.0f, 1.0f);
}

static float2 depth_compare(float center_depth, float sample_depth)
{
  float2 depth_scale = float2(DEPTH_SCALE, -DEPTH_SCALE);
  return math::clamp(0.5f + depth_scale * (sample_depth - center_depth), 0.0f, 1.0f);
}

/* Kill contribution if not going the same direction. */
static float dir_compare(float2 offset, float2 sample_motion, float sample_motion_length)
{
  if (sample_motion_length < 0.5f) {
    return 1.0f;
  }
  return (math::dot(offset, sample_motion) > 0.0f) ? 1.0f : 0.0f;
}

/* Return background (x) and foreground (y) weights. */
static float2 sample_weights(float center_depth,
                             float sample_depth,
                             float center_motion_length,
                             float sample_motion_length,
                             float offset_length)
{
  /* Classify foreground/background. */
  float2 depth_weight = depth_compare(center_depth, sample_depth);
  /* Weight if sample is overlapping or under the center pixel. */
  float2 spread_weight = spread_compare(center_motion_length, sample_motion_length, offset_length);
  return depth_weight * spread_weight;
}

struct Accumulator {
  float4 fg;
  float4 bg;
  /** x: Background, y: Foreground, z: dir. */
  float3 weight;
};

static void gather_sample(MemoryBuffer *image_buffer,
                          MemoryBuffer *depth_buffer,
                          MemoryBuffer *velocity_buffer,
                          int2 size,
                          float2 screen_uv,
                          float center_depth,
                          float center_motion_len,
                          float2 offset,
                          float offset_len,
                          const bool next,
                          float shutter_speed,
                          Accumulator &accum)
{
  float2 sample_uv = screen_uv - offset / float2(size);
  float4 sample_vectors = velocity_buffer->texture_bilinear_extend(sample_uv) *
                          float4(float2(shutter_speed), float2(-shutter_speed));
  float2 sample_motion = (next) ? sample_vectors.zw() : sample_vectors.xy();
  float sample_motion_len = math::length(sample_motion);
  float sample_depth = depth_buffer->texture_bilinear_extend(sample_uv).x;
  float4 sample_color = image_buffer->texture_bilinear_extend(sample_uv);

  float2 direct_weights = sample_weights(
      center_depth, sample_depth, center_motion_len, sample_motion_len, offset_len);

  float3 weights;
  weights.x = direct_weights.x;
  weights.y = direct_weights.y;
  weights.z = dir_compare(offset, sample_motion, sample_motion_len);
  weights.x *= weights.z;
  weights.y *= weights.z;

  accum.fg += sample_color * weights.y;
  accum.bg += sample_color * weights.x;
  accum.weight += weights;
}

static void gather_blur(MemoryBuffer *image_buffer,
                        MemoryBuffer *depth_buffer,
                        MemoryBuffer *velocity_buffer,
                        int2 size,
                        float2 screen_uv,
                        float2 center_motion,
                        float center_depth,
                        float2 max_motion,
                        float ofs,
                        const bool next,
                        int samples_count,
                        float shutter_speed,
                        Accumulator &accum)
{
  float center_motion_len = math::length(center_motion);
  float max_motion_len = math::length(max_motion);

  /* Tile boundaries randomization can fetch a tile where there is less motion than this pixel.
   * Fix this by overriding the max_motion. */
  if (max_motion_len < center_motion_len) {
    max_motion_len = center_motion_len;
    max_motion = center_motion;
  }

  if (max_motion_len < 0.5f) {
    return;
  }

  int i;
  float t, inc = 1.0f / float(samples_count);
  for (i = 0, t = ofs * inc; i < samples_count; i++, t += inc) {
    gather_sample(image_buffer,
                  depth_buffer,
                  velocity_buffer,
                  size,
                  screen_uv,
                  center_depth,
                  center_motion_len,
                  max_motion * t,
                  max_motion_len * t,
                  next,
                  shutter_speed,
                  accum);
  }

  if (center_motion_len < 0.5f) {
    return;
  }

  for (i = 0, t = ofs * inc; i < samples_count; i++, t += inc) {
    /* Also sample in center motion direction.
     * Allow recovering motion where there is conflicting
     * motion between foreground and background. */
    gather_sample(image_buffer,
                  depth_buffer,
                  velocity_buffer,
                  size,
                  screen_uv,
                  center_depth,
                  center_motion_len,
                  center_motion * t,
                  center_motion_len * t,
                  next,
                  shutter_speed,
                  accum);
  }
}

static void motion_blur(MemoryBuffer *image_buffer,
                        MemoryBuffer *depth_buffer,
                        MemoryBuffer *velocity_buffer,
                        MemoryBuffer *max_velocity_buffer,
                        MemoryBuffer *output,
                        int samples_count,
                        float shutter_speed)
{
  const int2 size = int2(image_buffer->get_width(), image_buffer->get_height());
  threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(size.x)) {
        const int2 texel = int2(x, y);
        float2 uv = (float2(texel) + 0.5f) / float2(size);

        /* Data of the center pixel of the gather (target). */
        float center_depth = *depth_buffer->get_elem(x, y);
        float4 center_motion = float4(velocity_buffer->get_elem(x, y)) *
                               float4(float2(shutter_speed), float2(-shutter_speed));
        float4 center_color = image_buffer->get_elem(x, y);

        /* Randomize tile boundary to avoid ugly discontinuities. Randomize 1/4th of the tile.
         * Note this randomize only in one direction but in practice it's enough. */
        float rand = interleaved_gradient_noise(texel);
        int2 tile = (texel + int2(rand * 2.0f - 1.0f * float(MOTION_BLUR_TILE_SIZE) * 0.25f)) /
                    MOTION_BLUR_TILE_SIZE;

        /* No need to multiply by the shutter speed and invert the next velocities since this was
         * already done in dilate_max_velocity. */
        float4 max_motion = max_velocity_buffer->get_elem(tile.x, tile.y);

        Accumulator accum;
        accum.weight = float3(0.0f, 0.0f, 1.0f);
        accum.bg = float4(0.0f);
        accum.fg = float4(0.0f);
        /* First linear gather. time = [T - delta, T] */
        gather_blur(image_buffer,
                    depth_buffer,
                    velocity_buffer,
                    size,
                    uv,
                    center_motion.xy(),
                    center_depth,
                    max_motion.xy(),
                    rand,
                    false,
                    samples_count,
                    shutter_speed,
                    accum);
        /* Second linear gather. time = [T, T + delta] */
        gather_blur(image_buffer,
                    depth_buffer,
                    velocity_buffer,
                    size,
                    uv,
                    center_motion.zw(),
                    center_depth,
                    max_motion.zw(),
                    rand,
                    true,
                    samples_count,
                    shutter_speed,
                    accum);

#if 1 /* Own addition. Not present in reference implementation. */
        /* Avoid division by 0.0. */
        float w = 1.0f / (50.0f * float(samples_count) * 4.0f);
        accum.bg += center_color * w;
        accum.weight.x += w;
        /* NOTE: In Jimenez's presentation, they used center sample.
         * We use background color as it contains more information for foreground
         * elements that have not enough weights.
         * Yield better blur in complex motion. */
        center_color = accum.bg / accum.weight.x;
#endif
        /* Merge background. */
        accum.fg += accum.bg;
        accum.weight.y += accum.weight.x;
        /* Balance accumulation for failed samples.
         * We replace the missing foreground by the background. */
        float blend_fac = math::clamp(1.0f - accum.weight.y / accum.weight.z, 0.0f, 1.0f);
        float4 out_color = (accum.fg / accum.weight.z) + center_color * blend_fac;

        copy_v4_v4(output->get_elem(x, y), out_color);
      }
    }
  });
}

void VectorBlurOperation::update_memory_buffer(MemoryBuffer *output,
                                               const rcti & /*area*/,
                                               Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *image = inputs[IMAGE_INPUT_INDEX];
  MemoryBuffer *depth = inputs[DEPTH_INPUT_INDEX];
  MemoryBuffer *velocity = inputs[VELOCITY_INPUT_INDEX];

  const bool image_needs_inflation = image->is_a_single_elem();
  const bool depth_needs_inflation = depth->is_a_single_elem();
  const bool velocity_needs_inflation = velocity->is_a_single_elem();

  MemoryBuffer *image_buffer = image_needs_inflation ? image->inflate() : image;
  MemoryBuffer *depth_buffer = depth_needs_inflation ? depth->inflate() : depth;
  MemoryBuffer *velocity_buffer = velocity_needs_inflation ? velocity->inflate() : velocity;

  MemoryBuffer max_tile_velocity = compute_max_tile_velocity(velocity_buffer);
  MemoryBuffer max_velocity = dilate_max_velocity(max_tile_velocity, settings_->fac);
  motion_blur(image_buffer,
              depth_buffer,
              velocity_buffer,
              &max_velocity,
              output,
              settings_->samples,
              settings_->fac);

  if (image_needs_inflation) {
    delete image_buffer;
  }

  if (depth_needs_inflation) {
    delete depth_buffer;
  }

  if (velocity_needs_inflation) {
    delete velocity_buffer;
  }
}

void VectorBlurOperation::get_area_of_interest(const int /*input_idx*/,
                                               const rcti & /*output_area*/,
                                               rcti &r_input_area)
{
  r_input_area = this->get_canvas();
}

}  // namespace blender::compositor
