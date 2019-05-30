
uniform mat4 ViewProjectionMatrix;

/* ---- Instantiated Attrs ---- */
in vec3 pos;

/* ---- Per instance Attrs ---- */
in vec3 color;
in float start;
in float end;
in mat4 InstanceModelMatrix;

uniform float size;

flat out vec4 finalColor;

void main()
{
  float len = end - start;
  vec3 sta = vec3(0.0, 0.0, -start);

  vec4 wPos = InstanceModelMatrix * vec4(pos * -len + sta, 1.0);

  gl_Position = ViewProjectionMatrix * wPos;
  gl_PointSize = size;
  finalColor = vec4(color, 1.0);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(wPos.xyz);
#endif
}
