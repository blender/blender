/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011, Blender Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

CCL_NAMESPACE_BEGIN

ccl_device float fresnel_dielectric(float eta,
                                    const float3 N,
                                    const float3 I,
                                    ccl_private float3 *R,
                                    ccl_private float3 *T,
#ifdef __RAY_DIFFERENTIALS__
                                    const float3 dIdx,
                                    const float3 dIdy,
                                    ccl_private float3 *dRdx,
                                    ccl_private float3 *dRdy,
                                    ccl_private float3 *dTdx,
                                    ccl_private float3 *dTdy,
#endif
                                    ccl_private bool *is_inside)
{
  float cos = dot(N, I), neta;
  float3 Nn;

  // check which side of the surface we are on
  if (cos > 0) {
    // we are on the outside of the surface, going in
    neta = 1 / eta;
    Nn = N;
    *is_inside = false;
  }
  else {
    // we are inside the surface
    cos = -cos;
    neta = eta;
    Nn = -N;
    *is_inside = true;
  }

  // compute reflection
  *R = (2 * cos) * Nn - I;
#ifdef __RAY_DIFFERENTIALS__
  *dRdx = (2 * dot(Nn, dIdx)) * Nn - dIdx;
  *dRdy = (2 * dot(Nn, dIdy)) * Nn - dIdy;
#endif

  float arg = 1 - (neta * neta * (1 - (cos * cos)));
  if (arg < 0) {
    *T = make_float3(0.0f, 0.0f, 0.0f);
#ifdef __RAY_DIFFERENTIALS__
    *dTdx = make_float3(0.0f, 0.0f, 0.0f);
    *dTdy = make_float3(0.0f, 0.0f, 0.0f);
#endif
    return 1;  // total internal reflection
  }
  else {
    float dnp = max(sqrtf(arg), 1e-7f);
    float nK = (neta * cos) - dnp;
    *T = -(neta * I) + (nK * Nn);
#ifdef __RAY_DIFFERENTIALS__
    *dTdx = -(neta * dIdx) + ((neta - neta * neta * cos / dnp) * dot(dIdx, Nn)) * Nn;
    *dTdy = -(neta * dIdy) + ((neta - neta * neta * cos / dnp) * dot(dIdy, Nn)) * Nn;
#endif
    // compute Fresnel terms
    float cosTheta1 = cos;  // N.R
    float cosTheta2 = -dot(Nn, *T);
    float pPara = (cosTheta1 - eta * cosTheta2) / (cosTheta1 + eta * cosTheta2);
    float pPerp = (eta * cosTheta1 - cosTheta2) / (eta * cosTheta1 + cosTheta2);
    return 0.5f * (pPara * pPara + pPerp * pPerp);
  }
}

ccl_device float fresnel_dielectric_cos(float cosi, float eta)
{
  // compute fresnel reflectance without explicitly computing
  // the refracted direction
  float c = fabsf(cosi);
  float g = eta * eta - 1 + c * c;
  if (g > 0) {
    g = sqrtf(g);
    float A = (g - c) / (g + c);
    float B = (c * (g + c) - 1) / (c * (g - c) + 1);
    return 0.5f * A * A * (1 + B * B);
  }
  return 1.0f;  // TIR(no refracted component)
}

ccl_device float3 fresnel_conductor(float cosi, const float3 eta, const float3 k)
{
  float3 cosi2 = make_float3(cosi * cosi, cosi * cosi, cosi * cosi);
  float3 one = make_float3(1.0f, 1.0f, 1.0f);
  float3 tmp_f = eta * eta + k * k;
  float3 tmp = tmp_f * cosi2;
  float3 Rparl2 = (tmp - (2.0f * eta * cosi) + one) / (tmp + (2.0f * eta * cosi) + one);
  float3 Rperp2 = (tmp_f - (2.0f * eta * cosi) + cosi2) / (tmp_f + (2.0f * eta * cosi) + cosi2);
  return (Rparl2 + Rperp2) * 0.5f;
}

ccl_device float schlick_fresnel(float u)
{
  float m = clamp(1.0f - u, 0.0f, 1.0f);
  float m2 = m * m;
  return m2 * m2 * m;  // pow(m, 5)
}

/* Calculate the fresnel color which is a blend between white and the F0 color (cspec0) */
ccl_device_forceinline float3
interpolate_fresnel_color(float3 L, float3 H, float ior, float F0, float3 cspec0)
{
  /* Calculate the fresnel interpolation factor
   * The value from fresnel_dielectric_cos(...) has to be normalized because
   * the cspec0 keeps the F0 color
   */
  float F0_norm = 1.0f / (1.0f - F0);
  float FH = (fresnel_dielectric_cos(dot(L, H), ior) - F0) * F0_norm;

  /* Blend between white and a specular color with respect to the fresnel */
  return cspec0 * (1.0f - FH) + make_float3(1.0f, 1.0f, 1.0f) * FH;
}

