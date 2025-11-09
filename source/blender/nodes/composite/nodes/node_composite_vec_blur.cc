/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <cstdint>

#include "BLI_index_range.hh"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "UI_resources.hh"

#include "GPU_compute.hh"
#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_storage_buffer.hh"
#include "GPU_vertex_buffer.hh"

#include "COM_node_operation.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** VECTOR BLUR ******************** */

namespace blender::nodes::node_composite_vec_blur_cc {

static void cmp_node_vec_blur_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Vector>("Speed")
      .dimensions(4)
      .default_value({0.0f, 0.0f, 0.0f})
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_VELOCITY)
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Float>("Z").default_value(0.0f).min(0.0f).structure_type(
      StructureType::Dynamic);
  b.add_input<decl::Int>("Samples").default_value(32).min(1).max(256).description(
      "The number of samples used to approximate the motion blur");
  b.add_input<decl::Float>("Shutter").default_value(0.5f).min(0.0f).description(
      "Time between shutter opening and closing in frames");
}

using namespace blender::compositor;

#define MOTION_BLUR_TILE_SIZE 32
#define DEPTH_SCALE 100.0f

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
 * atomic integer operations, hence the limited precision. See velocity_atomic_max_value for more
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
static Result compute_max_tile_velocity_cpu(Context &context, const Result &velocity_image)
{
  if (velocity_image.is_single_value()) {
    Result output = context.create_result(ResultType::Float4);
    output.allocate_single_value();
    output.set_single_value(velocity_image.get_single_value<float4>());
    return output;
  }

  const int2 tile_size = int2(MOTION_BLUR_TILE_SIZE);
  const int2 velocity_size = velocity_image.domain().size;
  const int2 tiles_count = math::divide_ceil(velocity_size, tile_size);
  Result output = context.create_result(ResultType::Float4);
  output.allocate_texture(Domain(tiles_count));

  parallel_for(tiles_count, [&](const int2 texel) {
    float2 max_previous_velocity = float2(0.0f);
    float2 max_next_velocity = float2(0.0f);

    for (int j = 0; j < tile_size.y; j++) {
      for (int i = 0; i < tile_size.x; i++) {
        int2 sub_texel = texel * tile_size + int2(i, j);
        const float4 velocity = velocity_image.load_pixel_extended<float4>(sub_texel);
        max_previous_velocity = max_velocity(velocity.xy(), max_previous_velocity);
        max_next_velocity = max_velocity(velocity.zw(), max_next_velocity);
      }
    }

    const float4 max_velocity = float4(max_previous_velocity, max_next_velocity);
    output.store_pixel(texel, max_velocity);
  });

  return output;
}

struct MotionRect {
  int2 bottom_left;
  int2 extent;
};

static MotionRect compute_motion_rect(const int2 &tile, const float2 &motion, const int2 &size)
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

static MotionLine compute_motion_line(const int2 &tile, const float2 &motion)
{
  float magnitude = math::length(motion);
  float2 dir = magnitude != 0.0f ? motion / magnitude : motion;

  MotionLine line;
  line.origin = float2(tile);
  /* Rotate 90 degrees counter-clockwise. */
  line.normal = float2(-dir.y, dir.x);
  return line;
}

static bool is_inside_motion_line(const int2 &tile, const MotionLine &motion_line)
{
  /* NOTE: Everything in is tile unit. */
  float distance_to_line = math::dot(motion_line.normal, motion_line.origin - float2(tile));
  /* In order to be conservative and for simplicity, we use the tiles bounding circles.
   * Consider that both the tile and the line have bounding radius of M_SQRT1_2. */
  return math::abs(distance_to_line) < math::numbers::sqrt2_v<float>;
}

/* The max tile velocity image computes the maximum within 32x32 blocks, while the velocity can
 * in fact extend beyond such a small block. So we dilate the max blocks by taking the maximum
 * along the path of each of the max velocity tiles. */
