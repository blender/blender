void node_add_shader(Closure shader1, Closure shader2, out Closure shader)
{
  shader = closure_add(shader1, shader2);
}
