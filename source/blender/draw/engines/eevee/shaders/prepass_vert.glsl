
uniform mat4 ModelMatrix;
uniform mat4 ModelMatrixInverse;

#ifdef CLIP_PLANES
/* keep in sync with DRWManager.view_data */
layout(std140) uniform clip_block
{
  vec4 ClipPlanes[1];
};
#endif

#ifndef HAIR_SHADER
in vec3 pos;
#endif

void main()
{
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
  gl_ClipDistance[0] = dot(vec4(worldPosition.xyz, 1.0), ClipPlanes[0]);
#endif
  /* TODO motion vectors */
}
