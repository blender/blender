void node_geometry(vec3 I,
                   vec3 N,
                   vec3 orco,
                   mat4 objmat,
                   mat4 toworld,
                   vec2 barycentric,
                   out vec3 position,
                   out vec3 normal,
                   out vec3 tangent,
                   out vec3 true_normal,
                   out vec3 incoming,
                   out vec3 parametric,
                   out float backfacing,
                   out float pointiness,
                   out float random_per_island)
{
  /* handle perspective/orthographic */
  vec3 I_view = (ProjectionMatrix[3][3] == 0.0) ? normalize(I) : vec3(0.0, 0.0, -1.0);
  incoming = -(toworld * vec4(I_view, 0.0)).xyz;

#if defined(WORLD_BACKGROUND) || defined(PROBE_CAPTURE)
  position = -incoming;
  true_normal = normal = incoming;
  tangent = parametric = vec3(0.0);
  vec3(0.0);
  backfacing = 0.0;
  pointiness = 0.0;
#else

  position = worldPosition;
#  ifndef VOLUMETRICS
  normal = normalize(N);
  vec3 B = dFdx(worldPosition);
  vec3 T = dFdy(worldPosition);
  true_normal = normalize(cross(B, T));
#  else
  normal = (toworld * vec4(N, 0.0)).xyz;
  true_normal = normal;
#  endif

#  ifdef HAIR_SHADER
  tangent = -hairTangent;
#  else
  tangent_orco_z(orco, orco);
  node_tangent(N, orco, objmat, tangent);
#  endif

  parametric = vec3(barycentric, 0.0);
  backfacing = (gl_FrontFacing) ? 0.0 : 1.0;
  pointiness = 0.5;
  random_per_island = 0.0;
#endif
}
