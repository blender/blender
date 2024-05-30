/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Select tile that have visible SSS effect and prepare the intermediate buffers for faster
 * processing.
 */

#pragma BLENDER_REQUIRE(gpu_shader_shared_exponent_lib.glsl)
#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)

shared uint has_visible_sss;

void main(void)
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  if (gl_LocalInvocationIndex == 0u) {
    has_visible_sss = 0u;
  }

  barrier();

  GBufferReader gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel);

  if (gbuffer_closure_get(gbuf, 0).type == CLOSURE_BSSRDF_BURLEY_ID) {
    vec3 radiance = rgb9e5_decode(imageLoad(direct_light_img, texel).r);
    radiance += imageLoad(indirect_light_img, texel).rgb;

    ClosureSubsurface closure = to_closure_subsurface(gbuffer_closure_get(gbuf, 0));
    float max_radius = reduce_max(closure.sss_radius);

    imageStore(radiance_img, texel, vec4(radiance, 0.0));
    imageStore(object_id_img, texel, uvec4(gbuf.object_id));

    vec2 center_uv = (vec2(texel) + 0.5) / vec2(textureSize(gbuf_header_tx, 0));
    float depth = texelFetch(depth_tx, texel, 0).r;
    /* TODO(fclem): Check if this simplifies. */
    float vPz = drw_depth_screen_to_view(depth);
    float homcoord = ProjectionMatrix[2][3] * vPz + ProjectionMatrix[3][3];
    float sample_scale = ProjectionMatrix[0][0] * (0.5 * max_radius / homcoord);
    float pixel_footprint = sample_scale * float(textureSize(gbuf_header_tx, 0).x);
    if (pixel_footprint > 1.0) {
      /* Race condition doesn't matter here. */
      has_visible_sss = 1u;
    }
  }
  else {
    /* No need to write radiance_img since the radiance won't be used at all. */
    imageStore(object_id_img, texel, uvec4(0));
  }

  barrier();

  if (gl_LocalInvocationIndex == 0u) {
    if (has_visible_sss > 0u) {
      uint tile_id = atomicAdd(convolve_dispatch_buf.num_groups_x, 1u);
      convolve_tile_buf[tile_id] = packUvec2x16(gl_WorkGroupID.xy);
      /* Race condition doesn't matter here. */
      convolve_dispatch_buf.num_groups_y = 1;
      convolve_dispatch_buf.num_groups_z = 1;
    }
  }
}
