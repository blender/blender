/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  shader_builtins_ = info.builtins_;

  attr_len_ = info.vertex_inputs_.size();
  uniform_len_ = info.push_constants_.size();
  constant_len_ = info.specialization_constants_.size();
  ssbo_len_ = 0;
  ubo_len_ = 0;
  image_offset_ = -1;
  int image_max_binding = -1;
  Vector<ShaderCreateInfo::Resource> all_resources;
  all_resources.extend(info.pass_resources_);
  all_resources.extend(info.batch_resources_);

  for (ShaderCreateInfo::Resource &res : all_resources) {
    switch (res.bind_type) {
      case ShaderCreateInfo::Resource::BindType::IMAGE:
        uniform_len_++;
        image_max_binding = max_ii(image_max_binding, res.slot);
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
      VKPushConstants::Layout::determine_storage_type(info, device);
  if (push_constants_storage_type == VKPushConstants::StorageType::UNIFORM_BUFFER) {
    ubo_len_++;
    names_size += PUSH_CONSTANTS_FALLBACK_NAME_LEN + 1;
  }

  /* Make sure that the image slots don't overlap with other sampler or image slots. */
  image_offset_++;
  if (image_offset_ != 0 && image_offset_ <= image_max_binding) {
    image_offset_ = image_max_binding + 1;
  }

  int32_t input_tot_len = attr_len_ + ubo_len_ + uniform_len_ + ssbo_len_ + constant_len_;
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

  /* Constants */
  int constant_id = 0;
  for (const ShaderCreateInfo::SpecializationConstant &constant : info.specialization_constants_) {
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

  /* Determine the descriptor set locations after the inputs have been sorted. */
  /* NOTE: input_tot_len is sometimes more than we need. */
  const uint32_t resources_len = input_tot_len;

  /* Initialize the descriptor set layout. */
  init_descriptor_set_layout_info(info, resources_len, all_resources, push_constants_storage_type);

  /* Update the descriptor set locations, bind types and access masks. */
  descriptor_set_locations_ = Array<VKDescriptorSet::Location>(resources_len);
  descriptor_set_locations_.fill(-1);
  descriptor_set_bind_types_ = Array<shader::ShaderCreateInfo::Resource::BindType>(resources_len);
  descriptor_set_bind_types_.fill(shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER);
  access_masks_ = Array<VkAccessFlags>(resources_len);
  access_masks_.fill(VK_ACCESS_NONE);
  uint32_t descriptor_set_location = 0;
  for (ShaderCreateInfo::Resource &res : all_resources) {
    const ShaderInput *input = shader_input_get(res);
    BLI_assert(input);
    descriptor_set_location_update(input, descriptor_set_location++, res.bind_type, res);
  }

  /* Post initializing push constants. */
  /* Determine the binding location of push constants fallback buffer. */
  int32_t push_constant_descriptor_set_location = -1;
  if (push_constants_storage_type == VKPushConstants::StorageType::UNIFORM_BUFFER) {
    push_constant_descriptor_set_location = descriptor_set_location++;
    const ShaderInput *push_constant_input = ubo_get(PUSH_CONSTANTS_FALLBACK_NAME);
    descriptor_set_location_update(push_constant_input,
                                   push_constants_fallback_location,
                                   shader::ShaderCreateInfo::Resource::UNIFORM_BUFFER,
                                   std::nullopt);
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

void VKShaderInterface::descriptor_set_location_update(
    const ShaderInput *shader_input,
    const VKDescriptorSet::Location location,
    const shader::ShaderCreateInfo::Resource::BindType bind_type,
    std::optional<const shader::ShaderCreateInfo::Resource> resource)
{
  BLI_assert_msg(resource.has_value() ||
                     bind_type == shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER,
                 "Incorrect parameters, when no resource is given, it must be the uniform buffer "
                 "for storing push constants.");
  BLI_assert_msg(!resource.has_value() || resource->bind_type == bind_type,
                 "Incorrect parameter, bind types do not match.");

  int32_t index = shader_input_index(inputs_, shader_input);
  BLI_assert(descriptor_set_locations_[index].binding == -1);
  descriptor_set_locations_[index] = location;
  descriptor_set_bind_types_[index] = bind_type;

  VkAccessFlags vk_access_flags = VK_ACCESS_NONE;
  if (resource.has_value()) {
    switch (resource->bind_type) {
      case shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
        vk_access_flags |= VK_ACCESS_UNIFORM_READ_BIT;
        break;

      case shader::ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
        if (bool(resource->storagebuf.qualifiers & shader::Qualifier::READ) == true) {
          vk_access_flags |= VK_ACCESS_SHADER_READ_BIT;
        }
        if (bool(resource->storagebuf.qualifiers & shader::Qualifier::WRITE) == true) {
          vk_access_flags |= VK_ACCESS_SHADER_WRITE_BIT;
        }
        break;

      case shader::ShaderCreateInfo::Resource::BindType::IMAGE:
        if (bool(resource->image.qualifiers & shader::Qualifier::READ) == true) {
          vk_access_flags |= VK_ACCESS_SHADER_READ_BIT;
        }
        if (bool(resource->image.qualifiers & shader::Qualifier::WRITE) == true) {
          vk_access_flags |= VK_ACCESS_SHADER_WRITE_BIT;
        }
        break;

      case shader::ShaderCreateInfo::Resource::BindType::SAMPLER:
        vk_access_flags |= VK_ACCESS_SHADER_READ_BIT;
        break;
    };
  }
  else if (bind_type == shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER) {
    access_masks_[index] = VK_ACCESS_UNIFORM_READ_BIT;
  }
  access_masks_[index] = vk_access_flags;
}

const VKDescriptorSet::Location VKShaderInterface::descriptor_set_location(
    const ShaderInput *shader_input) const
{
  int32_t index = shader_input_index(inputs_, shader_input);
  return descriptor_set_locations_[index];
}

const shader::ShaderCreateInfo::Resource::BindType VKShaderInterface::descriptor_set_bind_type(
    const ShaderInput *shader_input) const
{
  int32_t index = shader_input_index(inputs_, shader_input);
  return descriptor_set_bind_types_[index];
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
  if (descriptor_set_bind_type(shader_input) != bind_type) {
    return std::nullopt;
  }
  return descriptor_set_location(shader_input);
}

const VkAccessFlags VKShaderInterface::access_mask(const ShaderInput *shader_input) const
{
  int32_t index = shader_input_index(inputs_, shader_input);
  return access_masks_[index];
}

const VkAccessFlags VKShaderInterface::access_mask(
    const shader::ShaderCreateInfo::Resource::BindType &bind_type, int binding) const
{
  const ShaderInput *shader_input = shader_input_get(bind_type, binding);
  if (shader_input == nullptr) {
    return VK_ACCESS_NONE;
  }
  if (descriptor_set_bind_type(shader_input) != bind_type) {
    return VK_ACCESS_NONE;
  }
  return access_mask(shader_input);
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
      /* Not really nice, but the binding namespace between OpenGL and Vulkan don't match. To fix
       * this we need to check if one of both cases return a binding.
       * TODO: we might want to introduce a different API to fix this. */
      return texture_get((binding >= image_offset_) ? binding : binding + image_offset_);
    case shader::ShaderCreateInfo::Resource::BindType::SAMPLER:
      return texture_get(binding);
    case shader::ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
      return ssbo_get(binding);
    case shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
      return ubo_get(binding);
  }
  return nullptr;
}

void VKShaderInterface::init_descriptor_set_layout_info(
    const shader::ShaderCreateInfo &info,
    int64_t resources_len,
    Span<shader::ShaderCreateInfo::Resource> all_resources,
    VKPushConstants::StorageType push_constants_storage)
{
  BLI_assert(descriptor_set_layout_info_.bindings.is_empty());
  descriptor_set_layout_info_.bindings.reserve(resources_len);
  descriptor_set_layout_info_.vk_shader_stage_flags =
      info.compute_source_.is_empty() && info.compute_source_generated.empty() ?
          VK_SHADER_STAGE_ALL_GRAPHICS :
          VK_SHADER_STAGE_COMPUTE_BIT;
  for (const shader::ShaderCreateInfo::Resource &res : all_resources) {
    descriptor_set_layout_info_.bindings.append(to_vk_descriptor_type(res));
  }
  if (push_constants_storage == VKPushConstants::StorageType::UNIFORM_BUFFER) {
    descriptor_set_layout_info_.bindings.append(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  }
}

}  // namespace blender::gpu
