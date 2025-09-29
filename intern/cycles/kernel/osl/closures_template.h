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
  OSL_CLOSURE_STRUCT_MEMBER(Diffuse, VECTOR, packed_float3, N, nullptr)
OSL_CLOSURE_STRUCT_END(Diffuse, diffuse)

/* Deprecated form, will be removed in OSL 2.0. */
OSL_CLOSURE_STRUCT_BEGIN(OrenNayar, oren_nayar)
  OSL_CLOSURE_STRUCT_MEMBER(OrenNayar, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(OrenNayar, FLOAT, float, roughness, nullptr)
OSL_CLOSURE_STRUCT_END(OrenNayar, oren_nayar)

OSL_CLOSURE_STRUCT_BEGIN(OrenNayarDiffuseBSDF, oren_nayar_diffuse_bsdf)
  OSL_CLOSURE_STRUCT_MEMBER(OrenNayarDiffuseBSDF, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(OrenNayarDiffuseBSDF, VECTOR, packed_float3, albedo, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(OrenNayarDiffuseBSDF, FLOAT, float, roughness, nullptr)
OSL_CLOSURE_STRUCT_END(OrenNayarDiffuseBSDF, oren_nayar_diffuse_bsdf)

OSL_CLOSURE_STRUCT_BEGIN(BurleyDiffuseBSDF, burley_diffuse_bsdf)
  OSL_CLOSURE_STRUCT_MEMBER(BurleyDiffuseBSDF, VECTOR, packed_float3, N, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(BurleyDiffuseBSDF, VECTOR, packed_float3, albedo, NULL)
  OSL_CLOSURE_STRUCT_MEMBER(BurleyDiffuseBSDF, FLOAT, float, roughness, NULL)
OSL_CLOSURE_STRUCT_END(BurleyDiffuseBSDF, burley_diffuse_bsdf)

OSL_CLOSURE_STRUCT_BEGIN(Translucent, translucent)
  OSL_CLOSURE_STRUCT_MEMBER(Translucent, VECTOR, packed_float3, N, nullptr)
OSL_CLOSURE_STRUCT_END(Translucent, translucent)

OSL_CLOSURE_STRUCT_BEGIN(Reflection, reflection)
  OSL_CLOSURE_STRUCT_MEMBER(Reflection, VECTOR, packed_float3, N, nullptr)
OSL_CLOSURE_STRUCT_END(Reflection, reflection)

OSL_CLOSURE_STRUCT_BEGIN(Refraction, refraction)
  OSL_CLOSURE_STRUCT_MEMBER(Refraction, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(Refraction, FLOAT, float, ior, nullptr)
OSL_CLOSURE_STRUCT_END(Refraction, refraction)

OSL_CLOSURE_STRUCT_BEGIN(Transparent, transparent)
OSL_CLOSURE_STRUCT_END(Transparent, transparent)

OSL_CLOSURE_STRUCT_BEGIN(RayPortalBSDF, ray_portal_bsdf)
  OSL_CLOSURE_STRUCT_MEMBER(RayPortalBSDF, VECTOR, packed_float3, position, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(RayPortalBSDF, VECTOR, packed_float3, direction, nullptr)
OSL_CLOSURE_STRUCT_END(RayPortalBSDF, ray_portal_bsdf)

OSL_CLOSURE_STRUCT_BEGIN(DielectricBSDF, dielectric_bsdf)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, VECTOR, packed_float3, T, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, VECTOR, packed_float3, reflection_tint, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, VECTOR, packed_float3, transmission_tint, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, FLOAT, float, alpha_x, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, FLOAT, float, alpha_y, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, FLOAT, float, ior, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, STRING, DeviceString, distribution, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, FLOAT, float, thinfilm_thickness, "thinfilm_thickness")
  OSL_CLOSURE_STRUCT_MEMBER(DielectricBSDF, FLOAT, float, thinfilm_ior, "thinfilm_ior")
OSL_CLOSURE_STRUCT_END(DielectricBSDF, dielectric_bsdf)

OSL_CLOSURE_STRUCT_BEGIN(ConductorBSDF, conductor_bsdf)
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, VECTOR, packed_float3, T, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, FLOAT, float, alpha_x, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, FLOAT, float, alpha_y, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, VECTOR, packed_float3, ior, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, VECTOR, packed_float3, extinction, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, STRING, DeviceString, distribution, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, FLOAT, float, thinfilm_thickness, "thinfilm_thickness")
  OSL_CLOSURE_STRUCT_MEMBER(ConductorBSDF, FLOAT, float, thinfilm_ior, "thinfilm_ior")
OSL_CLOSURE_STRUCT_END(ConductorBSDF, conductor_bsdf)

OSL_CLOSURE_STRUCT_BEGIN(GeneralizedSchlickBSDF, generalized_schlick_bsdf)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, VECTOR, packed_float3, T, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(
      GeneralizedSchlickBSDF, VECTOR, packed_float3, reflection_tint, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(
      GeneralizedSchlickBSDF, VECTOR, packed_float3, transmission_tint, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, FLOAT, float, alpha_x, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, FLOAT, float, alpha_y, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, VECTOR, packed_float3, f0, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, VECTOR, packed_float3, f90, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, FLOAT, float, exponent, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, STRING, DeviceString, distribution, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(
      GeneralizedSchlickBSDF, FLOAT, float, thinfilm_thickness, "thinfilm_thickness")
  OSL_CLOSURE_STRUCT_MEMBER(GeneralizedSchlickBSDF, FLOAT, float, thinfilm_ior, "thinfilm_ior")
OSL_CLOSURE_STRUCT_END(GeneralizedSchlickBSDF, generalized_schlick_bsdf)

OSL_CLOSURE_STRUCT_BEGIN(Microfacet, microfacet)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, STRING, DeviceString, distribution, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, VECTOR, packed_float3, T, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, FLOAT, float, alpha_x, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, FLOAT, float, alpha_y, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, FLOAT, float, ior, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(Microfacet, INT, int, refract, nullptr)
OSL_CLOSURE_STRUCT_END(Microfacet, microfacet)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetF82Tint, microfacet_f82_tint)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetF82Tint, STRING, DeviceString, distribution, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetF82Tint, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetF82Tint, VECTOR, packed_float3, T, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetF82Tint, FLOAT, float, alpha_x, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetF82Tint, FLOAT, float, alpha_y, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetF82Tint, VECTOR, packed_float3, f0, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetF82Tint, VECTOR, packed_float3, f82, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(
      MicrofacetF82Tint, FLOAT, float, thinfilm_thickness, "thinfilm_thickness")
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetF82Tint, FLOAT, float, thinfilm_ior, "thinfilm_ior")
OSL_CLOSURE_STRUCT_END(MicrofacetF82Tint, microfacet)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetMultiGGXGlass, microfacet_multi_ggx_glass)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlass, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlass, FLOAT, float, alpha_x, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlass, FLOAT, float, ior, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGXGlass, VECTOR, packed_float3, color, nullptr)
OSL_CLOSURE_STRUCT_END(MicrofacetMultiGGXGlass, microfacet_multi_ggx_glass)

OSL_CLOSURE_STRUCT_BEGIN(MicrofacetMultiGGX, microfacet_multi_ggx_aniso)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGX, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGX, VECTOR, packed_float3, T, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGX, FLOAT, float, alpha_x, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGX, FLOAT, float, alpha_y, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(MicrofacetMultiGGX, VECTOR, packed_float3, color, nullptr)
OSL_CLOSURE_STRUCT_END(MicrofacetMultiGGX, microfacet_multi_ggx_aniso)

OSL_CLOSURE_STRUCT_BEGIN(AshikhminVelvet, ashikhmin_velvet)
  OSL_CLOSURE_STRUCT_MEMBER(AshikhminVelvet, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(AshikhminVelvet, FLOAT, float, sigma, nullptr)
OSL_CLOSURE_STRUCT_END(AshikhminVelvet, ashikhmin_velvet)

OSL_CLOSURE_STRUCT_BEGIN(Sheen, sheen)
  OSL_CLOSURE_STRUCT_MEMBER(Sheen, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(Sheen, FLOAT, float, roughness, nullptr)
OSL_CLOSURE_STRUCT_END(Sheen, sheen)

OSL_CLOSURE_STRUCT_BEGIN(SheenBSDF, sheen_bsdf)
  OSL_CLOSURE_STRUCT_MEMBER(SheenBSDF, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(SheenBSDF, VECTOR, packed_float3, albedo, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(SheenBSDF, FLOAT, float, roughness, nullptr)
OSL_CLOSURE_STRUCT_END(SheenBSDF, sheen_bsdf)

OSL_CLOSURE_STRUCT_BEGIN(DiffuseToon, diffuse_toon)
  OSL_CLOSURE_STRUCT_MEMBER(DiffuseToon, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(DiffuseToon, FLOAT, float, size, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(DiffuseToon, FLOAT, float, smooth, nullptr)
OSL_CLOSURE_STRUCT_END(DiffuseToon, diffuse_toon)

OSL_CLOSURE_STRUCT_BEGIN(GlossyToon, glossy_toon)
  OSL_CLOSURE_STRUCT_MEMBER(GlossyToon, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(GlossyToon, FLOAT, float, size, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(GlossyToon, FLOAT, float, smooth, nullptr)
OSL_CLOSURE_STRUCT_END(GlossyToon, glossy_toon)

OSL_CLOSURE_STRUCT_BEGIN(GenericEmissive, emission)
OSL_CLOSURE_STRUCT_END(GenericEmissive, emission)

OSL_CLOSURE_STRUCT_BEGIN(GenericBackground, background)
OSL_CLOSURE_STRUCT_END(GenericBackground, background)

OSL_CLOSURE_STRUCT_BEGIN(UniformEDF, uniform_edf)
  OSL_CLOSURE_STRUCT_MEMBER(UniformEDF, COLOR, packed_float3, emittance, nullptr)
OSL_CLOSURE_STRUCT_END(UniformEDF, uniform_edf)

OSL_CLOSURE_STRUCT_BEGIN(Holdout, holdout)
OSL_CLOSURE_STRUCT_END(Holdout, holdout)

OSL_CLOSURE_STRUCT_BEGIN(DiffuseRamp, diffuse_ramp)
  OSL_CLOSURE_STRUCT_MEMBER(DiffuseRamp, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_ARRAY_MEMBER(DiffuseRamp, COLOR, packed_float3, colors, nullptr, 8)
OSL_CLOSURE_STRUCT_END(DiffuseRamp, diffuse_ramp)

OSL_CLOSURE_STRUCT_BEGIN(PhongRamp, phong_ramp)
  OSL_CLOSURE_STRUCT_MEMBER(PhongRamp, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(PhongRamp, FLOAT, float, exponent, nullptr)
  OSL_CLOSURE_STRUCT_ARRAY_MEMBER(PhongRamp, COLOR, packed_float3, colors, nullptr, 8)
OSL_CLOSURE_STRUCT_END(PhongRamp, phong_ramp)

OSL_CLOSURE_STRUCT_BEGIN(BSSRDF, bssrdf)
  OSL_CLOSURE_STRUCT_MEMBER(BSSRDF, STRING, DeviceString, method, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(BSSRDF, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(BSSRDF, VECTOR, packed_float3, radius, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(BSSRDF, VECTOR, packed_float3, albedo, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(BSSRDF, FLOAT, float, roughness, "roughness")
  OSL_CLOSURE_STRUCT_MEMBER(BSSRDF, FLOAT, float, ior, "ior")
  OSL_CLOSURE_STRUCT_MEMBER(BSSRDF, FLOAT, float, anisotropy, "anisotropy")
OSL_CLOSURE_STRUCT_END(BSSRDF, bssrdf)

OSL_CLOSURE_STRUCT_BEGIN(SubsurfaceBSSRDF, subsurface_bssrdf)
  OSL_CLOSURE_STRUCT_MEMBER(SubsurfaceBSSRDF, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(SubsurfaceBSSRDF, VECTOR, packed_float3, albedo, nullptr)
#if OSL_LIBRARY_VERSION_CODE >= 11401
  OSL_CLOSURE_STRUCT_MEMBER(SubsurfaceBSSRDF, VECTOR, packed_float3, radius, nullptr)
#else
  OSL_CLOSURE_STRUCT_MEMBER(SubsurfaceBSSRDF, FLOAT, float, transmission_depth, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(SubsurfaceBSSRDF, VECTOR, packed_float3, transmission_color, nullptr)
#endif
  OSL_CLOSURE_STRUCT_MEMBER(SubsurfaceBSSRDF, FLOAT, float, anisotropy, nullptr)
OSL_CLOSURE_STRUCT_END(SubsurfaceBSSRDF, subsurface_bssrdf)

OSL_CLOSURE_STRUCT_BEGIN(HairReflection, hair_reflection)
  OSL_CLOSURE_STRUCT_MEMBER(HairReflection, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HairReflection, FLOAT, float, roughness1, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HairReflection, FLOAT, float, roughness2, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HairReflection, VECTOR, packed_float3, T, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HairReflection, FLOAT, float, offset, nullptr)
OSL_CLOSURE_STRUCT_END(HairReflection, hair_reflection)

OSL_CLOSURE_STRUCT_BEGIN(HairTransmission, hair_transmission)
  OSL_CLOSURE_STRUCT_MEMBER(HairTransmission, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HairTransmission, FLOAT, float, roughness1, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HairTransmission, FLOAT, float, roughness2, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HairReflection, VECTOR, packed_float3, T, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HairReflection, FLOAT, float, offset, nullptr)
OSL_CLOSURE_STRUCT_END(HairTransmission, hair_transmission)

OSL_CLOSURE_STRUCT_BEGIN(ChiangHair, hair_chiang)
  OSL_CLOSURE_STRUCT_MEMBER(ChiangHair, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(ChiangHair, VECTOR, packed_float3, sigma, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(ChiangHair, FLOAT, float, v, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(ChiangHair, FLOAT, float, s, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(ChiangHair, FLOAT, float, m0_roughness, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(ChiangHair, FLOAT, float, alpha, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(ChiangHair, FLOAT, float, eta, nullptr)
OSL_CLOSURE_STRUCT_END(ChiangHair, hair_chiang)

OSL_CLOSURE_STRUCT_BEGIN(HuangHair, hair_huang)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, VECTOR, packed_float3, N, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, VECTOR, packed_float3, sigma, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, FLOAT, float, roughness, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, FLOAT, float, tilt, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, FLOAT, float, eta, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, FLOAT, float, aspect_ratio, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, FLOAT, float, r_lobe, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, FLOAT, float, tt_lobe, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(HuangHair, FLOAT, float, trt_lobe, nullptr)
OSL_CLOSURE_STRUCT_END(HuangHair, hair_huang)

OSL_CLOSURE_STRUCT_BEGIN(VolumeAbsorption, absorption)
OSL_CLOSURE_STRUCT_END(VolumeAbsorption, absorption)

OSL_CLOSURE_STRUCT_BEGIN(VolumeHenyeyGreenstein, henyey_greenstein)
  OSL_CLOSURE_STRUCT_MEMBER(VolumeHenyeyGreenstein, FLOAT, float, g, nullptr)
OSL_CLOSURE_STRUCT_END(VolumeHenyeyGreenstein, henyey_greenstein)

OSL_CLOSURE_STRUCT_BEGIN(VolumeFournierForand, fournier_forand)
  OSL_CLOSURE_STRUCT_MEMBER(VolumeFournierForand, FLOAT, float, B, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(VolumeFournierForand, FLOAT, float, IOR, nullptr)
OSL_CLOSURE_STRUCT_END(VolumeFournierForand, fournier_forand)

OSL_CLOSURE_STRUCT_BEGIN(VolumeDraine, draine)
  OSL_CLOSURE_STRUCT_MEMBER(VolumeDraine, FLOAT, float, g, nullptr)
  OSL_CLOSURE_STRUCT_MEMBER(VolumeDraine, FLOAT, float, alpha, nullptr)
OSL_CLOSURE_STRUCT_END(VolumeDraine, draine)

OSL_CLOSURE_STRUCT_BEGIN(VolumeRayleigh, rayleigh)
OSL_CLOSURE_STRUCT_END(VolumeRayleigh, rayleigh)

#undef OSL_CLOSURE_STRUCT_BEGIN
#undef OSL_CLOSURE_STRUCT_END
#undef OSL_CLOSURE_STRUCT_MEMBER
#undef OSL_CLOSURE_STRUCT_ARRAY_MEMBER
