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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edinterface
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "GPU_draw.h"
#include "GPU_matrix.h"
#include "GPU_batch.h"
#include "GPU_immediate.h"
#include "GPU_state.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_fileops_types.h"
#include "BLI_math_vector.h"
#include "BLI_math_color_blend.h"

#include "DNA_brush_types.h"
#include "DNA_curve_types.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_workspace_types.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_paint.h"
#include "BKE_icons.h"
#include "BKE_appdir.h"
#include "BKE_studiolight.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "BIF_glutil.h"

#include "DEG_depsgraph.h"

#include "DRW_engine.h"

#include "ED_datafiles.h"
#include "ED_keyframes_draw.h"
#include "ED_render.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

#ifndef WITH_HEADLESS
#  define ICON_GRID_COLS 26
#  define ICON_GRID_ROWS 30

#  define ICON_MONO_BORDER_OUTSET 2
#  define ICON_GRID_MARGIN 10
#  define ICON_GRID_W 32
#  define ICON_GRID_H 32
#endif /* WITH_HEADLESS */

typedef struct IconImage {
  int w;
  int h;
  uint *rect;
  const uchar *datatoc_rect;
  int datatoc_size;
} IconImage;

typedef void (*VectorDrawFunc)(int x, int y, int w, int h, float alpha);

#define ICON_TYPE_PREVIEW 0
#define ICON_TYPE_COLOR_TEXTURE 1
#define ICON_TYPE_MONO_TEXTURE 2
#define ICON_TYPE_BUFFER 3
#define ICON_TYPE_VECTOR 4
#define ICON_TYPE_GEOM 5
#define ICON_TYPE_EVENT 6 /* draw keymap entries using custom renderer. */
#define ICON_TYPE_GPLAYER 7
#define ICON_TYPE_BLANK 8

typedef struct DrawInfo {
  int type;

  union {
    /* type specific data */
    struct {
      VectorDrawFunc func;
    } vector;
    struct {
      ImBuf *image_cache;
    } geom;
    struct {
      IconImage *image;
    } buffer;
    struct {
      int x, y, w, h;
      int theme_color;
    } texture;
    struct {
      /* Can be packed into a single int. */
      short event_type;
      short event_value;
      int icon;
      /* Allow lookups. */
      struct DrawInfo *next;
    } input;
  } data;
} DrawInfo;

typedef struct IconTexture {
  GLuint id[2];
  int num_textures;
  int w;
  int h;
  float invw;
  float invh;
} IconTexture;

typedef struct IconType {
  int type;
  int theme_color;
} IconType;

/* ******************* STATIC LOCAL VARS ******************* */
/* static here to cache results of icon directory scan, so it's not
 * scanning the filesystem each time the menu is drawn */
static struct ListBase iconfilelist = {NULL, NULL};
static IconTexture icongltex = {{0, 0}, 0, 0, 0, 0.0f, 0.0f};

#ifndef WITH_HEADLESS

static const IconType icontypes[] = {
#  define DEF_ICON(name) {ICON_TYPE_MONO_TEXTURE, 0},
#  define DEF_ICON_SCENE(name) {ICON_TYPE_MONO_TEXTURE, TH_ICON_SCENE},
#  define DEF_ICON_COLLECTION(name) {ICON_TYPE_MONO_TEXTURE, TH_ICON_COLLECTION},
#  define DEF_ICON_OBJECT(name) {ICON_TYPE_MONO_TEXTURE, TH_ICON_OBJECT},
#  define DEF_ICON_OBJECT_DATA(name) {ICON_TYPE_MONO_TEXTURE, TH_ICON_OBJECT_DATA},
#  define DEF_ICON_MODIFIER(name) {ICON_TYPE_MONO_TEXTURE, TH_ICON_MODIFIER},
#  define DEF_ICON_SHADING(name) {ICON_TYPE_MONO_TEXTURE, TH_ICON_SHADING},
#  define DEF_ICON_VECTOR(name) {ICON_TYPE_VECTOR, 0},
#  define DEF_ICON_COLOR(name) {ICON_TYPE_COLOR_TEXTURE, 0},
#  define DEF_ICON_BLANK(name) {ICON_TYPE_BLANK, 0},
#  include "UI_icons.h"
};

/* **************************************************** */

static DrawInfo *def_internal_icon(
    ImBuf *bbuf, int icon_id, int xofs, int yofs, int size, int type, int theme_color)
{
  Icon *new_icon = NULL;
  IconImage *iimg = NULL;
  DrawInfo *di;

  new_icon = MEM_callocN(sizeof(Icon), "texicon");

  new_icon->obj = NULL; /* icon is not for library object */
  new_icon->id_type = 0;

  di = MEM_callocN(sizeof(DrawInfo), "drawinfo");
  di->type = type;

  if (ELEM(type, ICON_TYPE_COLOR_TEXTURE, ICON_TYPE_MONO_TEXTURE)) {
    di->data.texture.theme_color = theme_color;
    di->data.texture.x = xofs;
    di->data.texture.y = yofs;
    di->data.texture.w = size;
    di->data.texture.h = size;
  }
  else if (type == ICON_TYPE_BUFFER) {
    iimg = MEM_callocN(sizeof(IconImage), "icon_img");
    iimg->w = size;
    iimg->h = size;

    /* icon buffers can get initialized runtime now, via datatoc */
    if (bbuf) {
      int y, imgsize;

      iimg->rect = MEM_mallocN(size * size * sizeof(uint), "icon_rect");

      /* Here we store the rect in the icon - same as before */
      if (size == bbuf->x && size == bbuf->y && xofs == 0 && yofs == 0) {
        memcpy(iimg->rect, bbuf->rect, size * size * sizeof(int));
      }
      else {
        /* this code assumes square images */
        imgsize = bbuf->x;
        for (y = 0; y < size; y++) {
          memcpy(
              &iimg->rect[y * size], &bbuf->rect[(y + yofs) * imgsize + xofs], size * sizeof(int));
        }
      }
    }
    di->data.buffer.image = iimg;
  }

  new_icon->drawinfo_free = UI_icons_free_drawinfo;
  new_icon->drawinfo = di;

  BKE_icon_set(icon_id, new_icon);

  return di;
}

static void def_internal_vicon(int icon_id, VectorDrawFunc drawFunc)
{
  Icon *new_icon = NULL;
  DrawInfo *di;

  new_icon = MEM_callocN(sizeof(Icon), "texicon");

  new_icon->obj = NULL; /* icon is not for library object */
  new_icon->id_type = 0;

  di = MEM_callocN(sizeof(DrawInfo), "drawinfo");
  di->type = ICON_TYPE_VECTOR;
  di->data.vector.func = drawFunc;

  new_icon->drawinfo_free = NULL;
  new_icon->drawinfo = di;

  BKE_icon_set(icon_id, new_icon);
}

/* Vector Icon Drawing Routines */

/* Utilities */

static void viconutil_set_point(GLint pt[2], int x, int y)
{
  pt[0] = x;
  pt[1] = y;
}

static void vicon_small_tri_right_draw(int x, int y, int w, int UNUSED(h), float alpha)
{
  GLint pts[3][2];
  int cx = x + w / 2 - 4;
  int cy = y + w / 2;
  int d = w / 5, d2 = w / 7;

  viconutil_set_point(pts[0], cx - d2, cy + d);
  viconutil_set_point(pts[1], cx - d2, cy - d);
  viconutil_set_point(pts[2], cx + d2, cy);

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor4f(0.2f, 0.2f, 0.2f, alpha);

  immBegin(GPU_PRIM_TRIS, 3);
  immVertex2iv(pos, pts[0]);
  immVertex2iv(pos, pts[1]);
  immVertex2iv(pos, pts[2]);
  immEnd();

  immUnbindProgram();
}

static void vicon_keytype_draw_wrapper(
    int x, int y, int w, int h, float alpha, short key_type, short handle_type)
{
  /* init dummy theme state for Action Editor - where these colors are defined
   * (since we're doing this offscreen, free from any particular space_id)
   */
  struct bThemeState theme_state;

  UI_Theme_Store(&theme_state);
  UI_SetTheme(SPACE_ACTION, RGN_TYPE_WINDOW);

  /* the "x" and "y" given are the bottom-left coordinates of the icon,
   * while the draw_keyframe_shape() function needs the midpoint for
   * the keyframe
   */
  float xco = x + w / 2 + 0.5f;
  float yco = y + h / 2 + 0.5f;

  GPUVertFormat *format = immVertexFormat();
  uint pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint size_id = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  uint color_id = GPU_vertformat_attr_add(
      format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  uint outline_color_id = GPU_vertformat_attr_add(
      format, "outlineColor", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  uint flags_id = GPU_vertformat_attr_add(format, "flags", GPU_COMP_U32, 1, GPU_FETCH_INT);

  immBindBuiltinProgram(GPU_SHADER_KEYFRAME_DIAMOND);
  GPU_enable_program_point_size();
  immUniform2f("ViewportSize", -1.0f, -1.0f);
  immBegin(GPU_PRIM_POINTS, 1);

  /* draw keyframe
   * - size: (default icon size == 16, default dopesheet icon size == 10)
   * - sel: true unless in handletype icons (so that "keyframe" state shows the iconic yellow icon)
   */
  bool sel = (handle_type == KEYFRAME_HANDLE_NONE);

  draw_keyframe_shape(xco,
                      yco,
                      (10.0f / 16.0f) * h,
                      sel,
                      key_type,
                      KEYFRAME_SHAPE_BOTH,
                      alpha,
                      pos_id,
                      size_id,
                      color_id,
                      outline_color_id,
                      flags_id,
                      handle_type,
                      KEYFRAME_EXTREME_NONE);

  immEnd();
  GPU_disable_program_point_size();
  immUnbindProgram();

  UI_Theme_Restore(&theme_state);
}

static void vicon_keytype_keyframe_draw(int x, int y, int w, int h, float alpha)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_KEYFRAME, KEYFRAME_HANDLE_NONE);
}

