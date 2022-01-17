#ifdef USE_WORLD_CLIP_PLANES
#  if defined(GPU_VERTEX_SHADER) || defined(GPU_GEOMETRY_SHADER)

uniform vec4 WorldClipPlanes[6];

#    define _world_clip_planes_calc_clip_distance(wpos, _clipplanes) \
      { \
        vec4 pos = vec4(wpos, 1.0); \
        gl_ClipDistance[0] = dot(_clipplanes[0], pos); \
        gl_ClipDistance[1] = dot(_clipplanes[1], pos); \
        gl_ClipDistance[2] = dot(_clipplanes[2], pos); \
        gl_ClipDistance[3] = dot(_clipplanes[3], pos); \
        gl_ClipDistance[4] = dot(_clipplanes[4], pos); \
        gl_ClipDistance[5] = dot(_clipplanes[5], pos); \
      }

/* HACK Dirty hack to be able to override the definition in common_view_lib.glsl.
 * Not doing this would require changing the include order in every shaders. */
#    define world_clip_planes_calc_clip_distance(wpos) \
      _world_clip_planes_calc_clip_distance(wpos, WorldClipPlanes)

#  endif

#  define world_clip_planes_set_clip_distance(c) \
    { \
      gl_ClipDistance[0] = (c)[0]; \
      gl_ClipDistance[1] = (c)[1]; \
      gl_ClipDistance[2] = (c)[2]; \
      gl_ClipDistance[3] = (c)[3]; \
      gl_ClipDistance[4] = (c)[4]; \
      gl_ClipDistance[5] = (c)[5]; \
    }

#endif
