/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(random_lib.glsl)
#pragma BLENDER_REQUIRE(bsdf_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(irradiance_lib.glsl)

#define M_4PI 12.5663706143591729

vec3 get_cubemap_vector(vec2 co, int face)
{
  /* NOTE(Metal): Declaring constant array in function scope to avoid increasing local shader
   * memory pressure. */
  const mat3 CUBE_ROTATIONS[6] = mat3[](
      mat3(vec3(0.0, 0.0, -1.0), vec3(0.0, -1.0, 0.0), vec3(-1.0, 0.0, 0.0)),
      mat3(vec3(0.0, 0.0, 1.0), vec3(0.0, -1.0, 0.0), vec3(1.0, 0.0, 0.0)),
      mat3(vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0), vec3(0.0, -1.0, 0.0)),
      mat3(vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, -1.0), vec3(0.0, 1.0, 0.0)),
      mat3(vec3(1.0, 0.0, 0.0), vec3(0.0, -1.0, 0.0), vec3(0.0, 0.0, -1.0)),
      mat3(vec3(-1.0, 0.0, 0.0), vec3(0.0, -1.0, 0.0), vec3(0.0, 0.0, 1.0)));
  return normalize(CUBE_ROTATIONS[face] * vec3(co * 2.0 - 1.0, 1.0));
}

float area_element(float x, float y)
{
  return atan(x * y, sqrt(x * x + y * y + 1));
}

float texel_solid_angle(vec2 co, float halfpix)
{
  vec2 v1 = (co - vec2(halfpix)) * 2.0 - 1.0;
  vec2 v2 = (co + vec2(halfpix)) * 2.0 - 1.0;

  return area_element(v1.x, v1.y) - area_element(v1.x, v2.y) - area_element(v2.x, v1.y) +
         area_element(v2.x, v2.y);
}

vec3 octahedral_to_cubemap_proj(vec2 co)
{
  co = co * 2.0 - 1.0;

  vec2 abs_co = abs(co);
  vec3 v = vec3(co, 1.0 - (abs_co.x + abs_co.y));

  if (abs_co.x + abs_co.y > 1.0) {
    v.xy = (abs(co.yx) - 1.0) * -sign(co.xy);
  }

  return v;
}

void main()
{
#if defined(IRRADIANCE_SH_L2)
  float pixstep = 1.0 / probeSize;
  float halfpix = pixstep / 2.0;

  /* Downside: leaks negative values, very bandwidth consuming */
  int comp = int(gl_FragCoord.x) % 3 + (int(gl_FragCoord.y) % 3) * 3;

  float weight_accum = 0.0;
  vec3 sh = vec3(0.0);

  for (int face = 0; face < 6; face++) {
    for (float x = halfpix; x < 1.0; x += pixstep) {
      for (float y = halfpix; y < 1.0; y += pixstep) {
        float weight, coef;
        vec2 facecoord = vec2(x, y);
        vec3 cubevec = get_cubemap_vector(facecoord, face);

        if (comp == 0) {
          coef = 0.282095;
        }
        else if (comp == 1) {
          coef = -0.488603 * cubevec.z * 2.0 / 3.0;
        }
        else if (comp == 2) {
          coef = 0.488603 * cubevec.y * 2.0 / 3.0;
        }
        else if (comp == 3) {
          coef = -0.488603 * cubevec.x * 2.0 / 3.0;
        }
        else if (comp == 4) {
          coef = 1.092548 * cubevec.x * cubevec.z * 1.0 / 4.0;
        }
        else if (comp == 5) {
          coef = -1.092548 * cubevec.z * cubevec.y * 1.0 / 4.0;
        }
        else if (comp == 6) {
          coef = 0.315392 * (3.0 * cubevec.y * cubevec.y - 1.0) * 1.0 / 4.0;
        }
        else if (comp == 7) {
          coef = -1.092548 * cubevec.x * cubevec.y * 1.0 / 4.0;
        }
        else { /* (comp == 8) */
          coef = 0.546274 * (cubevec.x * cubevec.x - cubevec.z * cubevec.z) * 1.0 / 4.0;
        }

        weight = texel_solid_angle(facecoord, halfpix);

        vec4 samp = textureLod(probeHdr, cubevec, lodMax);
        sh += samp.rgb * coef * weight;
        weight_accum += weight;
      }
    }
  }
  sh *= M_4PI / weight_accum;

  FragColor = vec4(sh, 1.0);
#else
#  if defined(IRRADIANCE_HL2)
  /* Downside: very very low resolution (6 texels), bleed lighting because of interpolation */
  int x = int(gl_FragCoord.x) % 3;
  int y = int(gl_FragCoord.y) % 2;

  vec3 cubevec = vec3(1.0, 0.0, 0.0);

  if (x == 1) {
    cubevec = cubevec.yxy;
  }
  else if (x == 2) {
    cubevec = cubevec.yyx;
  }

  if (y == 1) {
    cubevec = -cubevec;
  }
#  endif

  vec3 N, T, B, V;

  N = normalize(cubevec);

  make_orthonormal_basis(N, T, B); /* Generate tangent space */

  /* Integrating Envmap */
  float weight = 0.0;
  vec3 out_radiance = vec3(0.0);
  for (float i = 0; i < sampleCount; i++) {
    vec3 Xi = rand2d_to_cylinder(hammersley_2d(i, sampleCount));

    float pdf;
    vec3 L = sample_uniform_hemisphere(Xi, N, T, B, pdf);
    float NL = dot(N, L);

    if (NL > 0.0) {
      /* Coarse Approximation of the mapping distortion
       * Unit Sphere -> Cubemap Face */
      const float dist = 4.0 * M_PI / 6.0;
      /* http://http.developer.nvidia.com/GPUGems3/gpugems3_ch20.html : Equation 13 */
      float lod = clamp(lodFactor - 0.5 * log2(pdf * dist), 0.0, lodMax);

      out_radiance += textureLod(probeHdr, L, lod).rgb * NL;
      weight += NL;
    }
  }

  FragColor = irradiance_encode(intensityFac * out_radiance / weight);
#endif
}
