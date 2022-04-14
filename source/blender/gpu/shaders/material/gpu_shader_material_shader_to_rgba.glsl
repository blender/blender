
void node_shader_to_rgba(Closure cl, out vec4 outcol, out float outalpha)
{
  outcol = closure_to_rgba(cl);
  outalpha = outcol.a;
}
