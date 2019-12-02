
uniform ivec4 dataMask = ivec4(0xFF);

in vec3 pos;
in ivec4 data;

flat out vec4 faceColor;

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  ivec4 data_m = data & dataMask;

  faceColor = EDIT_MESH_face_color(data_m.x);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
