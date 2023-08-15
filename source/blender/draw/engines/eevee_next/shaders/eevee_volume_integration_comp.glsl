
#pragma BLENDER_REQUIRE(eevee_volume_lib.glsl)

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Step 3 : Integrate for each froxel the final amount of light
 * scattered back to the viewer and the amount of transmittance. */

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  ivec3 tex_size = volumes_info_buf.tex_size;

  if (any(greaterThanEqual(texel, tex_size.xy))) {
    return;
  }

  /* Start with full transmittance and no scattered light. */
  vec3 scattering = vec3(0.0);
  vec3 transmittance = vec3(1.0);

  /* Compute view ray. */
  vec2 uvs = (vec2(texel) + vec2(0.5)) / vec2(tex_size.xy);
  vec3 ndc_cell = volume_to_ndc(vec3(uvs, 1e-5));
  vec3 view_cell = get_view_space_from_depth(ndc_cell.xy, ndc_cell.z);

  float prev_ray_len;
  float orig_ray_len;

  bool is_persp = ProjectionMatrix[3][3] == 0.0;
  if (is_persp) {
    prev_ray_len = length(view_cell);
    orig_ray_len = prev_ray_len / view_cell.z;
  }
  else {
    prev_ray_len = view_cell.z;
    orig_ray_len = 1.0;
  }

  for (int i = 0; i <= tex_size.z; i++) {
    ivec3 froxel = ivec3(texel, i);

    vec3 froxel_scattering = texelFetch(in_scattering_tx, froxel, 0).rgb;
    vec3 extinction = texelFetch(in_extinction_tx, froxel, 0).rgb;

    float cell_depth = volume_z_to_view_z((float(i) + 1.0) / tex_size.z);
    float ray_len = orig_ray_len * cell_depth;

    /* Emission does not work if there is no extinction because
     * froxel_transmittance evaluates to 1.0 leading to froxel_scattering = 0.0. (See #65771) */
    extinction = max(vec3(1e-7) * step(1e-5, froxel_scattering), extinction);

    /* Evaluate Scattering. */
    float step_len = abs(ray_len - prev_ray_len);
    prev_ray_len = ray_len;
    vec3 froxel_transmittance = exp(-extinction * step_len);

    /* Integrate along the current step segment. */
    /** NOTE: Original calculation carries precision issues when compiling for AMD GPUs
     * and running Metal. This version of the equation retains precision well for all
     * macOS HW configurations. */
    froxel_scattering = (froxel_scattering * (1.0f - froxel_transmittance)) /
                        max(vec3(1e-8), extinction);

    /* Accumulate and also take into account the transmittance from previous steps. */
    scattering += transmittance * froxel_scattering;
    transmittance *= froxel_transmittance;

    imageStore(out_scattering_img, froxel, vec4(scattering, 1.0));
    imageStore(out_transmittance_img, froxel, vec4(transmittance, 1.0));
  }
}
