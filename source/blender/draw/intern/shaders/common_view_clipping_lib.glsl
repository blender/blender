
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

#if defined(GPU_VERTEX_SHADER) || defined(GPU_GEOMETRY_SHADER)

void view_clipping_distances(vec3 wpos)
{
#  ifdef USE_WORLD_CLIP_PLANES
  vec4 pos_4d = vec4(wpos, 1.0);
  gl_ClipDistance[0] = dot(drw_clipping_[0], pos_4d);
  gl_ClipDistance[1] = dot(drw_clipping_[1], pos_4d);
  gl_ClipDistance[2] = dot(drw_clipping_[2], pos_4d);
  gl_ClipDistance[3] = dot(drw_clipping_[3], pos_4d);
  gl_ClipDistance[4] = dot(drw_clipping_[4], pos_4d);
  gl_ClipDistance[5] = dot(drw_clipping_[5], pos_4d);
#  endif
}

void view_clipping_distances_bypass()
{
#  ifdef USE_WORLD_CLIP_PLANES
  gl_ClipDistance[0] = 1.0;
  gl_ClipDistance[1] = 1.0;
  gl_ClipDistance[2] = 1.0;
  gl_ClipDistance[3] = 1.0;
  gl_ClipDistance[4] = 1.0;
  gl_ClipDistance[5] = 1.0;
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
