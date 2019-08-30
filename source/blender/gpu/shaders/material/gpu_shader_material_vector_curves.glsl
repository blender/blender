void curves_vec(float fac, vec3 vec, sampler1DArray curvemap, float layer, out vec3 outvec)
{
  vec4 co = vec4(vec * 0.5 + 0.5, layer);
  outvec.x = texture(curvemap, co.xw).x;
  outvec.y = texture(curvemap, co.yw).y;
  outvec.z = texture(curvemap, co.zw).z;
  outvec = mix(vec, outvec, fac);
}
