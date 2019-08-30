void node_mix_shader(float fac, Closure shader1, Closure shader2, out Closure shader)
{
  shader = closure_mix(shader1, shader2, fac);
}
