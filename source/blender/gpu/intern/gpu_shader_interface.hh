/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU shader interface (C --> GLSL)
 *
 * Structure detailing needed vertex inputs and resources for a specific shader.
 * A shader interface can be shared between two similar shaders.
 */

#pragma once

#include <cstring> /* required for STREQ later on. */

#include "BLI_hash.h"
#include "BLI_sys_types.h"

#include "GPU_format.hh"
#include "GPU_shader.hh"
#include "GPU_vertex_format.hh" /* GPU_VERT_ATTR_MAX_LEN */
#include "gpu_shader_create_info.hh"
#include "gpu_texture_private.hh"

namespace blender::gpu {

struct ShaderInput {
  uint32_t name_offset;
  uint32_t name_hash;
  /**
   * Location is openGl legacy and its legacy usages should be phased out in Blender 3.7.
   *
   * Vulkan backend use location to encode the descriptor set binding. This binding is different
   * than the binding stored in the binding attribute. In Vulkan the binding inside a descriptor
   * set must be unique. In future the location will also be used to select the right descriptor
   * set.
   */
  int32_t location;
  /** Defined at interface creation or in shader. Only for Samplers, UBOs and Vertex Attributes. */
  int32_t binding;
};

/**
 * Implementation of Shader interface.
 * Base class which is then specialized for each implementation (GL, VK, ...).
 */
class ShaderInterface {
  friend shader::ShaderCreateInfo;
  /* TODO(fclem): should be protected. */
 public:
  /** Flat array. In this order: Attributes, Ubos, Uniforms, SSBOs, Constants. */
  ShaderInput *inputs_ = nullptr;
  /** Buffer containing all inputs names separated by '\0'. */
  char *name_buffer_ = nullptr;
  /** Input counts inside input array. */
  uint attr_len_ = 0;
  uint ubo_len_ = 0;
  uint uniform_len_ = 0;
  uint ssbo_len_ = 0;
  uint constant_len_ = 0;
  /** Enabled bind-points that needs to be fed with data. */
  uint16_t enabled_attr_mask_ = 0;
  uint16_t enabled_ubo_mask_ = 0;
  uint8_t enabled_ima_mask_ = 0;
  uint64_t enabled_tex_mask_ = 0;
  uint16_t enabled_ssbo_mask_ = 0;
  /* Bitmask to apply to enabled_ssbo_mask_ to get attributes that are sourced from SSBOs. */
  uint16_t ssbo_attr_mask_ = 0;
  /** Location of builtin uniforms. Fast access, no lookup needed. */
  int32_t builtins_[GPU_NUM_UNIFORMS];
  int32_t builtin_blocks_[GPU_NUM_UNIFORM_BLOCKS];

  /**
   * Currently only used for `GPU_shader_get_attribute_info`.
   * This utility is useful for automatic creation of `GPUVertFormat` in Python.
   * Use `ShaderInput::location` to identify the `Type`.
   */
  uint8_t attr_types_[GPU_VERT_ATTR_MAX_LEN];

  /* Formats of all image units. */
  std::array<TextureWriteFormat, GPU_MAX_IMAGE> image_formats_;

  ShaderInterface();
  virtual ~ShaderInterface();

  void debug_print() const;

  const ShaderInput *attr_get(const StringRefNull name) const
  {
    return input_lookup(inputs_, attr_len_, name);
  }
  const ShaderInput *attr_get(const int binding) const
  {
    return input_lookup(inputs_, attr_len_, binding);
  }

  const ShaderInput *ubo_get(const StringRefNull name) const
  {
    return input_lookup(inputs_ + attr_len_, ubo_len_, name);
  }
  const ShaderInput *ubo_get(const int binding) const
  {
    return input_lookup(inputs_ + attr_len_, ubo_len_, binding);
  }

  const ShaderInput *uniform_get(const StringRefNull name) const
  {
    return input_lookup(inputs_ + attr_len_ + ubo_len_, uniform_len_, name);
  }

  const ShaderInput *texture_get(const int binding) const
  {
    return input_lookup(inputs_ + attr_len_ + ubo_len_, uniform_len_, binding);
  }

