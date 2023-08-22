/* SPDX-FileCopyrightText: 2023 Blender Authors All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "vk_vertex_attribute_object.hh"

#include "vk_batch.hh"
#include "vk_context.hh"
#include "vk_immediate.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_vertex_buffer.hh"

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"

namespace blender::gpu {
VKVertexAttributeObject::VKVertexAttributeObject()
{
  clear();
}

void VKVertexAttributeObject::clear()
{
  is_valid = false;
  info.pNext = nullptr;
  bindings.clear();
  attributes.clear();
  vbos.clear();
  buffers.clear();
}

VKVertexAttributeObject &VKVertexAttributeObject::operator=(const VKVertexAttributeObject &other)
{
  if (this == &other) {
    return *this;
  }

  is_valid = other.is_valid;
  info = other.info;
  bindings.clear();
  bindings.extend(other.bindings);
  attributes.clear();
  attributes.extend(other.attributes);
  vbos.clear();
  vbos.extend(other.vbos);
  buffers.clear();
  buffers.extend(other.buffers);
  return *this;
}

/* -------------------------------------------------------------------- */
/** \name Bind resources
 * \{ */

void VKVertexAttributeObject::bind(VKContext &context)
{
  const bool use_vbos = !vbos.is_empty();
  if (use_vbos) {
    bind_vbos(context);
  }
  else {
    bind_buffers(context);
  }
}

void VKVertexAttributeObject::bind_vbos(VKContext &context)
{
  /* Bind VBOS from batches. */
  Array<bool> visited_bindings(bindings.size());
  visited_bindings.fill(false);

  for (VkVertexInputAttributeDescription attribute : attributes) {
    if (visited_bindings[attribute.binding]) {
      continue;
    }
    visited_bindings[attribute.binding] = true;

    if (attribute.binding < vbos.size()) {
      BLI_assert(vbos[attribute.binding]);
      VKVertexBuffer &vbo = *vbos[attribute.binding];
      vbo.upload();
      context.command_buffer_get().bind(attribute.binding, vbo, 0);
    }
    else {
      const VKBuffer &buffer = VKBackend::get().device_get().dummy_buffer_get();
      const VKBufferWithOffset buffer_with_offset = {buffer, 0};
      context.command_buffer_get().bind(attribute.binding, buffer_with_offset);
    }
  }
}

