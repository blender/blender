
#pragma BLENDER_REQUIRE(common_hair_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

#ifndef HAIR_SHADER
in vec3 pos;
#endif

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

#ifdef HAIR_SHADER
  float time, thick_time, thickness;
  vec3 worldPosition, tan, binor;
  hair_get_pos_tan_binor_time((ProjectionMatrix[3][3] == 0.0),
                              ModelMatrixInverse,
                              ViewMatrixInverse[3].xyz,
                              ViewMatrixInverse[2].xyz,
                              worldPosition,
                              tan,
                              binor,
                              time,
                              thickness,
                              thick_time);
#else
  vec3 worldPosition = point_object_to_world(pos);
#endif

  gl_Position = point_world_to_ndc(worldPosition);

#ifdef CLIP_PLANES
  gl_ClipDistance[0] = dot(vec4(worldPosition.xyz, 1.0), clipPlanes[0]);
#endif
}
