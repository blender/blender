/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Convert DrawPrototype into draw commands.
 */

#pragma once

#include "draw_command_shared.hh"
#include "draw_shader_shared.hh"

namespace draw {

#define atomicAddAndGet(dst, val) atomicAdd(dst, val) + val

struct GenerateCommand {
  [[storage(0, read_write)]] DrawGroup (&group_buf)[];
  [[storage(1, read)]] uint (&visibility_buf)[];
  [[storage(2, read)]] DrawPrototype (&prototype_buf)[];
  [[storage(3, write)]] DrawCommand (&command_buf)[];
  [[storage(DRW_RESOURCE_ID_SLOT, write)]] uint (&resource_id_buf)[];

  [[push_constant]] int prototype_len;
  [[push_constant]] int visibility_word_per_draw;
  [[push_constant]] int view_shift;
  [[push_constant]] int view_len;
  [[push_constant]] bool use_custom_ids;

  /* This is only called by the last thread executed over the group's prototype draws. */
  void write_draw_call(DrawGroup group, uint group_id)
  {
    const bool indexed_draw = group.base_index != -1;

    const uint back_facing_len = group_buf[group_id].back_facing_counter;
    const uint back_facing_start = group.start * uint(view_len);
    const uint front_facing_len = group_buf[group_id].front_facing_counter;
    const uint front_facing_start = (group.start + (group.len - group.front_facing_len)) *
                                    uint(view_len);

    /* Back-facing command. */
    DrawCommand cmd;
    if (indexed_draw) {
      DrawCommandIndexed cmd_indexed;
      cmd_indexed.vertex_len = uint(group.vertex_len);
      cmd_indexed.instance_len = back_facing_len;
      cmd_indexed.vertex_first = uint(group.vertex_first);
      cmd_indexed.base_index = uint(group.base_index);
      cmd_indexed.instance_first = back_facing_start;
      cmd.indexed() = cmd_indexed;
    }
    else {
      DrawCommandArray cmd_array;
      cmd_array.vertex_len = uint(group.vertex_len);
      cmd_array.instance_len = back_facing_len;
      cmd_array.vertex_first = uint(group.vertex_first);
      cmd_array.instance_first = back_facing_start;
      cmd.array() = cmd_array;
    }
    command_buf[group_id * 2 + 0] = cmd;

    /* Front-facing command. */
    if (indexed_draw) {
      DrawCommandIndexed cmd_indexed;
      cmd_indexed.vertex_len = uint(group.vertex_len);
      cmd_indexed.instance_len = front_facing_len;
      cmd_indexed.vertex_first = uint(group.vertex_first);
      cmd_indexed.base_index = uint(group.base_index);
      cmd_indexed.instance_first = front_facing_start;
      cmd.indexed() = cmd_indexed;
    }
    else {
      DrawCommandArray cmd_array;
      cmd_array.vertex_len = uint(group.vertex_len);
      cmd_array.instance_len = front_facing_len;
      cmd_array.vertex_first = uint(group.vertex_first);
      cmd_array.instance_first = front_facing_start;
      cmd.array() = cmd_array;
    }
    command_buf[group_id * 2 + 1] = cmd;

    /* Reset the counters for a next command gen dispatch. Avoids re-sending the whole data just
     * for this purpose. Only the last thread will execute this so it is thread-safe. */
    group_buf[group_id].front_facing_counter = 0u;
    group_buf[group_id].back_facing_counter = 0u;
    group_buf[group_id].total_counter = 0u;
  }
};

[[compute, local_size(DRW_COMMAND_GROUP_SIZE)]]
void command_gen([[resource_table]] GenerateCommand &srt,
                 [[global_invocation_id]] const uint3 global_id,
                 [[local_invocation_id]] const uint3 local_id,
                 [[work_group_id]] const uint3 workgroup_id)
{
  int proto_id = int(global_id.x);
  if (proto_id >= srt.prototype_len) {
    return;
  }

  DrawPrototype proto = srt.prototype_buf[proto_id];
  uint group_id = proto.group_id;
  bool is_inverted = (proto.res_id & 0x80000000u) != 0;
  uint resource_id = (proto.res_id & 0x7FFFFFFFu);

  /* Visibility test result. */
  uint visible_instance_len = 0;
  if (srt.visibility_word_per_draw > 0) {
    uint visibility_word = resource_id * uint(srt.visibility_word_per_draw);
    for (int i = 0; i < srt.visibility_word_per_draw; i++, visibility_word++) {
      /* NOTE: This assumes `proto.instance_len` is 1. */
      /* TODO: Assert. */
      visible_instance_len += uint(bitCount(srt.visibility_buf[visibility_word]));
    }
  }
  else {
    if ((srt.visibility_buf[resource_id / 32u] & (1u << (resource_id % 32u))) != 0) {
      visible_instance_len = proto.instance_len;
    }
  }
  bool is_visible = visible_instance_len > 0;

  DrawGroup group = srt.group_buf[group_id];

  if (!is_visible) {
    /* Skip the draw but still count towards the completion. */
    uint group_count = atomicAddAndGet(srt.group_buf[group_id].total_counter, proto.instance_len);
    if (group_count == group.len) {
      srt.write_draw_call(group, group_id);
    }
    return;
  }

  uint back_facing_len = (group.len - group.front_facing_len) * uint(srt.view_len);
  uint dst_index = group.start * uint(srt.view_len);
  if (is_inverted) {
    uint offset = atomicAdd(srt.group_buf[group_id].back_facing_counter, visible_instance_len);
    dst_index += offset;
    uint group_count = atomicAddAndGet(srt.group_buf[group_id].total_counter, proto.instance_len);
    if (group_count == group.len) {
      srt.write_draw_call(group, group_id);
    }
  }
  else {
    uint offset = atomicAdd(srt.group_buf[group_id].front_facing_counter, visible_instance_len);
    dst_index += back_facing_len + offset;
    uint group_count = atomicAddAndGet(srt.group_buf[group_id].total_counter, proto.instance_len);
    if (group_count == group.len) {
      srt.write_draw_call(group, group_id);
    }
  }

  /* Fill resource_id buffer for each instance of this draw. */
  if (srt.visibility_word_per_draw > 0) {
    uint visibility_word = resource_id * uint(srt.visibility_word_per_draw);
    for (int i = 0; i < srt.visibility_word_per_draw; i++, visibility_word++) {
      uint word = srt.visibility_buf[visibility_word];
      uint view_index = uint(i) * 32u;
      while (word != 0u) {
        if ((word & 1u) != 0u) {
          if (srt.use_custom_ids) {
            srt.resource_id_buf[dst_index * 2] = view_index | (resource_id << srt.view_shift);
            srt.resource_id_buf[dst_index * 2 + 1] = proto.custom_id;
          }
          else {
            srt.resource_id_buf[dst_index] = view_index | (resource_id << srt.view_shift);
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
      if (srt.use_custom_ids) {
        srt.resource_id_buf[i * 2] = resource_id;
        srt.resource_id_buf[i * 2 + 1] = proto.custom_id;
      }
      else {
        srt.resource_id_buf[i] = resource_id;
      }
    }
  }
}

PipelineCompute command_generate(command_gen);

}  // namespace draw
