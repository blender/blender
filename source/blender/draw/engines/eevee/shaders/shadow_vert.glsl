
uniform mat4 ModelViewProjectionMatrix;
#ifdef MESH_SHADER
uniform mat4 ModelViewMatrix;
#  ifndef USE_ATTR
uniform mat4 ModelMatrix;
#  endif
#endif

in vec3 pos;
in vec3 nor;

#ifdef MESH_SHADER
out vec3 worldPosition;
out vec3 viewPosition;
out vec3 worldNormal;
out vec3 viewNormal;
#endif

void main()
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
#ifdef MESH_SHADER
  viewPosition = (ModelViewMatrix * vec4(pos, 1.0)).xyz;
  worldPosition = (ModelMatrix * vec4(pos, 1.0)).xyz;

  worldNormal = normalize(transform_normal_object_to_world(nor));
  /* No need to normalize since this is just a rotation. */
  viewNormal = transform_normal_world_to_view(worldNormal);
#  ifdef USE_ATTR
  pass_attr(pos);
#  endif
#endif
}
