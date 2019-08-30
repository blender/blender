void node_attribute(vec3 attr, out vec4 outcol, out vec3 outvec, out float outf)
{
  outcol = vec4(attr, 1.0);
  outvec = attr;
  outf = avg(attr);
}
