/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef OSL_CLOSURE_STRUCT_BEGIN
#  define OSL_CLOSURE_STRUCT_BEGIN(Upper, lower)
#endif
#ifndef OSL_CLOSURE_STRUCT_END
#  define OSL_CLOSURE_STRUCT_END(Upper, lower)
#endif
#ifndef OSL_CLOSURE_STRUCT_MEMBER
#  define OSL_CLOSURE_STRUCT_MEMBER(Upper, TYPE, type, name, key)
#endif
#ifndef OSL_CLOSURE_STRUCT_ARRAY_MEMBER
#  define OSL_CLOSURE_STRUCT_ARRAY_MEMBER(Upper, TYPE, type, name, key, size)
#endif

OSL_CLOSURE_STRUCT_BEGIN(Diffuse, diffuse)
  OSL_CLOSURE_STRUCT_MEMBER(Diffuse, VECTOR, packed_float3, N, NULL)
OSL_CLOSURE_STRUCT_END(Diffuse, diffuse)

OSL_CLOSURE_STRUCT_BEGIN(OrenNayar, oren_nayar)
  OSL_CLOSURE_STRUCT_MEMBER(OrenNayar, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(OrenNayar, FLOAT, float, roughness, NULL)
OSL_CLOSURE_STRUCT_END(OrenNayar, oren_nayar)

OSL_CLOSURE_STRUCT_BEGIN(Translucent, translucent)
  OSL_CLOSURE_STRUCT_MEMBER(Translucent, VECTOR, packed_float3, N, NULL)
OSL_CLOSURE_STRUCT_END(Translucent, translucent)

OSL_CLOSURE_STRUCT_BEGIN(Reflection, reflection)
  OSL_CLOSURE_STRUCT_MEMBER(Reflection, VECTOR, packed_float3, N, NULL)
OSL_CLOSURE_STRUCT_END(Reflection, reflection)

OSL_CLOSURE_STRUCT_BEGIN(Refraction, refraction)
  OSL_CLOSURE_STRUCT_MEMBER(Refraction, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(Refraction, FLOAT, float, ior, NULL)
OSL_CLOSURE_STRUCT_END(Refraction, refraction)

OSL_CLOSURE_STRUCT_BEGIN(Transparent, transparent)
OSL_CLOSURE_STRUCT_END(Transparent, transparent)

OSL_CLOSURE_STRUCT_BEGIN(Microfacet, microfacet)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, STRING, DeviceString, distribution, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, VECTOR, packed_float3, T, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, FLOAT, float, alpha_y, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, FLOAT, float, ior, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, INT, int, refract, NULL)
OSL_CLOSURE_STRUCT_END(Microfacet, microfacet)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetGGXIsotropic, microfacet_ggx)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXIsotropic, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXIsotropic, FLOAT, float, alpha_x, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetGGXIsotropic, microfacet_ggx)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetGGX, microfacet_ggx_aniso)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGX, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGX, VECTOR, packed_float3, T, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGX, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGX, FLOAT, float, alpha_y, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetGGX, microfacet_ggx_aniso)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetGGXRefraction, microfacet_ggx_refraction)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXRefraction, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXRefraction, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXRefraction, FLOAT, float, ior, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetGGXRefraction, microfacet_ggx_refraction)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetMultiGGX, microfacet_multi_ggx)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGX, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGX, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGX, VECTOR, packed_float3, color, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetMultiGGX, microfacet_multi_ggx)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetMultiGGXGlass, microfacet_multi_ggx_glass)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlass, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlass, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlass, FLOAT, float, ior, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlass, VECTOR, packed_float3, color, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetMultiGGXGlass, microfacet_multi_ggx_glass)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetMultiGGXAniso, microfacet_multi_ggx_aniso)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXAniso, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXAniso, VECTOR, packed_float3, T, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXAniso, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXAniso, FLOAT, float, alpha_y, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXAniso, VECTOR, packed_float3, color, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetMultiGGXAniso, microfacet_multi_ggx_aniso)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetGGXFresnel, microfacet_ggx_fresnel)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXFresnel, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXFresnel, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXFresnel, FLOAT, float, ior, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXFresnel, VECTOR, packed_float3, color, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXFresnel, VECTOR, packed_float3, cspec0, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetGGXFresnel, microfacet_ggx_fresnel)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetGGXAnisoFresnel, microfacet_ggx_aniso_fresnel)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXAnisoFresnel, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXAnisoFresnel, VECTOR, packed_float3, T, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXAnisoFresnel, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXAnisoFresnel, FLOAT, float, alpha_y, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXAnisoFresnel, FLOAT, float, ior, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXAnisoFresnel, VECTOR, packed_float3, color, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetGGXAnisoFresnel, VECTOR, packed_float3, cspec0, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetGGXAnisoFresnel, microfacet_ggx_aniso_fresnel)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetMultiGGXFresnel, microfacet_multi_ggx_fresnel)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXFresnel, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXFresnel, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXFresnel, FLOAT, float, ior, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXFresnel, VECTOR, packed_float3, color, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXFresnel, VECTOR, packed_float3, cspec0, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetMultiGGXFresnel, microfacet_multi_ggx_fresnel)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetMultiGGXGlassFresnel, microfacet_multi_ggx_glass_fresnel)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlassFresnel, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlassFresnel, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlassFresnel, FLOAT, float, ior, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlassFresnel, VECTOR, packed_float3, color, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlassFresnel, VECTOR, packed_float3, cspec0, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetMultiGGXGlassFresnel, microfacet_multi_ggx_glass_fresnel)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetMultiGGXAnisoFresnel, microfacet_multi_ggx_aniso_fresnel)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXAnisoFresnel, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXAnisoFresnel, VECTOR, packed_float3, T, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXAnisoFresnel, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXAnisoFresnel, FLOAT, float, alpha_y, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXAnisoFresnel, FLOAT, float, ior, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXAnisoFresnel, VECTOR, packed_float3, color, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXAnisoFresnel, VECTOR, packed_float3, cspec0, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetMultiGGXAnisoFresnel, microfacet_multi_ggx_aniso_fresnel)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetBeckmannIsotropic, microfacet_beckmann)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetBeckmannIsotropic, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetBeckmannIsotropic, FLOAT, float, alpha_x, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetBeckmannIsotropic, microfacet_beckmann)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetBeckmann, microfacet_beckmann_aniso)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetBeckmann, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetBeckmann, VECTOR, packed_float3, T, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetBeckmann, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetBeckmann, FLOAT, float, alpha_y, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetBeckmann, microfacet_beckmann_aniso)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetBeckmannRefraction, microfacet_beckmann_refraction)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetBeckmannRefraction, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetBeckmannRefraction, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetBeckmannRefraction, FLOAT, float, ior, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetBeckmannRefraction, microfacet_beckmann_refraction)

