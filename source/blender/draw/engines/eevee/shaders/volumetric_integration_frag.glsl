
/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Step 3 : Integrate for each froxel the final amount of light
 * scattered back to the viewer and the amount of transmittance. */

uniform sampler3D volumeScattering; /* Result of the scatter step */
uniform sampler3D volumeExtinction;

flat in int slice;

layout(location = 0) out vec3 finalScattering;
layout(location = 1) out vec3 finalTransmittance;

void main()
{
  /* Start with full transmittance and no scattered light. */
  finalScattering = vec3(0.0);
  finalTransmittance = vec3(1.0);

  vec3 tex_size = vec3(textureSize(volumeScattering, 0).xyz);

  /* Compute view ray. */
  vec2 uvs = gl_FragCoord.xy / tex_size.xy;
  vec3 ndc_cell = volume_to_ndc(vec3(uvs, 1e-5));
  vec3 view_cell = get_view_space_from_depth(ndc_cell.xy, ndc_cell.z);

  /* Ortho */
  float prev_ray_len = view_cell.z;
  float orig_ray_len = 1.0;

  /* Persp */
  if (ProjectionMatrix[3][3] == 0.0) {
    prev_ray_len = length(view_cell);
    orig_ray_len = prev_ray_len / view_cell.z;
  }

  /* Without compute shader and arbitrary write we need to
   * accumulate from the beginning of the ray for each cell. */
  float integration_end = float(slice);
  for (int i = 0; i < slice; ++i) {
    ivec3 volume_cell = ivec3(gl_FragCoord.xy, i);

    vec3 Lscat = texelFetch(volumeScattering, volume_cell, 0).rgb;
    vec3 s_extinction = texelFetch(volumeExtinction, volume_cell, 0).rgb;

    float cell_depth = volume_z_to_view_z((float(i) + 1.0) / tex_size.z);
    float ray_len = orig_ray_len * cell_depth;

    /* Emission does not work of there is no extinction because
     * Tr evaluates to 1.0 leading to Lscat = 0.0. (See T65771) */
    s_extinction = max(vec3(1e-7) * step(1e-5, Lscat), s_extinction);

    /* Evaluate Scattering */
    float s_len = abs(ray_len - prev_ray_len);
    prev_ray_len = ray_len;
    vec3 Tr = exp(-s_extinction * s_len);

    /* integrate along the current step segment */
    Lscat = (Lscat - Lscat * Tr) / max(vec3(1e-8), s_extinction);
    /* accumulate and also take into account the transmittance from previous steps */
    finalScattering += finalTransmittance * Lscat;

    finalTransmittance *= Tr;
  }
}
