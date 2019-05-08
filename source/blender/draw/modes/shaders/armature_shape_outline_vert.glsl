
uniform vec2 viewportSize;

/* ---- Instantiated Attrs ---- */
in vec3 pos;
in vec3 snor;

/* ---- Per instance Attrs ---- */
in mat4 InstanceModelMatrix;
in vec4 outlineColorSize;

out vec4 pPos;
out vec3 vPos;
out vec2 ssPos;
out vec2 ssNor;
out vec4 vColSize;

/* project to screen space */
vec2 proj(vec4 pos)
{
  return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

void main()
{
  vec4 worldPosition = InstanceModelMatrix * vec4(pos, 1.0);
  vec4 viewpos = ViewMatrix * worldPosition;

  vPos = viewpos.xyz;
  pPos = ProjectionMatrix * viewpos;

  /* This is slow and run per vertex, but it's still faster than
   * doing it per instance on CPU and sending it on via instance attribute. */
  mat3 normal_mat = transpose(inverse(mat3(InstanceModelMatrix)));
  /* TODO FIX: there is still a problem with this vector
   * when the bone is scaled or in persp mode. But it's
   * barelly visible at the outline corners. */
  ssNor = normalize(transform_normal_world_to_view(normal_mat * snor).xy);

  ssPos = proj(pPos);

  vColSize = outlineColorSize;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(worldPosition.xyz);
#endif
}
