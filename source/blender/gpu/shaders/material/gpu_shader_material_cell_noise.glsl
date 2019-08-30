float cellnoise(vec3 p)
{
  int ix = quick_floor(p.x);
  int iy = quick_floor(p.y);
  int iz = quick_floor(p.z);

  return hash_uint3_to_float(uint(ix), uint(iy), uint(iz));
}

vec3 cellnoise_color(vec3 p)
{
  float r = cellnoise(p.xyz);
  float g = cellnoise(p.yxz);
  float b = cellnoise(p.yzx);

  return vec3(r, g, b);
}
