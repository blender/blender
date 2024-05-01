/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "BLI_vector.hh"

#include "gpu_shader_interface.hh"
#include "mtl_capabilities.hh"
#include "mtl_shader_interface_type.hh"

#include "GPU_common.hh"
#include "GPU_common_types.hh"
#include "GPU_texture.hh"
#include "gpu_texture_private.hh"
#include <Metal/Metal.h>
#include <functional>

namespace blender::gpu {

/* #MTLShaderInterface describes the layout and properties of a given shader,
 * including input and output bindings, and any special properties or modes
 * that the shader may require.
 *
 * -- Shader input/output bindings --
 *
 * We require custom data-structures for the binding information in Metal.
 * This is because certain bindings contain and require more information to
 * be stored than can be tracked solely within the `ShaderInput` struct.
 * e.g. data sizes and offsets.
 *
 * Upon interface completion, `prepare_common_shader_inputs` is used to
 * populate the global `ShaderInput*` array to enable correct functionality
 * of shader binding location lookups. These returned locations act as indices
 * into the arrays stored here in the #MTLShaderInterface, such that extraction
 * of required information can be performed within the back-end.
 *
 * e.g. `int loc = GPU_shader_get_uniform(...)`
 * `loc` will match the index into the `MTLShaderUniform uniforms_[]` array
 * to fetch the required Metal specific information.
 *
 *
 *
 * -- Argument Buffers and Argument Encoders --
 *
 * We can use #ArgumentBuffers (AB's) in Metal to extend the resource bind limitations
 * by providing bind-less support.
 *
 * Argument Buffers are used for sampler bindings when the builtin
 * sampler limit of 16 is exceeded, as in all cases for Blender,
 * each individual texture is associated with a given sampler, and this
 * lower limit would otherwise reduce the total availability of textures
 * used in shaders.
 *
 * In future, argument buffers may be extended to support other resource
 * types, if overall bind limits are ever increased within Blender.
 *
 * The #ArgumentEncoder cache used to store the generated #ArgumentEncoders for a given
 * shader permutation. The #ArgumentEncoder is the resource used to write resource binding
 * information to a specified buffer, and is unique to the shader's resource interface.
 */

enum class ShaderStage : uint8_t {
  VERTEX = 1 << 0,
  FRAGMENT = 1 << 1,
  COMPUTE = 2 << 1,
  ANY = (ShaderStage::VERTEX | ShaderStage::FRAGMENT | ShaderStage::COMPUTE),
};
ENUM_OPERATORS(ShaderStage, ShaderStage::ANY);

inline uint get_shader_stage_index(ShaderStage stage)
{
  switch (stage) {
    case ShaderStage::VERTEX:
      return 0;
    case ShaderStage::FRAGMENT:
      return 1;
    case ShaderStage::COMPUTE:
      return 2;
    default:
      BLI_assert_unreachable();
      return 0;
  }
  return 0;
}

/* Shader input/output binding information. */
struct MTLShaderInputAttribute {
  uint32_t name_offset;
  MTLVertexFormat format;
  uint32_t index;
  uint32_t location;
  uint32_t size;
  uint32_t buffer_index;
  uint32_t offset;
  /* For attributes of Matrix/array types, we need to insert "fake" attributes for
   * each element, as matrix types are not natively supported.
   *
   *   > 1 if matrix/arrays are used, specifying number of elements.
   *   = 1 for non-matrix types
   *   = 0 if used as a dummy slot for "fake" matrix attributes. */
  uint32_t matrix_element_count;
};

struct MTLShaderBufferBlock {
  uint32_t name_offset;
  uint32_t size = 0;
  /* Buffer resource bind index in shader `[[buffer(index)]]`. */
  uint32_t buffer_index;
  /* Explicit bind location for texture. */
  int location;
  /* Tracking for manual uniform addition. */
  uint32_t current_offset;
  ShaderStage stage_mask;
};

struct MTLShaderUniform {
  uint32_t name_offset;
  /* Index of `MTLShaderBufferBlock` this uniform belongs to. */
  uint32_t size_in_bytes;
  uint32_t byte_offset;
  eMTLDataType type;
  uint32_t array_len;
};

struct MTLShaderConstant {
  uint32_t name_offset;
};

struct MTLShaderTexture {
  bool used;
  uint32_t name_offset;
  /* Texture resource bind slot in shader `[[texture(n)]]`. */
  int slot_index;
  /* Explicit bind location for texture. */
  int location;
  eGPUTextureType type;
  eGPUSamplerFormat sampler_format;
  ShaderStage stage_mask;
  /* Whether texture resource is expected to be image or sampler. */
  bool is_texture_sampler;
  /* SSBO index for texture buffer binding. */
  int texture_buffer_ssbo_location = -1;
  /* Uniform location for texture buffer metadata. */
  int buffer_metadata_uniform_loc = -1;
};

struct MTLShaderSampler {
  uint32_t name_offset;
  /* Sampler resource bind slot in shader `[[sampler(n)]]`. */
  uint32_t slot_index = 0;
};

/* Utility Functions. */
MTLVertexFormat mtl_datatype_to_vertex_type(eMTLDataType type);

/**
 * Implementation of Shader interface for Metal Back-end.
 */
class MTLShaderInterface : public ShaderInterface {

