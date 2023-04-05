/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

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

#endif /* CCL_STDOSL_H */
