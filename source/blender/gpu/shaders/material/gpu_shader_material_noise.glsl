float noise_fade(float t)
{
  return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float noise_scale3(float result)
{
  return 0.9820 * result;
}

float noise_nerp(float t, float a, float b)
{
  return (1.0 - t) * a + t * b;
}

float noise_grad(uint hash, float x, float y, float z)
{
  uint h = hash & 15u;
  float u = h < 8u ? x : y;
  float vt = ((h == 12u) || (h == 14u)) ? x : z;
  float v = h < 4u ? y : vt;
  return (((h & 1u) != 0u) ? -u : u) + (((h & 2u) != 0u) ? -v : v);
}

float noise_perlin(float x, float y, float z)
{
  int X;
  float fx = floorfrac(x, X);
  int Y;
  float fy = floorfrac(y, Y);
  int Z;
  float fz = floorfrac(z, Z);

  float u = noise_fade(fx);
  float v = noise_fade(fy);
  float w = noise_fade(fz);

  float noise_u[2], noise_v[2];

  noise_u[0] = noise_nerp(u,
                          noise_grad(hash_int3(X, Y, Z), fx, fy, fz),
                          noise_grad(hash_int3(X + 1, Y, Z), fx - 1.0, fy, fz));

  noise_u[1] = noise_nerp(u,
                          noise_grad(hash_int3(X, Y + 1, Z), fx, fy - 1.0, fz),
                          noise_grad(hash_int3(X + 1, Y + 1, Z), fx - 1.0, fy - 1.0, fz));

  noise_v[0] = noise_nerp(v, noise_u[0], noise_u[1]);

  noise_u[0] = noise_nerp(u,
                          noise_grad(hash_int3(X, Y, Z + 1), fx, fy, fz - 1.0),
                          noise_grad(hash_int3(X + 1, Y, Z + 1), fx - 1.0, fy, fz - 1.0));

  noise_u[1] = noise_nerp(
      u,
      noise_grad(hash_int3(X, Y + 1, Z + 1), fx, fy - 1.0, fz - 1.0),
      noise_grad(hash_int3(X + 1, Y + 1, Z + 1), fx - 1.0, fy - 1.0, fz - 1.0));

  noise_v[1] = noise_nerp(v, noise_u[0], noise_u[1]);

  float r = noise_scale3(noise_nerp(w, noise_v[0], noise_v[1]));

  return (isinf(r)) ? 0.0 : r;
}

float noise(vec3 p)
{
  return 0.5 * noise_perlin(p.x, p.y, p.z) + 0.5;
}

float snoise(vec3 p)
{
  return noise_perlin(p.x, p.y, p.z);
}