static void vicon_keytype_breakdown_draw(int x, int y, int w, int h, float alpha)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_BREAKDOWN, KEYFRAME_HANDLE_NONE);
}

static void vicon_keytype_extreme_draw(int x, int y, int w, int h, float alpha)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_EXTREME, KEYFRAME_HANDLE_NONE);
}

static void vicon_keytype_jitter_draw(int x, int y, int w, int h, float alpha)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_JITTER, KEYFRAME_HANDLE_NONE);
}

static void vicon_keytype_moving_hold_draw(int x, int y, int w, int h, float alpha)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_MOVEHOLD, KEYFRAME_HANDLE_NONE);
}

static void vicon_handletype_free_draw(int x, int y, int w, int h, float alpha)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_KEYFRAME, KEYFRAME_HANDLE_FREE);
}

static void vicon_handletype_aligned_draw(int x, int y, int w, int h, float alpha)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_KEYFRAME, KEYFRAME_HANDLE_ALIGNED);
}

static void vicon_handletype_vector_draw(int x, int y, int w, int h, float alpha)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_KEYFRAME, KEYFRAME_HANDLE_VECTOR);
}

static void vicon_handletype_auto_draw(int x, int y, int w, int h, float alpha)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_KEYFRAME, KEYFRAME_HANDLE_AUTO);
}

static void vicon_handletype_auto_clamp_draw(int x, int y, int w, int h, float alpha)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_KEYFRAME, KEYFRAME_HANDLE_AUTO_CLAMP);
}

static void vicon_colorset_draw(int index, int x, int y, int w, int h, float UNUSED(alpha))
{
  bTheme *btheme = UI_GetTheme();
  ThemeWireColor *cs = &btheme->tarm[index];

  /* Draw three bands of color: One per color
   *    x-----a-----b-----c
   *    |  N  |  S  |  A  |
   *    x-----a-----b-----c
   */
  const int a = x + w / 3;
  const int b = x + w / 3 * 2;
  const int c = x + w;

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  /* XXX: Include alpha into this... */
  /* normal */
  immUniformColor3ubv((uchar *)cs->solid);
  immRecti(pos, x, y, a, y + h);

  /* selected */
  immUniformColor3ubv((uchar *)cs->select);
  immRecti(pos, a, y, b, y + h);

  /* active */
  immUniformColor3ubv((uchar *)cs->active);
  immRecti(pos, b, y, c, y + h);

  immUnbindProgram();
}

#  define DEF_ICON_VECTOR_COLORSET_DRAW_NTH(prefix, index) \
    static void vicon_colorset_draw_##prefix(int x, int y, int w, int h, float alpha) \
    { \
      vicon_colorset_draw(index, x, y, w, h, alpha); \
    }

DEF_ICON_VECTOR_COLORSET_DRAW_NTH(01, 0)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(02, 1)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(03, 2)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(04, 3)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(05, 4)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(06, 5)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(07, 6)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(08, 7)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(09, 8)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(10, 9)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(11, 10)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(12, 11)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(13, 12)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(14, 13)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(15, 14)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(16, 15)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(17, 16)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(18, 17)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(19, 18)
DEF_ICON_VECTOR_COLORSET_DRAW_NTH(20, 19)

#  undef DEF_ICON_VECTOR_COLORSET_DRAW_NTH

/* Dynamically render icon instead of rendering a plain color to a texture/buffer
 * This is mot strictly a "vicon", as it needs access to icon->obj to get the color info,
 * but it works in a very similar way.
 */
static void vicon_gplayer_color_draw(Icon *icon, int x, int y, int w, int h)
{
  bGPDlayer *gpl = (bGPDlayer *)icon->obj;

  /* Just draw a colored rect - Like for vicon_colorset_draw() */
  /* TODO: Make this have rounded corners, and maybe be a bit smaller.
   * However, UI_draw_roundbox_aa() draws the colors too dark, so can't be used.
   */
  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformColor3fv(gpl->color);
  immRecti(pos, x, y, x + w - 1, y + h - 1);

  immUnbindProgram();
}

#  ifndef WITH_HEADLESS

static void init_brush_icons(void)
{

#    define INIT_BRUSH_ICON(icon_id, name) \
      { \
        uchar *rect = (uchar *)datatoc_##name##_png; \
        int size = datatoc_##name##_png_size; \
        DrawInfo *di; \
\
        di = def_internal_icon(NULL, icon_id, 0, 0, w, ICON_TYPE_BUFFER, 0); \
        di->data.buffer.image->datatoc_rect = rect; \
        di->data.buffer.image->datatoc_size = size; \
      } \
      ((void)0)
  /* end INIT_BRUSH_ICON */

  const int w = 96; /* warning, brush size hardcoded in C, but it gets scaled */

  INIT_BRUSH_ICON(ICON_BRUSH_BLOB, blob);
  INIT_BRUSH_ICON(ICON_BRUSH_BLUR, blur);
  INIT_BRUSH_ICON(ICON_BRUSH_CLAY, clay);
  INIT_BRUSH_ICON(ICON_BRUSH_CLAY_STRIPS, claystrips);
  INIT_BRUSH_ICON(ICON_BRUSH_CLONE, clone);
  INIT_BRUSH_ICON(ICON_BRUSH_CREASE, crease);
  INIT_BRUSH_ICON(ICON_BRUSH_SCULPT_DRAW, draw);
  INIT_BRUSH_ICON(ICON_BRUSH_FILL, fill);
  INIT_BRUSH_ICON(ICON_BRUSH_FLATTEN, flatten);
  INIT_BRUSH_ICON(ICON_BRUSH_GRAB, grab);
  INIT_BRUSH_ICON(ICON_BRUSH_INFLATE, inflate);
  INIT_BRUSH_ICON(ICON_BRUSH_LAYER, layer);
  INIT_BRUSH_ICON(ICON_BRUSH_MASK, mask);
  INIT_BRUSH_ICON(ICON_BRUSH_MIX, mix);
  INIT_BRUSH_ICON(ICON_BRUSH_NUDGE, nudge);
  INIT_BRUSH_ICON(ICON_BRUSH_PINCH, pinch);
  INIT_BRUSH_ICON(ICON_BRUSH_SCRAPE, scrape);
  INIT_BRUSH_ICON(ICON_BRUSH_SMEAR, smear);
  INIT_BRUSH_ICON(ICON_BRUSH_SMOOTH, smooth);
  INIT_BRUSH_ICON(ICON_BRUSH_SNAKE_HOOK, snake_hook);
  INIT_BRUSH_ICON(ICON_BRUSH_SOFTEN, soften);
  INIT_BRUSH_ICON(ICON_BRUSH_TEXDRAW, texdraw);
  INIT_BRUSH_ICON(ICON_BRUSH_TEXFILL, texfill);
  INIT_BRUSH_ICON(ICON_BRUSH_TEXMASK, texmask);
  INIT_BRUSH_ICON(ICON_BRUSH_THUMB, thumb);
  INIT_BRUSH_ICON(ICON_BRUSH_ROTATE, twist);

  /* grease pencil sculpt */
  INIT_BRUSH_ICON(ICON_GPBRUSH_SMOOTH, gp_brush_smooth);
  INIT_BRUSH_ICON(ICON_GPBRUSH_THICKNESS, gp_brush_thickness);
  INIT_BRUSH_ICON(ICON_GPBRUSH_STRENGTH, gp_brush_strength);
  INIT_BRUSH_ICON(ICON_GPBRUSH_GRAB, gp_brush_grab);
  INIT_BRUSH_ICON(ICON_GPBRUSH_PUSH, gp_brush_push);
  INIT_BRUSH_ICON(ICON_GPBRUSH_TWIST, gp_brush_twist);
  INIT_BRUSH_ICON(ICON_GPBRUSH_PINCH, gp_brush_pinch);
  INIT_BRUSH_ICON(ICON_GPBRUSH_RANDOMIZE, gp_brush_randomize);
  INIT_BRUSH_ICON(ICON_GPBRUSH_CLONE, gp_brush_clone);
  INIT_BRUSH_ICON(ICON_GPBRUSH_WEIGHT, gp_brush_weight);

  /* grease pencil drawing brushes */
  INIT_BRUSH_ICON(ICON_GPBRUSH_PENCIL, gp_brush_pencil);
  INIT_BRUSH_ICON(ICON_GPBRUSH_PEN, gp_brush_pen);
  INIT_BRUSH_ICON(ICON_GPBRUSH_INK, gp_brush_ink);
  INIT_BRUSH_ICON(ICON_GPBRUSH_INKNOISE, gp_brush_inknoise);
  INIT_BRUSH_ICON(ICON_GPBRUSH_BLOCK, gp_brush_block);
  INIT_BRUSH_ICON(ICON_GPBRUSH_MARKER, gp_brush_marker);
  INIT_BRUSH_ICON(ICON_GPBRUSH_FILL, gp_brush_fill);
  INIT_BRUSH_ICON(ICON_GPBRUSH_ERASE_SOFT, gp_brush_erase_soft);
  INIT_BRUSH_ICON(ICON_GPBRUSH_ERASE_HARD, gp_brush_erase_hard);
  INIT_BRUSH_ICON(ICON_GPBRUSH_ERASE_STROKE, gp_brush_erase_stroke);

#    undef INIT_BRUSH_ICON
}

static DrawInfo *g_di_event_list = NULL;

