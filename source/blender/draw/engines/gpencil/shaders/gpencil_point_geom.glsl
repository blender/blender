uniform mat4 ModelViewProjectionMatrix;
uniform vec2 Viewport;
uniform int xraymode;
uniform int use_follow_path;

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in vec4 finalColor[1];
in float finalThickness[1];
in vec2 finaluvdata[1];
in vec4 finalprev_pos[1];

out vec4 mColor;
out vec2 mTexCoord;

#define GP_XRAY_FRONT 0
#define GP_XRAY_3DSPACE 1

#define M_PI 3.14159265358979323846  /* pi */
#define M_2PI 6.28318530717958647692 /* 2*pi */
#define FALSE 0

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

vec2 rotateUV(vec2 uv, float angle)
{
  /* translate center of rotation to the center of texture */
  vec2 new_uv = uv - vec2(0.5f, 0.5f);
  vec2 rot_uv;
  rot_uv.x = new_uv.x * cos(angle) - new_uv.y * sin(angle);
  rot_uv.y = new_uv.y * cos(angle) + new_uv.x * sin(angle);
  return rot_uv + vec2(0.5f, 0.5f);
}

vec2 rotatePoint(vec2 center, vec2 point, float angle)
{
  /* translate center of rotation to the center */
  vec2 new_point = point - center;
  vec2 rot_point;
  rot_point.x = new_point.x * cos(angle) - new_point.y * sin(angle);
  rot_point.y = new_point.y * cos(angle) + new_point.x * sin(angle);
  return rot_point + center;
}

/* Calculate angle of the stroke using previous point as reference.
 * The angle is calculated using the x axis (1, 0) as 0 degrees */
float getAngle(vec2 pt0, vec2 pt1)
{
  /* do not rotate one point only (no reference to rotate) */
  if (pt0 == pt1) {
    return 0.0;
  }

  if (use_follow_path == FALSE) {
    return 0.0;
  }

  /* default horizontal line (x-axis) in screen space */
  vec2 v0 = vec2(1.0, 0.0);

  /* vector of direction */
  vec2 vn = vec2(normalize(pt1 - pt0));

  /* angle signed (function ported from angle_signed_v2v2) */
  float perp_dot = (v0[1] * vn[0]) - (v0[0] * vn[1]);
  float angle = atan(perp_dot, dot(v0, vn));

  /* get full circle rotation */
  if (angle > 0.0) {
    angle = M_PI + (M_PI - angle);
  }
  else {
    angle *= -1.0;
  }

  return angle;
}

void main(void)
{
  /* receive points */
  vec4 P0 = gl_in[0].gl_Position;
  vec2 sp0 = toScreenSpace(P0);

  vec4 P1 = finalprev_pos[0];
  vec2 sp1 = toScreenSpace(P1);
  vec2 point;

  float size = finalThickness[0];
  vec2 center = vec2(sp0.x, sp0.y);

  /* get angle of stroke to rotate texture */
  float angle = getAngle(sp0, sp1);

  /* generate the triangle strip */
  mTexCoord = rotateUV(vec2(0, 1), finaluvdata[0].y);
  mColor = finalColor[0];
  point = rotatePoint(center, vec2(sp0.x - size, sp0.y + size), angle);
  gl_Position = vec4(point / Viewport, getZdepth(P0), 1.0);
  EmitVertex();

  mTexCoord = rotateUV(vec2(0, 0), finaluvdata[0].y);
  mColor = finalColor[0];
  point = rotatePoint(center, vec2(sp0.x - size, sp0.y - size), angle);
  gl_Position = vec4(point / Viewport, getZdepth(P0), 1.0);
  EmitVertex();

  mTexCoord = rotateUV(vec2(1, 1), finaluvdata[0].y);
  mColor = finalColor[0];
  point = rotatePoint(center, vec2(sp0.x + size, sp0.y + size), angle);
  gl_Position = vec4(point / Viewport, getZdepth(P0), 1.0);
  EmitVertex();

  mTexCoord = rotateUV(vec2(1, 0), finaluvdata[0].y);
  mColor = finalColor[0];
  point = rotatePoint(center, vec2(sp0.x + size, sp0.y - size), angle);
  gl_Position = vec4(point / Viewport, getZdepth(P0), 1.0);
  EmitVertex();

  EndPrimitive();
}