 private:
  /* Argument encoders caching.
   * Static size is based on common input permutation variations. */
  static const int ARGUMENT_ENCODERS_CACHE_SIZE = 3;
  struct ArgumentEncoderCacheEntry {
    id<MTLArgumentEncoder> encoder;
    int buffer_index;
  };
  ArgumentEncoderCacheEntry arg_encoders_[ARGUMENT_ENCODERS_CACHE_SIZE] = {};

  /* Vertex input Attributes. */
  uint32_t total_attributes_;
  uint32_t total_vert_stride_;
  MTLShaderInputAttribute attributes_[MTL_MAX_VERTEX_INPUT_ATTRIBUTES];

  /* Uniforms. */
  uint32_t total_uniforms_;
  MTLShaderUniform uniforms_[MTL_MAX_UNIFORMS_PER_BLOCK];

  /* Uniform Blocks. */
  uint32_t total_uniform_blocks_;
  uint32_t max_uniformbuf_index_;
  MTLShaderBufferBlock ubos_[MTL_MAX_BUFFER_BINDINGS];
  MTLShaderBufferBlock push_constant_block_;

  /* Storage blocks. */
  uint32_t total_storage_blocks_;
  uint32_t max_storagebuf_index_;
  MTLShaderBufferBlock ssbos_[MTL_MAX_BUFFER_BINDINGS];

  /* Textures. */
  /* Textures support explicit binding indices, so some texture slots
   * remain unused. */
  uint32_t total_textures_;
  int max_texture_index_;
  MTLShaderTexture textures_[MTL_MAX_TEXTURE_SLOTS];

  /* Specialization constants. */
  uint32_t total_constants_;
  Vector<MTLShaderConstant> constants_;

  /* Whether argument buffers are used for sampler bindings. */
  bool sampler_use_argument_buffer_;
  int sampler_argument_buffer_bind_index_[3];

  /* Attribute Mask. */
  uint32_t enabled_attribute_mask_;

  /* Debug. */
  char name[256];

 public:
  MTLShaderInterface(const char *name);
  ~MTLShaderInterface();

  void init();
  void add_input_attribute(uint32_t name_offset,
                           uint32_t attribute_location,
                           MTLVertexFormat format,
                           uint32_t buffer_index,
                           uint32_t size,
                           uint32_t offset,
                           int matrix_element_count = 1);
  uint32_t add_uniform_block(uint32_t name_offset,
                             uint32_t buffer_index,
                             uint32_t location,
                             uint32_t size,
                             ShaderStage stage_mask = ShaderStage::ANY);
  uint32_t add_storage_block(uint32_t name_offset,
                             uint32_t buffer_index,
                             uint32_t location,
                             uint32_t size,
                             ShaderStage stage_mask = ShaderStage::ANY);
  void add_uniform(uint32_t name_offset, eMTLDataType type, int array_len = 1);
  void add_texture(uint32_t name_offset,
                   uint32_t texture_slot,
                   uint32_t location,
                   eGPUTextureType tex_binding_type,
                   eGPUSamplerFormat sampler_format,
                   bool is_texture_sampler,
                   ShaderStage stage_mask = ShaderStage::FRAGMENT,
                   int tex_buffer_ssbo_location = -1);
  void add_push_constant_block(uint32_t name_offset);
  void add_constant(uint32_t name_offset);

  /* Resolve and cache locations of builtin uniforms and uniform blocks. */
  void map_builtins();
  void set_sampler_properties(bool use_argument_buffer,
                              uint32_t argument_buffer_bind_index_vert,
                              uint32_t argument_buffer_bind_index_frag,
                              uint32_t argument_buffer_bind_index_compute);

  /* Prepare #ShaderInput interface for binding resolution. */
  void prepare_common_shader_inputs();

  /* Fetch Uniforms. */
  const MTLShaderUniform &get_uniform(uint index) const;
  uint32_t get_total_uniforms() const;

  /* Fetch Constants. */
  uint32_t get_total_constants() const;

  /* Fetch Uniform Blocks. */
  const MTLShaderBufferBlock &get_uniform_block(uint index) const;
  uint32_t get_total_uniform_blocks() const;
  bool has_uniform_block(uint32_t block_index) const;
  uint32_t get_uniform_block_size(uint32_t block_index) const;

  /* Fetch Storage Blocks. */
  const MTLShaderBufferBlock &get_storage_block(uint index) const;
  uint32_t get_total_storage_blocks() const;
  bool has_storage_block(uint32_t block_index) const;
  uint32_t get_storage_block_size(uint32_t block_index) const;

  /* Push constant uniform data block should always be available. */
  const MTLShaderBufferBlock &get_push_constant_block() const;
  uint32_t get_max_buffer_index() const;

  /* Fetch textures. */
  const MTLShaderTexture &get_texture(uint index) const;
  uint32_t get_total_textures() const;
  uint32_t get_max_texture_index() const;
  bool uses_argument_buffer_for_samplers() const;
  int get_argument_buffer_bind_index(ShaderStage stage) const;

  /* Fetch Attributes. */
  const MTLShaderInputAttribute &get_attribute(uint index) const;
  uint32_t get_total_attributes() const;
  uint32_t get_total_vertex_stride() const;
  uint32_t get_enabled_attribute_mask() const;

  /* Name buffer fetching. */
  const char *get_name_at_offset(uint32_t offset) const;

  /* Interface name. */
  const char *get_name() const
  {
    return this->name;
  }

  /* Argument buffer encoder management. */
  id<MTLArgumentEncoder> find_argument_encoder(int buffer_index) const;

  void insert_argument_encoder(int buffer_index, id encoder);

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLShaderInterface");
};

}  // namespace blender::gpu
