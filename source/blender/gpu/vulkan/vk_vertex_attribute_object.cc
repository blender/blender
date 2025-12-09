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

#include "BLI_bit_vector.hh"
#include "BLI_math_vector_types.hh"

namespace blender::gpu {

void VKVertexInputDescription::clear()
{
  bindings.clear();
  attributes.clear();
}

VKVertexAttributeObject::VKVertexAttributeObject()
{
  clear();
}

void VKVertexAttributeObject::clear()
{
  vertex_input.clear();
  vbos.clear();
  buffers.clear();
}

VKVertexAttributeObject &VKVertexAttributeObject::operator=(const VKVertexAttributeObject &other)
{
  if (this == &other) {
    return *this;
  }

  vertex_input = other.vertex_input;

  vbos.clear();
  vbos.extend(other.vbos);
  buffers.clear();
  buffers.extend(other.buffers);
  return *this;
}

/* -------------------------------------------------------------------- */
/** \name Bind resources
 * \{ */

void VKVertexAttributeObject::bind(
    render_graph::VKVertexBufferBindings &r_vertex_buffer_bindings) const
{
  BitVector visited_bindings(vertex_input.bindings.size());

  const VKBuffer &dummy = VKBackend::get().device.dummy_buffer;
  for (VkVertexInputAttributeDescription2EXT attribute : vertex_input.attributes) {
    if (visited_bindings[attribute.binding]) {
      continue;
    }
    visited_bindings[attribute.binding].set(true);

    VkBuffer buffer = dummy.vk_handle();
    VkDeviceSize offset = 0;

    if (attribute.binding < buffers.size()) {
      buffer = buffers[attribute.binding].buffer;
      offset = buffers[attribute.binding].offset;
    }

    r_vertex_buffer_bindings.buffer[attribute.binding] = buffer;
    r_vertex_buffer_bindings.offset[attribute.binding] = offset;
    r_vertex_buffer_bindings.buffer_count = max_ii(r_vertex_buffer_bindings.buffer_count,
                                                   attribute.binding + 1);
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

  for (int v = 0; v < GPU_BATCH_VBO_MAX_LEN; v++) {
    VKVertexBuffer *vbo = batch.vertex_buffer_get(v);
    if (vbo) {
      update_bindings(vbo->format, vbo, nullptr, vbo->vertex_len, interface, occupied_attributes);
    }
  }

  if (occupied_attributes != interface.enabled_attr_mask_) {
    fill_unused_bindings(interface, occupied_attributes);
  }
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

    /* Use dummy binding. */
    shader::Type attribute_type = interface.get_attribute_type(location);
    const uint32_t binding = vertex_input.bindings.size();
    VkVertexInputAttributeDescription2EXT attribute_description = {
        VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT, nullptr};
    attribute_description.binding = binding;
    attribute_description.location = location;
    attribute_description.offset = 0;
    attribute_description.format = to_vk_format(attribute_type);
    vertex_input.attributes.append(attribute_description);

    VkVertexInputBindingDescription2EXT vk_binding_descriptor = {
        VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT, nullptr};
    vk_binding_descriptor.binding = binding;
    vk_binding_descriptor.stride = 0;
    vk_binding_descriptor.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vk_binding_descriptor.divisor = 1;
    vertex_input.bindings.append(vk_binding_descriptor);
  }
}

void VKVertexAttributeObject::update_bindings(VKImmediate &immediate)
{
  clear();
  const VKShaderInterface &interface = unwrap(unwrap(immediate.shader))->interface_get();
  AttributeMask occupied_attributes = 0;

  VKBufferWithOffset immediate_buffer = immediate.active_buffer();
  update_bindings(immediate.vertex_format,
                  nullptr,
                  &immediate_buffer,
                  immediate.vertex_len,
                  interface,
                  occupied_attributes);
  BLI_assert(interface.enabled_attr_mask_ == occupied_attributes);
}

void VKVertexAttributeObject::update_bindings(const GPUVertFormat &vertex_format,
                                              VKVertexBuffer *vertex_buffer,
                                              VKBufferWithOffset *immediate_vertex_buffer,
                                              const int64_t vertex_len,
                                              const VKShaderInterface &interface,
                                              AttributeMask &r_occupied_attributes)
{
  BLI_assert(vertex_buffer || immediate_vertex_buffer);
  BLI_assert(!(vertex_buffer && immediate_vertex_buffer));

  if (vertex_format.attr_len <= 0) {
    return;
  }

  /* Interleaved offset is added to the buffer binding. Attribute offsets are hardware
   * restricted (ref: VUID-VkVertexInputAttributeDescription-offset-00622). */
  uint32_t buffer_offset = 0;
  uint32_t attribute_offset = 0;
  uint32_t stride = vertex_format.stride;

  bool add_vbo = false;

  for (uint32_t attribute_index = 0; attribute_index < vertex_format.attr_len; attribute_index++) {
    const GPUVertAttr &attribute = vertex_format.attrs[attribute_index];
    if (vertex_format.deinterleaved) {
      buffer_offset += ((attribute_index == 0) ?
                            0 :
                            vertex_format.attrs[attribute_index - 1].type.size()) *
                       vertex_len;
      stride = attribute.type.size();
    }
    else {
      attribute_offset = attribute.offset;
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
      const uint32_t binding = vertex_input.bindings.size();
      VkVertexInputAttributeDescription2EXT attribute_description = {
          VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT, nullptr};
      attribute_description.binding = binding;
      attribute_description.location = shader_input->location;
      attribute_description.offset = attribute_offset;
      attribute_description.format = to_vk_format(
          attribute.type.comp_type(), attribute.type.size(), attribute.type.fetch_mode());
      vertex_input.attributes.append(attribute_description);

      VkVertexInputBindingDescription2EXT vk_binding_descriptor = {
          VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT, nullptr};
      vk_binding_descriptor.binding = binding;
      vk_binding_descriptor.stride = stride;
      vk_binding_descriptor.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
      vk_binding_descriptor.divisor = 1;
      vertex_input.bindings.append(vk_binding_descriptor);
      if (vertex_buffer) {
        add_vbo = true;
        vertex_buffer->upload();
        buffers.append({vertex_buffer->vk_handle(), buffer_offset});
      }
      if (immediate_vertex_buffer) {
        buffers.append(*immediate_vertex_buffer);
      }
    }
  }

  if (add_vbo) {
    BLI_assert(vertex_buffer != nullptr);
    vbos.append(vertex_buffer);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debugging
 * \{ */

void VKVertexAttributeObject::debug_print() const
{
  std::cout << __FILE__ << "::" << __func__ << "\n";
  BitVector visited_bindings(vertex_input.bindings.size());

  for (VkVertexInputAttributeDescription2EXT attribute : vertex_input.attributes) {
    std::cout << " - attribute(binding=" << attribute.binding
              << ", location=" << attribute.location << ")";

    if (visited_bindings[attribute.binding]) {
      std::cout << " WARNING: Already bound\n";
      continue;
    }
    visited_bindings[attribute.binding].set(true);

    if (attribute.binding < vbos.size()) {
      std::cout << " Attach to Buffer\n";
    }
    else {
      std::cout << " WARNING: Attach to dummy\n";
    }
  }
}

/** \} */

}  // namespace blender::gpu
