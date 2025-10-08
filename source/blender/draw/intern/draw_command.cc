/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "GPU_batch.hh"
#include "GPU_capabilities.hh"
#include "GPU_compute.hh"
#include "GPU_debug.hh"

#include "draw_command.hh"
#include "draw_pass.hh"
#include "draw_shader.hh"
#include "draw_view.hh"

#include <bitset>
#include <sstream>

namespace blender::draw::command {

static gpu::Batch *procedural_batch_get(GPUPrimType primitive)
{
  switch (primitive) {
    case GPU_PRIM_POINTS:
      return GPU_batch_procedural_points_get();
    case GPU_PRIM_LINES:
      return GPU_batch_procedural_lines_get();
    case GPU_PRIM_TRIS:
      return GPU_batch_procedural_triangles_get();
    case GPU_PRIM_TRI_STRIP:
      return GPU_batch_procedural_triangle_strips_get();
    default:
      /* Add new one as needed. */
      BLI_assert_unreachable();
      return nullptr;
  }
}

/* -------------------------------------------------------------------- */
/** \name Commands Execution
 * \{ */

void ShaderBind::execute(RecordingState &state) const
{
  state.shader_use_specialization = !GPU_shader_get_default_constant_state(shader).is_empty();
  if (assign_if_different(state.shader, shader) || state.shader_use_specialization) {
    GPU_shader_bind(shader, state.specialization_constants_get());
  }
  /* Signal that we can reload the default for a different specialization later on.
   * However, we keep the specialization_constants state around for compute shaders. */
  state.specialization_constants_in_use = false;
}

void FramebufferBind::execute() const
{
  GPU_framebuffer_bind(*framebuffer);
}

void SubPassTransition::execute() const
{
  /* TODO(fclem): Require framebuffer bind to always be part of the pass so that we can track it
   * inside RecordingState. */
  gpu::FrameBuffer *framebuffer = GPU_framebuffer_active_get();
  /* Unpack to the real enum type. */
  const GPUAttachmentState states[9] = {
      GPUAttachmentState(depth_state),
      GPUAttachmentState(color_states[0]),
      GPUAttachmentState(color_states[1]),
      GPUAttachmentState(color_states[2]),
      GPUAttachmentState(color_states[3]),
      GPUAttachmentState(color_states[4]),
      GPUAttachmentState(color_states[5]),
      GPUAttachmentState(color_states[6]),
      GPUAttachmentState(color_states[7]),
  };
  GPU_framebuffer_subpass_transition_array(framebuffer, states, ARRAY_SIZE(states));
}

void ResourceBind::execute() const
{
  if (slot == -1) {
    return;
  }
  switch (type) {
    case ResourceBind::Type::Sampler:
      GPU_texture_bind_ex(is_reference ? *texture_ref : texture, sampler, slot);
      break;
    case ResourceBind::Type::BufferSampler:
      GPU_vertbuf_bind_as_texture(is_reference ? *vertex_buf_ref : vertex_buf, slot);
      break;
    case ResourceBind::Type::Image:
      GPU_texture_image_bind(is_reference ? *texture_ref : texture, slot);
      break;
    case ResourceBind::Type::UniformBuf:
      GPU_uniformbuf_bind(is_reference ? *uniform_buf_ref : uniform_buf, slot);
      break;
    case ResourceBind::Type::StorageBuf:
      GPU_storagebuf_bind(is_reference ? *storage_buf_ref : storage_buf, slot);
      break;
    case ResourceBind::Type::UniformAsStorageBuf:
      GPU_uniformbuf_bind_as_ssbo(is_reference ? *uniform_buf_ref : uniform_buf, slot);
      break;
    case ResourceBind::Type::VertexAsStorageBuf:
      GPU_vertbuf_bind_as_ssbo(is_reference ? *vertex_buf_ref : vertex_buf, slot);
      break;
    case ResourceBind::Type::IndexAsStorageBuf:
      GPU_indexbuf_bind_as_ssbo(is_reference ? *index_buf_ref : index_buf, slot);
      break;
  }
}

void PushConstant::execute(RecordingState &state) const
{
  if (location == -1) {
    return;
  }
  switch (type) {
    case PushConstant::Type::IntValue:
      GPU_shader_uniform_int_ex(state.shader, location, comp_len, array_len, int4_value);
      break;
    case PushConstant::Type::IntReference:
      GPU_shader_uniform_int_ex(state.shader, location, comp_len, array_len, int_ref);
      break;
    case PushConstant::Type::FloatValue:
      GPU_shader_uniform_float_ex(state.shader, location, comp_len, array_len, float4_value);
      break;
    case PushConstant::Type::FloatReference:
      GPU_shader_uniform_float_ex(state.shader, location, comp_len, array_len, float_ref);
      break;
  }
}

void SpecializeConstant::execute(command::RecordingState &state) const
{
  /* All specialization constants should exist as they are not optimized out like uniforms. */
  BLI_assert(location != -1);

  if (state.specialization_constants_in_use == false) {
    state.specialization_constants = GPU_shader_get_default_constant_state(this->shader);
    state.specialization_constants_in_use = true;
  }

  switch (type) {
    case SpecializeConstant::Type::IntValue:
      state.specialization_constants.set_value(location, int_value);
      break;
    case SpecializeConstant::Type::IntReference:
      state.specialization_constants.set_value(location, *int_ref);
      break;
    case SpecializeConstant::Type::UintValue:
      state.specialization_constants.set_value(location, uint_value);
      break;
    case SpecializeConstant::Type::UintReference:
      state.specialization_constants.set_value(location, *uint_ref);
      break;
    case SpecializeConstant::Type::FloatValue:
      state.specialization_constants.set_value(location, float_value);
      break;
    case SpecializeConstant::Type::FloatReference:
      state.specialization_constants.set_value(location, *float_ref);
      break;
    case SpecializeConstant::Type::BoolValue:
      state.specialization_constants.set_value(location, bool_value);
      break;
    case SpecializeConstant::Type::BoolReference:
      state.specialization_constants.set_value(location, *bool_ref);
      break;
  }
}

void Draw::execute(RecordingState &state) const
{
  state.front_facing_set(res_index.has_inverted_handedness());

  /* Use same logic as in `finalize_commands`. */
  uint instance_first = 0;
  if (res_index.raw > 0) {
    instance_first = state.instance_offset;
    state.instance_offset += instance_len;
  }

  GPU_shader_get_default_constant_state(state.shader).is_empty();

  if (is_primitive_expansion()) {
    /* Expanded draw-call. */
    IndexRange expanded_range = GPU_batch_draw_expanded_parameter_get(
        batch->prim_type,
        GPUPrimType(expand_prim_type),
        vertex_len,
        vertex_first,
        expand_prim_len);

    if (expanded_range.is_empty()) {
      /* Nothing to draw, and can lead to asserts in GPU_batch_bind_as_resources. */
      return;
    }

    GPU_batch_bind_as_resources(batch, state.shader, state.specialization_constants_get());

    gpu::Batch *gpu_batch = procedural_batch_get(GPUPrimType(expand_prim_type));
    GPU_batch_set_shader(gpu_batch, state.shader, state.specialization_constants_get());
    GPU_batch_draw_advanced(
        gpu_batch, expanded_range.start(), expanded_range.size(), instance_first, instance_len);
  }
  else {
    /* Regular draw-call. */
    GPU_batch_set_shader(batch, state.shader, state.specialization_constants_get());
    GPU_batch_draw_advanced(batch, vertex_first, vertex_len, instance_first, instance_len);
  }
}

void DrawMulti::execute(RecordingState &state) const
{
  DrawMultiBuf::DrawCommandBuf &indirect_buf = multi_draw_buf->command_buf_;
  DrawMultiBuf::DrawGroupBuf &groups = multi_draw_buf->group_buf_;

  uint group_index = this->group_first;
  while (group_index != uint(-1)) {
    const DrawGroup &group = groups[group_index];

    if (group.vertex_len > 0) {
      gpu::Batch *batch = group.desc.gpu_batch;

      if (GPUPrimType(group.desc.expand_prim_type) != GPU_PRIM_NONE) {
        /* Bind original batch as resource and use a procedural batch to issue the draw-call. */
        GPU_batch_bind_as_resources(
            group.desc.gpu_batch, state.shader, state.specialization_constants_get());
        batch = procedural_batch_get(GPUPrimType(group.desc.expand_prim_type));
      }

      GPU_batch_set_shader(batch, state.shader, state.specialization_constants_get());

      constexpr intptr_t stride = sizeof(DrawCommand);
      /* We have 2 indirect command reserved per draw group. */
      intptr_t offset = stride * group_index * 2;

      /* Draw negatively scaled geometry first. */
      if (group.len - group.front_facing_len > 0) {
        state.front_facing_set(true);
        GPU_batch_draw_indirect(batch, indirect_buf, offset);
      }

      if (group.front_facing_len > 0) {
        state.front_facing_set(false);
        GPU_batch_draw_indirect(batch, indirect_buf, offset + stride);
      }
    }

    group_index = group.next;
  }
}

void DrawIndirect::execute(RecordingState &state) const
{
  state.front_facing_set(res_index.has_inverted_handedness());

  GPU_batch_draw_indirect(batch, *indirect_buf, 0);
}

void Dispatch::execute(RecordingState &state) const
{
  if (is_reference) {
    GPU_compute_dispatch(
        state.shader, size_ref->x, size_ref->y, size_ref->z, state.specialization_constants_get());
  }
  else {
    GPU_compute_dispatch(
        state.shader, size.x, size.y, size.z, state.specialization_constants_get());
  }
}

void DispatchIndirect::execute(RecordingState &state) const
{
  GPU_compute_dispatch_indirect(state.shader, *indirect_buf, state.specialization_constants_get());
}

void Barrier::execute() const
{
  GPU_memory_barrier(type);
}

void Clear::execute() const
{
  gpu::FrameBuffer *fb = GPU_framebuffer_active_get();
  GPU_framebuffer_clear(fb, (GPUFrameBufferBits)clear_channels, color, depth, stencil);
}

void ClearMulti::execute() const
{
  gpu::FrameBuffer *fb = GPU_framebuffer_active_get();
  GPU_framebuffer_multi_clear(fb, (const float (*)[4])colors);
}

void StateSet::execute(RecordingState &recording_state) const
{
  bool state_changed = assign_if_different(recording_state.pipeline_state, new_state);
  bool clip_changed = assign_if_different(recording_state.clip_plane_count, clip_plane_count);

  if (!state_changed && !clip_changed) {
    return;
  }

  GPU_state_set(to_write_mask(new_state),
                to_blend(new_state),
                to_face_cull_test(new_state),
                to_depth_test(new_state),
                to_stencil_test(new_state),
                to_stencil_op(new_state),
                to_provoking_vertex(new_state));

  if (new_state & DRW_STATE_CLIP_CONTROL_UNIT_RANGE) {
    GPU_clip_control_unit_range(true);
  }
  else {
    GPU_clip_control_unit_range(false);
  }

  if (new_state & DRW_STATE_SHADOW_OFFSET) {
    GPU_shadow_offset(true);
  }
  else {
    GPU_shadow_offset(false);
  }

  /* TODO: this should be part of shader state. */
  GPU_clip_distances(recording_state.clip_plane_count);

  if (new_state & DRW_STATE_IN_FRONT_SELECT) {
    /* XXX `GPU_depth_range` is not a perfect solution
     * since very distant geometries can still be occluded.
     * Also the depth test precision of these geometries is impaired.
     * However, it solves the selection for the vast majority of cases. */
    GPU_depth_range(0.0f, 0.01f);
  }
  else {
    GPU_depth_range(0.0f, 1.0f);
  }

  if (new_state & DRW_STATE_PROGRAM_POINT_SIZE) {
    GPU_program_point_size(true);
  }
  else {
    GPU_program_point_size(false);
  }
}

void StateSet::set(DRWState state)
{
  RecordingState recording_state;
  StateSet{state, 0}.execute(recording_state);

  /* This function is used for cleaning the state for the viewport drawing.
   * Make sure to reset textures resources to avoid feedback loop when rendering (see #131652). */
  GPU_texture_unbind_all();
  GPU_texture_image_unbind_all();
  GPU_uniformbuf_debug_unbind_all();
  GPU_storagebuf_debug_unbind_all();

  /* Remained of legacy draw manager. Kept it to avoid regression, but might become unneeded. */
  GPU_point_size(5);
  GPU_line_smooth(false);
  GPU_line_width(0.0f);
}

void StencilSet::execute() const
{
  GPU_stencil_write_mask_set(write_mask);
  GPU_stencil_compare_mask_set(compare_mask);
  GPU_stencil_reference_set(reference);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Commands Serialization for debugging
 * \{ */

std::string ShaderBind::serialize() const
{
  return std::string(".shader_bind(") + GPU_shader_get_name(shader) + ")";
}

std::string FramebufferBind::serialize() const
{
  return std::string(".framebuffer_bind(") +
         (*framebuffer == nullptr ? "nullptr" : GPU_framebuffer_get_name(*framebuffer)) + ")";
}

std::string SubPassTransition::serialize() const
{
  auto to_str = [](GPUAttachmentState state) {
    return (state != GPU_ATTACHMENT_IGNORE) ?
               ((state == GPU_ATTACHMENT_WRITE) ? "write" : "read") :
               "ignore";
  };

  return std::string(".subpass_transition(\n") +
         "depth=" + to_str(GPUAttachmentState(depth_state)) + ",\n" +
         "color0=" + to_str(GPUAttachmentState(color_states[0])) + ",\n" +
         "color1=" + to_str(GPUAttachmentState(color_states[1])) + ",\n" +
         "color2=" + to_str(GPUAttachmentState(color_states[2])) + ",\n" +
         "color3=" + to_str(GPUAttachmentState(color_states[3])) + ",\n" +
         "color4=" + to_str(GPUAttachmentState(color_states[4])) + ",\n" +
         "color5=" + to_str(GPUAttachmentState(color_states[5])) + ",\n" +
         "color6=" + to_str(GPUAttachmentState(color_states[6])) + ",\n" +
         "color7=" + to_str(GPUAttachmentState(color_states[7])) + "\n)";
}

std::string ResourceBind::serialize() const
{
  switch (type) {
    case Type::Sampler:
      return std::string(".bind_texture") + (is_reference ? "_ref" : "") + "(" +
             std::to_string(slot) + ", sampler=" + sampler.to_string() + ")";
    case Type::BufferSampler:
      return std::string(".bind_vertbuf_as_texture") + (is_reference ? "_ref" : "") + "(" +
             std::to_string(slot) + ")";
    case Type::Image:
      return std::string(".bind_image") + (is_reference ? "_ref" : "") + "(" +
             std::to_string(slot) + ")";
    case Type::UniformBuf:
      return std::string(".bind_uniform_buf") + (is_reference ? "_ref" : "") + "(" +
             std::to_string(slot) + ")";
    case Type::StorageBuf:
      return std::string(".bind_storage_buf") + (is_reference ? "_ref" : "") + "(" +
             std::to_string(slot) + ")";
    case Type::UniformAsStorageBuf:
      return std::string(".bind_uniform_as_ssbo") + (is_reference ? "_ref" : "") + "(" +
             std::to_string(slot) + ")";
    case Type::VertexAsStorageBuf:
      return std::string(".bind_vertbuf_as_ssbo") + (is_reference ? "_ref" : "") + "(" +
             std::to_string(slot) + ")";
    case Type::IndexAsStorageBuf:
      return std::string(".bind_indexbuf_as_ssbo") + (is_reference ? "_ref" : "") + "(" +
             std::to_string(slot) + ")";
    default:
      BLI_assert_unreachable();
      return "";
  }
}

std::string PushConstant::serialize() const
{
  std::stringstream ss;
  for (int i = 0; i < array_len; i++) {
    switch (comp_len) {
      case 1:
        switch (type) {
          case Type::IntValue:
            ss << int1_value;
            break;
          case Type::IntReference:
            ss << int_ref[i];
            break;
          case Type::FloatValue:
            ss << float1_value;
            break;
          case Type::FloatReference:
            ss << float_ref[i];
            break;
        }
        break;
      case 2:
        switch (type) {
          case Type::IntValue:
            ss << int2_value;
            break;
          case Type::IntReference:
            ss << int2_ref[i];
            break;
          case Type::FloatValue:
            ss << float2_value;
            break;
          case Type::FloatReference:
            ss << float2_ref[i];
            break;
        }
        break;
      case 3:
        switch (type) {
          case Type::IntValue:
            ss << int3_value;
            break;
          case Type::IntReference:
            ss << int3_ref[i];
            break;
          case Type::FloatValue:
            ss << float3_value;
            break;
          case Type::FloatReference:
            ss << float3_ref[i];
            break;
        }
        break;
      case 4:
        switch (type) {
          case Type::IntValue:
            ss << int4_value;
            break;
          case Type::IntReference:
            ss << int4_ref[i];
            break;
          case Type::FloatValue:
            ss << float4_value;
            break;
          case Type::FloatReference:
            ss << float4_ref[i];
            break;
        }
        break;
      case 16:
        switch (type) {
          case Type::IntValue:
          case Type::IntReference:
            BLI_assert_unreachable();
            break;
          case Type::FloatValue:
            ss << float4x4(
                (&float4_value)[0], (&float4_value)[1], (&float4_value)[2], (&float4_value)[3]);
            break;
          case Type::FloatReference:
            ss << *float4x4_ref;
            break;
        }
        break;
    }
    if (i < array_len - 1) {
      ss << ", ";
    }
  }

  return std::string(".push_constant(") + std::to_string(location) + ", data=" + ss.str() + ")";
}

std::string SpecializeConstant::serialize() const
{
  std::stringstream ss;
  switch (type) {
    case Type::IntValue:
      ss << int_value;
      break;
    case Type::UintValue:
      ss << uint_value;
      break;
    case Type::FloatValue:
      ss << float_value;
      break;
    case Type::BoolValue:
      ss << bool_value;
      break;
    case Type::IntReference:
      ss << *int_ref;
      break;
    case Type::UintReference:
      ss << *uint_ref;
      break;
    case Type::FloatReference:
      ss << *float_ref;
      break;
    case Type::BoolReference:
      ss << *bool_ref;
      break;
  }
  return std::string(".specialize_constant(") + std::to_string(location) + ", data=" + ss.str() +
         ")";
}

std::string Draw::serialize() const
{
  std::string inst_len = std::to_string(instance_len);
  std::string vert_len = (vertex_len == uint(-1)) ? "from_batch" : std::to_string(vertex_len);
  std::string vert_first = (vertex_first == uint(-1)) ? "from_batch" :
                                                        std::to_string(vertex_first);
  return std::string(".draw(inst_len=") + inst_len + ", vert_len=" + vert_len +
         ", vert_first=" + vert_first + ", res_id=" + std::to_string(res_index.resource_index()) +
         ")";
}

std::string DrawMulti::serialize(const std::string &line_prefix) const
{
  DrawMultiBuf::DrawGroupBuf &groups = multi_draw_buf->group_buf_;

  MutableSpan<DrawPrototype> prototypes(multi_draw_buf->prototype_buf_.data(),
                                        multi_draw_buf->prototype_count_);

  /* This emulates the GPU sorting but without the unstable draw order. */
  std::sort(
      prototypes.begin(), prototypes.end(), [](const DrawPrototype &a, const DrawPrototype &b) {
        return (a.group_id < b.group_id) ||
               (a.group_id == b.group_id && a.res_index > b.res_index);
      });

  /* Compute prefix sum to have correct offsets. */
  uint prefix_sum = 0u;
  for (DrawGroup &group : groups) {
    group.start = prefix_sum;
    prefix_sum += group.front_facing_counter + group.back_facing_counter;
  }

  std::stringstream ss;

  uint group_len = 0;
  uint group_index = this->group_first;
  while (group_index != uint(-1)) {
    const DrawGroup &grp = groups[group_index];

    ss << std::endl << line_prefix << "  .group(id=" << group_index << ", len=" << grp.len << ")";

    intptr_t offset = grp.start;

    if (grp.back_facing_counter > 0) {
      for (DrawPrototype &proto : prototypes.slice_safe({offset, grp.back_facing_counter})) {
        BLI_assert(proto.group_id == group_index);
        ResourceIndex res_index(proto.res_index);
        BLI_assert(res_index.has_inverted_handedness());
        ss << std::endl
           << line_prefix << "    .proto(instance_len=" << std::to_string(proto.instance_len)
           << ", resource_id=" << std::to_string(res_index.resource_index()) << ", back_face)";
      }
      offset += grp.back_facing_counter;
    }

    if (grp.front_facing_counter > 0) {
      for (DrawPrototype &proto : prototypes.slice_safe({offset, grp.front_facing_counter})) {
        BLI_assert(proto.group_id == group_index);
        ResourceIndex res_index(proto.res_index);
        BLI_assert(!res_index.has_inverted_handedness());
        ss << std::endl
           << line_prefix << "    .proto(instance_len=" << std::to_string(proto.instance_len)
           << ", resource_id=" << std::to_string(res_index.resource_index()) << ", front_face)";
      }
    }

    group_index = grp.next;
    group_len++;
  }

  ss << std::endl;

  return line_prefix + ".draw_multi(" + std::to_string(group_len) + ")" + ss.str();
}

std::string DrawIndirect::serialize() const
{
  return std::string(".draw_indirect()");
}

std::string Dispatch::serialize() const
{
  int3 sz = is_reference ? *size_ref : size;
  return std::string(".dispatch") + (is_reference ? "_ref" : "") + "(" + std::to_string(sz.x) +
         ", " + std::to_string(sz.y) + ", " + std::to_string(sz.z) + ")";
}

std::string DispatchIndirect::serialize() const
{
  return std::string(".dispatch_indirect()");
}

std::string Barrier::serialize() const
{
  /* TODO(@fclem): Better serialization... */
  return std::string(".barrier(") + std::to_string(type) + ")";
}

std::string Clear::serialize() const
{
  std::stringstream ss;
  if (GPUFrameBufferBits(clear_channels) & GPU_COLOR_BIT) {
    ss << "color=" << color;
    if (GPUFrameBufferBits(clear_channels) & (GPU_DEPTH_BIT | GPU_STENCIL_BIT)) {
      ss << ", ";
    }
  }
  if (GPUFrameBufferBits(clear_channels) & GPU_DEPTH_BIT) {
    ss << "depth=" << depth;
    if (GPUFrameBufferBits(clear_channels) & GPU_STENCIL_BIT) {
      ss << ", ";
    }
  }
  if (GPUFrameBufferBits(clear_channels) & GPU_STENCIL_BIT) {
    ss << "stencil=0b" << std::bitset<8>(stencil) << ")";
  }
  return std::string(".clear(") + ss.str() + ")";
}

std::string ClearMulti::serialize() const
{
  std::stringstream ss;
  for (float4 color : Span<float4>(colors, colors_len)) {
    ss << color << ", ";
  }
  return std::string(".clear_multi(colors={") + ss.str() + "})";
}

std::string StateSet::serialize() const
{
  /* TODO(@fclem): Better serialization... */
  return std::string(".state_set(") + std::to_string(new_state) + ")";
}

std::string StencilSet::serialize() const
{
  std::stringstream ss;
  ss << ".stencil_set(write_mask=0b" << std::bitset<8>(write_mask) << ", reference=0b"
     << std::bitset<8>(reference) << ", compare_mask=0b" << std::bitset<8>(compare_mask) << ")";
  return ss.str();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Commands buffers binding / command / resource ID generation
 * \{ */

void DrawCommandBuf::finalize_commands(Vector<Header, 0> &headers,
                                       Vector<Undetermined, 0> &commands,
                                       SubPassVector &sub_passes,
                                       uint &resource_id_count,
                                       ResourceIdBuf &resource_id_buf)
{
  for (const Header &header : headers) {
    if (header.type == Type::SubPass) {
      /** WARNING: Recursive. */
      auto &sub = sub_passes[int64_t(header.index)];
      finalize_commands(
          sub.headers_, sub.commands_, sub_passes, resource_id_count, resource_id_buf);
    }

    if (header.type != Type::Draw) {
      continue;
    }

    Draw &cmd = commands[header.index].draw;

    int batch_vert_len, batch_vert_first, batch_base_index, batch_inst_len;
    /* Now that GPUBatches are guaranteed to be finished, extract their parameters. */
    GPU_batch_draw_parameter_get(
        cmd.batch, &batch_vert_len, &batch_vert_first, &batch_base_index, &batch_inst_len);
    /* Instancing attributes are not supported using the new pipeline since we use the base
     * instance to set the correct resource_id. Workaround is a storage_buf + gl_InstanceID. */
    BLI_assert(batch_inst_len == 1);

    if (cmd.vertex_len == uint(-1)) {
      cmd.vertex_len = batch_vert_len;
    }

    /* NOTE: Only do this if a handle is present. If a draw-call is using instancing with null
     * handle, the shader should not rely on `resource_id` at ***all***. This allows procedural
     * instanced draw-calls with lots of instances with no overhead. */
    /* TODO(fclem): Think about either fixing this feature or removing support for instancing all
     * together. */
    if (cmd.res_index.raw > 0) {
      /* Save correct offset to start of resource_id buffer region for this draw. */
      uint instance_first = resource_id_count;
      resource_id_count += cmd.instance_len;
      /* Ensure the buffer is big enough. */
      resource_id_buf.get_or_resize(resource_id_count - 1);

      /* Copy the resource id for all instances. */
      uint index = cmd.res_index.resource_index();
      for (int i = instance_first; i < (instance_first + cmd.instance_len); i++) {
        resource_id_buf[i] = index;
      }
    }
  }
}

void DrawCommandBuf::generate_commands(Vector<Header, 0> &headers,
                                       Vector<Undetermined, 0> &commands,
                                       SubPassVector &sub_passes)
{
  /* First instance ID contains the null handle with identity transform.
   * This is referenced for draw-calls with no handle. */
  resource_id_buf_.get_or_resize(0) = 0;
  resource_id_count_ = 1;
  finalize_commands(headers, commands, sub_passes, resource_id_count_, resource_id_buf_);
  resource_id_buf_.push_update();
}

void DrawCommandBuf::bind(RecordingState & /*state*/)
{
  GPU_storagebuf_bind(resource_id_buf_, DRW_RESOURCE_ID_SLOT);
}

void DrawMultiBuf::generate_commands(Vector<Header, 0> & /*headers*/,
                                     Vector<Undetermined, 0> & /*commands*/,
                                     VisibilityBuf &visibility_buf,
                                     int visibility_word_per_draw,
                                     int view_len,
                                     bool use_custom_ids)
{
  GPU_debug_group_begin("DrawMultiBuf.bind");

  resource_id_count_ = 0u;
  for (DrawGroup &group : MutableSpan<DrawGroup>(group_buf_.data(), group_count_)) {
    /* Compute prefix sum of all instance of previous group. */
    group.start = resource_id_count_;
    resource_id_count_ += group.len;

    int batch_vert_len, batch_vert_first, batch_base_index, batch_inst_len;
    /* Now that GPUBatches are guaranteed to be finished, extract their parameters. */
    GPU_batch_draw_parameter_get(group.desc.gpu_batch,
                                 &batch_vert_len,
                                 &batch_vert_first,
                                 &batch_base_index,
                                 &batch_inst_len);

    group.vertex_len = group.desc.vertex_len == 0 ? batch_vert_len : group.desc.vertex_len;
    group.vertex_first = group.desc.vertex_first == -1 ? batch_vert_first :
                                                         group.desc.vertex_first;
    group.base_index = batch_base_index;
    /* Instancing attributes are not supported using the new pipeline since we use the base
     * instance to set the correct resource_id. Workaround is a storage_buf + gl_InstanceID. */
    BLI_assert(batch_inst_len == 1);
    UNUSED_VARS_NDEBUG(batch_inst_len);

    if (group.desc.expand_prim_type != GPU_PRIM_NONE) {
      /* Expanded draw-call. */
      IndexRange vert_range = GPU_batch_draw_expanded_parameter_get(
          group.desc.gpu_batch->prim_type,
          GPUPrimType(group.desc.expand_prim_type),
          group.vertex_len,
          group.vertex_first,
          group.desc.expand_prim_len);

      group.vertex_first = vert_range.start();
      group.vertex_len = vert_range.size();
      /* Override base index to -1 as the generated draw-call will not use an index buffer and do
       * the indirection manually inside the shader. */
      group.base_index = -1;
    }

    /* Reset counters to 0 for the GPU. */
    group.total_counter = group.front_facing_counter = group.back_facing_counter = 0;
  }

  group_buf_.push_update();
  prototype_buf_.push_update();
  /* Allocate enough for the expansion pass. */
  resource_id_buf_.get_or_resize(resource_id_count_ * view_len * (use_custom_ids ? 2 : 1));
  /* Two commands per group (inverted and non-inverted scale). */
  command_buf_.get_or_resize(group_count_ * 2);

  if (prototype_count_ > 0) {
    gpu::Shader *shader = DRW_shader_draw_command_generate_get();
    GPU_shader_bind(shader);
    GPU_shader_uniform_1i(shader, "prototype_len", prototype_count_);
    GPU_shader_uniform_1i(shader, "visibility_word_per_draw", visibility_word_per_draw);
    GPU_shader_uniform_1i(shader, "view_len", view_len);
    GPU_shader_uniform_1i(shader, "view_shift", log2_ceil_u(view_len));
    GPU_shader_uniform_1b(shader, "use_custom_ids", use_custom_ids);
    GPU_storagebuf_bind(group_buf_, GPU_shader_get_ssbo_binding(shader, "group_buf"));
    GPU_storagebuf_bind(visibility_buf, GPU_shader_get_ssbo_binding(shader, "visibility_buf"));
    GPU_storagebuf_bind(prototype_buf_, GPU_shader_get_ssbo_binding(shader, "prototype_buf"));
    GPU_storagebuf_bind(command_buf_, GPU_shader_get_ssbo_binding(shader, "command_buf"));
    GPU_storagebuf_bind(resource_id_buf_, DRW_RESOURCE_ID_SLOT);
    GPU_compute_dispatch(shader, divide_ceil_u(prototype_count_, DRW_COMMAND_GROUP_SIZE), 1, 1);
    /* TODO(@fclem): Investigate moving the barrier in the bind function. */
    GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
    GPU_storagebuf_sync_as_indirect_buffer(command_buf_);
  }

  GPU_debug_group_end();
}

void DrawMultiBuf::bind(RecordingState & /*state*/)
{
  GPU_storagebuf_bind(resource_id_buf_, DRW_RESOURCE_ID_SLOT);
}

/** \} */

};  // namespace blender::draw::command
