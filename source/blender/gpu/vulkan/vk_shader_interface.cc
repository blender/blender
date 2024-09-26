/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_shader_interface.hh"
#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_state_manager.hh"

namespace blender::gpu {

void VKShaderInterface::init(const shader::ShaderCreateInfo &info)
{
  static char PUSH_CONSTANTS_FALLBACK_NAME[] = "push_constants_fallback";
  static size_t PUSH_CONSTANTS_FALLBACK_NAME_LEN = strlen(PUSH_CONSTANTS_FALLBACK_NAME);
  static char SUBPASS_FALLBACK_NAME[] = "gpu_subpass_img_0";
  static size_t SUBPASS_FALLBACK_NAME_LEN = strlen(SUBPASS_FALLBACK_NAME);

  using namespace blender::gpu::shader;
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

  /* Sub-pass inputs are read as samplers.
   * In future this can change depending on extensions that will be supported. */
  uniform_len_ += info.subpass_inputs_.size();

  /* Reserve 1 uniform buffer for push constants fallback. */
  size_t names_size = info.interface_names_size_;
  const VKDevice &device = VKBackend::get().device;
  const VKPushConstants::StorageType push_constants_storage_type =
      VKPushConstants::Layout::determine_storage_type(info, device);
  if (push_constants_storage_type == VKPushConstants::StorageType::UNIFORM_BUFFER) {
    ubo_len_++;
    names_size += PUSH_CONSTANTS_FALLBACK_NAME_LEN + 1;
  }
  names_size += info.subpass_inputs_.size() * SUBPASS_FALLBACK_NAME_LEN;

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
  for (const ShaderCreateInfo::SubpassIn &subpass_in : info.subpass_inputs_) {
    copy_input_name(input, SUBPASS_FALLBACK_NAME, name_buffer_, name_buffer_offset);
    input->location = input->binding = subpass_in.index;
    input++;
  }
  for (const ShaderCreateInfo::Resource &res : all_resources) {
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER) {
      copy_input_name(input, res.sampler.name, name_buffer_, name_buffer_offset);
      input->location = input->binding = res.slot;
      input++;
    }
    else if (res.bind_type == ShaderCreateInfo::Resource::BindType::IMAGE) {
      copy_input_name(input, res.image.name, name_buffer_, name_buffer_offset);
      input->location = input->binding = res.slot + BIND_SPACE_IMAGE_OFFSET;
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

  /* Determine the descriptor set locations after the inputs have been sorted. */
  /* NOTE: input_tot_len is sometimes more than we need. */
  const uint32_t resources_len = input_tot_len;

  /* Initialize the descriptor set layout. */
  init_descriptor_set_layout_info(info, resources_len, all_resources, push_constants_storage_type);

  /* Update the descriptor set locations, bind types and access masks. */
  resource_bindings_ = Array<VKResourceBinding>(resources_len);
  resource_bindings_.fill({});

  uint32_t descriptor_set_location = 0;
  for (const ShaderCreateInfo::SubpassIn &subpass_in : info.subpass_inputs_) {
    const ShaderInput *input = shader_input_get(
        shader::ShaderCreateInfo::Resource::BindType::SAMPLER, subpass_in.index);
    BLI_assert(STREQ(input_name_get(input), SUBPASS_FALLBACK_NAME));
    BLI_assert(input);
    descriptor_set_location_update(input,
                                   descriptor_set_location++,
                                   shader::ShaderCreateInfo::Resource::BindType::SAMPLER,
                                   std::nullopt,
                                   VKImageViewArrayed::DONT_CARE);
  }
  for (ShaderCreateInfo::Resource &res : all_resources) {
    const ShaderInput *input = shader_input_get(res);
    BLI_assert(input);
    VKImageViewArrayed arrayed = VKImageViewArrayed::DONT_CARE;
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::IMAGE) {
      arrayed = ELEM(res.image.type,
                     shader::ImageType::FLOAT_1D_ARRAY,
                     shader::ImageType::FLOAT_2D_ARRAY,
                     shader::ImageType::FLOAT_CUBE_ARRAY,
                     shader::ImageType::INT_1D_ARRAY,
                     shader::ImageType::INT_2D_ARRAY,
                     shader::ImageType::INT_CUBE_ARRAY,
                     shader::ImageType::UINT_1D_ARRAY,
                     shader::ImageType::UINT_2D_ARRAY,
                     shader::ImageType::UINT_CUBE_ARRAY,
                     shader::ImageType::UINT_2D_ARRAY_ATOMIC,
                     shader::ImageType::INT_2D_ARRAY_ATOMIC) ?
                    VKImageViewArrayed::ARRAYED :
                    VKImageViewArrayed::NOT_ARRAYED;
    }
    else if (res.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER) {
      arrayed = ELEM(res.sampler.type,
                     shader::ImageType::FLOAT_1D_ARRAY,
                     shader::ImageType::FLOAT_2D_ARRAY,
                     shader::ImageType::FLOAT_CUBE_ARRAY,
                     shader::ImageType::INT_1D_ARRAY,
                     shader::ImageType::INT_2D_ARRAY,
                     shader::ImageType::INT_CUBE_ARRAY,
                     shader::ImageType::UINT_1D_ARRAY,
                     shader::ImageType::UINT_2D_ARRAY,
                     shader::ImageType::UINT_CUBE_ARRAY,
                     shader::ImageType::SHADOW_2D_ARRAY,
                     shader::ImageType::SHADOW_CUBE_ARRAY,
                     shader::ImageType::DEPTH_2D_ARRAY,
                     shader::ImageType::DEPTH_CUBE_ARRAY,
                     shader::ImageType::UINT_2D_ARRAY_ATOMIC,
                     shader::ImageType::INT_2D_ARRAY_ATOMIC) ?
                    VKImageViewArrayed::ARRAYED :
                    VKImageViewArrayed::NOT_ARRAYED;
    }
    descriptor_set_location_update(input, descriptor_set_location++, res.bind_type, res, arrayed);
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
                                   std::nullopt,
                                   VKImageViewArrayed::DONT_CARE);
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
    std::optional<const shader::ShaderCreateInfo::Resource> resource,
    VKImageViewArrayed arrayed)
{
  BLI_assert_msg(resource.has_value() ||
                     ELEM(bind_type,
                          shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER,
                          shader::ShaderCreateInfo::Resource::BindType::SAMPLER),
                 "Incorrect parameters, when no resource is given, it must be the uniform buffer "
                 "for storing push constants or sampler for subpasses.");
  BLI_assert_msg(!resource.has_value() || resource->bind_type == bind_type,
                 "Incorrect parameter, bind types do not match.");

  int32_t index = shader_input_index(inputs_, shader_input);
  BLI_assert(resource_bindings_[index].binding == -1);

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
    vk_access_flags |= VK_ACCESS_UNIFORM_READ_BIT;
  }
  else if (bind_type == shader::ShaderCreateInfo::Resource::BindType::SAMPLER) {
    vk_access_flags |= VK_ACCESS_SHADER_READ_BIT;
  }

