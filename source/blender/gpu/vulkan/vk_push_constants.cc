/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. */

/** \file
 * \ingroup gpu
 */

#include "vk_push_constants.hh"
#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_memory_layout.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_storage_buffer.hh"
#include "vk_uniform_buffer.hh"

namespace blender::gpu {

template<typename LayoutT>
static VKPushConstants::Layout::PushConstant init_constant(
    const shader::ShaderCreateInfo::PushConst &push_constant,
    const ShaderInput &shader_input,
    uint32_t *r_offset)
{
  align<LayoutT>(push_constant.type, push_constant.array_size, r_offset);

  VKPushConstants::Layout::PushConstant layout;
  layout.location = shader_input.location;
  layout.type = push_constant.type;
  layout.array_size = push_constant.array_size;
  layout.offset = *r_offset;

  reserve<LayoutT>(push_constant.type, push_constant.array_size, r_offset);
  return layout;
}

template<typename LayoutT>
uint32_t struct_size(Span<shader::ShaderCreateInfo::PushConst> push_constants)
{
  uint32_t offset = 0;
  for (const shader::ShaderCreateInfo::PushConst &push_constant : push_constants) {
    align<LayoutT>(push_constant.type, push_constant.array_size, &offset);
    reserve<LayoutT>(push_constant.type, push_constant.array_size, &offset);
  }

  align_end_of_struct<LayoutT>(&offset);
  return offset;
}

VKPushConstants::StorageType VKPushConstants::Layout::determine_storage_type(
    const shader::ShaderCreateInfo &info, const VkPhysicalDeviceLimits &vk_physical_device_limits)
{
  if (info.push_constants_.is_empty()) {
    return StorageType::NONE;
  }

  uint32_t size = struct_size<Std430>(info.push_constants_);
  return size <= vk_physical_device_limits.maxPushConstantsSize ? STORAGE_TYPE_DEFAULT :
                                                                  STORAGE_TYPE_FALLBACK;
}

template<typename LayoutT>
void init_struct(const shader::ShaderCreateInfo &info,
                 const VKShaderInterface &interface,
                 Vector<VKPushConstants::Layout::PushConstant> &r_struct,
                 uint32_t *r_offset)
{
  for (const shader::ShaderCreateInfo::PushConst &push_constant : info.push_constants_) {
    const ShaderInput *shader_input = interface.uniform_get(push_constant.name.c_str());
    r_struct.append(init_constant<LayoutT>(push_constant, *shader_input, r_offset));
  }
  align_end_of_struct<Std140>(r_offset);
}

void VKPushConstants::Layout::init(const shader::ShaderCreateInfo &info,
                                   const VKShaderInterface &interface,
                                   const StorageType storage_type,
                                   const VKDescriptorSet::Location location)
{
  BLI_assert(push_constants.is_empty());
  storage_type_ = storage_type;

  size_in_bytes_ = 0;
  if (storage_type == StorageType::UNIFORM_BUFFER) {
    descriptor_set_location_ = location;
    init_struct<Std140>(info, interface, push_constants, &size_in_bytes_);
  }
  else {
    init_struct<Std430>(info, interface, push_constants, &size_in_bytes_);
  }
}

const VKPushConstants::Layout::PushConstant *VKPushConstants::Layout::find(int32_t location) const
{
  for (const PushConstant &push_constant : push_constants) {
    if (push_constant.location == location) {
      return &push_constant;
    }
  }
  return nullptr;
}

VKPushConstants::VKPushConstants() = default;
VKPushConstants::VKPushConstants(const Layout *layout) : layout_(layout)
{
  data_ = MEM_mallocN(layout->size_in_bytes(), __func__);
}

VKPushConstants::VKPushConstants(VKPushConstants &&other) : layout_(other.layout_)
{
  data_ = other.data_;
  other.data_ = nullptr;
}

VKPushConstants::~VKPushConstants()
{
  if (data_ != nullptr) {
    MEM_freeN(data_);
    data_ = nullptr;
  }
}

VKPushConstants &VKPushConstants::operator=(VKPushConstants &&other)
{
  layout_ = other.layout_;

  data_ = other.data_;
  other.data_ = nullptr;

  return *this;
}

void VKPushConstants::update(VKContext &context)
{
  VKShader *shader = static_cast<VKShader *>(context.shader);
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  VKPipeline &pipeline = shader->pipeline_get();
  BLI_assert_msg(&pipeline.push_constants_get() == this,
                 "Invalid state detected. Push constants doesn't belong to the active shader of "
                 "the given context.");
  VKDescriptorSetTracker &descriptor_set = pipeline.descriptor_set_get();

  switch (layout_get().storage_type_get()) {
    case VKPushConstants::StorageType::NONE:
      break;

    case VKPushConstants::StorageType::PUSH_CONSTANTS:
      command_buffer.push_constants(*this, shader->vk_pipeline_layout_get(), VK_SHADER_STAGE_ALL);
      break;

    case VKPushConstants::StorageType::UNIFORM_BUFFER:
      update_uniform_buffer();
      descriptor_set.bind(*uniform_buffer_get(), layout_get().descriptor_set_location_get());
      break;
  }
}

void VKPushConstants::update_uniform_buffer()
{
  BLI_assert(layout_->storage_type_get() == StorageType::UNIFORM_BUFFER);
  BLI_assert(data_ != nullptr);
  VKContext &context = *VKContext::get();
  std::unique_ptr<VKUniformBuffer> &uniform_buffer = tracked_resource_for(context, is_dirty_);
  uniform_buffer->update(data_);
  is_dirty_ = false;
}

std::unique_ptr<VKUniformBuffer> &VKPushConstants::uniform_buffer_get()
{
  BLI_assert(layout_->storage_type_get() == StorageType::UNIFORM_BUFFER);
  return active_resource();
}

std::unique_ptr<VKUniformBuffer> VKPushConstants::create_resource(VKContext & /*context*/)
{
  return std::make_unique<VKUniformBuffer>(layout_->size_in_bytes(), __func__);
}

}  // namespace blender::gpu
