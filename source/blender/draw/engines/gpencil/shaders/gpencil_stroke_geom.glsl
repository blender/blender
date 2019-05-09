uniform vec2 Viewport;
uniform int xraymode;
uniform int color_type;
uniform int caps_mode[2];

layout(lines_adjacency) in;
layout(triangle_strip, max_vertices = 13) out;

in vec4 finalColor[4];
in float finalThickness[4];
in vec2 finaluvdata[4];

out vec4 mColor;
out vec2 mTexCoord;
out vec2 uvfac;

#define GP_XRAY_FRONT 0
#define GP_XRAY_3DSPACE 1

/* keep this list synchronized with list in gpencil_engine.h */
#define GPENCIL_COLOR_SOLID 0
#define GPENCIL_COLOR_TEXTURE 1
#define GPENCIL_COLOR_PATTERN 2

#define GPENCIL_FLATCAP 1

/* project 3d point to 2d on screen space */
vec2 toScreenSpace(vec4 vertex)
{
  return vec2(vertex.xy / vertex.w) * Viewport;
}

/* get zdepth value */
float getZdepth(vec4 point)
{
  if (xraymode == GP_XRAY_FRONT) {
    return min(-0.05, (point.z / point.w));
  }
  if (xraymode == GP_XRAY_3DSPACE) {
    return (point.z / point.w);
  }

  /* in front by default */
  return 0.000001;
}

/* check equality but with a small tolerance */
bool is_equal(vec4 p1, vec4 p2)
{
  float limit = 0.0001;
  float x = abs(p1.x - p2.x);
  float y = abs(p1.y - p2.y);
  float z = abs(p1.z - p2.z);

  if ((x < limit) && (y < limit) && (z < limit)) {
    return true;
  }

  return false;
}

