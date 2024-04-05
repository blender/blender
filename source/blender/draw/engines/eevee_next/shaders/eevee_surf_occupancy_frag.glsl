/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Pre-pass that voxelizes an object on frustum aligned voxels.
 *
 * There is two method available:
 *
 * - Fast method: For each fragment we compute the amount of
 *   froxels center in-front of it. We then convert that
 *   into occupancy bitmask that we apply to the occupancy
 *   texture using imageAtomicXor. This flips the bit for each
 *   surfaces encountered along the camera ray.
 *   This is straight forward and works well for any manifold
 *   geometry.
 *
 * - Accurate method:
 *   For each fragment we write the fragment depth
 *   in a list (contained in one array texture). This list
 *   is then processed by a full-screen pass (see
 *   `eevee_occupancy_convert_frag.glsl`) that sorts and
 *   converts all the hits to the occupancy bits. This
 *   emulate Cycles behavior by considering only back-face
 *   hits as exit events and front-face hits as entry events.
 *   The result stores it to the occupancy texture using
 *   bit-wise OR operation to compose it with other non-hit
 *   list objects. This also decouple the hit-list evaluation
 *   complexity from the material evaluation shader.
 *
 */

#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_velocity_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_volume_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_occupancy_lib.glsl)

vec4 closure_to_rgba(Closure cl)
{
  return vec4(0.0);
}

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);
  float vPz = dot(drw_view_forward(), interp.P) - dot(drw_view_forward(), drw_view_position());

  float offset = sampling_rng_1D_get(SAMPLING_VOLUME_W);
  float jitter = volume_froxel_jitter(texel, offset) * uniform_buf.volumes.inv_tex_size.z;
  float volume_z = view_z_to_volume_z(vPz) + jitter;

  if (use_fast_method) {
    OccupancyBits occupancy_bits = occupancy_from_depth(volume_z, uniform_buf.volumes.tex_size.z);
    for (int i = 0; i < imageSize(occupancy_img).z; i++) {
      /* Negate occupancy bits before XORing so that meshes clipped by the near plane fill the
       * space between the inner part of the mesh and the near plane.
       * It doesn't change anything for closed meshes. */
      occupancy_bits.bits[i] = ~occupancy_bits.bits[i];
      if (occupancy_bits.bits[i] != 0u) {
        imageAtomicXor(occupancy_img, ivec3(texel, i), occupancy_bits.bits[i]);
      }
    }
  }
  else {
    uint hit_id = imageAtomicAdd(hit_count_img, texel, 1u);
    if (hit_id < VOLUME_HIT_DEPTH_MAX) {
      float value = gl_FrontFacing ? volume_z : -volume_z;
      imageStore(hit_depth_img, ivec3(texel, hit_id), vec4(value));
    }
  }
}
