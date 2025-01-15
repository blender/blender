/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <map>

#include "util/string.h"

#include "util/array.h"
#include "util/hash.h"
#include "util/tbb.h"

#include "kernel/closure/bsdf_microfacet.h"
#include "kernel/sample/sobol_burley.h"

#include <iostream>

CCL_NAMESPACE_BEGIN

static float precompute_ggx_E(const float rough, const float mu, const float3 rand)
{
  MicrofacetBsdf bsdf;
  bsdf.weight = one_float3();
  bsdf.sample_weight = 1.0f;
  bsdf.N = make_float3(0.0f, 0.0f, 1.0f);
  bsdf.alpha_x = bsdf.alpha_y = sqr(rough);
  bsdf.ior = 1.0f;
  bsdf.T = make_float3(1.0f, 0.0f, 0.0f);

  bsdf_microfacet_ggx_setup(&bsdf);

  float3 omega_in;
  Spectrum eval;
  float pdf = 0.0f;
  float sampled_eta;
  float2 sampled_roughness;
  bsdf_microfacet_ggx_sample(nullptr,
                             (ShaderClosure *)&bsdf,
                             make_float3(0.0f, 0.0f, 1.0f),
                             make_float3(sqrtf(1.0f - sqr(mu)), 0.0f, mu),
                             rand,
                             &eval,
                             &omega_in,
                             &pdf,
                             &sampled_roughness,
                             &sampled_eta);
  if (pdf != 0.0f) {
    return average(eval) / pdf;
  }
  return 0.0f;
}

static float precompute_ggx_glass_E(const float rough,
                                    const float mu,
                                    const float eta,
                                    const float3 rand)
{
  MicrofacetBsdf bsdf;
  bsdf.weight = one_float3();
  bsdf.sample_weight = 1.0f;
  bsdf.N = make_float3(0.0f, 0.0f, 1.0f);
  bsdf.alpha_x = bsdf.alpha_y = sqr(rough);
  bsdf.ior = eta;
  bsdf.T = make_float3(1.0f, 0.0f, 0.0f);

  bsdf_microfacet_ggx_glass_setup(&bsdf);

  float3 omega_in;
  Spectrum eval;
  float pdf = 0.0f;
  float sampled_eta;
  float2 sampled_roughness;
  bsdf_microfacet_ggx_sample(nullptr,
                             (ShaderClosure *)&bsdf,
                             make_float3(0.0f, 0.0f, 1.0f),
                             make_float3(sqrtf(1.0f - sqr(mu)), 0.0f, mu),
                             rand,
                             &eval,
                             &omega_in,
                             &pdf,
                             &sampled_roughness,
                             &sampled_eta);
  if (pdf != 0.0f) {
    return average(eval) / pdf;
  }
  return 0.0f;
}

static float precompute_ggx_gen_schlick_s(
    const float rough, const float mu, const float eta, const float exponent, const float3 rand)
{
  MicrofacetBsdf bsdf;
  bsdf.weight = one_float3();
  bsdf.sample_weight = 1.0f;
  bsdf.N = make_float3(0.0f, 0.0f, 1.0f);
  bsdf.alpha_x = bsdf.alpha_y = sqr(rough);
  bsdf.ior = eta;
  bsdf.T = make_float3(1.0f, 0.0f, 0.0f);

  bsdf_microfacet_ggx_setup(&bsdf);

  FresnelGeneralizedSchlick fresnel;
  fresnel.reflection_tint = one_float3();
  fresnel.transmission_tint = one_float3();
  fresnel.f0 = make_float3(0.0f, 1.0f, 0.0f);
  fresnel.f90 = make_float3(1.0f, 1.0f, 0.0f);
  fresnel.exponent = exponent;

  bsdf.fresnel_type = MicrofacetFresnel::GENERALIZED_SCHLICK;
  bsdf.fresnel = &fresnel;

  float3 omega_in;
  Spectrum eval;
  float pdf = 0.0f;
  float sampled_eta;
  float2 sampled_roughness;
  bsdf_microfacet_ggx_sample(nullptr,
                             (ShaderClosure *)&bsdf,
                             make_float3(0.0f, 0.0f, 1.0f),
                             make_float3(sqrtf(1.0f - sqr(mu)), 0.0f, mu),
                             rand,
                             &eval,
                             &omega_in,
                             &pdf,
                             &sampled_roughness,
                             &sampled_eta);
  if (pdf != 0.0f) {
    /* The idea here is that the resulting Fresnel factor is always bounded by
     * F0..F90, so it's enough to precompute and store the interpolation factor. */
    return saturatef(eval.x / eval.y);
  }
  return 0.0f;
}

inline float ior_parametrization(const float z)
{
  /* This parametrization ensures that the entire [1..inf] range of IORs is covered
   * and that most precision is allocated to the common areas (1-2). */
  return ior_from_F0(sqr(sqr(z)));
}

struct PrecomputeTerm {
  int samples;
  int nx, ny, nz;
  std::function<float(float, float, float, float3)> evaluation;
};

