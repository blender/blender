
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

#if defined(GPU_VERTEX_SHADER) || defined(GPU_GEOMETRY_SHADER)

void view_clipping_distances(vec3 wpos)
{
#  ifdef USE_WORLD_CLIP_PLANES
  vec4 pos = vec4(wpos, 1.0);
  gl_ClipDistance[0] = dot(drw_view.clip_planes[0], pos);
  gl_ClipDistance[1] = dot(drw_view.clip_planes[1], pos);
  gl_ClipDistance[2] = dot(drw_view.clip_planes[2], pos);
  gl_ClipDistance[3] = dot(drw_view.clip_planes[3], pos);
  gl_ClipDistance[4] = dot(drw_view.clip_planes[4], pos);
  gl_ClipDistance[5] = dot(drw_view.clip_planes[5], pos);
#  endif
}

/* Kept as define for compiler compatibility. */
#  ifdef USE_WORLD_CLIP_PLANES
#    define view_clipping_distances_set(c) \
      gl_ClipDistance[0] = (c).gl_ClipDistance[0]; \
      gl_ClipDistance[1] = (c).gl_ClipDistance[1]; \
      gl_ClipDistance[2] = (c).gl_ClipDistance[2]; \
      gl_ClipDistance[3] = (c).gl_ClipDistance[3]; \
      gl_ClipDistance[4] = (c).gl_ClipDistance[4]; \
      gl_ClipDistance[5] = (c).gl_ClipDistance[5];

#  else
#    define view_clipping_distances_set(c)
#  endif

#endif
