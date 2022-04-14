
void node_volume_absorption(vec4 color, float density, float weight, out Closure result)
{
  ClosureVolumeAbsorption volume_absorption_data;
  volume_absorption_data.weight = weight;
  volume_absorption_data.absorption = (1.0 - color.rgb) * density;

  result = closure_eval(volume_absorption_data);
}
