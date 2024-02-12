/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#define DEGENERATE_TRIS_WORKAROUND
#define DEGENERATE_TRIS_AREA_THRESHOLD 4e-17

#define len_sqr(a) dot(a, a)

void extrude_edge(bool invert)
{
  /* Reverse order if back-facing the light. */
  ivec2 idx = (invert) ? ivec2(1, 2) : ivec2(2, 1);
  gl_Position = vData[idx.x].frontPosition;
  gpu_EmitVertex();
  gl_Position = vData[idx.y].frontPosition;
  gpu_EmitVertex();
  gl_Position = vData[idx.x].backPosition;
  gpu_EmitVertex();
  gl_Position = vData[idx.y].backPosition;
  gpu_EmitVertex();
  EndPrimitive();
}

void main()
{
  vec3 v10 = vData[0].pos - vData[1].pos;
  vec3 v12 = vData[2].pos - vData[1].pos;
  vec3 v13 = vData[3].pos - vData[1].pos;

  vec3 n1 = cross(v12, v10);
  vec3 n2 = cross(v13, v12);

#ifdef DEGENERATE_TRIS_WORKAROUND
  /* Check if area is null */
  vec2 faces_area = vec2(len_sqr(n1), len_sqr(n2));
  bvec2 degen_faces = lessThan(abs(faces_area), vec2(DEGENERATE_TRIS_AREA_THRESHOLD));

  /* Both triangles are degenerate, abort. */
  if (all(degen_faces)) {
    return;
  }
#endif

  vec2 facing = vec2(dot(n1, vData_flat[0].light_direction_os),
                     dot(n2, vData_flat[0].light_direction_os));

  /* WATCH: maybe unpredictable in some cases. */
  bool is_manifold = any(notEqual(vData[0].pos, vData[3].pos));

  bvec2 backface = greaterThan(facing, vec2(0.0));

#ifdef DEGENERATE_TRIS_WORKAROUND
#  ifndef DOUBLE_MANIFOLD
  /* If the mesh is known to be manifold and we don't use double count,
   * only create an quad if the we encounter a facing geom. */
  if ((degen_faces.x && backface.y) || (degen_faces.y && backface.x)) {
    return;
  }
#  endif

  /* If one of the 2 triangles is degenerate, replace edge by a non-manifold one. */
  backface.x = (degen_faces.x) ? !backface.y : backface.x;
  backface.y = (degen_faces.y) ? !backface.x : backface.y;
  is_manifold = (any(degen_faces)) ? false : is_manifold;
#endif

  /* If both faces face the same direction it's not an outline edge. */
  if (backface.x == backface.y) {
    return;
  }

  if (gl_InvocationID == 0) {
    extrude_edge(backface.x);
  }
  else if (is_manifold) {
#ifdef DOUBLE_MANIFOLD
    /* Increment/Decrement twice for manifold edges. */
    extrude_edge(backface.x);
#endif
  }
}
