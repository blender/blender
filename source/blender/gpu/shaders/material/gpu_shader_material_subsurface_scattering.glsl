#ifndef VOLUMETRICS
void node_subsurface_scattering(vec4 color,
                                float scale,
                                vec3 radius,
                                float sharpen,
                                float texture_blur,
                                vec3 N,
                                float sss_id,
                                out Closure result)
{
#  if defined(USE_SSS)
  N = normalize(N);
  vec3 out_diff, out_trans;
  vec3 vN = mat3(ViewMatrix) * N;
  result = CLOSURE_DEFAULT;
  closure_load_ssr_data(vec3(0.0), 0.0, N, viewCameraVec, -1, result);

  eevee_closure_subsurface(N, color.rgb, 1.0, scale, out_diff, out_trans);

  vec3 sss_radiance = out_diff + out_trans;
#    ifdef USE_SSS_ALBEDO
  /* Not perfect for texture_blur not exactly equal to 0.0 or 1.0. */
  vec3 sss_albedo = mix(color.rgb, vec3(1.0), texture_blur);
  sss_radiance *= mix(vec3(1.0), color.rgb, texture_blur);
#    else
  sss_radiance *= color.rgb;
#    endif
  closure_load_sss_data(scale,
                        sss_radiance,
#    ifdef USE_SSS_ALBEDO
                        sss_albedo,
#    endif
                        int(sss_id),
                        result);
#  else
  node_bsdf_diffuse(color, 0.0, N, result);
#  endif
}
#else
/* Stub subsurface scattering because it is not compatible with volumetrics. */
#  define node_subsurface_scattering
#endif