ccl_device float3 ensure_valid_reflection(float3 Ng, float3 I, float3 N)
{
  float3 R = 2 * dot(N, I) * N - I;

  /* Reflection rays may always be at least as shallow as the incoming ray. */
  float threshold = min(0.9f * dot(Ng, I), 0.01f);
  if (dot(Ng, R) >= threshold) {
    return N;
  }

  /* Form coordinate system with Ng as the Z axis and N inside the X-Z-plane.
   * The X axis is found by normalizing the component of N that's orthogonal to Ng.
   * The Y axis isn't actually needed.
   */
  float NdotNg = dot(N, Ng);
  float3 X = normalize(N - NdotNg * Ng);

  /* Keep math expressions. */
  /* clang-format off */
  /* Calculate N.z and N.x in the local coordinate system.
   *
   * The goal of this computation is to find a N' that is rotated towards Ng just enough
   * to lift R' above the threshold (here called t), therefore dot(R', Ng) = t.
   *
   * According to the standard reflection equation,
   * this means that we want dot(2*dot(N', I)*N' - I, Ng) = t.
   *
   * Since the Z axis of our local coordinate system is Ng, dot(x, Ng) is just x.z, so we get
   * 2*dot(N', I)*N'.z - I.z = t.
   *
   * The rotation is simple to express in the coordinate system we formed -
   * since N lies in the X-Z-plane, we know that N' will also lie in the X-Z-plane,
   * so N'.y = 0 and therefore dot(N', I) = N'.x*I.x + N'.z*I.z .
   *
   * Furthermore, we want N' to be normalized, so N'.x = sqrt(1 - N'.z^2).
   *
   * With these simplifications,
   * we get the final equation 2*(sqrt(1 - N'.z^2)*I.x + N'.z*I.z)*N'.z - I.z = t.
   *
   * The only unknown here is N'.z, so we can solve for that.
   *
   * The equation has four solutions in general:
   *
   * N'.z = +-sqrt(0.5*(+-sqrt(I.x^2*(I.x^2 + I.z^2 - t^2)) + t*I.z + I.x^2 + I.z^2)/(I.x^2 + I.z^2))
   * We can simplify this expression a bit by grouping terms:
   *
   * a = I.x^2 + I.z^2
   * b = sqrt(I.x^2 * (a - t^2))
   * c = I.z*t + a
   * N'.z = +-sqrt(0.5*(+-b + c)/a)
   *
   * Two solutions can immediately be discarded because they're negative so N' would lie in the
   * lower hemisphere.
   */
  /* clang-format on */

  float Ix = dot(I, X), Iz = dot(I, Ng);
  float Ix2 = sqr(Ix), Iz2 = sqr(Iz);
  float a = Ix2 + Iz2;

  float b = safe_sqrtf(Ix2 * (a - sqr(threshold)));
  float c = Iz * threshold + a;

  /* Evaluate both solutions.
   * In many cases one can be immediately discarded (if N'.z would be imaginary or larger than
   * one), so check for that first. If no option is viable (might happen in extreme cases like N
   * being in the wrong hemisphere), give up and return Ng. */
  float fac = 0.5f / a;
  float N1_z2 = fac * (b + c), N2_z2 = fac * (-b + c);
  bool valid1 = (N1_z2 > 1e-5f) && (N1_z2 <= (1.0f + 1e-5f));
  bool valid2 = (N2_z2 > 1e-5f) && (N2_z2 <= (1.0f + 1e-5f));

  float2 N_new;
  if (valid1 && valid2) {
    /* If both are possible, do the expensive reflection-based check. */
    float2 N1 = make_float2(safe_sqrtf(1.0f - N1_z2), safe_sqrtf(N1_z2));
    float2 N2 = make_float2(safe_sqrtf(1.0f - N2_z2), safe_sqrtf(N2_z2));

    float R1 = 2 * (N1.x * Ix + N1.y * Iz) * N1.y - Iz;
    float R2 = 2 * (N2.x * Ix + N2.y * Iz) * N2.y - Iz;

    valid1 = (R1 >= 1e-5f);
    valid2 = (R2 >= 1e-5f);
    if (valid1 && valid2) {
      /* If both solutions are valid, return the one with the shallower reflection since it will be
       * closer to the input (if the original reflection wasn't shallow, we would not be in this
       * part of the function). */
      N_new = (R1 < R2) ? N1 : N2;
    }
    else {
      /* If only one reflection is valid (= positive), pick that one. */
      N_new = (R1 > R2) ? N1 : N2;
    }
  }
  else if (valid1 || valid2) {
    /* Only one solution passes the N'.z criterium, so pick that one. */
    float Nz2 = valid1 ? N1_z2 : N2_z2;
    N_new = make_float2(safe_sqrtf(1.0f - Nz2), safe_sqrtf(Nz2));
  }
  else {
    return Ng;
  }

  return N_new.x * X + N_new.y * Ng;
}

CCL_NAMESPACE_END
