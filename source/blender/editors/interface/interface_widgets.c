/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edinterface
 */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "DNA_brush_types.h"
#include "DNA_userdef_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "RNA_access.h"

#include "BLF_api.h"

#include "ED_node.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "interface_intern.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_platform.h"
#include "GPU_state.h"

#ifdef WITH_INPUT_IME
#  include "WM_types.h"
#endif

/* -------------------------------------------------------------------- */
/** \name Local Enums/Defines
 * \{ */

/* icons are 80% of height of button (16 pixels inside 20 height) */
#define ICON_SIZE_FROM_BUTRECT(rect) (0.8f * BLI_rcti_size_y(rect))

/* visual types for drawing */
/* for time being separated from functional types */
typedef enum {
  /* default */
  UI_WTYPE_REGULAR,

  /* standard set */
  UI_WTYPE_LABEL,
  UI_WTYPE_TOGGLE,
  UI_WTYPE_CHECKBOX,
  UI_WTYPE_RADIO,
  UI_WTYPE_NUMBER,
  UI_WTYPE_SLIDER,
  UI_WTYPE_EXEC,
  UI_WTYPE_TOOLBAR_ITEM,
  UI_WTYPE_TAB,
  UI_WTYPE_TOOLTIP,

  /* strings */
  UI_WTYPE_NAME,
  UI_WTYPE_NAME_LINK,
  UI_WTYPE_POINTER_LINK,
  UI_WTYPE_FILENAME,

  /* menus */
  UI_WTYPE_MENU_RADIO,
  UI_WTYPE_MENU_ICON_RADIO,
  UI_WTYPE_MENU_POINTER_LINK,
  UI_WTYPE_MENU_NODE_LINK,

  UI_WTYPE_PULLDOWN,
  UI_WTYPE_MENU_ITEM,
  UI_WTYPE_MENU_ITEM_RADIAL,
  UI_WTYPE_MENU_BACK,

  /* specials */
  UI_WTYPE_ICON,
  UI_WTYPE_ICON_LABEL,
  UI_WTYPE_SWATCH,
  UI_WTYPE_RGB_PICKER,
  UI_WTYPE_UNITVEC,
  UI_WTYPE_BOX,
  UI_WTYPE_SCROLL,
  UI_WTYPE_LISTITEM,
  UI_WTYPE_PROGRESSBAR,
  UI_WTYPE_NODESOCKET,
} uiWidgetTypeEnum;

/* Button state argument shares bits with 'uiBut.flag'.
 * reuse flags that aren't needed for drawing to avoid collision. */
enum {
  /* Show that holding the button opens a menu. */
  UI_STATE_HOLD_ACTION = UI_BUT_UPDATE_DELAY,
  UI_STATE_TEXT_INPUT = UI_BUT_UNDO,
  UI_STATE_ACTIVE_LEFT = UI_BUT_VALUE_CLEAR,
  UI_STATE_ACTIVE_RIGHT = UI_BUT_TEXTEDIT_UPDATE,
  UI_STATE_TEXT_BEFORE_WIDGET = UI_BUT_IMMEDIATE,

