/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#ifndef CCL_STDCYCLESOSL_H
#define CCL_STDCYCLESOSL_H

#include "stdosl.h"

// Constants
#define FLT_MAX 3.402823466e+38  // max value

// Declaration of built-in functions and closures, stdosl.h does not make
// these available so we have to redefine them.
#define BUILTIN [[int builtin = 1]]
#define BUILTIN_DERIV [[ int builtin = 1, int deriv = 1 ]]

closure color diffuse_ramp(normal N, color colors[8]) BUILTIN;
closure color phong_ramp(normal N, float exponent, color colors[8]) BUILTIN;
closure color diffuse_toon(normal N, float size, float smooth) BUILTIN;
closure color glossy_toon(normal N, float size, float smooth) BUILTIN;
closure color ashikhmin_velvet(normal N, float sigma) BUILTIN;
closure color sheen(normal N, float roughness) BUILTIN;
closure color ambient_occlusion() BUILTIN;
closure color principled_diffuse(normal N, float roughness) BUILTIN;

/* Needed to pass along the color for multi-scattering saturation adjustment,
 * otherwise could be replaced by microfacet() */
closure color microfacet_multi_ggx_glass(normal N, float ag, float eta, color C) BUILTIN;
closure color microfacet_multi_ggx_aniso(normal N, vector T, float ax, float ay, color C) BUILTIN;
/* Needed to pass along the IOR for the Principled V1 Fresnel calculation,
 * otherwise could be replaced by generalized_schlick_bsdf() */
closure color microfacet_aniso_fresnel(
    normal N, vector T, float ax, float ay, color f0, color f90, float eta, string dist) BUILTIN;

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

#endif /* CCL_STDOSL_H */
