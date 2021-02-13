
#pragma BLENDER_REQUIRE(bsdf_common_lib.glsl)

uniform sampler1D texHammersley;

vec3 tangent_to_world(vec3 vector, vec3 N, vec3 T, vec3 B)
{
  return T * vector.x + B * vector.y + N * vector.z;
}

#ifdef HAMMERSLEY_SIZE
vec3 hammersley_3d(float i, float invsamplenbr)
{
  vec3 Xi; /* Theta, cos(Phi), sin(Phi) */

  Xi.x = i * invsamplenbr;
  Xi.yz = texelFetch(texHammersley, int(i), 0).rg;

  return Xi;
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
vec3 sample_ggx(float nsample, float inv_sample_count, float a2, vec3 N, vec3 T, vec3 B)
{
  vec3 Xi = hammersley_3d(nsample, inv_sample_count);
  vec3 Ht = sample_ggx(Xi, a2);
  return tangent_to_world(Ht, N, T, B);
}

vec3 sample_hemisphere(float nsample, float inv_sample_count, vec3 N, vec3 T, vec3 B)
{
  vec3 Xi = hammersley_3d(nsample, inv_sample_count);

  float z = Xi.x;                         /* cos theta */
  float r = sqrt(max(0.0, 1.0f - z * z)); /* sin theta */
  float x = r * Xi.y;
  float y = r * Xi.z;

  vec3 Ht = vec3(x, y, z);

  return tangent_to_world(Ht, N, T, B);
}

vec3 sample_cone(float nsample, float inv_sample_count, float angle, vec3 N, vec3 T, vec3 B)
{
  vec3 Xi = hammersley_3d(nsample, inv_sample_count);

  float z = cos(angle * Xi.x);            /* cos theta */
  float r = sqrt(max(0.0, 1.0f - z * z)); /* sin theta */
  float x = r * Xi.y;
  float y = r * Xi.z;

  vec3 Ht = vec3(x, y, z);

  return tangent_to_world(Ht, N, T, B);
}
#endif
