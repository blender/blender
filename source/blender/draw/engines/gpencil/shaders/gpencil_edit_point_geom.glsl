uniform vec2 Viewport;

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in vec4 finalColor[1];
in float finalThickness[1];

out vec4 mColor;
out vec2 mTexCoord;

/* project 3d point to 2d on screen space */
vec2 toScreenSpace(vec4 vertex)
{
  return vec2(vertex.xy / vertex.w) * Viewport;
}

/* get zdepth value */
float getZdepth(vec4 point)
{
  return min(-0.05, (point.z / point.w));
}

void main(void)
{
  vec4 P0 = gl_in[0].gl_Position;
  vec2 sp0 = toScreenSpace(P0);

  float size = finalThickness[0];

  /* generate the triangle strip */
  mTexCoord = vec2(0, 1);
  mColor = finalColor[0];
  gl_Position = vec4(vec2(sp0.x - size, sp0.y + size) / Viewport, getZdepth(P0), 1.0);
  EmitVertex();

  mTexCoord = vec2(0, 0);
  mColor = finalColor[0];
  gl_Position = vec4(vec2(sp0.x - size, sp0.y - size) / Viewport, getZdepth(P0), 1.0);
  EmitVertex();

  mTexCoord = vec2(1, 1);
  mColor = finalColor[0];
  gl_Position = vec4(vec2(sp0.x + size, sp0.y + size) / Viewport, getZdepth(P0), 1.0);
  EmitVertex();

  mTexCoord = vec2(1, 0);
  mColor = finalColor[0];
  gl_Position = vec4(vec2(sp0.x + size, sp0.y - size) / Viewport, getZdepth(P0), 1.0);
  EmitVertex();

  EndPrimitive();
}
