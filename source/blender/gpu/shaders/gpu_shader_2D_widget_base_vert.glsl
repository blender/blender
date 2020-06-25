
uniform mat4 ModelViewProjectionMatrix;

#define MAX_PARAM 12
#ifdef USE_INSTANCE
#  define MAX_INSTANCE 6
uniform vec4 parameters[MAX_PARAM * MAX_INSTANCE];
#else
uniform vec4 parameters[MAX_PARAM];
#endif

/* gl_InstanceID is 0 if not drawing instances. */
#define recti parameters[gl_InstanceID * MAX_PARAM + 0]
#define rect parameters[gl_InstanceID * MAX_PARAM + 1]
#define radsi parameters[gl_InstanceID * MAX_PARAM + 2].x
#define rads parameters[gl_InstanceID * MAX_PARAM + 2].y
#define faci parameters[gl_InstanceID * MAX_PARAM + 2].zw
#define roundCorners parameters[gl_InstanceID * MAX_PARAM + 3]
#define colorInner1 parameters[gl_InstanceID * MAX_PARAM + 4]
#define colorInner2 parameters[gl_InstanceID * MAX_PARAM + 5]
#define colorEdge parameters[gl_InstanceID * MAX_PARAM + 6]
#define colorEmboss parameters[gl_InstanceID * MAX_PARAM + 7]
#define colorTria parameters[gl_InstanceID * MAX_PARAM + 8]
#define tria1Center parameters[gl_InstanceID * MAX_PARAM + 9].xy
#define tria2Center parameters[gl_InstanceID * MAX_PARAM + 9].zw
#define tria1Size parameters[gl_InstanceID * MAX_PARAM + 10].x
#define tria2Size parameters[gl_InstanceID * MAX_PARAM + 10].y
#define shadeDir parameters[gl_InstanceID * MAX_PARAM + 10].z
#define alphaDiscard parameters[gl_InstanceID * MAX_PARAM + 10].w
#define triaType parameters[gl_InstanceID * MAX_PARAM + 11].x

/* We encode alpha check and discard factor together. */
#define doAlphaCheck (alphaDiscard < 0.0)
#define discardFactor abs(alphaDiscard)

noperspective out vec2 uvInterp;
flat out vec2 outRectSize;
flat out vec4 outRoundCorners;
noperspective out vec4 innerColor;
flat out vec4 borderColor;
flat out vec4 embossColor;
flat out float lineWidth;
noperspective out float butCo;
flat out float discardFac;

vec2 do_widget(void)
{
  lineWidth = abs(rect.x - recti.x);
  vec2 emboss_ofs = vec2(0.0, -lineWidth);
  vec2 v_pos[4] = vec2[4](rect.xz + emboss_ofs, rect.xw, rect.yz + emboss_ofs, rect.yw);
  vec2 pos = v_pos[gl_VertexID];

  uvInterp = pos - rect.xz;
  outRectSize = rect.yw - rect.xz;
  outRoundCorners = rads * roundCorners;

  vec2 uv = uvInterp / outRectSize;
  float fac = clamp((shadeDir > 0.0) ? uv.y : uv.x, 0.0, 1.0);
  /* Note innerColor is premultiplied inside the fragment shader. */
  if (doAlphaCheck) {
    innerColor = colorInner1;
    butCo = uv.x;
  }
  else {
    innerColor = mix(colorInner2, colorInner1, fac);
    butCo = -abs(uv.x);
  }

  /* We need premultiplied color for transparency. */
  borderColor = colorEdge * vec4(colorEdge.aaa, 1.0);
  embossColor = colorEmboss * vec4(colorEmboss.aaa, 1.0);

  return pos;
}

