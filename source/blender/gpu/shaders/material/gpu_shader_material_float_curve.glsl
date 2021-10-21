/* ext is vec4(in_x, in_dy, out_x, out_dy). */
float curve_float_extrapolate(float x, float y, vec4 ext)
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

void curve_float(float fac,
                 float vec,
                 sampler1DArray curvemap,
                 float layer,
                 float range,
                 vec4 ext,
                 out float outvec)
{
  float xyz_min = ext.x;
  vec = RANGE_RESCALE(vec, xyz_min, range);

  outvec = texture(curvemap, vec2(vec, layer)).x;

  outvec = curve_float_extrapolate(vec, outvec, ext);

  outvec = mix(vec, outvec, fac);
}
