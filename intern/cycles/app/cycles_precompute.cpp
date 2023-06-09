#include "util/math.h"
#include "util/string.h"
#include "util/system.h"

#include "util/array.h"
#include "util/hash.h"
#include "util/task.h"

#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"

#include "kernel/sample/lcg.h"
#include "kernel/sample/mapping.h"

#include "kernel/closure/bsdf_microfacet.h"

#include <iostream>

CCL_NAMESPACE_BEGIN

static float precompute_ggx_E(float rough, float mu, float3 rand)
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
  float pdf = 0.0f, sampled_eta;
  float2 sampled_roughness;
  bsdf_microfacet_ggx_sample((ShaderClosure *)&bsdf,
                             0,
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

static float precompute_ggx_glass_E(float rough, float mu, float eta, float3 rand)
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
  float pdf = 0.0f, sampled_eta;
  float2 sampled_roughness;
  bsdf_microfacet_ggx_sample((ShaderClosure *)&bsdf,
                             0,
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

struct PrecomputeTerm {
  int samples;
  int nx, ny, nz;
  std::function<float(float, float, float, float3)> evaluation;
};

static bool cycles_precompute(std::string name)
{
  std::map<string, PrecomputeTerm> precompute_terms;
  precompute_terms["ggx_E"] = {
      1 << 23, 32, 32, 1, [](float rough, float mu, float ior, float3 rand) {
        return precompute_ggx_E(rough, mu, rand);
      }};
  precompute_terms["ggx_Eavg"] = {
      1 << 26, 32, 1, 1, [](float rough, float mu, float ior, float3 rand) {
        return 2.0f * mu * precompute_ggx_E(rough, mu, rand);
      }};
  precompute_terms["ggx_glass_E"] = {
      1 << 23, 16, 16, 16, [](float rough, float mu, float ior, float3 rand) {
        return precompute_ggx_glass_E(rough, mu, ior, rand);
      }};
  precompute_terms["ggx_glass_Eavg"] = {
      1 << 26, 16, 1, 16, [](float rough, float mu, float ior, float3 rand) {
        return 2.0f * mu * precompute_ggx_glass_E(rough, mu, ior, rand);
      }};
  precompute_terms["ggx_glass_inv_E"] = {
      1 << 23, 16, 16, 16, [](float rough, float mu, float ior, float3 rand) {
        return precompute_ggx_glass_E(rough, mu, 1.0f / ior, rand);
      }};
  precompute_terms["ggx_glass_inv_Eavg"] = {
      1 << 26, 16, 1, 16, [](float rough, float mu, float ior, float3 rand) {
        return 2.0f * mu * precompute_ggx_glass_E(rough, mu, 1.0f / ior, rand);
      }};

  if (precompute_terms.count(name) == 0) {
    return false;
  }

  const PrecomputeTerm &term = precompute_terms[name];

  const int samples = term.samples;
  const int nz = term.nz, ny = term.ny, nx = term.nx;

  std::cout << "static const float table_" << name << "[" << nz * ny * nx << "] = {" << std::endl;
  for (int z = 0; z < nz; z++) {
    array<float> data(nx * ny);
    parallel_for(0, nx * ny, [&](int64_t i) {
      int y = i / nx, x = i % nx;
      uint seed = hash_uint2(x, y);
      double sum = 0.0;
      for (int sample = 0; sample < samples; sample++) {
        float4 rand = sobol_burley_sample_4D(sample, 0, seed, 0xffffffff);

        float rough = (nx == 1) ? 0.0f : clamp(float(x) / float(nx - 1), 1e-4f, 1.0f);
        float mu = (ny == 1) ? rand.w : clamp(float(y) / float(ny - 1), 1e-4f, 1.0f);
        float ior = (nz == 1) ? 0.0f : clamp(float(z) / float(nz - 1), 1e-4f, 0.99f);
        /* This parametrization ensures that the entire [1..inf] range of IORs is covered
         * and that most precision is allocated to the common areas (1-2). */
        ior = ior_from_F0(sqr(sqr(ior)));

        float value = term.evaluation(rough, mu, ior, float4_to_float3(rand));
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

int main(int argc, const char **argv)
{
  if (argc < 2) {
    return 1;
  }
  return ccl::cycles_precompute(argv[1]) ? 0 : 1;
}
