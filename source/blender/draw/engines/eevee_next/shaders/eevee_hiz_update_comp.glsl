
/**
 * Shader that down-sample depth buffer, creating a Hierarchical-Z buffer.
 * Saves max value of each 2x2 texel in the mipmap above the one we are
 * rendering to. Adapted from
 * http://rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/
 *
 * Major simplification has been made since we pad the buffer to always be
 * bigger than input to avoid mipmapping misalignment.
 *
 * Start by copying the base level by quad loading the depth.
 * Then each thread compute it's local depth for level 1.
 * After that we use shared variables to do inter thread communication and
 * downsample to max level.
 */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)

shared float local_depths[gl_WorkGroupSize.y][gl_WorkGroupSize.x];

/* Load values from the previous lod level. */
vec4 load_local_depths(ivec2 pixel)
{
  pixel *= 2;
  return vec4(local_depths[pixel.y + 1][pixel.x + 0],
              local_depths[pixel.y + 1][pixel.x + 1],
              local_depths[pixel.y + 0][pixel.x + 1],
              local_depths[pixel.y + 0][pixel.x + 0]);
}

void store_local_depth(ivec2 pixel, float depth)
{
  local_depths[pixel.y][pixel.x] = depth;
}

void main()
{
  ivec2 local_px = ivec2(gl_LocalInvocationID.xy);
  /* Bottom left corner of the kernel. */
  ivec2 kernel_origin = ivec2(gl_WorkGroupSize.xy * gl_WorkGroupID.xy);

  /* Copy level 0. */
  ivec2 src_px = ivec2(kernel_origin + local_px) * 2;
  vec2 samp_co = (vec2(src_px) + 0.5) / vec2(textureSize(depth_tx, 0));
  vec4 samp = textureGather(depth_tx, samp_co);

  if (update_mip_0) {
    imageStore(out_mip_0, src_px + ivec2(0, 1), samp.xxxx);
    imageStore(out_mip_0, src_px + ivec2(1, 1), samp.yyyy);
    imageStore(out_mip_0, src_px + ivec2(1, 0), samp.zzzz);
    imageStore(out_mip_0, src_px + ivec2(0, 0), samp.wwww);
  }

  /* Level 1. (No load) */
  float max_depth = max_v4(samp);
  ivec2 dst_px = ivec2(kernel_origin + local_px);
  imageStore(out_mip_1, dst_px, vec4(max_depth));
  store_local_depth(local_px, max_depth);

  /* Level 2-5. */
  bool active_thread;
  int mask_shift = 1;

#define downsample_level(out_mip__, lod_) \
  active_thread = all(lessThan(uvec2(local_px), gl_WorkGroupSize.xy >> uint(mask_shift))); \
  barrier(); /* Wait for previous writes to finish. */ \
  if (active_thread) { \
    max_depth = max_v4(load_local_depths(local_px)); \
    dst_px = ivec2((kernel_origin >> mask_shift) + local_px); \
    imageStore(out_mip__, dst_px, vec4(max_depth)); \
  } \
  barrier(); /* Wait for previous reads to finish. */ \
  if (active_thread) { \
    store_local_depth(local_px, max_depth); \
  } \
  mask_shift++;

  downsample_level(out_mip_2, 2);
  downsample_level(out_mip_3, 3);
  downsample_level(out_mip_4, 4);
  downsample_level(out_mip_5, 5);

  /* Since we pad the destination texture, the mip size is equal to the dispatch size. */
  uint tile_count = uint(imageSize(out_mip_5).x * imageSize(out_mip_5).y);
  /* Let the last tile handle the remaining LOD. */
  bool last_tile = atomicAdd(finished_tile_counter, 1u) + 1u < tile_count;
  if (last_tile == false) {
    return;
  }
  finished_tile_counter = 0u;

  ivec2 iter = divide_ceil(imageSize(out_mip_5), ivec2(gl_WorkGroupSize.xy * 2u));
  ivec2 image_border = imageSize(out_mip_5) - 1;
  for (int y = 0; y < iter.y; y++) {
    for (int x = 0; x < iter.x; x++) {
      /* Load result of the other work groups. */
      kernel_origin = ivec2(gl_WorkGroupSize.xy) * ivec2(x, y);
      src_px = ivec2(kernel_origin + local_px) * 2;
      vec4 samp;
      samp.x = imageLoad(out_mip_5, min(src_px + ivec2(0, 1), image_border)).x;
      samp.y = imageLoad(out_mip_5, min(src_px + ivec2(1, 1), image_border)).x;
      samp.z = imageLoad(out_mip_5, min(src_px + ivec2(1, 0), image_border)).x;
      samp.w = imageLoad(out_mip_5, min(src_px + ivec2(0, 0), image_border)).x;
      /* Level 6. */
      float max_depth = max_v4(samp);
      ivec2 dst_px = ivec2(kernel_origin + local_px);
      imageStore(out_mip_6, dst_px, vec4(max_depth));
      store_local_depth(local_px, max_depth);

      mask_shift = 1;

      /* Level 7. */
      downsample_level(out_mip_7, 7);

      /* Limited by OpenGL maximum of 8 image slot. */
      // downsample_level(out_mip_8, 8);
      // downsample_level(out_mip_9, 9);
      // downsample_level(out_mip_10, 10);
    }
  }
}
