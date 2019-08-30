void node_light_falloff(
    float strength, float tsmooth, out float quadratic, out float linear, out float constant)
{
  quadratic = strength;
  linear = strength;
  constant = strength;
}
