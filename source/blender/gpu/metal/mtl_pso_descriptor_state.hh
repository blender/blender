/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#pragma once

#include "BLI_math_bits.h"
#include "GPU_batch.hh"
#include "GPU_vertex_format.hh"

#include <Metal/Metal.h>

#include "BLI_vector.hh"

#include "gpu_framebuffer_private.hh"
#include "gpu_shader_private.hh"

namespace blender::gpu {

/**
 * Vertex attribute and buffer descriptor wrappers
 * for use in PSO construction and caching.
 */
struct MTLVertexAttributeDescriptorPSO {
  ::MTLVertexFormat format;
  int offset;
  int buffer_index;
  GPUVertFetchMode format_conversion_mode;

  bool operator==(const MTLVertexAttributeDescriptorPSO &other) const
  {
    return (format == other.format) && (offset == other.offset) &&
           (buffer_index == other.buffer_index) &&
           (format_conversion_mode == other.format_conversion_mode);
  }

  uint64_t hash() const
  {
    return uint64_t((uint64_t(this->format) ^ (this->offset << 4) ^ (this->buffer_index << 8) ^
                     (this->format_conversion_mode << 12)));
  }

  void reset()
  {
    format = MTLVertexFormatInvalid;
    offset = 0;
    buffer_index = 0;
    format_conversion_mode = GPU_FETCH_FLOAT;
  }
};

struct MTLVertexBufferLayoutDescriptorPSO {
  ::MTLVertexStepFunction step_function;
  int step_rate;
  int stride;
  int buffer_slot;

  bool operator==(const MTLVertexBufferLayoutDescriptorPSO &other) const
  {
    return (step_function == other.step_function) && (step_rate == other.step_rate) &&
           (stride == other.stride) && (buffer_slot == other.buffer_slot);
  }

  uint64_t hash() const
  {
    return uint64_t(uint64_t(this->step_function) ^ (this->step_rate << 4) ^ (this->stride << 8) ^
                    (uint64_t(this->buffer_slot) << 32));
  }

  void reset()
  {
    step_function = MTLVertexStepFunctionPerVertex;
    step_rate = 1;
    stride = 0;
  }
};

struct MTLVertexDescriptor {
  /* Core Vertex Attributes. */
  MTLVertexAttributeDescriptorPSO attributes[GPU_VERT_ATTR_MAX_LEN];
  MTLVertexBufferLayoutDescriptorPSO buffer_layouts[GPU_BATCH_VBO_MAX_LEN];
  int max_attribute_value;
  int total_attributes;
  int num_vert_buffers;
  ::MTLPrimitiveTopologyClass prim_topology_class;

  bool operator==(const MTLVertexDescriptor &other) const
  {
    if ((this->max_attribute_value != other.max_attribute_value) ||
        (this->total_attributes != other.total_attributes) ||
        (this->num_vert_buffers != other.num_vert_buffers))
    {
      return false;
    }
    if (this->prim_topology_class != other.prim_topology_class) {
      return false;
    };

    for (const int a : IndexRange(this->max_attribute_value + 1)) {
      if (!(this->attributes[a] == other.attributes[a])) {
        return false;
      }
    }

    for (const int b : IndexRange(ARRAY_SIZE(this->buffer_layouts))) {
      if (!(this->buffer_layouts[b] == other.buffer_layouts[b])) {
        return false;
      }
    }

    /* NOTE: No need to compare SSBO attributes, as these will match attribute bindings for the
     * given shader. These are simply extra pre-resolved properties we want to include in the
     * cache. */
    return true;
  }

  uint64_t hash() const
  {
    uint64_t hash = (uint64_t)(this->max_attribute_value ^ this->num_vert_buffers);
    for (const int a : IndexRange(this->max_attribute_value + 1)) {
      hash ^= this->attributes[a].hash() << a;
    }

    for (const int b : IndexRange(this->num_vert_buffers)) {
      hash ^= this->buffer_layouts[b].hash() << (b + 10);
    }
    return hash;
  }
};

struct SpecializationStateDescriptor {
  Vector<shader::SpecializationConstant::Value> values;

