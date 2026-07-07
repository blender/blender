/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * This shader is used for:
 * - copying the world space lighting just after it has been extracted.
 * - rotate the world space lighting into the view following lighting before each frame.
 */

#include "infos/eevee_lookdev_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_lookdev_copy_world)

#include "eevee_lightprobe_sphere_mapping_lib.glsl"
#include "eevee_spherical_harmonics_lib.glsl"

float4 get_mip_data(int mip,
                    int2 texel,
                    SphereProbeUvArea read_coord,
                    SphereProbePixelArea write_coord,
                    float3x3 rotation_mat)
{
  float3 rotated_ws_direction = sphere_probe_texel_to_direction(
      float2(texel), write_coord, read_coord);
  /* Multiplying with the inverse (which is also the transposed given the rotation matrix is
   * orthonormal) as we want the reversed transform. */
  float3 original_ws_direction = transpose(rotation_mat) * rotated_ws_direction;
  float2 uv = sphere_probe_direction_to_uv(original_ws_direction, float(mip), read_coord);
  return textureLod(in_sphere_tx, float3(uv, read_coord.layer), float(mip));
}

void main()
{
  float3x3 rotation_mat = to_float3x3(lookdev_rotation);
  if (all(equal(gl_GlobalInvocationID.xy, uint2(0)))) {
    {
      SphericalHarmonicL1 sh;
      sh.L0.M0 = in_sh.L0_M0;
      sh.L1.M0 = in_sh.L1_M0;
      sh.L1.Mn1 = in_sh.L1_Mn1;
      sh.L1.Mp1 = in_sh.L1_Mp1;
      sh = spherical_harmonics_rotate(rotation_mat, sh);
      out_sh.L0_M0 = sh.L0.M0;
      out_sh.L1_M0 = sh.L1.M0;
      out_sh.L1_Mn1 = sh.L1.Mn1;
      out_sh.L1_Mp1 = sh.L1.Mp1;
    }
    {
      LightData sun = in_sun;
      float3x3 sun_direction = to_float3x3(transform_to_matrix(sun.object_to_world));
      sun_direction = rotation_mat * sun_direction;
      sun.object_to_world = transform_from_matrix(to_float4x4(sun_direction));
      out_sun = sun;
    }
  }

  int2 local_texel = int2(gl_GlobalInvocationID.xy);
  SphereProbeUvArea read_coord = reinterpret_as_atlas_coord(read_coord_packed);
  SphereProbePixelArea write_coord;
  /* Define to avoid code duplication.
   * TODO(fclem): Could be removed by function call when we support image argument. */
#define PROCESS_MIP(_mip) \
  /* Exit when pixel being written doesn't fit in the area reserved for the probe. */ \
  write_coord = reinterpret_as_write_coord(write_coord_mip##_mip##_packed); \
  if (any(greaterThanEqual(local_texel, int2(write_coord.extent)))) { \
    return; \
  } \
  imageStore(out_sphere_mip##_mip, \
             int3(local_texel, write_coord.layer), \
             get_mip_data(_mip, local_texel, read_coord, write_coord, rotation_mat));
  /* We cannot put the following code in a loop because of the different image accesses. */
  PROCESS_MIP(0)
  PROCESS_MIP(1)
  PROCESS_MIP(2)
  PROCESS_MIP(3)
  PROCESS_MIP(4)
#ifdef __cplusplus
  /* If this mismatches, manual unroll needs to be updated. */
  static_assert(SPHERE_PROBE_MIPMAP_LEVELS == 5);
#endif
}
