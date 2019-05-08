
uniform mat4 ModelMatrix;
uniform mat4 ModelMatrixInverse;

uniform float wireStepParam;
uniform float ofs;

in vec3 pos;
in vec3 nor;
in float wd; /* wiredata */

float get_edge_sharpness(float wd)
{
#ifndef USE_SCULPT
  return ((wd == 0.0) ? -1.5 : wd) + wireStepParam;
#else
  return 1.0;
#endif
}

/* Geometry shader version */
#if defined(SELECT_EDGES) || defined(USE_GEOM)
out float facing_g;
out float edgeSharpness_g;

#else /* USE_GEOM */
out float facing;
flat out float edgeSharpness;
#  define facing_g facing
#  define edgeSharpness_g edgeSharpness

#endif /* SELECT_EDGES */

void main()
{
  mat4 projmat = ProjectionMatrix;
  projmat[3][2] -= ofs;

  vec4 wpos = ModelMatrix * vec4(pos, 1.0);
  gl_Position = projmat * (ViewMatrix * wpos);

  vec3 wnor = normalize(transform_normal_object_to_world(nor));
  facing_g = dot(wnor, ViewMatrixInverse[2].xyz);
  edgeSharpness_g = get_edge_sharpness(wd);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(wpos.xyz);
#endif
}
