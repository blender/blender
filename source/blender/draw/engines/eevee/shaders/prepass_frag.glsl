
#ifdef USE_ALPHA_HASH

/* From the paper "Hashed Alpha Testing" by Chris Wyman and Morgan McGuire */
float hash(vec2 a)
{
  return fract(1e4 * sin(17.0 * a.x + 0.1 * a.y) * (0.1 + abs(sin(13.0 * a.y + a.x))));
}

float hash3d(vec3 a)
{
  return hash(vec2(hash(a.xy), a.z));
}

float hashed_alpha_threshold(vec3 co)
{
  /* Find the discretized derivatives of our coordinates. */
  float max_deriv = max(length(dFdx(co)), length(dFdy(co)));
  float pix_scale = 1.0 / (alphaHashScale * max_deriv);

  /* Find two nearest log-discretized noise scales. */
  float pix_scale_log = log2(pix_scale);
  vec2 pix_scales;
  pix_scales.x = exp2(floor(pix_scale_log));
  pix_scales.y = exp2(ceil(pix_scale_log));

  /* Compute alpha thresholds at our two noise scales. */
  vec2 alpha;
  alpha.x = hash3d(floor(pix_scales.x * co));
  alpha.y = hash3d(floor(pix_scales.y * co));

  /* Factor to interpolate lerp with. */
  float fac = fract(log2(pix_scale));

  /* Interpolate alpha threshold from noise at two scales. */
  float x = mix(alpha.x, alpha.y, fac);

  /* Pass into CDF to compute uniformly distrib threshold. */
  float a = min(fac, 1.0 - fac);
  float one_a = 1.0 - a;
  float denom = 1.0 / (2 * a * one_a);
  float one_x = (1 - x);
  vec3 cases = vec3((x * x) * denom, (x - 0.5 * a) / one_a, 1.0 - (one_x * one_x * denom));

  /* Find our final, uniformly distributed alpha threshold. */
  float threshold = (x < one_a) ? ((x < a) ? cases.x : cases.y) : cases.z;

  /* Jitter the threshold for TAA accumulation. */
  threshold = fract(threshold + alphaHashOffset);

  /* Avoids threshold == 0. */
  threshold = clamp(threshold, 1.0e-6, 1.0);

  return threshold;
}

#endif

#define NODETREE_EXEC

void main()
{
#if defined(USE_ALPHA_HASH)

  Closure cl = nodetree_exec();

  float opacity = saturate(1.0 - avg(cl.transmittance));

#  if defined(USE_ALPHA_HASH)
  /* Hashed Alpha Testing */
  if (opacity < hashed_alpha_threshold(worldPosition)) {
    discard;
  }
#  endif
#endif
}
