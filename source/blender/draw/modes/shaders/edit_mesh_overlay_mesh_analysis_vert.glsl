uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;

in vec3 pos;
in vec4 weight_color;

#ifdef FACE_COLOR
flat out vec4 weightColor;
#endif

#ifdef VERTEX_COLOR
out vec4 weightColor;
#endif

void main()
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
  weightColor = vec4(weight_color.rgb, 1.0);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance((ModelMatrix * vec4(pos, 1.0)).xyz);
#endif
}
