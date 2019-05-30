
uniform mat4 ViewProjectionMatrix;
#ifdef USE_WORLD_CLIP_PLANES
uniform mat4 ModelMatrix;
#endif
uniform int baseId;

/* ---- Instantiated Attrs ---- */
in vec3 pos;

/* ---- Per instance Attrs ---- */
in mat4 InstanceModelMatrix;
#ifdef UNIFORM_SCALE
in float size;
#else
in vec3 size;
#endif
in int callId;

flat out uint finalId;

void main()
{
  vec4 pos_4d = vec4(pos * size, 1.0);
  gl_Position = ViewProjectionMatrix * InstanceModelMatrix * pos_4d;
  finalId = uint(baseId + callId);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance((ModelMatrix * InstanceModelMatrix * pos_4d).xyz);
#endif
}