vec2 do_tria()
{
  int vidx = gl_VertexID % 4;
  bool tria2 = gl_VertexID > 7;

  vec2 pos;
  float size = (tria2) ? -tria2Size : tria1Size;
  vec2 center = (tria2) ? tria2Center : tria1Center;

  vec2 arrow_pos[4] = vec2[4](vec2(0.0, 0.6), vec2(0.6, 0.0), vec2(-0.6, 0.0), vec2(0.0, -0.6));
  /* Rotated uv space by 45deg and mirrored. */
  vec2 arrow_uvs[4] = vec2[4](vec2(0.0, 0.85), vec2(0.85, 0.85), vec2(0.0, 0.0), vec2(0.0, 0.85));

  vec2 point_pos[4] = vec2[4](vec2(-1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0));
  vec2 point_uvs[4] = vec2[4](vec2(0.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0));

  /* We reuse the SDF roundbox rendering of widget to render the tria shapes.
   * This means we do clever tricks to position the rectangle the way we want using
   * the 2 triangles uvs. */
  if (triaType == 0.0) {
    /* ROUNDBOX_TRIA_NONE */
    outRectSize = uvInterp = pos = vec2(0);
    outRoundCorners = vec4(0.01);
  }
  else if (triaType == 1.0) {
    /* ROUNDBOX_TRIA_ARROWS */
    pos = arrow_pos[vidx];
    uvInterp = arrow_uvs[vidx];
    uvInterp -= vec2(0.05, 0.63); /* Translate */
    outRectSize = vec2(0.74, 0.17);
    outRoundCorners = vec4(0.08);
  }
  else if (triaType == 2.0) {
    /* ROUNDBOX_TRIA_SCROLL */
    pos = point_pos[vidx];
    uvInterp = point_uvs[vidx];
    outRectSize = vec2(1.0);
    outRoundCorners = vec4(0.5);
  }
  else if (triaType == 3.0) {
    /* ROUNDBOX_TRIA_MENU */
    pos = tria2 ? vec2(0.0) : arrow_pos[vidx]; /* Solo tria */
    pos = vec2(pos.y, -pos.x);                 /* Rotate */
    pos += vec2(-0.05, 0.0);                   /* Translate */
    size *= 0.8;                               /* Scale */
    uvInterp = arrow_uvs[vidx];
    uvInterp -= vec2(0.05, 0.63); /* Translate */
    outRectSize = vec2(0.74, 0.17);
    outRoundCorners = vec4(0.01);
  }
  else if (triaType == 4.0) {
    /* ROUNDBOX_TRIA_CHECK */
    /* A bit more hacky: We use the two trias joined together to render
     * both sides of the checkmark with different length. */
    pos = arrow_pos[min(vidx, 2)];                                    /* Only keep 1 triangle. */
    pos.y = tria2 ? -pos.y : pos.y;                                   /* Mirror along X */
    pos = pos.x * vec2(0.0872, -0.996) + pos.y * vec2(0.996, 0.0872); /* Rotate (85deg) */
    pos += vec2(-0.1, 0.2);                                           /* Translate */
    center = tria1Center;
    size = tria1Size * 1.7; /* Scale */
    uvInterp = arrow_uvs[vidx];
    uvInterp -= tria2 ? vec2(0.4, 0.65) : vec2(0.08, 0.65); /* Translate */
    outRectSize = vec2(0.74, 0.14);
    outRoundCorners = vec4(0.01);
  }
  else {
    /* ROUNDBOX_TRIA_HOLD_ACTION_ARROW */
    /* We use a single triangle to cut the round rect in half. The edge will not be Antialiased. */
    pos = tria2 ? vec2(0.0) : arrow_pos[min(vidx, 2)];              /* Only keep 1 triangle. */
    pos = pos.x * vec2(0.707, 0.707) + pos.y * vec2(-0.707, 0.707); /* Rotate (45deg) */
    pos += vec2(-1.7, 2.4); /* Translate  (hardcoded, might want to remove) */
    size *= 0.4;            /* Scale */
    uvInterp = arrow_uvs[vidx];
    uvInterp -= vec2(0.05, 0.05); /* Translate */
    outRectSize = vec2(0.75);
    outRoundCorners = vec4(0.01);
  }

  uvInterp *= abs(size);
  outRectSize *= abs(size);
  outRoundCorners *= abs(size);

  pos = pos * size + center;

  innerColor = colorTria * vec4(colorTria.aaa, 1.0);

  lineWidth = 0.0;
  borderColor = vec4(0.0);
  embossColor = vec4(0.0);

  butCo = -2.0;

  return pos;
}

void main()
{
  discardFac = discardFactor;
  bool is_tria = (gl_VertexID > 3);
  vec2 pos = (is_tria) ? do_tria() : do_widget();

  gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);
}
