void invert(float fac, vec4 col, out vec4 outcol)
{
  outcol.xyz = mix(col.xyz, vec3(1.0) - col.xyz, fac);
  outcol.w = col.w;
}