  VKResourceBinding &resource_binding = resource_bindings_[index];
  resource_binding.bind_type = bind_type;
  resource_binding.binding = shader_input->binding;
  resource_binding.location = location;
  resource_binding.arrayed = arrayed;
  resource_binding.access_mask = vk_access_flags;
}

const VKResourceBinding &VKShaderInterface::resource_binding_info(
    const ShaderInput *shader_input) const
{
  int32_t index = shader_input_index(inputs_, shader_input);
  return resource_bindings_[index];
}

const VKDescriptorSet::Location VKShaderInterface::descriptor_set_location(
    const shader::ShaderCreateInfo::Resource &resource) const
{
  const ShaderInput *shader_input = shader_input_get(resource);
  BLI_assert(shader_input);
  return resource_binding_info(shader_input).location;
}

const std::optional<VKDescriptorSet::Location> VKShaderInterface::descriptor_set_location(
    const shader::ShaderCreateInfo::Resource::BindType &bind_type, int binding) const
{
  const ShaderInput *shader_input = shader_input_get(bind_type, binding);
  if (shader_input == nullptr) {
    return std::nullopt;
  }
  const VKResourceBinding &resource_binding = resource_binding_info(shader_input);
  if (resource_binding.bind_type != bind_type) {
    return std::nullopt;
  }
  return resource_binding.location;
}

const VkAccessFlags VKShaderInterface::access_mask(
    const shader::ShaderCreateInfo::Resource::BindType &bind_type, int binding) const
{
  const ShaderInput *shader_input = shader_input_get(bind_type, binding);
  if (shader_input == nullptr) {
    return VK_ACCESS_NONE;
  }
  const VKResourceBinding &resource_binding = resource_binding_info(shader_input);
  if (resource_binding.bind_type != bind_type) {
    return VK_ACCESS_NONE;
  }
  return resource_binding.access_mask;
}

const VKImageViewArrayed VKShaderInterface::arrayed(
    const shader::ShaderCreateInfo::Resource::BindType &bind_type, int binding) const
{
  const ShaderInput *shader_input = shader_input_get(bind_type, binding);
  if (shader_input == nullptr) {
    return VKImageViewArrayed::DONT_CARE;
  }
  return resource_binding_info(shader_input).arrayed;
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
       */
      return texture_get((binding >= BIND_SPACE_IMAGE_OFFSET) ? binding :
                                                                binding + BIND_SPACE_IMAGE_OFFSET);
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
  for (int index : IndexRange(info.subpass_inputs_.size())) {
    UNUSED_VARS(index);
    descriptor_set_layout_info_.bindings.append(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  }
  for (const shader::ShaderCreateInfo::Resource &res : all_resources) {
    descriptor_set_layout_info_.bindings.append(to_vk_descriptor_type(res));
  }
  if (push_constants_storage == VKPushConstants::StorageType::UNIFORM_BUFFER) {
    descriptor_set_layout_info_.bindings.append(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  }
}

}  // namespace blender::gpu
