/* SPDX-FileCopyrightText: 2017-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_bxdf_types.bsl.hh"
#include "eevee_utility_tx.bsl.hh"
#include "gpu_shader_compat.hh"
#include "gpu_shader_math_matrix_construct.bsl.hh"

namespace eevee {

namespace detail {

/**
 * Given BxDF parameters, return UV coordinates for sampling of the isotropic LTC LUT.
 */
float2 get_isotropic_coords(float cos_theta, float roughness)
{
  return float2(roughness, sqrt(saturate(1.0f - cos_theta)));
}

/**
 * Given 4 isotropic LTC LUT values, unpack a 3x3 matrix normalized by its central component.
 */
float3x3 unpack_isotropic_matrix(float4 v)
{
  return float3x3(float3(v.x, 0, v.y), float3(0, 1, 0), float3(v.z, 0, v.w));
}

}  // namespace detail

/**
 * Unpacked output and secondary data from the isotropic GGX LTC LUT.
 * This data is evaluated in `eevee_ltc_lib.bsl.hh`, and stored in a
 * packed format in ClosureLight in between area light evaluations.
 */
struct LTCData {
  /* Inverse LTC matrix. */
  float3x3 Minv;
  /* LTC lobe attenuation is scaled by 1 minus value. */
  float attenuation_factor;
  /* Type of form factor computation applied during LTC evaluation . */
  LTCFormFactorType form_factor_type;

  /**
   * Unpack LTC matrix inverse and associated data from uint[5] in ClosureLight.
   */
  static LTCData unpack_from(ClosureLight cl)
  {
    LTCData ltc_data;

    /* Unpack matrix values from 9 x fp16. */
    ltc_data.Minv[0].xy = unpackHalf2x16(cl.ltc_data_packed[0]);
    ltc_data.Minv[1].xy = unpackHalf2x16(cl.ltc_data_packed[1]);
    ltc_data.Minv[2].xy = unpackHalf2x16(cl.ltc_data_packed[2]);
    float2 v3 = unpackHalf2x16(cl.ltc_data_packed[3]);
    float2 v4 = unpackHalf2x16(cl.ltc_data_packed[4]);
    ltc_data.Minv[0].z = v3.x;
    ltc_data.Minv[1].z = v3.y;
    ltc_data.Minv[2].z = v4.y;

    /* Unpack associated values from 16 lsb of [4]. */
    float4 pack = unpackUnorm4x8(cl.ltc_data_packed[4]);
    ltc_data.attenuation_factor = pack.x;
    ltc_data.form_factor_type = LTCFormFactorType(pack.y);

    return ltc_data;
  }

  /**
   * Pack LTC matrix inverse and associated data to uint[5] in ClosureLight.
   */
  void pack_to(ClosureLight &cl) const
  {
    /* Pack associated values in 16 lsb . */
    uint values_lsb = packUnorm4x8(
        float4(attenuation_factor, float(form_factor_type), 0.0f, 0.0f));

    /* Pack matrix values as 9 x fp16, plus associated values in 16 lsb of [4]. */
    cl.ltc_data_packed[0] = packHalf2x16(Minv[0].xy);
    cl.ltc_data_packed[1] = packHalf2x16(Minv[1].xy);
    cl.ltc_data_packed[2] = packHalf2x16(Minv[2].xy);
    cl.ltc_data_packed[3] = packHalf2x16(float2(Minv[0].z, Minv[1].z));
    cl.ltc_data_packed[4] = packHalf2x16(float2(0.0f, Minv[2].z)) | values_lsb;
  }

  /**
   * Sample matrix data from an isotropic LTC LUT.
   */
  static LTCData sample_lut(const UtilityTexture &util_tx,
                            const float3 N,
                            const float3 V,
                            const float cos_theta,
                            const float roughness,
                            const int layer)
  {
    /* Sample 4 components from isotropic LTC LUT. */
    const float2 coords = detail::get_isotropic_coords(cos_theta, roughness);
    const float4 lut_packed = util_tx.sample_lut(coords, layer);

    /* Expand components to full inverse LTC matrix. */
    float3x3 Minv = detail::unpack_isotropic_matrix(lut_packed);

    /* Rotate into orthonormal basis around N. */
    const float3x3 T = from_incident_vector(N, V);
    Minv = Minv * transpose(T);

    return {.Minv = Minv,
            .attenuation_factor = 1.0f,
            .form_factor_type = LTCFormFactorType::OneSidedCosineSphereClipped};
  }

  /**
   * Return matrix data producing a cosine distribution.
   */
  static LTCData identity(const float3 N, const float3 V)
  {
    /* Rotate into orthonormal basis around N. LTC matrix is simply the identity matrix. */
    const float3x3 T = from_incident_vector(N, V);

    return {.Minv = transpose(T),
            .attenuation_factor = 1.0f,
            .form_factor_type = LTCFormFactorType::OneSidedCosineSphereClipped};
  }

  /**
   * Sample matrix data from the isotropic GGX LTC LUT.
   */
  static LTCData sample_ggx_lut(const UtilityTexture &util_tx,
                                const float3 N,
                                const float3 V,
                                const float cos_theta,
                                const float roughness)
  {
    LTCData ltc_data = LTCData::sample_lut(
        util_tx, N, V, cos_theta, roughness, UTIL_LTC_MAT_GGX_LAYER);
    /* LTC attenuation linearly disappears from roughness 0.15 to 0.375. */
    ltc_data.attenuation_factor = saturate((roughness - 0.15f) * 2.5f);
    return ltc_data;
  }

  /**
   * Define matrix data producing a cosine distribution and store to ClosureLight packing.
   */
  static void pack_identity(ClosureLight &cl, const float3 N, const float3 V)
  {
    LTCData ltc_data = LTCData::identity(N, V);
    ltc_data.pack_to(cl);
  }
};

}  // namespace eevee
