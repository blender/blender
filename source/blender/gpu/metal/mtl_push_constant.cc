/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.hh"

#include "mtl_push_constant.hh"

namespace blender::gpu {

static size_t padded_size(const shader::ShaderCreateInfo::PushConst &push_constant,
                          size_t &alignment)
{
  int comp = to_component_count(push_constant.type);
  size_t size;
  if (comp == 3) {
    /* Padded size for float3. */
    size = 4 * sizeof(float);
    alignment = 16;
  }
  else if (comp == 9) {
    /* Padded size for float3x3. */
    size = 3 * 4 * sizeof(float);
    alignment = 16;
  }
  else if (comp == 16) {
    /* Special alignment case for float4x4. */
    size = 4 * 4 * sizeof(float);
    alignment = 16;
  }
  else {
    size = comp * sizeof(float);
    alignment = size;
  }
  return size * push_constant.array_size_safe();
}

MTLPushConstantBuf::MTLPushConstantBuf(const shader::ShaderCreateInfo &info)
{
  BLI_assert(info.push_constants_.is_empty() == false);
  size_t max_alignement = 0;
  /* Compute size of backing buffer. */
  size_ = 0;
  for (const shader::ShaderCreateInfo::PushConst &push_constant : info.push_constants_) {
    size_t alignment;
    size_t pc_size = padded_size(push_constant, alignment);
    max_alignement = max_uu(max_alignement, alignment);
    /* Padding for alignment. */
    size_ = ceil_to_multiple_u(size_, alignment);
    size_ += pc_size;
  }
  /* Pad to max alignment. */
  size_ = ceil_to_multiple_u(size_, max_alignement);
  data_ = reinterpret_cast<uint8_t *>(
      MEM_calloc_arrayN_aligned(1, size_, 128, "MTLPushConstantData"));

  if (G.debug & G_DEBUG_GPU) {
    /* Poison values to detect unset values. */
    memset(data_, 0xFD, size_);
  }
}

MTLPushConstantBuf::~MTLPushConstantBuf()
{
  MEM_freeN(data_);
}

int MTLPushConstantBuf::append(shader::ShaderCreateInfo::PushConst push_constant)
{
  size_t alignment;
  size_t pc_size = padded_size(push_constant, alignment);
  /* Padding for alignment. */
  offset_ = ceil_to_multiple_u(offset_, alignment);
  int loc = offset_;
  offset_ += pc_size;
  BLI_assert(offset_ <= size_);
  return loc;
}

}  // namespace blender::gpu