  const ShaderInput *ssbo_get(const StringRefNull name) const
  {
    return input_lookup(inputs_ + attr_len_ + ubo_len_ + uniform_len_, ssbo_len_, name);
  }
  const ShaderInput *ssbo_get(const int binding) const
  {
    return input_lookup(inputs_ + attr_len_ + ubo_len_ + uniform_len_, ssbo_len_, binding);
  }

  const ShaderInput *constant_get(const StringRefNull name) const
  {
    return input_lookup(
        inputs_ + attr_len_ + ubo_len_ + uniform_len_ + ssbo_len_, constant_len_, name);
  }

  const char *input_name_get(const ShaderInput *input) const
  {
    return name_buffer_ + input->name_offset;
  }

  /* Returns uniform location. */
  int32_t uniform_builtin(const GPUUniformBuiltin builtin) const
  {
    BLI_assert(builtin >= 0 && builtin < GPU_NUM_UNIFORMS);
    return builtins_[builtin];
  }

  /* Returns binding position. */
  int32_t ubo_builtin(const GPUUniformBlockBuiltin builtin) const
  {
    BLI_assert(builtin >= 0 && builtin < GPU_NUM_UNIFORM_BLOCKS);
    return builtin_blocks_[builtin];
  }

  inline uint valid_bindings_get(const ShaderInput *const inputs, const uint inputs_len) const;

  bool attr_len_get() const
  {
    return attr_len_;
  }

  bool ubo_len_get() const
  {
    return ubo_len_;
  }

  bool uniform_len_get() const
  {
    return uniform_len_;
  }

  bool ssbo_len_get() const
  {
    return ssbo_len_;
  }

  bool constant_len_get() const
  {
    return constant_len_;
  }

 protected:
  static inline const char *builtin_uniform_name(GPUUniformBuiltin u);
  static inline const char *builtin_uniform_block_name(GPUUniformBlockBuiltin u);

  inline uint32_t set_input_name(ShaderInput *input, char *name, uint32_t name_len) const;
  inline void copy_input_name(ShaderInput *input,
                              const StringRefNull &name,
                              char *name_buffer,
                              uint32_t &name_buffer_offset) const;

  /**
   * Finalize interface construction by sorting the #ShaderInputs for faster lookups.
   */
  void sort_inputs();

  void set_image_formats_from_info(const shader::ShaderCreateInfo &info);

 private:
  inline const ShaderInput *input_lookup(const ShaderInput *const inputs,
                                         uint inputs_len,
                                         StringRefNull name) const;

