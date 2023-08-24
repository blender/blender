/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Clips point to near clip plane before perspective divide. */
vec4 clip_line_point_homogeneous_space(vec4 p, vec4 q)
{
  if (p.z < -p.w) {
    /* Just solves p + (q - p) * A; for A when p.z / p.w = -1.0. */
    float denom = q.z - p.z + q.w - p.w;
    if (denom == 0.0) {
      /* No solution. */
      return p;
    }
    float A = (-p.z - p.w) / denom;
    p = p + (q - p) * A;
  }
  return p;
}

void do_vertex(const int i, vec4 pos, vec2 ofs)
{
#if defined(UNIFORM)
  interp_out.final_color = color;

#elif defined(FLAT)
  /* WATCH: Assuming last provoking vertex. */
  interp_out.final_color = interp_in[1].final_color;

#elif defined(SMOOTH)
  interp_out.final_color = interp_in[i].final_color;
#endif

#ifdef CLIP
  interp_out.clip = interp_in[i].clip;
#endif

  interp_noperspective_out.smoothline = (lineWidth + SMOOTH_WIDTH * float(lineSmooth)) * 0.5;
  gl_Position = pos;
  gl_Position.xy += ofs * pos.w;
  EmitVertex();

  interp_noperspective_out.smoothline = -(lineWidth + SMOOTH_WIDTH * float(lineSmooth)) * 0.5;
  gl_Position = pos;
  gl_Position.xy -= ofs * pos.w;
  EmitVertex();
}

void main(void)
{
  vec4 p0 = clip_line_point_homogeneous_space(gl_in[0].gl_Position, gl_in[1].gl_Position);
  vec4 p1 = clip_line_point_homogeneous_space(gl_in[1].gl_Position, gl_in[0].gl_Position);
  vec2 e = normalize(((p1.xy / p1.w) - (p0.xy / p0.w)) * viewportSize.xy);

#if 0 /* Hard turn when line direction changes quadrant. */
  e = abs(e);
  vec2 ofs = (e.x > e.y) ? vec2(0.0, 1.0 / e.x) : vec2(1.0 / e.y, 0.0);
#else /* Use perpendicular direction. */
  vec2 ofs = vec2(-e.y, e.x);
#endif
  ofs /= viewportSize.xy;
  ofs *= lineWidth + SMOOTH_WIDTH * float(lineSmooth);

  do_vertex(0, p0, ofs);
  do_vertex(1, p1, ofs);

  EndPrimitive();
}
