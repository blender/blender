void node_light_falloff(float strength,
                        float tsmooth,
                        out float quadratic,
                        out float linear,
                        out float falloff_constant)
{
  quadratic = strength;
  linear = strength;
  falloff_constant = strength;
}
