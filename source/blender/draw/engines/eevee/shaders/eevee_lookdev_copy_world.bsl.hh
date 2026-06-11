/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_light_shared.hh"
#include "eevee_lightprobe_sphere.bsl.hh"
#include "eevee_spherical_harmonics.bsl.hh"

namespace eevee::lookdev {

struct CopyWorld {
  [[storage(0, read)]] const SphereProbeHarmonic &in_sh;
  [[storage(1, write)]] SphereProbeHarmonic &out_sh;
  /* WORKAROUND: The no_restrict flag is here to workaround an NVidia linker quirk (see #134239).*/
  [[storage(2, read_no_restrict)]] const LightData &in_sun;
  [[storage(3, write)]] LightData &out_sun;
  [[push_constant]] const int4 read_coord_packed;
  [[push_constant]] const int4 write_coord_mip0_packed;
  [[push_constant]] const int4 write_coord_mip1_packed;
  [[push_constant]] const int4 write_coord_mip2_packed;
  [[push_constant]] const int4 write_coord_mip3_packed;
  [[push_constant]] const int4 write_coord_mip4_packed;
  [[push_constant]] const float4x4 lookdev_rotation;
  [[sampler(0)]] sampler2DArray in_sphere_tx;
  [[image(0, write, SPHERE_PROBE_FORMAT)]] image2DArray out_sphere_mip0;
  [[image(1, write, SPHERE_PROBE_FORMAT)]] image2DArray out_sphere_mip1;
  [[image(2, write, SPHERE_PROBE_FORMAT)]] image2DArray out_sphere_mip2;
  [[image(3, write, SPHERE_PROBE_FORMAT)]] image2DArray out_sphere_mip3;
  [[image(4, write, SPHERE_PROBE_FORMAT)]] image2DArray out_sphere_mip4;

  float4 get_mip_data(int mip,
                      int2 texel,
                      SphereProbeUvArea read_coord,
                      SphereProbePixelArea write_coord,
                      float3x3 rotation_mat)
  {
    float3 rotated_ws_direction = lightprobe::sphere::texel_to_direction(float2(texel),
                                                                         write_coord);
    /* Multiplying with the inverse (which is also the transposed given the rotation matrix is
     * orthonormal) as we want the reversed transform. */
    float3 original_ws_direction = transpose(rotation_mat) * rotated_ws_direction;
    float2 uv = lightprobe::sphere::direction_to_uv(original_ws_direction, float(mip), read_coord);
    return textureLod(in_sphere_tx, float3(uv, read_coord.layer), float(mip));
  }
};

/**
 * This shader is used for:
 * - copying the world space lighting just after it has been extracted.
 * - rotate the world space lighting into the view following lighting before each frame.
 */
[[compute, local_size(SPHERE_PROBE_REMAP_GROUP_SIZE, SPHERE_PROBE_REMAP_GROUP_SIZE)]]
void copy_world_main([[resource_table]] CopyWorld &srt,
                     [[global_invocation_id]] const uint3 global_id)
{
  float3x3 rotation_mat = to_float3x3(srt.lookdev_rotation);
  if (all(equal(global_id.xy, uint2(0)))) {
    {
      SphericalHarmonicL1<float4> sh;
      sh.L0.M0 = srt.in_sh.L0_M0;
      sh.L1.M0 = srt.in_sh.L1_M0;
      sh.L1.Mn1 = srt.in_sh.L1_Mn1;
      sh.L1.Mp1 = srt.in_sh.L1_Mp1;
      sh = spherical_harmonics::rotate(rotation_mat, sh);
      srt.out_sh.L0_M0 = sh.L0.M0;
      srt.out_sh.L1_M0 = sh.L1.M0;
      srt.out_sh.L1_Mn1 = sh.L1.Mn1;
      srt.out_sh.L1_Mp1 = sh.L1.Mp1;
    }
    {
      LightData sun = srt.in_sun;
      float3x3 sun_direction = to_float3x3(transform_to_matrix(sun.object_to_world));
      sun_direction = rotation_mat * sun_direction;
      sun.object_to_world = transform_from_matrix(to_float4x4(sun_direction));
      srt.out_sun = sun;
    }
  }

  int2 local_texel = int2(global_id.xy);
  SphereProbeUvArea read_coord = reinterpret_as_atlas_coord(srt.read_coord_packed);
  SphereProbePixelArea write_coord;
  /* Exit when pixel being written doesn't fit in the area reserved for the probe. */
  /* We cannot put the following code in a loop because of the different image accesses.
   * So we have to manually unroll the loop. */
  write_coord = reinterpret_as_write_coord(srt.write_coord_mip0_packed);
  if (any(greaterThanEqual(local_texel, int2(write_coord.extent)))) {
    return;
  }
  imageStore(srt.out_sphere_mip0,
             int3(local_texel, write_coord.layer),
             srt.get_mip_data(0, local_texel, read_coord, write_coord, rotation_mat));

  write_coord = reinterpret_as_write_coord(srt.write_coord_mip1_packed);
  if (any(greaterThanEqual(local_texel, int2(write_coord.extent)))) {
    return;
  }
  imageStore(srt.out_sphere_mip1,
             int3(local_texel, write_coord.layer),
             srt.get_mip_data(1, local_texel, read_coord, write_coord, rotation_mat));

  write_coord = reinterpret_as_write_coord(srt.write_coord_mip2_packed);
  if (any(greaterThanEqual(local_texel, int2(write_coord.extent)))) {
    return;
  }
  imageStore(srt.out_sphere_mip2,
             int3(local_texel, write_coord.layer),
             srt.get_mip_data(2, local_texel, read_coord, write_coord, rotation_mat));

  write_coord = reinterpret_as_write_coord(srt.write_coord_mip3_packed);
  if (any(greaterThanEqual(local_texel, int2(write_coord.extent)))) {
    return;
  }
  imageStore(srt.out_sphere_mip3,
             int3(local_texel, write_coord.layer),
             srt.get_mip_data(3, local_texel, read_coord, write_coord, rotation_mat));

  write_coord = reinterpret_as_write_coord(srt.write_coord_mip4_packed);
  if (any(greaterThanEqual(local_texel, int2(write_coord.extent)))) {
    return;
  }
  imageStore(srt.out_sphere_mip4,
             int3(local_texel, write_coord.layer),
             srt.get_mip_data(4, local_texel, read_coord, write_coord, rotation_mat));

#ifdef GLSL_CPP_STUBS
  /* If this mismatches, manual unroll needs to be updated. */
  static_assert(SPHERE_PROBE_MIPMAP_LEVELS == 5);
#endif
}

}  // namespace eevee::lookdev

PipelineCompute eevee_lookdev_copy_world(eevee::lookdev::copy_world_main);
