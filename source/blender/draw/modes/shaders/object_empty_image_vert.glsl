uniform mat4 ModelMatrix;

uniform float aspectX;
uniform float aspectY;
uniform float size;
uniform vec2 offset;
#ifdef USE_WIRE
uniform vec3 color;
#else
uniform vec4 objectColor;
#endif

in vec2 texCoord;
in vec2 pos;

flat out vec4 finalColor;

#ifndef USE_WIRE
out vec2 texCoord_interp;
#endif

void main()
{
  vec3 pos = vec3((pos + offset) * (size * vec2(aspectX, aspectY)), 0.0);
  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);
#ifdef USE_WIRE
  gl_Position.z -= 1e-5;
  finalColor = vec4(color, 1.0);
#else
  texCoord_interp = texCoord;
  finalColor = objectColor;
#endif

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
