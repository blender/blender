/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Identical to eevee_motion_blur_dilate_comp.glsl but with minor adjustments to work with the
 * compositor. */

#include "gpu_shader_compositor_motion_blur_lib.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_constants_lib.glsl"

struct MotionRect {
  int2 bottom_left;
  int2 extent;
};

MotionRect compute_motion_rect(int2 tile, float2 motion)
{
  /* `ceil()` to number of tile touched. */
  int2 point1 = tile + int2(sign(motion) * ceil(abs(motion) / float(MOTION_BLUR_TILE_SIZE)));
  int2 point2 = tile;

  int2 max_point = max(point1, point2);
  int2 min_point = min(point1, point2);
  /* Clamp to bounds. */
  max_point = min(max_point, texture_size(input_tx) - 1);
  min_point = max(min_point, int2(0));

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

MotionLine compute_motion_line(int2 tile, float2 motion)
{
  float magnitude = length(motion);
  float2 dir = magnitude != 0.0f ? motion / magnitude : motion;

  MotionLine line;
  line.origin = float2(tile);
  /* Rotate 90 degrees counter-clockwise. */
  line.normal = float2(-dir.y, dir.x);
  return line;
}

bool is_inside_motion_line(int2 tile, MotionLine motion_line)
{
  /* NOTE: Everything in is tile unit. */
  float distance_to_line = dot(motion_line.normal, motion_line.origin - float2(tile));
  /* In order to be conservative and for simplicity, we use the tiles bounding circles.
   * Consider that both the tile and the line have bounding radius of M_SQRT1_2. */
  return abs(distance_to_line) < M_SQRT2;
}

void main()
{
  int2 src_tile = int2(gl_GlobalInvocationID.xy);
  if (any(greaterThanEqual(src_tile, texture_size(input_tx)))) {
    return;
  }

  float4 max_motion = texture_load(input_tx, src_tile) *
                      float4(float2(shutter_speed), float2(-shutter_speed));

  MotionPayload payload_prv = motion_blur_tile_indirection_pack_payload(max_motion.xy,
                                                                        uint2(src_tile));
  MotionPayload payload_nxt = motion_blur_tile_indirection_pack_payload(max_motion.zw,
                                                                        uint2(src_tile));
  if (true) {
    /* Rectangular area (in tiles) where the motion vector spreads. */
    MotionRect motion_rect = compute_motion_rect(src_tile, max_motion.xy);
    MotionLine motion_line = compute_motion_line(src_tile, max_motion.xy);
    /* Do a conservative rasterization of the line of the motion vector line. */
    for (int x = 0; x < motion_rect.extent.x; x++) {
      for (int y = 0; y < motion_rect.extent.y; y++) {
        int2 tile = motion_rect.bottom_left + int2(x, y);
        if (is_inside_motion_line(tile, motion_line)) {
          motion_blur_tile_indirection_store(
              tile_indirection_buf, MOTION_PREV, uint2(tile), payload_prv);
          /* FIXME: This is a bit weird, but for some reason, we need the store the same vector in
           * the motion next so that weighting in gather pass is better. */
          motion_blur_tile_indirection_store(
              tile_indirection_buf, MOTION_NEXT, uint2(tile), payload_nxt);
        }
      }
    }
  }

  if (true) {
    MotionPayload payload = motion_blur_tile_indirection_pack_payload(max_motion.zw,
                                                                      uint2(src_tile));
    /* Rectangular area (in tiles) where the motion vector spreads. */
    MotionRect motion_rect = compute_motion_rect(src_tile, max_motion.zw);
    MotionLine motion_line = compute_motion_line(src_tile, max_motion.zw);
    /* Do a conservative rasterization of the line of the motion vector line. */
    for (int x = 0; x < motion_rect.extent.x; x++) {
      for (int y = 0; y < motion_rect.extent.y; y++) {
        int2 tile = motion_rect.bottom_left + int2(x, y);
        if (is_inside_motion_line(tile, motion_line)) {
          motion_blur_tile_indirection_store(
              tile_indirection_buf, MOTION_NEXT, uint2(tile), payload_nxt);
          /* FIXME: This is a bit weird, but for some reason, we need the store the same vector in
           * the motion next so that weighting in gather pass is better. */
          motion_blur_tile_indirection_store(
              tile_indirection_buf, MOTION_PREV, uint2(tile), payload_prv);
        }
      }
    }
  }
}
