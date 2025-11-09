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
#include "mtl_push_constant.hh"
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

/**
 * Implementation of Shader interface for Metal Back-end.
 */
class MTLShaderInterface : public ShaderInterface {
 private:
  /* Argument buffer encoder for sampler buffer. */
  id<MTLArgumentEncoder> arg_encoder_ = nil;

  /* Bit Mask representing the free buffer slots from this interface.
   * Used to bind the vertex and index buffers.
   * This references the Metal Buffer bind space, not the GPU one. */
  uint32_t vertex_buffer_mask_ = ~(0xFFFFFFFFu << MTL_MAX_BUFFER_BINDINGS);

  shader::BuiltinBits shader_builtins_ = shader::BuiltinBits::NONE;

  /* Used for texture atomic workaround. */
  int image_names_offsets_[MTL_MAX_IMAGE_SLOTS];
  int sampler_names_offsets_[MTL_MAX_SAMPLER_SLOTS];

  /* Debug. */
  char name[256];

 public:
  MTLShaderInterface(const char *name,
                     const shader::ShaderCreateInfo &info,
                     MTLPushConstantBuf *push_constant_buf = nullptr);
  ~MTLShaderInterface() override;

  uint32_t vertex_buffer_mask() const
  {
    return vertex_buffer_mask_;
  }

  bool use_layer() const
  {
    return bool(shader_builtins_ & shader::BuiltinBits::LAYER);
  }
  bool use_viewport_index() const
  {
    return bool(shader_builtins_ & shader::BuiltinBits::VIEWPORT_INDEX);
  }
  bool use_samplers_argument_buffer() const
  {
    return bool(shader_builtins_ & shader::BuiltinBits::USE_SAMPLER_ARG_BUFFER);
  }
  bool use_texture_atomic() const
  {
    return bool(shader_builtins_ & shader::BuiltinBits::TEXTURE_ATOMIC);
  }

  bool is_point_shader() const
  {
    return (shader_builtins_ & shader::BuiltinBits::POINT_SIZE) == shader::BuiltinBits::POINT_SIZE;
  }

  /* Name buffer fetching. */
  const char *name_at_offset(uint32_t offset) const;

  /* Interface name. */
  const char *name_get() const
  {
    return this->name;
  }

  const char *image_name_get(int slot) const
  {
    return name_at_offset(image_names_offsets_[slot]);
  }

  const char *sampler_name_get(int slot) const
  {
    return name_at_offset(sampler_names_offsets_[slot]);
  }

  id<MTLArgumentEncoder> ensure_argument_encoder(id<MTLFunction> mtl_function);

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLShaderInterface");
};

}  // namespace blender::gpu
