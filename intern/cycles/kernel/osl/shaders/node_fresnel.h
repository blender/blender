/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

float fresnel_dielectric_cos(float cosi, float eta)
{
  /* compute fresnel reflectance without explicitly computing
   * the refracted direction */
  float c = fabs(cosi);
  float g = eta * eta - 1 + c * c;
  float result;

  if (g > 0) {
    g = sqrt(g);
    float A = (g - c) / (g + c);
    float B = (c * (g + c) - 1) / (c * (g - c) + 1);
    result = 0.5 * A * A * (1 + B * B);
  }
  else
    result = 1.0; /* TIR (no refracted component) */

  return result;
}

color fresnel_conductor(float cosi, color eta, color k)
{
  color cosi2 = color(cosi * cosi);
  color one = color(1, 1, 1);
  color tmp_f = eta * eta + k * k;
  color tmp = tmp_f * cosi2;
  color Rparl2 = (tmp - (2.0 * eta * cosi) + one) / (tmp + (2.0 * eta * cosi) + one);
  color Rperp2 = (tmp_f - (2.0 * eta * cosi) + cosi2) / (tmp_f + (2.0 * eta * cosi) + cosi2);
  return (Rparl2 + Rperp2) * 0.5;
}

float F0_from_ior(float eta)
{
  float f0 = (eta - 1.0) / (eta + 1.0);
  return f0 * f0;
}
