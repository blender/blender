
void node_bsdf_transparent(vec4 color, float weight, out Closure result)
{
  ClosureTransparency transparency_data;
  transparency_data.weight = weight;
  transparency_data.transmittance = color.rgb;
  transparency_data.holdout = 0.0;

  result = closure_eval(transparency_data);
}
