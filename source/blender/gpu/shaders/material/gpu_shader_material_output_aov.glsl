
void node_output_aov(vec4 color, float value, float hash, out Closure dummy)
{
  output_aov(color, value, floatBitsToUint(hash));
}
