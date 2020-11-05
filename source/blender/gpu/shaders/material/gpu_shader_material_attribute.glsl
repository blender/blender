void node_attribute(
    vec4 attr, out vec4 outcol, out vec3 outvec, out float outf, out float outalpha)
{
  outcol = vec4(attr.xyz, 1.0);
  outvec = attr.xyz;
  outf = avg(attr.xyz);
  outalpha = attr.w;
}