int UI_icon_from_event_type(short event_type, short event_value)
{
  if (event_type == RIGHTSHIFTKEY) {
    event_type = LEFTSHIFTKEY;
  }
  else if (event_type == RIGHTCTRLKEY) {
    event_type = LEFTCTRLKEY;
  }
  else if (event_type == RIGHTALTKEY) {
    event_type = LEFTALTKEY;
  }
  else if (event_type == EVT_TWEAK_L) {
    event_type = LEFTMOUSE;
    event_value = KM_CLICK_DRAG;
  }
  else if (event_type == EVT_TWEAK_M) {
    event_type = MIDDLEMOUSE;
    event_value = KM_CLICK_DRAG;
  }
  else if (event_type == EVT_TWEAK_R) {
    event_type = RIGHTMOUSE;
    event_value = KM_CLICK_DRAG;
  }

  DrawInfo *di = g_di_event_list;
  do {
    if (di->data.input.event_type == event_type) {
      return di->data.input.icon;
    }
  } while ((di = di->data.input.next));

  if (event_type == LEFTMOUSE) {
    return ELEM(event_value, KM_CLICK, KM_PRESS) ? ICON_MOUSE_LMB : ICON_MOUSE_LMB_DRAG;
  }
  else if (event_type == MIDDLEMOUSE) {
    return ELEM(event_value, KM_CLICK, KM_PRESS) ? ICON_MOUSE_MMB : ICON_MOUSE_MMB_DRAG;
  }
  else if (event_type == RIGHTMOUSE) {
    return ELEM(event_value, KM_CLICK, KM_PRESS) ? ICON_MOUSE_RMB : ICON_MOUSE_RMB_DRAG;
  }

  return ICON_NONE;
}

int UI_icon_from_keymap_item(const wmKeyMapItem *kmi, int r_icon_mod[4])
{
  if (r_icon_mod) {
    memset(r_icon_mod, 0x0, sizeof(int[4]));
    int i = 0;
    if (!ELEM(kmi->ctrl, KM_NOTHING, KM_ANY)) {
      r_icon_mod[i++] = ICON_EVENT_CTRL;
    }
    if (!ELEM(kmi->alt, KM_NOTHING, KM_ANY)) {
      r_icon_mod[i++] = ICON_EVENT_ALT;
    }
    if (!ELEM(kmi->shift, KM_NOTHING, KM_ANY)) {
      r_icon_mod[i++] = ICON_EVENT_SHIFT;
    }
    if (!ELEM(kmi->oskey, KM_NOTHING, KM_ANY)) {
      r_icon_mod[i++] = ICON_EVENT_OS;
    }
  }
  return UI_icon_from_event_type(kmi->type, kmi->val);
}

static void init_event_icons(void)
{
  DrawInfo *di_next = NULL;

#    define INIT_EVENT_ICON(icon_id, type, value) \
      { \
        DrawInfo *di = def_internal_icon(NULL, icon_id, 0, 0, w, ICON_TYPE_EVENT, 0); \
        di->data.input.event_type = type; \
        di->data.input.event_value = value; \
        di->data.input.icon = icon_id; \
        di->data.input.next = di_next; \
        di_next = di; \
      } \
      ((void)0)
  /* end INIT_EVENT_ICON */

  const int w = 16; /* DUMMY */

  INIT_EVENT_ICON(ICON_EVENT_A, AKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_B, BKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_C, CKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_D, DKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_E, EKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F, FKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_G, GKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_H, HKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_I, IKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_J, JKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_K, KKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_L, LKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_M, MKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_N, NKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_O, OKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_P, PKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_Q, QKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_R, RKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_S, SKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_T, TKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_U, UKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_V, VKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_W, WKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_X, XKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_Y, YKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_Z, ZKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_SHIFT, LEFTSHIFTKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_CTRL, LEFTCTRLKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_ALT, LEFTALTKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_OS, OSKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F1, F1KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F2, F2KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F3, F3KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F4, F4KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F5, F5KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F6, F6KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F7, F7KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F8, F8KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F9, F9KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F10, F10KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F11, F11KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F12, F12KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_ESC, ESCKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_TAB, TABKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAGEUP, PAGEUPKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAGEDOWN, PAGEDOWNKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_RETURN, RETKEY, KM_ANY);

  g_di_event_list = di_next;

#    undef INIT_EVENT_ICON
}

static void icon_verify_datatoc(IconImage *iimg)
{
  /* if it has own rect, things are all OK */
  if (iimg->rect) {
    return;
  }

  if (iimg->datatoc_rect) {
    ImBuf *bbuf = IMB_ibImageFromMemory(
        iimg->datatoc_rect, iimg->datatoc_size, IB_rect, NULL, "<matcap icon>");
    /* w and h were set on initialize */
    if (bbuf->x != iimg->h && bbuf->y != iimg->w) {
      IMB_scaleImBuf(bbuf, iimg->w, iimg->h);
    }

    iimg->rect = bbuf->rect;
    bbuf->rect = NULL;
    IMB_freeImBuf(bbuf);
  }
}

static ImBuf *create_mono_icon_with_border(ImBuf *buf,
                                           int resolution_divider,
                                           float border_intensity)
{
  ImBuf *result = IMB_dupImBuf(buf);
  const float border_sharpness = 16.0 / (resolution_divider * resolution_divider);

  float blurred_alpha_buffer[(ICON_GRID_W + 2 * ICON_MONO_BORDER_OUTSET) *
                             (ICON_GRID_H + 2 * ICON_MONO_BORDER_OUTSET)];
  const int icon_width = (ICON_GRID_W + 2 * ICON_MONO_BORDER_OUTSET) / resolution_divider;
  const int icon_height = (ICON_GRID_W + 2 * ICON_MONO_BORDER_OUTSET) / resolution_divider;

  for (int y = 0; y < ICON_GRID_ROWS; y++) {
    for (int x = 0; x < ICON_GRID_COLS; x++) {
      IconType icontype = icontypes[y * ICON_GRID_COLS + x];
      if (icontype.type != ICON_TYPE_MONO_TEXTURE) {
        continue;
      }

      int sx = x * (ICON_GRID_W + ICON_GRID_MARGIN) + ICON_GRID_MARGIN - ICON_MONO_BORDER_OUTSET;
      int sy = y * (ICON_GRID_H + ICON_GRID_MARGIN) + ICON_GRID_MARGIN - ICON_MONO_BORDER_OUTSET;
      sx = sx / resolution_divider;
      sy = sy / resolution_divider;

      /* blur the alpha channel and store it in blurred_alpha_buffer */
      int blur_size = 2 / resolution_divider;
      for (int bx = 0; bx < icon_width; bx++) {
        const int asx = MAX2(bx - blur_size, 0);
        const int aex = MIN2(bx + blur_size + 1, icon_width);
        for (int by = 0; by < icon_height; by++) {
          const int asy = MAX2(by - blur_size, 0);
          const int aey = MIN2(by + blur_size + 1, icon_height);

          // blur alpha channel
          const int write_offset = by * (ICON_GRID_W + 2 * ICON_MONO_BORDER_OUTSET) + bx;
          float alpha_accum = 0.0;
          unsigned int alpha_samples = 0;
          for (int ax = asx; ax < aex; ax++) {
            for (int ay = asy; ay < aey; ay++) {
              const int offset_read = (sy + ay) * buf->x + (sx + ax);
              unsigned int color_read = buf->rect[offset_read];
              const float alpha_read = ((color_read & 0xff000000) >> 24) / 255.0;
              alpha_accum += alpha_read;
              alpha_samples += 1;
            }
          }
          blurred_alpha_buffer[write_offset] = alpha_accum / alpha_samples;
        }
      }

      /* apply blurred alpha */
      for (int bx = 0; bx < icon_width; bx++) {
        for (int by = 0; by < icon_height; by++) {
          const int blurred_alpha_offset = by * (ICON_GRID_W + 2 * ICON_MONO_BORDER_OUTSET) + bx;
          const int offset_write = (sy + by) * buf->x + (sx + bx);
          const float blurred_alpha = blurred_alpha_buffer[blurred_alpha_offset];
          float border_srgb[4] = {
              0, 0, 0, MIN2(1.0, blurred_alpha * border_sharpness) * border_intensity};

          const unsigned int color_read = buf->rect[offset_write];
          const unsigned char *orig_color = (unsigned char *)&color_read;

          float border_rgba[4];
          float orig_rgba[4];
          float dest_rgba[4];
          float dest_srgb[4];

          srgb_to_linearrgb_v4(border_rgba, border_srgb);
          srgb_to_linearrgb_uchar4(orig_rgba, orig_color);
          blend_color_interpolate_float(dest_rgba, orig_rgba, border_rgba, 1.0 - orig_rgba[3]);
          linearrgb_to_srgb_v4(dest_srgb, dest_rgba);

          unsigned int alpha_mask = ((unsigned int)(dest_srgb[3] * 255)) << 24;
          unsigned int cpack = rgb_to_cpack(dest_srgb[0], dest_srgb[1], dest_srgb[2]) | alpha_mask;
          result->rect[offset_write] = cpack;
        }
      }
    }
  }
  return result;
}

/* Generate the mipmap levels for the icon textures
 * During creation the source16 ImBuf will be freed to reduce memory overhead
 * A new ImBuf will be returned that needs is owned by the caller.
 *
 * FIXME: Mipmap levels are generated until the width of the image is 1, which
 *        are too many levels than that are needed.*/
static ImBuf *create_mono_icon_mipmaps(ImBuf *source32, ImBuf *source16, int level)
{
  if (level == 0) {
    glTexImage2D(GL_TEXTURE_2D,
                 level,
                 GL_RGBA8,
                 source32->x,
                 source32->y,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 source32->rect);
    return create_mono_icon_mipmaps(source32, source16, level + 1);
  }
  else {
    glTexImage2D(GL_TEXTURE_2D,
                 level,
                 GL_RGBA8,
                 source16->x,
                 source16->y,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 source16->rect);
    if (source16->x > 1) {
      ImBuf *nbuf = IMB_onehalf(source16);
      IMB_freeImBuf(source16);
      source16 = create_mono_icon_mipmaps(source32, nbuf, level + 1);
    }
    return source16;
  }
}

