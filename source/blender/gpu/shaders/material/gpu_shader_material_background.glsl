
void node_background(vec4 color, float strength, float weight, out Closure result)
{
  ClosureEmission emission_data;
  emission_data.weight = weight;
  emission_data.emission = color.rgb * strength;

  result = closure_eval(emission_data);
}
