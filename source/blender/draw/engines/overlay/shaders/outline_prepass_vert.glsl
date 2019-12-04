
uniform bool isTransform;

in vec3 pos;

#ifdef USE_GEOM
out vec3 vPos;
out int objectId_g;
#  define objectId objectId_g
#else

flat out int objectId;
#endif

int outline_colorid_get(void)
{
  int flag = int(abs(ObjectInfo.w));
  bool is_from_dupli = (flag & DRW_BASE_FROM_DUPLI) != 0;
  bool is_active = (flag & DRW_BASE_ACTIVE) != 0;

  if (is_from_dupli) {
    if (isTransform) {
      return 0; /* colorTransform */
    }
    else {
      return 2; /* colorDupliSelect */
    }
  }

  if (isTransform) {
    return 0; /* colorTransform */
  }
  else if (is_active) {
    return 3; /* colorActive */
  }
  else {
    return 1; /* colorSelect */
  }

  return 0;
}

/* Replace top 2 bits (of the 16bit output) by outlineId.
 * This leaves 16K different IDs to create outlines between objects.
 * SHIFT = (32 - (16 - 2)) */
#define SHIFT 18u

void main()
{
  vec3 world_pos = point_object_to_world(pos);
#ifdef USE_GEOM
  vPos = point_world_to_view(world_pos);
#endif
  gl_Position = point_world_to_ndc(world_pos);
  /* Small bias to always be on top of the geom. */
  gl_Position.z -= 1e-3;

  /* ID 0 is nothing (background) */
  objectId = resource_handle + 1;

  /* Should be 2 bits only [0..3]. */
  int outline_id = outline_colorid_get();

  /* Combine for 16bit uint target. */
  objectId = (outline_id << 14) | ((objectId << SHIFT) >> SHIFT);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
