/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  layout.inner_row_padding = LayoutT::inner_row_padding(push_constant.type);

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
    const shader::ShaderCreateInfo &info, const VKDevice &device)
{
  if (info.push_constants_.is_empty()) {
    return StorageType::NONE;
  }

  uint32_t max_push_constants_size =
      device.physical_device_properties_get().limits.maxPushConstantsSize;
  uint32_t size = struct_size<Std430>(info.push_constants_);
  return size <= max_push_constants_size ? STORAGE_TYPE_DEFAULT : STORAGE_TYPE_FALLBACK;
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

void VKPushConstants::Layout::debug_print() const
{
  std::ostream &stream = std::cout;
  stream << "VKPushConstants::Layout::debug_print()\n";
  for (const PushConstant &push_constant : push_constants) {
    stream << "  - location:" << push_constant.location;
    stream << ", offset:" << push_constant.offset;
    stream << ", array_size:" << push_constant.array_size;
    stream << "\n";
  }
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

void VKPushConstants::update_uniform_buffer()
{
  BLI_assert(layout_->storage_type_get() == StorageType::UNIFORM_BUFFER);
  BLI_assert(data_ != nullptr);
  if (!uniform_buffer_) {
    uniform_buffer_ = std::make_unique<VKUniformBuffer>(layout_->size_in_bytes(),
                                                        "push constants buffer");
  }

  uniform_buffer_->reset_data_uploaded();
  uniform_buffer_->update(data_);
}

std::unique_ptr<VKUniformBuffer> &VKPushConstants::uniform_buffer_get()
{
  BLI_assert(layout_->storage_type_get() == StorageType::UNIFORM_BUFFER);
  BLI_assert(uniform_buffer_);
  return uniform_buffer_;
}

}  // namespace blender::gpu
