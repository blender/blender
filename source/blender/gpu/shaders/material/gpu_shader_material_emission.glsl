void node_emission(vec4 color, float strength, vec3 vN, out Closure result)
{
  result = CLOSURE_DEFAULT;
#ifndef VOLUMETRICS
  result.radiance = render_pass_emission_mask(color.rgb) * strength;
  result.ssr_normal = normal_encode(vN, viewCameraVec(viewPosition));
#else
  result.emission = color.rgb * strength;
#endif
}