void VKVertexAttributeObject::bind_buffers(VKContext &context)
{
  /* Bind dynamic buffers from immediate mode. */
  Array<bool> visited_bindings(bindings.size());
  visited_bindings.fill(false);

  for (VkVertexInputAttributeDescription attribute : attributes) {
    if (visited_bindings[attribute.binding]) {
      continue;
    }
    visited_bindings[attribute.binding] = true;

    if (attribute.binding < buffers.size()) {
      VKBufferWithOffset &buffer = buffers[attribute.binding];
      context.command_buffer_get().bind(attribute.binding, buffer);
    }
    else {
      const VKBuffer &buffer = VKBackend::get().device_get().dummy_buffer_get();
      const VKBufferWithOffset buffer_with_offset = {buffer, 0};
      context.command_buffer_get().bind(attribute.binding, buffer_with_offset);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update bindings
 * \{ */

void VKVertexAttributeObject::update_bindings(const VKContext &context, VKBatch &batch)
{
  clear();
  const VKShaderInterface &interface = unwrap(context.shader)->interface_get();
  AttributeMask occupied_attributes = 0;

  for (int v = 0; v < GPU_BATCH_INST_VBO_MAX_LEN; v++) {
    VKVertexBuffer *vbo = batch.instance_buffer_get(v);
    if (vbo) {
      update_bindings(
          vbo->format, vbo, nullptr, vbo->vertex_len, interface, occupied_attributes, true);
    }
  }
  for (int v = 0; v < GPU_BATCH_VBO_MAX_LEN; v++) {
    VKVertexBuffer *vbo = batch.vertex_buffer_get(v);
    if (vbo) {
      update_bindings(
          vbo->format, vbo, nullptr, vbo->vertex_len, interface, occupied_attributes, false);
    }
  }

  if (occupied_attributes != interface.enabled_attr_mask_) {
    fill_unused_bindings(interface, occupied_attributes);
  }
  is_valid = true;
}

/* Determine the number of binding location the given attribute uses. */
static uint32_t to_binding_location_len(const GPUVertAttr &attribute)
{
  return ceil_division(attribute.comp_len, 4u);
}

/* Determine the number of binding location the given type uses. */
static uint32_t to_binding_location_len(const shader::Type type)
{
  switch (type) {
    case shader::Type::FLOAT:
    case shader::Type::VEC2:
    case shader::Type::VEC3:
    case shader::Type::VEC4:
    case shader::Type::UINT:
    case shader::Type::UVEC2:
    case shader::Type::UVEC3:
    case shader::Type::UVEC4:
    case shader::Type::INT:
    case shader::Type::IVEC2:
    case shader::Type::IVEC3:
    case shader::Type::IVEC4:
    case shader::Type::BOOL:
    case shader::Type::VEC3_101010I2:
    case shader::Type::UCHAR:
    case shader::Type::UCHAR2:
    case shader::Type::UCHAR3:
    case shader::Type::UCHAR4:
    case shader::Type::CHAR:
    case shader::Type::CHAR2:
    case shader::Type::CHAR3:
    case shader::Type::CHAR4:
      return 1;
    case shader::Type::MAT3:
      return 3;
    case shader::Type::MAT4:
      return 4;
  }

  return 1;
}

void VKVertexAttributeObject::fill_unused_bindings(const VKShaderInterface &interface,
                                                   const AttributeMask occupied_attributes)
{
  for (int location : IndexRange(16)) {
    AttributeMask location_mask = 1 << location;
    /* Skip occupied slots */
    if (occupied_attributes & location_mask) {
      continue;
    }
    /* Skip slots that are not used by the vertex shader. */
    if ((interface.enabled_attr_mask_ & location_mask) == 0) {
      continue;
    }

    /* Use dummy binding...*/
    shader::Type attribute_type = interface.get_attribute_type(location);
    const uint32_t num_locations = to_binding_location_len(attribute_type);
    for (const uint32_t location_offset : IndexRange(num_locations)) {
      const uint32_t binding = bindings.size();
      VkVertexInputAttributeDescription attribute_description = {};
      attribute_description.binding = binding;
      attribute_description.location = location + location_offset;
      attribute_description.offset = 0;
      attribute_description.format = to_vk_format(attribute_type);
      attributes.append(attribute_description);

      VkVertexInputBindingDescription vk_binding_descriptor = {};
      vk_binding_descriptor.binding = binding;
      vk_binding_descriptor.stride = 0;
      vk_binding_descriptor.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
      bindings.append(vk_binding_descriptor);
    }
  }
}

void VKVertexAttributeObject::update_bindings(VKImmediate &immediate)
{
  clear();
  const VKShaderInterface &interface = unwrap(unwrap(immediate.shader))->interface_get();
  AttributeMask occupied_attributes = 0;

  VKBufferWithOffset immediate_buffer = {*immediate.active_resource(),
                                         immediate.subbuffer_offset_get()};

  update_bindings(immediate.vertex_format,
                  nullptr,
                  &immediate_buffer,
                  immediate.vertex_len,
                  interface,
                  occupied_attributes,
                  false);
  is_valid = true;
  BLI_assert(interface.enabled_attr_mask_ == occupied_attributes);
}

void VKVertexAttributeObject::update_bindings(const GPUVertFormat &vertex_format,
                                              VKVertexBuffer *vertex_buffer,
                                              VKBufferWithOffset *immediate_vertex_buffer,
                                              const int64_t vertex_len,
                                              const VKShaderInterface &interface,
                                              AttributeMask &r_occupied_attributes,
                                              const bool use_instancing)
{
  BLI_assert(vertex_buffer || immediate_vertex_buffer);
  BLI_assert(!(vertex_buffer && immediate_vertex_buffer));

  if (vertex_format.attr_len <= 0) {
    return;
  }

  uint32_t offset = 0;
  uint32_t stride = vertex_format.stride;

  for (uint32_t attribute_index = 0; attribute_index < vertex_format.attr_len; attribute_index++) {
    const GPUVertAttr &attribute = vertex_format.attrs[attribute_index];
    if (vertex_format.deinterleaved) {
      offset += ((attribute_index == 0) ? 0 : vertex_format.attrs[attribute_index - 1].size) *
                vertex_len;
      stride = attribute.size;
    }
    else {
      offset = attribute.offset;
    }

    for (uint32_t name_index = 0; name_index < attribute.name_len; name_index++) {
      const char *name = GPU_vertformat_attr_name_get(&vertex_format, &attribute, name_index);
      const ShaderInput *shader_input = interface.attr_get(name);
      if (shader_input == nullptr || shader_input->location == -1) {
        continue;
      }

      /* Don't overwrite attributes that are already occupied. */
      AttributeMask attribute_mask = 1 << shader_input->location;
      if (r_occupied_attributes & attribute_mask) {
        continue;
      }
      r_occupied_attributes |= attribute_mask;
      const uint32_t num_locations = to_binding_location_len(attribute);
      for (const uint32_t location_offset : IndexRange(num_locations)) {
        const uint32_t binding = bindings.size();
        VkVertexInputAttributeDescription attribute_description = {};
        attribute_description.binding = binding;
        attribute_description.location = shader_input->location + location_offset;
        attribute_description.offset = offset + location_offset * sizeof(float4);
        attribute_description.format = to_vk_format(
            static_cast<GPUVertCompType>(attribute.comp_type),
            attribute.size,
            static_cast<GPUVertFetchMode>(attribute.fetch_mode));
        attributes.append(attribute_description);

        VkVertexInputBindingDescription vk_binding_descriptor = {};
        vk_binding_descriptor.binding = binding;
        vk_binding_descriptor.stride = stride;
        vk_binding_descriptor.inputRate = use_instancing ? VK_VERTEX_INPUT_RATE_INSTANCE :
                                                           VK_VERTEX_INPUT_RATE_VERTEX;
        bindings.append(vk_binding_descriptor);
        if (vertex_buffer) {
          vbos.append(vertex_buffer);
        }
        if (immediate_vertex_buffer) {
          buffers.append(*immediate_vertex_buffer);
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debugging
 * \{ */

void VKVertexAttributeObject::debug_print() const
{
  std::cout << __FILE__ << "::" << __func__ << "\n";
  Array<bool> visited_bindings(bindings.size());
  visited_bindings.fill(false);

  for (VkVertexInputAttributeDescription attribute : attributes) {
    std::cout << " - attribute(binding=" << attribute.binding
              << ", location=" << attribute.location << ")";

    if (visited_bindings[attribute.binding]) {
      std::cout << " WARNING: Already bound\n";
      continue;
    }
    visited_bindings[attribute.binding] = true;

    /* Bind VBOS from batches. */
    if (!vbos.is_empty()) {
      if (attribute.binding < vbos.size()) {
        std::cout << " Attach to VBO [" << vbos[attribute.binding] << "]\n";
      }
      else {
        std::cout << " WARNING: Attach to dummy\n";
      }
    }
    else if (!buffers.is_empty()) {
      if (attribute.binding < vbos.size()) {
        std::cout << " Attach to ImmediateModeVBO\n";
      }
      else {
        std::cout << " WARNING: Attach to dummy\n";
      }
    }
  }
}

/** \} */

}  // namespace blender::gpu
