
uniform mat4 ViewMatrix;
uniform mat4 ViewMatrixInverse;
uniform mat4 ViewProjectionMatrix;
uniform mat4 ProjectionMatrix;

uniform vec2 viewportSize;
uniform float lineThickness = 2.0;

/* ---- Instantiated Attrs ---- */
in vec2 pos0;
in vec2 pos1;
in vec2 pos2;

/* ---- Per instance Attrs ---- */
/* Assumed to be in world coordinate already. */
in vec4 headSphere;
in vec4 tailSphere;
in vec4 outlineColorSize;
in vec3 xAxis;

flat out vec4 finalColor;

/* project to screen space */
vec2 proj(vec4 pos)
{
  return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

vec2 compute_dir(vec2 v0, vec2 v1, vec2 v2)
{
  vec2 dir = normalize(v2 - v0);
  dir = vec2(dir.y, -dir.x);
  return dir;
}

mat3 compute_mat(vec4 sphere, vec3 bone_vec, out float z_ofs)
{
  bool is_persp = (ProjectionMatrix[3][3] == 0.0);
  vec3 cam_ray = (is_persp) ? sphere.xyz - ViewMatrixInverse[3].xyz : -ViewMatrixInverse[2].xyz;

  /* Sphere center distance from the camera (persp) in world space. */
  float cam_dist = length(cam_ray);

  /* Compute view aligned orthonormal space. */
  vec3 z_axis = cam_ray / cam_dist;
  vec3 x_axis = normalize(cross(bone_vec, z_axis));
  vec3 y_axis = cross(z_axis, x_axis);
  z_ofs = 0.0;

  if (is_persp) {
    /* For perspective, the projected sphere radius
     * can be bigger than the center disc. Compute the
     * max angular size and compensate by sliding the disc
     * towards the camera and scale it accordingly. */
    const float half_pi = 3.1415926 * 0.5;
    float rad = sphere.w;
    /* Let be :
     * V the view vector origin.
     * O the sphere origin.
     * T the point on the target circle.
     * We compute the angle between (OV) and (OT). */
    float a = half_pi - asin(rad / cam_dist);
    float cos_b = cos(a);
    float sin_b = sqrt(clamp(1.0 - cos_b * cos_b, 0.0, 1.0));

    x_axis *= sin_b;
    y_axis *= sin_b;
    z_ofs = -rad * cos_b;
  }

  return mat3(x_axis, y_axis, z_axis);
}

struct Bone {
  vec3 vec;
  float sinb;
};

bool bone_blend_starts(vec3 p, Bone b)
{
  /* we just want to know when the head sphere starts interpolating. */
  return dot(p, b.vec) > -b.sinb;
}

vec3 get_outline_point(vec2 pos,
                       vec4 sph_near,
                       vec4 sph_far,
                       mat3 mat_near,
                       mat3 mat_far,
                       float z_ofs_near,
                       float z_ofs_far,
                       Bone b)
{
  /* Compute outline position on the nearest sphere and check
   * if it penetrates the capsule body. If it does, put this
   * vertex on the farthest sphere. */
  vec3 wpos = mat_near * vec3(pos * sph_near.w, z_ofs_near);
  if (bone_blend_starts(wpos, b)) {
    wpos = sph_far.xyz + mat_far * vec3(pos * sph_far.w, z_ofs_far);
  }
  else {
    wpos += sph_near.xyz;
  }
  return wpos;
}

void main()
{
  float dst_head = distance(headSphere.xyz, ViewMatrixInverse[3].xyz);
  float dst_tail = distance(tailSphere.xyz, ViewMatrixInverse[3].xyz);
  // float dst_head = -dot(headSphere.xyz, ViewMatrix[2].xyz);
  // float dst_tail = -dot(tailSphere.xyz, ViewMatrix[2].xyz);

  vec4 sph_near, sph_far;
  if ((dst_head > dst_tail) && (ProjectionMatrix[3][3] == 0.0)) {
    sph_near = tailSphere;
    sph_far = headSphere;
  }
  else {
    sph_near = headSphere;
    sph_far = tailSphere;
  }

  vec3 bone_vec = (sph_far.xyz - sph_near.xyz) + 1e-8;

  Bone b;
  float bone_lenrcp = 1.0 / max(1e-8, sqrt(dot(bone_vec, bone_vec)));
  b.sinb = (sph_far.w - sph_near.w) * bone_lenrcp * sph_near.w;
  b.vec = bone_vec * bone_lenrcp;

  float z_ofs_near, z_ofs_far;
  mat3 mat_near = compute_mat(sph_near, bone_vec, z_ofs_near);
  mat3 mat_far = compute_mat(sph_far, bone_vec, z_ofs_far);

  vec3 wpos0 = get_outline_point(
      pos0, sph_near, sph_far, mat_near, mat_far, z_ofs_near, z_ofs_far, b);
  vec3 wpos1 = get_outline_point(
      pos1, sph_near, sph_far, mat_near, mat_far, z_ofs_near, z_ofs_far, b);
  vec3 wpos2 = get_outline_point(
      pos2, sph_near, sph_far, mat_near, mat_far, z_ofs_near, z_ofs_far, b);

  vec4 pos_4d = vec4(wpos1, 1.0);
#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(pos_4d.xyz);
#endif

  vec4 V = ViewMatrix * pos_4d;
  float pres_fac = (ProjectionMatrix[3][3] == 0.0) ? abs(V.z) : 1.0;

  vec4 p0 = ViewProjectionMatrix * vec4(wpos0, 1.0);
  vec4 p1 = ProjectionMatrix * V;
  vec4 p2 = ViewProjectionMatrix * vec4(wpos2, 1.0);

  /* compute position from 3 vertex because the change in direction
   * can happen very quicky and lead to very thin edges. */
  vec2 ss0 = proj(p0);
  vec2 ss1 = proj(p1);
  vec2 ss2 = proj(p2);
  vec2 edge_dir = compute_dir(ss0, ss1, ss2);

  bool outer = ((gl_VertexID & 1) == 1);
  vec2 t = outlineColorSize.w * (lineThickness / viewportSize);
  t *= pres_fac;
  t = (outer) ? t : vec2(0.0);

  gl_Position = p1;
  gl_Position.xy += t * edge_dir;

  finalColor = vec4(outlineColorSize.rgb, 1.0);
}
