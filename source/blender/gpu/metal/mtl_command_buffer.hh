/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "mtl_capabilities.hh"
#include "mtl_common.hh"
#include "mtl_debug.hh"
#include "mtl_memory.hh"
#include "mtl_texture.hh"

/* Metal profiling tools complain about redundant bindings. Using our own tracking mechanism we can
 * avoid these redundant binds. Set to 0 to turn off this feature. */
#define MTL_ENABLE_REDUNDANT_BINDING_OPTIMIZATION 1
/* Avoid using the offset only update and force rebind even if buffer is the same. */
#define MTL_FORCE_BUFFER_REBIND 0

namespace blender::gpu {

/* Combined sampler state configuration for Argument Buffer caching. */
struct MTLSamplerArray {
  uint num_samplers;
  /* MTLSamplerState permutations between 0..256 - slightly more than a byte. */
  MTLSamplerState mtl_sampler_flags[MTL_MAX_TEXTURE_SLOTS];
  id<MTLSamplerState> mtl_sampler[MTL_MAX_TEXTURE_SLOTS];

  bool operator==(const MTLSamplerArray &other) const
  {
    if (this->num_samplers != other.num_samplers) {
      return false;
    }
    return (memcmp(this->mtl_sampler_flags,
                   other.mtl_sampler_flags,
                   sizeof(MTLSamplerState) * this->num_samplers) == 0);
  }

  uint32_t hash() const
  {
    uint32_t hash = this->num_samplers;
    for (int i = 0; i < this->num_samplers; i++) {
      hash ^= uint32_t(this->mtl_sampler_flags[i]) << (i % 3);
    }
    return hash;
  }
};

/* Structs containing information on current binding state for textures and samplers. */
struct MTLTextureBinding {
  gpu::MTLTexture *texture_resource = nullptr;
};

struct MTLSamplerBinding {
  gpu::MTLSamplerState state = DEFAULT_SAMPLER_STATE;

  bool operator==(MTLSamplerBinding const &other) const
  {
    return state == other.state;
  }

  id<MTLSamplerState> get_mtl_sampler(MTLContext &ctx);
};

/* Caching of CommandEncoder Vertex/Fragment buffer bindings. */
struct MTLBufferBindingCached {
  id<MTLBuffer> metal_buffer = nil;
  uint64_t offset = -1;
};

/* Caching of CommandEncoder textures bindings. */
struct MTLTextureBindingCached {
  id<MTLTexture> metal_texture = nil;
};

/* Cached of CommandEncoder sampler states. */
struct MTLSamplerStateBindingCached {
  gpu::MTLSamplerState binding_state = DEFAULT_SAMPLER_STATE;
  id<MTLSamplerState> sampler_state = nil;
  bool is_arg_buffer_binding = false;
};

/* Thin wrappers over to allow overloading of objective-C methods that have different names per
 * encoder. */

struct MTLComputeCommandEncoder {
  id<MTLComputeCommandEncoder> enc;

  MTLComputeCommandEncoder(id<MTLComputeCommandEncoder> encoder) : enc(encoder) {}

  void set_buffer_offset(size_t offset, int index);
  void set_buffer(id<MTLBuffer> buf, size_t offset, int index);
  void set_bytes(const void *bytes, size_t length, int index);
  void set_texture(id<MTLTexture> tex, int index);
  void set_sampler(id<MTLSamplerState> sampler_state, int index);
};

struct MTLVertexCommandEncoder {
  id<MTLRenderCommandEncoder> enc;

  MTLVertexCommandEncoder(id<MTLRenderCommandEncoder> encoder) : enc(encoder) {}

  void set_buffer_offset(size_t offset, int index);
  void set_buffer(id<MTLBuffer> buf, size_t offset, int index);
  void set_bytes(const void *bytes, size_t length, int index);
  void set_texture(id<MTLTexture> tex, int index);
  void set_sampler(id<MTLSamplerState> sampler_state, int index);
};

struct MTLFragmentCommandEncoder {
  id<MTLRenderCommandEncoder> enc;

  MTLFragmentCommandEncoder(id<MTLRenderCommandEncoder> encoder) : enc(encoder) {}

