
#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)

uniform sampler1D texHammersley;
uniform float sampleCount;
uniform float invSampleCount;

vec2 jitternoise = vec2(0.0);

#ifndef UTIL_TEX
#  define UTIL_TEX

#endif /* UTIL_TEX */

void setup_noise(void)
{
  jitternoise = texelfetch_noise_tex(gl_FragCoord.xy).rg; /* Global variable */
}

vec3 tangent_to_world(vec3 vector, vec3 N, vec3 T, vec3 B)
{
  return T * vector.x + B * vector.y + N * vector.z;
}

#ifdef HAMMERSLEY_SIZE
vec3 hammersley_3d(float i, float invsamplenbr)
{
  vec3 Xi; /* Theta, cos(Phi), sin(Phi) */

  Xi.x = i * invsamplenbr; /* i/samples */
  Xi.x = fract(Xi.x + jitternoise.x);

  int u = int(mod(i + jitternoise.y * HAMMERSLEY_SIZE, HAMMERSLEY_SIZE));

  Xi.yz = texelFetch(texHammersley, u, 0).rg;

  return Xi;
}

vec3 hammersley_3d(float i)
{
  return hammersley_3d(i, invSampleCount);
}
#endif

/* -------------- BSDFS -------------- */

float pdf_ggx_reflect(float NH, float a2)
{
  return NH * a2 / D_ggx_opti(NH, a2);
}

float pdf_hemisphere()
{
  return 0.5 * M_1_PI;
}

vec3 sample_ggx(vec3 rand, float a2)
{
  /* Theta is the aperture angle of the cone */
  float z = sqrt((1.0 - rand.x) / (1.0 + a2 * rand.x - rand.x)); /* cos theta */
  float r = sqrt(max(0.0, 1.0f - z * z));                        /* sin theta */
  float x = r * rand.y;
  float y = r * rand.z;

  /* Microfacet Normal */
  return vec3(x, y, z);
}

vec3 sample_ggx(vec3 rand, float a2, vec3 N, vec3 T, vec3 B, out float NH)
{
  vec3 Ht = sample_ggx(rand, a2);
  NH = Ht.z;
  return tangent_to_world(Ht, N, T, B);
}

#ifdef HAMMERSLEY_SIZE
vec3 sample_ggx(float nsample, float a2, vec3 N, vec3 T, vec3 B)
{
  vec3 Xi = hammersley_3d(nsample);
  vec3 Ht = sample_ggx(Xi, a2);
  return tangent_to_world(Ht, N, T, B);
}

vec3 sample_hemisphere(float nsample, vec3 N, vec3 T, vec3 B)
{
  vec3 Xi = hammersley_3d(nsample);

  float z = Xi.x;                         /* cos theta */
  float r = sqrt(max(0.0, 1.0f - z * z)); /* sin theta */
  float x = r * Xi.y;
  float y = r * Xi.z;

  vec3 Ht = vec3(x, y, z);

  return tangent_to_world(Ht, N, T, B);
}

vec3 sample_cone(float nsample, float angle, vec3 N, vec3 T, vec3 B)
{
  vec3 Xi = hammersley_3d(nsample);

  float z = cos(angle * Xi.x);            /* cos theta */
  float r = sqrt(max(0.0, 1.0f - z * z)); /* sin theta */
  float x = r * Xi.y;
  float y = r * Xi.z;

  vec3 Ht = vec3(x, y, z);

  return tangent_to_world(Ht, N, T, B);
}
#endif
