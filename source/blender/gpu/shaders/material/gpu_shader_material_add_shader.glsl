void node_add_shader(inout Closure shader1, inout Closure shader2, out Closure shader)
{
  shader = closure_add(shader1, shader2);
}
