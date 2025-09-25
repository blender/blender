/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shaders that down-sample velocity buffer into squared tile of MB_TILE_DIVISOR pixels wide.
 * Outputs the largest motion vector in the tile area.
 * Also perform velocity resolve to speedup the convolution pass.
 *
 * Based on:
 * A Fast and Stable Feature-Aware Motion Blur Filter
 * by Jean-Philippe Guertin, Morgan McGuire, Derek Nowrouzezahrai
 *
 * Adapted from G3D Innovation Engine implementation.
 */

#include "infos/eevee_motion_blur_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_motion_blur_tiles_flatten_rgba)

#include "draw_math_geom_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "eevee_velocity_lib.glsl"

shared uint payload_prev;
shared uint payload_next;
shared float2 max_motion_prev;
shared float2 max_motion_next;

/* Store velocity magnitude in the MSB and thread id in the LSB. */
uint pack_payload(float2 motion, uint2 thread_id)
{
  /* NOTE: We clamp max velocity to 16k pixels. */
  return (min(uint(ceil(length(motion))), 0xFFFFu) << 16u) | (thread_id.y << 8) | thread_id.x;
}

/* Return thread index from the payload. */
uint2 unpack_payload(uint payload)
{
  return uint2(payload & 0xFFu, (payload >> 8) & 0xFFu);
}

void main()
{
  if (gl_LocalInvocationIndex == 0u) {
    payload_prev = 0u;
    payload_next = 0u;
  }
  barrier();

  uint local_payload_prev = 0u;
  uint local_payload_next = 0u;
  float2 local_max_motion_prev;
  float2 local_max_motion_next;

  int2 texel = min(int2(gl_GlobalInvocationID.xy), imageSize(velocity_img) - 1);

  float2 render_size = float2(imageSize(velocity_img).xy);
  float2 uv = (float2(texel) + 0.5f) / render_size;
  float depth = reverse_z::read(texelFetch(depth_tx, texel, 0).r);
  float4 motion = velocity_resolve(imageLoad(velocity_img, texel), uv, depth);
#ifdef FLATTEN_RG
  /* imageLoad does not perform the swizzling like sampler does. Do it manually. */
  motion = motion.xyxy;
#endif

  /* Store resolved velocity to speedup the gather pass. Out of bounds writes are ignored.
   * Unfortunately, we cannot convert to pixel space here since it is also used by TAA and the
   * motion blur needs to remain optional. */
  imageStore(velocity_img, int2(gl_GlobalInvocationID.xy), velocity_pack(motion));
  /* Clip velocity to viewport bounds (in NDC space). */
  float2 line_clip;
  line_clip.x = line_unit_square_intersect_dist_safe(uv * 2.0f - 1.0f, motion.xy * 2.0f);
  line_clip.y = line_unit_square_intersect_dist_safe(uv * 2.0f - 1.0f, -motion.zw * 2.0f);
  motion *= min(line_clip, float2(1.0f)).xxyy;
  /* Convert to pixel space. Note this is only for velocity tiles. */
  motion *= render_size.xyxy;
  /* Rescale to shutter relative motion for viewport. */
  motion *= motion_blur_buf.motion_scale.xxyy;

  uint sample_payload_prev = pack_payload(motion.xy, gl_LocalInvocationID.xy);
  if (local_payload_prev < sample_payload_prev) {
    local_payload_prev = sample_payload_prev;
    local_max_motion_prev = motion.xy;
  }

  uint sample_payload_next = pack_payload(motion.zw, gl_LocalInvocationID.xy);
  if (local_payload_next < sample_payload_next) {
    local_payload_next = sample_payload_next;
    local_max_motion_next = motion.zw;
  }

  /* Compare the local payload with the other threads. */
  atomicMax(payload_prev, local_payload_prev);
  atomicMax(payload_next, local_payload_next);
  barrier();

  /* Need to broadcast the result to another thread in order to issue a unique write. */
  if (all(equal(unpack_payload(payload_prev), gl_LocalInvocationID.xy))) {
    max_motion_prev = local_max_motion_prev;
  }
  if (all(equal(unpack_payload(payload_next), gl_LocalInvocationID.xy))) {
    max_motion_next = local_max_motion_next;
  }
  barrier();

  if (gl_LocalInvocationIndex == 0u) {
    int2 tile_co = int2(gl_WorkGroupID.xy);
    imageStore(out_tiles_img, tile_co, float4(max_motion_prev, max_motion_next));
  }
}
