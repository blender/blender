/* Musgrave fBm
 *
 * H: fractal increment parameter
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 *
 * from "Texturing and Modelling: A procedural approach"
 */

float noise_musgrave_fBm(vec3 p, float H, float lacunarity, float octaves)
{
  float rmd;
  float value = 0.0;
  float pwr = 1.0;
  float pwHL = pow(lacunarity, -H);

  for (int i = 0; i < int(octaves); i++) {
    value += snoise(p) * pwr;
    pwr *= pwHL;
    p *= lacunarity;
  }

  rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    value += rmd * snoise(p) * pwr;
  }

  return value;
}

/* Musgrave Multifractal
 *
 * H: highest fractal dimension
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 */

float noise_musgrave_multi_fractal(vec3 p, float H, float lacunarity, float octaves)
{
  float rmd;
  float value = 1.0;
  float pwr = 1.0;
  float pwHL = pow(lacunarity, -H);

  for (int i = 0; i < int(octaves); i++) {
    value *= (pwr * snoise(p) + 1.0);
    pwr *= pwHL;
    p *= lacunarity;
  }

  rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    value *= (rmd * pwr * snoise(p) + 1.0); /* correct? */
  }

  return value;
}

/* Musgrave Heterogeneous Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float noise_musgrave_hetero_terrain(vec3 p, float H, float lacunarity, float octaves, float offset)
{
  float value, increment, rmd;
  float pwHL = pow(lacunarity, -H);
  float pwr = pwHL;

  /* first unscaled octave of function; later octaves are scaled */
  value = offset + snoise(p);
  p *= lacunarity;

  for (int i = 1; i < int(octaves); i++) {
    increment = (snoise(p) + offset) * pwr * value;
    value += increment;
    pwr *= pwHL;
    p *= lacunarity;
  }

  rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    increment = (snoise(p) + offset) * pwr * value;
    value += rmd * increment;
  }

  return value;
}

/* Hybrid Additive/Multiplicative Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float noise_musgrave_hybrid_multi_fractal(
    vec3 p, float H, float lacunarity, float octaves, float offset, float gain)
{
  float result, signal, weight, rmd;
  float pwHL = pow(lacunarity, -H);
  float pwr = pwHL;

  result = snoise(p) + offset;
  weight = gain * result;
  p *= lacunarity;

  for (int i = 1; (weight > 0.001f) && (i < int(octaves)); i++) {
    if (weight > 1.0) {
      weight = 1.0;
    }

    signal = (snoise(p) + offset) * pwr;
    pwr *= pwHL;
    result += weight * signal;
    weight *= gain * signal;
    p *= lacunarity;
  }

  rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    result += rmd * ((snoise(p) + offset) * pwr);
  }

  return result;
}

/* Ridged Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

float noise_musgrave_ridged_multi_fractal(
    vec3 p, float H, float lacunarity, float octaves, float offset, float gain)
{
  float result, signal, weight;
  float pwHL = pow(lacunarity, -H);
  float pwr = pwHL;

  signal = offset - abs(snoise(p));
  signal *= signal;
  result = signal;
  weight = 1.0;

  for (int i = 1; i < int(octaves); i++) {
    p *= lacunarity;
    weight = clamp(signal * gain, 0.0, 1.0);
    signal = offset - abs(snoise(p));
    signal *= signal;
    signal *= weight;
    result += signal * pwr;
    pwr *= pwHL;
  }

  return result;
}

float svm_musgrave(int type,
                   float dimension,
                   float lacunarity,
                   float octaves,
                   float offset,
                   float intensity,
                   float gain,
                   vec3 p)
{
  if (type == 0 /* NODE_MUSGRAVE_MULTIFRACTAL */) {
    return intensity * noise_musgrave_multi_fractal(p, dimension, lacunarity, octaves);
  }
  else if (type == 1 /* NODE_MUSGRAVE_FBM */) {
    return intensity * noise_musgrave_fBm(p, dimension, lacunarity, octaves);
  }
  else if (type == 2 /* NODE_MUSGRAVE_HYBRID_MULTIFRACTAL */) {
    return intensity *
           noise_musgrave_hybrid_multi_fractal(p, dimension, lacunarity, octaves, offset, gain);
  }
  else if (type == 3 /* NODE_MUSGRAVE_RIDGED_MULTIFRACTAL */) {
    return intensity *
           noise_musgrave_ridged_multi_fractal(p, dimension, lacunarity, octaves, offset, gain);
  }
  else if (type == 4 /* NODE_MUSGRAVE_HETERO_TERRAIN */) {
    return intensity * noise_musgrave_hetero_terrain(p, dimension, lacunarity, octaves, offset);
  }
  return 0.0;
}

void node_tex_musgrave(vec3 co,
                       float scale,
                       float detail,
                       float dimension,
                       float lacunarity,
                       float offset,
                       float gain,
                       float type,
                       out vec4 color,
                       out float fac)
{
  fac = svm_musgrave(int(type), dimension, lacunarity, detail, offset, 1.0, gain, co *scale);

  color = vec4(fac, fac, fac, 1.0);
}
