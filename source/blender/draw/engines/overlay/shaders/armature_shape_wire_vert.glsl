
/* ---- Instantiated Attrs ---- */
in vec3 pos;
in vec3 nor;

/* ---- Per instance Attrs ---- */
in mat4 inst_obmat;

flat out vec4 finalColor;
flat out vec2 edgeStart;
noperspective out vec2 edgePos;

void main()
{
  vec4 bone_color, state_color;
  mat4 model_mat = extract_matrix_packed_data(inst_obmat, state_color, bone_color);

  vec3 world_pos = (model_mat * vec4(pos, 1.0)).xyz;
  gl_Position = point_world_to_ndc(world_pos);

  finalColor.rgb = mix(state_color.rgb, bone_color.rgb, 0.5);
  finalColor.a = 1.0;

  edgeStart = edgePos = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
