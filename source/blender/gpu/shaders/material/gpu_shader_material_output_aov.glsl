
void node_output_aov(vec4 color, float value, out Closure result)
{
  result = CLOSURE_DEFAULT;
#ifndef VOLUMETRICS
  if (render_pass_aov_is_color()) {
    result.radiance = color.rgb;
  }
  else {
    result.radiance = vec3(value);
  }
#endif
}
