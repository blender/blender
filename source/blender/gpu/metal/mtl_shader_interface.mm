/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU shader interface (C --> GLSL)
 */

#include "BLI_bitmap.h"

#include "GPU_capabilities.hh"

#include "mtl_common.hh"
#include "mtl_debug.hh"
#include "mtl_push_constant.hh"
#include "mtl_shader_generate.hh"
#include "mtl_shader_interface.hh"
#include "mtl_shader_interface_type.hh"

#include "BLI_math_base.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"

namespace blender::gpu {

MTLShaderInterface::MTLShaderInterface(const char *name,
                                       const shader::ShaderCreateInfo &info,
                                       MTLPushConstantBuf *push_constant_buf)
{
  using namespace blender::gpu::shader;

  if (name != nullptr) {
    STRNCPY(this->name, name);
  }

  shader_builtins_ = info.builtins_;

  attr_len_ = info.vertex_inputs_.size();
  uniform_len_ = info.push_constants_.size();
  constant_len_ = info.specialization_constants_.size();
  ssbo_len_ = 0;
  ubo_len_ = 0;
  Vector<ShaderCreateInfo::Resource> all_resources;
  all_resources.extend(info.pass_resources_);
  all_resources.extend(info.batch_resources_);
  all_resources.extend(info.geometry_resources_);

  for (ShaderCreateInfo::Resource &res : all_resources) {
    switch (res.bind_type) {
      case ShaderCreateInfo::Resource::BindType::IMAGE:
      case ShaderCreateInfo::Resource::BindType::SAMPLER:
        uniform_len_++;
        break;
      case ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
        ubo_len_++;
        break;
      case ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
        ssbo_len_++;
        break;
    }
  }

  int32_t input_tot_len = attr_len_ + ubo_len_ + uniform_len_ + ssbo_len_ + constant_len_;
  inputs_ = MEM_calloc_arrayN<ShaderInput>(input_tot_len, __func__);
  ShaderInput *input = inputs_;

  size_t names_size = info.interface_names_size_;
  name_buffer_ = (char *)MEM_mallocN(names_size, "name_buffer");
  uint32_t name_buffer_offset = 0;

  /* Attributes */
  for (const ShaderCreateInfo::VertIn &attr : info.vertex_inputs_) {
    copy_input_name(input, attr.name, name_buffer_, name_buffer_offset);
    input->location = input->binding = attr.index;
    if (input->location != -1) {
      enabled_attr_mask_ |= (1 << input->location);
      /* Used in `GPU_shader_get_attribute_info`. */
      attr_types_[input->location] = uint8_t(attr.type);
    }
    input++;
  }

  /* Uniform blocks */
  for (const ShaderCreateInfo::Resource &res : all_resources) {
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER) {
      copy_input_name(input, res.uniformbuf.name, name_buffer_, name_buffer_offset);
      input->location = input->binding = res.slot;
      enabled_ubo_mask_ |= (1 << input->binding);
      vertex_buffer_mask_ &= ~(1 << (input->binding + MTL_UBO_SLOT_OFFSET));
      input++;
    }
  }

  /* Images, Samplers and buffers. */
  for (const ShaderCreateInfo::Resource &res : all_resources) {
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER) {
      sampler_names_offsets_[res.slot] = name_buffer_offset;
      copy_input_name(input, res.sampler.name, name_buffer_, name_buffer_offset);
      input->location = -1; /* Setting location is not possible in MSL. */
      input->binding = res.slot;
      enabled_tex_mask_ |= (1ull << input->binding);
      input++;
    }
    else if (res.bind_type == ShaderCreateInfo::Resource::BindType::IMAGE) {
      image_names_offsets_[res.slot] = name_buffer_offset;
      copy_input_name(input, res.image.name, name_buffer_, name_buffer_offset);
      input->location = -1; /* Setting location is not possible in MSL. */
      input->binding = res.slot;
      enabled_ima_mask_ |= (1 << input->binding);
      input++;
    }
  }
  set_image_formats_from_info(info);

  /* Push constants. */
  for (const ShaderCreateInfo::PushConst &push_constant : info.push_constants_) {
    copy_input_name(input, push_constant.name, name_buffer_, name_buffer_offset);
    input->location = push_constant_buf->append(push_constant);
    input->binding = -1;
    input++;
  }

  /* Storage buffers */
  for (const ShaderCreateInfo::Resource &res : all_resources) {
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER) {
      copy_input_name(input, res.storagebuf.name, name_buffer_, name_buffer_offset);
      input->location = input->binding = res.slot;
      enabled_ssbo_mask_ |= (1 << input->binding);
      vertex_buffer_mask_ &= ~(1 << (input->binding + MTL_SSBO_SLOT_OFFSET));
      input++;
    }
  }

  for (const ShaderCreateInfo::Resource &res : info.geometry_resources_) {
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER) {
      ssbo_attr_mask_ |= (1 << res.slot);
    }
    else {
      BLI_assert_msg(0, "Resource type is not supported for Geometry frequency");
    }
  }

  /* Constants */
  int constant_id = 0;
  for (const SpecializationConstant &constant : info.specialization_constants_) {
    copy_input_name(input, constant.name, name_buffer_, name_buffer_offset);
    input->location = constant_id++;
    input++;
  }

  sort_inputs();

  /* Builtin Uniforms */
  for (int32_t u_int = 0; u_int < GPU_NUM_UNIFORMS; u_int++) {
    GPUUniformBuiltin u = static_cast<GPUUniformBuiltin>(u_int);
    const ShaderInput *uni = this->uniform_get(builtin_uniform_name(u));
    builtins_[u] = (uni != nullptr) ? uni->location : -1;
  }

  /* Builtin Uniforms Blocks */
  for (int32_t u_int = 0; u_int < GPU_NUM_UNIFORM_BLOCKS; u_int++) {
    GPUUniformBlockBuiltin u = static_cast<GPUUniformBlockBuiltin>(u_int);
    const ShaderInput *block = this->ubo_get(builtin_uniform_block_name(u));
    builtin_blocks_[u] = (block != nullptr) ? block->binding : -1;
  }
}

MTLShaderInterface::~MTLShaderInterface()
{
  if (arg_encoder_ != nil) {
    [arg_encoder_ release];
    arg_encoder_ = nil;
  }
}

const char *MTLShaderInterface::name_at_offset(uint32_t offset) const
{
  return name_buffer_ + offset;
}

id<MTLArgumentEncoder> MTLShaderInterface::ensure_argument_encoder(id<MTLFunction> mtl_function)
{
  if (arg_encoder_ == nil) {
    arg_encoder_ = [mtl_function
        newArgumentEncoderWithBufferIndex:MTL_SAMPLER_ARGUMENT_BUFFER_SLOT];
  }
  return arg_encoder_;
}

}  // namespace blender::gpu