void main(void)
{
  float MiterLimit = 0.75;
  uvfac = vec2(0.0, 0.0);

  /* receive 4 points */
  vec4 P0 = gl_in[0].gl_Position;
  vec4 P1 = gl_in[1].gl_Position;
  vec4 P2 = gl_in[2].gl_Position;
  vec4 P3 = gl_in[3].gl_Position;

  /* get the four vertices passed to the shader */
  vec2 sp0 = toScreenSpace(P0);  // start of previous segment
  vec2 sp1 = toScreenSpace(P1);  // end of previous segment, start of current segment
  vec2 sp2 = toScreenSpace(P2);  // end of current segment, start of next segment
  vec2 sp3 = toScreenSpace(P3);  // end of next segment

  /* culling outside viewport */
  vec2 area = Viewport * 4.0;
  if (sp1.x < -area.x || sp1.x > area.x)
    return;
  if (sp1.y < -area.y || sp1.y > area.y)
    return;
  if (sp2.x < -area.x || sp2.x > area.x)
    return;
  if (sp2.y < -area.y || sp2.y > area.y)
    return;

  /* culling behind camera */
  if (P1.w < 0 || P2.w < 0)
    return;

  /* determine the direction of each of the 3 segments (previous, current, next) */
  vec2 v0 = normalize(sp1 - sp0);
  vec2 v1 = normalize(sp2 - sp1);
  vec2 v2 = normalize(sp3 - sp2);

  /* determine the normal of each of the 3 segments (previous, current, next) */
  vec2 n0 = vec2(-v0.y, v0.x);
  vec2 n1 = vec2(-v1.y, v1.x);
  vec2 n2 = vec2(-v2.y, v2.x);

  /* determine miter lines by averaging the normals of the 2 segments */
  vec2 miter_a = normalize(n0 + n1);  // miter at start of current segment
  vec2 miter_b = normalize(n1 + n2);  // miter at end of current segment

  /* determine the length of the miter by projecting it onto normal and then inverse it */
  float an1 = dot(miter_a, n1);
  float bn1 = dot(miter_b, n2);
  if (an1 == 0)
    an1 = 1;
  if (bn1 == 0)
    bn1 = 1;
  float length_a = finalThickness[1] / an1;
  float length_b = finalThickness[2] / bn1;
  if (length_a <= 0.0)
    length_a = 0.01;
  if (length_b <= 0.0)
    length_b = 0.01;

  /* prevent excessively long miters at sharp corners */
  if (dot(v0, v1) < -MiterLimit) {
    miter_a = n1;
    length_a = finalThickness[1];

    /* close the gap */
    if (dot(v0, n1) > 0) {
      mTexCoord = vec2(0, 0);
      mColor = finalColor[1];
      gl_Position = vec4((sp1 + finalThickness[1] * n0) / Viewport, getZdepth(P1), 1.0);
      EmitVertex();

      mTexCoord = vec2(0, 0);
      mColor = finalColor[1];
      gl_Position = vec4((sp1 + finalThickness[1] * n1) / Viewport, getZdepth(P1), 1.0);
      EmitVertex();

      mTexCoord = vec2(0, 0.5);
      mColor = finalColor[1];
      gl_Position = vec4(sp1 / Viewport, getZdepth(P1), 1.0);
      EmitVertex();

      EndPrimitive();
    }
    else {
      mTexCoord = vec2(0, 1);
      mColor = finalColor[1];
      gl_Position = vec4((sp1 - finalThickness[1] * n1) / Viewport, getZdepth(P1), 1.0);
      EmitVertex();

      mTexCoord = vec2(0, 1);
      mColor = finalColor[1];
      gl_Position = vec4((sp1 - finalThickness[1] * n0) / Viewport, getZdepth(P1), 1.0);
      EmitVertex();

      mTexCoord = vec2(0, 0.5);
      mColor = finalColor[1];
      gl_Position = vec4(sp1 / Viewport, getZdepth(P1), 1.0);
      EmitVertex();

      EndPrimitive();
    }
  }

  if (dot(v1, v2) < -MiterLimit) {
    miter_b = n1;
    length_b = finalThickness[2];
  }

  /* generate the start endcap */
  if ((caps_mode[0] != GPENCIL_FLATCAP) && is_equal(P0, P2)) {
    vec4 cap_color = finalColor[1];

    mTexCoord = vec2(2.0, 0.5);
    mColor = cap_color;
    vec2 svn1 = normalize(sp1 - sp2) * length_a * 4.0;
    uvfac = vec2(0.0, 1.0);
    gl_Position = vec4((sp1 + svn1) / Viewport, getZdepth(P1), 1.0);
    EmitVertex();

    mTexCoord = vec2(0.0, -0.5);
    mColor = cap_color;
    uvfac = vec2(0.0, 1.0);
    gl_Position = vec4((sp1 - (length_a * 2.0) * miter_a) / Viewport, getZdepth(P1), 1.0);
    EmitVertex();

    mTexCoord = vec2(0.0, 1.5);
    mColor = cap_color;
    uvfac = vec2(0.0, 1.0);
    gl_Position = vec4((sp1 + (length_a * 2.0) * miter_a) / Viewport, getZdepth(P1), 1.0);
    EmitVertex();
  }

  float y_a = 0.0;
  float y_b = 1.0;

  /* invert uv (vertical) */
  if (finaluvdata[2].x > 1.0) {
    if ((finaluvdata[1].y != 0.0) && (finaluvdata[2].y != 0.0)) {
      float d = ceil(finaluvdata[2].x) - 1.0;
      if (floor(d / 2.0) == (d / 2.0)) {
        y_a = 1.0;
        y_b = 0.0;
      }
    }
  }
  /* generate the triangle strip */
  uvfac = vec2(0.0, 0.0);
  mTexCoord = (color_type == GPENCIL_COLOR_SOLID) ? vec2(0, 0) : vec2(finaluvdata[1].x, y_a);
  mColor = finalColor[1];
  gl_Position = vec4((sp1 + length_a * miter_a) / Viewport, getZdepth(P1), 1.0);
  EmitVertex();

  mTexCoord = (color_type == GPENCIL_COLOR_SOLID) ? vec2(0, 1) : vec2(finaluvdata[1].x, y_b);
  mColor = finalColor[1];
  gl_Position = vec4((sp1 - length_a * miter_a) / Viewport, getZdepth(P1), 1.0);
  EmitVertex();

  mTexCoord = (color_type == GPENCIL_COLOR_SOLID) ? vec2(1, 0) : vec2(finaluvdata[2].x, y_a);
  mColor = finalColor[2];
  gl_Position = vec4((sp2 + length_b * miter_b) / Viewport, getZdepth(P2), 1.0);
  EmitVertex();

  mTexCoord = (color_type == GPENCIL_COLOR_SOLID) ? vec2(1, 1) : vec2(finaluvdata[2].x, y_b);
  mColor = finalColor[2];
  gl_Position = vec4((sp2 - length_b * miter_b) / Viewport, getZdepth(P2), 1.0);
  EmitVertex();

  /* generate the end endcap */
  if ((caps_mode[1] != GPENCIL_FLATCAP) && is_equal(P1, P3) && (finaluvdata[2].x > 0)) {
    vec4 cap_color = finalColor[2];

    mTexCoord = vec2(finaluvdata[2].x, 1.5);
    mColor = cap_color;
    uvfac = vec2(finaluvdata[2].x, 1.0);
    gl_Position = vec4((sp2 + (length_b * 2.0) * miter_b) / Viewport, getZdepth(P2), 1.0);
    EmitVertex();

    mTexCoord = vec2(finaluvdata[2].x, -0.5);
    mColor = cap_color;
    uvfac = vec2(finaluvdata[2].x, 1.0);
    gl_Position = vec4((sp2 - (length_b * 2.0) * miter_b) / Viewport, getZdepth(P2), 1.0);
    EmitVertex();

    mTexCoord = vec2(finaluvdata[2].x + 2, 0.5);
    mColor = cap_color;
    uvfac = vec2(finaluvdata[2].x, 1.0);
    vec2 svn2 = normalize(sp2 - sp1) * length_b * 4.0;
    gl_Position = vec4((sp2 + svn2) / Viewport, getZdepth(P2), 1.0);
    EmitVertex();
  }

  EndPrimitive();
}
