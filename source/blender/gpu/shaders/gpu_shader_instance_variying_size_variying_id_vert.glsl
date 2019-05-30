
uniform mat4 ViewProjectionMatrix;

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
  vec4 wPos = InstanceModelMatrix * vec4(pos * size, 1.0);
  gl_Position = ViewProjectionMatrix * wPos;
  finalId = uint(baseId + callId);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(wPos.xyz);
#endif
}
