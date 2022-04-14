
void node_holdout(float weight, out Closure result)
{
  ClosureTransparency transparency_data;
  transparency_data.weight = weight;
  transparency_data.transmittance = vec3(0.0);
  transparency_data.holdout = 1.0;

  result = closure_eval(transparency_data);
}
