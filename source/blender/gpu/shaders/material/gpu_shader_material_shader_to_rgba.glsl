#ifndef VOLUMETRICS
void node_shader_to_rgba(Closure cl, out vec4 outcol, out float outalpha)
{
  vec4 spec_accum = vec4(0.0);
  if (ssrToggle && FLAG_TEST(cl.flag, CLOSURE_SSR_FLAG)) {
    vec3 V = cameraVec(worldPosition);
    vec3 vN = normal_decode(cl.ssr_normal, viewCameraVec(viewPosition));
    vec3 N = transform_direction(ViewMatrixInverse, vN);
    float roughness = cl.ssr_data.a;
    float roughnessSquared = max(1e-3, roughness * roughness);
    fallback_cubemap(N, V, worldPosition, viewPosition, roughness, roughnessSquared, spec_accum);
  }

  outalpha = saturate(1.0 - avg(cl.transmittance));
  outcol = vec4((spec_accum.rgb * cl.ssr_data.rgb) + cl.radiance, 1.0);

#  ifdef USE_SSS
  outcol.rgb += cl.sss_irradiance.rgb * cl.sss_albedo;
#  endif
}
#endif /* VOLUMETRICS */