static void free_icons_textures(void)
{
  if (icongltex.num_textures > 0) {
    glDeleteTextures(icongltex.num_textures, icongltex.id);
    icongltex.id[0] = 0;
    icongltex.id[1] = 0;
    icongltex.num_textures = 0;
  }
}

/* Reload the textures for internal icons.
 * This function will release the previous textures. */
void UI_icons_reload_internal_textures(void)
{
  bTheme *btheme = UI_GetTheme();
  ImBuf *b16buf = NULL, *b32buf = NULL, *b16buf_border = NULL, *b32buf_border = NULL;
  const float icon_border_intensity = btheme->tui.icon_border_intensity;
  bool need_icons_with_border = icon_border_intensity > 0.0f;

  if (b16buf == NULL) {
    b16buf = IMB_ibImageFromMemory((const uchar *)datatoc_blender_icons16_png,
                                   datatoc_blender_icons16_png_size,
                                   IB_rect,
                                   NULL,
                                   "<blender icons>");
  }
  if (b16buf) {
    if (need_icons_with_border) {
      b16buf_border = create_mono_icon_with_border(b16buf, 2, icon_border_intensity);
      IMB_premultiply_alpha(b16buf_border);
    }
    IMB_premultiply_alpha(b16buf);
  }

  if (b32buf == NULL) {
    b32buf = IMB_ibImageFromMemory((const uchar *)datatoc_blender_icons32_png,
                                   datatoc_blender_icons32_png_size,
                                   IB_rect,
                                   NULL,
                                   "<blender icons>");
  }
  if (b32buf) {
    if (need_icons_with_border) {
      b32buf_border = create_mono_icon_with_border(b32buf, 1, icon_border_intensity);
      IMB_premultiply_alpha(b32buf_border);
    }
    IMB_premultiply_alpha(b32buf);
  }

  if (b16buf && b32buf) {
    /* Free existing texture if any. */
    free_icons_textures();

    /* Allocate OpenGL texture. */
    icongltex.num_textures = need_icons_with_border ? 2 : 1;
    glGenTextures(icongltex.num_textures, icongltex.id);

    if (icongltex.id) {
      icongltex.w = b32buf->x;
      icongltex.h = b32buf->y;
      icongltex.invw = 1.0f / b32buf->x;
      icongltex.invh = 1.0f / b32buf->y;

      glBindTexture(GL_TEXTURE_2D, icongltex.id[0]);
      b16buf = create_mono_icon_mipmaps(b32buf, b16buf, 0);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glBindTexture(GL_TEXTURE_2D, 0);
    }

    if (need_icons_with_border && icongltex.id[1]) {
      glBindTexture(GL_TEXTURE_2D, icongltex.id[1]);
      b16buf_border = create_mono_icon_mipmaps(b32buf_border, b16buf_border, 0);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glBindTexture(GL_TEXTURE_2D, 0);
    }
  }

  IMB_freeImBuf(b16buf);
  IMB_freeImBuf(b32buf);
  IMB_freeImBuf(b16buf_border);
  IMB_freeImBuf(b32buf_border);
}

static void init_internal_icons(void)
{
  int x, y;

#    if 0  // temp disabled
  if ((btheme != NULL) && btheme->tui.iconfile[0]) {
    char *icondir = BKE_appdir_folder_id(BLENDER_DATAFILES, "icons");
    char iconfilestr[FILE_MAX];

    if (icondir) {
      BLI_join_dirfile(iconfilestr, sizeof(iconfilestr), icondir, btheme->tui.iconfile);

      /* if the image is missing bbuf will just be NULL */
      bbuf = IMB_loadiffname(iconfilestr, IB_rect, NULL);

      if (bbuf && (bbuf->x < ICON_IMAGE_W || bbuf->y < ICON_IMAGE_H)) {
        printf(
            "\n***WARNING***\n"
            "Icons file '%s' too small.\n"
            "Using built-in Icons instead\n",
            iconfilestr);
        IMB_freeImBuf(bbuf);
        bbuf = NULL;
      }
    }
    else {
      printf("%s: 'icons' data path not found, continuing\n", __func__);
    }
  }
#    endif

  /* Define icons. */
  for (y = 0; y < ICON_GRID_ROWS; y++) {
    /* Row W has monochrome icons. */
    for (x = 0; x < ICON_GRID_COLS; x++) {
      IconType icontype = icontypes[y * ICON_GRID_COLS + x];
      if (!ELEM(icontype.type, ICON_TYPE_COLOR_TEXTURE, ICON_TYPE_MONO_TEXTURE)) {
        continue;
      }

      def_internal_icon(NULL,
                        BIFICONID_FIRST + y * ICON_GRID_COLS + x,
                        x * (ICON_GRID_W + ICON_GRID_MARGIN) + ICON_GRID_MARGIN,
                        y * (ICON_GRID_H + ICON_GRID_MARGIN) + ICON_GRID_MARGIN,
                        ICON_GRID_W,
                        icontype.type,
                        icontype.theme_color);
    }
  }

  def_internal_vicon(ICON_SMALL_TRI_RIGHT_VEC, vicon_small_tri_right_draw);

  def_internal_vicon(ICON_KEYTYPE_KEYFRAME_VEC, vicon_keytype_keyframe_draw);
  def_internal_vicon(ICON_KEYTYPE_BREAKDOWN_VEC, vicon_keytype_breakdown_draw);
  def_internal_vicon(ICON_KEYTYPE_EXTREME_VEC, vicon_keytype_extreme_draw);
  def_internal_vicon(ICON_KEYTYPE_JITTER_VEC, vicon_keytype_jitter_draw);
  def_internal_vicon(ICON_KEYTYPE_MOVING_HOLD_VEC, vicon_keytype_moving_hold_draw);

  def_internal_vicon(ICON_HANDLETYPE_FREE_VEC, vicon_handletype_free_draw);
  def_internal_vicon(ICON_HANDLETYPE_ALIGNED_VEC, vicon_handletype_aligned_draw);
  def_internal_vicon(ICON_HANDLETYPE_VECTOR_VEC, vicon_handletype_vector_draw);
  def_internal_vicon(ICON_HANDLETYPE_AUTO_VEC, vicon_handletype_auto_draw);
  def_internal_vicon(ICON_HANDLETYPE_AUTO_CLAMP_VEC, vicon_handletype_auto_clamp_draw);

  def_internal_vicon(ICON_COLORSET_01_VEC, vicon_colorset_draw_01);
  def_internal_vicon(ICON_COLORSET_02_VEC, vicon_colorset_draw_02);
  def_internal_vicon(ICON_COLORSET_03_VEC, vicon_colorset_draw_03);
  def_internal_vicon(ICON_COLORSET_04_VEC, vicon_colorset_draw_04);
  def_internal_vicon(ICON_COLORSET_05_VEC, vicon_colorset_draw_05);
  def_internal_vicon(ICON_COLORSET_06_VEC, vicon_colorset_draw_06);
  def_internal_vicon(ICON_COLORSET_07_VEC, vicon_colorset_draw_07);
  def_internal_vicon(ICON_COLORSET_08_VEC, vicon_colorset_draw_08);
  def_internal_vicon(ICON_COLORSET_09_VEC, vicon_colorset_draw_09);
  def_internal_vicon(ICON_COLORSET_10_VEC, vicon_colorset_draw_10);
  def_internal_vicon(ICON_COLORSET_11_VEC, vicon_colorset_draw_11);
  def_internal_vicon(ICON_COLORSET_12_VEC, vicon_colorset_draw_12);
  def_internal_vicon(ICON_COLORSET_13_VEC, vicon_colorset_draw_13);
  def_internal_vicon(ICON_COLORSET_14_VEC, vicon_colorset_draw_14);
  def_internal_vicon(ICON_COLORSET_15_VEC, vicon_colorset_draw_15);
  def_internal_vicon(ICON_COLORSET_16_VEC, vicon_colorset_draw_16);
  def_internal_vicon(ICON_COLORSET_17_VEC, vicon_colorset_draw_17);
  def_internal_vicon(ICON_COLORSET_18_VEC, vicon_colorset_draw_18);
  def_internal_vicon(ICON_COLORSET_19_VEC, vicon_colorset_draw_19);
  def_internal_vicon(ICON_COLORSET_20_VEC, vicon_colorset_draw_20);
}
#  endif /* WITH_HEADLESS */

static void init_iconfile_list(struct ListBase *list)
{
  IconFile *ifile;
  struct direntry *dir;
  int totfile, i, index = 1;
  const char *icondir;

  BLI_listbase_clear(list);
  icondir = BKE_appdir_folder_id(BLENDER_DATAFILES, "icons");

  if (icondir == NULL) {
    return;
  }

  totfile = BLI_filelist_dir_contents(icondir, &dir);

  for (i = 0; i < totfile; i++) {
    if ((dir[i].type & S_IFREG)) {
      const char *filename = dir[i].relname;

      if (BLI_path_extension_check(filename, ".png")) {
        /* loading all icons on file start is overkill & slows startup
         * its possible they change size after blender load anyway. */
#  if 0
        int ifilex, ifiley;
        char iconfilestr[FILE_MAX + 16]; /* allow 256 chars for file+dir */
        ImBuf *bbuf = NULL;
        /* check to see if the image is the right size, continue if not */
        /* copying strings here should go ok, assuming that we never get back
         * a complete path to file longer than 256 chars */
        BLI_join_dirfile(iconfilestr, sizeof(iconfilestr), icondir, filename);
        bbuf = IMB_loadiffname(iconfilestr, IB_rect);

        if (bbuf) {
          ifilex = bbuf->x;
          ifiley = bbuf->y;
          IMB_freeImBuf(bbuf);
        }
        else {
          ifilex = ifiley = 0;
        }

        /* bad size or failed to load */
        if ((ifilex != ICON_IMAGE_W) || (ifiley != ICON_IMAGE_H)) {
          printf("icon '%s' is wrong size %dx%d\n", iconfilestr, ifilex, ifiley);
          continue;
        }
#  endif /* removed */

        /* found a potential icon file, so make an entry for it in the cache list */
        ifile = MEM_callocN(sizeof(IconFile), "IconFile");

        BLI_strncpy(ifile->filename, filename, sizeof(ifile->filename));
        ifile->index = index;

        BLI_addtail(list, ifile);

        index++;
      }
    }
  }

  BLI_filelist_free(dir, totfile);
  dir = NULL;
}