static Result dilate_max_velocity_cpu(Context &context,
                                      const Result &max_tile_velocity,
                                      const float shutter_speed)
{
  if (max_tile_velocity.is_single_value()) {
    Result output = context.create_result(ResultType::Float4);
    output.allocate_single_value();
    output.set_single_value(max_tile_velocity.get_single_value<float4>());
    return output;
  }

  const int2 size = max_tile_velocity.domain().size;
  Result output = context.create_result(ResultType::Float4);
  output.allocate_texture(Domain(size));

  parallel_for(size, [&](const int2 texel) { output.store_pixel(texel, float4(0.0f)); });

  for (const int64_t y : IndexRange(size.y)) {
    for (const int64_t x : IndexRange(size.x)) {
      const int2 src_tile = int2(x, y);

      const float4 max_motion = max_tile_velocity.load_pixel<float4>(src_tile);
      const float2 max_previous_velocity = max_motion.xy() * shutter_speed;
      const float2 max_next_velocity = max_motion.zw() * -shutter_speed;

      {
        /* Rectangular area (in tiles) where the motion vector spreads. */
        MotionRect motion_rect = compute_motion_rect(src_tile, max_previous_velocity, size);
        MotionLine motion_line = compute_motion_line(src_tile, max_previous_velocity);
        /* Do a conservative rasterization of the line of the motion vector line. */
        for (int j = 0; j < motion_rect.extent.y; j++) {
          for (int i = 0; i < motion_rect.extent.x; i++) {
            int2 tile = motion_rect.bottom_left + int2(i, j);
            if (is_inside_motion_line(tile, motion_line)) {
              const float4 current_max_velocity = output.load_pixel<float4>(tile);
              const float2 new_max_previous_velocity = max_velocity_approximate(
                  current_max_velocity.xy(), max_previous_velocity, tile, src_tile);
              const float2 new_max_next_velocity = max_velocity_approximate(
                  current_max_velocity.zw(), max_next_velocity, tile, src_tile);
              output.store_pixel(tile, float4(new_max_previous_velocity, new_max_next_velocity));
            }
          }
        }
      }

      {
        /* Rectangular area (in tiles) where the motion vector spreads. */
        MotionRect motion_rect = compute_motion_rect(src_tile, max_next_velocity, size);
        MotionLine motion_line = compute_motion_line(src_tile, max_next_velocity);
        /* Do a conservative rasterization of the line of the motion vector line. */
        for (int j = 0; j < motion_rect.extent.y; j++) {
          for (int i = 0; i < motion_rect.extent.x; i++) {
            int2 tile = motion_rect.bottom_left + int2(i, j);
            if (is_inside_motion_line(tile, motion_line)) {
              const float4 current_max_velocity = output.load_pixel<float4>(tile);
              const float2 new_max_previous_velocity = max_velocity_approximate(
                  current_max_velocity.xy(), max_previous_velocity, tile, src_tile);
              const float2 new_max_next_velocity = max_velocity_approximate(
                  current_max_velocity.zw(), max_next_velocity, tile, src_tile);
              output.store_pixel(tile, float4(new_max_previous_velocity, new_max_next_velocity));
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
static float interleaved_gradient_noise(const int2 &p)
{
  return math::fract(52.9829189f * math::fract(0.06711056f * p.x + 0.00583715f * p.y));
}

static float2 spread_compare(const float center_motion_length,
                             const float sample_motion_length,
                             const float offset_length)
{
  return math::clamp(
      float2(center_motion_length, sample_motion_length) - offset_length + 1.0f, 0.0f, 1.0f);
}

static float2 depth_compare(const float center_depth, const float sample_depth)
{
  float2 depth_scale = float2(DEPTH_SCALE, -DEPTH_SCALE);
  return math::clamp(0.5f + depth_scale * (sample_depth - center_depth), 0.0f, 1.0f);
}

/* Kill contribution if not going the same direction. */
static float dir_compare(const float2 &offset,
                         const float2 &sample_motion,
                         const float &sample_motion_length)
{
  if (sample_motion_length < 0.5f) {
    return 1.0f;
  }
  return (math::dot(offset, sample_motion) > 0.0f) ? 1.0f : 0.0f;
}

/* Return background (x) and foreground (y) weights. */
static float2 sample_weights(const float center_depth,
                             const float sample_depth,
                             const float center_motion_length,
                             const float sample_motion_length,
                             const float offset_length)
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

static void gather_sample(const Result &input_image,
                          const Result &input_depth,
                          const Result &input_velocity,
                          const int2 &size,
                          const float2 &screen_uv,
                          const float center_depth,
                          const float center_motion_len,
                          const float2 &offset,
                          const float offset_len,
                          const bool next,
                          const float shutter_speed,
                          Accumulator &accum)
{
  float2 sample_uv = screen_uv - offset / float2(size);
  float4 sample_vectors = input_velocity.sample_bilinear_extended(sample_uv) *
                          float4(float2(shutter_speed), float2(-shutter_speed));
  float2 sample_motion = (next) ? sample_vectors.zw() : sample_vectors.xy();
  float sample_motion_len = math::length(sample_motion);
  float sample_depth = input_depth.sample_bilinear_extended(sample_uv).x;
  float4 sample_color = input_image.sample_bilinear_extended(sample_uv);

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

static void gather_blur(const Result &input_image,
                        const Result &input_depth,
                        const Result &input_velocity,
                        const int2 &size,
                        const float2 &screen_uv,
                        const float2 &center_motion,
                        const float center_depth,
                        const float2 &max_motion,
                        const float ofs,
                        const bool next,
                        const int samples_count,
                        const float shutter_speed,
                        Accumulator &accum)
{
  float center_motion_len = math::length(center_motion);
  float max_motion_len = math::length(max_motion);

  /* Tile boundaries randomization can fetch a tile where there is less motion than this pixel.
   * Fix this by overriding the max_motion. */
  float2 sanitized_max_motion = max_motion;
  if (max_motion_len < center_motion_len) {
    max_motion_len = center_motion_len;
    sanitized_max_motion = center_motion;
  }

  if (max_motion_len < 0.5f) {
    return;
  }

  int i;
  float t, inc = 1.0f / float(samples_count);
  for (i = 0, t = ofs * inc; i < samples_count; i++, t += inc) {
    gather_sample(input_image,
                  input_depth,
                  input_velocity,
                  size,
                  screen_uv,
                  center_depth,
                  center_motion_len,
                  sanitized_max_motion * t,
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
    gather_sample(input_image,
                  input_depth,
                  input_velocity,
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

static void motion_blur_cpu(const Result &input_image,
                            const Result &input_depth,
                            const Result &input_velocity,
                            const Result &max_velocity,
                            Result &output,
                            const int samples_count,
                            const float shutter_speed)
{
  const int2 size = input_image.domain().size;
  threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(size.x)) {
        const int2 texel = int2(x, y);
        float2 uv = (float2(texel) + 0.5f) / float2(size);

        /* Data of the center pixel of the gather (target). */
        float center_depth = input_depth.load_pixel<float, true>(texel);
        float4 center_motion = input_velocity.load_pixel<float4, true>(texel);
        float2 center_previous_motion = center_motion.xy() * shutter_speed;
        float2 center_next_motion = center_motion.zw() * -shutter_speed;
        float4 center_color = float4(input_image.load_pixel<Color>(texel));

        /* Randomize tile boundary to avoid ugly discontinuities. Randomize 1/4th of the tile.
         * Note this randomize only in one direction but in practice it's enough. */
        float rand = interleaved_gradient_noise(texel);
        int2 tile = (texel + int2(rand * 2.0f - 1.0f * float(MOTION_BLUR_TILE_SIZE) * 0.25f)) /
                    MOTION_BLUR_TILE_SIZE;

        /* No need to multiply by the shutter speed and invert the next velocities since this was
         * already done in dilate_max_velocity. */
        float4 max_motion = max_velocity.load_pixel<float4, true>(tile);

        Accumulator accum;
        accum.weight = float3(0.0f, 0.0f, 1.0f);
        accum.bg = float4(0.0f);
        accum.fg = float4(0.0f);
        /* First linear gather. time = [T - delta, T] */
        gather_blur(input_image,
                    input_depth,
                    input_velocity,
                    size,
                    uv,
                    center_previous_motion,
                    center_depth,
                    max_motion.xy(),
                    rand,
                    false,
                    samples_count,
                    shutter_speed,
                    accum);
        /* Second linear gather. time = [T, T + delta] */
        gather_blur(input_image,
                    input_depth,
                    input_velocity,
                    size,
                    uv,
                    center_next_motion,
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

        output.store_pixel(texel, Color(out_color));
      }
    }
  });
}

class VectorBlurOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input("Image");
    if (input.is_single_value()) {
      Result &output = this->get_result("Image");
      output.share_data(input);
      return;
    }

    if (this->context().use_gpu()) {
      this->execute_gpu();
    }
    else {
      this->execute_cpu();
    }
  }

  void execute_gpu()
  {
    Result max_tile_velocity = this->compute_max_tile_velocity();
    gpu::StorageBuf *tile_indirection_buffer = this->dilate_max_velocity(max_tile_velocity);
    this->compute_motion_blur(max_tile_velocity, tile_indirection_buffer);
    max_tile_velocity.release();
    GPU_storagebuf_free(tile_indirection_buffer);
  }

  /* Reduces each 32x32 block of velocity pixels into a single velocity whose magnitude is largest.
   * Each of the previous and next velocities are reduces independently. */
  Result compute_max_tile_velocity()
  {
    gpu::Shader *shader = context().get_shader("compositor_max_velocity");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "is_initial_reduction", true);

    Result &input = get_input("Speed");
    input.bind_as_texture(shader, "input_tx");

    Result output = context().create_result(ResultType::Float4);
    const int2 tiles_count = math::divide_ceil(input.domain().size, int2(32));
    output.allocate_texture(Domain(tiles_count));
    output.bind_as_image(shader, "output_img");

    GPU_compute_dispatch(shader, tiles_count.x, tiles_count.y, 1);

    GPU_shader_unbind();
    input.unbind_as_texture();
    output.unbind_as_image();

    return output;
  }

  /* The max tile velocity image computes the maximum within 32x32 blocks, while the velocity can
   * in fact extend beyond such a small block. So we dilate the max blocks by taking the maximum
   * along the path of each of the max velocity tiles. Since the shader uses custom max atomics,
   * the output will be an indirection buffer that points to a particular tile in the original max
   * tile velocity image. This is done as a form of performance optimization, see the shader for
   * more information. */
  gpu::StorageBuf *dilate_max_velocity(Result &max_tile_velocity)
  {
    gpu::Shader *shader = context().get_shader("compositor_motion_blur_max_velocity_dilate");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "shutter_speed", this->get_shutter());

    max_tile_velocity.bind_as_texture(shader, "input_tx");

    /* The shader assumes a maximum input size of 16k, and since the max tile velocity image is
     * composed of blocks of 32, we get 16k / 32 = 512. So the table is 512x512, but we store two
     * tables for the previous and next velocities, so we double that. */
    const int size = sizeof(uint32_t) * 512 * 512 * 2;
    gpu::StorageBuf *tile_indirection_buffer = GPU_storagebuf_create_ex(
        size, nullptr, GPU_USAGE_DEVICE_ONLY, __func__);
    GPU_storagebuf_clear_to_zero(tile_indirection_buffer);
    const int slot = GPU_shader_get_ssbo_binding(shader, "tile_indirection_buf");
    GPU_storagebuf_bind(tile_indirection_buffer, slot);

    compute_dispatch_threads_at_least(shader, max_tile_velocity.domain().size);

    GPU_shader_unbind();
    max_tile_velocity.unbind_as_texture();
    GPU_storagebuf_unbind(tile_indirection_buffer);

    return tile_indirection_buffer;
  }

  void compute_motion_blur(Result &max_tile_velocity, gpu::StorageBuf *tile_indirection_buffer)
  {
    gpu::Shader *shader = context().get_shader("compositor_motion_blur");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "samples_count", this->get_samples_count());
    GPU_shader_uniform_1f(shader, "shutter_speed", this->get_shutter());

    Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    Result &depth = get_input("Z");
    depth.bind_as_texture(shader, "depth_tx");

    Result &velocity = get_input("Speed");
    velocity.bind_as_texture(shader, "velocity_tx");

    max_tile_velocity.bind_as_texture(shader, "max_velocity_tx");

    GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
    const int slot = GPU_shader_get_ssbo_binding(shader, "tile_indirection_buf");
    GPU_storagebuf_bind(tile_indirection_buffer, slot);

    Result &output = get_result("Image");
    const Domain domain = compute_domain();
    output.allocate_texture(domain);
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, output.domain().size);

    GPU_shader_unbind();
    input.unbind_as_texture();
    depth.unbind_as_texture();
    velocity.unbind_as_texture();
    max_tile_velocity.unbind_as_texture();
    output.unbind_as_image();
  }

  void execute_cpu()
  {
    const float shutter_speed = this->get_shutter();
    const int samples_count = this->get_samples_count();

    const Result &input_image = get_input("Image");
    const Result &input_depth = get_input("Z");
    const Result &input_velocity = get_input("Speed");

    Result &output = get_result("Image");
    const Domain domain = compute_domain();
    output.allocate_texture(domain);

    Result max_tile_velocity = compute_max_tile_velocity_cpu(this->context(), input_velocity);
    Result max_velocity = dilate_max_velocity_cpu(
        this->context(), max_tile_velocity, shutter_speed);
    max_tile_velocity.release();
    motion_blur_cpu(input_image,
                    input_depth,
                    input_velocity,
                    max_velocity,
                    output,
                    samples_count,
                    shutter_speed);
    max_velocity.release();
  }

  int get_samples_count()
  {
    return math::clamp(this->get_input("Samples").get_single_value_default(32), 1, 256);
  }

  float get_shutter()
  {
    /* Divide by two since the motion blur algorithm expects shutter per motion step and has two
     * motion steps, while the user inputs the entire shutter across all steps. */
    return math::max(0.0f, this->get_input("Shutter").get_single_value_default(0.5f)) / 2.0f;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new VectorBlurOperation(context, node);
}

}  // namespace blender::nodes::node_composite_vec_blur_cc

static void register_node_type_cmp_vecblur()
{
  namespace file_ns = blender::nodes::node_composite_vec_blur_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeVecBlur", CMP_NODE_VECBLUR);
  ntype.ui_name = "Vector Blur";
  ntype.ui_description = "Uses the vector speed render pass to blur the image pixels in 2D";
  ntype.enum_name_legacy = "VECBLUR";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_vec_blur_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_vecblur)
