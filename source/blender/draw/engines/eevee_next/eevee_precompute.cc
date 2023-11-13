/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * LUT generation module.
 */

#include "eevee_precompute.hh"

namespace blender::eevee {

Precompute::Precompute(draw::Manager &manager, PrecomputeType type, int3 table_extent)
{
  table_extent_ = table_extent;

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_WRITE | GPU_TEXTURE_USAGE_HOST_READ;
  Texture table_tx = {"Precompute"};
  table_tx.ensure_3d(GPU_RGBA32F, table_extent, usage);

  GPUShader *shader = GPU_shader_create_from_info_name("eevee_lut");

  PassSimple lut_ps = {"Precompute"};
  lut_ps.shader_set(shader);
  lut_ps.push_constant("table_type", int(type));
  lut_ps.push_constant("table_extent", table_extent);
  lut_ps.bind_image("table_img", table_tx);
  lut_ps.dispatch(math::divide_ceil(table_extent, int3(int2(LUT_WORKGROUP_SIZE), 1)));
  lut_ps.barrier(GPU_BARRIER_TEXTURE_UPDATE);

  manager.submit(lut_ps);

  raw_data_ = table_tx.read<float4>(GPU_DATA_FLOAT);

  GPU_shader_free(shader);
}

Precompute::~Precompute()
{
  MEM_SAFE_FREE(raw_data_);
}

}  // namespace blender::eevee