static void free_iconfile_list(struct ListBase *list)
{
  IconFile *ifile = NULL, *next_ifile = NULL;

  for (ifile = list->first; ifile; ifile = next_ifile) {
    next_ifile = ifile->next;
    BLI_freelinkN(list, ifile);
  }
}

#endif /* WITH_HEADLESS */

int UI_iconfile_get_index(const char *filename)
{
  IconFile *ifile;
  ListBase *list = &(iconfilelist);

  for (ifile = list->first; ifile; ifile = ifile->next) {
    if (BLI_path_cmp(filename, ifile->filename) == 0) {
      return ifile->index;
    }
  }

  return 0;
}

ListBase *UI_iconfile_list(void)
{
  ListBase *list = &(iconfilelist);

  return list;
}

void UI_icons_free(void)
{
#ifndef WITH_HEADLESS
  free_icons_textures();
  free_iconfile_list(&iconfilelist);
  BKE_icons_free();
#endif
}

void UI_icons_free_drawinfo(void *drawinfo)
{
  DrawInfo *di = drawinfo;

  if (di) {
    if (di->type == ICON_TYPE_BUFFER) {
      if (di->data.buffer.image) {
        if (di->data.buffer.image->rect) {
          MEM_freeN(di->data.buffer.image->rect);
        }
        MEM_freeN(di->data.buffer.image);
      }
    }
    else if (di->type == ICON_TYPE_GEOM) {
      if (di->data.geom.image_cache) {
        IMB_freeImBuf(di->data.geom.image_cache);
      }
    }

    MEM_freeN(di);
  }
}

/**
 * #Icon.data_type and #Icon.obj
 */
static DrawInfo *icon_create_drawinfo(Icon *icon)
{
  int icon_data_type = icon->obj_type;
  DrawInfo *di = NULL;

  di = MEM_callocN(sizeof(DrawInfo), "di_icon");

  if (ELEM(icon_data_type, ICON_DATA_ID, ICON_DATA_PREVIEW)) {
    di->type = ICON_TYPE_PREVIEW;
  }
  else if (icon_data_type == ICON_DATA_GEOM) {
    di->type = ICON_TYPE_GEOM;
  }
  else if (icon_data_type == ICON_DATA_STUDIOLIGHT) {
    di->type = ICON_TYPE_BUFFER;
  }
  else if (icon_data_type == ICON_DATA_GPLAYER) {
    di->type = ICON_TYPE_GPLAYER;
  }
  else {
    BLI_assert(0);
  }

  return di;
}

static DrawInfo *icon_ensure_drawinfo(Icon *icon)
{
  if (icon->drawinfo) {
    return icon->drawinfo;
  }
  DrawInfo *di = icon_create_drawinfo(icon);
  icon->drawinfo = di;
  icon->drawinfo_free = UI_icons_free_drawinfo;
  return di;
}

/* note!, returns unscaled by DPI */
int UI_icon_get_width(int icon_id)
{
  Icon *icon = NULL;
  DrawInfo *di = NULL;

  icon = BKE_icon_get(icon_id);

  if (icon == NULL) {
    if (G.debug & G_DEBUG) {
      printf("%s: Internal error, no icon for icon ID: %d\n", __func__, icon_id);
    }
    return 0;
  }

  di = icon_ensure_drawinfo(icon);
  if (di) {
    return ICON_DEFAULT_WIDTH;
  }

  return 0;
}

int UI_icon_get_height(int icon_id)
{
  Icon *icon = BKE_icon_get(icon_id);
  if (icon == NULL) {
    if (G.debug & G_DEBUG) {
      printf("%s: Internal error, no icon for icon ID: %d\n", __func__, icon_id);
    }
    return 0;
  }

  DrawInfo *di = icon_ensure_drawinfo(icon);
  if (di) {
    return ICON_DEFAULT_HEIGHT;
  }

  return 0;
}

bool UI_icon_get_theme_color(int icon_id, uchar color[4])
{
  Icon *icon = BKE_icon_get(icon_id);
  if (icon == NULL) {
    return false;
  }

  DrawInfo *di = icon_ensure_drawinfo(icon);
  return UI_GetIconThemeColor4ubv(di->data.texture.theme_color, color);
}

void UI_icons_init()
{
#ifndef WITH_HEADLESS
  init_iconfile_list(&iconfilelist);
  UI_icons_reload_internal_textures();
  init_internal_icons();
  init_brush_icons();
  init_event_icons();
#endif
}

/* Render size for preview images and icons
 */
int UI_preview_render_size(enum eIconSizes size)
{
  switch (size) {
    case ICON_SIZE_ICON:
      return ICON_RENDER_DEFAULT_HEIGHT;
    case ICON_SIZE_PREVIEW:
      return PREVIEW_RENDER_DEFAULT_HEIGHT;
    default:
      return 0;
  }
}

/* Create rect for the icon
 */
static void icon_create_rect(struct PreviewImage *prv_img, enum eIconSizes size)
{
  uint render_size = UI_preview_render_size(size);

  if (!prv_img) {
    if (G.debug & G_DEBUG) {
      printf("%s, error: requested preview image does not exist", __func__);
    }
  }
  else if (!prv_img->rect[size]) {
    prv_img->w[size] = render_size;
    prv_img->h[size] = render_size;
    prv_img->flag[size] |= PRV_CHANGED;
    prv_img->changed_timestamp[size] = 0;
    prv_img->rect[size] = MEM_callocN(render_size * render_size * sizeof(uint), "prv_rect");
  }
}

static void ui_id_preview_image_render_size(
    const bContext *C, Scene *scene, ID *id, PreviewImage *pi, int size, const bool use_job);

static void ui_studiolight_icon_job_exec(void *customdata,
                                         short *UNUSED(stop),
                                         short *UNUSED(do_update),
                                         float *UNUSED(progress))
{
  Icon **tmp = (Icon **)customdata;
  Icon *icon = *tmp;
  DrawInfo *di = icon_ensure_drawinfo(icon);
  StudioLight *sl = icon->obj;
  BKE_studiolight_preview(di->data.buffer.image->rect, sl, icon->id_type);
}

static void ui_studiolight_kill_icon_preview_job(wmWindowManager *wm, int icon_id)
{
  Icon *icon = BKE_icon_get(icon_id);
  WM_jobs_kill_type(wm, icon, WM_JOB_TYPE_STUDIOLIGHT);
  icon->obj = NULL;
}

static void ui_studiolight_free_function(StudioLight *sl, void *data)
{
  wmWindowManager *wm = data;

  /* Happens if job was canceled or already finished. */
  if (wm == NULL) {
    return;
  }

  // get icons_id, get icons and kill wm jobs
  if (sl->icon_id_radiance) {
    ui_studiolight_kill_icon_preview_job(wm, sl->icon_id_radiance);
  }
  if (sl->icon_id_irradiance) {
    ui_studiolight_kill_icon_preview_job(wm, sl->icon_id_irradiance);
  }
  if (sl->icon_id_matcap) {
    ui_studiolight_kill_icon_preview_job(wm, sl->icon_id_matcap);
  }
  if (sl->icon_id_matcap_flipped) {
    ui_studiolight_kill_icon_preview_job(wm, sl->icon_id_matcap_flipped);
  }
}

static void ui_studiolight_icon_job_end(void *customdata)
{
  Icon **tmp = (Icon **)customdata;
  Icon *icon = *tmp;
  StudioLight *sl = icon->obj;
  BKE_studiolight_set_free_function(sl, &ui_studiolight_free_function, NULL);
}

void ui_icon_ensure_deferred(const bContext *C, const int icon_id, const bool big)
{
  Icon *icon = BKE_icon_get(icon_id);

  if (icon) {
    DrawInfo *di = icon_ensure_drawinfo(icon);

    if (di) {
      switch (di->type) {
        case ICON_TYPE_PREVIEW: {
          ID *id = (icon->id_type != 0) ? icon->obj : NULL;
          PreviewImage *prv = id ? BKE_previewimg_id_ensure(id) : icon->obj;
          /* Using jobs for screen previews crashes due to offscreen rendering.
           * XXX would be nicer if PreviewImage could store if it supports jobs */
          const bool use_jobs = !id || (GS(id->name) != ID_SCR);

          if (prv) {
            const int size = big ? ICON_SIZE_PREVIEW : ICON_SIZE_ICON;

            if (id || (prv->tag & PRV_TAG_DEFFERED) != 0) {
              ui_id_preview_image_render_size(C, NULL, id, prv, size, use_jobs);
            }
          }
          break;
        }
        case ICON_TYPE_BUFFER: {
          if (icon->obj_type == ICON_DATA_STUDIOLIGHT) {
            if (di->data.buffer.image == NULL) {
              wmWindowManager *wm = CTX_wm_manager(C);
              StudioLight *sl = icon->obj;
              BKE_studiolight_set_free_function(sl, &ui_studiolight_free_function, wm);
              IconImage *img = MEM_mallocN(sizeof(IconImage), __func__);

              img->w = STUDIOLIGHT_ICON_SIZE;
              img->h = STUDIOLIGHT_ICON_SIZE;
              size_t size = STUDIOLIGHT_ICON_SIZE * STUDIOLIGHT_ICON_SIZE * sizeof(uint);
              img->rect = MEM_mallocN(size, __func__);
              memset(img->rect, 0, size);
              di->data.buffer.image = img;

              wmJob *wm_job = WM_jobs_get(
                  wm, CTX_wm_window(C), icon, "StudioLight Icon", 0, WM_JOB_TYPE_STUDIOLIGHT);
              Icon **tmp = MEM_callocN(sizeof(Icon *), __func__);
              *tmp = icon;
              WM_jobs_customdata_set(wm_job, tmp, MEM_freeN);
              WM_jobs_timer(wm_job, 0.01, 0, NC_WINDOW);
              WM_jobs_callbacks(
                  wm_job, ui_studiolight_icon_job_exec, NULL, NULL, ui_studiolight_icon_job_end);
              WM_jobs_start(CTX_wm_manager(C), wm_job);
            }
          }
          break;
        }
      }
    }
  }
}

