
uniform ivec3 grid_resolution;
uniform vec3 corner;
uniform vec3 increment_x;
uniform vec3 increment_y;
uniform vec3 increment_z;
uniform bool isTransform;

flat out int objectId;

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
  vec3 ls_cell_location;
  /* Keep in sync with update_irradiance_probe */
  ls_cell_location.z = float(gl_VertexID % grid_resolution.z);
  ls_cell_location.y = float((gl_VertexID / grid_resolution.z) % grid_resolution.y);
  ls_cell_location.x = float(gl_VertexID / (grid_resolution.z * grid_resolution.y));

  vec3 ws_cell_location = corner +
                          (increment_x * ls_cell_location.x + increment_y * ls_cell_location.y +
                           increment_z * ls_cell_location.z);

  gl_Position = ViewProjectionMatrix * vec4(ws_cell_location, 1.0);
  gl_PointSize = 2.0f;

  /* ID 0 is nothing (background) */
  objectId = resource_handle + 1;

  /* Should be 2 bits only [0..3]. */
  int outline_id = outline_colorid_get();

  /* Combine for 16bit uint target. */
  objectId = (outline_id << 14) | ((objectId << SHIFT) >> SHIFT);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(ws_cell_location);
#endif
}
