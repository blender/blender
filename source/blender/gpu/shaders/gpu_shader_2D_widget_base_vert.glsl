/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_widget_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_2D_widget_base)

#define recti parameters[widgetID * MAX_PARAM + 0]
#define rect parameters[widgetID * MAX_PARAM + 1]
#define radsi parameters[widgetID * MAX_PARAM + 2].x
#define rads parameters[widgetID * MAX_PARAM + 2].y
#define faci parameters[widgetID * MAX_PARAM + 2].zw
#define roundCorners parameters[widgetID * MAX_PARAM + 3]
#define colorInner1 parameters[widgetID * MAX_PARAM + 4]
#define colorInner2 parameters[widgetID * MAX_PARAM + 5]
#define colorEdge parameters[widgetID * MAX_PARAM + 6]
#define colorEmboss parameters[widgetID * MAX_PARAM + 7]
#define colorTria parameters[widgetID * MAX_PARAM + 8]
#define tria1Center parameters[widgetID * MAX_PARAM + 9].xy
#define tria2Center parameters[widgetID * MAX_PARAM + 9].zw
#define tria1Size parameters[widgetID * MAX_PARAM + 10].x
#define tria2Size parameters[widgetID * MAX_PARAM + 10].y
#define shadeDir parameters[widgetID * MAX_PARAM + 10].z
#define alphaDiscard parameters[widgetID * MAX_PARAM + 10].w
#define triaType parameters[widgetID * MAX_PARAM + 11].x

/* We encode alpha check and discard factor together. */
#define doAlphaCheck (alphaDiscard < 0.0f)
#define discardFactor abs(alphaDiscard)

float2 do_widget()
{
  /* Offset to avoid losing pixels (mimics conservative rasterization). */
  constexpr float2 ofs = float2(0.5f, -0.5f);
  lineWidth = abs(rect.x - recti.x);
  float2 emboss_ofs = float2(0.0f, -lineWidth);

  float2 pos;
  switch (gl_VertexID) {
    default:
    case 0: {
      pos = rect.xz + emboss_ofs + ofs.yy;
      break;
    }
    case 1: {
      pos = rect.xw + ofs.yx;
      break;
    }
    case 2: {
      pos = rect.yz + emboss_ofs + ofs.xy;
      break;
    }
    case 3: {
      pos = rect.yw + ofs.xx;
      break;
    }
  }

  uvInterp = pos - rect.xz;
  outRectSize = rect.yw - rect.xz;
  outRoundCorners = rads * roundCorners;

  float2 uv = uvInterp / outRectSize;
  float fac = clamp((shadeDir > 0.0f) ? uv.y : uv.x, 0.0f, 1.0f);
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
  borderColor = colorEdge * float4(colorEdge.aaa, 1.0f);
  embossColor = colorEmboss * float4(colorEmboss.aaa, 1.0f);

  return pos;
}

