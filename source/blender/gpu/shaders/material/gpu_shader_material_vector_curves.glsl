/* ext is vec4(in_x, in_dy, out_x, out_dy). */
float curve_vec_extrapolate(float x, float y, vec4 ext)
{
  if (x < 0.0) {
    return y + x * ext.y;
  }
  else if (x > 1.0) {
    return y + (x - 1.0) * ext.w;
  }
  else {
    return y;
  }
}

#define RANGE_RESCALE(x, min, range) ((x - min) * range)

void curves_vec(float fac,
                vec3 vec,
                sampler1DArray curvemap,
                float layer,
                vec3 range,
                vec4 ext_x,
                vec4 ext_y,
                vec4 ext_z,
                out vec3 outvec)
{
  vec4 co = vec4(vec, layer);

  vec3 xyz_min = vec3(ext_x.x, ext_y.x, ext_z.x);
  co.xyz = RANGE_RESCALE(co.xyz, xyz_min, range);

  outvec.x = texture(curvemap, co.xw).x;
  outvec.y = texture(curvemap, co.yw).y;
  outvec.z = texture(curvemap, co.zw).z;

  outvec.x = curve_vec_extrapolate(co.x, outvec.r, ext_x);
  outvec.y = curve_vec_extrapolate(co.y, outvec.g, ext_y);
  outvec.z = curve_vec_extrapolate(co.z, outvec.b, ext_z);

  outvec = mix(vec, outvec, fac);
}
