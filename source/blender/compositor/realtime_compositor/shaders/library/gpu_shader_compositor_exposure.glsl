void node_composite_exposure(vec4 color, float exposure, out vec4 result)
{
  float multiplier = exp2(exposure);
  result.rgb = color.rgb * multiplier;
  result.a = color.a;
}