float2 do_tria()
{
  int vidx = gl_VertexID % 4;
  bool tria2 = gl_VertexID > 7;

  float2 pos = float2(0.0f);
  float size = (tria2) ? -tria2Size : tria1Size;
  float2 center = (tria2) ? tria2Center : tria1Center;

  float2 arrow_pos[4] = float2_array(
      float2(0.0f, 0.6f), float2(0.6f, 0.0f), float2(-0.6f, 0.0f), float2(0.0f, -0.6f));
  /* Rotated uv space by 45deg and mirrored. */
  float2 arrow_uvs[4] = float2_array(
      float2(0.0f, 0.85f), float2(0.85f, 0.85f), float2(0.0f, 0.0f), float2(0.0f, 0.85f));

  float2 point_pos[4] = float2_array(
      float2(-1.0f, -1.0f), float2(-1.0f, 1.0f), float2(1.0f, -1.0f), float2(1.0f, 1.0f));
  float2 point_uvs[4] = float2_array(
      float2(0.0f, 0.0f), float2(0.0f, 1.0f), float2(1.0f, 0.0f), float2(1.0f, 1.0f));

  /* We reuse the SDF round-box rendering of widget to render the tria shapes.
   * This means we do clever tricks to position the rectangle the way we want using
   * the 2 triangles uvs. */
  if (triaType == 0.0f) {
    /* ROUNDBOX_TRIA_NONE */
    outRectSize = uvInterp = pos = float2(0);
    outRoundCorners = float4(0.01f);
  }
  else if (triaType == 1.0f) {
    /* ROUNDBOX_TRIA_ARROWS */
    pos = arrow_pos[vidx];
    uvInterp = arrow_uvs[vidx];
    uvInterp -= float2(0.05f, 0.63f); /* Translate */
    outRectSize = float2(0.74f, 0.17f);
    outRoundCorners = float4(0.08f);
  }
  else if (triaType == 2.0f) {
    /* ROUNDBOX_TRIA_SCROLL */
    pos = point_pos[vidx];
    uvInterp = point_uvs[vidx];
    outRectSize = float2(1.0f);
    outRoundCorners = float4(0.5f);
  }
  else if (triaType == 3.0f) {
    /* ROUNDBOX_TRIA_MENU */
    pos = tria2 ? float2(0.0f) : arrow_pos[vidx]; /* Solo tria */
    pos = float2(pos.y, -pos.x);                  /* Rotate */
    pos += float2(-0.05f, 0.0f);                  /* Translate */
    size *= 0.8f;                                 /* Scale */
    uvInterp = arrow_uvs[vidx];
    uvInterp -= float2(0.05f, 0.63f); /* Translate */
    outRectSize = float2(0.74f, 0.17f);
    outRoundCorners = float4(0.01f);
  }
  else if (triaType == 4.0f) {
    /* ROUNDBOX_TRIA_CHECK */
    /* A bit more hacky: We use the two triangles joined together to render
     * both sides of the check-mark with different length. */
    pos = arrow_pos[min(vidx, 2)];  /* Only keep 1 triangle. */
    pos.y = tria2 ? -pos.y : pos.y; /* Mirror along X */
    pos = pos.x * float2(0.0872f, -0.996f) + pos.y * float2(0.996f, 0.0872f); /* Rotate (85deg) */
    pos += float2(-0.1f, 0.2f);                                               /* Translate */
    center = tria1Center;
    size = tria1Size * 1.7f; /* Scale */
    uvInterp = arrow_uvs[vidx];
    uvInterp -= tria2 ? float2(0.4f, 0.65f) : float2(0.08f, 0.65f); /* Translate */
    outRectSize = float2(0.74f, 0.14f);
    outRoundCorners = float4(0.01f);
  }
  else if (triaType == 5.0f) {
    /* ROUNDBOX_TRIA_HOLD_ACTION_ARROW */
    /* We use a single triangle to cut the round rect in half.
     * The edge will not be Anti-aliased. */
    pos = tria2 ? float2(0.0f) : arrow_pos[min(vidx, 2)]; /* Only keep 1 triangle. */
    pos = pos.x * float2(0.707f, 0.707f) + pos.y * float2(-0.707f, 0.707f); /* Rotate (45deg) */
    pos += float2(-1.7f, 2.4f); /* Translate (hard-coded, might want to remove). */
    size *= 0.4f;               /* Scale */
    uvInterp = arrow_uvs[vidx];
    uvInterp -= float2(0.05f, 0.05f); /* Translate */
    outRectSize = float2(0.75f);
    outRoundCorners = float4(0.01f);
  }
  else if (triaType == 6.0f) {
    /* ROUNDBOX_TRIA_DASH */
    pos = point_pos[vidx];
    uvInterp = point_uvs[vidx];
    uvInterp -= float2(0.2f, 0.45f); /* Translate */
    outRectSize = float2(0.6f, 0.1f);
    outRoundCorners = float4(0.01f);
  }

  uvInterp *= abs(size);
  outRectSize *= abs(size);
  outRoundCorners *= abs(size);

  pos = pos * size + center;

  innerColor = colorTria * float4(colorTria.aaa, 1.0f);

  lineWidth = 0.0f;
  borderColor = float4(0.0f);
  embossColor = float4(0.0f);

  butCo = -2.0f;

  return pos;
}

void main()
{
  discardFac = discardFactor;
  bool is_tria = (gl_VertexID > 3);
  float2 pos = (is_tria) ? do_tria() : do_widget();

  gl_Position = ModelViewProjectionMatrix * float4(pos, 0.0f, 1.0f);
}
