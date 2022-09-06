
/**
 * Convert DrawPrototype into draw commands.
 */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)

#define atomicAddAndGet(dst, val) (atomicAdd(dst, val) + val)

/* This is only called by the last thread executed over the group's prototype draws. */
void write_draw_call(DrawGroup group, uint group_id)
{
  DrawCommand cmd;
  cmd.vertex_len = group.vertex_len;
  cmd.vertex_first = group.vertex_first;
  if (group.base_index != -1) {
    cmd.base_index = group.base_index;
    cmd.instance_first_indexed = group.start;
  }
  else {
    cmd._instance_first_array = group.start;
  }
  /* Back-facing command. */
  cmd.instance_len = group_buf[group_id].back_facing_counter;
  command_buf[group_id * 2 + 0] = cmd;
  /* Front-facing command. */
  cmd.instance_len = group_buf[group_id].front_facing_counter;
  command_buf[group_id * 2 + 1] = cmd;

  /* Reset the counters for a next command gen dispatch. Avoids resending the whole data just
   * for this purpose. Only the last thread will execute this so it is thread-safe. */
  group_buf[group_id].front_facing_counter = 0u;
  group_buf[group_id].back_facing_counter = 0u;
  group_buf[group_id].total_counter = 0u;
}

void main()
{
  uint proto_id = gl_GlobalInvocationID.x;
  if (proto_id >= prototype_len) {
    return;
  }

  DrawPrototype proto = prototype_buf[proto_id];
  uint group_id = proto.group_id;
  bool is_inverted = (proto.resource_handle & 0x80000000u) != 0;
  uint resource_index = (proto.resource_handle & 0x7FFFFFFFu);

  /* Visibility test result. */
  bool is_visible = ((visibility_buf[resource_index / 32u] & (1u << (resource_index % 32u)))) != 0;

  DrawGroup group = group_buf[group_id];

  if (!is_visible) {
    /* Skip the draw but still count towards the completion. */
    if (atomicAddAndGet(group_buf[group_id].total_counter, proto.instance_len) == group.len) {
      write_draw_call(group, group_id);
    }
    return;
  }

  uint back_facing_len = group.len - group.front_facing_len;
  uint front_facing_len = group.front_facing_len;
  uint dst_index = group.start;
  if (is_inverted) {
    uint offset = atomicAdd(group_buf[group_id].back_facing_counter, proto.instance_len);
    dst_index += offset;
    if (atomicAddAndGet(group_buf[group_id].total_counter, proto.instance_len) == group.len) {
      write_draw_call(group, group_id);
    }
  }
  else {
    uint offset = atomicAdd(group_buf[group_id].front_facing_counter, proto.instance_len);
    dst_index += back_facing_len + offset;
    if (atomicAddAndGet(group_buf[group_id].total_counter, proto.instance_len) == group.len) {
      write_draw_call(group, group_id);
    }
  }

  for (uint i = dst_index; i < dst_index + proto.instance_len; i++) {
    /* Fill resource_id buffer for each instance of this draw */
    resource_id_buf[i] = resource_index;
  }
}