/* only called when icon has changed */
/* only call with valid pointer from UI_icon_draw */
static void icon_set_image(const bContext *C,
                           Scene *scene,
                           ID *id,
                           PreviewImage *prv_img,
                           enum eIconSizes size,
                           const bool use_job)
{
  if (!prv_img) {
    if (G.debug & G_DEBUG) {
      printf("%s: no preview image for this ID: %s\n", __func__, id->name);
    }
    return;
  }

  if (prv_img->flag[size] & PRV_USER_EDITED) {
    /* user-edited preview, do not auto-update! */
    return;
  }

  icon_create_rect(prv_img, size);

  if (use_job) {
    /* Job (background) version */
    ED_preview_icon_job(C, prv_img, id, prv_img->rect[size], prv_img->w[size], prv_img->h[size]);
  }
  else {
    if (!scene) {
      scene = CTX_data_scene(C);
    }
    /* Immediate version */
    ED_preview_icon_render(
        CTX_data_main(C), scene, id, prv_img->rect[size], prv_img->w[size], prv_img->h[size]);
  }
}

PreviewImage *UI_icon_to_preview(int icon_id)
{
  Icon *icon = BKE_icon_get(icon_id);

  if (icon) {
    DrawInfo *di = (DrawInfo *)icon->drawinfo;
    if (di) {
      if (di->type == ICON_TYPE_PREVIEW) {
        PreviewImage *prv = (icon->id_type != 0) ? BKE_previewimg_id_ensure((ID *)icon->obj) :
                                                   icon->obj;

        if (prv) {
          return BKE_previewimg_copy(prv);
        }
      }
      else if (di->data.buffer.image) {
        ImBuf *bbuf;

        bbuf = IMB_ibImageFromMemory(di->data.buffer.image->datatoc_rect,
                                     di->data.buffer.image->datatoc_size,
                                     IB_rect,
                                     NULL,
                                     __func__);
        if (bbuf) {
          PreviewImage *prv = BKE_previewimg_create();

          prv->rect[0] = bbuf->rect;

          prv->w[0] = bbuf->x;
          prv->h[0] = bbuf->y;

          bbuf->rect = NULL;
          IMB_freeImBuf(bbuf);

          return prv;
        }
      }
    }
  }
  return NULL;
}

static void icon_draw_rect(float x,
                           float y,
                           int w,
                           int h,
                           float UNUSED(aspect),
                           int rw,
                           int rh,
                           uint *rect,
                           float alpha,
                           const float desaturate)
{
  ImBuf *ima = NULL;
  int draw_w = w;
  int draw_h = h;
  int draw_x = x;
  int draw_y = y;

  /* sanity check */
  if (w <= 0 || h <= 0 || w > 2000 || h > 2000) {
    printf("%s: icons are %i x %i pixels?\n", __func__, w, h);
    BLI_assert(!"invalid icon size");
    return;
  }
  /* modulate color */
  float col[4] = {1.0f, 1.0f, 1.0f, alpha};

  /* rect contains image in 'rendersize', we only scale if needed */
  if (rw != w || rh != h) {
    /* preserve aspect ratio and center */
    if (rw > rh) {
      draw_w = w;
      draw_h = (int)(((float)rh / (float)rw) * (float)w);
      draw_y += (h - draw_h) / 2;
    }
    else if (rw < rh) {
      draw_w = (int)(((float)rw / (float)rh) * (float)h);
      draw_h = h;
      draw_x += (w - draw_w) / 2;
    }
    /* if the image is squared, the draw_ initialization values are good */

    /* first allocate imbuf for scaling and copy preview into it */
    ima = IMB_allocImBuf(rw, rh, 32, IB_rect);
    memcpy(ima->rect, rect, rw * rh * sizeof(uint));
    IMB_scaleImBuf(ima, draw_w, draw_h); /* scale it */
    rect = ima->rect;
  }

  /* draw */
  eGPUBuiltinShader shader;
  if (desaturate != 0.0f) {
    shader = GPU_SHADER_2D_IMAGE_DESATURATE_COLOR;
  }
  else {
    shader = GPU_SHADER_2D_IMAGE_COLOR;
  }
  IMMDrawPixelsTexState state = immDrawPixelsTexSetup(shader);

  if (shader == GPU_SHADER_2D_IMAGE_DESATURATE_COLOR) {
    immUniform1f("factor", desaturate);
  }

  immDrawPixelsTex(&state,
                   draw_x,
                   draw_y,
                   draw_w,
                   draw_h,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   GL_NEAREST,
                   rect,
                   1.0f,
                   1.0f,
                   col);

  if (ima) {
    IMB_freeImBuf(ima);
  }
}

/* High enough to make a difference, low enough so that
 * small draws are still efficient with the use of glUniform.
 * NOTE TODO: We could use UBO but we would need some triple
 * buffer system + persistent mapping for this to be more
 * efficient than simple glUniform calls. */
#define ICON_DRAW_CACHE_SIZE 16

typedef struct IconDrawCall {
  rctf pos;
  rctf tex;
  float color[4];
} IconDrawCall;

typedef struct IconTextureDrawCall {
  IconDrawCall drawcall_cache[ICON_DRAW_CACHE_SIZE];
  int calls; /* Number of calls batched together */
} IconTextureDrawCall;

static struct {
  IconTextureDrawCall normal;
  IconTextureDrawCall border;
  bool enabled;
  float mat[4][4];
} g_icon_draw_cache = {{{{{0}}}}};

void UI_icon_draw_cache_begin(void)
{
  BLI_assert(g_icon_draw_cache.enabled == false);
  g_icon_draw_cache.enabled = true;
}

static void icon_draw_cache_texture_flush_ex(GLuint texture,
                                             IconTextureDrawCall *texture_draw_calls)
{
  if (texture_draw_calls->calls == 0) {
    return;
  }

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);

  GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_2D_IMAGE_MULTI_RECT_COLOR);
  GPU_shader_bind(shader);

  int img_loc = GPU_shader_get_uniform_ensure(shader, "image");
  int data_loc = GPU_shader_get_uniform_ensure(shader, "calls_data[0]");

  glUniform1i(img_loc, 0);
  glUniform4fv(data_loc, ICON_DRAW_CACHE_SIZE * 3, (float *)texture_draw_calls->drawcall_cache);

  GPU_draw_primitive(GPU_PRIM_TRIS, 6 * texture_draw_calls->calls);

  glBindTexture(GL_TEXTURE_2D, 0);

  texture_draw_calls->calls = 0;
}

static void icon_draw_cache_flush_ex(bool only_full_caches)
{
  bool should_draw = false;
  if (only_full_caches) {
    should_draw = g_icon_draw_cache.normal.calls == ICON_DRAW_CACHE_SIZE ||
                  g_icon_draw_cache.border.calls == ICON_DRAW_CACHE_SIZE;
  }
  else {
    should_draw = g_icon_draw_cache.normal.calls || g_icon_draw_cache.border.calls;
  }

  if (should_draw) {
    /* We need to flush widget base first to ensure correct ordering. */
    UI_widgetbase_draw_cache_flush();

    GPU_blend_set_func(GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

    if (!only_full_caches || g_icon_draw_cache.normal.calls == ICON_DRAW_CACHE_SIZE) {
      icon_draw_cache_texture_flush_ex(icongltex.id[0], &g_icon_draw_cache.normal);
    }

    if (!only_full_caches || g_icon_draw_cache.border.calls == ICON_DRAW_CACHE_SIZE) {
      icon_draw_cache_texture_flush_ex(icongltex.id[1], &g_icon_draw_cache.border);
    }

    GPU_blend_set_func_separate(
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
  }
}

void UI_icon_draw_cache_end(void)
{
  BLI_assert(g_icon_draw_cache.enabled == true);
  g_icon_draw_cache.enabled = false;

  /* Don't change blend state if it's not needed. */
  if (g_icon_draw_cache.border.calls == 0 && g_icon_draw_cache.normal.calls == 0) {
    return;
  }

  GPU_blend(true);
  icon_draw_cache_flush_ex(false);
  GPU_blend(false);
}

static void icon_draw_texture_cached(float x,
                                     float y,
                                     float w,
                                     float h,
                                     int ix,
                                     int iy,
                                     int UNUSED(iw),
                                     int ih,
                                     float alpha,
                                     const float rgb[3],
                                     bool with_border)
{

  float mvp[4][4];
  GPU_matrix_model_view_projection_get(mvp);

  IconTextureDrawCall *texture_call = with_border ? &g_icon_draw_cache.border :
                                                    &g_icon_draw_cache.normal;

  IconDrawCall *call = &texture_call->drawcall_cache[texture_call->calls];
  texture_call->calls++;

  /* Manual mat4*vec2 */
  call->pos.xmin = x * mvp[0][0] + y * mvp[1][0] + mvp[3][0];
  call->pos.ymin = x * mvp[0][1] + y * mvp[1][1] + mvp[3][1];
  call->pos.xmax = call->pos.xmin + w * mvp[0][0] + h * mvp[1][0];
  call->pos.ymax = call->pos.ymin + w * mvp[0][1] + h * mvp[1][1];

  call->tex.xmin = ix * icongltex.invw;
  call->tex.xmax = (ix + ih) * icongltex.invw;
  call->tex.ymin = iy * icongltex.invh;
  call->tex.ymax = (iy + ih) * icongltex.invh;

  if (rgb) {
    copy_v4_fl4(call->color, rgb[0], rgb[1], rgb[2], alpha);
  }
  else {
    copy_v4_fl(call->color, alpha);
  }

  if (texture_call->calls == ICON_DRAW_CACHE_SIZE) {
    icon_draw_cache_flush_ex(true);
  }
}

static void icon_draw_texture(float x,
                              float y,
                              float w,
                              float h,
                              int ix,
                              int iy,
                              int iw,
                              int ih,
                              float alpha,
                              const float rgb[3],
                              bool with_border)
{
  if (g_icon_draw_cache.enabled) {
    icon_draw_texture_cached(x, y, w, h, ix, iy, iw, ih, alpha, rgb, with_border);
    return;
  }

  /* We need to flush widget base first to ensure correct ordering. */
  UI_widgetbase_draw_cache_flush();

  GPU_blend_set_func(GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  float x1, x2, y1, y2;

  x1 = ix * icongltex.invw;
  x2 = (ix + ih) * icongltex.invw;
  y1 = iy * icongltex.invh;
  y2 = (iy + ih) * icongltex.invh;

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, with_border ? icongltex.id[1] : icongltex.id[0]);

  GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_2D_IMAGE_RECT_COLOR);
  GPU_shader_bind(shader);

  if (rgb) {
    glUniform4f(
        GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_COLOR), rgb[0], rgb[1], rgb[2], alpha);
  }
  else {
    glUniform4f(
        GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_COLOR), alpha, alpha, alpha, alpha);
  }

  glUniform1i(GPU_shader_get_uniform_ensure(shader, "image"), 0);
  glUniform4f(GPU_shader_get_uniform_ensure(shader, "rect_icon"), x1, y1, x2, y2);
  glUniform4f(GPU_shader_get_uniform_ensure(shader, "rect_geom"), x, y, x + w, y + h);

  GPU_draw_primitive(GPU_PRIM_TRI_STRIP, 4);

  glBindTexture(GL_TEXTURE_2D, 0);

  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
}