  UI_STATE_FLAGS_ALL = (UI_STATE_HOLD_ACTION | UI_STATE_TEXT_INPUT | UI_STATE_ACTIVE_LEFT |
                        UI_STATE_ACTIVE_RIGHT | UI_STATE_TEXT_BEFORE_WIDGET),
};
/* Prevent accidental use. */
#define UI_BUT_UPDATE_DELAY ((void)0)
#define UI_BUT_UNDO ((void)0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Color Utilities
 * \{ */

static void color_blend_v3_v3(uchar cp[3], const uchar cpstate[3], const float fac)
{
  if (fac != 0.0f) {
    cp[0] = (int)((1.0f - fac) * cp[0] + fac * cpstate[0]);
    cp[1] = (int)((1.0f - fac) * cp[1] + fac * cpstate[1]);
    cp[2] = (int)((1.0f - fac) * cp[2] + fac * cpstate[2]);
  }
}

static void color_blend_v4_v4v4(uchar r_col[4],
                                const uchar col1[4],
                                const uchar col2[4],
                                const float fac)
{
  const int faci = unit_float_to_uchar_clamp(fac);
  const int facm = 255 - faci;

  r_col[0] = (faci * col1[0] + facm * col2[0]) / 256;
  r_col[1] = (faci * col1[1] + facm * col2[1]) / 256;
  r_col[2] = (faci * col1[2] + facm * col2[2]) / 256;
  r_col[3] = (faci * col1[3] + facm * col2[3]) / 256;
}

static void color_add_v3_i(uchar cp[3], int tint)
{
  cp[0] = clamp_i(cp[0] + tint, 0, 255);
  cp[1] = clamp_i(cp[1] + tint, 0, 255);
  cp[2] = clamp_i(cp[2] + tint, 0, 255);
}

static void color_ensure_contrast_v3(uchar cp[3], const uchar cp_other[3], int contrast)
{
  BLI_assert(contrast > 0);
  const int item_value = rgb_to_grayscale_byte(cp);
  const int inner_value = rgb_to_grayscale_byte(cp_other);
  const int delta = item_value - inner_value;
  if (delta >= 0) {
    if (contrast > delta) {
      color_add_v3_i(cp, contrast - delta);
    }
  }
  else {
    if (contrast > -delta) {
      color_add_v3_i(cp, -contrast - delta);
    }
  }
}

static void color_mul_hsl_v3(uchar ch[3], float h_factor, float s_factor, float l_factor)
{
  float rgb[3], hsl[3];
  rgb_uchar_to_float(rgb, ch);
  rgb_to_hsl_v(rgb, hsl);
  hsl[0] *= h_factor;
  hsl[1] *= s_factor;
  hsl[2] *= l_factor;
  hsl_to_rgb_v(hsl, rgb);
  rgb_float_to_uchar(ch, rgb);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Widget Base Type
 * \{ */

/**
 * - in: roundbox codes for corner types and radius
 * - return: array of `[size][2][x, y]` points, the edges of the roundbox, + UV coords
 *
 * - draw black box with alpha 0 on exact button boundbox
 * - for every AA step:
 *    - draw the inner part for a round filled box, with color blend codes or texture coords
 *    - draw outline in outline color
 *    - draw outer part, bottom half, extruded 1 pixel to bottom, for emboss shadow
 *    - draw extra decorations
 * - draw background color box with alpha 1 on exact button boundbox
 */

/* fill this struct with polygon info to draw AA'ed */
/* it has outline, back, and two optional tria meshes */

typedef struct uiWidgetTrias {
  uint tot;
  int type;
  float size, center[2];

  float vec[16][2];
  const uint (*index)[3];

} uiWidgetTrias;

/* max as used by round_box__edges */
/* Make sure to change widget_base_vert.glsl accordingly. */
#define WIDGET_CURVE_RESOLU 9
#define WIDGET_SIZE_MAX (WIDGET_CURVE_RESOLU * 4)

typedef struct uiWidgetBase {
  /* TODO remove these completely */
  int totvert, halfwayvert;
  float outer_v[WIDGET_SIZE_MAX][2];
  float inner_v[WIDGET_SIZE_MAX][2];
  float inner_uv[WIDGET_SIZE_MAX][2];

  bool draw_inner, draw_outline, draw_emboss;

  uiWidgetTrias tria1;
  uiWidgetTrias tria2;

  /* Widget shader parameters, must match the shader layout. */
  uiWidgetBaseParameters uniform_params;
} uiWidgetBase;

/**
 * For time being only for visual appearance,
 * later, a handling callback can be added too.
 */
typedef struct uiWidgetType {

  /* pointer to theme color definition */
  const uiWidgetColors *wcol_theme;
  uiWidgetStateColors *wcol_state;

  /* converted colors for state */
  uiWidgetColors wcol;

  void (*state)(struct uiWidgetType *, int state, int drawflag, eUIEmbossType emboss);
  void (*draw)(uiWidgetColors *, rcti *, int state, int roundboxalign);
  void (*custom)(uiBut *, uiWidgetColors *, rcti *, int state, int roundboxalign);
  void (*text)(const uiFontStyle *, const uiWidgetColors *, uiBut *, rcti *);

} uiWidgetType;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape Preset Data
 * \{ */

static const float cornervec[WIDGET_CURVE_RESOLU][2] = {
    {0.0, 0.0},
    {0.195, 0.02},
    {0.383, 0.067},
    {0.55, 0.169},
    {0.707, 0.293},
    {0.831, 0.45},
    {0.924, 0.617},
    {0.98, 0.805},
    {1.0, 1.0},
};

const float ui_pixel_jitter[UI_PIXEL_AA_JITTER][2] = {
    {0.468813, -0.481430},
    {-0.155755, -0.352820},
    {0.219306, -0.238501},
    {-0.393286, -0.110949},
    {-0.024699, 0.013908},
    {0.343805, 0.147431},
    {-0.272855, 0.269918},
    {0.095909, 0.388710},
};
#define WIDGET_AA_JITTER UI_PIXEL_AA_JITTER
#define jit ui_pixel_jitter

static const float g_shape_preset_number_arrow_vert[3][2] = {
    {-0.352077, 0.532607},
    {-0.352077, -0.549313},
    {0.330000, -0.008353},
};
static const uint g_shape_preset_number_arrow_face[1][3] = {
    {0, 1, 2},
};

static const float g_shape_preset_scroll_circle_vert[16][2] = {
    {0.382684, 0.923879},
    {0.000001, 1.000000},
    {-0.382683, 0.923880},
    {-0.707107, 0.707107},
    {-0.923879, 0.382684},
    {-1.000000, 0.000000},
    {-0.923880, -0.382684},
    {-0.707107, -0.707107},
    {-0.382683, -0.923880},
    {0.000000, -1.000000},
    {0.382684, -0.923880},
    {0.707107, -0.707107},
    {0.923880, -0.382684},
    {1.000000, -0.000000},
    {0.923880, 0.382683},
    {0.707107, 0.707107},
};
static const uint g_shape_preset_scroll_circle_face[14][3] = {
    {0, 1, 2},
    {2, 0, 3},
    {3, 0, 15},
    {3, 15, 4},
    {4, 15, 14},
    {4, 14, 5},
    {5, 14, 13},
    {5, 13, 6},
    {6, 13, 12},
    {6, 12, 7},
    {7, 12, 11},
    {7, 11, 8},
    {8, 11, 10},
    {8, 10, 9},
};

static const float g_shape_preset_menu_arrow_vert[6][2] = {
    {-0.33, 0.16},
    {0.33, 0.16},
    {0, 0.82},
    {0, -0.82},
    {-0.33, -0.16},
    {0.33, -0.16},
};
static const uint g_shape_preset_menu_arrow_face[2][3] = {{2, 0, 1}, {3, 5, 4}};

static const float g_shape_preset_checkmark_vert[6][2] = {
    {-0.578579, 0.253369},
    {-0.392773, 0.412794},
    {-0.004241, -0.328551},
    {-0.003001, 0.034320},
    {1.055313, 0.864744},
    {0.866408, 1.026895},
};

static const uint g_shape_preset_checkmark_face[4][3] = {
    {3, 2, 4},
    {3, 4, 5},
    {1, 0, 3},
    {0, 2, 3},
};

#define OY (-0.2 / 2)
#define SC (0.35 * 2)
static const float g_shape_preset_hold_action_vert[6][2] = {
    {-0.5 + SC, 1.0 + OY},
    {0.5, 1.0 + OY},
    {0.5, 0.0 + OY + SC},
};
static const uint g_shape_preset_hold_action_face[2][3] = {{2, 0, 1}, {3, 5, 4}};
#undef OY
#undef SC

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GPUBatch Creation
 *
 * In order to speed up UI drawing we create some batches that are then
 * modified by specialized shaders to draw certain elements really fast.
 * TODO: find a better place. Maybe its own file?
 *
 * \{ */

static struct {
  GPUBatch *roundbox_widget;
  GPUBatch *roundbox_shadow;

  /* TODO remove */
  GPUVertFormat format;
  uint vflag_id;
} g_ui_batch_cache = {0};

static GPUVertFormat *vflag_format(void)
{
  if (g_ui_batch_cache.format.attr_len == 0) {
    GPUVertFormat *format = &g_ui_batch_cache.format;
    g_ui_batch_cache.vflag_id = GPU_vertformat_attr_add(
        format, "vflag", GPU_COMP_U32, 1, GPU_FETCH_INT);
  }
  return &g_ui_batch_cache.format;
}

#define INNER 0
#define OUTLINE 1
#define EMBOSS 2
#define NO_AA 0

static void set_roundbox_vertex_data(GPUVertBufRaw *vflag_step, uint32_t d)
{
  uint32_t *data = GPU_vertbuf_raw_step(vflag_step);
  *data = d;
}

static uint32_t set_roundbox_vertex(GPUVertBufRaw *vflag_step,
                                    int corner_id,
                                    int corner_v,
                                    int jit_v,
                                    bool inner,
                                    bool emboss,
                                    int color)
{
  uint32_t *data = GPU_vertbuf_raw_step(vflag_step);
  *data = corner_id;
  *data |= corner_v << 2;
  *data |= jit_v << 6;
  *data |= color << 12;
  *data |= (inner) ? (1 << 10) : 0;  /* is inner vert */
  *data |= (emboss) ? (1 << 11) : 0; /* is emboss vert */
  return *data;
}

GPUBatch *ui_batch_roundbox_widget_get(void)
{
  if (g_ui_batch_cache.roundbox_widget == NULL) {
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(vflag_format());

    GPU_vertbuf_data_alloc(vbo, 12);

    GPUIndexBufBuilder ibuf;
    GPU_indexbuf_init(&ibuf, GPU_PRIM_TRIS, 6, 12);
    /* Widget */
    GPU_indexbuf_add_tri_verts(&ibuf, 0, 1, 2);
    GPU_indexbuf_add_tri_verts(&ibuf, 2, 1, 3);
    /* Trias */
    GPU_indexbuf_add_tri_verts(&ibuf, 4, 5, 6);
    GPU_indexbuf_add_tri_verts(&ibuf, 6, 5, 7);

    GPU_indexbuf_add_tri_verts(&ibuf, 8, 9, 10);
    GPU_indexbuf_add_tri_verts(&ibuf, 10, 9, 11);

    g_ui_batch_cache.roundbox_widget = GPU_batch_create_ex(
        GPU_PRIM_TRIS, vbo, GPU_indexbuf_build(&ibuf), GPU_BATCH_OWNS_INDEX | GPU_BATCH_OWNS_VBO);
    gpu_batch_presets_register(g_ui_batch_cache.roundbox_widget);
  }
  return g_ui_batch_cache.roundbox_widget;
}

GPUBatch *ui_batch_roundbox_shadow_get(void)
{
  if (g_ui_batch_cache.roundbox_shadow == NULL) {
    uint32_t last_data;
    GPUVertBufRaw vflag_step;
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(vflag_format());
    const int vcount = (WIDGET_SIZE_MAX + 1) * 2 + 2 + WIDGET_SIZE_MAX;
    GPU_vertbuf_data_alloc(vbo, vcount);
    GPU_vertbuf_attr_get_raw_data(vbo, g_ui_batch_cache.vflag_id, &vflag_step);

    for (int c = 0; c < 4; c++) {
      for (int a = 0; a < WIDGET_CURVE_RESOLU; a++) {
        set_roundbox_vertex(&vflag_step, c, a, NO_AA, true, false, INNER);
        set_roundbox_vertex(&vflag_step, c, a, NO_AA, false, false, INNER);
      }
    }
    /* close loop */
    last_data = set_roundbox_vertex(&vflag_step, 0, 0, NO_AA, true, false, INNER);
    last_data = set_roundbox_vertex(&vflag_step, 0, 0, NO_AA, false, false, INNER);
    /* restart */
    set_roundbox_vertex_data(&vflag_step, last_data);
    set_roundbox_vertex(&vflag_step, 0, 0, NO_AA, true, false, INNER);
    /* filled */
    for (int c1 = 0, c2 = 3; c1 < 2; c1++, c2--) {
      for (int a1 = 0, a2 = WIDGET_CURVE_RESOLU - 1; a2 >= 0; a1++, a2--) {
        set_roundbox_vertex(&vflag_step, c1, a1, NO_AA, true, false, INNER);
        set_roundbox_vertex(&vflag_step, c2, a2, NO_AA, true, false, INNER);
      }
    }
    g_ui_batch_cache.roundbox_shadow = GPU_batch_create_ex(
        GPU_PRIM_TRI_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
    gpu_batch_presets_register(g_ui_batch_cache.roundbox_shadow);
  }
  return g_ui_batch_cache.roundbox_shadow;
}

#undef INNER
#undef OUTLINE
#undef EMBOSS
#undef NO_AA

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Triangle Arrow
 * \{ */

void UI_draw_anti_tria(
    float x1, float y1, float x2, float y2, float x3, float y3, const float color[4])
{
  const float tri_arr[3][2] = {{x1, y1}, {x2, y2}, {x3, y3}};

  float draw_color[4];
  copy_v4_v4(draw_color, color);
  /* Note: This won't give back the original color. */
  draw_color[3] *= 1.0f / WIDGET_AA_JITTER;

  GPU_blend(GPU_BLEND_ALPHA);

  const uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformColor4fv(draw_color);
  immBegin(GPU_PRIM_TRIS, 3 * WIDGET_AA_JITTER);

  /* for each AA step */
  for (int j = 0; j < WIDGET_AA_JITTER; j++) {
    immVertex2f(pos, tri_arr[0][0] + jit[j][0], tri_arr[0][1] + jit[j][1]);
    immVertex2f(pos, tri_arr[1][0] + jit[j][0], tri_arr[1][1] + jit[j][1]);
    immVertex2f(pos, tri_arr[2][0] + jit[j][0], tri_arr[2][1] + jit[j][1]);
  }

  immEnd();

  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
}

/* Triangle 'icon' for panel header and other cases. */
void UI_draw_icon_tri(float x, float y, char dir, const float color[4])
{
  const float f3 = 0.05 * U.widget_unit;
  const float f5 = 0.15 * U.widget_unit;
  const float f7 = 0.25 * U.widget_unit;

  if (dir == 'h') {
    UI_draw_anti_tria(x - f3, y - f5, x - f3, y + f5, x + f7, y, color);
  }
  else if (dir == 't') {
    UI_draw_anti_tria(x - f5, y - f7, x + f5, y - f7, x, y + f3, color);
  }
  else { /* 'v' = vertical, down. */
    UI_draw_anti_tria(x - f5, y + f3, x + f5, y + f3, x, y - f7, color);
  }
}

/* triangle 'icon' inside rect */
void ui_draw_anti_tria_rect(const rctf *rect, char dir, const float color[4])
{
  if (dir == 'h') {
    const float half = 0.5f * BLI_rctf_size_y(rect);
    UI_draw_anti_tria(
        rect->xmin, rect->ymin, rect->xmin, rect->ymax, rect->xmax, rect->ymin + half, color);
  }
  else {
    const float half = 0.5f * BLI_rctf_size_x(rect);
    UI_draw_anti_tria(
        rect->xmin, rect->ymax, rect->xmax, rect->ymax, rect->xmin + half, rect->ymin, color);
  }
}

void UI_draw_anti_fan(float tri_array[][2], uint length, const float color[4])
{
  float draw_color[4];

  copy_v4_v4(draw_color, color);
  draw_color[3] *= 2.0f / WIDGET_AA_JITTER;

  GPU_blend(GPU_BLEND_ALPHA);

  const uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformColor4fv(draw_color);

  /* for each AA step */
  for (int j = 0; j < WIDGET_AA_JITTER; j++) {
    immBegin(GPU_PRIM_TRI_FAN, length);
    immVertex2f(pos, tri_array[0][0], tri_array[0][1]);
    immVertex2f(pos, tri_array[1][0], tri_array[1][1]);

    /* We jitter only the middle of the fan, the extremes are pinned. */
    for (int i = 2; i < length - 1; i++) {
      immVertex2f(pos, tri_array[i][0] + jit[j][0], tri_array[i][1] + jit[j][1]);
    }

    immVertex2f(pos, tri_array[length - 1][0], tri_array[length - 1][1]);
    immEnd();
  }

  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
}

static void widget_init(uiWidgetBase *wtb)
{
  wtb->totvert = wtb->halfwayvert = 0;
  wtb->tria1.tot = 0;
  wtb->tria2.tot = 0;
  wtb->tria1.type = ROUNDBOX_TRIA_NONE;
  wtb->tria1.size = 0;
  wtb->tria2.size = 0;

  wtb->draw_inner = true;
  wtb->draw_outline = true;
  wtb->draw_emboss = true;

  wtb->uniform_params.shade_dir = 1.0f;
  wtb->uniform_params.alpha_discard = 1.0f;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Round Box
 * \{ */

/* helper call, makes shadow rect, with 'sun' above menu, so only shadow to left/right/bottom */
/* return tot */
static int round_box_shadow_edges(
    float (*vert)[2], const rcti *rect, float rad, int roundboxalign, float step)
{
  float vec[WIDGET_CURVE_RESOLU][2];
  int tot = 0;

  rad += step;

  if (2.0f * rad > BLI_rcti_size_y(rect)) {
    rad = 0.5f * BLI_rcti_size_y(rect);
  }

  const float minx = rect->xmin - step;
  const float miny = rect->ymin - step;
  const float maxx = rect->xmax + step;
  const float maxy = rect->ymax + step;

  /* Multiply. */
  for (int a = 0; a < WIDGET_CURVE_RESOLU; a++) {
    vec[a][0] = rad * cornervec[a][0];
    vec[a][1] = rad * cornervec[a][1];
  }

  /* start with left-top, anti clockwise */
  if (roundboxalign & UI_CNR_TOP_LEFT) {
    for (int a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
      vert[tot][0] = minx + rad - vec[a][0];
      vert[tot][1] = maxy - vec[a][1];
    }
  }
  else {
    for (int a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
      vert[tot][0] = minx;
      vert[tot][1] = maxy;
    }
  }

  if (roundboxalign & UI_CNR_BOTTOM_LEFT) {
    for (int a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
      vert[tot][0] = minx + vec[a][1];
      vert[tot][1] = miny + rad - vec[a][0];
    }
  }
  else {
    for (int a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
      vert[tot][0] = minx;
      vert[tot][1] = miny;
    }
  }

  if (roundboxalign & UI_CNR_BOTTOM_RIGHT) {
    for (int a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
      vert[tot][0] = maxx - rad + vec[a][0];
      vert[tot][1] = miny + vec[a][1];
    }
  }
  else {
    for (int a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
      vert[tot][0] = maxx;
      vert[tot][1] = miny;
    }
  }

  if (roundboxalign & UI_CNR_TOP_RIGHT) {
    for (int a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
      vert[tot][0] = maxx - vec[a][1];
      vert[tot][1] = maxy - rad + vec[a][0];
    }
  }
  else {
    for (int a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
      vert[tot][0] = maxx;
      vert[tot][1] = maxy;
    }
  }
  return tot;
}

/* this call has 1 extra arg to allow mask outline */
static void round_box__edges(
    uiWidgetBase *wt, int roundboxalign, const rcti *rect, float rad, float radi)
{
  float vec[WIDGET_CURVE_RESOLU][2], veci[WIDGET_CURVE_RESOLU][2];
  const float minx = rect->xmin, miny = rect->ymin, maxx = rect->xmax, maxy = rect->ymax;
  const float minxi = minx + U.pixelsize; /* boundbox inner */
  const float maxxi = maxx - U.pixelsize;
  const float minyi = miny + U.pixelsize;
  const float maxyi = maxy - U.pixelsize;
  /* for uv, can divide by zero */
  const float facxi = (maxxi != minxi) ? 1.0f / (maxxi - minxi) : 0.0f;
  const float facyi = (maxyi != minyi) ? 1.0f / (maxyi - minyi) : 0.0f;
  int tot = 0;
  const int hnum = ((roundboxalign & (UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT)) ==
                        (UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT) ||
                    (roundboxalign & (UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT)) ==
                        (UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT)) ?
                       1 :
                       2;
  const int vnum = ((roundboxalign & (UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT)) ==
                        (UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT) ||
                    (roundboxalign & (UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT)) ==
                        (UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT)) ?
                       1 :
                       2;

  const int minsize = min_ii(BLI_rcti_size_x(rect) * hnum, BLI_rcti_size_y(rect) * vnum);

  if (2.0f * rad > minsize) {
    rad = 0.5f * minsize;
  }

  if (2.0f * (radi + 1.0f) > minsize) {
    radi = 0.5f * minsize - U.pixelsize;
  }

  wt->uniform_params.rad = rad;
  wt->uniform_params.radi = radi;
  wt->uniform_params.facxi = facxi;
  wt->uniform_params.facyi = facyi;
  wt->uniform_params.round_corners[0] = (roundboxalign & UI_CNR_BOTTOM_LEFT) ? 1.0f : 0.0f;
  wt->uniform_params.round_corners[1] = (roundboxalign & UI_CNR_BOTTOM_RIGHT) ? 1.0f : 0.0f;
  wt->uniform_params.round_corners[2] = (roundboxalign & UI_CNR_TOP_RIGHT) ? 1.0f : 0.0f;
  wt->uniform_params.round_corners[3] = (roundboxalign & UI_CNR_TOP_LEFT) ? 1.0f : 0.0f;
  BLI_rctf_rcti_copy(&wt->uniform_params.rect, rect);
  BLI_rctf_init(&wt->uniform_params.recti, minxi, maxxi, minyi, maxyi);

  /* mult */
  for (int a = 0; a < WIDGET_CURVE_RESOLU; a++) {
    veci[a][0] = radi * cornervec[a][0];
    veci[a][1] = radi * cornervec[a][1];
    vec[a][0] = rad * cornervec[a][0];
    vec[a][1] = rad * cornervec[a][1];
  }

  /* corner left-bottom */
  if (roundboxalign & UI_CNR_BOTTOM_LEFT) {
    for (int a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
      wt->inner_v[tot][0] = minxi + veci[a][1];
      wt->inner_v[tot][1] = minyi + radi - veci[a][0];

      wt->outer_v[tot][0] = minx + vec[a][1];
      wt->outer_v[tot][1] = miny + rad - vec[a][0];

      wt->inner_uv[tot][0] = facxi * (wt->inner_v[tot][0] - minxi);
      wt->inner_uv[tot][1] = facyi * (wt->inner_v[tot][1] - minyi);
    }
  }
  else {
    wt->inner_v[tot][0] = minxi;
    wt->inner_v[tot][1] = minyi;

    wt->outer_v[tot][0] = minx;
    wt->outer_v[tot][1] = miny;

    wt->inner_uv[tot][0] = 0.0f;
    wt->inner_uv[tot][1] = 0.0f;

    tot++;
  }

  /* corner right-bottom */
  if (roundboxalign & UI_CNR_BOTTOM_RIGHT) {
    for (int a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
      wt->inner_v[tot][0] = maxxi - radi + veci[a][0];
      wt->inner_v[tot][1] = minyi + veci[a][1];

      wt->outer_v[tot][0] = maxx - rad + vec[a][0];
      wt->outer_v[tot][1] = miny + vec[a][1];

      wt->inner_uv[tot][0] = facxi * (wt->inner_v[tot][0] - minxi);
      wt->inner_uv[tot][1] = facyi * (wt->inner_v[tot][1] - minyi);
    }
  }
  else {
    wt->inner_v[tot][0] = maxxi;
    wt->inner_v[tot][1] = minyi;

    wt->outer_v[tot][0] = maxx;
    wt->outer_v[tot][1] = miny;

    wt->inner_uv[tot][0] = 1.0f;
    wt->inner_uv[tot][1] = 0.0f;

    tot++;
  }

  wt->halfwayvert = tot;

  /* corner right-top */
  if (roundboxalign & UI_CNR_TOP_RIGHT) {
    for (int a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
      wt->inner_v[tot][0] = maxxi - veci[a][1];
      wt->inner_v[tot][1] = maxyi - radi + veci[a][0];

      wt->outer_v[tot][0] = maxx - vec[a][1];
      wt->outer_v[tot][1] = maxy - rad + vec[a][0];

      wt->inner_uv[tot][0] = facxi * (wt->inner_v[tot][0] - minxi);
      wt->inner_uv[tot][1] = facyi * (wt->inner_v[tot][1] - minyi);
    }
  }
  else {
    wt->inner_v[tot][0] = maxxi;
    wt->inner_v[tot][1] = maxyi;

    wt->outer_v[tot][0] = maxx;
    wt->outer_v[tot][1] = maxy;

    wt->inner_uv[tot][0] = 1.0f;
    wt->inner_uv[tot][1] = 1.0f;

    tot++;
  }

  /* corner left-top */
  if (roundboxalign & UI_CNR_TOP_LEFT) {
    for (int a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
      wt->inner_v[tot][0] = minxi + radi - veci[a][0];
      wt->inner_v[tot][1] = maxyi - veci[a][1];

      wt->outer_v[tot][0] = minx + rad - vec[a][0];
      wt->outer_v[tot][1] = maxy - vec[a][1];

      wt->inner_uv[tot][0] = facxi * (wt->inner_v[tot][0] - minxi);
      wt->inner_uv[tot][1] = facyi * (wt->inner_v[tot][1] - minyi);
    }
  }
  else {
    wt->inner_v[tot][0] = minxi;
    wt->inner_v[tot][1] = maxyi;

    wt->outer_v[tot][0] = minx;
    wt->outer_v[tot][1] = maxy;

    wt->inner_uv[tot][0] = 0.0f;
    wt->inner_uv[tot][1] = 1.0f;

    tot++;
  }

  BLI_assert(tot <= WIDGET_SIZE_MAX);

  wt->totvert = tot;
}

static void round_box_edges(uiWidgetBase *wt, int roundboxalign, const rcti *rect, float rad)
{
  round_box__edges(wt, roundboxalign, rect, rad, rad - U.pixelsize);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape Preset Mini API
 * \{ */

/* based on button rect, return scaled array of triangles */
static void shape_preset_init_trias_ex(uiWidgetTrias *tria,
                                       const rcti *rect,
                                       float triasize,
                                       char where,
                                       /* input data */
                                       const float verts[][2],
                                       const int verts_tot,
                                       const uint tris[][3],
                                       const int tris_tot)
{
  float sizex, sizey;
  int i1 = 0, i2 = 1;

  const float minsize = ELEM(where, 'r', 'l') ? BLI_rcti_size_y(rect) : BLI_rcti_size_x(rect);

  /* center position and size */
  float centx = (float)rect->xmin + 0.4f * minsize;
  float centy = (float)rect->ymin + 0.5f * minsize;
  tria->size = sizex = sizey = -0.5f * triasize * minsize;

  if (where == 'r') {
    centx = (float)rect->xmax - 0.4f * minsize;
    sizex = -sizex;
  }
  else if (where == 't') {
    centx = (float)rect->xmin + 0.5f * minsize;
    centy = (float)rect->ymax - 0.5f * minsize;
    sizey = -sizey;
    i2 = 0;
    i1 = 1;
  }
  else if (where == 'b') {
    centx = (float)rect->xmin + 0.5f * minsize;
    sizex = -sizex;
    i2 = 0;
    i1 = 1;
  }

  for (int a = 0; a < verts_tot; a++) {
    tria->vec[a][0] = sizex * verts[a][i1] + centx;
    tria->vec[a][1] = sizey * verts[a][i2] + centy;
  }

  tria->center[0] = centx;
  tria->center[1] = centy;

  tria->tot = tris_tot;
  tria->index = tris;
}

static void shape_preset_init_number_arrows(uiWidgetTrias *tria,
                                            const rcti *rect,
                                            float triasize,
                                            char where)
{
  tria->type = ROUNDBOX_TRIA_ARROWS;
  shape_preset_init_trias_ex(tria,
                             rect,
                             triasize,
                             where,
                             g_shape_preset_number_arrow_vert,
                             ARRAY_SIZE(g_shape_preset_number_arrow_vert),
                             g_shape_preset_number_arrow_face,
                             ARRAY_SIZE(g_shape_preset_number_arrow_face));
}

static void shape_preset_init_hold_action(uiWidgetTrias *tria,
                                          const rcti *rect,
                                          float triasize,
                                          char where)
{
  tria->type = ROUNDBOX_TRIA_HOLD_ACTION_ARROW;
  /* With the current changes to use batches for widget drawing, the code
   * below is doing almost nothing effectively. 'where' doesn't work either,
   * shader is currently hardcoded to work for the button triangle pointing
   * at the lower right. The same limitation applies to other trias as well.
   * XXX Should be addressed. */
  shape_preset_init_trias_ex(tria,
                             rect,
                             triasize,
                             where,
                             g_shape_preset_hold_action_vert,
                             ARRAY_SIZE(g_shape_preset_hold_action_vert),
                             g_shape_preset_hold_action_face,
                             ARRAY_SIZE(g_shape_preset_hold_action_face));
}

static void shape_preset_init_scroll_circle(uiWidgetTrias *tria,
                                            const rcti *rect,
                                            float triasize,
                                            char where)
{
  tria->type = ROUNDBOX_TRIA_SCROLL;
  shape_preset_init_trias_ex(tria,
                             rect,
                             triasize,
                             where,
                             g_shape_preset_scroll_circle_vert,
                             ARRAY_SIZE(g_shape_preset_scroll_circle_vert),
                             g_shape_preset_scroll_circle_face,
                             ARRAY_SIZE(g_shape_preset_scroll_circle_face));
}

static void widget_draw_vertex_buffer(uint pos,
                                      uint col,
                                      int mode,
                                      const float quads_pos[WIDGET_SIZE_MAX][2],
                                      const uchar quads_col[WIDGET_SIZE_MAX][4],
                                      uint totvert)
{
  immBegin(mode, totvert);
  for (int i = 0; i < totvert; i++) {
    if (quads_col) {
      immAttr4ubv(col, quads_col[i]);
    }
    immVertex2fv(pos, quads_pos[i]);
  }
  immEnd();
}

static void shape_preset_trias_from_rect_menu(uiWidgetTrias *tria, const rcti *rect)
{
  const float width = BLI_rcti_size_x(rect);
  const float height = BLI_rcti_size_y(rect);
  float centx, centy, size;

  tria->type = ROUNDBOX_TRIA_MENU;

  /* Center position and size. */
  tria->center[0] = centx = rect->xmin + 0.52f * BLI_rcti_size_y(rect);
  tria->center[1] = centy = rect->ymin + 0.52f * BLI_rcti_size_y(rect);
  tria->size = size = 0.4f * height;

  if (width > height * 1.1f) {
    /* For wider buttons align tighter to the right. */
    tria->center[0] = centx = rect->xmax - 0.32f * height;
  }

  for (int a = 0; a < 6; a++) {
    tria->vec[a][0] = size * g_shape_preset_menu_arrow_vert[a][0] + centx;
    tria->vec[a][1] = size * g_shape_preset_menu_arrow_vert[a][1] + centy;
  }

  tria->tot = 2;
  tria->index = g_shape_preset_menu_arrow_face;
}

static void shape_preset_trias_from_rect_checkmark(uiWidgetTrias *tria, const rcti *rect)
{
  float centx, centy, size;

  tria->type = ROUNDBOX_TRIA_CHECK;

  /* Center position and size. */
  tria->center[0] = centx = rect->xmin + 0.5f * BLI_rcti_size_y(rect);
  tria->center[1] = centy = rect->ymin + 0.5f * BLI_rcti_size_y(rect);
  tria->size = size = 0.5f * BLI_rcti_size_y(rect);

  for (int a = 0; a < 6; a++) {
    tria->vec[a][0] = size * g_shape_preset_checkmark_vert[a][0] + centx;
    tria->vec[a][1] = size * g_shape_preset_checkmark_vert[a][1] + centy;
  }

  tria->tot = 4;
  tria->index = g_shape_preset_checkmark_face;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Widget Base Drawing
 * \{ */

/* prepares shade colors */
static void shadecolors4(
    uchar coltop[4], uchar coldown[4], const uchar *color, short shadetop, short shadedown)
{
  coltop[0] = CLAMPIS(color[0] + shadetop, 0, 255);
  coltop[1] = CLAMPIS(color[1] + shadetop, 0, 255);
  coltop[2] = CLAMPIS(color[2] + shadetop, 0, 255);
  coltop[3] = color[3];

  coldown[0] = CLAMPIS(color[0] + shadedown, 0, 255);
  coldown[1] = CLAMPIS(color[1] + shadedown, 0, 255);
  coldown[2] = CLAMPIS(color[2] + shadedown, 0, 255);
  coldown[3] = color[3];
}

static void widget_verts_to_triangle_strip(uiWidgetBase *wtb,
                                           const int totvert,
                                           float triangle_strip[WIDGET_SIZE_MAX * 2 + 2][2])
{
  int a;
  for (a = 0; a < totvert; a++) {
    copy_v2_v2(triangle_strip[a * 2], wtb->outer_v[a]);
    copy_v2_v2(triangle_strip[a * 2 + 1], wtb->inner_v[a]);
  }
  copy_v2_v2(triangle_strip[a * 2], wtb->outer_v[0]);
  copy_v2_v2(triangle_strip[a * 2 + 1], wtb->inner_v[0]);
}

static void widgetbase_outline(uiWidgetBase *wtb, uint pos)
{
  float triangle_strip[WIDGET_SIZE_MAX * 2 + 2][2]; /* + 2 because the last pair is wrapped */
  widget_verts_to_triangle_strip(wtb, wtb->totvert, triangle_strip);

  widget_draw_vertex_buffer(
      pos, 0, GPU_PRIM_TRI_STRIP, triangle_strip, NULL, wtb->totvert * 2 + 2);
}

static void widgetbase_set_uniform_alpha_discard(uiWidgetBase *wtb,
                                                 const bool alpha_check,
                                                 const float discard_factor)
{
  if (alpha_check) {
    wtb->uniform_params.alpha_discard = -discard_factor;
  }
  else {
    wtb->uniform_params.alpha_discard = discard_factor;
  }
}

static void widgetbase_set_uniform_alpha_check(uiWidgetBase *wtb, const bool alpha_check)
{
  const float discard_factor = fabs(wtb->uniform_params.alpha_discard);
  widgetbase_set_uniform_alpha_discard(wtb, alpha_check, discard_factor);
}

static void widgetbase_set_uniform_discard_factor(uiWidgetBase *wtb, const float discard_factor)
{
  const bool alpha_check = wtb->uniform_params.alpha_discard < 0.0f;
  widgetbase_set_uniform_alpha_discard(wtb, alpha_check, discard_factor);
}

static void widgetbase_set_uniform_colors_ubv(uiWidgetBase *wtb,
                                              const uchar *col1,
                                              const uchar *col2,
                                              const uchar *outline,
                                              const uchar *emboss,
                                              const uchar *tria,
                                              const bool alpha_check)
{
  widgetbase_set_uniform_alpha_check(wtb, alpha_check);
  rgba_float_args_set_ch(wtb->uniform_params.color_inner1, col1[0], col1[1], col1[2], col1[3]);
  rgba_float_args_set_ch(wtb->uniform_params.color_inner2, col2[0], col2[1], col2[2], col2[3]);
  rgba_float_args_set_ch(
      wtb->uniform_params.color_outline, outline[0], outline[1], outline[2], outline[3]);
  rgba_float_args_set_ch(
      wtb->uniform_params.color_emboss, emboss[0], emboss[1], emboss[2], emboss[3]);
  rgba_float_args_set_ch(wtb->uniform_params.color_tria, tria[0], tria[1], tria[2], tria[3]);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Widget Base Drawing #GPUBatch Cache
 * \{ */

/* keep in sync with shader */
#define MAX_WIDGET_BASE_BATCH 6
#define MAX_WIDGET_PARAMETERS 12

static struct {
  uiWidgetBaseParameters params[MAX_WIDGET_BASE_BATCH];
  int count;
  bool enabled;
} g_widget_base_batch = {{{{0}}}};

void UI_widgetbase_draw_cache_flush(void)
{
  const float checker_params[3] = {
      UI_ALPHA_CHECKER_DARK / 255.0f, UI_ALPHA_CHECKER_LIGHT / 255.0f, 8.0f};

  if (g_widget_base_batch.count == 0) {
    return;
  }

  GPUBatch *batch = ui_batch_roundbox_widget_get();
  if (g_widget_base_batch.count == 1) {
    /* draw single */
    GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_WIDGET_BASE);
    GPU_batch_uniform_4fv_array(
        batch, "parameters", MAX_WIDGET_PARAMETERS, (const float(*)[4])g_widget_base_batch.params);
    GPU_batch_uniform_3fv(batch, "checkerColorAndSize", checker_params);
    GPU_batch_draw(batch);
  }
  else {
    GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_WIDGET_BASE_INST);
    GPU_batch_uniform_4fv_array(batch,
                                "parameters",
                                MAX_WIDGET_PARAMETERS * MAX_WIDGET_BASE_BATCH,
                                (float(*)[4])g_widget_base_batch.params);
    GPU_batch_uniform_3fv(batch, "checkerColorAndSize", checker_params);
    GPU_batch_draw_instanced(batch, g_widget_base_batch.count);
  }
  g_widget_base_batch.count = 0;
}

void UI_widgetbase_draw_cache_begin(void)
{
  BLI_assert(g_widget_base_batch.enabled == false);
  g_widget_base_batch.enabled = true;
}

void UI_widgetbase_draw_cache_end(void)
{
  BLI_assert(g_widget_base_batch.enabled == true);
  g_widget_base_batch.enabled = false;

  GPU_blend(GPU_BLEND_ALPHA);

  UI_widgetbase_draw_cache_flush();

  GPU_blend(GPU_BLEND_NONE);
}

/* Disable cached/instanced drawing and enforce single widget drawing pipeline.
 * Works around interface artifacts happening on certain driver and hardware
 * configurations. */
static bool draw_widgetbase_batch_skip_draw_cache(void)
{
  /* MacOS is known to have issues on Mac Mini and MacBook Pro with Intel Iris GPU.
   * For example, T78307. */
  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_MAC, GPU_DRIVER_ANY)) {
    return true;
  }

  /* There are also reports that some AMD and Mesa driver configuration suffer from the
   * same issue, T78803. */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OPENSOURCE)) {
    return true;
  }

  return false;
}

static void draw_widgetbase_batch(uiWidgetBase *wtb)
{
  wtb->uniform_params.tria_type = wtb->tria1.type;
  wtb->uniform_params.tria1_size = wtb->tria1.size;
  wtb->uniform_params.tria2_size = wtb->tria2.size;
  copy_v2_v2(wtb->uniform_params.tria1_center, wtb->tria1.center);
  copy_v2_v2(wtb->uniform_params.tria2_center, wtb->tria2.center);

  if (g_widget_base_batch.enabled && !draw_widgetbase_batch_skip_draw_cache()) {
    g_widget_base_batch.params[g_widget_base_batch.count] = wtb->uniform_params;
    g_widget_base_batch.count++;

    if (g_widget_base_batch.count == MAX_WIDGET_BASE_BATCH) {
      UI_widgetbase_draw_cache_flush();
    }
  }
  else {
    const float checker_params[3] = {
        UI_ALPHA_CHECKER_DARK / 255.0f, UI_ALPHA_CHECKER_LIGHT / 255.0f, 8.0f};
    /* draw single */
    GPUBatch *batch = ui_batch_roundbox_widget_get();
    GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_WIDGET_BASE);
    GPU_batch_uniform_4fv_array(
        batch, "parameters", MAX_WIDGET_PARAMETERS, (float(*)[4]) & wtb->uniform_params);
    GPU_batch_uniform_3fv(batch, "checkerColorAndSize", checker_params);
    GPU_batch_draw(batch);
  }
}

static void widgetbase_draw_ex(uiWidgetBase *wtb,
                               const uiWidgetColors *wcol,
                               bool show_alpha_checkers)
{
  uchar inner_col1[4] = {0};
  uchar inner_col2[4] = {0};
  uchar emboss_col[4] = {0};
  uchar outline_col[4] = {0};
  uchar tria_col[4] = {0};
  /* For color widget. */
  if (wcol->shaded != 0) {
    show_alpha_checkers = false;
  }

  /* backdrop non AA */
  if (wtb->draw_inner) {
    if (wcol->shaded == 0) {
      /* simple fill */
      inner_col1[0] = inner_col2[0] = wcol->inner[0];
      inner_col1[1] = inner_col2[1] = wcol->inner[1];
      inner_col1[2] = inner_col2[2] = wcol->inner[2];
      inner_col1[3] = inner_col2[3] = wcol->inner[3];
    }
    else {
      /* gradient fill */
      shadecolors4(inner_col1, inner_col2, wcol->inner, wcol->shadetop, wcol->shadedown);
    }
  }

  if (wtb->draw_outline) {
    outline_col[0] = wcol->outline[0];
    outline_col[1] = wcol->outline[1];
    outline_col[2] = wcol->outline[2];
    outline_col[3] = wcol->outline[3];

    /* emboss bottom shadow */
    if (wtb->draw_emboss) {
      UI_GetThemeColor4ubv(TH_WIDGET_EMBOSS, emboss_col);
    }
  }

  if (wtb->tria1.type != ROUNDBOX_TRIA_NONE) {
    tria_col[0] = wcol->item[0];
    tria_col[1] = wcol->item[1];
    tria_col[2] = wcol->item[2];
    tria_col[3] = wcol->item[3];
  }

  /* Draw everything in one draw-call. */
  if (inner_col1[3] || inner_col2[3] || outline_col[3] || emboss_col[3] || tria_col[3] ||
      show_alpha_checkers) {
    widgetbase_set_uniform_colors_ubv(
        wtb, inner_col1, inner_col2, outline_col, emboss_col, tria_col, show_alpha_checkers);

    GPU_blend(GPU_BLEND_ALPHA);
    draw_widgetbase_batch(wtb);
    GPU_blend(GPU_BLEND_NONE);
  }
}

static void widgetbase_draw(uiWidgetBase *wtb, const uiWidgetColors *wcol)
{
  widgetbase_draw_ex(wtb, wcol, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text/Icon Drawing
 * \{ */

#define UI_TEXT_CLIP_MARGIN (0.25f * U.widget_unit / but->block->aspect)

#define PREVIEW_PAD 4

static float widget_alpha_factor(const int state)
{
  if (state & (UI_BUT_INACTIVE | UI_BUT_DISABLED)) {
    if (state & UI_SEARCH_FILTER_NO_MATCH) {
      return 0.25f;
    }
    return 0.5f;
  }

  if (state & UI_SEARCH_FILTER_NO_MATCH) {
    return 0.5f;
  }

  return 1.0f;
}

static void widget_draw_preview(BIFIconID icon, float alpha, const rcti *rect)
{
  if (icon == ICON_NONE) {
    return;
  }

  const int w = BLI_rcti_size_x(rect);
  const int h = BLI_rcti_size_y(rect);
  const int size = MIN2(w, h) - PREVIEW_PAD * 2;

  if (size > 0) {
    const int x = rect->xmin + w / 2 - size / 2;
    const int y = rect->ymin + h / 2 - size / 2;

    UI_icon_draw_preview(x, y, icon, 1.0f, alpha, size);
  }
}

static int ui_but_draw_menu_icon(const uiBut *but)
{
  return (but->flag & UI_BUT_ICON_SUBMENU) && (but->emboss == UI_EMBOSS_PULLDOWN);
}

/* icons have been standardized... and this call draws in untransformed coordinates */

static void widget_draw_icon(
    const uiBut *but, BIFIconID icon, float alpha, const rcti *rect, const uchar mono_color[4])
{
  float xs = 0.0f, ys = 0.0f;

  if (but->flag & UI_BUT_ICON_PREVIEW) {
    GPU_blend(GPU_BLEND_ALPHA);
    widget_draw_preview(icon, alpha, rect);
    GPU_blend(GPU_BLEND_NONE);
    return;
  }

  /* this icon doesn't need draw... */
  if (icon == ICON_BLANK1 && (but->flag & UI_BUT_ICON_SUBMENU) == 0) {
    return;
  }

  const float aspect = but->block->aspect * U.inv_dpi_fac;
  const float height = ICON_DEFAULT_HEIGHT / aspect;

  /* calculate blend color */
  if (ELEM(but->type, UI_BTYPE_TOGGLE, UI_BTYPE_ROW, UI_BTYPE_TOGGLE_N, UI_BTYPE_LISTROW)) {
    if (but->flag & UI_SELECT) {
      /* pass */
    }
    else if (but->flag & UI_ACTIVE) {
      /* pass */
    }
    else {
      alpha = 0.75f;
    }
  }
  else if ((but->type == UI_BTYPE_LABEL)) {
    /* extra feature allows more alpha blending */
    if (but->a1 == 1.0f) {
      alpha *= but->a2;
    }
  }
  else if (ELEM(but->type, UI_BTYPE_BUT, UI_BTYPE_DECORATOR)) {
    alpha *= widget_alpha_factor(but->flag);
  }

  GPU_blend(GPU_BLEND_ALPHA);

  if (icon && icon != ICON_BLANK1) {
    const float ofs = 1.0f / aspect;

    if (but->drawflag & UI_BUT_ICON_LEFT) {
      /* special case - icon_only pie buttons */
      if (ui_block_is_pie_menu(but->block) && !ELEM(but->type, UI_BTYPE_MENU, UI_BTYPE_POPOVER) &&
          but->str && but->str[0] == '\0') {
        xs = rect->xmin + 2.0f * ofs;
      }
      else if (but->emboss == UI_EMBOSS_NONE || but->type == UI_BTYPE_LABEL) {
        xs = rect->xmin + 2.0f * ofs;
      }
      else {
        xs = rect->xmin + 4.0f * ofs;
      }
    }
    else {
      xs = (rect->xmin + rect->xmax - height) / 2.0f;
    }
    ys = (rect->ymin + rect->ymax - height) / 2.0f;

    /* force positions to integers, for zoom levels near 1. draws icons crisp. */
    if (aspect > 0.95f && aspect < 1.05f) {
      xs = (int)(xs + 0.1f);
      ys = (int)(ys + 0.1f);
    }

    /* Get theme color. */
    uchar color[4] = {mono_color[0], mono_color[1], mono_color[2], mono_color[3]};
    const bool has_theme = UI_icon_get_theme_color(icon, color);

    /* to indicate draggable */
    if (but->dragpoin && (but->flag & UI_ACTIVE)) {
      UI_icon_draw_ex(xs, ys, icon, aspect, 1.25f, 0.0f, color, has_theme);
    }
    else if ((but->flag & (UI_ACTIVE | UI_SELECT | UI_SELECT_DRAW))) {
      UI_icon_draw_ex(xs, ys, icon, aspect, alpha, 0.0f, color, has_theme);
    }
    else if (!((but->icon != ICON_NONE) && UI_but_is_tool(but))) {
      if (has_theme) {
        alpha *= 0.8f;
      }
      UI_icon_draw_ex(xs, ys, icon, aspect, alpha, 0.0f, color, has_theme);
    }
    else {
      const bTheme *btheme = UI_GetTheme();
      const float desaturate = 1.0 - btheme->tui.icon_saturation;
      UI_icon_draw_ex(xs, ys, icon, aspect, alpha, desaturate, color, has_theme);
    }
  }

  GPU_blend(GPU_BLEND_NONE);
}

static void widget_draw_submenu_tria(const uiBut *but,
                                     const rcti *rect,
                                     const uiWidgetColors *wcol)
{
  const float aspect = but->block->aspect * U.inv_dpi_fac;
  const int tria_height = (int)(ICON_DEFAULT_HEIGHT / aspect);
  const int tria_width = (int)(ICON_DEFAULT_WIDTH / aspect) - 2 * U.pixelsize;
  const int xs = rect->xmax - tria_width;
  const int ys = (rect->ymin + rect->ymax - tria_height) / 2.0f;

  float col[4];
  rgba_uchar_to_float(col, wcol->text);
  col[3] *= 0.8f;

  rctf tria_rect;
  BLI_rctf_init(&tria_rect, xs, xs + tria_width, ys, ys + tria_height);
  BLI_rctf_scale(&tria_rect, 0.4f);

  GPU_blend(GPU_BLEND_ALPHA);
  UI_widgetbase_draw_cache_flush();
  GPU_blend(GPU_BLEND_NONE);
  ui_draw_anti_tria_rect(&tria_rect, 'h', col);
}

static void ui_text_clip_give_prev_off(uiBut *but, const char *str)
{
  const char *prev_utf8 = BLI_str_find_prev_char_utf8(str, str + but->ofs);
  const int bytes = str + but->ofs - prev_utf8;

  but->ofs -= bytes;
}

static void ui_text_clip_give_next_off(uiBut *but, const char *str)
{
  const char *next_utf8 = BLI_str_find_next_char_utf8(str + but->ofs, NULL);
  const int bytes = next_utf8 - (str + but->ofs);

  but->ofs += bytes;
}

/**
 * Helper.
 * This func assumes things like kerning handling have already been handled!
 * Return the length of modified (right-clipped + ellipsis) string.
 */
static void ui_text_clip_right_ex(const uiFontStyle *fstyle,
                                  char *str,
                                  const size_t max_len,
                                  const float okwidth,
                                  const char *sep,
                                  const int sep_len,
                                  const float sep_strwidth,
                                  size_t *r_final_len)
{
  BLI_assert(str[0]);

  /* How many BYTES (not characters) of this utf-8 string can fit, along with appended ellipsis. */
  int l_end = BLF_width_to_strlen(fstyle->uifont_id, str, max_len, okwidth - sep_strwidth, NULL);

  if (l_end > 0) {
    /* At least one character, so clip and add the ellipsis.  */
    memcpy(str + l_end, sep, sep_len + 1); /* +1 for trailing '\0'. */
    if (r_final_len) {
      *r_final_len = (size_t)(l_end) + sep_len;
    }
  }
  else {
    /* Otherwise fit as much as we can without adding an ellipsis.  */
    l_end = BLF_width_to_strlen(fstyle->uifont_id, str, max_len, okwidth, NULL);
    str[l_end] = '\0';
    if (r_final_len) {
      *r_final_len = (size_t)l_end;
    }
  }
}

/**
 * Cut off the middle of the text to fit into the given width.
 *
 * \note in case this middle clipping would just remove a few chars,
 * it rather clips right, which is more readable.
 *
 * If rpart_sep is not Null, the part of str starting to first occurrence of rpart_sep
 * is preserved at all cost.
 * Useful for strings with shortcuts
 * (like 'AVeryLongFooBarLabelForMenuEntry|Ctrl O' -> 'AVeryLong...MenuEntry|Ctrl O').
 */
float UI_text_clip_middle_ex(const uiFontStyle *fstyle,
                             char *str,
                             float okwidth,
                             const float minwidth,
                             const size_t max_len,
                             const char rpart_sep)
{
  /* Add some epsilon to OK width, avoids 'ellipsing' text that nearly fits!
   * Better to have a small piece of the last char cut out,
   * than two remaining chars replaced by an ellipsis... */
  okwidth += 1.0f + UI_DPI_FAC;

  BLI_assert(str[0]);

  /* need to set this first */
  UI_fontstyle_set(fstyle);

  if (fstyle->kerning == 1) {
    /* for BLF_width */
    BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
  }

  float strwidth = BLF_width(fstyle->uifont_id, str, max_len);

  if ((okwidth > 0.0f) && (strwidth > okwidth)) {
    /* Ellipsis. Some compilers complain with real literal string. */
    const char sep[] = {0xe2, 0x80, 0xA6, 0x0};
    const int sep_len = sizeof(sep) - 1;
    const float sep_strwidth = BLF_width(fstyle->uifont_id, sep, sep_len + 1);

    char *rpart = NULL, rpart_buf[UI_MAX_DRAW_STR];
    float rpart_width = 0.0f;
    size_t rpart_len = 0;
    size_t final_lpart_len;

    if (rpart_sep) {
      rpart = strrchr(str, rpart_sep);

      if (rpart) {
        rpart_len = strlen(rpart);
        rpart_width = BLF_width(fstyle->uifont_id, rpart, rpart_len);
        okwidth -= rpart_width;
        strwidth -= rpart_width;

        if (okwidth < 0.0f) {
          /* Not enough place for actual label, just display protected right part.
           * Here just for safety, should never happen in real life! */
          memmove(str, rpart, rpart_len + 1);
          rpart = NULL;
          okwidth += rpart_width;
          strwidth = rpart_width;
        }
      }
    }

    const float parts_strwidth = (okwidth - sep_strwidth) / 2.0f;

    if (rpart) {
      strcpy(rpart_buf, rpart);
      *rpart = '\0';
      rpart = rpart_buf;
    }

    const size_t l_end = BLF_width_to_strlen(
        fstyle->uifont_id, str, max_len, parts_strwidth, NULL);
    if (l_end < 10 || min_ff(parts_strwidth, strwidth - okwidth) < minwidth) {
      /* If we really have no place, or we would clip a very small piece of string in the middle,
       * only show start of string.
       */
      ui_text_clip_right_ex(
          fstyle, str, max_len, okwidth, sep, sep_len, sep_strwidth, &final_lpart_len);
    }
    else {
      size_t r_offset, r_len;

      r_offset = BLF_width_to_rstrlen(fstyle->uifont_id, str, max_len, parts_strwidth, NULL);
      r_len = strlen(str + r_offset) + 1; /* +1 for the trailing '\0'. */

      if (l_end + sep_len + r_len + rpart_len > max_len) {
        /* Corner case, the str already takes all available mem,
         * and the ellipsis chars would actually add more chars.
         * Better to just trim one or two letters to the right in this case...
         * Note: with a single-char ellipsis, this should never happen! But better be safe
         * here...
         */
        ui_text_clip_right_ex(
            fstyle, str, max_len, okwidth, sep, sep_len, sep_strwidth, &final_lpart_len);
      }
      else {
        memmove(str + l_end + sep_len, str + r_offset, r_len);
        memcpy(str + l_end, sep, sep_len);
        /* -1 to remove trailing '\0'! */
        final_lpart_len = (size_t)(l_end + sep_len + r_len - 1);

        while (BLF_width(fstyle->uifont_id, str, max_len) > okwidth) {
          /* This will happen because a lot of string width processing is done in integer pixels,
           * which can introduce a rather high error in the end (about 2 pixels or so).
           * Only one char removal shall ever be needed in real-life situation... */
          r_len--;
          final_lpart_len--;
          char *c = str + l_end + sep_len;
          memmove(c, c + 1, r_len);
        }
      }
    }

    if (rpart) {
      /* Add back preserved right part to our shorten str. */
      memcpy(str + final_lpart_len, rpart, rpart_len + 1); /* +1 for trailing '\0'. */
      okwidth += rpart_width;
    }

    strwidth = BLF_width(fstyle->uifont_id, str, max_len);
  }

  if (fstyle->kerning == 1) {
    BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
  }

  BLI_assert(strwidth <= okwidth);

  return strwidth;
}

/**
 * Wrapper around UI_text_clip_middle_ex.
 */
static void ui_text_clip_middle(const uiFontStyle *fstyle, uiBut *but, const rcti *rect)
{
  /* No margin for labels! */
  const int border = ELEM(but->type, UI_BTYPE_LABEL, UI_BTYPE_MENU, UI_BTYPE_POPOVER) ?
                         0 :
                         (int)(UI_TEXT_CLIP_MARGIN + 0.5f);
  const float okwidth = (float)max_ii(BLI_rcti_size_x(rect) - border, 0);
  const size_t max_len = sizeof(but->drawstr);
  const float minwidth = (float)(UI_DPI_ICON_SIZE) / but->block->aspect * 2.0f;

  but->ofs = 0;
  but->strwidth = UI_text_clip_middle_ex(fstyle, but->drawstr, okwidth, minwidth, max_len, '\0');
}

/**
 * Like #ui_text_clip_middle(), but protect/preserve at all cost
 * the right part of the string after sep.
 * Useful for strings with shortcuts
 * (like 'AVeryLongFooBarLabelForMenuEntry|Ctrl O' -> 'AVeryLong...MenuEntry|Ctrl O').
 */
static void ui_text_clip_middle_protect_right(const uiFontStyle *fstyle,
                                              uiBut *but,
                                              const rcti *rect,
                                              const char rsep)
{
  /* No margin for labels! */
  const int border = ELEM(but->type, UI_BTYPE_LABEL, UI_BTYPE_MENU, UI_BTYPE_POPOVER) ?
                         0 :
                         (int)(UI_TEXT_CLIP_MARGIN + 0.5f);
  const float okwidth = (float)max_ii(BLI_rcti_size_x(rect) - border, 0);
  const size_t max_len = sizeof(but->drawstr);
  const float minwidth = (float)(UI_DPI_ICON_SIZE) / but->block->aspect * 2.0f;

  but->ofs = 0;
  but->strwidth = UI_text_clip_middle_ex(fstyle, but->drawstr, okwidth, minwidth, max_len, rsep);
}

/**
 * Cut off the text, taking into account the cursor location (text display while editing).
 */
static void ui_text_clip_cursor(const uiFontStyle *fstyle, uiBut *but, const rcti *rect)
{
  const int border = (int)(UI_TEXT_CLIP_MARGIN + 0.5f);
  const int okwidth = max_ii(BLI_rcti_size_x(rect) - border, 0);

  BLI_assert(but->editstr && but->pos >= 0);

  /* need to set this first */
  UI_fontstyle_set(fstyle);

  if (fstyle->kerning == 1) {
    /* for BLF_width */
    BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
  }

  /* define ofs dynamically */
  if (but->ofs > but->pos) {
    but->ofs = but->pos;
  }

  if (BLF_width(fstyle->uifont_id, but->editstr, INT_MAX) <= okwidth) {
    but->ofs = 0;
  }

  but->strwidth = BLF_width(fstyle->uifont_id, but->editstr + but->ofs, INT_MAX);

  if (but->strwidth > okwidth) {
    int len = strlen(but->editstr);

    while (but->strwidth > okwidth) {
      float width;

      /* string position of cursor */
      width = BLF_width(fstyle->uifont_id, but->editstr + but->ofs, (but->pos - but->ofs));

      /* if cursor is at 20 pixels of right side button we clip left */
      if (width > okwidth - 20) {
        ui_text_clip_give_next_off(but, but->editstr);
      }
      else {
        int bytes;
        /* shift string to the left */
        if (width < 20 && but->ofs > 0) {
          ui_text_clip_give_prev_off(but, but->editstr);
        }
        bytes = BLI_str_utf8_size(BLI_str_find_prev_char_utf8(but->editstr, but->editstr + len));
        if (bytes == -1) {
          bytes = 1;
        }
        len -= bytes;
      }

      but->strwidth = BLF_width(fstyle->uifont_id, but->editstr + but->ofs, len - but->ofs);

      if (but->strwidth < 10) {
        break;
      }
    }
  }

  if (fstyle->kerning == 1) {
    BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
  }
}

/**
 * Cut off the end of text to fit into the width of \a rect.
 *
 * \note deals with ': ' especially for number buttons
 */
static void ui_text_clip_right_label(const uiFontStyle *fstyle, uiBut *but, const rcti *rect)
{
  const int border = UI_TEXT_CLIP_MARGIN + 1;
  const int okwidth = max_ii(BLI_rcti_size_x(rect) - border, 0);
  int drawstr_len = strlen(but->drawstr);
  const char *cpend = but->drawstr + drawstr_len;

  /* need to set this first */
  UI_fontstyle_set(fstyle);

  if (fstyle->kerning == 1) {
    /* for BLF_width */
    BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
  }

  but->strwidth = BLF_width(fstyle->uifont_id, but->drawstr, sizeof(but->drawstr));
  but->ofs = 0;

  /* First shorten number-buttons eg,
   *   Translucency: 0.000
   * becomes
   *   Trans: 0.000
   */

  /* find the space after ':' separator */
  char *cpoin = strrchr(but->drawstr, ':');

  if (cpoin && (cpoin < cpend - 2)) {
    char *cp2 = cpoin;

    /* chop off the leading text, starting from the right */
    while (but->strwidth > okwidth && cp2 > but->drawstr) {
      const char *prev_utf8 = BLI_str_find_prev_char_utf8(but->drawstr, cp2);
      const int bytes = cp2 - prev_utf8;

      /* shift the text after and including cp2 back by 1 char,
       * +1 to include null terminator */
      memmove(cp2 - bytes, cp2, drawstr_len + 1);
      cp2 -= bytes;

      drawstr_len -= bytes;
      // BLI_assert(strlen(but->drawstr) == drawstr_len);

      but->strwidth = BLF_width(
          fstyle->uifont_id, but->drawstr + but->ofs, sizeof(but->drawstr) - but->ofs);
      if (but->strwidth < 10) {
        break;
      }
    }

    /* after the leading text is gone, chop off the : and following space, with ofs */
    while ((but->strwidth > okwidth) && (but->ofs < 2)) {
      ui_text_clip_give_next_off(but, but->drawstr);
      but->strwidth = BLF_width(
          fstyle->uifont_id, but->drawstr + but->ofs, sizeof(but->drawstr) - but->ofs);
      if (but->strwidth < 10) {
        break;
      }
    }
  }

  /* Now just remove trailing chars */
  /* once the label's gone, chop off the least significant digits */
  if (but->strwidth > okwidth) {
    float strwidth;
    drawstr_len = BLF_width_to_strlen(fstyle->uifont_id,
                                      but->drawstr + but->ofs,
                                      drawstr_len - but->ofs,
                                      okwidth,
                                      &strwidth) +
                  but->ofs;
    but->strwidth = strwidth;
    but->drawstr[drawstr_len] = 0;
  }

  if (fstyle->kerning == 1) {
    BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
  }
}

#ifdef WITH_INPUT_IME
static void widget_draw_text_ime_underline(const uiFontStyle *fstyle,
                                           const uiWidgetColors *wcol,
                                           const uiBut *but,
                                           const rcti *rect,
                                           const wmIMEData *ime_data,
                                           const char *drawstr)
{
  int ofs_x, width;
  int rect_x = BLI_rcti_size_x(rect);
  int sel_start = ime_data->sel_start, sel_end = ime_data->sel_end;
  float fcol[4];

  if (drawstr[0] != 0) {
    if (but->pos >= but->ofs) {
      ofs_x = BLF_width(fstyle->uifont_id, drawstr + but->ofs, but->pos - but->ofs);
    }
    else {
      ofs_x = 0;
    }

    width = BLF_width(
        fstyle->uifont_id, drawstr + but->ofs, ime_data->composite_len + but->pos - but->ofs);

    rgba_uchar_to_float(fcol, wcol->text);
    UI_draw_text_underline(rect->xmin + ofs_x,
                           rect->ymin + 6 * U.pixelsize,
                           min_ii(width, rect_x - 2) - ofs_x,
                           1,
                           fcol);

    /* draw the thick line */
    if (sel_start != -1 && sel_end != -1) {
      sel_end -= sel_start;
      sel_start += but->pos;

      if (sel_start >= but->ofs) {
        ofs_x = BLF_width(fstyle->uifont_id, drawstr + but->ofs, sel_start - but->ofs);
      }
      else {
        ofs_x = 0;
      }

      width = BLF_width(fstyle->uifont_id, drawstr + but->ofs, sel_end + sel_start - but->ofs);

      UI_draw_text_underline(rect->xmin + ofs_x,
                             rect->ymin + 6 * U.pixelsize,
                             min_ii(width, rect_x - 2) - ofs_x,
                             2,
                             fcol);
    }
  }
}
#endif /* WITH_INPUT_IME */

static bool widget_draw_text_underline_calc_center_x(const char *UNUSED(str),
                                                     const size_t str_step_ofs,
                                                     const rcti *glyph_step_bounds,
                                                     const int UNUSED(glyph_advance_x),
                                                     const rctf *glyph_bounds,
                                                     const int glyph_bearing[2],
                                                     void *user_data)
{
  /* The index of the character to get, set to the x-position. */
  int *ul_data = user_data;
  if (ul_data[0] == (int)str_step_ofs) {
    ul_data[1] = glyph_step_bounds->xmin + glyph_bearing[0] +
                 (BLI_rctf_size_x(glyph_bounds) / 2.0f);
    /* Early exit. */
    return false;
  }
  return true;
}

static void widget_draw_text(const uiFontStyle *fstyle,
                             const uiWidgetColors *wcol,
                             uiBut *but,
                             rcti *rect)
{
  int drawstr_left_len = UI_MAX_DRAW_STR;
  const char *drawstr = but->drawstr;
  const char *drawstr_right = NULL;
  bool use_right_only = false;

#ifdef WITH_INPUT_IME
  const wmIMEData *ime_data;
#endif

  UI_fontstyle_set(fstyle);

  eFontStyle_Align align;
  if (but->editstr || (but->drawflag & UI_BUT_TEXT_LEFT)) {
    align = UI_STYLE_TEXT_LEFT;
  }
  else if (but->drawflag & UI_BUT_TEXT_RIGHT) {
    align = UI_STYLE_TEXT_RIGHT;
  }
  else {
    align = UI_STYLE_TEXT_CENTER;
  }

  if (fstyle->kerning == 1) {
    /* for BLF_width */
    BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
  }

  /* Special case: when we're entering text for multiple buttons,
   * don't draw the text for any of the multi-editing buttons */
  if (UNLIKELY(but->flag & UI_BUT_DRAG_MULTI)) {
    uiBut *but_edit = ui_but_drag_multi_edit_get(but);
    if (but_edit) {
      drawstr = but_edit->editstr;
      align = UI_STYLE_TEXT_LEFT;
    }
  }
  else {
    if (but->editstr) {
      /* max length isn't used in this case,
       * we rely on string being NULL terminated. */
      drawstr_left_len = INT_MAX;

#ifdef WITH_INPUT_IME
      /* FIXME, IME is modifying 'const char *drawstr! */
      ime_data = ui_but_ime_data_get(but);

      if (ime_data && ime_data->composite_len) {
        /* insert composite string into cursor pos */
        BLI_snprintf((char *)drawstr,
                     UI_MAX_DRAW_STR,
                     "%s%s%s",
                     but->editstr,
                     ime_data->str_composite,
                     but->editstr + but->pos);
      }
      else
#endif
      {
        drawstr = but->editstr;
      }
    }
  }

  /* text button selection, cursor, composite underline */
  if (but->editstr && but->pos != -1) {
    int but_pos_ofs;
    /* Shape of the cursor for drawing. */
    rcti but_cursor_shape;

    /* text button selection */
    if ((but->selend - but->selsta) > 0) {
      int selsta_draw, selwidth_draw;

      if (drawstr[0] != 0) {
        /* We are drawing on top of widget bases. Flush cache. */
        GPU_blend(GPU_BLEND_ALPHA);
        UI_widgetbase_draw_cache_flush();
        GPU_blend(GPU_BLEND_NONE);

        if (but->selsta >= but->ofs) {
          selsta_draw = BLF_width(fstyle->uifont_id, drawstr + but->ofs, but->selsta - but->ofs);
        }
        else {
          selsta_draw = 0;
        }

        selwidth_draw = BLF_width(fstyle->uifont_id, drawstr + but->ofs, but->selend - but->ofs);

        uint pos = GPU_vertformat_attr_add(
            immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
        immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

        immUniformColor4ubv(wcol->item);
        immRecti(pos,
                 rect->xmin + selsta_draw,
                 rect->ymin + U.pixelsize,
                 min_ii(rect->xmin + selwidth_draw, rect->xmax - 2),
                 rect->ymax - U.pixelsize);

        immUnbindProgram();
      }
    }

    /* text cursor */
    but_pos_ofs = but->pos;

#ifdef WITH_INPUT_IME
    /* if is ime compositing, move the cursor */
    if (ime_data && ime_data->composite_len && ime_data->cursor_pos != -1) {
      but_pos_ofs += ime_data->cursor_pos;
    }
#endif

    if (but->pos >= but->ofs) {
      int t;
      if (drawstr[0] != 0) {
        t = BLF_width(fstyle->uifont_id, drawstr + but->ofs, but_pos_ofs - but->ofs);
      }
      else {
        t = 0;
      }
      /* We are drawing on top of widget bases. Flush cache. */
      GPU_blend(GPU_BLEND_ALPHA);
      UI_widgetbase_draw_cache_flush();
      GPU_blend(GPU_BLEND_NONE);

      uint pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

      immUniformThemeColor(TH_WIDGET_TEXT_CURSOR);

      but_cursor_shape.xmin = (rect->xmin + t) - U.pixelsize;
      but_cursor_shape.ymin = rect->ymin + U.pixelsize;
      but_cursor_shape.xmax = (rect->xmin + t) + U.pixelsize;
      but_cursor_shape.ymax = rect->ymax - U.pixelsize;

      /* draw cursor */
      immRecti(pos,
               but_cursor_shape.xmin,
               but_cursor_shape.ymin,
               but_cursor_shape.xmax,
               but_cursor_shape.ymax);

      immUnbindProgram();
    }

#ifdef WITH_INPUT_IME
    if (ime_data && ime_data->composite_len) {
      /* ime cursor following */
      if (but->pos >= but->ofs) {
        ui_but_ime_reposition(but, but_cursor_shape.xmax + 5, but_cursor_shape.ymin + 3, false);
      }

      /* composite underline */
      widget_draw_text_ime_underline(fstyle, wcol, but, rect, ime_data, drawstr);
    }
#endif
  }

  if (fstyle->kerning == 1) {
    BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
  }

#if 0
  ui_rasterpos_safe(x, y, but->aspect);
  transopts = ui_translate_buttons();
#endif

  /* cut string in 2 parts - only for menu entries */
  if ((but->drawflag & UI_BUT_HAS_SHORTCUT) && (but->editstr == NULL)) {
    if (but->flag & UI_BUT_HAS_SEP_CHAR) {
      drawstr_right = strrchr(drawstr, UI_SEP_CHAR);
      if (drawstr_right) {
        drawstr_left_len = (drawstr_right - drawstr);
        drawstr_right++;
      }
    }
  }

#ifdef USE_NUMBUTS_LR_ALIGN
  if (!drawstr_right && (but->drawflag & UI_BUT_TEXT_LEFT) &&
      ELEM(but->type, UI_BTYPE_NUM, UI_BTYPE_NUM_SLIDER) &&
      /* if we're editing or multi-drag (fake editing), then use left alignment */
      (but->editstr == NULL) && (drawstr == but->drawstr)) {
    drawstr_right = strrchr(drawstr + but->ofs, ':');
    if (drawstr_right) {
      drawstr_right++;
      drawstr_left_len = (drawstr_right - drawstr - 1);

      while (*drawstr_right == ' ') {
        drawstr_right++;
      }
    }
    else {
      /* no prefix, even so use only cpoin */
      drawstr_right = drawstr + but->ofs;
      use_right_only = true;
    }
  }
#endif

  if (!use_right_only) {
    /* for underline drawing */
    int font_xofs, font_yofs;

    int drawlen = (drawstr_left_len == INT_MAX) ? strlen(drawstr + but->ofs) :
                                                  (drawstr_left_len - but->ofs);

    if (drawlen > 0) {
      UI_fontstyle_draw_ex(fstyle,
                           rect,
                           drawstr + but->ofs,
                           wcol->text,
                           &(struct uiFontStyleDraw_Params){
                               .align = align,
                           },
                           drawlen,
                           &font_xofs,
                           &font_yofs,
                           NULL);

      if (but->menu_key != '\0') {
        const char *drawstr_ofs = drawstr + but->ofs;
        int ul_index = -1;

        {
          /* Find upper case, fallback to lower case. */
          const char *drawstr_end = drawstr_ofs + drawlen;
          const char keys[] = {but->menu_key - 32, but->menu_key};
          for (int i = 0; i < ARRAY_SIZE(keys); i++) {
            const char *drawstr_menu = strchr(drawstr_ofs, keys[i]);
            if (drawstr_menu != NULL && drawstr_menu < drawstr_end) {
              ul_index = (int)(drawstr_menu - drawstr_ofs);
              break;
            }
          }
        }

        if (ul_index != -1) {
          if (fstyle->kerning == 1) {
            BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
          }

          int ul_data[2] = {
              ul_index, /* Character index to test. */
              0,        /* Write the x-offset here. */
          };
          BLF_boundbox_foreach_glyph(fstyle->uifont_id,
                                     drawstr_ofs,
                                     ul_index + 1,
                                     widget_draw_text_underline_calc_center_x,
                                     ul_data);
          ul_data[1] -= BLF_width(fstyle->uifont_id, "_", 2) / 2.0f;

          BLF_position(fstyle->uifont_id,
                       rect->xmin + font_xofs + ul_data[1],
                       rect->ymin + font_yofs,
                       0.0f);
          BLF_color4ubv(fstyle->uifont_id, wcol->text);
          BLF_draw(fstyle->uifont_id, "_", 2);

          if (fstyle->kerning == 1) {
            BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
          }
        }
      }
    }
  }

  /* part text right aligned */
  if (drawstr_right) {
    uchar col[4];
    copy_v4_v4_uchar(col, wcol->text);
    if (but->drawflag & UI_BUT_HAS_SHORTCUT) {
      col[3] *= 0.5f;
    }

    rect->xmax -= UI_TEXT_CLIP_MARGIN;
    UI_fontstyle_draw(fstyle,
                      rect,
                      drawstr_right,
                      col,
                      &(struct uiFontStyleDraw_Params){
                          .align = UI_STYLE_TEXT_RIGHT,
                      });
  }
}

static void widget_draw_extra_icons(const uiWidgetColors *wcol,
                                    uiBut *but,
                                    rcti *rect,
                                    float alpha)
{
  const float icon_size = ICON_SIZE_FROM_BUTRECT(rect);

  /* Offset of icons from the right edge. Keep in sync
     with 'ui_but_extra_operator_icon_mouse_over_get'. */
  if (!BLI_listbase_is_empty(&but->extra_op_icons)) {
    /* Eyeballed. */
    rect->xmax -= 0.2 * icon_size;
  }

  /* Inverse order, from right to left. */
  LISTBASE_FOREACH_BACKWARD (uiButExtraOpIcon *, op_icon, &but->extra_op_icons) {
    rcti temp = *rect;
    float alpha_this = alpha;

    temp.xmin = temp.xmax - icon_size;

    if (!op_icon->highlighted) {
      alpha_this *= 0.75f;
    }

    /* Draw the icon at the center, and restore the flags after. */
    const int old_drawflags = but->drawflag;
    UI_but_drawflag_disable(but, UI_BUT_ICON_LEFT);
    widget_draw_icon(but, op_icon->icon, alpha_this, &temp, wcol->text);
    but->drawflag = old_drawflags;

    rect->xmax -= icon_size;
  }
}

static void widget_draw_node_link_socket(const uiWidgetColors *wcol,
                                         const rcti *rect,
                                         uiBut *but,
                                         float alpha)
{
  /* Node socket pointer can be passed as custom_data, see UI_but_node_link_set(). */
  if (but->custom_data) {
    const float scale = 0.9f / but->block->aspect;

    float col[4];
    rgba_uchar_to_float(col, but->col);
    col[3] *= alpha;

    GPU_blend(GPU_BLEND_ALPHA);
    UI_widgetbase_draw_cache_flush();
    GPU_blend(GPU_BLEND_NONE);

    ED_node_socket_draw(but->custom_data, rect, col, scale);
  }
  else {
    widget_draw_icon(but, ICON_LAYER_USED, alpha, rect, wcol->text);
  }
}

/* draws text and icons for buttons */
static void widget_draw_text_icon(const uiFontStyle *fstyle,
                                  const uiWidgetColors *wcol,
                                  uiBut *but,
                                  rcti *rect)
{
  const bool show_menu_icon = ui_but_draw_menu_icon(but);
  const float alpha = (float)wcol->text[3] / 255.0f;
  char password_str[UI_MAX_DRAW_STR];
  bool no_text_padding = but->drawflag & UI_BUT_NO_TEXT_PADDING;

  ui_but_text_password_hide(password_str, but, false);

  /* check for button text label */
  if (ELEM(but->type, UI_BTYPE_MENU, UI_BTYPE_POPOVER) && (but->flag & UI_BUT_NODE_LINK)) {
    rcti temp = *rect;
    const int size = BLI_rcti_size_y(rect) + 1; /* Not the icon size! */

    if (but->drawflag & UI_BUT_ICON_LEFT) {
      temp.xmax = rect->xmin + size;
      rect->xmin = temp.xmax;
      /* Further padding looks off. */
      no_text_padding = true;
    }
    else {
      temp.xmin = rect->xmax - size;
      rect->xmax = temp.xmin;
    }

    widget_draw_node_link_socket(wcol, &temp, but, alpha);
  }

  /* If there's an icon too (made with uiDefIconTextBut) then draw the icon
   * and offset the text label to accommodate it */

  /* Big previews with optional text label below */
  if (but->flag & UI_BUT_ICON_PREVIEW && ui_block_is_menu(but->block)) {
    const BIFIconID icon = ui_but_icon(but);
    int icon_size = BLI_rcti_size_y(rect);
    int text_size = 0;

    /* This is a bit brittle, but avoids adding an 'UI_BUT_HAS_LABEL' flag to but... */
    if (icon_size > BLI_rcti_size_x(rect)) {
      /* button is not square, it has extra height for label */
      text_size = UI_UNIT_Y;
      icon_size -= text_size;
    }

    /* draw icon in rect above the space reserved for the label */
    rect->ymin += text_size;
    GPU_blend(GPU_BLEND_ALPHA);
    widget_draw_preview(icon, alpha, rect);
    GPU_blend(GPU_BLEND_NONE);

    /* offset rect to draw label in */
    rect->ymin -= text_size;
    rect->ymax -= icon_size;

    /* vertically centering text */
    rect->ymin += UI_UNIT_Y / 2;
  }
  /* Icons on the left with optional text label on the right */
  else if (but->flag & UI_HAS_ICON || show_menu_icon) {
    const bool is_tool = ((but->icon != ICON_NONE) & UI_but_is_tool(but));

    /* XXX add way to draw icons at a different size!
     * Use small icons for popup. */
#ifdef USE_UI_TOOLBAR_HACK
    const float aspect_orig = but->block->aspect;
    if (is_tool && (but->block->flag & UI_BLOCK_POPOVER)) {
      but->block->aspect *= 2.0f;
    }
#endif

    const BIFIconID icon = ui_but_icon(but);
    const int icon_size_init = is_tool ? ICON_DEFAULT_HEIGHT_TOOLBAR : ICON_DEFAULT_HEIGHT;
    const float icon_size = icon_size_init / (but->block->aspect * U.inv_dpi_fac);
    const float icon_padding = 2 * UI_DPI_FAC;

#ifdef USE_UI_TOOLBAR_HACK
    if (is_tool) {
      /* pass (even if its a menu toolbar) */
      but->drawflag |= UI_BUT_TEXT_LEFT;
      but->drawflag |= UI_BUT_ICON_LEFT;
    }
#endif

    /* menu item - add some more padding so menus don't feel cramped. it must
     * be part of the button so that this area is still clickable */
    if (is_tool) {
      /* pass (even if its a menu toolbar) */
    }
    else if (ui_block_is_pie_menu(but->block)) {
      if (but->emboss == UI_EMBOSS_RADIAL) {
        rect->xmin += 0.3f * U.widget_unit;
      }
    }
    /* Menu items, but only if they are not icon-only (rare). */
    else if (ui_block_is_menu(but->block) && but->drawstr[0]) {
      rect->xmin += 0.2f * U.widget_unit;
    }

    /* By default icon is the color of text, but can optionally override with but->col. */
    widget_draw_icon(but, icon, alpha, rect, (but->col[3] != 0) ? but->col : wcol->text);

    if (show_menu_icon) {
      BLI_assert(but->block->content_hints & UI_BLOCK_CONTAINS_SUBMENU_BUT);
      widget_draw_submenu_tria(but, rect, wcol);
    }

#ifdef USE_UI_TOOLBAR_HACK
    but->block->aspect = aspect_orig;
#endif

    rect->xmin += icon_size + icon_padding;
  }

  if (!no_text_padding) {
    const int text_padding = (UI_TEXT_MARGIN_X * U.widget_unit) / but->block->aspect;
    if (but->editstr) {
      rect->xmin += text_padding;
    }
    else if (but->flag & UI_BUT_DRAG_MULTI) {
      const bool text_is_edited = ui_but_drag_multi_edit_get(but) != NULL;
      if (text_is_edited || (but->drawflag & UI_BUT_TEXT_LEFT)) {
        rect->xmin += text_padding;
      }
    }
    else if (but->drawflag & UI_BUT_TEXT_LEFT) {
      rect->xmin += text_padding;
    }
    else if (but->drawflag & UI_BUT_TEXT_RIGHT) {
      rect->xmax -= text_padding;
    }
  }
  else {
    /* In case a separate text label and some other button are placed under each other,
       and the outline of the button does not contrast with the background.
       Add an offset (thickness of the outline) so that the text does not stick out visually. */
    if (but->drawflag & UI_BUT_TEXT_LEFT) {
      rect->xmin += U.pixelsize;
    }
    else if (but->drawflag & UI_BUT_TEXT_RIGHT) {
      rect->xmax -= U.pixelsize;
    }
  }

  /* Menu contains sub-menu items with triangle icon on their right. Shortcut
   * strings should be drawn with some padding to the right then. */
  if (ui_block_is_menu(but->block) &&
      (but->block->content_hints & UI_BLOCK_CONTAINS_SUBMENU_BUT)) {
    rect->xmax -= UI_MENU_SUBMENU_PADDING;
  }

  /* extra icons, e.g. 'x' icon to clear text or icon for eyedropper */
  widget_draw_extra_icons(wcol, but, rect, alpha);

  /* clip but->drawstr to fit in available space */
  if (but->editstr && but->pos >= 0) {
    ui_text_clip_cursor(fstyle, but, rect);
  }
  else if (but->drawstr[0] == '\0') {
    /* bypass text clipping on icon buttons */
    but->ofs = 0;
    but->strwidth = 0;
  }
  else if (ELEM(but->type, UI_BTYPE_NUM, UI_BTYPE_NUM_SLIDER)) {
    ui_text_clip_right_label(fstyle, but, rect);
  }
  else if (but->flag & UI_BUT_HAS_SEP_CHAR) {
    /* Clip middle, but protect in all case right part containing the shortcut, if any. */
    ui_text_clip_middle_protect_right(fstyle, but, rect, UI_SEP_CHAR);
  }
  else {
    ui_text_clip_middle(fstyle, but, rect);
  }

  /* always draw text for textbutton cursor */
  widget_draw_text(fstyle, wcol, but, rect);

  ui_but_text_password_hide(password_str, but, true);

  /* if a widget uses font shadow it has to be deactivated now */
  BLF_disable(fstyle->uifont_id, BLF_SHADOW);
}

#undef UI_TEXT_CLIP_MARGIN

/** \} */

/* -------------------------------------------------------------------- */
/** \name Widget State Management
 *
 * Adjust widget display based on animated, driven, overridden ... etc.
 * \{ */

/* put all widget colors on half alpha, use local storage */
static void ui_widget_color_disabled(uiWidgetType *wt, const int state)
{
  static uiWidgetColors wcol_theme_s;

  wcol_theme_s = *wt->wcol_theme;

  const float factor = widget_alpha_factor(state);

  wcol_theme_s.outline[3] *= factor;
  wcol_theme_s.inner[3] *= factor;
  wcol_theme_s.inner_sel[3] *= factor;
  wcol_theme_s.item[3] *= factor;
  wcol_theme_s.text[3] *= factor;
  wcol_theme_s.text_sel[3] *= factor;

  wt->wcol_theme = &wcol_theme_s;
}

static void widget_active_color(uiWidgetColors *wcol)
{
  const bool dark = (rgb_to_grayscale_byte(wcol->text) > rgb_to_grayscale_byte(wcol->inner));
  color_mul_hsl_v3(wcol->inner, 1.0f, 1.15f, dark ? 1.2f : 1.1f);
  color_mul_hsl_v3(wcol->outline, 1.0f, 1.15f, 1.15f);
  color_mul_hsl_v3(wcol->text, 1.0f, 1.15f, dark ? 1.25f : 0.8f);
}

static const uchar *widget_color_blend_from_flags(const uiWidgetStateColors *wcol_state,
                                                  int state,
                                                  int drawflag,
                                                  const eUIEmbossType emboss)
{
  /* Explicitly require #UI_EMBOSS_NONE_OR_STATUS for color blending with no emboss. */
  if (emboss == UI_EMBOSS_NONE) {
    return NULL;
  }

  if (drawflag & UI_BUT_ANIMATED_CHANGED) {
    return wcol_state->inner_changed_sel;
  }
  if (state & UI_BUT_ANIMATED_KEY) {
    return wcol_state->inner_key_sel;
  }
  if (state & UI_BUT_ANIMATED) {
    return wcol_state->inner_anim_sel;
  }
  if (state & UI_BUT_DRIVEN) {
    return wcol_state->inner_driven_sel;
  }
  if (state & UI_BUT_OVERRIDDEN) {
    return wcol_state->inner_overridden_sel;
  }
  return NULL;
}

/* copy colors from theme, and set changes in it based on state */
static void widget_state(uiWidgetType *wt, int state, int drawflag, eUIEmbossType emboss)
{
  uiWidgetStateColors *wcol_state = wt->wcol_state;

  if ((state & UI_BUT_LIST_ITEM) && !(state & UI_STATE_TEXT_INPUT)) {
    /* Override default widget's colors. */
    bTheme *btheme = UI_GetTheme();
    wt->wcol_theme = &btheme->tui.wcol_list_item;

    if (state & (UI_BUT_DISABLED | UI_BUT_INACTIVE | UI_SEARCH_FILTER_NO_MATCH)) {
      ui_widget_color_disabled(wt, state);
    }
  }

  wt->wcol = *(wt->wcol_theme);

  const uchar *color_blend = widget_color_blend_from_flags(wcol_state, state, drawflag, emboss);

  if (state & UI_SELECT) {
    copy_v4_v4_uchar(wt->wcol.inner, wt->wcol.inner_sel);
    if (color_blend != NULL) {
      color_blend_v3_v3(wt->wcol.inner, color_blend, wcol_state->blend);
    }

    copy_v3_v3_uchar(wt->wcol.text, wt->wcol.text_sel);

    if (state & UI_SELECT) {
      SWAP(short, wt->wcol.shadetop, wt->wcol.shadedown);
    }
  }
  else {
    if (state & UI_BUT_ACTIVE_DEFAULT) {
      copy_v4_v4_uchar(wt->wcol.inner, wt->wcol.inner_sel);
      copy_v4_v4_uchar(wt->wcol.text, wt->wcol.text_sel);
    }
    if (color_blend != NULL) {
      color_blend_v3_v3(wt->wcol.inner, color_blend, wcol_state->blend);
    }

    /* Add "hover" highlight. Ideally this could apply in all cases,
     * even if UI_SELECT. But currently this causes some flickering
     * as buttons can be created and updated without respect to mouse
     * position and so can draw without UI_ACTIVE set.  See D6503. */
    if (state & UI_ACTIVE) {
      widget_active_color(&wt->wcol);
    }
  }

  if (state & UI_BUT_REDALERT) {
    const uchar red[4] = {255, 0, 0};
    if (wt->draw && emboss != UI_EMBOSS_NONE) {
      color_blend_v3_v3(wt->wcol.inner, red, 0.4f);
    }
    else {
      color_blend_v3_v3(wt->wcol.text, red, 0.4f);
    }
  }

  if (state & UI_BUT_DRAG_MULTI) {
    /* the button isn't SELECT but we're editing this so draw with sel color */
    copy_v4_v4_uchar(wt->wcol.inner, wt->wcol.inner_sel);
    SWAP(short, wt->wcol.shadetop, wt->wcol.shadedown);
    color_blend_v3_v3(wt->wcol.text, wt->wcol.text_sel, 0.85f);
  }

  if (state & UI_BUT_NODE_ACTIVE) {
    const uchar blue[4] = {86, 128, 194};
    color_blend_v3_v3(wt->wcol.inner, blue, 0.3f);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Widget Types
 * \{ */

/* sliders use special hack which sets 'item' as inner when drawing filling */
static void widget_state_numslider(uiWidgetType *wt, int state, int drawflag, eUIEmbossType emboss)
{
  uiWidgetStateColors *wcol_state = wt->wcol_state;

  /* call this for option button */
  widget_state(wt, state, drawflag, emboss);

  const uchar *color_blend = widget_color_blend_from_flags(wcol_state, state, drawflag, emboss);
  if (color_blend != NULL) {
    /* Set the slider 'item' so that it reflects state settings too.
     * De-saturate so the color of the slider doesn't conflict with the blend color,
     * which can make the color hard to see when the slider is set to full (see T66102). */
    wt->wcol.item[0] = wt->wcol.item[1] = wt->wcol.item[2] = rgb_to_grayscale_byte(wt->wcol.item);
    color_blend_v3_v3(wt->wcol.item, color_blend, wcol_state->blend);
    color_ensure_contrast_v3(wt->wcol.item, wt->wcol.inner, 30);
  }

  if (state & UI_SELECT) {
    SWAP(short, wt->wcol.shadetop, wt->wcol.shadedown);
  }
}

/* labels use theme colors for text */
static void widget_state_option_menu(uiWidgetType *wt,
                                     int state,
                                     int drawflag,
                                     eUIEmbossType emboss)
{
  const bTheme *btheme = UI_GetTheme();

  const uiWidgetColors *old_wcol = wt->wcol_theme;
  uiWidgetColors wcol_menu_option = *wt->wcol_theme;

  /* Override the checkbox theme colors to use the menu-back text colors. */
  copy_v3_v3_uchar(wcol_menu_option.text, btheme->tui.wcol_menu_back.text);
  copy_v3_v3_uchar(wcol_menu_option.text_sel, btheme->tui.wcol_menu_back.text_sel);
  wt->wcol_theme = &wcol_menu_option;

  widget_state(wt, state, drawflag, emboss);

  wt->wcol_theme = old_wcol;
}

static void widget_state_nothing(uiWidgetType *wt,
                                 int UNUSED(state),
                                 int UNUSED(drawflag),
                                 eUIEmbossType UNUSED(emboss))
{
  wt->wcol = *(wt->wcol_theme);
}

/* special case, button that calls pulldown */
static void widget_state_pulldown(uiWidgetType *wt,
                                  int UNUSED(state),
                                  int UNUSED(drawflag),
                                  eUIEmbossType UNUSED(emboss))
{
  wt->wcol = *(wt->wcol_theme);
}

/* special case, pie menu items */
static void widget_state_pie_menu_item(uiWidgetType *wt,
                                       int state,
                                       int UNUSED(drawflag),
                                       eUIEmbossType UNUSED(emboss))
{
  wt->wcol = *(wt->wcol_theme);

  /* active and disabled (not so common) */
  if ((state & UI_BUT_DISABLED) && (state & UI_ACTIVE)) {
    color_blend_v3_v3(wt->wcol.text, wt->wcol.text_sel, 0.5f);
    /* draw the backdrop at low alpha, helps navigating with keys
     * when disabled items are active */
    copy_v4_v4_uchar(wt->wcol.inner, wt->wcol.item);
    wt->wcol.inner[3] = 64;
  }
  else {
    /* regular active */
    if (state & (UI_SELECT | UI_ACTIVE)) {
      copy_v3_v3_uchar(wt->wcol.text, wt->wcol.text_sel);
    }
    else if (state & (UI_BUT_DISABLED | UI_BUT_INACTIVE)) {
      /* regular disabled */
      color_blend_v3_v3(wt->wcol.text, wt->wcol.inner, 0.5f);
    }

    if (state & UI_SELECT) {
      copy_v4_v4_uchar(wt->wcol.inner, wt->wcol.inner_sel);
    }
    else if (state & UI_ACTIVE) {
      copy_v4_v4_uchar(wt->wcol.inner, wt->wcol.item);
    }
  }
}

/* special case, menu items */
static void widget_state_menu_item(uiWidgetType *wt,
                                   int state,
                                   int UNUSED(drawflag),
                                   eUIEmbossType UNUSED(emboss))
{
  wt->wcol = *(wt->wcol_theme);

  /* active and disabled (not so common) */
  if ((state & UI_BUT_DISABLED) && (state & UI_ACTIVE)) {
    /* draw the backdrop at low alpha, helps navigating with keys
     * when disabled items are active */
    wt->wcol.text[3] = 128;
    color_blend_v3_v3(wt->wcol.inner, wt->wcol.text, 0.5f);
    wt->wcol.inner[3] = 64;
  }
  else {
    /* regular active */
    if (state & UI_ACTIVE) {
      copy_v3_v3_uchar(wt->wcol.text, wt->wcol.text_sel);
    }
    else if (state & (UI_BUT_DISABLED | UI_BUT_INACTIVE)) {
      /* regular disabled */
      color_blend_v3_v3(wt->wcol.text, wt->wcol.inner, 0.5f);
    }

    if (state & UI_ACTIVE) {
      copy_v4_v4_uchar(wt->wcol.inner, wt->wcol.inner_sel);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Menu Backdrop
 * \{ */

/* outside of rect, rad to left/bottom/right */
static void widget_softshadow(const rcti *rect, int roundboxalign, const float radin)
{
  bTheme *btheme = UI_GetTheme();
  uiWidgetBase wtb;
  rcti rect1 = *rect;
  float triangle_strip[WIDGET_SIZE_MAX * 2 + 2][2];
  const float radout = UI_ThemeMenuShadowWidth();

  /* disabled shadow */
  if (radout == 0.0f) {
    return;
  }

  /* prevent tooltips to not show round shadow */
  if (radout > 0.2f * BLI_rcti_size_y(&rect1)) {
    rect1.ymax -= 0.2f * BLI_rcti_size_y(&rect1);
  }
  else {
    rect1.ymax -= radout;
  }

  /* inner part */
  const int totvert = round_box_shadow_edges(wtb.inner_v,
                                             &rect1,
                                             radin,
                                             roundboxalign &
                                                 (UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT),
                                             0.0f);

  /* we draw a number of increasing size alpha quad strips */
  const float alphastep = 3.0f * btheme->tui.menu_shadow_fac / radout;

  const uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  for (int step = 1; step <= (int)radout; step++) {
    const float expfac = sqrtf(step / radout);

    round_box_shadow_edges(wtb.outer_v, &rect1, radin, UI_CNR_ALL, (float)step);

    immUniformColor4f(0.0f, 0.0f, 0.0f, alphastep * (1.0f - expfac));

    widget_verts_to_triangle_strip(&wtb, totvert, triangle_strip);

    widget_draw_vertex_buffer(pos, 0, GPU_PRIM_TRI_STRIP, triangle_strip, NULL, totvert * 2);
  }

  immUnbindProgram();
}

static void widget_menu_back(uiWidgetColors *wcol, rcti *rect, int flag, int direction)
{
  uiWidgetBase wtb;
  int roundboxalign = UI_CNR_ALL;

  widget_init(&wtb);

  /* menu is 2nd level or deeper */
  if (flag & UI_BLOCK_POPUP) {
    // rect->ymin -= 4.0;
    // rect->ymax += 4.0;
  }
  else if (direction == UI_DIR_DOWN) {
    roundboxalign = (UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT);
    rect->ymin -= 0.1f * U.widget_unit;
  }
  else if (direction == UI_DIR_UP) {
    roundboxalign = UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT;
    rect->ymax += 0.1f * U.widget_unit;
  }

  GPU_blend(GPU_BLEND_ALPHA);
  widget_softshadow(rect, roundboxalign, wcol->roundness * U.widget_unit);

  round_box_edges(&wtb, roundboxalign, rect, wcol->roundness * U.widget_unit);
  wtb.draw_emboss = false;
  widgetbase_draw(&wtb, wcol);

  GPU_blend(GPU_BLEND_NONE);
}

static void ui_hsv_cursor(float x, float y)
{
  const uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformColor3f(1.0f, 1.0f, 1.0f);
  imm_draw_circle_fill_2d(pos, x, y, 3.0f * U.pixelsize, 8);

  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_smooth(true);
  immUniformColor3f(0.0f, 0.0f, 0.0f);
  imm_draw_circle_wire_2d(pos, x, y, 3.0f * U.pixelsize, 12);
  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);

  immUnbindProgram();
}

void ui_hsvcircle_vals_from_pos(
    const rcti *rect, const float mx, const float my, float *r_val_rad, float *r_val_dist)
{
  /* duplication of code... well, simple is better now */
  const float centx = BLI_rcti_cent_x_fl(rect);
  const float centy = BLI_rcti_cent_y_fl(rect);
  const float radius = (float)min_ii(BLI_rcti_size_x(rect), BLI_rcti_size_y(rect)) / 2.0f;
  const float m_delta[2] = {mx - centx, my - centy};
  const float dist_sq = len_squared_v2(m_delta);

  *r_val_dist = (dist_sq < (radius * radius)) ? sqrtf(dist_sq) / radius : 1.0f;
  *r_val_rad = atan2f(m_delta[0], m_delta[1]) / (2.0f * (float)M_PI) + 0.5f;
}

/* cursor in hsv circle, in float units -1 to 1, to map on radius */
void ui_hsvcircle_pos_from_vals(
    const ColorPicker *cpicker, const rcti *rect, const float *hsv, float *r_xpos, float *r_ypos)
{
  /* duplication of code... well, simple is better now */
  const float centx = BLI_rcti_cent_x_fl(rect);
  const float centy = BLI_rcti_cent_y_fl(rect);
  const float radius = (float)min_ii(BLI_rcti_size_x(rect), BLI_rcti_size_y(rect)) / 2.0f;

  const float ang = 2.0f * (float)M_PI * hsv[0] + (float)M_PI_2;

  float radius_t;
  if (cpicker->use_color_cubic && (U.color_picker_type == USER_CP_CIRCLE_HSV)) {
    radius_t = (1.0f - pow3f(1.0f - hsv[1]));
  }
  else {
    radius_t = hsv[1];
  }

  const float rad = clamp_f(radius_t, 0.0f, 1.0f) * radius;
  *r_xpos = centx + cosf(-ang) * rad;
  *r_ypos = centy + sinf(-ang) * rad;
}

static void ui_draw_but_HSVCIRCLE(uiBut *but, const uiWidgetColors *wcol, const rcti *rect)
{
  /* TODO(merwin): reimplement as shader for pixel-perfect colors */

  const int tot = 64;
  const float radstep = 2.0f * (float)M_PI / (float)tot;
  const float centx = BLI_rcti_cent_x_fl(rect);
  const float centy = BLI_rcti_cent_y_fl(rect);
  const float radius = (float)min_ii(BLI_rcti_size_x(rect), BLI_rcti_size_y(rect)) / 2.0f;

  ColorPicker *cpicker = but->custom_data;
  float rgb[3], hsv[3], rgb_center[3];
  const bool is_color_gamma = ui_but_is_color_gamma(but);

  /* Initialize for compatibility. */
  copy_v3_v3(hsv, cpicker->hsv_perceptual);

  /* Compute current hue. */
  ui_but_v3_get(but, rgb);
  ui_scene_linear_to_perceptual_space(but, rgb);
  ui_color_picker_rgb_to_hsv_compat(rgb, hsv);

  CLAMP(hsv[2], 0.0f, 1.0f); /* for display only */

  /* exception: if 'lock' is set
   * lock the value of the color wheel to 1.
   * Useful for color correction tools where you're only interested in hue. */
  if (cpicker->use_color_lock) {
    if (U.color_picker_type == USER_CP_CIRCLE_HSV) {
      hsv[2] = 1.0f;
    }
    else {
      hsv[2] = 0.5f;
    }
  }

  const float hsv_center[3] = {0.0f, 0.0f, hsv[2]};
  ui_color_picker_hsv_to_rgb(hsv_center, rgb_center);
  ui_perceptual_to_scene_linear_space(but, rgb_center);

  if (!is_color_gamma) {
    ui_block_cm_to_display_space_v3(but->block, rgb_center);
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  const uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);

  immBegin(GPU_PRIM_TRI_FAN, tot + 2);
  immAttr3fv(color, rgb_center);
  immVertex2f(pos, centx, centy);

  float ang = 0.0f;
  for (int a = 0; a <= tot; a++, ang += radstep) {
    const float si = sinf(ang);
    const float co = cosf(ang);
    float hsv_ang[3];
    float rgb_ang[3];

    ui_hsvcircle_vals_from_pos(
        rect, centx + co * radius, centy + si * radius, hsv_ang, hsv_ang + 1);
    hsv_ang[2] = hsv[2];

    ui_color_picker_hsv_to_rgb(hsv_ang, rgb_ang);
    ui_perceptual_to_scene_linear_space(but, rgb_ang);

    if (!is_color_gamma) {
      ui_block_cm_to_display_space_v3(but->block, rgb_ang);
    }

    immAttr3fv(color, rgb_ang);
    immVertex2f(pos, centx + co * radius, centy + si * radius);
  }
  immEnd();
  immUnbindProgram();

  /* fully rounded outline */
  format = immVertexFormat();
  pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_smooth(true);

  immUniformColor3ubv(wcol->outline);
  imm_draw_circle_wire_2d(pos, centx, centy, radius, tot);

  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);

  /* cursor */
  copy_v3_v3(hsv, cpicker->hsv_perceptual);
  ui_but_v3_get(but, rgb);
  ui_scene_linear_to_perceptual_space(but, rgb);
  ui_color_picker_rgb_to_hsv_compat(rgb, hsv);

  float xpos, ypos;
  ui_hsvcircle_pos_from_vals(cpicker, rect, hsv, &xpos, &ypos);
  ui_hsv_cursor(xpos, ypos);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Custom Buttons
 * \{ */

/* draws in resolution of 48x4 colors */
void ui_draw_gradient(const rcti *rect,
                      const float hsv[3],
                      const eButGradientType type,
                      const float alpha)
{
  /* allows for 4 steps (red->yellow) */
  const int steps = 48;
  const float color_step = 1.0f / steps;
  int a;
  const float h = hsv[0], s = hsv[1], v = hsv[2];
  float dx, dy, sx1, sx2, sy;
  float col0[4][3]; /* left half, rect bottom to top */
  float col1[4][3]; /* right half, rect bottom to top */

  /* draw series of gouraud rects */

  switch (type) {
    case UI_GRAD_SV:
      hsv_to_rgb(h, 0.0, 0.0, &col1[0][0], &col1[0][1], &col1[0][2]);
      hsv_to_rgb(h, 0.0, 0.333, &col1[1][0], &col1[1][1], &col1[1][2]);
      hsv_to_rgb(h, 0.0, 0.666, &col1[2][0], &col1[2][1], &col1[2][2]);
      hsv_to_rgb(h, 0.0, 1.0, &col1[3][0], &col1[3][1], &col1[3][2]);
      break;
    case UI_GRAD_HV:
      hsv_to_rgb(0.0, s, 0.0, &col1[0][0], &col1[0][1], &col1[0][2]);
      hsv_to_rgb(0.0, s, 0.333, &col1[1][0], &col1[1][1], &col1[1][2]);
      hsv_to_rgb(0.0, s, 0.666, &col1[2][0], &col1[2][1], &col1[2][2]);
      hsv_to_rgb(0.0, s, 1.0, &col1[3][0], &col1[3][1], &col1[3][2]);
      break;
    case UI_GRAD_HS:
      hsv_to_rgb(0.0, 0.0, v, &col1[0][0], &col1[0][1], &col1[0][2]);
      hsv_to_rgb(0.0, 0.333, v, &col1[1][0], &col1[1][1], &col1[1][2]);
      hsv_to_rgb(0.0, 0.666, v, &col1[2][0], &col1[2][1], &col1[2][2]);
      hsv_to_rgb(0.0, 1.0, v, &col1[3][0], &col1[3][1], &col1[3][2]);
      break;
    case UI_GRAD_H:
      hsv_to_rgb(0.0, 1.0, 1.0, &col1[0][0], &col1[0][1], &col1[0][2]);
      copy_v3_v3(col1[1], col1[0]);
      copy_v3_v3(col1[2], col1[0]);
      copy_v3_v3(col1[3], col1[0]);
      break;
    case UI_GRAD_S:
      hsv_to_rgb(1.0, 0.0, 1.0, &col1[1][0], &col1[1][1], &col1[1][2]);
      copy_v3_v3(col1[0], col1[1]);
      copy_v3_v3(col1[2], col1[1]);
      copy_v3_v3(col1[3], col1[1]);
      break;
    case UI_GRAD_V:
      hsv_to_rgb(1.0, 1.0, 0.0, &col1[2][0], &col1[2][1], &col1[2][2]);
      copy_v3_v3(col1[0], col1[2]);
      copy_v3_v3(col1[1], col1[2]);
      copy_v3_v3(col1[3], col1[2]);
      break;
    default:
      BLI_assert(!"invalid 'type' argument");
      hsv_to_rgb(1.0, 1.0, 1.0, &col1[2][0], &col1[2][1], &col1[2][2]);
      copy_v3_v3(col1[0], col1[2]);
      copy_v3_v3(col1[1], col1[2]);
      copy_v3_v3(col1[3], col1[2]);
      break;
  }

  /* old below */
  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  const uint col = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);

  immBegin(GPU_PRIM_TRIS, steps * 3 * 6);

  /* 0.999 = prevent float inaccuracy for steps */
  for (dx = 0.0f; dx < 0.999f; dx += color_step) {
    const float dx_next = dx + color_step;

    /* previous color */
    copy_v3_v3(col0[0], col1[0]);
    copy_v3_v3(col0[1], col1[1]);
    copy_v3_v3(col0[2], col1[2]);
    copy_v3_v3(col0[3], col1[3]);

    /* new color */
    switch (type) {
      case UI_GRAD_SV:
        hsv_to_rgb(h, dx, 0.0, &col1[0][0], &col1[0][1], &col1[0][2]);
        hsv_to_rgb(h, dx, 0.333, &col1[1][0], &col1[1][1], &col1[1][2]);
        hsv_to_rgb(h, dx, 0.666, &col1[2][0], &col1[2][1], &col1[2][2]);
        hsv_to_rgb(h, dx, 1.0, &col1[3][0], &col1[3][1], &col1[3][2]);
        break;
      case UI_GRAD_HV:
        hsv_to_rgb(dx_next, s, 0.0, &col1[0][0], &col1[0][1], &col1[0][2]);
        hsv_to_rgb(dx_next, s, 0.333, &col1[1][0], &col1[1][1], &col1[1][2]);
        hsv_to_rgb(dx_next, s, 0.666, &col1[2][0], &col1[2][1], &col1[2][2]);
        hsv_to_rgb(dx_next, s, 1.0, &col1[3][0], &col1[3][1], &col1[3][2]);
        break;
      case UI_GRAD_HS:
        hsv_to_rgb(dx_next, 0.0, v, &col1[0][0], &col1[0][1], &col1[0][2]);
        hsv_to_rgb(dx_next, 0.333, v, &col1[1][0], &col1[1][1], &col1[1][2]);
        hsv_to_rgb(dx_next, 0.666, v, &col1[2][0], &col1[2][1], &col1[2][2]);
        hsv_to_rgb(dx_next, 1.0, v, &col1[3][0], &col1[3][1], &col1[3][2]);
        break;
      case UI_GRAD_H:
        /* annoying but without this the color shifts - could be solved some other way
         * - campbell */
        hsv_to_rgb(dx_next, 1.0, 1.0, &col1[0][0], &col1[0][1], &col1[0][2]);
        copy_v3_v3(col1[1], col1[0]);
        copy_v3_v3(col1[2], col1[0]);
        copy_v3_v3(col1[3], col1[0]);
        break;
      case UI_GRAD_S:
        hsv_to_rgb(h, dx, 1.0, &col1[1][0], &col1[1][1], &col1[1][2]);
        copy_v3_v3(col1[0], col1[1]);
        copy_v3_v3(col1[2], col1[1]);
        copy_v3_v3(col1[3], col1[1]);
        break;
      case UI_GRAD_V:
        hsv_to_rgb(h, 1.0, dx, &col1[2][0], &col1[2][1], &col1[2][2]);
        copy_v3_v3(col1[0], col1[2]);
        copy_v3_v3(col1[1], col1[2]);
        copy_v3_v3(col1[3], col1[2]);
        break;
      default:
        break;
    }

    /* rect */
    sx1 = rect->xmin + dx * BLI_rcti_size_x(rect);
    sx2 = rect->xmin + dx_next * BLI_rcti_size_x(rect);
    sy = rect->ymin;
    dy = (float)BLI_rcti_size_y(rect) / 3.0f;

    for (a = 0; a < 3; a++, sy += dy) {
      immAttr4f(col, col0[a][0], col0[a][1], col0[a][2], alpha);
      immVertex2f(pos, sx1, sy);

      immAttr4f(col, col1[a][0], col1[a][1], col1[a][2], alpha);
      immVertex2f(pos, sx2, sy);

      immAttr4f(col, col1[a + 1][0], col1[a + 1][1], col1[a + 1][2], alpha);
      immVertex2f(pos, sx2, sy + dy);

      immAttr4f(col, col0[a][0], col0[a][1], col0[a][2], alpha);
      immVertex2f(pos, sx1, sy);

      immAttr4f(col, col1[a + 1][0], col1[a + 1][1], col1[a + 1][2], alpha);
      immVertex2f(pos, sx2, sy + dy);

      immAttr4f(col, col0[a + 1][0], col0[a + 1][1], col0[a + 1][2], alpha);
      immVertex2f(pos, sx1, sy + dy);
    }
  }
  immEnd();

  immUnbindProgram();
}

void ui_hsvcube_pos_from_vals(
    const uiButHSVCube *hsv_but, const rcti *rect, const float *hsv, float *r_xp, float *r_yp)
{
  float x = 0.0f, y = 0.0f;

  switch (hsv_but->gradient_type) {
    case UI_GRAD_SV:
      x = hsv[1];
      y = hsv[2];
      break;
    case UI_GRAD_HV:
      x = hsv[0];
      y = hsv[2];
      break;
    case UI_GRAD_HS:
      x = hsv[0];
      y = hsv[1];
      break;
    case UI_GRAD_H:
      x = hsv[0];
      y = 0.5;
      break;
    case UI_GRAD_S:
      x = hsv[1];
      y = 0.5;
      break;
    case UI_GRAD_V:
      x = hsv[2];
      y = 0.5;
      break;
    case UI_GRAD_L_ALT:
      x = 0.5f;
      /* exception only for value strip - use the range set in but->min/max */
      y = hsv[2];
      break;
    case UI_GRAD_V_ALT:
      x = 0.5f;
      /* exception only for value strip - use the range set in but->min/max */
      y = (hsv[2] - hsv_but->but.softmin) / (hsv_but->but.softmax - hsv_but->but.softmin);
      break;
  }

  /* cursor */
  *r_xp = rect->xmin + x * BLI_rcti_size_x(rect);
  *r_yp = rect->ymin + y * BLI_rcti_size_y(rect);
}

static void ui_draw_but_HSVCUBE(uiBut *but, const rcti *rect)
{
  const uiButHSVCube *hsv_but = (uiButHSVCube *)but;
  float rgb[3];
  float x = 0.0f, y = 0.0f;
  ColorPicker *cpicker = but->custom_data;
  float *hsv = cpicker->hsv_perceptual;
  float hsv_n[3];

  /* Initialize for compatibility. */
  copy_v3_v3(hsv_n, hsv);

  ui_but_v3_get(but, rgb);
  ui_scene_linear_to_perceptual_space(but, rgb);
  rgb_to_hsv_compat_v(rgb, hsv_n);

  ui_draw_gradient(rect, hsv_n, hsv_but->gradient_type, 1.0f);

  ui_hsvcube_pos_from_vals(hsv_but, rect, hsv_n, &x, &y);
  CLAMP(x, rect->xmin + 3.0f, rect->xmax - 3.0f);
  CLAMP(y, rect->ymin + 3.0f, rect->ymax - 3.0f);

  ui_hsv_cursor(x, y);

  /* outline */
  const uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor3ub(0, 0, 0);
  imm_draw_box_wire_2d(pos, (rect->xmin), (rect->ymin), (rect->xmax), (rect->ymax));
  immUnbindProgram();
}

/* vertical 'value' slider, using new widget code */
static void ui_draw_but_HSV_v(uiBut *but, const rcti *rect)
{
  const uiButHSVCube *hsv_but = (uiButHSVCube *)but;
  bTheme *btheme = UI_GetTheme();
  uiWidgetColors *wcol = &btheme->tui.wcol_numslider;
  uiWidgetBase wtb;
  const float rad = wcol->roundness * BLI_rcti_size_x(rect);
  float x, y;
  float rgb[3], hsv[3], v;

  ui_but_v3_get(but, rgb);
  ui_scene_linear_to_perceptual_space(but, rgb);

  if (hsv_but->gradient_type == UI_GRAD_L_ALT) {
    rgb_to_hsl_v(rgb, hsv);
  }
  else {
    rgb_to_hsv_v(rgb, hsv);
  }
  v = hsv[2];

  /* map v from property range to [0,1] */
  if (hsv_but->gradient_type == UI_GRAD_V_ALT) {
    const float min = but->softmin, max = but->softmax;
    v = (v - min) / (max - min);
  }

  widget_init(&wtb);

  /* fully rounded */
  round_box_edges(&wtb, UI_CNR_ALL, rect, rad);

  /* setup temp colors */
  widgetbase_draw(&wtb,
                  &((uiWidgetColors){
                      .outline = {0, 0, 0, 255},
                      .inner = {128, 128, 128, 255},
                      .shadetop = 127,
                      .shadedown = -128,
                      .shaded = 1,
                  }));

  /* We are drawing on top of widget bases. Flush cache. */
  GPU_blend(GPU_BLEND_ALPHA);
  UI_widgetbase_draw_cache_flush();
  GPU_blend(GPU_BLEND_NONE);

  /* cursor */
  x = rect->xmin + 0.5f * BLI_rcti_size_x(rect);
  y = rect->ymin + v * BLI_rcti_size_y(rect);
  CLAMP(y, rect->ymin + 3.0f, rect->ymax - 3.0f);

  ui_hsv_cursor(x, y);
}

/** Separator for menus. */
static void ui_draw_separator(const rcti *rect, const uiWidgetColors *wcol)
{
  const int y = rect->ymin + BLI_rcti_size_y(rect) / 2 - 1;
  const uchar col[4] = {
      wcol->text[0],
      wcol->text[1],
      wcol->text[2],
      30,
  };

  const uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  GPU_blend(GPU_BLEND_ALPHA);
  immUniformColor4ubv(col);
  GPU_line_width(1.0f);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(pos, rect->xmin, y);
  immVertex2f(pos, rect->xmax, y);
  immEnd();

  GPU_blend(GPU_BLEND_NONE);

  immUnbindProgram();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button Draw Callbacks
 * \{ */

static void widget_numbut_draw(
    uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign, bool emboss)
{
  const float rad = wcol->roundness * BLI_rcti_size_y(rect);
  const int handle_width = min_ii(BLI_rcti_size_x(rect) / 3, BLI_rcti_size_y(rect) * 0.7f);

  if (state & UI_SELECT) {
    SWAP(short, wcol->shadetop, wcol->shadedown);
  }

  uiWidgetBase wtb;
  widget_init(&wtb);

  if (!emboss) {
    round_box_edges(&wtb, roundboxalign, rect, rad);
  }
  else {
    wtb.draw_inner = false;
    wtb.draw_outline = false;
  }

  /* decoration */
  if ((state & UI_ACTIVE) && !(state & UI_STATE_TEXT_INPUT)) {
    uiWidgetColors wcol_zone;
    uiWidgetBase wtb_zone;
    rcti rect_zone;
    int roundboxalign_zone;

    /* left arrow zone */
    widget_init(&wtb_zone);
    wtb_zone.draw_outline = false;
    wtb_zone.draw_emboss = false;

    wcol_zone = *wcol;
    copy_v3_v3_uchar(wcol_zone.item, wcol->text);
    if (state & UI_STATE_ACTIVE_LEFT) {
      widget_active_color(&wcol_zone);
    }

    rect_zone = *rect;
    rect_zone.xmax = rect->xmin + handle_width + U.pixelsize;
    roundboxalign_zone = roundboxalign & ~(UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT);
    round_box_edges(&wtb_zone, roundboxalign_zone, &rect_zone, rad);

    shape_preset_init_number_arrows(&wtb_zone.tria1, &rect_zone, 0.6f, 'l');
    widgetbase_draw(&wtb_zone, &wcol_zone);

    /* right arrow zone */
    widget_init(&wtb_zone);
    wtb_zone.draw_outline = false;
    wtb_zone.draw_emboss = false;
    wtb_zone.tria1.type = ROUNDBOX_TRIA_ARROWS;

    wcol_zone = *wcol;
    copy_v3_v3_uchar(wcol_zone.item, wcol->text);
    if (state & UI_STATE_ACTIVE_RIGHT) {
      widget_active_color(&wcol_zone);
    }

    rect_zone = *rect;
    rect_zone.xmin = rect->xmax - handle_width - U.pixelsize;
    roundboxalign_zone = roundboxalign & ~(UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT);
    round_box_edges(&wtb_zone, roundboxalign_zone, &rect_zone, rad);

    shape_preset_init_number_arrows(&wtb_zone.tria2, &rect_zone, 0.6f, 'r');
    widgetbase_draw(&wtb_zone, &wcol_zone);

    /* middle highlight zone */
    widget_init(&wtb_zone);
    wtb_zone.draw_outline = false;
    wtb_zone.draw_emboss = false;

    wcol_zone = *wcol;
    copy_v3_v3_uchar(wcol_zone.item, wcol->text);
    if (!(state & (UI_STATE_ACTIVE_LEFT | UI_STATE_ACTIVE_RIGHT))) {
      widget_active_color(&wcol_zone);
    }

    rect_zone = *rect;
    rect_zone.xmin = rect->xmin + handle_width - U.pixelsize;
    rect_zone.xmax = rect->xmax - handle_width + U.pixelsize;
    round_box_edges(&wtb_zone, 0, &rect_zone, 0);
    widgetbase_draw(&wtb_zone, &wcol_zone);

    /* outline */
    wtb.draw_inner = false;
    widgetbase_draw(&wtb, wcol);
  }
  else {
    /* inner and outline */
    widgetbase_draw(&wtb, wcol);
  }

  if (!(state & UI_STATE_TEXT_INPUT)) {
    const float textofs = 0.425f * BLI_rcti_size_y(rect);

    /* text space */
    rect->xmin += textofs;
    rect->xmax -= textofs;
  }
}

static void widget_numbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
  widget_numbut_draw(wcol, rect, state, roundboxalign, false);
}

static void widget_menubut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
  uiWidgetBase wtb;
  widget_init(&wtb);

  const float rad = wcol->roundness * U.widget_unit;
  round_box_edges(&wtb, roundboxalign, rect, rad);

  /* decoration */
  shape_preset_trias_from_rect_menu(&wtb.tria1, rect);
  /* copy size and center to 2nd tria */
  wtb.tria2 = wtb.tria1;

  widgetbase_draw(&wtb, wcol);

  /* text space, arrows are about 0.6 height of button */
  rect->xmax -= (6 * BLI_rcti_size_y(rect)) / 10;
}

/**
 * Draw menu buttons still with triangles when field is not embossed
 */
static void widget_menubut_embossn(uiBut *UNUSED(but),
                                   uiWidgetColors *wcol,
                                   rcti *rect,
                                   int UNUSED(state),
                                   int UNUSED(roundboxalign))
{
  uiWidgetBase wtb;
  widget_init(&wtb);
  wtb.draw_inner = false;
  wtb.draw_outline = false;

  /* decoration */
  shape_preset_trias_from_rect_menu(&wtb.tria1, rect);
  /* copy size and center to 2nd tria */
  wtb.tria2 = wtb.tria1;

  widgetbase_draw(&wtb, wcol);
}

/**
 * Draw number buttons still with triangles when field is not embossed
 */
static void widget_numbut_embossn(
    uiBut *UNUSED(but), uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
  widget_numbut_draw(wcol, rect, state, roundboxalign, true);
}

/* function in use for buttons and for view2d sliders */
void UI_draw_widget_scroll(uiWidgetColors *wcol, const rcti *rect, const rcti *slider, int state)
{
  uiWidgetBase wtb;
  bool outline = false;

  widget_init(&wtb);

  /* determine horizontal/vertical */
  const bool horizontal = (BLI_rcti_size_x(rect) > BLI_rcti_size_y(rect));

  const float rad = (horizontal) ? wcol->roundness * BLI_rcti_size_y(rect) :
                                   wcol->roundness * BLI_rcti_size_x(rect);

  wtb.uniform_params.shade_dir = (horizontal) ? 1.0f : 0.0;

  /* draw back part, colors swapped and shading inverted */
  if (horizontal) {
    SWAP(short, wcol->shadetop, wcol->shadedown);
  }

  round_box_edges(&wtb, UI_CNR_ALL, rect, rad);
  widgetbase_draw(&wtb, wcol);

  /* slider */
  if ((BLI_rcti_size_x(slider) < 2) || (BLI_rcti_size_y(slider) < 2)) {
    /* pass */
  }
  else {
    SWAP(short, wcol->shadetop, wcol->shadedown);

    copy_v4_v4_uchar(wcol->inner, wcol->item);

    if (wcol->shadetop > wcol->shadedown) {
      wcol->shadetop += 20; /* XXX violates themes... */
    }
    else {
      wcol->shadedown += 20;
    }

    if (state & UI_SCROLL_PRESSED) {
      wcol->inner[0] = wcol->inner[0] >= 250 ? 255 : wcol->inner[0] + 5;
      wcol->inner[1] = wcol->inner[1] >= 250 ? 255 : wcol->inner[1] + 5;
      wcol->inner[2] = wcol->inner[2] >= 250 ? 255 : wcol->inner[2] + 5;
    }

    /* draw */
    wtb.draw_emboss = false; /* only emboss once */

    /* exception for progress bar */
    if (state & UI_SCROLL_NO_OUTLINE) {
      SWAP(bool, outline, wtb.draw_outline);
    }

    round_box_edges(&wtb, UI_CNR_ALL, slider, rad);

    if (state & UI_SCROLL_ARROWS) {
      if (wcol->item[0] > 48) {
        wcol->item[0] -= 48;
      }
      if (wcol->item[1] > 48) {
        wcol->item[1] -= 48;
      }
      if (wcol->item[2] > 48) {
        wcol->item[2] -= 48;
      }
      wcol->item[3] = 255;

      if (horizontal) {
        rcti slider_inset = *slider;
        slider_inset.xmin += 0.05 * U.widget_unit;
        slider_inset.xmax -= 0.05 * U.widget_unit;
        shape_preset_init_scroll_circle(&wtb.tria1, &slider_inset, 0.6f, 'l');
        shape_preset_init_scroll_circle(&wtb.tria2, &slider_inset, 0.6f, 'r');
      }
      else {
        shape_preset_init_scroll_circle(&wtb.tria1, slider, 0.6f, 'b');
        shape_preset_init_scroll_circle(&wtb.tria2, slider, 0.6f, 't');
      }
    }
    widgetbase_draw(&wtb, wcol);

    if (state & UI_SCROLL_NO_OUTLINE) {
      SWAP(bool, outline, wtb.draw_outline);
    }
  }
}

static void widget_scroll(
    uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int UNUSED(roundboxalign))
{
  /* calculate slider part */
  const float value = (float)ui_but_value_get(but);

  const float size = max_ff((but->softmax + but->a1 - but->softmin), 2.0f);

  /* position */
  rcti rect1 = *rect;

  /* determine horizontal/vertical */
  const bool horizontal = (BLI_rcti_size_x(rect) > BLI_rcti_size_y(rect));

  if (horizontal) {
    const float fac = BLI_rcti_size_x(rect) / size;
    rect1.xmin = rect1.xmin + ceilf(fac * (value - but->softmin));
    rect1.xmax = rect1.xmin + ceilf(fac * (but->a1 - but->softmin));

    /* Ensure minimum size. */
    const float min = BLI_rcti_size_y(rect);

    if (BLI_rcti_size_x(&rect1) < min) {
      rect1.xmax = rect1.xmin + min;

      if (rect1.xmax > rect->xmax) {
        rect1.xmax = rect->xmax;
        rect1.xmin = max_ii(rect1.xmax - min, rect->xmin);
      }
    }
  }
  else {
    const float fac = BLI_rcti_size_y(rect) / size;
    rect1.ymax = rect1.ymax - ceilf(fac * (value - but->softmin));
    rect1.ymin = rect1.ymax - ceilf(fac * (but->a1 - but->softmin));

    /* Ensure minimum size. */
    const float min = BLI_rcti_size_x(rect);

    if (BLI_rcti_size_y(&rect1) < min) {
      rect1.ymax = rect1.ymin + min;

      if (rect1.ymax > rect->ymax) {
        rect1.ymax = rect->ymax;
        rect1.ymin = max_ii(rect1.ymax - min, rect->ymin);
      }
    }
  }

  if (state & UI_SELECT) {
    state = UI_SCROLL_PRESSED;
  }
  else {
    state = 0;
  }
  UI_draw_widget_scroll(wcol, rect, &rect1, state);
}

static void widget_progressbar(
    uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
  uiButProgressbar *but_progressbar = (uiButProgressbar *)but;
  rcti rect_prog = *rect, rect_bar = *rect;

  uiWidgetBase wtb, wtb_bar;
  widget_init(&wtb);
  widget_init(&wtb_bar);

  /* round corners */
  const float value = but_progressbar->progress;
  const float ofs = wcol->roundness * BLI_rcti_size_y(&rect_prog);
  float w = value * BLI_rcti_size_x(&rect_prog);

  /* Ensure minimum size. */
  w = MAX2(w, ofs);

  rect_bar.xmax = rect_bar.xmin + w;

  round_box_edges(&wtb, roundboxalign, &rect_prog, ofs);
  round_box_edges(&wtb_bar, roundboxalign, &rect_bar, ofs);

  wtb.draw_outline = true;
  widgetbase_draw(&wtb, wcol);

  /* "slider" bar color */
  copy_v3_v3_uchar(wcol->inner, wcol->item);
  widgetbase_draw(&wtb_bar, wcol);

  /* raise text a bit */
  rect->xmin += (BLI_rcti_size_x(&rect_prog) / 2);
  rect->xmax += (BLI_rcti_size_x(&rect_prog) / 2);
}

static void widget_nodesocket(
    uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
  const int radi = 5;

  uiWidgetBase wtb;
  widget_init(&wtb);

  uchar old_inner[3], old_outline[3];
  copy_v3_v3_uchar(old_inner, wcol->inner);
  copy_v3_v3_uchar(old_outline, wcol->outline);

  wcol->inner[0] = but->col[0];
  wcol->inner[1] = but->col[1];
  wcol->inner[2] = but->col[2];
  wcol->outline[0] = 0;
  wcol->outline[1] = 0;
  wcol->outline[2] = 0;
  wcol->outline[3] = 150;

  const int cent_x = BLI_rcti_cent_x(rect);
  const int cent_y = BLI_rcti_cent_y(rect);
  rect->xmin = cent_x - radi;
  rect->xmax = cent_x + radi;
  rect->ymin = cent_y - radi;
  rect->ymax = cent_y + radi;

  wtb.draw_outline = true;
  round_box_edges(&wtb, UI_CNR_ALL, rect, (float)radi);
  widgetbase_draw(&wtb, wcol);

  copy_v3_v3_uchar(wcol->inner, old_inner);
  copy_v3_v3_uchar(wcol->outline, old_outline);
}

static void widget_numslider(
    uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
  uiWidgetBase wtb, wtb1;
  widget_init(&wtb);
  widget_init(&wtb1);

  /* Backdrop first. */
  const float ofs = wcol->roundness * BLI_rcti_size_y(rect);
  const float toffs = ofs * 0.75f;
  round_box_edges(&wtb, roundboxalign, rect, ofs);

  wtb.draw_outline = false;
  widgetbase_draw(&wtb, wcol);

  /* Draw slider part only when not in text editing. */
  if (!(state & UI_STATE_TEXT_INPUT)) {
    int roundboxalign_slider = roundboxalign;

    uchar outline[3];
    copy_v3_v3_uchar(outline, wcol->outline);
    copy_v3_v3_uchar(wcol->outline, wcol->item);
    copy_v3_v3_uchar(wcol->inner, wcol->item);

    if (!(state & UI_SELECT)) {
      SWAP(short, wcol->shadetop, wcol->shadedown);
    }

    rcti rect1 = *rect;
    float factor, factor_ui;
    float factor_discard = 1.0f; /* No discard. */
    const float value = (float)ui_but_value_get(but);

    if (but->rnaprop && (RNA_property_subtype(but->rnaprop) == PROP_PERCENTAGE)) {
      factor = value / but->softmax;
    }
    else {
      factor = (value - but->softmin) / (but->softmax - but->softmin);
    }

    const float width = (float)BLI_rcti_size_x(rect);
    factor_ui = factor * width;

    if (factor_ui <= ofs) {
      /* Left part only. */
      roundboxalign_slider &= ~(UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT);
      rect1.xmax = rect1.xmin + ofs;
      factor_discard = factor_ui / ofs;
    }
    else if (factor_ui <= width - ofs) {
      /* Left part + middle part. */
      roundboxalign_slider &= ~(UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT);
      rect1.xmax = rect1.xmin + factor_ui;
    }
    else {
      /* Left part + middle part + right part. */
      factor_discard = factor;
    }

    round_box_edges(&wtb1, roundboxalign_slider, &rect1, ofs);
    wtb1.draw_outline = false;
    widgetbase_set_uniform_discard_factor(&wtb1, factor_discard);
    widgetbase_draw(&wtb1, wcol);

    copy_v3_v3_uchar(wcol->outline, outline);

    if (!(state & UI_SELECT)) {
      SWAP(short, wcol->shadetop, wcol->shadedown);
    }
  }

  /* Outline. */
  wtb.draw_outline = true;
  wtb.draw_inner = false;
  widgetbase_draw(&wtb, wcol);

  /* Add space at either side of the button so text aligns with number-buttons
   * (which have arrow icons). */
  if (!(state & UI_STATE_TEXT_INPUT)) {
    rect->xmax -= toffs;
    rect->xmin += toffs;
  }
}

/* I think 3 is sufficient border to indicate keyed status */
#define SWATCH_KEYED_BORDER 3

static void widget_swatch(
    uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
  BLI_assert(but->type == UI_BTYPE_COLOR);
  uiButColor *color_but = (uiButColor *)but;
  float col[4];

  col[3] = 1.0f;

  if (but->rnaprop) {
    BLI_assert(but->rnaindex == -1);

    if (RNA_property_array_length(&but->rnapoin, but->rnaprop) == 4) {
      col[3] = RNA_property_float_get_index(&but->rnapoin, but->rnaprop, 3);
    }
  }

  uiWidgetBase wtb;
  widget_init(&wtb);

  const float rad = wcol->roundness * U.widget_unit;
  round_box_edges(&wtb, roundboxalign, rect, rad);

  ui_but_v3_get(but, col);

  if ((state & (UI_BUT_ANIMATED | UI_BUT_ANIMATED_KEY | UI_BUT_DRIVEN | UI_BUT_OVERRIDDEN |
                UI_BUT_REDALERT)) ||
      (but->drawflag & UI_BUT_ANIMATED_CHANGED)) {
    /* draw based on state - color for keyed etc */
    widgetbase_draw(&wtb, wcol);

    /* inset to draw swatch color */
    rect->xmin += SWATCH_KEYED_BORDER;
    rect->xmax -= SWATCH_KEYED_BORDER;
    rect->ymin += SWATCH_KEYED_BORDER;
    rect->ymax -= SWATCH_KEYED_BORDER;

    round_box_edges(&wtb, roundboxalign, rect, rad);
  }

  if (!ui_but_is_color_gamma(but)) {
    ui_block_cm_to_display_space_v3(but->block, col);
  }

  rgba_float_to_uchar(wcol->inner, col);
  const bool show_alpha_checkers = (wcol->inner[3] < 255);

  wcol->shaded = 0;

  /* Now we reduce alpha of the inner color (i.e. the color shown)
   * so that this setting can look grayed out, while retaining
   * the checkerboard (for transparent values). This is needed
   * here as the effects of ui_widget_color_disabled() are overwritten. */
  wcol->inner[3] *= widget_alpha_factor(state);

  widgetbase_draw_ex(&wtb, wcol, show_alpha_checkers);
  if (color_but->is_pallete_color &&
      ((Palette *)but->rnapoin.owner_id)->active_color == color_but->palette_color_index) {
    const float width = rect->xmax - rect->xmin;
    const float height = rect->ymax - rect->ymin;
    /* find color luminance and change it slightly */
    float bw = rgb_to_grayscale(col);

    bw += (bw < 0.5f) ? 0.5f : -0.5f;

    /* We are drawing on top of widget bases. Flush cache. */
    GPU_blend(GPU_BLEND_ALPHA);
    UI_widgetbase_draw_cache_flush();
    GPU_blend(GPU_BLEND_NONE);

    const uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    immUniformColor3f(bw, bw, bw);
    immBegin(GPU_PRIM_TRIS, 3);
    immVertex2f(pos, rect->xmin + 0.1f * width, rect->ymin + 0.9f * height);
    immVertex2f(pos, rect->xmin + 0.1f * width, rect->ymin + 0.5f * height);
    immVertex2f(pos, rect->xmin + 0.5f * width, rect->ymin + 0.9f * height);
    immEnd();

    immUnbindProgram();
  }
}

static void widget_unitvec(
    uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
  ui_draw_but_UNITVEC(but, wcol, rect);
}

static void widget_icon_has_anim(
    uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
  if (state & (UI_BUT_ANIMATED | UI_BUT_ANIMATED_KEY | UI_BUT_DRIVEN | UI_BUT_REDALERT) &&
      but->emboss != UI_EMBOSS_NONE) {
    uiWidgetBase wtb;
    widget_init(&wtb);
    wtb.draw_outline = false;

    const float rad = wcol->roundness * BLI_rcti_size_y(rect);
    round_box_edges(&wtb, UI_CNR_ALL, rect, rad);
    widgetbase_draw(&wtb, wcol);
  }
  else if (but->type == UI_BTYPE_NUM) {
    /* Draw number buttons still with left/right
     * triangles when field is not embossed */
    widget_numbut_embossn(but, wcol, rect, state, roundboxalign);
  }
  else if (but->type == UI_BTYPE_MENU) {
    /* Draw menu buttons still with down arrow. */
    widget_menubut_embossn(but, wcol, rect, state, roundboxalign);
  }
}

static void widget_textbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
  if (state & UI_SELECT) {
    SWAP(short, wcol->shadetop, wcol->shadedown);
  }

  uiWidgetBase wtb;
  widget_init(&wtb);

  const float rad = wcol->roundness * U.widget_unit;
  round_box_edges(&wtb, roundboxalign, rect, rad);

  widgetbase_draw(&wtb, wcol);
}

static void widget_menuiconbut(uiWidgetColors *wcol,
                               rcti *rect,
                               int UNUSED(state),
                               int roundboxalign)
{
  uiWidgetBase wtb;
  widget_init(&wtb);

  const float rad = wcol->roundness * U.widget_unit;
  round_box_edges(&wtb, roundboxalign, rect, rad);

  /* decoration */
  widgetbase_draw(&wtb, wcol);
}

static void widget_pulldownbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
  float back[4];
  UI_GetThemeColor4fv(TH_BACK, back);

  if ((state & UI_ACTIVE) || (back[3] < 1.0f)) {
    uiWidgetBase wtb;
    const float rad = wcol->roundness * U.widget_unit;

    if (state & UI_ACTIVE) {
      copy_v4_v4_uchar(wcol->inner, wcol->inner_sel);
      copy_v3_v3_uchar(wcol->text, wcol->text_sel);
      copy_v3_v3_uchar(wcol->outline, wcol->inner);
    }
    else {
      wcol->inner[3] *= 1.0f - back[3];
      wcol->outline[3] = 0.0f;
    }

    widget_init(&wtb);

    /* half rounded */
    round_box_edges(&wtb, roundboxalign, rect, rad);

    widgetbase_draw(&wtb, wcol);
  }
}

static void widget_menu_itembut(uiWidgetColors *wcol,
                                rcti *rect,
                                int UNUSED(state),
                                int UNUSED(roundboxalign))
{
  uiWidgetBase wtb;
  widget_init(&wtb);

  /* not rounded, no outline */
  wtb.draw_outline = false;
  round_box_edges(&wtb, 0, rect, 0.0f);

  widgetbase_draw(&wtb, wcol);
}

static void widget_menu_radial_itembut(
    uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
  const float fac = but->block->pie_data.alphafac;

  uiWidgetBase wtb;
  widget_init(&wtb);

  wtb.draw_emboss = false;

  const float rad = wcol->roundness * BLI_rcti_size_y(rect);
  round_box_edges(&wtb, UI_CNR_ALL, rect, rad);

  wcol->inner[3] *= fac;
  wcol->inner_sel[3] *= fac;
  wcol->item[3] *= fac;
  wcol->text[3] *= fac;
  wcol->text_sel[3] *= fac;
  wcol->outline[3] *= fac;

  widgetbase_draw(&wtb, wcol);
}

static void widget_list_itembut(uiWidgetColors *wcol,
                                rcti *rect,
                                int UNUSED(state),
                                int UNUSED(roundboxalign))
{
  uiWidgetBase wtb;
  widget_init(&wtb);

  /* no outline */
  wtb.draw_outline = false;
  const float rad = wcol->roundness * U.widget_unit;
  round_box_edges(&wtb, UI_CNR_ALL, rect, rad);

  widgetbase_draw(&wtb, wcol);
}

static void widget_optionbut(uiWidgetColors *wcol,
                             rcti *rect,
                             int state,
                             int UNUSED(roundboxalign))
{
  const bool text_before_widget = (state & UI_STATE_TEXT_BEFORE_WIDGET);
  rcti recttemp = *rect;

  uiWidgetBase wtb;
  widget_init(&wtb);

  /* square */
  if (text_before_widget) {
    recttemp.xmin = recttemp.xmax - BLI_rcti_size_y(&recttemp);
  }
  else {
    recttemp.xmax = recttemp.xmin + BLI_rcti_size_y(&recttemp);
  }

  /* smaller */
  const int delta = (BLI_rcti_size_y(&recttemp) - 2 * U.pixelsize) / 6;
  BLI_rcti_resize(
      &recttemp, BLI_rcti_size_x(&recttemp) - delta * 2, BLI_rcti_size_y(&recttemp) - delta * 2);
  /* Keep one edge in place. */
  BLI_rcti_translate(&recttemp, text_before_widget ? delta : -delta, 0);

  const float rad = wcol->roundness * BLI_rcti_size_y(&recttemp);
  round_box_edges(&wtb, UI_CNR_ALL, &recttemp, rad);

  /* decoration */
  if (state & UI_SELECT) {
    shape_preset_trias_from_rect_checkmark(&wtb.tria1, &recttemp);
  }

  widgetbase_draw(&wtb, wcol);

  /* Text space - factor is really just eyeballed. */
  const float offset = delta * 0.9;
  if (text_before_widget) {
    rect->xmax = recttemp.xmin - offset;
  }
  else {
    rect->xmin = recttemp.xmax + offset;
  }
}

/* labels use Editor theme colors for text */
static void widget_state_label(uiWidgetType *wt, int state, int drawflag, eUIEmbossType emboss)
{
  if (state & UI_BUT_LIST_ITEM) {
    /* Override default label theme's colors. */
    bTheme *btheme = UI_GetTheme();
    wt->wcol_theme = &btheme->tui.wcol_list_item;
    /* call this for option button */
    widget_state(wt, state, drawflag, emboss);
  }
  else {
    /* call this for option button */
    widget_state(wt, state, drawflag, emboss);
    if (state & UI_SELECT) {
      UI_GetThemeColor3ubv(TH_TEXT_HI, wt->wcol.text);
    }
    else {
      UI_GetThemeColor3ubv(TH_TEXT, wt->wcol.text);
    }
  }

  if (state & UI_BUT_REDALERT) {
    const uchar red[4] = {255, 0, 0};
    color_blend_v3_v3(wt->wcol.text, red, 0.4f);
  }
}

static void widget_radiobut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
  uiWidgetBase wtb;
  widget_init(&wtb);

  const float rad = wcol->roundness * U.widget_unit;
  round_box_edges(&wtb, roundboxalign, rect, rad);

  widgetbase_draw(&wtb, wcol);
}

static void widget_box(
    uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
  uiWidgetBase wtb;
  widget_init(&wtb);

  uchar old_col[3];
  copy_v3_v3_uchar(old_col, wcol->inner);

  /* abuse but->hsv - if it's non-zero, use this color as the box's background */
  if (but != NULL && but->col[3]) {
    wcol->inner[0] = but->col[0];
    wcol->inner[1] = but->col[1];
    wcol->inner[2] = but->col[2];
    wcol->inner[3] = but->col[3];
  }

  const float rad = wcol->roundness * U.widget_unit;
  round_box_edges(&wtb, roundboxalign, rect, rad);

  widgetbase_draw(&wtb, wcol);

  copy_v3_v3_uchar(wcol->inner, old_col);
}

static void widget_but(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
  uiWidgetBase wtb;
  widget_init(&wtb);

  const float rad = wcol->roundness * U.widget_unit;
  round_box_edges(&wtb, roundboxalign, rect, rad);

  widgetbase_draw(&wtb, wcol);
}

#if 0
static void widget_roundbut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
  uiWidgetBase wtb;
  const float rad = wcol->roundness * U.widget_unit;

  widget_init(&wtb);

  /* half rounded */
  round_box_edges(&wtb, roundboxalign, rect, rad);

  widgetbase_draw(&wtb, wcol);
}
#endif

static void widget_roundbut_exec(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
  const float rad = wcol->roundness * U.widget_unit;

  uiWidgetBase wtb;
  widget_init(&wtb);

  if (state & UI_STATE_HOLD_ACTION) {
    /* Show that keeping pressed performs another action (typically a menu). */
    shape_preset_init_hold_action(&wtb.tria1, rect, 0.75f, 'r');
  }

  /* half rounded */
  round_box_edges(&wtb, roundboxalign, rect, rad);

  widgetbase_draw(&wtb, wcol);
}

static void widget_tab(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
  const float rad = wcol->roundness * U.widget_unit;
  const bool is_active = (state & UI_SELECT);

  /* Draw shaded outline - Disabled for now,
   * seems incorrect and also looks nicer without it imho ;) */
  // #define USE_TAB_SHADED_HIGHLIGHT

  uchar theme_col_tab_highlight[3];

#ifdef USE_TAB_SHADED_HIGHLIGHT
  /* create outline highlight colors */
  if (is_active) {
    interp_v3_v3v3_uchar(theme_col_tab_highlight, wcol->inner_sel, wcol->outline, 0.2f);
  }
  else {
    interp_v3_v3v3_uchar(theme_col_tab_highlight, wcol->inner, wcol->outline, 0.12f);
  }
#endif

  uiWidgetBase wtb;
  widget_init(&wtb);

  /* half rounded */
  round_box_edges(&wtb, roundboxalign, rect, rad);

  /* draw inner */
#ifdef USE_TAB_SHADED_HIGHLIGHT
  wtb.draw_outline = 0;
#endif
  widgetbase_draw(&wtb, wcol);

  /* We are drawing on top of widget bases. Flush cache. */
  GPU_blend(GPU_BLEND_ALPHA);
  UI_widgetbase_draw_cache_flush();
  GPU_blend(GPU_BLEND_NONE);

#ifdef USE_TAB_SHADED_HIGHLIGHT
  /* draw outline (3d look) */
  ui_draw_but_TAB_outline(rect, rad, theme_col_tab_highlight, wcol->inner);
#endif

#ifndef USE_TAB_SHADED_HIGHLIGHT
  UNUSED_VARS(is_active, theme_col_tab_highlight);
#endif
}

static void widget_draw_extra_mask(const bContext *C, uiBut *but, uiWidgetType *wt, rcti *rect)
{
  bTheme *btheme = UI_GetTheme();
  uiWidgetColors *wcol = &btheme->tui.wcol_radio;
  const float rad = wcol->roundness * U.widget_unit;

  /* state copy! */
  wt->wcol = *(wt->wcol_theme);

  uiWidgetBase wtb;
  widget_init(&wtb);

  if (but->block->drawextra) {
    /* note: drawextra can change rect +1 or -1, to match round errors of existing previews */
    but->block->drawextra(
        C, but->poin, but->block->drawextra_arg1, but->block->drawextra_arg2, rect);

    const uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    /* make mask to draw over image */
    uchar col[4];
    UI_GetThemeColor3ubv(TH_BACK, col);
    immUniformColor3ubv(col);

    round_box__edges(&wtb, UI_CNR_ALL, rect, 0.0f, rad);
    widgetbase_outline(&wtb, pos);

    immUnbindProgram();
  }

  /* outline */
  round_box_edges(&wtb, UI_CNR_ALL, rect, rad);
  wtb.draw_outline = true;
  wtb.draw_inner = false;
  widgetbase_draw(&wtb, &wt->wcol);
}

static uiWidgetType *widget_type(uiWidgetTypeEnum type)
{
  bTheme *btheme = UI_GetTheme();

  /* defaults */
  static uiWidgetType wt;
  wt.wcol_theme = &btheme->tui.wcol_regular;
  wt.wcol_state = &btheme->tui.wcol_state;
  wt.state = widget_state;
  wt.draw = widget_but;
  wt.custom = NULL;
  wt.text = widget_draw_text_icon;

  switch (type) {
    case UI_WTYPE_REGULAR:
      break;

    case UI_WTYPE_LABEL:
      wt.draw = NULL;
      wt.state = widget_state_label;
      break;

    case UI_WTYPE_TOGGLE:
      wt.wcol_theme = &btheme->tui.wcol_toggle;
      break;

    case UI_WTYPE_CHECKBOX:
      wt.wcol_theme = &btheme->tui.wcol_option;
      wt.draw = widget_optionbut;
      break;

    case UI_WTYPE_RADIO:
      wt.wcol_theme = &btheme->tui.wcol_radio;
      wt.draw = widget_radiobut;
      break;

    case UI_WTYPE_NUMBER:
      wt.wcol_theme = &btheme->tui.wcol_num;
      wt.draw = widget_numbut;
      break;

    case UI_WTYPE_SLIDER:
      wt.wcol_theme = &btheme->tui.wcol_numslider;
      wt.custom = widget_numslider;
      wt.state = widget_state_numslider;
      break;

    case UI_WTYPE_EXEC:
      wt.wcol_theme = &btheme->tui.wcol_tool;
      wt.draw = widget_roundbut_exec;
      break;

    case UI_WTYPE_TOOLBAR_ITEM:
      wt.wcol_theme = &btheme->tui.wcol_toolbar_item;
      wt.draw = widget_roundbut_exec;
      break;

    case UI_WTYPE_TAB:
      wt.wcol_theme = &btheme->tui.wcol_tab;
      wt.draw = widget_tab;
      break;

    case UI_WTYPE_TOOLTIP:
      wt.wcol_theme = &btheme->tui.wcol_tooltip;
      wt.draw = widget_menu_back;
      break;

    /* strings */
    case UI_WTYPE_NAME:
      wt.wcol_theme = &btheme->tui.wcol_text;
      wt.draw = widget_textbut;
      break;

    case UI_WTYPE_NAME_LINK:
      break;

    case UI_WTYPE_POINTER_LINK:
      break;

    case UI_WTYPE_FILENAME:
      break;

    /* start menus */
    case UI_WTYPE_MENU_RADIO:
      wt.wcol_theme = &btheme->tui.wcol_menu;
      wt.draw = widget_menubut;
      break;

    case UI_WTYPE_MENU_ICON_RADIO:
    case UI_WTYPE_MENU_NODE_LINK:
      wt.wcol_theme = &btheme->tui.wcol_menu;
      wt.draw = widget_menuiconbut;
      break;

    case UI_WTYPE_MENU_POINTER_LINK:
      wt.wcol_theme = &btheme->tui.wcol_menu;
      wt.draw = widget_menubut;
      break;

    case UI_WTYPE_PULLDOWN:
      wt.wcol_theme = &btheme->tui.wcol_pulldown;
      wt.draw = widget_pulldownbut;
      wt.state = widget_state_pulldown;
      break;

    /* in menus */
    case UI_WTYPE_MENU_ITEM:
      wt.wcol_theme = &btheme->tui.wcol_menu_item;
      wt.draw = widget_menu_itembut;
      wt.state = widget_state_menu_item;
      break;

    case UI_WTYPE_MENU_BACK:
      wt.wcol_theme = &btheme->tui.wcol_menu_back;
      wt.draw = widget_menu_back;
      break;

    /* specials */
    case UI_WTYPE_ICON:
      wt.custom = widget_icon_has_anim;
      break;

    case UI_WTYPE_ICON_LABEL:
      /* behave like regular labels (this is simply a label with an icon) */
      wt.state = widget_state_label;
      wt.custom = widget_icon_has_anim;
      break;

    case UI_WTYPE_SWATCH:
      wt.custom = widget_swatch;
      break;

    case UI_WTYPE_BOX:
      wt.custom = widget_box;
      wt.wcol_theme = &btheme->tui.wcol_box;
      break;

    case UI_WTYPE_RGB_PICKER:
      break;

    case UI_WTYPE_UNITVEC:
      wt.custom = widget_unitvec;
      break;

    case UI_WTYPE_SCROLL:
      wt.wcol_theme = &btheme->tui.wcol_scroll;
      wt.state = widget_state_nothing;
      wt.custom = widget_scroll;
      break;

    case UI_WTYPE_LISTITEM:
      wt.wcol_theme = &btheme->tui.wcol_list_item;
      wt.draw = widget_list_itembut;
      break;

    case UI_WTYPE_PROGRESSBAR:
      wt.wcol_theme = &btheme->tui.wcol_progress;
      wt.custom = widget_progressbar;
      break;

    case UI_WTYPE_NODESOCKET:
      wt.custom = widget_nodesocket;
      break;

    case UI_WTYPE_MENU_ITEM_RADIAL:
      wt.wcol_theme = &btheme->tui.wcol_pie_menu;
      wt.custom = widget_menu_radial_itembut;
      wt.state = widget_state_pie_menu_item;
      break;
  }

  return &wt;
}

static int widget_roundbox_set(uiBut *but, rcti *rect)
{
  int roundbox = UI_CNR_ALL;

  /* alignment */
  if ((but->drawflag & UI_BUT_ALIGN) && but->type != UI_BTYPE_PULLDOWN) {

    /* ui_popup_block_position has this correction too, keep in sync */
    if (but->drawflag & (UI_BUT_ALIGN_TOP | UI_BUT_ALIGN_STITCH_TOP)) {
      rect->ymax += U.pixelsize;
    }
    if (but->drawflag & (UI_BUT_ALIGN_LEFT | UI_BUT_ALIGN_STITCH_LEFT)) {
      rect->xmin -= U.pixelsize;
    }

    switch (but->drawflag & UI_BUT_ALIGN) {
      case UI_BUT_ALIGN_TOP:
        roundbox = UI_CNR_BOTTOM_LEFT | UI_CNR_BOTTOM_RIGHT;
        break;
      case UI_BUT_ALIGN_DOWN:
        roundbox = UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT;
        break;
      case UI_BUT_ALIGN_LEFT:
        roundbox = UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT;
        break;
      case UI_BUT_ALIGN_RIGHT:
        roundbox = UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT;
        break;
      case UI_BUT_ALIGN_DOWN | UI_BUT_ALIGN_RIGHT:
        roundbox = UI_CNR_TOP_LEFT;
        break;
      case UI_BUT_ALIGN_DOWN | UI_BUT_ALIGN_LEFT:
        roundbox = UI_CNR_TOP_RIGHT;
        break;
      case UI_BUT_ALIGN_TOP | UI_BUT_ALIGN_RIGHT:
        roundbox = UI_CNR_BOTTOM_LEFT;
        break;
      case UI_BUT_ALIGN_TOP | UI_BUT_ALIGN_LEFT:
        roundbox = UI_CNR_BOTTOM_RIGHT;
        break;
      default:
        roundbox = 0;
        break;
    }
  }

  /* align with open menu */
  if (but->active && (but->type != UI_BTYPE_POPOVER) && !ui_but_menu_draw_as_popover(but)) {
    const int direction = ui_but_menu_direction(but);

    if (direction == UI_DIR_UP) {
      roundbox &= ~(UI_CNR_TOP_RIGHT | UI_CNR_TOP_LEFT);
    }
    else if (direction == UI_DIR_DOWN) {
      roundbox &= ~(UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT);
    }
    else if (direction == UI_DIR_LEFT) {
      roundbox &= ~(UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT);
    }
    else if (direction == UI_DIR_RIGHT) {
      roundbox &= ~(UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT);
    }
  }

  return roundbox;
}

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

/* conversion from old to new buttons, so still messy */
void ui_draw_but(const bContext *C, struct ARegion *region, uiStyle *style, uiBut *but, rcti *rect)
{
  bTheme *btheme = UI_GetTheme();
  const ThemeUI *tui = &btheme->tui;
  const uiFontStyle *fstyle = &style->widget;
  uiWidgetType *wt = NULL;

  /* handle menus separately */
  if (but->emboss == UI_EMBOSS_PULLDOWN) {
    switch (but->type) {
      case UI_BTYPE_LABEL:
        widget_draw_text_icon(&style->widgetlabel, &tui->wcol_menu_back, but, rect);
        break;
      case UI_BTYPE_SEPR_LINE:
        ui_draw_separator(rect, &tui->wcol_menu_item);
        break;
      default:
        wt = widget_type(UI_WTYPE_MENU_ITEM);
        break;
    }
  }
  else if (ELEM(but->emboss, UI_EMBOSS_NONE, UI_EMBOSS_NONE_OR_STATUS)) {
    /* Use the same widget types for both no emboss types. Later on,
     * #UI_EMBOSS_NONE_OR_STATUS will blend state colors if they apply. */
    switch (but->type) {
      case UI_BTYPE_LABEL:
        wt = widget_type(UI_WTYPE_ICON_LABEL);
        break;
      default:
        wt = widget_type(UI_WTYPE_ICON);
        break;
    }
  }
  else if (but->emboss == UI_EMBOSS_RADIAL) {
    wt = widget_type(UI_WTYPE_MENU_ITEM_RADIAL);
  }
  else {
    BLI_assert(but->emboss == UI_EMBOSS);

    switch (but->type) {
      case UI_BTYPE_LABEL:
        wt = widget_type(UI_WTYPE_LABEL);
        fstyle = &style->widgetlabel;
        if (but->drawflag & UI_BUT_BOX_ITEM) {
          wt->wcol_theme = &tui->wcol_box;
          wt->state = widget_state;
        }
        else if (but->block->theme_style == UI_BLOCK_THEME_STYLE_POPUP) {
          wt->wcol_theme = &tui->wcol_menu_back;
          wt->state = widget_state;
        }
        if (!(but->flag & UI_HAS_ICON)) {
          but->drawflag |= UI_BUT_NO_TEXT_PADDING;
        }
        break;

      case UI_BTYPE_SEPR:
      case UI_BTYPE_SEPR_LINE:
      case UI_BTYPE_SEPR_SPACER:
        break;

      case UI_BTYPE_BUT:
      case UI_BTYPE_DECORATOR:
#ifdef USE_UI_TOOLBAR_HACK
        if ((but->icon != ICON_NONE) && UI_but_is_tool(but)) {
          wt = widget_type(UI_WTYPE_TOOLBAR_ITEM);
        }
        else {
          wt = widget_type(UI_WTYPE_EXEC);
        }
#else
        wt = widget_type(UI_WTYPE_EXEC);
#endif
        break;

      case UI_BTYPE_NUM:
        wt = widget_type(UI_WTYPE_NUMBER);
        break;

      case UI_BTYPE_NUM_SLIDER:
        wt = widget_type(UI_WTYPE_SLIDER);
        break;

      case UI_BTYPE_ROW:
        wt = widget_type(UI_WTYPE_RADIO);
        break;

      case UI_BTYPE_LISTROW:
        wt = widget_type(UI_WTYPE_LISTITEM);
        break;

      case UI_BTYPE_TEXT:
        wt = widget_type(UI_WTYPE_NAME);
        break;

      case UI_BTYPE_SEARCH_MENU:
        wt = widget_type(UI_WTYPE_NAME);
        break;

      case UI_BTYPE_TAB:
        wt = widget_type(UI_WTYPE_TAB);
        break;

      case UI_BTYPE_BUT_TOGGLE:
      case UI_BTYPE_TOGGLE:
      case UI_BTYPE_TOGGLE_N:
        wt = widget_type(UI_WTYPE_TOGGLE);
        break;

      case UI_BTYPE_CHECKBOX:
      case UI_BTYPE_CHECKBOX_N:
        if (!(but->flag & UI_HAS_ICON)) {
          wt = widget_type(UI_WTYPE_CHECKBOX);

          if ((but->drawflag & (UI_BUT_TEXT_LEFT | UI_BUT_TEXT_RIGHT)) == 0) {
            but->drawflag |= UI_BUT_TEXT_LEFT;
          }
          /* #widget_optionbut() carefully sets the text rectangle for fine tuned paddings. If the
           * text drawing were to add its own padding, DPI and zoom factor would be applied twice
           * in the final padding, so it's difficult to control it. */
          but->drawflag |= UI_BUT_NO_TEXT_PADDING;
        }
        else {
          wt = widget_type(UI_WTYPE_TOGGLE);
        }

        /* option buttons have strings outside, on menus use different colors */
        if (but->block->theme_style == UI_BLOCK_THEME_STYLE_POPUP) {
          wt->state = widget_state_option_menu;
        }
        break;

      case UI_BTYPE_MENU:
      case UI_BTYPE_BLOCK:
      case UI_BTYPE_POPOVER:
        if (but->flag & UI_BUT_NODE_LINK) {
          /* new node-link button, not active yet XXX */
          wt = widget_type(UI_WTYPE_MENU_NODE_LINK);
        }
        else {
          /* with menu arrows */

          /* We could use a flag for this, but for now just check size,
           * add up/down arrows if there is room. */
          if ((!but->str[0] && but->icon && (BLI_rcti_size_x(rect) < BLI_rcti_size_y(rect) + 2)) ||
              /* disable for brushes also */
              (but->flag & UI_BUT_ICON_PREVIEW)) {
            /* no arrows */
            wt = widget_type(UI_WTYPE_MENU_ICON_RADIO);
          }
          else {
            wt = widget_type(UI_WTYPE_MENU_RADIO);
          }
        }
        break;

      case UI_BTYPE_PULLDOWN:
        wt = widget_type(UI_WTYPE_PULLDOWN);
        break;

      case UI_BTYPE_BUT_MENU:
        wt = widget_type(UI_WTYPE_MENU_ITEM);
        break;

      case UI_BTYPE_COLOR:
        wt = widget_type(UI_WTYPE_SWATCH);
        break;

      case UI_BTYPE_ROUNDBOX:
      case UI_BTYPE_LISTBOX:
        wt = widget_type(UI_WTYPE_BOX);
        break;

      case UI_BTYPE_EXTRA:
        widget_draw_extra_mask(C, but, widget_type(UI_WTYPE_BOX), rect);
        break;

      case UI_BTYPE_HSVCUBE: {
        const uiButHSVCube *hsv_but = (uiButHSVCube *)but;

        if (ELEM(hsv_but->gradient_type, UI_GRAD_V_ALT, UI_GRAD_L_ALT)) {
          /* vertical V slider, uses new widget draw now */
          ui_draw_but_HSV_v(but, rect);
        }
        else { /* other HSV pickers... */
          ui_draw_but_HSVCUBE(but, rect);
        }
        break;
      }

      case UI_BTYPE_HSVCIRCLE:
        ui_draw_but_HSVCIRCLE(but, &tui->wcol_regular, rect);
        break;

      case UI_BTYPE_COLORBAND:
        /* do not draw right to edge of rect */
        rect->xmin += (0.25f * UI_UNIT_X);
        rect->xmax -= (0.3f * UI_UNIT_X);
        ui_draw_but_COLORBAND(but, &tui->wcol_regular, rect);
        break;

      case UI_BTYPE_UNITVEC:
        wt = widget_type(UI_WTYPE_UNITVEC);
        break;

      case UI_BTYPE_IMAGE:
        ui_draw_but_IMAGE(region, but, &tui->wcol_regular, rect);
        break;

      case UI_BTYPE_HISTOGRAM:
        ui_draw_but_HISTOGRAM(region, but, &tui->wcol_regular, rect);
        break;

      case UI_BTYPE_WAVEFORM:
        ui_draw_but_WAVEFORM(region, but, &tui->wcol_regular, rect);
        break;

      case UI_BTYPE_VECTORSCOPE:
        ui_draw_but_VECTORSCOPE(region, but, &tui->wcol_regular, rect);
        break;

      case UI_BTYPE_CURVE:
        /* do not draw right to edge of rect */
        rect->xmin += (0.2f * UI_UNIT_X);
        rect->xmax -= (0.2f * UI_UNIT_X);
        ui_draw_but_CURVE(region, but, &tui->wcol_regular, rect);
        break;

      case UI_BTYPE_CURVEPROFILE:
        ui_draw_but_CURVEPROFILE(region, but, &tui->wcol_regular, rect);
        break;

      case UI_BTYPE_PROGRESS_BAR:
        wt = widget_type(UI_WTYPE_PROGRESSBAR);
        fstyle = &style->widgetlabel;
        break;

      case UI_BTYPE_SCROLL:
        wt = widget_type(UI_WTYPE_SCROLL);
        break;

      case UI_BTYPE_GRIP:
        wt = widget_type(UI_WTYPE_ICON);
        break;

      case UI_BTYPE_TRACK_PREVIEW:
        ui_draw_but_TRACKPREVIEW(region, but, &tui->wcol_regular, rect);
        break;

      case UI_BTYPE_NODE_SOCKET:
        wt = widget_type(UI_WTYPE_NODESOCKET);
        break;

      default:
        wt = widget_type(UI_WTYPE_REGULAR);
        break;
    }
  }

  if (wt == NULL) {
    return;
  }

  // rcti disablerect = *rect; /* rect gets clipped smaller for text */

  const int roundboxalign = widget_roundbox_set(but, rect);

  /* Mask out flags re-used for local state. */
  int state = but->flag & ~UI_STATE_FLAGS_ALL;
  const int drawflag = but->drawflag;

  if (state & UI_SELECT_DRAW) {
    state |= UI_SELECT;
  }

  if ((but->editstr) ||
      (UNLIKELY(but->flag & UI_BUT_DRAG_MULTI) && ui_but_drag_multi_edit_get(but))) {
    state |= UI_STATE_TEXT_INPUT;
  }

  if (but->hold_func) {
    state |= UI_STATE_HOLD_ACTION;
  }

  if (state & UI_ACTIVE) {
    if (but->drawflag & UI_BUT_ACTIVE_LEFT) {
      state |= UI_STATE_ACTIVE_LEFT;
    }
    else if (but->drawflag & UI_BUT_ACTIVE_RIGHT) {
      state |= UI_STATE_ACTIVE_RIGHT;
    }
  }

  bool use_alpha_blend = false;
  if (but->emboss != UI_EMBOSS_PULLDOWN) {
    if (state & (UI_BUT_DISABLED | UI_BUT_INACTIVE | UI_SEARCH_FILTER_NO_MATCH)) {
      use_alpha_blend = true;
      ui_widget_color_disabled(wt, state);
    }
  }

  if (drawflag & UI_BUT_TEXT_RIGHT) {
    state |= UI_STATE_TEXT_BEFORE_WIDGET;
  }

#ifdef USE_UI_POPOVER_ONCE
  if (but->block->flag & UI_BLOCK_POPOVER_ONCE) {
    if ((state & UI_ACTIVE) && ui_but_is_popover_once_compat(but)) {
      state |= UI_BUT_ACTIVE_DEFAULT;
    }
  }
#endif

  wt->state(wt, state, drawflag, but->emboss);
  if (wt->custom) {
    wt->custom(but, &wt->wcol, rect, state, roundboxalign);
  }
  else if (wt->draw) {
    wt->draw(&wt->wcol, rect, state, roundboxalign);
  }

  if (use_alpha_blend) {
    GPU_blend(GPU_BLEND_ALPHA);
  }

  wt->text(fstyle, &wt->wcol, but, rect);
  if (use_alpha_blend) {
    GPU_blend(GPU_BLEND_NONE);
  }
}

static void ui_draw_clip_tri(uiBlock *block, rcti *rect, uiWidgetType *wt)
{
  if (block) {
    float draw_color[4];
    const uchar *color = wt->wcol.text;

    draw_color[0] = ((float)color[0]) / 255.0f;
    draw_color[1] = ((float)color[1]) / 255.0f;
    draw_color[2] = ((float)color[2]) / 255.0f;
    draw_color[3] = 1.0f;

    if (block->flag & UI_BLOCK_CLIPTOP) {
      /* XXX no scaling for UI here yet */
      UI_draw_icon_tri(BLI_rcti_cent_x(rect), rect->ymax - 6 * U.dpi_fac, 't', draw_color);
    }
    if (block->flag & UI_BLOCK_CLIPBOTTOM) {
      /* XXX no scaling for UI here yet */
      UI_draw_icon_tri(BLI_rcti_cent_x(rect), rect->ymin + 10 * U.dpi_fac, 'v', draw_color);
    }
  }
}

void ui_draw_menu_back(uiStyle *UNUSED(style), uiBlock *block, rcti *rect)
{
  uiWidgetType *wt = widget_type(UI_WTYPE_MENU_BACK);

  wt->state(wt, 0, 0, UI_EMBOSS_UNDEFINED);
  if (block) {
    wt->draw(&wt->wcol, rect, block->flag, block->direction);
  }
  else {
    wt->draw(&wt->wcol, rect, 0, 0);
  }

  ui_draw_clip_tri(block, rect, wt);
}

/**
 * Uses the widget base drawing and colors from from the box widget, but ensures an opaque
 * inner color.
 */
void ui_draw_box_opaque(rcti *rect, int roundboxalign)
{
  uiWidgetType *wt = widget_type(UI_WTYPE_BOX);

  /* Alpha blend with the region's background color to force an opaque background. */
  uiWidgetColors *wcol = &wt->wcol;
  wt->state(wt, 0, 0, UI_EMBOSS_UNDEFINED);
  float background[4];
  UI_GetThemeColor4fv(TH_BACK, background);
  float new_inner[4];
  rgba_uchar_to_float(new_inner, wcol->inner);
  new_inner[0] = (new_inner[0] * new_inner[3]) + (background[0] * (1.0f - new_inner[3]));
  new_inner[1] = (new_inner[1] * new_inner[3]) + (background[1] * (1.0f - new_inner[3]));
  new_inner[2] = (new_inner[2] * new_inner[3]) + (background[2] * (1.0f - new_inner[3]));
  new_inner[3] = 1.0f;
  rgba_float_to_uchar(wcol->inner, new_inner);

  wt->custom(NULL, wcol, rect, 0, roundboxalign);
}

/**
 * Similar to 'widget_menu_back', however we can't use the widget preset system
 * because we need to pass in the original location so we know where to show the arrow.
 */
static void ui_draw_popover_back_impl(const uiWidgetColors *wcol,
                                      rcti *rect,
                                      int direction,
                                      const float unit_size,
                                      const float mval_origin[2])
{
  /* tsk, this isn't nice. */
  const float unit_half = unit_size / 2;
  const float cent_x = mval_origin ? CLAMPIS(mval_origin[0],
                                             rect->xmin + unit_size,
                                             rect->xmax - unit_size) :
                                     BLI_rcti_cent_x(rect);

  GPU_blend(GPU_BLEND_ALPHA);

  /* Extracted from 'widget_menu_back', keep separate to avoid menu changes breaking popovers */
  {
    uiWidgetBase wtb;
    widget_init(&wtb);

    const int roundboxalign = UI_CNR_ALL;
    widget_softshadow(rect, roundboxalign, wcol->roundness * U.widget_unit);

    round_box_edges(&wtb, roundboxalign, rect, wcol->roundness * U.widget_unit);
    wtb.draw_emboss = false;
    widgetbase_draw(&wtb, wcol);
  }

  /* Draw popover arrow (top/bottom) */
  if (ELEM(direction, UI_DIR_UP, UI_DIR_DOWN)) {
    const uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    const bool is_down = (direction == UI_DIR_DOWN);
    const int sign = is_down ? 1 : -1;
    float y = is_down ? rect->ymax : rect->ymin;

    GPU_blend(GPU_BLEND_ALPHA);
    immBegin(GPU_PRIM_TRIS, 3);
    immUniformColor4ubv(wcol->outline);
    immVertex2f(pos, cent_x - unit_half, y);
    immVertex2f(pos, cent_x + unit_half, y);
    immVertex2f(pos, cent_x, y + sign * unit_half);
    immEnd();

    y = y - sign * round(U.pixelsize * 1.41);

    GPU_blend(GPU_BLEND_NONE);
    immBegin(GPU_PRIM_TRIS, 3);
    immUniformColor4ub(0, 0, 0, 0);
    immVertex2f(pos, cent_x - unit_half, y);
    immVertex2f(pos, cent_x + unit_half, y);
    immVertex2f(pos, cent_x, y + sign * unit_half);
    immEnd();

    GPU_blend(GPU_BLEND_ALPHA);
    immBegin(GPU_PRIM_TRIS, 3);
    immUniformColor4ubv(wcol->inner);
    immVertex2f(pos, cent_x - unit_half, y);
    immVertex2f(pos, cent_x + unit_half, y);
    immVertex2f(pos, cent_x, y + sign * unit_half);
    immEnd();

    immUnbindProgram();
  }

  GPU_blend(GPU_BLEND_NONE);
}

void ui_draw_popover_back(struct ARegion *region,
                          uiStyle *UNUSED(style),
                          uiBlock *block,
                          rcti *rect)
{
  uiWidgetType *wt = widget_type(UI_WTYPE_MENU_BACK);

  if (block) {
    float mval_origin[2] = {UNPACK2(block->bounds_offset)};
    ui_window_to_block_fl(region, block, &mval_origin[0], &mval_origin[1]);
    ui_draw_popover_back_impl(
        wt->wcol_theme, rect, block->direction, U.widget_unit / block->aspect, mval_origin);
  }
  else {
    wt->state(wt, 0, 0, UI_EMBOSS_UNDEFINED);
    wt->draw(&wt->wcol, rect, 0, 0);
  }

  ui_draw_clip_tri(block, rect, wt);
}

static void draw_disk_shaded(float start,
                             float angle,
                             float radius_int,
                             float radius_ext,
                             int subd,
                             const uchar col1[4],
                             const uchar col2[4],
                             bool shaded)
{
  const float radius_ext_scale = (0.5f / radius_ext); /* 1 / (2 * radius_ext) */

  uint col;
  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  if (shaded) {
    col = GPU_vertformat_attr_add(format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);
  }
  else {
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    immUniformColor4ubv(col1);
  }

  immBegin(GPU_PRIM_TRI_STRIP, subd * 2);
  for (int i = 0; i < subd; i++) {
    const float a = start + ((i) / (float)(subd - 1)) * angle;
    const float s = sinf(a);
    const float c = cosf(a);
    const float y1 = s * radius_int;
    const float y2 = s * radius_ext;

    if (shaded) {
      uchar r_col[4];
      const float fac = (y1 + radius_ext) * radius_ext_scale;
      color_blend_v4_v4v4(r_col, col1, col2, fac);
      immAttr4ubv(col, r_col);
    }
    immVertex2f(pos, c * radius_int, s * radius_int);

    if (shaded) {
      uchar r_col[4];
      const float fac = (y2 + radius_ext) * radius_ext_scale;
      color_blend_v4_v4v4(r_col, col1, col2, fac);
      immAttr4ubv(col, r_col);
    }
    immVertex2f(pos, c * radius_ext, s * radius_ext);
  }
  immEnd();

  immUnbindProgram();
}

void ui_draw_pie_center(uiBlock *block)
{
  bTheme *btheme = UI_GetTheme();
  const float cx = block->pie_data.pie_center_spawned[0];
  const float cy = block->pie_data.pie_center_spawned[1];

  float *pie_dir = block->pie_data.pie_dir;

  const float pie_radius_internal = U.dpi_fac * U.pie_menu_threshold;
  const float pie_radius_external = U.dpi_fac * (U.pie_menu_threshold + 7.0f);

  const int subd = 40;

  const float angle = atan2f(pie_dir[1], pie_dir[0]);
  const float range = (block->pie_data.flags & UI_PIE_DEGREES_RANGE_LARGE) ? M_PI_2 : M_PI_4;

  GPU_matrix_push();
  GPU_matrix_translate_2f(cx, cy);

  GPU_blend(GPU_BLEND_ALPHA);
  if (btheme->tui.wcol_pie_menu.shaded) {
    uchar col1[4], col2[4];
    shadecolors4(col1,
                 col2,
                 btheme->tui.wcol_pie_menu.inner,
                 btheme->tui.wcol_pie_menu.shadetop,
                 btheme->tui.wcol_pie_menu.shadedown);
    draw_disk_shaded(0.0f,
                     (float)(M_PI * 2.0),
                     pie_radius_internal,
                     pie_radius_external,
                     subd,
                     col1,
                     col2,
                     true);
  }
  else {
    draw_disk_shaded(0.0f,
                     (float)(M_PI * 2.0),
                     pie_radius_internal,
                     pie_radius_external,
                     subd,
                     btheme->tui.wcol_pie_menu.inner,
                     NULL,
                     false);
  }

  if (!(block->pie_data.flags & UI_PIE_INVALID_DIR)) {
    if (btheme->tui.wcol_pie_menu.shaded) {
      uchar col1[4], col2[4];
      shadecolors4(col1,
                   col2,
                   btheme->tui.wcol_pie_menu.inner_sel,
                   btheme->tui.wcol_pie_menu.shadetop,
                   btheme->tui.wcol_pie_menu.shadedown);
      draw_disk_shaded(angle - range / 2.0f,
                       range,
                       pie_radius_internal,
                       pie_radius_external,
                       subd,
                       col1,
                       col2,
                       true);
    }
    else {
      draw_disk_shaded(angle - range / 2.0f,
                       range,
                       pie_radius_internal,
                       pie_radius_external,
                       subd,
                       btheme->tui.wcol_pie_menu.inner_sel,
                       NULL,
                       false);
    }
  }

  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor4ubv(btheme->tui.wcol_pie_menu.outline);

  imm_draw_circle_wire_2d(pos, 0.0f, 0.0f, pie_radius_internal, subd);
  imm_draw_circle_wire_2d(pos, 0.0f, 0.0f, pie_radius_external, subd);

  immUnbindProgram();

  if (U.pie_menu_confirm > 0 &&
      !(block->pie_data.flags & (UI_PIE_INVALID_DIR | UI_PIE_CLICK_STYLE))) {
    const float pie_confirm_radius = U.dpi_fac * (pie_radius_internal + U.pie_menu_confirm);
    const float pie_confirm_external = U.dpi_fac *
                                       (pie_radius_internal + U.pie_menu_confirm + 7.0f);

    const uchar col[4] = {UNPACK3(btheme->tui.wcol_pie_menu.text_sel), 64};
    draw_disk_shaded(angle - range / 2.0f,
                     range,
                     pie_confirm_radius,
                     pie_confirm_external,
                     subd,
                     col,
                     NULL,
                     false);
  }

  GPU_blend(GPU_BLEND_NONE);
  GPU_matrix_pop();
}

const uiWidgetColors *ui_tooltip_get_theme(void)
{
  uiWidgetType *wt = widget_type(UI_WTYPE_TOOLTIP);
  return wt->wcol_theme;
}

/**
 * Generic drawing for background.
 */
static void ui_draw_widget_back_color(uiWidgetTypeEnum type,
                                      bool use_shadow,
                                      const rcti *rect,
                                      const float color[4])
{
  uiWidgetType *wt = widget_type(type);

  if (use_shadow) {
    GPU_blend(GPU_BLEND_ALPHA);
    widget_softshadow(rect, UI_CNR_ALL, 0.25f * U.widget_unit);
    GPU_blend(GPU_BLEND_NONE);
  }

  rcti rect_copy = *rect;
  wt->state(wt, 0, 0, UI_EMBOSS_UNDEFINED);
  if (color) {
    rgba_float_to_uchar(wt->wcol.inner, color);
  }
  wt->draw(&wt->wcol, &rect_copy, 0, UI_CNR_ALL);
}
void ui_draw_widget_menu_back_color(const rcti *rect, bool use_shadow, const float color[4])
{
  ui_draw_widget_back_color(UI_WTYPE_MENU_BACK, use_shadow, rect, color);
}

void ui_draw_widget_menu_back(const rcti *rect, bool use_shadow)
{
  ui_draw_widget_back_color(UI_WTYPE_MENU_BACK, use_shadow, rect, NULL);
}

void ui_draw_tooltip_background(const uiStyle *UNUSED(style), uiBlock *UNUSED(block), rcti *rect)
{
  uiWidgetType *wt = widget_type(UI_WTYPE_TOOLTIP);
  wt->state(wt, 0, 0, UI_EMBOSS_UNDEFINED);
  /* wt->draw ends up using same function to draw the tooltip as menu_back */
  wt->draw(&wt->wcol, rect, 0, 0);
}

/**
 * Helper call to draw a menu item without a button.
 *
 * \param state: The state of the button,
 * typically #UI_ACTIVE, #UI_BUT_DISABLED, #UI_BUT_INACTIVE.
 * \param separator_type: The kind of separator which controls if and how the string is clipped.
 * \param r_xmax: The right hand position of the text, this takes into the icon,
 * padding and text clipping when there is not enough room to display the full text.
 */
void ui_draw_menu_item(const uiFontStyle *fstyle,
                       rcti *rect,
                       const char *name,
                       int iconid,
                       int state,
                       uiMenuItemSeparatorType separator_type,
                       int *r_xmax)
{
  uiWidgetType *wt = widget_type(UI_WTYPE_MENU_ITEM);
  const rcti _rect = *rect;
  const int row_height = BLI_rcti_size_y(rect);
  int max_hint_width = INT_MAX;
  int padding = 0.25f * row_height;
  char *cpoin = NULL;

  wt->state(wt, state, 0, UI_EMBOSS_UNDEFINED);
  wt->draw(&wt->wcol, rect, 0, 0);

  UI_fontstyle_set(fstyle);

  /* text location offset */
  rect->xmin += padding;
  if (iconid) {
    rect->xmin += row_height; /* Use square area for icon. */
  }

  /* cut string in 2 parts? */
  if (separator_type != UI_MENU_ITEM_SEPARATOR_NONE) {
    cpoin = strrchr(name, UI_SEP_CHAR);
    if (cpoin) {
      *cpoin = 0;

      /* need to set this first */
      UI_fontstyle_set(fstyle);

      if (fstyle->kerning == 1) {
        /* for BLF_width */
        BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
      }

      if (separator_type == UI_MENU_ITEM_SEPARATOR_SHORTCUT) {
        /* Shrink rect to exclude the shortcut string. */
        rect->xmax -= BLF_width(fstyle->uifont_id, cpoin + 1, INT_MAX) + UI_DPI_ICON_SIZE;
      }
      else if (separator_type == UI_MENU_ITEM_SEPARATOR_HINT) {
        /* Determine max-width for the hint string to leave the name string un-clipped (if there's
         * enough space to display it). */

        const int available_width = BLI_rcti_size_x(rect) - padding;
        const int name_width = BLF_width(fstyle->uifont_id, name, INT_MAX);
        const int hint_width = BLF_width(fstyle->uifont_id, cpoin + 1, INT_MAX) + padding;

        if ((name_width + hint_width) > available_width) {
          /* Clipping width for hint string. */
          max_hint_width = available_width * 0.40f;
          /* Clipping xmax for clipping of item name. */
          rect->xmax = (hint_width < max_hint_width) ?
                           (rect->xmax - hint_width) :
                           (rect->xmin + (available_width - max_hint_width));
        }
      }
      else {
        BLI_assert(!"Unknwon menu item separator type");
      }

      if (fstyle->kerning == 1) {
        BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
      }
    }
  }

  {
    char drawstr[UI_MAX_DRAW_STR];
    const float okwidth = (float)BLI_rcti_size_x(rect);
    const size_t max_len = sizeof(drawstr);
    const float minwidth = (float)(UI_DPI_ICON_SIZE);

    BLI_strncpy(drawstr, name, sizeof(drawstr));
    if (drawstr[0]) {
      UI_text_clip_middle_ex(fstyle, drawstr, okwidth, minwidth, max_len, '\0');
    }

    int xofs = 0, yofs = 0;
    struct ResultBLF info;
    UI_fontstyle_draw_ex(fstyle,
                         rect,
                         drawstr,
                         wt->wcol.text,
                         &(struct uiFontStyleDraw_Params){
                             .align = UI_STYLE_TEXT_LEFT,
                         },
                         BLF_DRAW_STR_DUMMY_MAX,
                         &xofs,
                         &yofs,
                         &info);
    if (r_xmax != NULL) {
      *r_xmax = xofs + info.width;
    }
  }

  /* restore rect, was messed with */
  *rect = _rect;

  if (iconid) {
    float height, aspect;
    const int xs = rect->xmin + 0.2f * UI_UNIT_X;
    const int ys = rect->ymin + 0.1f * BLI_rcti_size_y(rect);

    height = ICON_SIZE_FROM_BUTRECT(rect);
    aspect = ICON_DEFAULT_HEIGHT / height;

    GPU_blend(GPU_BLEND_ALPHA);
    /* XXX scale weak get from fstyle? */
    UI_icon_draw_ex(xs, ys, iconid, aspect, 1.0f, 0.0f, wt->wcol.text, false);
    GPU_blend(GPU_BLEND_NONE);
  }

  /* part text right aligned */
  if (separator_type != UI_MENU_ITEM_SEPARATOR_NONE) {
    if (cpoin) {
      /* Set inactive state for grayed out text. */
      wt->state(wt, state | UI_BUT_INACTIVE, 0, UI_EMBOSS_UNDEFINED);

      char hint_drawstr[UI_MAX_DRAW_STR];
      {
        const size_t max_len = sizeof(hint_drawstr);
        const float minwidth = (float)(UI_DPI_ICON_SIZE);

        BLI_strncpy(hint_drawstr, cpoin + 1, sizeof(hint_drawstr));
        if (hint_drawstr[0] && (max_hint_width < INT_MAX)) {
          UI_text_clip_middle_ex(fstyle, hint_drawstr, max_hint_width, minwidth, max_len, '\0');
        }
      }

      rect->xmax = _rect.xmax - 5;
      UI_fontstyle_draw(fstyle,
                        rect,
                        hint_drawstr,
                        wt->wcol.text,
                        &(struct uiFontStyleDraw_Params){
                            .align = UI_STYLE_TEXT_RIGHT,
                        });
      *cpoin = UI_SEP_CHAR;
    }
  }
}

void ui_draw_preview_item(
    const uiFontStyle *fstyle, rcti *rect, const char *name, int iconid, int state)
{
  rcti trect = *rect;
  const float text_size = UI_UNIT_Y;
  float font_dims[2] = {0.0f, 0.0f};
  uiWidgetType *wt = widget_type(UI_WTYPE_MENU_ITEM);

  /* drawing button background */
  wt->state(wt, state, 0, UI_EMBOSS_UNDEFINED);
  wt->draw(&wt->wcol, rect, 0, 0);

  /* draw icon in rect above the space reserved for the label */
  rect->ymin += text_size;
  GPU_blend(GPU_BLEND_ALPHA);
  widget_draw_preview(iconid, 1.0f, rect);
  GPU_blend(GPU_BLEND_NONE);

  BLF_width_and_height(
      fstyle->uifont_id, name, BLF_DRAW_STR_DUMMY_MAX, &font_dims[0], &font_dims[1]);

  /* text rect */
  trect.xmin += 0;
  trect.xmax = trect.xmin + font_dims[0] + U.widget_unit / 2;
  trect.ymin += U.widget_unit / 2;
  trect.ymax = trect.ymin + font_dims[1];
  if (trect.xmax > rect->xmax - PREVIEW_PAD) {
    trect.xmax = rect->xmax - PREVIEW_PAD;
  }

  {
    char drawstr[UI_MAX_DRAW_STR];
    const float okwidth = (float)BLI_rcti_size_x(&trect);
    const size_t max_len = sizeof(drawstr);
    const float minwidth = (float)(UI_DPI_ICON_SIZE);

    BLI_strncpy(drawstr, name, sizeof(drawstr));
    UI_text_clip_middle_ex(fstyle, drawstr, okwidth, minwidth, max_len, '\0');

    UI_fontstyle_draw(fstyle,
                      &trect,
                      drawstr,
                      wt->wcol.text,
                      &(struct uiFontStyleDraw_Params){
                          .align = UI_STYLE_TEXT_CENTER,
                      });
  }
}

/** \} */
