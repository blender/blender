/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Ray data packing utils.
 *
 * Direction use octahedral encoding with 24 bits per axis. Half float is not precise enough for
 * smooth surfaces at grazing angle (see #125721).
 * Inverse pdf keeps the upper 16 bits to keep the full range.
 */
#include "eevee_gbuffer_types.bsl.hh"

namespace eevee {

uint4 ray_data_pack(float3 L, float pdf_inv)
{
  uint2 oct = uint2(round(gbuffer::normal_pack(L) * float(0xFFFFFFu)));
  uint pdf_bits = (floatBitsToUint(pdf_inv) + 0x8000u) >> 16u;
  return uint4(oct & 0xFFFFu, (oct.x >> 16u) | ((oct.y >> 16u) << 8u), pdf_bits);
}

float3 ray_data_direction(uint4 data)
{
  uint2 oct = data.xy | ((uint2(data.z, data.z >> 8u) & 0xFFu) << 16u);
  return gbuffer::normal_unpack(float2(oct) / float(0xFFFFFFu));
}

float ray_data_pdf_inv(uint4 data)
{
  return uintBitsToFloat(data.w << 16u);
}

}  // namespace eevee