/* Drawing size for preview images */
static int get_draw_size(enum eIconSizes size)
{
  switch (size) {
    case ICON_SIZE_ICON:
      return ICON_DEFAULT_HEIGHT;
    case ICON_SIZE_PREVIEW:
      return PREVIEW_DEFAULT_HEIGHT;
    default:
      return 0;
  }
}

static void icon_draw_size(float x,
                           float y,
                           int icon_id,
                           float aspect,
                           float alpha,
                           enum eIconSizes size,
                           int draw_size,
                           const float desaturate,
                           const char mono_rgba[4],
                           const bool mono_border)
{
  bTheme *btheme = UI_GetTheme();
  Icon *icon = NULL;
  IconImage *iimg;
  const float fdraw_size = (float)draw_size;
  int w, h;

  icon = BKE_icon_get(icon_id);
  alpha *= btheme->tui.icon_alpha;

  if (icon == NULL) {
    if (G.debug & G_DEBUG) {
      printf("%s: Internal error, no icon for icon ID: %d\n", __func__, icon_id);
    }
    return;
  }

  /* scale width and height according to aspect */
  w = (int)(fdraw_size / aspect + 0.5f);
  h = (int)(fdraw_size / aspect + 0.5f);

  DrawInfo *di = icon_ensure_drawinfo(icon);

  /* We need to flush widget base first to ensure correct ordering. */
  UI_widgetbase_draw_cache_flush();

  if (di->type == ICON_TYPE_VECTOR) {
    /* vector icons use the uiBlock transformation, they are not drawn
     * with untransformed coordinates like the other icons */
    di->data.vector.func((int)x, (int)y, w, h, 1.0f);
  }
  else if (di->type == ICON_TYPE_GEOM) {
#ifdef USE_UI_TOOLBAR_HACK
    /* TODO(campbell): scale icons up for toolbar,
     * we need a way to detect larger buttons and do this automatic. */
    {
      float scale = (float)ICON_DEFAULT_HEIGHT_TOOLBAR / (float)ICON_DEFAULT_HEIGHT;
      y = (y + (h / 2)) - ((h * scale) / 2);
      w *= scale;
      h *= scale;
    }
#endif

    /* This could re-generate often if rendered at different sizes in the one interface.
     * TODO(campbell): support caching multiple sizes. */
    ImBuf *ibuf = di->data.geom.image_cache;
    if ((ibuf == NULL) || (ibuf->x != w) || (ibuf->y != h)) {
      if (ibuf) {
        IMB_freeImBuf(ibuf);
      }
      ibuf = BKE_icon_geom_rasterize(icon->obj, w, h);
      di->data.geom.image_cache = ibuf;
    }

    GPU_blend_set_func_separate(
        GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    icon_draw_rect(x, y, w, h, aspect, w, h, ibuf->rect, alpha, desaturate);
    GPU_blend_set_func_separate(
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
  }
  else if (di->type == ICON_TYPE_EVENT) {
    const short event_type = di->data.input.event_type;
    const short event_value = di->data.input.event_value;
    icon_draw_rect_input(x, y, w, h, alpha, event_type, event_value);
  }
  else if (di->type == ICON_TYPE_COLOR_TEXTURE) {
    /* texture image use premul alpha for correct scaling */
    icon_draw_texture(x,
                      y,
                      (float)w,
                      (float)h,
                      di->data.texture.x,
                      di->data.texture.y,
                      di->data.texture.w,
                      di->data.texture.h,
                      alpha,
                      NULL,
                      false);
  }
  else if (di->type == ICON_TYPE_MONO_TEXTURE) {
    /* Monochrome icon that uses text or theme color. */
    bool with_border = mono_border && (btheme->tui.icon_border_intensity > 0.0f);
    float color[4];
    if (mono_rgba) {
      rgba_uchar_to_float(color, (const uchar *)mono_rgba);
    }
    else {
      UI_GetThemeColor4fv(TH_TEXT, color);
    }

    mul_v4_fl(color, alpha);

    float border_outset = 0.0;
    unsigned int border_texel = 0;
    if (with_border) {
      const float scale = (float)ICON_GRID_W / (float)ICON_DEFAULT_WIDTH;
      border_texel = ICON_MONO_BORDER_OUTSET;
      border_outset = ICON_MONO_BORDER_OUTSET / (scale * aspect);
    }
    icon_draw_texture(x - border_outset,
                      y - border_outset,
                      (float)w + 2 * border_outset,
                      (float)h + 2 * border_outset,
                      di->data.texture.x - border_texel,
                      di->data.texture.y - border_texel,
                      di->data.texture.w + 2 * border_texel,
                      di->data.texture.h + 2 * border_texel,
                      color[3],
                      color,
                      with_border);
  }

  else if (di->type == ICON_TYPE_BUFFER) {
    /* it is a builtin icon */
    iimg = di->data.buffer.image;
#ifndef WITH_HEADLESS
    icon_verify_datatoc(iimg);
#endif
    if (!iimg->rect) {
      /* something has gone wrong! */
      return;
    }

    icon_draw_rect(x, y, w, h, aspect, iimg->w, iimg->h, iimg->rect, alpha, desaturate);
  }
  else if (di->type == ICON_TYPE_PREVIEW) {
    PreviewImage *pi = (icon->id_type != 0) ? BKE_previewimg_id_ensure((ID *)icon->obj) :
                                              icon->obj;

    if (pi) {
      /* no create icon on this level in code */
      if (!pi->rect[size]) {
        /* Something has gone wrong! */
        return;
      }

      /* Preview images use premultiplied alpha. */
      GPU_blend_set_func_separate(
          GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
      icon_draw_rect(
          x, y, w, h, aspect, pi->w[size], pi->h[size], pi->rect[size], alpha, desaturate);
      GPU_blend_set_func_separate(
          GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    }
  }
  else if (di->type == ICON_TYPE_GPLAYER) {
    BLI_assert(icon->obj != NULL);

    /* Just draw a colored rect - Like for vicon_colorset_draw() */
#ifndef WITH_HEADLESS
    vicon_gplayer_color_draw(icon, (int)x, (int)y, w, h);
#endif
  }
}

static void ui_id_preview_image_render_size(
    const bContext *C, Scene *scene, ID *id, PreviewImage *pi, int size, const bool use_job)
{
  /* changed only ever set by dynamic icons */
  if (((pi->flag[size] & PRV_CHANGED) || !pi->rect[size])) {
    /* create the rect if necessary */
    icon_set_image(C, scene, id, pi, size, use_job);

    pi->flag[size] &= ~PRV_CHANGED;
  }
}

void UI_id_icon_render(const bContext *C, Scene *scene, ID *id, const bool big, const bool use_job)
{
  PreviewImage *pi = BKE_previewimg_id_ensure(id);

  if (pi) {
    if (big) {
      /* bigger preview size */
      ui_id_preview_image_render_size(C, scene, id, pi, ICON_SIZE_PREVIEW, use_job);
    }
    else {
      /* icon size */
      ui_id_preview_image_render_size(C, scene, id, pi, ICON_SIZE_ICON, use_job);
    }
  }
}

static void ui_id_icon_render(const bContext *C, ID *id, bool use_jobs)
{
  PreviewImage *pi = BKE_previewimg_id_ensure(id);
  enum eIconSizes i;

  if (!pi) {
    return;
  }

  for (i = 0; i < NUM_ICON_SIZES; i++) {
    /* check if rect needs to be created; changed
     * only set by dynamic icons */
    if (((pi->flag[i] & PRV_CHANGED) || !pi->rect[i])) {
      icon_set_image(C, NULL, id, pi, i, use_jobs);
      pi->flag[i] &= ~PRV_CHANGED;
    }
  }
}

static int ui_id_brush_get_icon(const bContext *C, ID *id)
{
  Brush *br = (Brush *)id;

  if (br->flag & BRUSH_CUSTOM_ICON) {
    BKE_icon_id_ensure(id);
    ui_id_icon_render(C, id, true);
  }
  else {
    Object *ob = CTX_data_active_object(C);
    const EnumPropertyItem *items = NULL;
    ePaintMode paint_mode = PAINT_MODE_INVALID;
    ScrArea *sa = CTX_wm_area(C);
    char space_type = sa->spacetype;
    /* Fallback to 3D view. */
    if (space_type == SPACE_PROPERTIES) {
      space_type = SPACE_VIEW3D;
    }

    /* XXX: this is not nice, should probably make brushes
     * be strictly in one paint mode only to avoid
     * checking various context stuff here */

    if ((space_type == SPACE_VIEW3D) && ob) {
      if (ob->mode & OB_MODE_SCULPT) {
        paint_mode = PAINT_MODE_SCULPT;
      }
      else if (ob->mode & OB_MODE_VERTEX_PAINT) {
        paint_mode = PAINT_MODE_VERTEX;
      }
      else if (ob->mode & OB_MODE_WEIGHT_PAINT) {
        paint_mode = PAINT_MODE_WEIGHT;
      }
      else if (ob->mode & OB_MODE_TEXTURE_PAINT) {
        paint_mode = PAINT_MODE_TEXTURE_3D;
      }
    }
    else if (space_type == SPACE_IMAGE) {
      if (sa->spacetype == space_type) {
        const SpaceImage *sima = sa->spacedata.first;
        if (sima->mode == SI_MODE_PAINT) {
          paint_mode = PAINT_MODE_TEXTURE_2D;
        }
      }
    }

    /* reset the icon */
    if ((ob != NULL) && (ob->mode & OB_MODE_PAINT_GPENCIL) && (br->gpencil_settings != NULL)) {
      switch (br->gpencil_settings->icon_id) {
        case GP_BRUSH_ICON_PENCIL:
          br->id.icon_id = ICON_GPBRUSH_PENCIL;
          break;
        case GP_BRUSH_ICON_PEN:
          br->id.icon_id = ICON_GPBRUSH_PEN;
          break;
        case GP_BRUSH_ICON_INK:
          br->id.icon_id = ICON_GPBRUSH_INK;
          break;
        case GP_BRUSH_ICON_INKNOISE:
          br->id.icon_id = ICON_GPBRUSH_INKNOISE;
          break;
        case GP_BRUSH_ICON_BLOCK:
          br->id.icon_id = ICON_GPBRUSH_BLOCK;
          break;
        case GP_BRUSH_ICON_MARKER:
          br->id.icon_id = ICON_GPBRUSH_MARKER;
          break;
        case GP_BRUSH_ICON_FILL:
          br->id.icon_id = ICON_GPBRUSH_FILL;
          break;
        case GP_BRUSH_ICON_ERASE_SOFT:
          br->id.icon_id = ICON_GPBRUSH_ERASE_SOFT;
          break;
        case GP_BRUSH_ICON_ERASE_HARD:
          br->id.icon_id = ICON_GPBRUSH_ERASE_HARD;
          break;
        case GP_BRUSH_ICON_ERASE_STROKE:
          br->id.icon_id = ICON_GPBRUSH_ERASE_STROKE;
          break;
        default:
          br->id.icon_id = ICON_GPBRUSH_PEN;
          break;
      }
      return id->icon_id;
    }
    else if (paint_mode != PAINT_MODE_INVALID) {
      items = BKE_paint_get_tool_enum_from_paintmode(paint_mode);
      const uint tool_offset = BKE_paint_get_brush_tool_offset_from_paintmode(paint_mode);
      const int tool_type = *(char *)POINTER_OFFSET(br, tool_offset);
      if (!items || !RNA_enum_icon_from_value(items, tool_type, &id->icon_id)) {
        id->icon_id = 0;
      }
    }
    else {
      id->icon_id = 0;
    }
  }

  return id->icon_id;
}

static int ui_id_screen_get_icon(const bContext *C, ID *id)
{
  BKE_icon_id_ensure(id);
  /* Don't use jobs here, offscreen rendering doesn't like this and crashes. */
  ui_id_icon_render(C, id, false);

  return id->icon_id;
}

int ui_id_icon_get(const bContext *C, ID *id, const bool big)
{
  int iconid = 0;

  /* icon */
  switch (GS(id->name)) {
    case ID_BR:
      iconid = ui_id_brush_get_icon(C, id);
      break;
    case ID_MA: /* fall through */
    case ID_TE: /* fall through */
    case ID_IM: /* fall through */
    case ID_WO: /* fall through */
    case ID_LA: /* fall through */
      iconid = BKE_icon_id_ensure(id);
      /* checks if not exists, or changed */
      UI_id_icon_render(C, NULL, id, big, true);
      break;
    case ID_SCR:
      iconid = ui_id_screen_get_icon(C, id);
      break;
    default:
      break;
  }

  return iconid;
}

int UI_rnaptr_icon_get(bContext *C, PointerRNA *ptr, int rnaicon, const bool big)
{
  ID *id = NULL;

  if (!ptr->data) {
    return rnaicon;
  }

  /* try ID, material, texture or dynapaint slot */
  if (RNA_struct_is_ID(ptr->type)) {
    id = ptr->id.data;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_MaterialSlot)) {
    id = RNA_pointer_get(ptr, "material").data;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_TextureSlot)) {
    id = RNA_pointer_get(ptr, "texture").data;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_DynamicPaintSurface)) {
    DynamicPaintSurface *surface = ptr->data;

    if (surface->format == MOD_DPAINT_SURFACE_F_PTEX) {
      return ICON_SHADING_TEXTURE;
    }
    else if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX) {
      return ICON_OUTLINER_DATA_MESH;
    }
    else if (surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ) {
      return ICON_FILE_IMAGE;
    }
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_StudioLight)) {
    StudioLight *sl = ptr->data;
    switch (sl->flag & STUDIOLIGHT_FLAG_ORIENTATIONS) {
      case STUDIOLIGHT_TYPE_STUDIO:
        return sl->icon_id_irradiance;
      case STUDIOLIGHT_TYPE_WORLD:
      default:
        return sl->icon_id_radiance;
      case STUDIOLIGHT_TYPE_MATCAP:
        return sl->icon_id_matcap;
    }
  }

  /* get icon from ID */
  if (id) {
    int icon = ui_id_icon_get(C, id, big);

    return icon ? icon : rnaicon;
  }

  return rnaicon;
}

