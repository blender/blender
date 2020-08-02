
#pragma BLENDER_REQUIRE(bsdf_common_lib.glsl)

/* ---------------------------------------------------------------------- */
/** \name Utiltex
 *
 * Utiltex is a sampler2DArray that stores a number of useful small utilitary textures and lookup
 * tables.
 * \{ */

uniform sampler2DArray utilTex;

#define LUT_SIZE 64

#define texelfetch_noise_tex(coord) texelFetch(utilTex, ivec3(ivec2(coord) % LUT_SIZE, 2.0), 0)

/* Return texture coordinates to sample Surface LUT */
vec2 lut_coords(float cosTheta, float roughness)
{
  float theta = acos(cosTheta);
  vec2 coords = vec2(roughness, theta / M_PI_2);

  /* scale and bias coordinates, for correct filtered lookup */
  return coords * (LUT_SIZE - 1.0) / LUT_SIZE + 0.5 / LUT_SIZE;
}

vec2 lut_coords_ltc(float cosTheta, float roughness)
{
  vec2 coords = vec2(roughness, sqrt(1.0 - cosTheta));

  /* scale and bias coordinates, for correct filtered lookup */
  return coords * (LUT_SIZE - 1.0) / LUT_SIZE + 0.5 / LUT_SIZE;
}

float get_btdf_lut(float NV, float roughness, float ior)
{
  const vec3 lut_scale_bias_texel_size = vec3((LUT_SIZE - 1.0), 0.5, 1.5) / LUT_SIZE;

  vec3 coords;
  /* Try to compensate for the low resolution and interpolation error. */
  coords.x = (ior > 1.0) ? (0.9 + lut_scale_bias_texel_size.z) +
                               (0.1 - lut_scale_bias_texel_size.z) * f0_from_ior(ior) :
                           (0.9 + lut_scale_bias_texel_size.z) * ior * ior;
  coords.y = 1.0 - saturate(NV);
  coords.xy *= lut_scale_bias_texel_size.x;
  coords.xy += lut_scale_bias_texel_size.y;

  const float lut_lvl_ofs = 4.0;    /* First texture lvl of roughness. */
  const float lut_lvl_scale = 16.0; /* How many lvl of roughness in the lut. */

  float mip = roughness * lut_lvl_scale;
  float mip_floor = floor(mip);

  coords.z = lut_lvl_ofs + mip_floor + 1.0;
  float btdf_high = textureLod(utilTex, coords, 0.0).r;

  coords.z -= 1.0;
  float btdf_low = textureLod(utilTex, coords, 0.0).r;

  float btdf = (ior == 1.0) ? 1.0 : mix(btdf_low, btdf_high, mip - coords.z);

  return btdf;
}

/** \} */
