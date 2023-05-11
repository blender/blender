/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "vk_shader_interface.hh"
#include "vk_backend.hh"
#include "vk_context.hh"

namespace blender::gpu {

void VKShaderInterface::init(const shader::ShaderCreateInfo &info)
{
  static char PUSH_CONSTANTS_FALLBACK_NAME[] = "push_constants_fallback";
  static size_t PUSH_CONSTANTS_FALLBACK_NAME_LEN = strlen(PUSH_CONSTANTS_FALLBACK_NAME);

  using namespace blender::gpu::shader;

  attr_len_ = info.vertex_inputs_.size();
  uniform_len_ = info.push_constants_.size();
  ssbo_len_ = 0;
  ubo_len_ = 0;
  image_offset_ = -1;

  Vector<ShaderCreateInfo::Resource> all_resources;
  all_resources.extend(info.pass_resources_);
  all_resources.extend(info.batch_resources_);

  for (ShaderCreateInfo::Resource &res : all_resources) {
    switch (res.bind_type) {
      case ShaderCreateInfo::Resource::BindType::IMAGE:
        uniform_len_++;
        break;
      case ShaderCreateInfo::Resource::BindType::SAMPLER:
        image_offset_ = max_ii(image_offset_, res.slot);
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

  /* Reserve 1 uniform buffer for push constants fallback. */
  size_t names_size = info.interface_names_size_;
  const VKDevice &device = VKBackend::get().device_get();
  const VKPushConstants::StorageType push_constants_storage_type =
      VKPushConstants::Layout::determine_storage_type(info, device.physical_device_limits_get());
  if (push_constants_storage_type == VKPushConstants::StorageType::UNIFORM_BUFFER) {
    ubo_len_++;
    names_size += PUSH_CONSTANTS_FALLBACK_NAME_LEN + 1;
  }

  /* Make sure that the image slots don't overlap with the sampler slots. */
  image_offset_++;

  int32_t input_tot_len = attr_len_ + ubo_len_ + uniform_len_ + ssbo_len_;
  inputs_ = static_cast<ShaderInput *>(
      MEM_calloc_arrayN(input_tot_len, sizeof(ShaderInput), __func__));
  ShaderInput *input = inputs_;

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
      input++;
    }
  }
  /* Add push constant when using uniform buffer as fallback. */
  int32_t push_constants_fallback_location = -1;
  if (push_constants_storage_type == VKPushConstants::StorageType::UNIFORM_BUFFER) {
    copy_input_name(input, PUSH_CONSTANTS_FALLBACK_NAME, name_buffer_, name_buffer_offset);
    input->location = input->binding = -1;
    input++;
  }

  /* Images, Samplers and buffers. */
  for (const ShaderCreateInfo::Resource &res : all_resources) {
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER) {
      copy_input_name(input, res.sampler.name, name_buffer_, name_buffer_offset);
      input->location = input->binding = res.slot;
      input++;
    }
    else if (res.bind_type == ShaderCreateInfo::Resource::BindType::IMAGE) {
      copy_input_name(input, res.image.name, name_buffer_, name_buffer_offset);
      input->location = input->binding = res.slot + image_offset_;
      input++;
    }
  }

  /* Push constants. */
  int32_t push_constant_location = 1024;
  for (const ShaderCreateInfo::PushConst &push_constant : info.push_constants_) {
    copy_input_name(input, push_constant.name, name_buffer_, name_buffer_offset);
    input->location = push_constant_location++;
    input->binding = -1;
    input++;
  }

  /* Storage buffers */
  for (const ShaderCreateInfo::Resource &res : all_resources) {
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER) {
      copy_input_name(input, res.storagebuf.name, name_buffer_, name_buffer_offset);
      input->location = input->binding = res.slot;
      input++;
    }
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

  /* Determine the descriptor set locations after the inputs have been sorted. */
  /* Note: input_tot_len is sometimes more than we need. */
  const uint32_t resources_len = input_tot_len;
  descriptor_set_locations_ = Array<VKDescriptorSet::Location>(resources_len);
  uint32_t descriptor_set_location = 0;
  for (ShaderCreateInfo::Resource &res : all_resources) {
    const ShaderInput *input = shader_input_get(res);
    descriptor_set_location_update(input, descriptor_set_location++);
  }

  /* Post initializing push constants. */
  /* Determine the binding location of push constants fallback buffer. */
  int32_t push_constant_descriptor_set_location = -1;
  if (push_constants_storage_type == VKPushConstants::StorageType::UNIFORM_BUFFER) {
    push_constant_descriptor_set_location = descriptor_set_location++;
    const ShaderInput *push_constant_input = ubo_get(PUSH_CONSTANTS_FALLBACK_NAME);
    descriptor_set_location_update(push_constant_input, push_constants_fallback_location);
  }
  push_constants_layout_.init(
      info, *this, push_constants_storage_type, push_constant_descriptor_set_location);
}

static int32_t shader_input_index(const ShaderInput *shader_inputs,
                                  const ShaderInput *shader_input)
{
  int32_t index = (shader_input - shader_inputs);
  return index;
}

void VKShaderInterface::descriptor_set_location_update(const ShaderInput *shader_input,
                                                       const VKDescriptorSet::Location location)
{
  int32_t index = shader_input_index(inputs_, shader_input);
  descriptor_set_locations_[index] = location;
}

const VKDescriptorSet::Location VKShaderInterface::descriptor_set_location(
    const ShaderInput *shader_input) const
{
  int32_t index = shader_input_index(inputs_, shader_input);
  return descriptor_set_locations_[index];
}

const VKDescriptorSet::Location VKShaderInterface::descriptor_set_location(
    const shader::ShaderCreateInfo::Resource &resource) const
{
  const ShaderInput *shader_input = shader_input_get(resource);
  BLI_assert(shader_input);
  return descriptor_set_location(shader_input);
}

const std::optional<VKDescriptorSet::Location> VKShaderInterface::descriptor_set_location(
    const shader::ShaderCreateInfo::Resource::BindType &bind_type, int binding) const
{
  const ShaderInput *shader_input = shader_input_get(bind_type, binding);
  if (shader_input == nullptr) {
    return std::nullopt;
  }
  return descriptor_set_location(shader_input);
}

const ShaderInput *VKShaderInterface::shader_input_get(
    const shader::ShaderCreateInfo::Resource &resource) const
{
  return shader_input_get(resource.bind_type, resource.slot);
}

const ShaderInput *VKShaderInterface::shader_input_get(
    const shader::ShaderCreateInfo::Resource::BindType &bind_type, int binding) const
{
  switch (bind_type) {
    case shader::ShaderCreateInfo::Resource::BindType::IMAGE:
      return texture_get(binding + image_offset_);
    case shader::ShaderCreateInfo::Resource::BindType::SAMPLER:
      return texture_get(binding);
    case shader::ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
      return ssbo_get(binding);
    case shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
      return ubo_get(binding);
  }
  return nullptr;
}

}  // namespace blender::gpu