  SpecializationStateDescriptor() = default;
  SpecializationStateDescriptor(Vector<shader::SpecializationConstant::Value> source)
      : values(source)
  {
  }

  bool operator==(const SpecializationStateDescriptor &other) const
  {
    return values == other.values;
  }

  uint64_t hash() const
  {
    uint64_t hash = values.size();
    uint seed = 0xFF;
    for (const shader::SpecializationConstant::Value &value : values) {
      seed = seed << 1;
      hash ^= seed ^ value.u;
    }
    return hash;
  }
};

/* Metal Render Pipeline State Descriptor -- All unique information which feeds PSO creation. */
struct MTLRenderPipelineStateDescriptor {
  /* This state descriptor will contain ALL parameters which generate a unique PSO.
   * We will then use this state-object to efficiently look-up or create a
   * new PSO for the current shader.
   *
   * Unlike the 'MTLContextGlobalShaderPipelineState', this struct contains a subset of
   * parameters used to distinguish between unique PSOs. This struct is hash-able and only
   * contains those parameters which are required by PSO generation. Non-unique state such as
   * bound resources is not tracked here, as it does not require a unique PSO permutation if
   * changed. */

  /* Input Vertex Descriptor. */
  MTLVertexDescriptor vertex_descriptor;

  /* Render Target attachment state.
   * Assign to #MTLPixelFormatInvalid if not used. */
  int num_color_attachments;
  MTLPixelFormat color_attachment_format[GPU_FB_MAX_COLOR_ATTACHMENT];
  MTLPixelFormat depth_attachment_format;
  MTLPixelFormat stencil_attachment_format;

  /* Render Pipeline State affecting PSO creation. */
  bool blending_enabled;
  MTLBlendOperation alpha_blend_op;
  MTLBlendOperation rgb_blend_op;
  MTLBlendFactor dest_alpha_blend_factor;
  MTLBlendFactor dest_rgb_blend_factor;
  MTLBlendFactor src_alpha_blend_factor;
  MTLBlendFactor src_rgb_blend_factor;

  /* Global color write mask as this cannot be specified per attachment. */
  MTLColorWriteMask color_write_mask;

  /* Clip distance enablement. */
  uchar clipping_plane_enable_mask = 0;

  /* Point size required by point primitives. */
  float point_size = 0.0f;

  /* Specialization constants map. */
  SpecializationStateDescriptor specialization_state;

  /* Comparison Operator for caching. */
  bool operator==(const MTLRenderPipelineStateDescriptor &other) const
  {
    if (!(vertex_descriptor == other.vertex_descriptor)) {
      return false;
    }

    if (clipping_plane_enable_mask != other.clipping_plane_enable_mask) {
      return false;
    }

    if ((num_color_attachments != other.num_color_attachments) ||
        (depth_attachment_format != other.depth_attachment_format) ||
        (stencil_attachment_format != other.stencil_attachment_format) ||
        (color_write_mask != other.color_write_mask) ||
        (blending_enabled != other.blending_enabled) || (alpha_blend_op != other.alpha_blend_op) ||
        (rgb_blend_op != other.rgb_blend_op) ||
        (dest_alpha_blend_factor != other.dest_alpha_blend_factor) ||
        (dest_rgb_blend_factor != other.dest_rgb_blend_factor) ||
        (src_alpha_blend_factor != other.src_alpha_blend_factor) ||
        (src_rgb_blend_factor != other.src_rgb_blend_factor) ||
        (vertex_descriptor.prim_topology_class != other.vertex_descriptor.prim_topology_class) ||
        (point_size != other.point_size))
    {
      return false;
    }

    /* Attachments can be skipped, so num_color_attachments will not define the range. */
    for (const int c : IndexRange(GPU_FB_MAX_COLOR_ATTACHMENT)) {
      if (color_attachment_format[c] != other.color_attachment_format[c]) {
        return false;
      }
    }

    if (!(specialization_state == other.specialization_state)) {
      return false;
    }

    return true;
  }