OSL_CLOSURE_STRUCT_BEGIN(AshikhminShirley, ashikhmin_shirley)
  OSL_CLOSURE_STRUCT_MEMBER(AshikhminShirley, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(AshikhminShirley, VECTOR, packed_float3, T, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(AshikhminShirley, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(AshikhminShirley, FLOAT, float, alpha_y, NULL)
OSL_CLOSURE_STRUCT_END(AshikhminShirley, ashikhmin_shirley)

OSL_CLOSURE_STRUCT_BEGIN(AshikhminVelvet, ashikhmin_velvet)
  OSL_CLOSURE_STRUCT_MEMBER(AshikhminVelvet, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(AshikhminVelvet, FLOAT, float, sigma, NULL)
OSL_CLOSURE_STRUCT_END(AshikhminVelvet, ashikhmin_velvet)

OSL_CLOSURE_STRUCT_BEGIN(DiffuseToon, diffuse_toon)
  OSL_CLOSURE_STRUCT_MEMBER(DiffuseToon, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(DiffuseToon, FLOAT, float, size, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(DiffuseToon, FLOAT, float, smooth, NULL)
OSL_CLOSURE_STRUCT_END(DiffuseToon, diffuse_toon)

OSL_CLOSURE_STRUCT_BEGIN(GlossyToon, glossy_toon)
  OSL_CLOSURE_STRUCT_MEMBER(GlossyToon, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(GlossyToon, FLOAT, float, size, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(GlossyToon, FLOAT, float, smooth, NULL)
OSL_CLOSURE_STRUCT_END(GlossyToon, glossy_toon)

OSL_CLOSURE_STRUCT_BEGIN(PrincipledDiffuse, principled_diffuse)
  OSL_CLOSURE_STRUCT_MEMBER(PrincipledDiffuse, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(PrincipledDiffuse, FLOAT, float, roughness, NULL)
OSL_CLOSURE_STRUCT_END(PrincipledDiffuse, principled_diffuse)

OSL_CLOSURE_STRUCT_BEGIN(PrincipledSheen, principled_sheen)
  OSL_CLOSURE_STRUCT_MEMBER(PrincipledSheen, VECTOR, packed_float3, N, NULL)
OSL_CLOSURE_STRUCT_END(PrincipledSheen, principled_sheen)

OSL_CLOSURE_STRUCT_BEGIN(PrincipledClearcoat, principled_clearcoat)
  OSL_CLOSURE_STRUCT_MEMBER(PrincipledClearcoat, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(PrincipledClearcoat, FLOAT, float, clearcoat, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(PrincipledClearcoat, FLOAT, float, clearcoat_roughness, NULL)
OSL_CLOSURE_STRUCT_END(PrincipledClearcoat, principled_clearcoat)

OSL_CLOSURE_STRUCT_BEGIN(GenericEmissive, emission)
OSL_CLOSURE_STRUCT_END(GenericEmissive, emission)

OSL_CLOSURE_STRUCT_BEGIN(GenericBackground, background)
OSL_CLOSURE_STRUCT_END(GenericBackground, background)

OSL_CLOSURE_STRUCT_BEGIN(Holdout, holdout)
OSL_CLOSURE_STRUCT_END(Holdout, holdout)

OSL_CLOSURE_STRUCT_BEGIN(DiffuseRamp, diffuse_ramp)
  OSL_CLOSURE_STRUCT_MEMBER(DiffuseRamp, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_ARRAY_MEMBER(DiffuseRamp, COLOR, packed_float3, colors, NULL, 8)
OSL_CLOSURE_STRUCT_END(DiffuseRamp, diffuse_ramp)

OSL_CLOSURE_STRUCT_BEGIN(PhongRamp, phong_ramp)
  OSL_CLOSURE_STRUCT_MEMBER(PhongRamp, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(PhongRamp, FLOAT, float, exponent, NULL)
  OSL_CLOSURE_STRUCT_ARRAY_MEMBER(PhongRamp, COLOR, packed_float3, colors, NULL, 8)
OSL_CLOSURE_STRUCT_END(PhongRamp, phong_ramp)

OSL_CLOSURE_STRUCT_BEGIN(BSSRDF, bssrdf)
  OSL_CLOSURE_STRUCT_MEMBER(BSSRDF, STRING, DeviceString, method, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(BSSRDF, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(BSSRDF, VECTOR, packed_float3, radius, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(BSSRDF, VECTOR, packed_float3, albedo, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(BSSRDF, FLOAT, float, roughness, "roughness")
  OSL_CLOSURE_STRUCT_MEMBER(BSSRDF, FLOAT, float, ior, "ior")
  OSL_CLOSURE_STRUCT_MEMBER(BSSRDF, FLOAT, float, anisotropy, "anisotropy")
OSL_CLOSURE_STRUCT_END(BSSRDF, bssrdf)

OSL_CLOSURE_STRUCT_BEGIN(HairReflection, hair_reflection)
  OSL_CLOSURE_STRUCT_MEMBER(HairReflection, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HairReflection, FLOAT, float, roughness1, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HairReflection, FLOAT, float, roughness2, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HairReflection, VECTOR, packed_float3, T, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HairReflection, FLOAT, float, offset, NULL)
OSL_CLOSURE_STRUCT_END(HairReflection, hair_reflection)

OSL_CLOSURE_STRUCT_BEGIN(HairTransmission, hair_transmission)
  OSL_CLOSURE_STRUCT_MEMBER(HairTransmission, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HairTransmission, FLOAT, float, roughness1, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HairTransmission, FLOAT, float, roughness2, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HairReflection, VECTOR, packed_float3, T, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HairReflection, FLOAT, float, offset, NULL)
OSL_CLOSURE_STRUCT_END(HairTransmission, hair_transmission)

OSL_CLOSURE_STRUCT_BEGIN(PrincipledHair, principled_hair)
  OSL_CLOSURE_STRUCT_MEMBER(PrincipledHair, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(PrincipledHair, VECTOR, packed_float3, sigma, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(PrincipledHair, FLOAT, float, v, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(PrincipledHair, FLOAT, float, s, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(PrincipledHair, FLOAT, float, m0_roughness, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(PrincipledHair, FLOAT, float, alpha, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(PrincipledHair, FLOAT, float, eta, NULL)
OSL_CLOSURE_STRUCT_END(PrincipledHair, principled_hair)

OSL_CLOSURE_STRUCT_BEGIN(VolumeAbsorption, absorption)
OSL_CLOSURE_STRUCT_END(VolumeAbsorption, absorption)

OSL_CLOSURE_STRUCT_BEGIN(VolumeHenyeyGreenstein, henyey_greenstein)
  OSL_CLOSURE_STRUCT_MEMBER(VolumeHenyeyGreenstein, FLOAT, float, g, NULL)
OSL_CLOSURE_STRUCT_END(VolumeHenyeyGreenstein, henyey_greenstein)

#undef OSL_CLOSURE_STRUCT_BEGIN
#undef OSL_CLOSURE_STRUCT_END
#undef OSL_CLOSURE_STRUCT_MEMBER
#undef OSL_CLOSURE_STRUCT_ARRAY_MEMBER
