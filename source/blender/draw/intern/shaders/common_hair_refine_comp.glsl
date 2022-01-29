#pragma BLENDER_REQUIRE(common_hair_lib.glsl)

#ifndef USE_GPU_SHADER_CREATE_INFO
/*
 * To be compiled with common_hair_lib.glsl.
 */

layout(local_size_x = 1, local_size_y = 1) in;
layout(std430, binding = 0) writeonly buffer hairPointOutputBuffer
{
  vec4 posTime[];
}
out_vertbuf;
#endif

void main(void)
{
  float interp_time;
  vec4 data0, data1, data2, data3;
  hair_get_interp_attrs(data0, data1, data2, data3, interp_time);

  vec4 weights = hair_get_weights_cardinal(interp_time);
  vec4 result = hair_interp_data(data0, data1, data2, data3, weights);

  uint index = uint(hair_get_id() * hairStrandsRes) + gl_GlobalInvocationID.y;
  posTime[index] = result;
}