  uint64_t hash() const
  {
    /* NOTE(Metal): Current setup aims to minimize overlap of parameters
     * which are more likely to be different, to ensure earlier hash
     * differences without having to fallback to comparisons.
     * Though this could likely be further improved to remove
     * has collisions. */

    uint64_t hash = this->vertex_descriptor.hash();
    hash ^= uint64_t(this->num_color_attachments) << 16;     /* up to 6 (3 bits). */
    hash ^= uint64_t(this->depth_attachment_format) << 18;   /* up to 555 (9 bits). */
    hash ^= uint64_t(this->stencil_attachment_format) << 20; /* up to 555 (9 bits). */
    hash ^= uint64_t(
        *((uint64_t *)&this->vertex_descriptor.prim_topology_class)); /* Up to 3 (2 bits). */

    /* Only include elements in Hash if they are needed - avoids variable null assignments
     * influencing hash. */
    if (this->num_color_attachments > 0) {
      hash ^= uint64_t(this->color_write_mask) << 22;        /* 4 bit bit-mask. */
      hash ^= uint64_t(this->alpha_blend_op) << 26;          /* Up to 4 (3 bits). */
      hash ^= uint64_t(this->rgb_blend_op) << 29;            /* Up to 4 (3 bits). */
      hash ^= uint64_t(this->dest_alpha_blend_factor) << 32; /* Up to 18 (5 bits). */
      hash ^= uint64_t(this->dest_rgb_blend_factor) << 37;   /* Up to 18 (5 bits). */
      hash ^= uint64_t(this->src_alpha_blend_factor) << 42;  /* Up to 18 (5 bits). */
      hash ^= uint64_t(this->src_rgb_blend_factor) << 47;    /* Up to 18 (5 bits). */

      for (const uint c : IndexRange(GPU_FB_MAX_COLOR_ATTACHMENT)) {
        hash ^= uint64_t(this->color_attachment_format[c]) << (c + 52); /* Up to 555 (9 bits). */
      }
    }

    hash |= uint64_t((this->blending_enabled && (this->num_color_attachments > 0)) ? 1 : 0) << 62;
    hash ^= uint64_t(float_as_uint(this->point_size));

    /* Clipping plane enablement. */
    hash ^= uint64_t(clipping_plane_enable_mask) << 20;

    /* Specialization constants. We can treat the raw bytes as uint. */
    hash ^= specialization_state.hash();

    return hash;
  }

  /* Reset the Vertex Descriptor to default. */
  void reset_vertex_descriptor()
  {
    vertex_descriptor.total_attributes = 0;
    vertex_descriptor.max_attribute_value = 0;
    vertex_descriptor.num_vert_buffers = 0;
    vertex_descriptor.prim_topology_class = MTLPrimitiveTopologyClassUnspecified;
    for (int i = 0; i < GPU_VERT_ATTR_MAX_LEN; i++) {
      vertex_descriptor.attributes[i].reset();
    }
  }
};

/* Metal Compute Pipeline State Descriptor containing all unique information which feeds PSO
 * creation. */
struct MTLComputePipelineStateDescriptor {

  /* Specialization constants map. */
  SpecializationStateDescriptor specialization_state;

  MTLComputePipelineStateDescriptor() = default;
  MTLComputePipelineStateDescriptor(Vector<shader::SpecializationConstant::Value> values)
  {
    specialization_state.values = values;
  }

  /* Comparison Operator for caching. */
  bool operator==(const MTLComputePipelineStateDescriptor &other) const
  {
    return (specialization_state == other.specialization_state);
  }

  uint64_t hash() const
  {
    return specialization_state.hash();
  }
};

}  // namespace blender::gpu