static bool cycles_precompute(std::string name)
{
  std::map<string, PrecomputeTerm> precompute_terms;
  /* Overall albedo of the GGX microfacet BRDF, depending on cosI and roughness. */
  precompute_terms["ggx_E"] = {
      1 << 23, 32, 32, 1, [](const float rough, const float mu, float, const float3 rand) {
        return precompute_ggx_E(rough, mu, rand);
      }};
  /* Overall albedo of the GGX microfacet BRDF, averaged over cosI */
  precompute_terms["ggx_Eavg"] = {
      1 << 26, 32, 1, 1, [](const float rough, const float mu, float, const float3 rand) {
        return 2.0f * mu * precompute_ggx_E(rough, mu, rand);
      }};
  /* Overall albedo of the GGX microfacet BSDF with dielectric Fresnel,
   * depending on cosI and roughness, for IOR>1. */
  precompute_terms["ggx_glass_E"] = {
      1 << 23,
      16,
      16,
      16,
      [](const float rough, const float mu, const float z, const float3 rand) {
        const float ior = ior_parametrization(z);
        return precompute_ggx_glass_E(rough, mu, ior, rand);
      }};
  /* Overall albedo of the GGX microfacet BSDF with dielectric Fresnel,
   * averaged over cosI, for IOR>1. */
  precompute_terms["ggx_glass_Eavg"] = {
      1 << 26, 16, 1, 16, [](const float rough, const float mu, const float z, const float3 rand) {
        const float ior = ior_parametrization(z);
        return 2.0f * mu * precompute_ggx_glass_E(rough, mu, ior, rand);
      }};
  /* Overall albedo of the GGX microfacet BSDF with dielectric Fresnel,
   * depending on cosI and roughness, for IOR<1. */
  precompute_terms["ggx_glass_inv_E"] = {
      1 << 23,
      16,
      16,
      16,
      [](const float rough, const float mu, const float z, const float3 rand) {
        const float ior = ior_parametrization(z);
        return precompute_ggx_glass_E(rough, mu, 1.0f / ior, rand);
      }};
  /* Overall albedo of the GGX microfacet BSDF with dielectric Fresnel,
   * averaged over cosI, for IOR<1. */
  precompute_terms["ggx_glass_inv_Eavg"] = {
      1 << 26, 16, 1, 16, [](const float rough, const float mu, const float z, const float3 rand) {
        const float ior = ior_parametrization(z);
        return 2.0f * mu * precompute_ggx_glass_E(rough, mu, 1.0f / ior, rand);
      }};

  /* Interpolation factor between F0 and F90 for the generalized Schlick Fresnel,
   * depending on cosI and roughness, for IOR>1, using dielectric Fresnel mode. */
  precompute_terms["ggx_gen_schlick_ior_s"] = {
      1 << 20,
      16,
      16,
      16,
      [](const float rough, const float mu, const float z, const float3 rand) {
        const float ior = ior_parametrization(z);
        return precompute_ggx_gen_schlick_s(rough, mu, ior, -1.0f, rand);
      }};

  /* Interpolation factor between F0 and F90 for the generalized Schlick Fresnel,
   * depending on cosI and roughness, for IOR>1. */
  precompute_terms["ggx_gen_schlick_s"] = {
      1 << 20,
      16,
      16,
      16,
      [](const float rough, const float mu, const float z, const float3 rand) {
        /* Remap 0..1 to 0..inf, with 0.5 mapping to 5 (the default value). */
        const float exponent = 5.0f * ((1.0f - z) / z);
        return precompute_ggx_gen_schlick_s(rough, mu, 1.0f, exponent, rand);
      }};

  if (precompute_terms.count(name) == 0) {
    return false;
  }

  const PrecomputeTerm &term = precompute_terms[name];

  const int samples = term.samples;
  const int nz = term.nz;
  const int ny = term.ny;
  const int nx = term.nx;

  std::cout << "static const float table_" << name << "[" << nz * ny * nx << "] = {" << std::endl;
  for (int z = 0; z < nz; z++) {
    array<float> data(nx * ny);
    parallel_for(0, nx * ny, [&](int64_t i) {
      const int y = i / nx;
      const int x = i % nx;
      const uint seed = hash_uint2(x, y);
      double sum = 0.0;
      for (int sample = 0; sample < samples; sample++) {
        const float4 rand = sobol_burley_sample_4D(sample, 0, seed, 0xffffffff);

        const float rough = (nx == 1) ? 0.0f : clamp(float(x) / float(nx - 1), 1e-4f, 1.0f);
        const float mu = (ny == 1) ? rand.w : clamp(float(y) / float(ny - 1), 1e-4f, 1.0f);
        const float ior = (nz == 1) ? 0.0f : clamp(float(z) / float(nz - 1), 1e-4f, 0.99f);

        float value = term.evaluation(rough, mu, ior, make_float3(rand));
        if (isnan(value)) {
          value = 0.0f;
        }
        sum += (double)value;
      }
      data[y * nx + x] = saturatef(float(sum / double(samples)));
    });

    /* Print data formatted as C++ array */
    for (int y = 0; y < ny; y++) {
      std::cout << "  ";
      for (int x = 0; x < nx; x++) {
        std::cout << std::to_string(data[y * nx + x]);
        if (x + 1 < nx) {
          /* Next number will follow in same line */
          std::cout << "f, ";
        }
        else if (y + 1 < ny || z + 1 < nz) {
          /* Next number will follow in next line */
          std::cout << "f,";
        }
        else {
          /* No next number */
          std::cout << "f";
        }
      }
      std::cout << std::endl;
    }
    /* If the array is three-dimensional, put an empty line between each slice. */
    if (ny > 1 && z + 1 < nz) {
      std::cout << std::endl;
    }
  }
  std::cout << "};" << std::endl;

  return true;
}

CCL_NAMESPACE_END

int main(const int argc, const char **argv)
{
  if (argc < 2) {
    return 1;
  }
  return ccl::cycles_precompute(argv[1]) ? 0 : 1;
}
