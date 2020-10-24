
/* ---- Instantiated Attrs ---- */
in vec3 pos;
in vec3 snor;

/* ---- Per instance Attrs ---- */
in vec4 color;
in mat4 inst_obmat;

out vec4 pPos;
out vec3 vPos;
out vec2 ssPos;
out vec2 ssNor;
out vec4 vColSize;
out int inverted;

/* project to screen space */
vec2 proj(vec4 pos)
{
  return (0.5 * (pos.xy / pos.w) + 0.5) * sizeViewport.xy;
}

void main()
{
  vec4 bone_color, state_color;
  mat4 model_mat = extract_matrix_packed_data(inst_obmat, state_color, bone_color);

  vec4 worldPosition = model_mat * vec4(pos, 1.0);
  vec4 viewpos = ViewMatrix * worldPosition;

  vPos = viewpos.xyz;
  pPos = ProjectionMatrix * viewpos;

  inverted = int(dot(cross(model_mat[0].xyz, model_mat[1].xyz), model_mat[2].xyz) < 0.0);

  /* This is slow and run per vertex, but it's still faster than
   * doing it per instance on CPU and sending it on via instance attribute. */
  mat3 normal_mat = transpose(inverse(mat3(model_mat)));
  /* TODO FIX: there is still a problem with this vector
   * when the bone is scaled or in persp mode. But it's
   * barely visible at the outline corners. */
  ssNor = normalize(normal_world_to_view(normal_mat * snor).xy);

  ssPos = proj(pPos);

  vColSize = bone_color;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(worldPosition.xyz);
#endif
}
