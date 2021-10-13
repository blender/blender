#ifndef VOLUMETRICS

CLOSURE_EVAL_FUNCTION_DECLARE_1(node_shader_to_rgba, Glossy)

void node_shader_to_rgba(Closure cl, out vec4 outcol, out float outalpha)
{
  vec4 spec_accum = vec4(0.0);
  if (ssrToggle && FLAG_TEST(cl.flag, CLOSURE_SSR_FLAG)) {
    CLOSURE_VARS_DECLARE_1(Glossy);

    vec3 vN = normal_decode(cl.ssr_normal, viewCameraVec(viewPosition));
    vec3 N = transform_direction(ViewMatrixInverse, vN);

    in_Glossy_0.N = N; /* Normalized during eval. */
    in_Glossy_0.roughness = cl.ssr_data.a;

    CLOSURE_EVAL_FUNCTION_1(node_shader_to_rgba, Glossy);

    spec_accum.rgb = out_Glossy_0.radiance;
  }

  outalpha = saturate(1.0 - avg(cl.transmittance));
  outcol = vec4((spec_accum.rgb * cl.ssr_data.rgb) + cl.radiance, 1.0);

#  ifdef USE_SSS
  outcol.rgb += cl.sss_irradiance.rgb * cl.sss_albedo;
#  endif
}
#endif /* VOLUMETRICS */