  void set_buffer_offset(size_t offset, int index);
  void set_buffer(id<MTLBuffer> buf, size_t offset, int index);
  void set_bytes(const void *bytes, size_t length, int index);
  void set_texture(id<MTLTexture> tex, int index);
  void set_sampler(id<MTLSamplerState> sampler_state, int index);
};

/* Class to remove redundant resource bindings. */
template<typename CommandEncoderT> struct MTLBindingCache {
  /* Indexed by final backend bindings, not by shader interface bindings. */
  std::array<MTLBufferBindingCached, MTL_MAX_BUFFER_BINDINGS> buffer_bindings = {};
  std::array<MTLTextureBindingCached, MTL_MAX_TEXTURE_SLOTS> texture_bindings = {};
  std::array<MTLSamplerStateBindingCached, MTL_MAX_TEXTURE_SLOTS> sampler_state_bindings = {};

  void bind_buffer(CommandEncoderT enc, id<MTLBuffer> buf, size_t offset, uint index);
  void bind_bytes(CommandEncoderT enc,
                  MTLScratchBufferManager &scratch_buffer,
                  const void *bytes,
                  size_t length,
                  uint index);
  void bind_texture(CommandEncoderT enc, id<MTLTexture> tex, uint index);
  void bind_sampler(CommandEncoderT enc,
                    gpu::MTLSamplerArray &sampler_array,
                    id<MTLSamplerState> sampler_state,
                    gpu::MTLSamplerState binding_state,
                    bool use_samplers_argument_buffer,
                    uint index);
};

template<typename CommandEncoderT>
void MTLBindingCache<CommandEncoderT>::bind_buffer(CommandEncoderT enc,
                                                   id<MTLBuffer> buf,
                                                   size_t offset,
                                                   uint index)
{
  BLI_assert(buf != nil);
  BLI_assert(index >= 0);
  BLI_assert(index < MTL_MAX_BUFFER_BINDINGS);
  MTLBufferBindingCached &binding = this->buffer_bindings[index];

#if MTL_ENABLE_REDUNDANT_BINDING_OPTIMIZATION
  if (binding.metal_buffer == buf && binding.offset == offset) {
    return;
  }
#endif

  if (binding.metal_buffer == buf && MTL_FORCE_BUFFER_REBIND == 0) {
    enc.set_buffer_offset(offset, index);
  }
  else {
    enc.set_buffer(buf, offset, index);
  }

  binding.metal_buffer = buf;
  binding.offset = offset;
}

template<typename CommandEncoderT>
void MTLBindingCache<CommandEncoderT>::bind_bytes(CommandEncoderT enc,
                                                  MTLScratchBufferManager &scratch_buffer,
                                                  const void *bytes,
                                                  size_t length,
                                                  uint index)
{
  /* Bytes are always updated as source data may have changed. */
  BLI_assert(index >= 0 && index < MTL_MAX_BUFFER_BINDINGS);
  BLI_assert(length > 0);
  BLI_assert(bytes != nullptr);
  MTLBufferBindingCached &binding = this->buffer_bindings[index];

  if (length >= MTL_MAX_SET_BYTES_SIZE) {
    /* We have run over the setBytes limit, bind buffer instead. */
    MTLTemporaryBuffer range = scratch_buffer.scratch_buffer_allocate_range_aligned(length, 256);
    memcpy(range.data, bytes, length);
    this->bind_buffer(enc, range.metal_buffer, range.buffer_offset, index);
    return;
  }

  enc.set_bytes(bytes, length, index);

  binding.metal_buffer = nil;
  binding.offset = -1;
}

template<typename CommandEncoderT>
void MTLBindingCache<CommandEncoderT>::bind_texture(CommandEncoderT enc,
                                                    id<MTLTexture> tex,
                                                    uint index)
{
  BLI_assert(tex != nil);
  BLI_assert(index >= 0);
  BLI_assert(index < MTL_MAX_TEXTURE_SLOTS);
  MTLTextureBindingCached &binding = this->texture_bindings[index];

#if MTL_ENABLE_REDUNDANT_BINDING_OPTIMIZATION
  if (binding.metal_texture == tex) {
    return;
  }
#endif

  enc.set_texture(tex, index);

  binding.metal_texture = tex;
}

template<typename CommandEncoderT>
void MTLBindingCache<CommandEncoderT>::bind_sampler(CommandEncoderT enc,
                                                    gpu::MTLSamplerArray &sampler_array,
                                                    id<MTLSamplerState> sampler_state,
                                                    gpu::MTLSamplerState binding_state,
                                                    bool use_samplers_argument_buffer,
                                                    uint index)
{
  BLI_assert(index >= 0);
  BLI_assert(index < MTL_MAX_TEXTURE_SLOTS);
  MTLSamplerStateBindingCached &binding = this->sampler_state_bindings[index];

#if MTL_ENABLE_REDUNDANT_BINDING_OPTIMIZATION
  /* If sampler state has not changed for the given slot, we do not need to fetch. */
  if (binding.sampler_state != nil && binding.binding_state == binding_state &&
      !use_samplers_argument_buffer)
  {
    return;
  }
#endif

  /* Flag last binding type. */
  binding.is_arg_buffer_binding = use_samplers_argument_buffer;

  /* Always assign to argument buffer samplers binding array - Efficiently ensures the value in
   * the samplers array is always up to date. */
  sampler_array.mtl_sampler[index] = sampler_state;
  sampler_array.mtl_sampler_flags[index] = binding_state;

  if (use_samplers_argument_buffer) {
    /* TODO(fclem): Always updating the argument buffer is something that was done before the huge
     * Metal backend refactor. Maybe we can move all the update inside this if clause. */
    return;
  }

  /* Update binding and cached state. */
  enc.set_sampler(sampler_state, index);

  binding.binding_state = binding_state;
  binding.sampler_state = sampler_state;
}

}  // namespace blender::gpu
