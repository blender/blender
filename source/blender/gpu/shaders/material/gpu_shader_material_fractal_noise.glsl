/* The fractal_noise functions are all exactly the same except for the input type. */
float fractal_noise(float p, float octaves)
{
  float fscale = 1.0;
  float amp = 1.0;
  float sum = 0.0;
  octaves = clamp(octaves, 0.0, 16.0);
  int n = int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = noise(fscale * p);
    sum += t * amp;
    amp *= 0.5;
    fscale *= 2.0;
  }
  float rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    float t = noise(fscale * p);
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

/* The fractal_noise functions are all exactly the same except for the input type. */
float fractal_noise(vec2 p, float octaves)
{
  float fscale = 1.0;
  float amp = 1.0;
  float sum = 0.0;
  octaves = clamp(octaves, 0.0, 16.0);
  int n = int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = noise(fscale * p);
    sum += t * amp;
    amp *= 0.5;
    fscale *= 2.0;
  }
  float rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    float t = noise(fscale * p);
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

/* The fractal_noise functions are all exactly the same except for the input type. */
float fractal_noise(vec3 p, float octaves)
{
  float fscale = 1.0;
  float amp = 1.0;
  float sum = 0.0;
  octaves = clamp(octaves, 0.0, 16.0);
  int n = int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = noise(fscale * p);
    sum += t * amp;
    amp *= 0.5;
    fscale *= 2.0;
  }
  float rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    float t = noise(fscale * p);
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

/* The fractal_noise functions are all exactly the same except for the input type. */
float fractal_noise(vec4 p, float octaves)
{
  float fscale = 1.0;
  float amp = 1.0;
  float sum = 0.0;
  octaves = clamp(octaves, 0.0, 16.0);
  int n = int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = noise(fscale * p);
    sum += t * amp;
    amp *= 0.5;
    fscale *= 2.0;
  }
  float rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    float t = noise(fscale * p);
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
