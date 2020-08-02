
#pragma BLENDER_REQUIRE(common_math_lib.glsl)

/* ---------------------------------------------------------------------- */
/** \name Math intersection & projection functions.
 * \{ */

float point_plane_projection_dist(vec3 lineorigin, vec3 planeorigin, vec3 planenormal)
{
  return dot(planenormal, planeorigin - lineorigin);
}

float line_plane_intersect_dist(vec3 lineorigin,
                                vec3 linedirection,
                                vec3 planeorigin,
                                vec3 planenormal)
{
  return dot(planenormal, planeorigin - lineorigin) / dot(planenormal, linedirection);
}

float line_plane_intersect_dist(vec3 lineorigin, vec3 linedirection, vec4 plane)
{
  vec3 plane_co = plane.xyz * (-plane.w / len_squared(plane.xyz));
  vec3 h = lineorigin - plane_co;
  return -dot(plane.xyz, h) / dot(plane.xyz, linedirection);
}

vec3 line_plane_intersect(vec3 lineorigin, vec3 linedirection, vec3 planeorigin, vec3 planenormal)
{
  float dist = line_plane_intersect_dist(lineorigin, linedirection, planeorigin, planenormal);
  return lineorigin + linedirection * dist;
}

vec3 line_plane_intersect(vec3 lineorigin, vec3 linedirection, vec4 plane)
{
  float dist = line_plane_intersect_dist(lineorigin, linedirection, plane);
  return lineorigin + linedirection * dist;
}

float line_aligned_plane_intersect_dist(vec3 lineorigin, vec3 linedirection, vec3 planeorigin)
{
  /* aligned plane normal */
  vec3 L = planeorigin - lineorigin;
  float diskdist = length(L);
  vec3 planenormal = -normalize(L);
  return -diskdist / dot(planenormal, linedirection);
}

vec3 line_aligned_plane_intersect(vec3 lineorigin, vec3 linedirection, vec3 planeorigin)
{
  float dist = line_aligned_plane_intersect_dist(lineorigin, linedirection, planeorigin);
  if (dist < 0) {
    /* if intersection is behind we fake the intersection to be
     * really far and (hopefully) not inside the radius of interest */
    dist = 1e16;
  }
  return lineorigin + linedirection * dist;
}

float line_unit_sphere_intersect_dist(vec3 lineorigin, vec3 linedirection)
{
  float a = dot(linedirection, linedirection);
  float b = dot(linedirection, lineorigin);
  float c = dot(lineorigin, lineorigin) - 1;

  float dist = 1e15;
  float determinant = b * b - a * c;
  if (determinant >= 0) {
    dist = (sqrt(determinant) - b) / a;
  }

  return dist;
}

float line_unit_box_intersect_dist(vec3 lineorigin, vec3 linedirection)
{
  /* https://seblagarde.wordpress.com/2012/09/29/image-based-lighting-approaches-and-parallax-corrected-cubemap/
   */
  vec3 firstplane = (vec3(1.0) - lineorigin) / linedirection;
  vec3 secondplane = (vec3(-1.0) - lineorigin) / linedirection;
  vec3 furthestplane = max(firstplane, secondplane);

  return min_v3(furthestplane);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Other useful functions.
 * \{ */

void make_orthonormal_basis(vec3 N, out vec3 T, out vec3 B)
{
  vec3 UpVector = abs(N.z) < 0.99999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
  T = normalize(cross(UpVector, N));
  B = cross(N, T);
}

/* ---- Encode / Decode Normal buffer data ---- */
/* From http://aras-p.info/texts/CompactNormalStorage.html
 * Using Method #4: Spheremap Transform */
vec2 normal_encode(vec3 n, vec3 view)
{
  float p = sqrt(n.z * 8.0 + 8.0);
  return n.xy / p + 0.5;
}

vec3 normal_decode(vec2 enc, vec3 view)
{
  vec2 fenc = enc * 4.0 - 2.0;
  float f = dot(fenc, fenc);
  float g = sqrt(1.0 - f / 4.0);
  vec3 n;
  n.xy = fenc * g;
  n.z = 1 - f / 2;
  return n;
}

/** \} */
