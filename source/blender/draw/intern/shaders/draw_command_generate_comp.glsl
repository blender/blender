/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Convert DrawPrototype into draw commands.
 */

#include "draw_view_infos.hh"

COMPUTE_SHADER_CREATE_INFO(draw_command_generate)

#define atomicAddAndGet(dst, val) (atomicAdd(dst, val) + val)

/* This is only called by the last thread executed over the group's prototype draws. */
void write_draw_call(DrawGroup group, uint group_id)
{
  DrawCommand cmd;
  cmd.vertex_len = group.vertex_len;
  cmd.vertex_first = group.vertex_first;
  bool indexed_draw = group.base_index != -1;

  /* Back-facing command. */
  uint back_facing_start = group.start * view_len;
  if (indexed_draw) {
    cmd.base_index = group.base_index;
    cmd.instance_first_indexed = back_facing_start;
  }
  else {
    cmd._instance_first_array = back_facing_start;
  }
  cmd.instance_len = group_buf[group_id].back_facing_counter;
  command_buf[group_id * 2 + 0] = cmd;

  /* Front-facing command. */
  uint front_facing_start = (group.start + (group.len - group.front_facing_len)) * view_len;
  if (indexed_draw) {
    cmd.instance_first_indexed = front_facing_start;
  }
  else {
    cmd._instance_first_array = front_facing_start;
  }
  cmd.instance_len = group_buf[group_id].front_facing_counter;
  command_buf[group_id * 2 + 1] = cmd;

  /* Reset the counters for a next command gen dispatch. Avoids re-sending the whole data just
   * for this purpose. Only the last thread will execute this so it is thread-safe. */
  group_buf[group_id].front_facing_counter = 0u;
  group_buf[group_id].back_facing_counter = 0u;
  group_buf[group_id].total_counter = 0u;
}

void main()
{
  int proto_id = int(gl_GlobalInvocationID.x);
  if (proto_id >= prototype_len) {
    return;
  }

  DrawPrototype proto = prototype_buf[proto_id];
  uint group_id = proto.group_id;
  bool is_inverted = (proto.res_index & 0x80000000u) != 0;
  uint resource_index = (proto.res_index & 0x7FFFFFFFu);

  /* Visibility test result. */
  uint visible_instance_len = 0;
  if (visibility_word_per_draw > 0) {
    uint visibility_word = resource_index * visibility_word_per_draw;
    for (int i = 0; i < visibility_word_per_draw; i++, visibility_word++) {
      /* NOTE: This assumes `proto.instance_len` is 1. */
      /* TODO: Assert. */
      visible_instance_len += bitCount(visibility_buf[visibility_word]);
    }
  }
  else {
    if ((visibility_buf[resource_index / 32u] & (1u << (resource_index % 32u))) != 0) {
      visible_instance_len = proto.instance_len;
    }
  }
  bool is_visible = visible_instance_len > 0;

  DrawGroup group = group_buf[group_id];

  if (!is_visible) {
    /* Skip the draw but still count towards the completion. */
    if (atomicAddAndGet(group_buf[group_id].total_counter, proto.instance_len) == group.len) {
      write_draw_call(group, group_id);
    }
    return;
  }

  uint back_facing_len = (group.len - group.front_facing_len) * view_len;
  uint dst_index = group.start * view_len;
  if (is_inverted) {
    uint offset = atomicAdd(group_buf[group_id].back_facing_counter, visible_instance_len);
    dst_index += offset;
    if (atomicAddAndGet(group_buf[group_id].total_counter, proto.instance_len) == group.len) {
      write_draw_call(group, group_id);
    }
  }
  else {
    uint offset = atomicAdd(group_buf[group_id].front_facing_counter, visible_instance_len);
    dst_index += back_facing_len + offset;
    if (atomicAddAndGet(group_buf[group_id].total_counter, proto.instance_len) == group.len) {
      write_draw_call(group, group_id);
    }
  }

  /* Fill resource_id buffer for each instance of this draw. */
  if (visibility_word_per_draw > 0) {
    uint visibility_word = resource_index * visibility_word_per_draw;
    for (int i = 0; i < visibility_word_per_draw; i++, visibility_word++) {
      uint word = visibility_buf[visibility_word];
      uint view_index = i * 32u;
      while (word != 0u) {
        if ((word & 1u) != 0u) {
          if (use_custom_ids) {
            resource_id_buf[dst_index * 2] = view_index | (resource_index << view_shift);
            resource_id_buf[dst_index * 2 + 1] = proto.custom_id;
          }
          else {
            resource_id_buf[dst_index] = view_index | (resource_index << view_shift);
          }
          dst_index++;
        }
        view_index++;
        word >>= 1u;
      }
    }
  }
  else {
    for (uint i = dst_index; i < dst_index + visible_instance_len; i++) {
      if (use_custom_ids) {
        resource_id_buf[i * 2] = resource_index;
        resource_id_buf[i * 2 + 1] = proto.custom_id;
      }
      else {
        resource_id_buf[i] = resource_index;
      }
    }
  }
}
