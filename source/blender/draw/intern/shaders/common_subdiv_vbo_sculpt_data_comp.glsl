
/* To be compile with common_subdiv_lib.glsl */

struct SculptData {
  uint face_set_color;
  float mask;
};

layout(std430, binding = 0) readonly restrict buffer sculptMask
{
  float sculpt_mask[];
};

layout(std430, binding = 1) readonly restrict buffer faceSetColor
{
  uint face_set_color[];
};

layout(std430, binding = 2) writeonly restrict buffer sculptData
{
  SculptData sculpt_data[];
};

void main()
{
  /* We execute for each quad. */
  uint quad_index = get_global_invocation_index();
  if (quad_index >= total_dispatch_size) {
    return;
  }

  uint start_loop_index = quad_index * 4;

  for (uint loop_index = start_loop_index; loop_index < start_loop_index + 4; loop_index++) {
    SculptData data;
    data.face_set_color = face_set_color[loop_index];

    if (has_sculpt_mask) {
      data.mask = sculpt_mask[loop_index];
    }
    else {
      data.mask = 0.0;
    }

    sculpt_data[loop_index] = data;
  }
}
