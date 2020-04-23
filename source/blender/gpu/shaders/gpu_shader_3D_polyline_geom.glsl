
layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

uniform vec4 color;
uniform vec2 viewportSize;
uniform float lineWidth;

#if !defined(UNIFORM)
in vec4 finalColor_g[];
#endif

#ifdef CLIP
in float clip_g[];
out float clip;
#endif

out vec4 finalColor;
noperspective out float smoothline;

#define SMOOTH_WIDTH 1.0

void do_vertex(const int i, vec2 ofs)
{
#if defined(UNIFORM)
  finalColor = color;

#elif defined(FLAT)
  finalColor = finalColor_g[0];

#elif defined(SMOOTH)
  finalColor = finalColor_g[i];
#endif

#ifdef CLIP
  clip = clip_g[i];
#endif

  smoothline = (lineWidth + SMOOTH_WIDTH) * 0.5;
  gl_Position = gl_in[i].gl_Position;
  gl_Position.xy += ofs * gl_Position.w;
  EmitVertex();

  smoothline = -(lineWidth + SMOOTH_WIDTH) * 0.5;
  gl_Position = gl_in[i].gl_Position;
  gl_Position.xy -= ofs * gl_Position.w;
  EmitVertex();
}

void main(void)
{
  vec2 p0 = gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
  vec2 p1 = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;
  vec2 e = normalize((p1 - p0) * viewportSize.xy);
#if 0 /* Hard turn when line direction changes quadrant. */
  e = abs(e);
  vec2 ofs = (e.x > e.y) ? vec2(0.0, 1.0 / e.x) : vec2(1.0 / e.y, 0.0);
#else /* Use perpendicular direction. */
  vec2 ofs = vec2(-e.y, e.x);
#endif
  ofs /= viewportSize.xy;
  ofs *= lineWidth + SMOOTH_WIDTH;

  do_vertex(0, ofs);
  do_vertex(1, ofs);

  EndPrimitive();
}
