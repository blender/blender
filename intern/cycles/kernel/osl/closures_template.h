/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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

OSL_CLOSURE_STRUCT_BEGIN(RayPortalBSDF, ray_portal_bsdf)
  OSL_CLOSURE_STRUCT_MEMBER(RayPortalBSDF, VECTOR, packed_float3, position, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(RayPortalBSDF, VECTOR, packed_float3, direction, NULL)
OSL_CLOSURE_STRUCT_END(RayPortalBSDF, ray_portal_bsdf)

OSL_CLOSURE_STRUCT_BEGIN(DielectricBSDF, dielectric_bsdf)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, VECTOR, packed_float3, T, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, VECTOR, packed_float3, reflection_tint, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, VECTOR, packed_float3, transmission_tint, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, FLOAT, float, alpha_y, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, FLOAT, float, ior, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, STRING, DeviceString, distribution, NULL)
OSL_CLOSURE_STRUCT_END(DielectricBSDF, dielectric_bsdf)

OSL_CLOSURE_STRUCT_BEGIN(ConductorBSDF, conductor_bsdf)
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, VECTOR, packed_float3, T, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, FLOAT, float, alpha_y, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, VECTOR, packed_float3, ior, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, VECTOR, packed_float3, extinction, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, STRING, DeviceString, distribution, NULL)
OSL_CLOSURE_STRUCT_END(ConductorBSDF, conductor_bsdf)

OSL_CLOSURE_STRUCT_BEGIN(GeneralizedSchlickBSDF, generalized_schlick_bsdf)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, VECTOR, packed_float3, T, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, VECTOR, packed_float3, reflection_tint, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, VECTOR, packed_float3, transmission_tint, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, FLOAT, float, alpha_y, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, VECTOR, packed_float3, f0, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, VECTOR, packed_float3, f90, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, FLOAT, float, exponent, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, STRING, DeviceString, distribution, NULL)
OSL_CLOSURE_STRUCT_END(GeneralizedSchlickBSDF, generalized_schlick_bsdf)

OSL_CLOSURE_STRUCT_BEGIN(Microfacet, microfacet)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, STRING, DeviceString, distribution, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, VECTOR, packed_float3, T, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, FLOAT, float, alpha_y, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, FLOAT, float, ior, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, INT, int, refract, NULL)
OSL_CLOSURE_STRUCT_END(Microfacet, microfacet)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetF82Tint, microfacet_f82_tint)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetF82Tint, STRING, DeviceString, distribution, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetF82Tint, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetF82Tint, VECTOR, packed_float3, T, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetF82Tint, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetF82Tint, FLOAT, float, alpha_y, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetF82Tint, VECTOR, packed_float3, f0, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetF82Tint, VECTOR, packed_float3, f82, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetF82Tint, microfacet)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetMultiGGXGlass, microfacet_multi_ggx_glass)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlass, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlass, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlass, FLOAT, float, ior, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlass, VECTOR, packed_float3, color, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetMultiGGXGlass, microfacet_multi_ggx_glass)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetMultiGGX, microfacet_multi_ggx_aniso)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGX, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGX, VECTOR, packed_float3, T, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGX, FLOAT, float, alpha_x, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGX, FLOAT, float, alpha_y, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGX, VECTOR, packed_float3, color, NULL)
OSL_CLOSURE_STRUCT_END(MicrofacetMultiGGX, microfacet_multi_ggx_aniso)

OSL_CLOSURE_STRUCT_BEGIN(AshikhminVelvet, ashikhmin_velvet)
  OSL_CLOSURE_STRUCT_MEMBER(AshikhminVelvet, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(AshikhminVelvet, FLOAT, float, sigma, NULL)
OSL_CLOSURE_STRUCT_END(AshikhminVelvet, ashikhmin_velvet)

OSL_CLOSURE_STRUCT_BEGIN(Sheen, sheen)
  OSL_CLOSURE_STRUCT_MEMBER(Sheen, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(Sheen, FLOAT, float, roughness, NULL)
OSL_CLOSURE_STRUCT_END(Sheen, sheen)

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

OSL_CLOSURE_STRUCT_BEGIN(ChiangHair, hair_chiang)
  OSL_CLOSURE_STRUCT_MEMBER(ChiangHair, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(ChiangHair, VECTOR, packed_float3, sigma, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(ChiangHair, FLOAT, float, v, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(ChiangHair, FLOAT, float, s, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(ChiangHair, FLOAT, float, m0_roughness, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(ChiangHair, FLOAT, float, alpha, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(ChiangHair, FLOAT, float, eta, NULL)
OSL_CLOSURE_STRUCT_END(ChiangHair, hair_chiang)

OSL_CLOSURE_STRUCT_BEGIN(HuangHair, hair_huang)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, VECTOR, packed_float3, sigma, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, FLOAT, float, roughness, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, FLOAT, float, tilt, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, FLOAT, float, eta, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, FLOAT, float, aspect_ratio, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, FLOAT, float, r_lobe, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, FLOAT, float, tt_lobe, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, FLOAT, float, trt_lobe, NULL)
OSL_CLOSURE_STRUCT_END(HuangHair, hair_huang)

OSL_CLOSURE_STRUCT_BEGIN(VolumeAbsorption, absorption)
OSL_CLOSURE_STRUCT_END(VolumeAbsorption, absorption)

OSL_CLOSURE_STRUCT_BEGIN(VolumeHenyeyGreenstein, henyey_greenstein)
  OSL_CLOSURE_STRUCT_MEMBER(VolumeHenyeyGreenstein, FLOAT, float, g, NULL)
OSL_CLOSURE_STRUCT_END(VolumeHenyeyGreenstein, henyey_greenstein)

#undef OSL_CLOSURE_STRUCT_BEGIN
#undef OSL_CLOSURE_STRUCT_END
#undef OSL_CLOSURE_STRUCT_MEMBER
#undef OSL_CLOSURE_STRUCT_ARRAY_MEMBER