  inline const ShaderInput *input_lookup(const ShaderInput *const inputs,
                                         uint inputs_len,
                                         int binding) const;
};

inline const char *ShaderInterface::builtin_uniform_name(GPUUniformBuiltin u)
{
  switch (u) {
    case GPU_UNIFORM_MODEL:
      return "ModelMatrix";
    case GPU_UNIFORM_VIEW:
      return "ViewMatrix";
    case GPU_UNIFORM_MODELVIEW:
      return "ModelViewMatrix";
    case GPU_UNIFORM_PROJECTION:
      return "ProjectionMatrix";
    case GPU_UNIFORM_VIEWPROJECTION:
      return "ViewProjectionMatrix";
    case GPU_UNIFORM_MVP:
      return "ModelViewProjectionMatrix";

    case GPU_UNIFORM_MODEL_INV:
      return "ModelMatrixInverse";
    case GPU_UNIFORM_VIEW_INV:
      return "ViewMatrixInverse";
    case GPU_UNIFORM_MODELVIEW_INV:
      return "ModelViewMatrixInverse";
    case GPU_UNIFORM_PROJECTION_INV:
      return "ProjectionMatrixInverse";
    case GPU_UNIFORM_VIEWPROJECTION_INV:
      return "ViewProjectionMatrixInverse";

    case GPU_UNIFORM_NORMAL:
      return "NormalMatrix";
    case GPU_UNIFORM_CLIPPLANES:
      return "WorldClipPlanes";

    case GPU_UNIFORM_COLOR:
      return "color";
    case GPU_UNIFORM_BASE_INSTANCE:
      return "gpu_BaseInstance";
    case GPU_UNIFORM_RESOURCE_CHUNK:
      return "drw_resourceChunk";
    case GPU_UNIFORM_RESOURCE_ID:
      return "drw_ResourceID";
    case GPU_UNIFORM_SRGB_TRANSFORM:
      return "srgbTarget";
    case GPU_UNIFORM_SCENE_LINEAR_XFORM:
      return "gpu_scene_linear_to_rec709";

    default:
      return nullptr;
  }
}

inline const char *ShaderInterface::builtin_uniform_block_name(GPUUniformBlockBuiltin u)
{
  switch (u) {
    case GPU_UNIFORM_BLOCK_VIEW:
      return "viewBlock";
    case GPU_UNIFORM_BLOCK_MODEL:
      return "modelBlock";
    case GPU_UNIFORM_BLOCK_INFO:
      return "infoBlock";

    case GPU_UNIFORM_BLOCK_DRW_VIEW:
      return "drw_view_";
    case GPU_UNIFORM_BLOCK_DRW_MODEL:
      return "drw_matrices";
    case GPU_UNIFORM_BLOCK_DRW_INFOS:
      return "drw_infos";
    case GPU_UNIFORM_BLOCK_DRW_CLIPPING:
      return "drw_clipping_";
    default:
      return nullptr;
  }
}

/* Returns string length including '\0' terminator. */
inline uint32_t ShaderInterface::set_input_name(ShaderInput *input,
                                                char *name,
                                                uint32_t name_len) const
{
  /* remove "[0]" from array name */
  if (name[name_len - 1] == ']') {
    for (; name_len > 1; name_len--) {
      if (name[name_len] == '[') {
        name[name_len] = '\0';
        break;
      }
    }
  }

  input->name_offset = (uint32_t)(name - name_buffer_);
  input->name_hash = BLI_hash_string(name);
  return name_len + 1; /* include NULL terminator */
}

inline void ShaderInterface::copy_input_name(ShaderInput *input,
                                             const StringRefNull &name,
                                             char *name_buffer,
                                             uint32_t &name_buffer_offset) const
{
  uint32_t name_len = name.size();
  /* Copy include NULL terminator. */
  memcpy(name_buffer + name_buffer_offset, name.c_str(), name_len + 1);
  name_buffer_offset += set_input_name(input, name_buffer + name_buffer_offset, name_len);
}

inline const ShaderInput *ShaderInterface::input_lookup(const ShaderInput *const inputs,
                                                        const uint inputs_len,
                                                        const StringRefNull name) const
{
  const uint name_hash = BLI_hash_string(name.c_str());
  /* Simple linear search for now. */
  for (int i = inputs_len - 1; i >= 0; i--) {
    if (inputs[i].name_hash == name_hash) {
      if ((i > 0) && UNLIKELY(inputs[i - 1].name_hash == name_hash)) {
        /* Hash collision resolve. */
        for (; i >= 0 && inputs[i].name_hash == name_hash; i--) {
          if (name == (name_buffer_ + inputs[i].name_offset)) {
            return inputs + i; /* not found */
          }
        }
        return nullptr; /* not found */
      }

      /* This is a bit dangerous since we could have a hash collision.
       * where the asked uniform that does not exist has the same hash
       * as a real uniform. */
      BLI_assert(name == (name_buffer_ + inputs[i].name_offset));
      return inputs + i;
    }
  }
  return nullptr; /* not found */
}

inline const ShaderInput *ShaderInterface::input_lookup(const ShaderInput *const inputs,
                                                        const uint inputs_len,
                                                        const int binding) const
{
  /* Simple linear search for now. */
  for (int i = inputs_len - 1; i >= 0; i--) {
    if (inputs[i].binding == binding) {
      return inputs + i;
    }
  }
  return nullptr; /* not found */
}

inline uint ShaderInterface::valid_bindings_get(const ShaderInput *const inputs,
                                                const uint inputs_len) const
{
  /* Simple linear search for now. */
  int valid_bindings = 0;
  for (int i = inputs_len - 1; i >= 0; i--) {
    if (inputs[i].binding > -1) {
      valid_bindings++;
    }
  }
  return valid_bindings;
}

}  // namespace blender::gpu
