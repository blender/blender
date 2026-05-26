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
#pragma once

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_geom_iface_info)
FRAGMENT_SHADER_CREATE_INFO(eevee_nodetree)

#include "eevee_occupancy_lib.bsl.hh"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_volume_lib.bsl.hh"

namespace eevee {

struct SurfOccupancy {
  [[legacy_info]] ShaderCreateInfo eevee_geom_iface_info;

  [[image(VOLUME_HIT_DEPTH_SLOT, write, SFLOAT_32)]] image3D hit_depth_img;
  [[image(VOLUME_HIT_COUNT_SLOT, read_write, UINT_32)]] uimage2DAtomic hit_count_img;
  [[image(VOLUME_OCCUPANCY_SLOT, read_write, UINT_32)]] uimage3DAtomic occupancy_img;

  [[push_constant]] bool use_fast_method;
};

/* Note: All fragments need to be invoked even if we write to the depth buffer.
 * So not early fragment tests. */
[[fragment]] [[texture_atomic]]
void surf_occupancy([[resource_table]] SurfOccupancy &srt,
                    [[resource_table]] const Uniform &uni,
                    [[resource_table]] const Sampling &sampling,
                    [[front_facing]] const bool front_facing,
                    [[frag_coord]] const float4 frag_co)
{
  int2 texel = int2(frag_co.xy);
  float vPz = dot(drw_view_forward(), interp.P) - dot(drw_view_forward(), drw_view_position());

  float offset = sampling.rng_1D_get(SAMPLING_VOLUME_W);
  float jitter = volume_froxel_jitter(texel, offset) * uni.uniform_buf.volumes.inv_tex_size.z;
  float volume_z = view_z_to_volume_z(uni, vPz) + jitter;

  if (srt.use_fast_method) {
    occupancy::Bits occupancy_bits = occupancy::bits_from_depth(
        volume_z, uni.uniform_buf.volumes.tex_size.z);
    for (int i = 0; i < imageSize(srt.occupancy_img).z; i++) {
      /* Negate occupancy bits before XORing so that meshes clipped by the near plane fill the
       * space between the inner part of the mesh and the near plane.
       * It doesn't change anything for closed meshes. */
      occupancy_bits.bits[i] = ~occupancy_bits.bits[i];
      if (occupancy_bits.bits[i] != 0u) {
        imageAtomicXor(srt.occupancy_img, int3(texel, i), occupancy_bits.bits[i]);
      }
    }
  }
  else {
    if (volume_z > 0.0f) {
      uint hit_id = imageAtomicAdd(srt.hit_count_img, texel, 1u);
      if (hit_id < VOLUME_HIT_DEPTH_MAX) {
        float value = front_facing ? volume_z : -volume_z;
        imageStore(srt.hit_depth_img, int3(texel, int(hit_id)), float4(value));
      }
    }
  }
}

}  // namespace eevee
