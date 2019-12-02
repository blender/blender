
uniform sampler1D weightTex;
uniform vec4 color; /* Drawsize packed in alpha */

/* ---- Instantiated Attrs ---- */
in vec3 pos;
in int vclass;

/* ---- Per instance Attrs ---- */
in vec3 part_pos;
in vec4 part_rot;
in float part_val;

#ifdef USE_DOTS
out vec4 finalColor;
#else
flat out vec4 finalColor;
#endif

#define VCLASS_SCREENALIGNED (1 << 9)

#define VCLASS_EMPTY_AXES (1 << 11)

vec3 rotate(vec3 vec, vec4 quat)
{
  /* The quaternion representation here stores the w component in the first index */
  return vec + 2.0 * cross(quat.yzw, cross(quat.yzw, vec) + quat.x * vec);
}

void main()
{
  float draw_size = color.a;

  vec3 world_pos = part_pos;

#ifdef USE_DOTS
  gl_Position = point_world_to_ndc(world_pos);
  /* World sized points. */
  gl_PointSize = sizePixel * draw_size * ProjectionMatrix[1][1] * sizeViewport.y / gl_Position.w;
#else

  if ((vclass & VCLASS_SCREENALIGNED) != 0) {
    /* World sized, camera facing geometry. */
    world_pos += (screenVecs[0].xyz * pos.x + screenVecs[1].xyz * pos.y) * draw_size;
  }
  else {
    world_pos += rotate(pos, part_rot) * draw_size;
  }

  gl_Position = point_world_to_ndc(world_pos);
#endif

  /* Coloring */
  if ((vclass & VCLASS_EMPTY_AXES) != 0) {
    /* see VBO construction for explanation. */
    finalColor = vec4(clamp(pos * 10000.0, 0.0, 1.0), 1.0);
  }
  else if (part_val < 0.0) {
    finalColor = vec4(color.rgb, 1.0);
  }
  else {
    finalColor = vec4(texture(weightTex, part_val).rgb, 1.0);
  }

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
