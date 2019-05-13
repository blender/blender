
#ifndef HAIR_SHADER
in vec3 pos;
in vec3 nor;
#endif

out vec3 worldPosition;
out vec3 viewPosition;

/* Used for planar reflections */
/* keep in sync with EEVEE_ClipPlanesUniformBuffer */
layout(std140) uniform clip_block
{
  vec4 ClipPlanes[1];
};

out vec3 worldNormal;
out vec3 viewNormal;

#ifdef HAIR_SHADER
out vec3 hairTangent;
out float hairThickTime;
out float hairThickness;
out float hairTime;
flat out int hairStrandID;
#endif

void main()
{
#ifdef GPU_INTEL
  /* Due to some shader compiler bug, we somewhat
   * need to access gl_VertexID to make it work. even
   * if it's actually dead code. */
  gl_Position.x = float(gl_VertexID);
#endif

#ifdef HAIR_SHADER
  hairStrandID = hair_get_strand_id();
  vec3 pos, binor;
  hair_get_pos_tan_binor_time((ProjectionMatrix[3][3] == 0.0),
                              ModelMatrixInverse,
                              ViewMatrixInverse[3].xyz,
                              ViewMatrixInverse[2].xyz,
                              pos,
                              hairTangent,
                              binor,
                              hairTime,
                              hairThickness,
                              hairThickTime);

  hairTangent = normalize(hairTangent);
  worldNormal = cross(binor, hairTangent);
  worldPosition = pos;
#else
  worldPosition = point_object_to_world(pos);
  worldNormal = normalize(normal_object_to_world(nor));
#endif

  /* No need to normalize since this is just a rotation. */
  viewNormal = normal_world_to_view(worldNormal);

  viewPosition = point_world_to_view(worldPosition);
  gl_Position = point_world_to_ndc(worldPosition);

  /* Used for planar reflections */
  gl_ClipDistance[0] = dot(vec4(worldPosition, 1.0), ClipPlanes[0]);

#ifdef USE_ATTR
  pass_attr(pos);
#endif
}
