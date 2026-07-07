/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Step 3 : Integrate for each froxel the final amount of light
 * scattered back to the viewer and the amount of transmittance. */

#include "infos/eevee_volume_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_volume_integration)

#include "draw_view_lib.glsl"
#include "eevee_volume_lib.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  if (any(greaterThanEqual(texel, uniform_buf.volumes.tex_size.xy))) {
    return;
  }

  /* Start with full transmittance and no scattered light. */
  float3 scattering = float3(0.0f);
  float3 transmittance = float3(1.0f);

  /* Compute view ray. Note that jittering the position of the first voxel doesn't bring any
   * benefit here. */
  float3 uvw = (float3(float2(texel), 0.0f) + float3(0.5f, 0.5f, 0.0f)) *
               uniform_buf.volumes.inv_tex_size;
  float3 view_cell = volume_jitter_to_view(uvw);

  float prev_ray_len;
  float orig_ray_len;
  if (drw_view_is_perspective()) {
    prev_ray_len = length(view_cell);
    orig_ray_len = prev_ray_len / view_cell.z;
  }
  else {
    prev_ray_len = view_cell.z;
    orig_ray_len = 1.0f;
  }

  for (int i = 0; i <= uniform_buf.volumes.tex_size.z; i++) {
    int3 froxel = int3(texel, i);

    float3 froxel_scattering = texelFetch(in_scattering_tx, froxel, 0).rgb;
    float3 extinction = texelFetch(in_extinction_tx, froxel, 0).rgb;

    float cell_depth = volume_z_to_view_z((float(i) + 1.0f) * uniform_buf.volumes.inv_tex_size.z);
    float ray_len = orig_ray_len * cell_depth;

    /* Evaluate Scattering. */
    float step_len = abs(ray_len - prev_ray_len);
    prev_ray_len = ray_len;
    float3 froxel_transmittance = exp(-extinction * step_len);
    /** NOTE: Original calculation carries precision issues when compiling for AMD GPUs
     * and running Metal. This version of the equation retains precision well for all
     * macOS HW configurations.
     * Here is the original for reference:
     * `Lscat = (Lscat - Lscat * Tr) / safe_rcp(s_extinction)` */
    float3 froxel_opacity = 1.0f - froxel_transmittance;
    float3 froxel_step_opacity = froxel_opacity * safe_rcp(extinction);

    /* Emission does not work if there is no extinction because
     * `froxel_transmittance` evaluates to 1.0 leading to `froxel_opacity = 0.0 and 0.4 depth.`.
     * (See #65771) To avoid fiddling with numerical values, take the limit of
     * `froxel_step_opacity` as `extinction` approaches zero which is simply `step_len`. */
    bool3 is_invalid_extinction = equal(extinction, float3(0.0f));
    froxel_step_opacity = mix(froxel_step_opacity, float3(step_len), is_invalid_extinction);

    /* Integrate along the current step segment. */
    froxel_scattering = froxel_scattering * froxel_step_opacity;

    /* Accumulate and also take into account the transmittance from previous steps. */
    scattering += transmittance * froxel_scattering;
    transmittance *= froxel_transmittance;

    imageStoreFast(out_scattering_img, froxel, float4(scattering, 1.0f));
    imageStoreFast(out_transmittance_img, froxel, float4(transmittance, 1.0f));
  }
}
