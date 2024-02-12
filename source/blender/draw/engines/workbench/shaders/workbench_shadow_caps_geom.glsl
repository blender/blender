/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

vec4 get_pos(int v, bool backface)
{
  return (backface) ? vData[v].backPosition : vData[v].frontPosition;
}

void emit_cap(const bool front, bool reversed)
{
  if (front) {
    gl_Position = vData[0].frontPosition;
    gpu_EmitVertex();
    gl_Position = vData[reversed ? 2 : 1].frontPosition;
    gpu_EmitVertex();
    gl_Position = vData[reversed ? 1 : 2].frontPosition;
    gpu_EmitVertex();
  }
  else {
    gl_Position = vData[0].backPosition;
    gpu_EmitVertex();
    gl_Position = vData[reversed ? 1 : 2].backPosition;
    gpu_EmitVertex();
    gl_Position = vData[reversed ? 2 : 1].backPosition;
    gpu_EmitVertex();
  }
  EndPrimitive();
}

void main()
{
  vec3 v10 = vData[0].pos - vData[1].pos;
  vec3 v12 = vData[2].pos - vData[1].pos;

  vec3 n = cross(v12, v10);

  float facing = dot(n, vData_flat[0].light_direction_os);

  bool backface = facing > 0.0;

#ifdef DOUBLE_MANIFOLD
  /* In case of non manifold geom, we only increase/decrease
   * the stencil buffer by one but do every faces as they were facing the light. */
  bool invert = backface;
  const bool is_manifold = false;
#else
  const bool invert = false;
  const bool is_manifold = true;
#endif

  if (!is_manifold || !backface) {
    bool do_front = (gl_InvocationID & 1) == 0;
    emit_cap(do_front, invert);
  }
}
