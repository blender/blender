
#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)

void main()
{
  int index = int(gl_GlobalInvocationID.x);
  if (index < tilemaps_clip_buf_len) {
    tilemaps_clip_buf[index].clip_near_stored = 0;
    tilemaps_clip_buf[index].clip_far_stored = 0;
    tilemaps_clip_buf[index].clip_near = floatBitsToOrderedInt(-FLT_MAX);
    tilemaps_clip_buf[index].clip_far = floatBitsToOrderedInt(FLT_MAX);
  }
}
