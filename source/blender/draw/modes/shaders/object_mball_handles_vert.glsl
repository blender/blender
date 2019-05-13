
/* This shader takes a 2D shape, puts it in 3D Object space such that is stays aligned with view,
 * and scales the shape according to per-instance attributes
 * Note that if the stiffness is zero, it assumes the scale is directly multiplied by the radius */

uniform mat4 ViewProjectionMatrix;
uniform vec3 screen_vecs[2];

/* ---- Instantiated Attrs ---- */
in vec2 pos;

/* ---- Per instance Attrs ---- */
in mat3x4 ScaleTranslationMatrix;
in float radius;
in vec3 color;

flat out vec4 finalColor;

void main()
{
  mat3 Scamat = mat3(ScaleTranslationMatrix);
  vec4 world_pos = vec4(ScaleTranslationMatrix[0][3],
                        ScaleTranslationMatrix[1][3],
                        ScaleTranslationMatrix[2][3],
                        1.0);

  vec3 screen_pos = screen_vecs[0].xyz * pos.x + screen_vecs[1].xyz * pos.y;
  world_pos.xyz += Scamat * (screen_pos * radius);

  gl_Position = ViewProjectionMatrix * world_pos;
  finalColor = vec4(color, 1.0);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos.xyz);
#endif
}
