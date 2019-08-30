float noise_turbulence(vec3 p, float octaves, int hard)
{
  float fscale = 1.0;
  float amp = 1.0;
  float sum = 0.0;
  octaves = clamp(octaves, 0.0, 16.0);
  int n = int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = noise(fscale * p);
    if (hard != 0) {
      t = abs(2.0 * t - 1.0);
    }
    sum += t * amp;
    amp *= 0.5;
    fscale *= 2.0;
  }
  float rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    float t = noise(fscale * p);
    if (hard != 0) {
      t = abs(2.0 * t - 1.0);
    }
    float sum2 = sum + t * amp;
    sum *= (float(1 << n) / float((1 << (n + 1)) - 1));
    sum2 *= (float(1 << (n + 1)) / float((1 << (n + 2)) - 1));
    return (1.0 - rmd) * sum + rmd * sum2;
  }
  else {
    sum *= (float(1 << n) / float((1 << (n + 1)) - 1));
    return sum;
  }
}
