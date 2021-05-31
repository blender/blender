/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.  All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Sony Pictures Imageworks nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/////////////////////////////////////////////////////////////////////////////

#ifndef CCL_STDCYCLESOSL_H
#define CCL_STDCYCLESOSL_H

#include "stdosl.h"

// Declaration of built-in functions and closures, stdosl.h does not make
// these available so we have to redefine them.
#define BUILTIN [[int builtin = 1]]
#define BUILTIN_DERIV [[ int builtin = 1, int deriv = 1 ]]

closure color diffuse_ramp(normal N, color colors[8]) BUILTIN;
closure color phong_ramp(normal N, float exponent, color colors[8]) BUILTIN;
closure color diffuse_toon(normal N, float size, float smooth) BUILTIN;
closure color glossy_toon(normal N, float size, float smooth) BUILTIN;
closure color microfacet_ggx(normal N, float ag) BUILTIN;
closure color microfacet_ggx_aniso(normal N, vector T, float ax, float ay) BUILTIN;
closure color microfacet_ggx_refraction(normal N, float ag, float eta) BUILTIN;
closure color microfacet_multi_ggx(normal N, float ag, color C) BUILTIN;
closure color microfacet_multi_ggx_aniso(normal N, vector T, float ax, float ay, color C) BUILTIN;
closure color microfacet_multi_ggx_glass(normal N, float ag, float eta, color C) BUILTIN;
closure color microfacet_ggx_fresnel(normal N, float ag, float eta, color C, color Cspec0) BUILTIN;
closure color microfacet_ggx_aniso_fresnel(
    normal N, vector T, float ax, float ay, float eta, color C, color Cspec0) BUILTIN;
closure color
microfacet_multi_ggx_fresnel(normal N, float ag, float eta, color C, color Cspec0) BUILTIN;
closure color microfacet_multi_ggx_aniso_fresnel(
    normal N, vector T, float ax, float ay, float eta, color C, color Cspec0) BUILTIN;
closure color
microfacet_multi_ggx_glass_fresnel(normal N, float ag, float eta, color C, color Cspec0) BUILTIN;
closure color microfacet_beckmann(normal N, float ab) BUILTIN;
closure color microfacet_beckmann_aniso(normal N, vector T, float ax, float ay) BUILTIN;
closure color microfacet_beckmann_refraction(normal N, float ab, float eta) BUILTIN;
closure color ashikhmin_shirley(normal N, vector T, float ax, float ay) BUILTIN;
closure color ashikhmin_velvet(normal N, float sigma) BUILTIN;
closure color ambient_occlusion() BUILTIN;
closure color principled_diffuse(normal N, float roughness) BUILTIN;
closure color principled_sheen(normal N) BUILTIN;
closure color principled_clearcoat(normal N, float clearcoat, float clearcoat_roughness) BUILTIN;

// BSSRDF
closure color bssrdf(string method, normal N, vector radius, color albedo) BUILTIN;

// Hair
closure color
hair_reflection(normal N, float roughnessu, float roughnessv, vector T, float offset) BUILTIN;
closure color
hair_transmission(normal N, float roughnessu, float roughnessv, vector T, float offset) BUILTIN;
closure color principled_hair(normal N,
                              color sigma,
                              float roughnessu,
                              float roughnessv,
                              float coat,
                              float alpha,
                              float eta) BUILTIN;

// Volume
closure color henyey_greenstein(float g) BUILTIN;
closure color absorption() BUILTIN;

normal ensure_valid_reflection(normal Ng, vector I, normal N)
{
  /* The implementation here mirrors the one in kernel_montecarlo.h,
   * check there for an explanation of the algorithm. */

  float sqr(float x)
  {
    return x * x;
  }

  vector R = 2 * dot(N, I) * N - I;

  float threshold = min(0.9 * dot(Ng, I), 0.01);
  if (dot(Ng, R) >= threshold) {
    return N;
  }

  float NdotNg = dot(N, Ng);
  vector X = normalize(N - NdotNg * Ng);

  float Ix = dot(I, X), Iz = dot(I, Ng);
  float Ix2 = sqr(Ix), Iz2 = sqr(Iz);
  float a = Ix2 + Iz2;

  float b = sqrt(Ix2 * (a - sqr(threshold)));
  float c = Iz * threshold + a;

  float fac = 0.5 / a;
  float N1_z2 = fac * (b + c), N2_z2 = fac * (-b + c);
  int valid1 = (N1_z2 > 1e-5) && (N1_z2 <= (1.0 + 1e-5));
  int valid2 = (N2_z2 > 1e-5) && (N2_z2 <= (1.0 + 1e-5));

  float N_new_x, N_new_z;
  if (valid1 && valid2) {
    float N1_x = sqrt(1.0 - N1_z2), N1_z = sqrt(N1_z2);
    float N2_x = sqrt(1.0 - N2_z2), N2_z = sqrt(N2_z2);

    float R1 = 2 * (N1_x * Ix + N1_z * Iz) * N1_z - Iz;
    float R2 = 2 * (N2_x * Ix + N2_z * Iz) * N2_z - Iz;

    valid1 = (R1 >= 1e-5);
    valid2 = (R2 >= 1e-5);
    if (valid1 && valid2) {
      N_new_x = (R1 < R2) ? N1_x : N2_x;
      N_new_z = (R1 < R2) ? N1_z : N2_z;
    }
    else {
      N_new_x = (R1 > R2) ? N1_x : N2_x;
      N_new_z = (R1 > R2) ? N1_z : N2_z;
    }
  }
  else if (valid1 || valid2) {
    float Nz2 = valid1 ? N1_z2 : N2_z2;
    N_new_x = sqrt(1.0 - Nz2);
    N_new_z = sqrt(Nz2);
  }
  else {
    return Ng;
  }

  return N_new_x * X + N_new_z * Ng;
}

#endif /* CCL_STDOSL_H */
