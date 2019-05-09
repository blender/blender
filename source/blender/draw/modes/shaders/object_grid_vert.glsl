
/* Infinite grid
 * Clément Foucault */

uniform vec3 planeAxes;
uniform vec4 gridSettings;
uniform float meshSize;

#define gridDistance gridSettings.x
#define gridResolution gridSettings.y
#define gridScale gridSettings.z
#define gridSubdiv gridSettings.w

uniform int gridFlag;

#define cameraPos (ViewMatrixInverse[3].xyz)

#define PLANE_XY (1 << 4)
#define PLANE_XZ (1 << 5)
#define PLANE_YZ (1 << 6)
#define CLIP_Z_POS (1 << 7)
#define CLIP_Z_NEG (1 << 8)

in vec3 pos;

out vec3 local_pos;

void main()
{
  vec3 vert_pos;

  /* Project camera pos to the needed plane */
  if ((gridFlag & PLANE_XY) != 0) {
    vert_pos = vec3(pos.x, pos.y, 0.0);
  }
  else if ((gridFlag & PLANE_XZ) != 0) {
    vert_pos = vec3(pos.x, 0.0, pos.y);
  }
  else {
    vert_pos = vec3(0.0, pos.x, pos.y);
  }

  local_pos = vert_pos;

  vec3 real_pos = cameraPos * planeAxes + vert_pos * meshSize;

  /* Used for additional Z axis */
  if ((gridFlag & CLIP_Z_POS) != 0) {
    real_pos.z = clamp(real_pos.z, 0.0, 1e30);
    local_pos.z = clamp(local_pos.z, 0.0, 1.0);
  }
  if ((gridFlag & CLIP_Z_NEG) != 0) {
    real_pos.z = clamp(real_pos.z, -1e30, 0.0);
    local_pos.z = clamp(local_pos.z, -1.0, 0.0);
  }

  gl_Position = ViewProjectionMatrix * vec4(real_pos, 1.0);
}