int UI_idcode_icon_get(const int idcode)
{
  switch (idcode) {
    case ID_AC:
      return ICON_ACTION;
    case ID_AR:
      return ICON_ARMATURE_DATA;
    case ID_BR:
      return ICON_BRUSH_DATA;
    case ID_CA:
      return ICON_CAMERA_DATA;
    case ID_CF:
      return ICON_FILE;
    case ID_CU:
      return ICON_CURVE_DATA;
    case ID_GD:
      return ICON_GREASEPENCIL;
    case ID_GR:
      return ICON_GROUP;
    case ID_IM:
      return ICON_IMAGE_DATA;
    case ID_LA:
      return ICON_LIGHT_DATA;
    case ID_LS:
      return ICON_LINE_DATA;
    case ID_LT:
      return ICON_LATTICE_DATA;
    case ID_MA:
      return ICON_MATERIAL_DATA;
    case ID_MB:
      return ICON_META_DATA;
    case ID_MC:
      return ICON_TRACKER;
    case ID_ME:
      return ICON_MESH_DATA;
    case ID_MSK:
      return ICON_MOD_MASK; /* TODO! this would need its own icon! */
    case ID_NT:
      return ICON_NODETREE;
    case ID_OB:
      return ICON_OBJECT_DATA;
    case ID_PA:
      return ICON_PARTICLE_DATA;
    case ID_PAL:
      return ICON_COLOR; /* TODO! this would need its own icon! */
    case ID_PC:
      return ICON_CURVE_BEZCURVE; /* TODO! this would need its own icon! */
    case ID_LP:
      return ICON_OUTLINER_DATA_LIGHTPROBE;
    case ID_SCE:
      return ICON_SCENE_DATA;
    case ID_SPK:
      return ICON_SPEAKER;
    case ID_SO:
      return ICON_SOUND;
    case ID_TE:
      return ICON_TEXTURE_DATA;
    case ID_TXT:
      return ICON_TEXT;
    case ID_VF:
      return ICON_FONT_DATA;
    case ID_WO:
      return ICON_WORLD_DATA;
    default:
      return ICON_NONE;
  }
}

/* draws icon with dpi scale factor */
void UI_icon_draw(float x, float y, int icon_id)
{
  UI_icon_draw_ex(x, y, icon_id, U.inv_dpi_fac, 1.0f, 0.0f, NULL, false);
}

void UI_icon_draw_alpha(float x, float y, int icon_id, float alpha)
{
  UI_icon_draw_ex(x, y, icon_id, U.inv_dpi_fac, alpha, 0.0f, NULL, false);
}

void UI_icon_draw_preview(float x, float y, int icon_id, float aspect, float alpha, int size)
{
  icon_draw_size(x, y, icon_id, aspect, alpha, ICON_SIZE_PREVIEW, size, false, NULL, false);
}

void UI_icon_draw_ex(float x,
                     float y,
                     int icon_id,
                     float aspect,
                     float alpha,
                     float desaturate,
                     const char mono_color[4],
                     const bool mono_border)
{
  int draw_size = get_draw_size(ICON_SIZE_ICON);
  icon_draw_size(x,
                 y,
                 icon_id,
                 aspect,
                 alpha,
                 ICON_SIZE_ICON,
                 draw_size,
                 desaturate,
                 mono_color,
                 mono_border);
}
