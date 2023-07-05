/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_shader_shared.h"
#include "GPU_state.h"
#include "GPU_texture.h"

#include "BLI_blenlib.h"
#include "BLI_fileops_types.h"
#include "BLI_math_color_blend.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "BKE_appdir.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_paint.h"
#include "BKE_studiolight.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "BIF_glutil.h"

#include "ED_datafiles.h"
#include "ED_keyframes_draw.h"
#include "ED_keyframes_keylist.h"
#include "ED_render.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.hh"

#ifndef WITH_HEADLESS
#  define ICON_GRID_COLS 26
#  define ICON_GRID_ROWS 30

#  define ICON_MONO_BORDER_OUTSET 2
#  define ICON_GRID_MARGIN 10
#  define ICON_GRID_W 32
#  define ICON_GRID_H 32
#endif /* WITH_HEADLESS */

struct IconImage {
  int w;
  int h;
  uint8_t *rect;
  const uchar *datatoc_rect;
  int datatoc_size;
};

using VectorDrawFunc = void (*)(int x, int y, int w, int h, float alpha);

#define ICON_TYPE_PREVIEW 0
#define ICON_TYPE_COLOR_TEXTURE 1
#define ICON_TYPE_MONO_TEXTURE 2
#define ICON_TYPE_BUFFER 3
#define ICON_TYPE_IMBUF 4
#define ICON_TYPE_VECTOR 5
#define ICON_TYPE_GEOM 6
#define ICON_TYPE_EVENT 7 /* draw keymap entries using custom renderer. */
#define ICON_TYPE_GPLAYER 8
#define ICON_TYPE_BLANK 9

struct DrawInfo {
  int type;

  union {
    /* type specific data */
    struct {
      VectorDrawFunc func;
    } vector;
    struct {
      ImBuf *image_cache;
      bool inverted;
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
      DrawInfo *next;
    } input;
  } data;
};

struct IconTexture {
  GPUTexture *tex[2];
  int num_textures;
  int w;
  int h;
  float invw;
  float invh;
};

struct IconType {
  int type;
  int theme_color;
};

/* ******************* STATIC LOCAL VARS ******************* */
/* Static here to cache results of icon directory scan, so it's not
 * scanning the file-system each time the menu is drawn. */
static ListBase iconfilelist = {nullptr, nullptr};
static IconTexture icongltex = {{nullptr, nullptr}, 0, 0, 0, 0.0f, 0.0f};

#ifndef WITH_HEADLESS

static const IconType icontypes[] = {
#  define DEF_ICON(name) {ICON_TYPE_MONO_TEXTURE, 0},
#  define DEF_ICON_SCENE(name) {ICON_TYPE_MONO_TEXTURE, TH_ICON_SCENE},
#  define DEF_ICON_COLLECTION(name) {ICON_TYPE_MONO_TEXTURE, TH_ICON_COLLECTION},
#  define DEF_ICON_OBJECT(name) {ICON_TYPE_MONO_TEXTURE, TH_ICON_OBJECT},
#  define DEF_ICON_OBJECT_DATA(name) {ICON_TYPE_MONO_TEXTURE, TH_ICON_OBJECT_DATA},
#  define DEF_ICON_MODIFIER(name) {ICON_TYPE_MONO_TEXTURE, TH_ICON_MODIFIER},
#  define DEF_ICON_SHADING(name) {ICON_TYPE_MONO_TEXTURE, TH_ICON_SHADING},
#  define DEF_ICON_FOLDER(name) {ICON_TYPE_MONO_TEXTURE, TH_ICON_FOLDER},
#  define DEF_ICON_FUND(name) {ICON_TYPE_MONO_TEXTURE, TH_ICON_FUND},
#  define DEF_ICON_VECTOR(name) {ICON_TYPE_VECTOR, 0},
#  define DEF_ICON_COLOR(name) {ICON_TYPE_COLOR_TEXTURE, 0},
#  define DEF_ICON_BLANK(name) {ICON_TYPE_BLANK, 0},
#  include "UI_icons.h"
};

/* **************************************************** */

static DrawInfo *def_internal_icon(
    ImBuf *bbuf, int icon_id, int xofs, int yofs, int size, int type, int theme_color)
{
  Icon *new_icon = MEM_cnew<Icon>(__func__);

  new_icon->obj = nullptr; /* icon is not for library object */
  new_icon->id_type = 0;

  DrawInfo *di = MEM_cnew<DrawInfo>(__func__);
  di->type = type;

  if (ELEM(type, ICON_TYPE_COLOR_TEXTURE, ICON_TYPE_MONO_TEXTURE)) {
    di->data.texture.theme_color = theme_color;
    di->data.texture.x = xofs;
    di->data.texture.y = yofs;
    di->data.texture.w = size;
    di->data.texture.h = size;
  }
  else if (type == ICON_TYPE_BUFFER) {
    IconImage *iimg = MEM_cnew<IconImage>(__func__);
    iimg->w = size;
    iimg->h = size;

    /* icon buffers can get initialized runtime now, via datatoc */
    if (bbuf) {
      int y, imgsize;

      iimg->rect = static_cast<uint8_t *>(MEM_mallocN(size * size * sizeof(uint), __func__));

      /* Here we store the rect in the icon - same as before */
      if (size == bbuf->x && size == bbuf->y && xofs == 0 && yofs == 0) {
        memcpy(iimg->rect, bbuf->byte_buffer.data, size * size * 4 * sizeof(uint8_t));
      }
      else {
        /* this code assumes square images */
        imgsize = bbuf->x;
        for (y = 0; y < size; y++) {
          memcpy(&iimg->rect[y * size],
                 &bbuf->byte_buffer.data[(y + yofs) * imgsize + xofs],
                 size * 4 * sizeof(uint8_t));
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
  Icon *new_icon = MEM_cnew<Icon>("texicon");

  new_icon->obj = nullptr; /* icon is not for library object */
  new_icon->id_type = 0;

  DrawInfo *di = MEM_cnew<DrawInfo>("drawinfo");
  di->type = ICON_TYPE_VECTOR;
  di->data.vector.func = drawFunc;

  new_icon->drawinfo_free = nullptr;
  new_icon->drawinfo = di;

  BKE_icon_set(icon_id, new_icon);
}

/* Vector Icon Drawing Routines */

/* Utilities */

static void vicon_keytype_draw_wrapper(
    int x, int y, int w, int h, float alpha, short key_type, short handle_type)
{
  /* Initialize dummy theme state for Action Editor - where these colors are defined
   * (since we're doing this off-screen, free from any particular space_id). */
  bThemeState theme_state;

  UI_Theme_Store(&theme_state);
  UI_SetTheme(SPACE_ACTION, RGN_TYPE_WINDOW);

  /* The "x" and "y" given are the bottom-left coordinates of the icon,
   * while the #draw_keyframe_shape() function needs the midpoint for the keyframe. */
  const float xco = x + w / 2 + 0.5f;
  const float yco = y + h / 2 + 0.5f;

  GPUVertFormat *format = immVertexFormat();
  KeyframeShaderBindings sh_bindings;
  sh_bindings.pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  sh_bindings.size_id = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  sh_bindings.color_id = GPU_vertformat_attr_add(
      format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  sh_bindings.outline_color_id = GPU_vertformat_attr_add(
      format, "outlineColor", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  sh_bindings.flags_id = GPU_vertformat_attr_add(format, "flags", GPU_COMP_U32, 1, GPU_FETCH_INT);

  GPU_program_point_size(true);
  immBindBuiltinProgram(GPU_SHADER_KEYFRAME_SHAPE);
  immUniform1f("outline_scale", 1.0f);
  immUniform2f("ViewportSize", -1.0f, -1.0f);
  immBegin(GPU_PRIM_POINTS, 1);

  /* draw keyframe
   * - size: (default icon size == 16, default dopesheet icon size == 10)
   * - sel: true unless in handletype icons (so that "keyframe" state shows the iconic yellow icon)
   */
  const bool sel = (handle_type == KEYFRAME_HANDLE_NONE);

  draw_keyframe_shape(xco,
                      yco,
                      (10.0f / 16.0f) * h,
                      sel,
                      key_type,
                      KEYFRAME_SHAPE_BOTH,
                      alpha,
                      &sh_bindings,
                      handle_type,
                      KEYFRAME_EXTREME_NONE);

  immEnd();
  GPU_program_point_size(false);
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

static void vicon_colorset_draw(int index, int x, int y, int w, int h, float /*alpha*/)
{
  bTheme *btheme = UI_GetTheme();
  const ThemeWireColor *cs = &btheme->tarm[index];

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
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* XXX: Include alpha into this... */
  /* normal */
  immUniformColor3ubv(cs->solid);
  immRecti(pos, x, y, a, y + h);

  /* selected */
  immUniformColor3ubv(cs->select);
  immRecti(pos, a, y, b, y + h);

  /* active */
  immUniformColor3ubv(cs->active);
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

static void vicon_collection_color_draw(
    short color_tag, int x, int y, int w, int /*h*/, float /*alpha*/)
{
  bTheme *btheme = UI_GetTheme();
  const ThemeCollectionColor *collection_color = &btheme->collection_color[color_tag];

  const float aspect = float(ICON_DEFAULT_WIDTH) / float(w);

  UI_icon_draw_ex(x,
                  y,
                  ICON_OUTLINER_COLLECTION,
                  aspect,
                  1.0f,
                  0.0f,
                  collection_color->color,
                  true,
                  UI_NO_ICON_OVERLAY_TEXT);
}

#  define DEF_ICON_COLLECTION_COLOR_DRAW(index, color) \
    static void vicon_collection_color_draw_##index(int x, int y, int w, int h, float alpha) \
    { \
      vicon_collection_color_draw(color, x, y, w, h, alpha); \
    }

DEF_ICON_COLLECTION_COLOR_DRAW(01, COLLECTION_COLOR_01);
DEF_ICON_COLLECTION_COLOR_DRAW(02, COLLECTION_COLOR_02);
DEF_ICON_COLLECTION_COLOR_DRAW(03, COLLECTION_COLOR_03);
DEF_ICON_COLLECTION_COLOR_DRAW(04, COLLECTION_COLOR_04);
DEF_ICON_COLLECTION_COLOR_DRAW(05, COLLECTION_COLOR_05);
DEF_ICON_COLLECTION_COLOR_DRAW(06, COLLECTION_COLOR_06);
DEF_ICON_COLLECTION_COLOR_DRAW(07, COLLECTION_COLOR_07);
DEF_ICON_COLLECTION_COLOR_DRAW(08, COLLECTION_COLOR_08);

#  undef DEF_ICON_COLLECTION_COLOR_DRAW

static void vicon_strip_color_draw(
    short color_tag, int x, int y, int w, int /*h*/, float /*alpha*/)
{
  bTheme *btheme = UI_GetTheme();
  const ThemeStripColor *strip_color = &btheme->strip_color[color_tag];

  const float aspect = float(ICON_DEFAULT_WIDTH) / float(w);

  UI_icon_draw_ex(
      x, y, ICON_SNAP_FACE, aspect, 1.0f, 0.0f, strip_color->color, true, UI_NO_ICON_OVERLAY_TEXT);
}

#  define DEF_ICON_STRIP_COLOR_DRAW(index, color) \
    static void vicon_strip_color_draw_##index(int x, int y, int w, int h, float alpha) \
    { \
      vicon_strip_color_draw(color, x, y, w, h, alpha); \
    }

DEF_ICON_STRIP_COLOR_DRAW(01, SEQUENCE_COLOR_01);
DEF_ICON_STRIP_COLOR_DRAW(02, SEQUENCE_COLOR_02);
DEF_ICON_STRIP_COLOR_DRAW(03, SEQUENCE_COLOR_03);
DEF_ICON_STRIP_COLOR_DRAW(04, SEQUENCE_COLOR_04);
DEF_ICON_STRIP_COLOR_DRAW(05, SEQUENCE_COLOR_05);
DEF_ICON_STRIP_COLOR_DRAW(06, SEQUENCE_COLOR_06);
DEF_ICON_STRIP_COLOR_DRAW(07, SEQUENCE_COLOR_07);
DEF_ICON_STRIP_COLOR_DRAW(08, SEQUENCE_COLOR_08);
DEF_ICON_STRIP_COLOR_DRAW(09, SEQUENCE_COLOR_09);

#  undef DEF_ICON_STRIP_COLOR_DRAW

#  define ICON_INDIRECT_DATA_ALPHA 0.6f

static void vicon_strip_color_draw_library_data_indirect(
    int x, int y, int w, int /*h*/, float alpha)
{
  const float aspect = float(ICON_DEFAULT_WIDTH) / float(w);

  UI_icon_draw_ex(x,
                  y,
                  ICON_LIBRARY_DATA_DIRECT,
                  aspect,
                  ICON_INDIRECT_DATA_ALPHA * alpha,
                  0.0f,
                  nullptr,
                  false,
                  UI_NO_ICON_OVERLAY_TEXT);
}

static void vicon_strip_color_draw_library_data_override_noneditable(
    int x, int y, int w, int /*h*/, float alpha)
{
  const float aspect = float(ICON_DEFAULT_WIDTH) / float(w);

  UI_icon_draw_ex(x,
                  y,
                  ICON_LIBRARY_DATA_OVERRIDE,
                  aspect,
                  ICON_INDIRECT_DATA_ALPHA * alpha * 0.75f,
                  0.0f,
                  nullptr,
                  false,
                  UI_NO_ICON_OVERLAY_TEXT);
}

/* Dynamically render icon instead of rendering a plain color to a texture/buffer
 * This is not strictly a "vicon", as it needs access to icon->obj to get the color info,
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
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformColor3fv(gpl->color);
  immRecti(pos, x, y, x + w - 1, y + h - 1);

  immUnbindProgram();
}

static void init_brush_icons()
{

#  define INIT_BRUSH_ICON(icon_id, name) \
    { \
      uchar *rect = (uchar *)datatoc_##name##_png; \
      const int size = datatoc_##name##_png_size; \
      DrawInfo *di; \
\
      di = def_internal_icon(nullptr, icon_id, 0, 0, w, ICON_TYPE_BUFFER, 0); \
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
  INIT_BRUSH_ICON(ICON_BRUSH_PAINT_SELECT, paint_select);
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
  INIT_BRUSH_ICON(ICON_GPBRUSH_AIRBRUSH, gp_brush_airbrush);
  INIT_BRUSH_ICON(ICON_GPBRUSH_CHISEL, gp_brush_chisel);
  INIT_BRUSH_ICON(ICON_GPBRUSH_ERASE_SOFT, gp_brush_erase_soft);
  INIT_BRUSH_ICON(ICON_GPBRUSH_ERASE_HARD, gp_brush_erase_hard);
  INIT_BRUSH_ICON(ICON_GPBRUSH_ERASE_STROKE, gp_brush_erase_stroke);

  /* Curves sculpt. */
  INIT_BRUSH_ICON(ICON_BRUSH_CURVES_ADD, curves_sculpt_add);
  INIT_BRUSH_ICON(ICON_BRUSH_CURVES_COMB, curves_sculpt_comb);
  INIT_BRUSH_ICON(ICON_BRUSH_CURVES_CUT, curves_sculpt_cut);
  INIT_BRUSH_ICON(ICON_BRUSH_CURVES_DELETE, curves_sculpt_delete);
  INIT_BRUSH_ICON(ICON_BRUSH_CURVES_DENSITY, curves_sculpt_density);
  INIT_BRUSH_ICON(ICON_BRUSH_CURVES_GROW_SHRINK, curves_sculpt_grow_shrink);
  INIT_BRUSH_ICON(ICON_BRUSH_CURVES_PINCH, curves_sculpt_pinch);
  INIT_BRUSH_ICON(ICON_BRUSH_CURVES_PUFF, curves_sculpt_puff);
  INIT_BRUSH_ICON(ICON_BRUSH_CURVES_SLIDE, curves_sculpt_slide);
  INIT_BRUSH_ICON(ICON_BRUSH_CURVES_SMOOTH, curves_sculpt_smooth);
  INIT_BRUSH_ICON(ICON_BRUSH_CURVES_SNAKE_HOOK, curves_sculpt_snake_hook);

#  undef INIT_BRUSH_ICON
}

static DrawInfo *g_di_event_list = nullptr;

int UI_icon_from_event_type(short event_type, short event_value)
{
  if (event_type == EVT_RIGHTSHIFTKEY) {
    event_type = EVT_LEFTSHIFTKEY;
  }
  else if (event_type == EVT_RIGHTCTRLKEY) {
    event_type = EVT_LEFTCTRLKEY;
  }
  else if (event_type == EVT_RIGHTALTKEY) {
    event_type = EVT_LEFTALTKEY;
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
  if (event_type == MIDDLEMOUSE) {
    return ELEM(event_value, KM_CLICK, KM_PRESS) ? ICON_MOUSE_MMB : ICON_MOUSE_MMB_DRAG;
  }
  if (event_type == RIGHTMOUSE) {
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

static void init_event_icons()
{
  DrawInfo *di_next = nullptr;

#  define INIT_EVENT_ICON(icon_id, type, value) \
    { \
      DrawInfo *di = def_internal_icon(nullptr, icon_id, 0, 0, w, ICON_TYPE_EVENT, 0); \
      di->data.input.event_type = type; \
      di->data.input.event_value = value; \
      di->data.input.icon = icon_id; \
      di->data.input.next = di_next; \
      di_next = di; \
    } \
    ((void)0)
  /* end INIT_EVENT_ICON */

  const int w = 16; /* DUMMY */

  INIT_EVENT_ICON(ICON_EVENT_A, EVT_AKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_B, EVT_BKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_C, EVT_CKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_D, EVT_DKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_E, EVT_EKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F, EVT_FKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_G, EVT_GKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_H, EVT_HKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_I, EVT_IKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_J, EVT_JKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_K, EVT_KKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_L, EVT_LKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_M, EVT_MKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_N, EVT_NKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_O, EVT_OKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_P, EVT_PKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_Q, EVT_QKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_R, EVT_RKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_S, EVT_SKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_T, EVT_TKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_U, EVT_UKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_V, EVT_VKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_W, EVT_WKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_X, EVT_XKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_Y, EVT_YKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_Z, EVT_ZKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_SHIFT, EVT_LEFTSHIFTKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_CTRL, EVT_LEFTCTRLKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_ALT, EVT_LEFTALTKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_OS, EVT_OSKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F1, EVT_F1KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F2, EVT_F2KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F3, EVT_F3KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F4, EVT_F4KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F5, EVT_F5KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F6, EVT_F6KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F7, EVT_F7KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F8, EVT_F8KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F9, EVT_F9KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F10, EVT_F10KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F11, EVT_F11KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F12, EVT_F12KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_ESC, EVT_ESCKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_TAB, EVT_TABKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAGEUP, EVT_PAGEUPKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAGEDOWN, EVT_PAGEDOWNKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_RETURN, EVT_RETKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_SPACEKEY, EVT_SPACEKEY, KM_ANY);

  g_di_event_list = di_next;

#  undef INIT_EVENT_ICON
}

static void icon_verify_datatoc(IconImage *iimg)
{
  /* if it has own rect, things are all OK */
  if (iimg->rect) {
    return;
  }

  if (iimg->datatoc_rect) {
    ImBuf *bbuf = IMB_ibImageFromMemory(
        iimg->datatoc_rect, iimg->datatoc_size, IB_rect, nullptr, "<matcap icon>");
    /* w and h were set on initialize */
    if (bbuf->x != iimg->h && bbuf->y != iimg->w) {
      IMB_scaleImBuf(bbuf, iimg->w, iimg->h);
    }

    iimg->rect = IMB_steal_byte_buffer(bbuf);
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

  const uint *buf_rect = reinterpret_cast<const uint *>(buf->byte_buffer.data);
  uint *result_rect = reinterpret_cast<uint *>(result->byte_buffer.data);

  for (int y = 0; y < ICON_GRID_ROWS; y++) {
    for (int x = 0; x < ICON_GRID_COLS; x++) {
      const IconType icontype = icontypes[y * ICON_GRID_COLS + x];
      if (icontype.type != ICON_TYPE_MONO_TEXTURE) {
        continue;
      }

      int sx = x * (ICON_GRID_W + ICON_GRID_MARGIN) + ICON_GRID_MARGIN - ICON_MONO_BORDER_OUTSET;
      int sy = y * (ICON_GRID_H + ICON_GRID_MARGIN) + ICON_GRID_MARGIN - ICON_MONO_BORDER_OUTSET;
      sx = sx / resolution_divider;
      sy = sy / resolution_divider;

      /* blur the alpha channel and store it in blurred_alpha_buffer */
      const int blur_size = 2 / resolution_divider;
      for (int bx = 0; bx < icon_width; bx++) {
        const int asx = MAX2(bx - blur_size, 0);
        const int aex = MIN2(bx + blur_size + 1, icon_width);
        for (int by = 0; by < icon_height; by++) {
          const int asy = MAX2(by - blur_size, 0);
          const int aey = MIN2(by + blur_size + 1, icon_height);

          /* blur alpha channel */
          const int write_offset = by * (ICON_GRID_W + 2 * ICON_MONO_BORDER_OUTSET) + bx;
          float alpha_accum = 0.0;
          uint alpha_samples = 0;
          for (int ax = asx; ax < aex; ax++) {
            for (int ay = asy; ay < aey; ay++) {
              const int offset_read = (sy + ay) * buf->x + (sx + ax);
              const uint color_read = buf_rect[offset_read];
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
          const float border_srgb[4] = {
              0, 0, 0, MIN2(1.0f, blurred_alpha * border_sharpness) * border_intensity};

          const uint color_read = buf_rect[offset_write];
          const uchar *orig_color = (uchar *)&color_read;

          float border_rgba[4];
          float orig_rgba[4];
          float dest_rgba[4];
          float dest_srgb[4];

          srgb_to_linearrgb_v4(border_rgba, border_srgb);
          srgb_to_linearrgb_uchar4(orig_rgba, orig_color);
          blend_color_interpolate_float(dest_rgba, orig_rgba, border_rgba, 1.0 - orig_rgba[3]);
          linearrgb_to_srgb_v4(dest_srgb, dest_rgba);

          const uint alpha_mask = uint(dest_srgb[3] * 255) << 24;
          const uint cpack = rgb_to_cpack(dest_srgb[0], dest_srgb[1], dest_srgb[2]) | alpha_mask;

          result_rect[offset_write] = cpack;
        }
      }
    }
  }
  return result;
}

static void free_icons_textures()
{
  if (icongltex.num_textures > 0) {
    for (int i = 0; i < 2; i++) {
      if (icongltex.tex[i]) {
        GPU_texture_free(icongltex.tex[i]);
        icongltex.tex[i] = nullptr;
      }
    }
    icongltex.num_textures = 0;
  }
}

void UI_icons_reload_internal_textures()
{
  bTheme *btheme = UI_GetTheme();
  ImBuf *b16buf = nullptr, *b32buf = nullptr, *b16buf_border = nullptr, *b32buf_border = nullptr;
  const float icon_border_intensity = btheme->tui.icon_border_intensity;
  const bool need_icons_with_border = icon_border_intensity > 0.0f;

  if (b16buf == nullptr) {
    b16buf = IMB_ibImageFromMemory((const uchar *)datatoc_blender_icons16_png,
                                   datatoc_blender_icons16_png_size,
                                   IB_rect,
                                   nullptr,
                                   "<blender icons>");
  }
  if (b16buf) {
    if (need_icons_with_border) {
      b16buf_border = create_mono_icon_with_border(b16buf, 2, icon_border_intensity);
      IMB_premultiply_alpha(b16buf_border);
    }
    IMB_premultiply_alpha(b16buf);
  }

  if (b32buf == nullptr) {
    b32buf = IMB_ibImageFromMemory((const uchar *)datatoc_blender_icons32_png,
                                   datatoc_blender_icons32_png_size,
                                   IB_rect,
                                   nullptr,
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

    /* Note the filter and LOD bias were tweaked to better preserve icon
     * sharpness at different UI scales. */
    if (icongltex.tex[0] == nullptr) {
      icongltex.w = b32buf->x;
      icongltex.h = b32buf->y;
      icongltex.invw = 1.0f / b32buf->x;
      icongltex.invh = 1.0f / b32buf->y;

      icongltex.tex[0] = GPU_texture_create_2d(
          "icons", b32buf->x, b32buf->y, 2, GPU_RGBA8, GPU_TEXTURE_USAGE_SHADER_READ, nullptr);
      GPU_texture_update_mipmap(icongltex.tex[0], 0, GPU_DATA_UBYTE, b32buf->byte_buffer.data);
      GPU_texture_update_mipmap(icongltex.tex[0], 1, GPU_DATA_UBYTE, b16buf->byte_buffer.data);
    }

    if (need_icons_with_border && icongltex.tex[1] == nullptr) {
      icongltex.tex[1] = GPU_texture_create_2d("icons_border",
                                               b32buf_border->x,
                                               b32buf_border->y,
                                               2,
                                               GPU_RGBA8,
                                               GPU_TEXTURE_USAGE_SHADER_READ,
                                               nullptr);
      GPU_texture_update_mipmap(
          icongltex.tex[1], 0, GPU_DATA_UBYTE, b32buf_border->byte_buffer.data);
      GPU_texture_update_mipmap(
          icongltex.tex[1], 1, GPU_DATA_UBYTE, b16buf_border->byte_buffer.data);
    }
  }

  IMB_freeImBuf(b16buf);
  IMB_freeImBuf(b32buf);
  IMB_freeImBuf(b16buf_border);
  IMB_freeImBuf(b32buf_border);
}

static void init_internal_icons()
{
#  if 0 /* temp disabled */
  if ((btheme != nullptr) && btheme->tui.iconfile[0]) {
    char *icondir = BKE_appdir_folder_id(BLENDER_DATAFILES, "icons");
    char iconfilestr[FILE_MAX];

    if (icondir) {
      BLI_path_join(iconfilestr, sizeof(iconfilestr), icondir, btheme->tui.iconfile);

      /* if the image is missing bbuf will just be nullptr */
      bbuf = IMB_loadiffname(iconfilestr, IB_rect, nullptr);

      if (bbuf && (bbuf->x < ICON_IMAGE_W || bbuf->y < ICON_IMAGE_H)) {
        printf(
            "\n***WARNING***\n"
            "Icons file '%s' too small.\n"
            "Using built-in Icons instead\n",
            iconfilestr);
        IMB_freeImBuf(bbuf);
        bbuf = nullptr;
      }
    }
    else {
      printf("%s: 'icons' data path not found, continuing\n", __func__);
    }
  }
#  endif

  /* Define icons. */
  for (int y = 0; y < ICON_GRID_ROWS; y++) {
    /* Row W has monochrome icons. */
    for (int x = 0; x < ICON_GRID_COLS; x++) {
      const IconType icontype = icontypes[y * ICON_GRID_COLS + x];
      if (!ELEM(icontype.type, ICON_TYPE_COLOR_TEXTURE, ICON_TYPE_MONO_TEXTURE)) {
        continue;
      }

      def_internal_icon(nullptr,
                        BIFICONID_FIRST + y * ICON_GRID_COLS + x,
                        x * (ICON_GRID_W + ICON_GRID_MARGIN) + ICON_GRID_MARGIN,
                        y * (ICON_GRID_H + ICON_GRID_MARGIN) + ICON_GRID_MARGIN,
                        ICON_GRID_W,
                        icontype.type,
                        icontype.theme_color);
    }
  }

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

  def_internal_vicon(ICON_COLLECTION_COLOR_01, vicon_collection_color_draw_01);
  def_internal_vicon(ICON_COLLECTION_COLOR_02, vicon_collection_color_draw_02);
  def_internal_vicon(ICON_COLLECTION_COLOR_03, vicon_collection_color_draw_03);
  def_internal_vicon(ICON_COLLECTION_COLOR_04, vicon_collection_color_draw_04);
  def_internal_vicon(ICON_COLLECTION_COLOR_05, vicon_collection_color_draw_05);
  def_internal_vicon(ICON_COLLECTION_COLOR_06, vicon_collection_color_draw_06);
  def_internal_vicon(ICON_COLLECTION_COLOR_07, vicon_collection_color_draw_07);
  def_internal_vicon(ICON_COLLECTION_COLOR_08, vicon_collection_color_draw_08);

  def_internal_vicon(ICON_SEQUENCE_COLOR_01, vicon_strip_color_draw_01);
  def_internal_vicon(ICON_SEQUENCE_COLOR_02, vicon_strip_color_draw_02);
  def_internal_vicon(ICON_SEQUENCE_COLOR_03, vicon_strip_color_draw_03);
  def_internal_vicon(ICON_SEQUENCE_COLOR_04, vicon_strip_color_draw_04);
  def_internal_vicon(ICON_SEQUENCE_COLOR_05, vicon_strip_color_draw_05);
  def_internal_vicon(ICON_SEQUENCE_COLOR_06, vicon_strip_color_draw_06);
  def_internal_vicon(ICON_SEQUENCE_COLOR_07, vicon_strip_color_draw_07);
  def_internal_vicon(ICON_SEQUENCE_COLOR_08, vicon_strip_color_draw_08);
  def_internal_vicon(ICON_SEQUENCE_COLOR_09, vicon_strip_color_draw_09);

  def_internal_vicon(ICON_LIBRARY_DATA_INDIRECT, vicon_strip_color_draw_library_data_indirect);
  def_internal_vicon(ICON_LIBRARY_DATA_OVERRIDE_NONEDITABLE,
                     vicon_strip_color_draw_library_data_override_noneditable);
}

static void init_iconfile_list(ListBase *list)
{
  BLI_listbase_clear(list);
  const char *icondir = BKE_appdir_folder_id(BLENDER_DATAFILES, "icons");

  if (icondir == nullptr) {
    return;
  }

  direntry *dir;
  const int totfile = BLI_filelist_dir_contents(icondir, &dir);

  int index = 1;
  for (int i = 0; i < totfile; i++) {
    if (dir[i].type & S_IFREG) {
      const char *filename = dir[i].relname;

      if (BLI_path_extension_check(filename, ".png")) {
        /* loading all icons on file start is overkill & slows startup
         * its possible they change size after blender load anyway. */
#  if 0
        int ifilex, ifiley;
        char iconfilestr[FILE_MAX + 16]; /* allow 256 chars for file+dir */
        ImBuf *bbuf = nullptr;
        /* check to see if the image is the right size, continue if not */
        /* copying strings here should go ok, assuming that we never get back
         * a complete path to file longer than 256 chars */
        BLI_path_join(iconfilestr, sizeof(iconfilestr), icondir, filename);
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
        IconFile *ifile = MEM_cnew<IconFile>(__func__);

        STRNCPY(ifile->filename, filename);
        ifile->index = index;

        BLI_addtail(list, ifile);

        index++;
      }
    }
  }

  BLI_filelist_free(dir, totfile);
  dir = nullptr;
}

static void free_iconfile_list(ListBase *list)
{
  LISTBASE_FOREACH_MUTABLE (IconFile *, ifile, &iconfilelist) {
    BLI_freelinkN(list, ifile);
  }
}

#else

void UI_icons_reload_internal_textures() {}

#endif /* WITH_HEADLESS */

int UI_iconfile_get_index(const char *filename)
{
  LISTBASE_FOREACH (const IconFile *, ifile, &iconfilelist) {
    if (BLI_path_cmp(filename, ifile->filename) == 0) {
      return ifile->index;
    }
  }

  return 0;
}

ListBase *UI_iconfile_list()
{
  ListBase *list = &(iconfilelist);

  return list;
}

void UI_icons_free()
{
#ifndef WITH_HEADLESS
  free_icons_textures();
  free_iconfile_list(&iconfilelist);
#endif
  BKE_icons_free();
}

void UI_icons_free_drawinfo(void *drawinfo)
{
  DrawInfo *di = static_cast<DrawInfo *>(drawinfo);

  if (di == nullptr) {
    return;
  }

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

/**
 * #Icon.data_type and #Icon.obj
 */
static DrawInfo *icon_create_drawinfo(Icon *icon)
{
  const int icon_data_type = icon->obj_type;

  DrawInfo *di = MEM_cnew<DrawInfo>("di_icon");

  if (ELEM(icon_data_type, ICON_DATA_ID, ICON_DATA_PREVIEW)) {
    di->type = ICON_TYPE_PREVIEW;
  }
  else if (icon_data_type == ICON_DATA_IMBUF) {
    di->type = ICON_TYPE_IMBUF;
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
    return static_cast<DrawInfo *>(icon->drawinfo);
  }
  DrawInfo *di = icon_create_drawinfo(icon);
  icon->drawinfo = di;
  icon->drawinfo_free = UI_icons_free_drawinfo;
  return di;
}

int UI_icon_get_width(int icon_id)
{
  Icon *icon = BKE_icon_get(icon_id);

  if (icon == nullptr) {
    if (G.debug & G_DEBUG) {
      printf("%s: Internal error, no icon for icon ID: %d\n", __func__, icon_id);
    }
    return 0;
  }

  DrawInfo *di = icon_ensure_drawinfo(icon);
  if (di) {
    return ICON_DEFAULT_WIDTH;
  }

  return 0;
}

int UI_icon_get_height(int icon_id)
{
  Icon *icon = BKE_icon_get(icon_id);
  if (icon == nullptr) {
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
  if (icon == nullptr) {
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

int UI_icon_preview_to_render_size(enum eIconSizes size)
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
static void icon_create_rect(PreviewImage *prv_img, enum eIconSizes size)
{
  const uint render_size = UI_icon_preview_to_render_size(size);

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
    prv_img->rect[size] = static_cast<uint *>(
        MEM_callocN(render_size * render_size * sizeof(uint), "prv_rect"));
  }
}

static void ui_id_preview_image_render_size(
    const bContext *C, Scene *scene, ID *id, PreviewImage *pi, int size, const bool use_job);

static void ui_studiolight_icon_job_exec(void *customdata,
                                         bool * /*stop*/,
                                         bool * /*do_update*/,
                                         float * /*progress*/)
{
  Icon **tmp = (Icon **)customdata;
  Icon *icon = *tmp;
  DrawInfo *di = icon_ensure_drawinfo(icon);
  StudioLight *sl = static_cast<StudioLight *>(icon->obj);
  BKE_studiolight_preview(
      reinterpret_cast<uint *>(di->data.buffer.image->rect), sl, icon->id_type);
}

static void ui_studiolight_kill_icon_preview_job(wmWindowManager *wm, int icon_id)
{
  Icon *icon = BKE_icon_get(icon_id);
  WM_jobs_kill_type(wm, icon, WM_JOB_TYPE_STUDIOLIGHT);
  icon->obj = nullptr;
}

static void ui_studiolight_free_function(StudioLight *sl, void *data)
{
  wmWindowManager *wm = static_cast<wmWindowManager *>(data);

  /* Happens if job was canceled or already finished. */
  if (wm == nullptr) {
    return;
  }

  /* get icons_id, get icons and kill wm jobs */
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
  StudioLight *sl = static_cast<StudioLight *>(icon->obj);
  BKE_studiolight_set_free_function(sl, &ui_studiolight_free_function, nullptr);
}

void ui_icon_ensure_deferred(const bContext *C, const int icon_id, const bool big)
{
  Icon *icon = BKE_icon_get(icon_id);

  if (icon == nullptr) {
    return;
  }

  DrawInfo *di = icon_ensure_drawinfo(icon);

  if (di == nullptr) {
    return;
  }

  switch (di->type) {
    case ICON_TYPE_PREVIEW: {
      ID *id = (icon->id_type != 0) ? static_cast<ID *>(icon->obj) : nullptr;
      PreviewImage *prv = id ? BKE_previewimg_id_ensure(id) :
                               static_cast<PreviewImage *>(icon->obj);
      /* Using jobs for screen previews crashes due to off-screen rendering.
       * XXX: would be nicer if #PreviewImage could store if it supports jobs. */
      const bool use_jobs = !id || (GS(id->name) != ID_SCR);

      if (prv) {
        const int size = big ? ICON_SIZE_PREVIEW : ICON_SIZE_ICON;

        if (id || (prv->tag & PRV_TAG_DEFFERED) != 0) {
          ui_id_preview_image_render_size(C, nullptr, id, prv, size, use_jobs);
        }
      }
      break;
    }
    case ICON_TYPE_BUFFER: {
      if (icon->obj_type == ICON_DATA_STUDIOLIGHT) {
        if (di->data.buffer.image == nullptr) {
          wmWindowManager *wm = CTX_wm_manager(C);
          StudioLight *sl = static_cast<StudioLight *>(icon->obj);
          BKE_studiolight_set_free_function(sl, &ui_studiolight_free_function, wm);
          IconImage *img = MEM_cnew<IconImage>(__func__);

          img->w = STUDIOLIGHT_ICON_SIZE;
          img->h = STUDIOLIGHT_ICON_SIZE;
          const size_t size = STUDIOLIGHT_ICON_SIZE * STUDIOLIGHT_ICON_SIZE * sizeof(uint);
          img->rect = static_cast<uint8_t *>(MEM_mallocN(size, __func__));
          memset(img->rect, 0, size);
          di->data.buffer.image = img;

          wmJob *wm_job = WM_jobs_get(wm,
                                      CTX_wm_window(C),
                                      icon,
                                      "StudioLight Icon",
                                      eWM_JobFlag(0),
                                      WM_JOB_TYPE_STUDIOLIGHT);
          Icon **tmp = MEM_cnew<Icon *>(__func__);
          *tmp = icon;
          WM_jobs_customdata_set(wm_job, tmp, MEM_freeN);
          WM_jobs_timer(wm_job, 0.01, 0, NC_WINDOW);
          WM_jobs_callbacks(
              wm_job, ui_studiolight_icon_job_exec, nullptr, nullptr, ui_studiolight_icon_job_end);
          WM_jobs_start(CTX_wm_manager(C), wm_job);
        }
      }
      break;
    }
  }
}

/**
 * * Only call with valid pointer from UI_icon_draw.
 * * Only called when icon has changed.
 *
 * Note that if an ID doesn't support jobs for preview creation, \a use_job will be ignored.
 */
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

  const bool delay = prv_img->rect[size] != nullptr;
  icon_create_rect(prv_img, size);

  if (use_job && (!id || BKE_previewimg_id_supports_jobs(id))) {
    /* Job (background) version */
    ED_preview_icon_job(C, prv_img, id, size, delay);
  }
  else {
    if (!scene) {
      scene = CTX_data_scene(C);
    }
    /* Immediate version */
    ED_preview_icon_render(C, scene, prv_img, id, size);
  }
}

PreviewImage *UI_icon_to_preview(int icon_id)
{
  Icon *icon = BKE_icon_get(icon_id);

  if (icon == nullptr) {
    return nullptr;
  }

  DrawInfo *di = (DrawInfo *)icon->drawinfo;

  if (di == nullptr) {
    return nullptr;
  }

  if (di->type == ICON_TYPE_PREVIEW) {
    PreviewImage *prv = (icon->id_type != 0) ? BKE_previewimg_id_ensure((ID *)icon->obj) :
                                               static_cast<PreviewImage *>(icon->obj);

    if (prv) {
      return BKE_previewimg_copy(prv);
    }
  }
  else if (di->data.buffer.image) {
    ImBuf *bbuf;

    bbuf = IMB_ibImageFromMemory(di->data.buffer.image->datatoc_rect,
                                 di->data.buffer.image->datatoc_size,
                                 IB_rect,
                                 nullptr,
                                 __func__);
    if (bbuf) {
      PreviewImage *prv = BKE_previewimg_create();

      prv->rect[0] = reinterpret_cast<uint *>(IMB_steal_byte_buffer(bbuf));

      prv->w[0] = bbuf->x;
      prv->h[0] = bbuf->y;

      IMB_freeImBuf(bbuf);

      return prv;
    }
  }

  return nullptr;
}

static void icon_draw_rect(float x,
                           float y,
                           int w,
                           int h,
                           float /*aspect*/,
                           int rw,
                           int rh,
                           uint8_t *rect,
                           float alpha,
                           const float desaturate)
{
  int draw_w = w;
  int draw_h = h;
  int draw_x = x;
  /* We need to round y, to avoid the icon jittering in some cases. */
  int draw_y = round_fl_to_int(y);

  /* sanity check */
  if (w <= 0 || h <= 0 || w > 2000 || h > 2000) {
    printf("%s: icons are %i x %i pixels?\n", __func__, w, h);
    BLI_assert_msg(0, "invalid icon size");
    return;
  }
  /* modulate color */
  const float col[4] = {alpha, alpha, alpha, alpha};

  float scale_x = 1.0f;
  float scale_y = 1.0f;
  /* rect contains image in 'rendersize', we only scale if needed */
  if (rw != w || rh != h) {
    /* preserve aspect ratio and center */
    if (rw > rh) {
      draw_w = w;
      draw_h = int((float(rh) / float(rw)) * float(w));
      draw_y += (h - draw_h) / 2;
    }
    else if (rw < rh) {
      draw_w = int((float(rw) / float(rh)) * float(h));
      draw_h = h;
      draw_x += (w - draw_w) / 2;
    }
    scale_x = draw_w / float(rw);
    scale_y = draw_h / float(rh);
    /* If the image is squared, the `draw_*` initialization values are good. */
  }

  /* draw */
  eGPUBuiltinShader shader;
  if (desaturate != 0.0f) {
    shader = GPU_SHADER_2D_IMAGE_DESATURATE_COLOR;
  }
  else {
    shader = GPU_SHADER_3D_IMAGE_COLOR;
  }
  IMMDrawPixelsTexState state = immDrawPixelsTexSetup(shader);

  if (shader == GPU_SHADER_2D_IMAGE_DESATURATE_COLOR) {
    immUniform1f("factor", desaturate);
  }

  immDrawPixelsTexScaledFullSize(
      &state, draw_x, draw_y, rw, rh, GPU_RGBA8, true, rect, scale_x, scale_y, 1.0f, 1.0f, col);
}

/* High enough to make a difference, low enough so that
 * small draws are still efficient with the use of glUniform.
 * NOTE TODO: We could use UBO but we would need some triple
 * buffer system + persistent mapping for this to be more
 * efficient than simple glUniform calls. */
#define ICON_DRAW_CACHE_SIZE 16

struct IconDrawCall {
  rctf pos;
  rctf tex;
  float color[4];
};

struct IconTextureDrawCall {
  IconDrawCall drawcall_cache[ICON_DRAW_CACHE_SIZE];
  int calls; /* Number of calls batched together */
};

static struct {
  IconTextureDrawCall normal;
  IconTextureDrawCall border;
  bool enabled;
} g_icon_draw_cache = {{{{{0}}}}};

void UI_icon_draw_cache_begin()
{
  BLI_assert(g_icon_draw_cache.enabled == false);
  g_icon_draw_cache.enabled = true;
}

static void icon_draw_cache_texture_flush_ex(GPUTexture *texture,
                                             IconTextureDrawCall *texture_draw_calls)
{
  if (texture_draw_calls->calls == 0) {
    return;
  }

  GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_ICON_MULTI);
  GPU_shader_bind(shader);

  const int data_binding = GPU_shader_get_ubo_binding(shader, "multi_icon_data");
  GPUUniformBuf *ubo = GPU_uniformbuf_create_ex(
      sizeof(MultiIconCallData), texture_draw_calls->drawcall_cache, __func__);
  GPU_uniformbuf_bind(ubo, data_binding);

  const int img_binding = GPU_shader_get_sampler_binding(shader, "image");
  GPU_texture_bind_ex(texture, GPUSamplerState::icon_sampler(), img_binding);

  GPUBatch *quad = GPU_batch_preset_quad();
  GPU_batch_set_shader(quad, shader);
  GPU_batch_draw_instance_range(quad, 0, texture_draw_calls->calls);

  GPU_texture_unbind(texture);
  GPU_uniformbuf_unbind(ubo);
  GPU_uniformbuf_free(ubo);

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

    GPU_blend(GPU_BLEND_ALPHA_PREMULT);

    if (!only_full_caches || g_icon_draw_cache.normal.calls == ICON_DRAW_CACHE_SIZE) {
      icon_draw_cache_texture_flush_ex(icongltex.tex[0], &g_icon_draw_cache.normal);
    }

    if (!only_full_caches || g_icon_draw_cache.border.calls == ICON_DRAW_CACHE_SIZE) {
      icon_draw_cache_texture_flush_ex(icongltex.tex[1], &g_icon_draw_cache.border);
    }

    GPU_blend(GPU_BLEND_ALPHA);
  }
}

void UI_icon_draw_cache_end()
{
  BLI_assert(g_icon_draw_cache.enabled == true);
  g_icon_draw_cache.enabled = false;

  /* Don't change blend state if it's not needed. */
  if (g_icon_draw_cache.border.calls == 0 && g_icon_draw_cache.normal.calls == 0) {
    return;
  }

  GPU_blend(GPU_BLEND_ALPHA);
  icon_draw_cache_flush_ex(false);
  GPU_blend(GPU_BLEND_NONE);
}

static void icon_draw_texture_cached(float x,
                                     float y,
                                     float w,
                                     float h,
                                     int ix,
                                     int iy,
                                     int /*iw*/,
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
                              bool with_border,
                              const IconTextOverlay *text_overlay)
{
  const float zoom_factor = w / UI_ICON_SIZE;
  float text_width = 0.0f;

  /* No need to show if too zoomed out, otherwise it just adds noise. */
  const bool show_indicator = (text_overlay && text_overlay->text[0] != '\0') &&
                              (zoom_factor > 0.7f);

  if (show_indicator) {
    /* Handle the little numbers on top of the icon. */
    uchar text_color[4];
    UI_GetThemeColor3ubv(TH_TEXT, text_color);
    text_color[3] = 255;

    uiFontStyle fstyle_small = *UI_FSTYLE_WIDGET;
    fstyle_small.points *= zoom_factor;
    fstyle_small.points *= 0.8f;

    rcti text_rect{};
    text_rect.xmax = x + UI_UNIT_X * zoom_factor;
    text_rect.xmin = x;
    text_rect.ymax = y;
    text_rect.ymin = y;

    uiFontStyleDraw_Params params{};
    params.align = UI_STYLE_TEXT_RIGHT;
    UI_fontstyle_draw(&fstyle_small,
                      &text_rect,
                      text_overlay->text,
                      sizeof(text_overlay->text),
                      text_color,
                      &params);
    text_width = float(UI_fontstyle_string_width(&fstyle_small, text_overlay->text)) / UI_UNIT_X /
                 zoom_factor;
  }

  /* Draw the actual icon. */
  if (!show_indicator && g_icon_draw_cache.enabled) {
    icon_draw_texture_cached(x, y, w, h, ix, iy, iw, ih, alpha, rgb, with_border);
    return;
  }

  /* We need to flush widget base first to ensure correct ordering. */
  UI_widgetbase_draw_cache_flush();

  GPU_blend(GPU_BLEND_ALPHA_PREMULT);

  const float x1 = ix * icongltex.invw;
  const float x2 = (ix + ih) * icongltex.invw;
  const float y1 = iy * icongltex.invh;
  const float y2 = (iy + ih) * icongltex.invh;

  GPUTexture *texture = with_border ? icongltex.tex[1] : icongltex.tex[0];

  GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_ICON);
  GPU_shader_bind(shader);

  const int img_binding = GPU_shader_get_sampler_binding(shader, "image");
  const int color_loc = GPU_shader_get_uniform(shader, "finalColor");
  const int rect_tex_loc = GPU_shader_get_uniform(shader, "rect_icon");
  const int rect_geom_loc = GPU_shader_get_uniform(shader, "rect_geom");

  if (rgb) {
    const float color[4] = {rgb[0], rgb[1], rgb[2], alpha};
    GPU_shader_uniform_float_ex(shader, color_loc, 4, 1, color);
  }
  else {
    const float color[4] = {alpha, alpha, alpha, alpha};
    GPU_shader_uniform_float_ex(shader, color_loc, 4, 1, color);
  }

  const float tex_color[4] = {x1, y1, x2, y2};
  const float geom_color[4] = {x, y, x + w, y + h};

  GPU_shader_uniform_float_ex(shader, rect_tex_loc, 4, 1, tex_color);
  GPU_shader_uniform_float_ex(shader, rect_geom_loc, 4, 1, geom_color);
  GPU_shader_uniform_1f(shader, "text_width", text_width);

  GPU_texture_bind_ex(texture, GPUSamplerState::icon_sampler(), img_binding);

  GPUBatch *quad = GPU_batch_preset_quad();
  GPU_batch_set_shader(quad, shader);
  GPU_batch_draw(quad);

  GPU_texture_unbind(texture);

  GPU_blend(GPU_BLEND_ALPHA);
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
                           const uchar mono_rgba[4],
                           const bool mono_border,
                           const IconTextOverlay *text_overlay)
{
  bTheme *btheme = UI_GetTheme();
  const float fdraw_size = float(draw_size);

  Icon *icon = BKE_icon_get(icon_id);
  alpha *= btheme->tui.icon_alpha;

  if (icon == nullptr) {
    if (G.debug & G_DEBUG) {
      printf("%s: Internal error, no icon for icon ID: %d\n", __func__, icon_id);
    }
    return;
  }

  /* scale width and height according to aspect */
  int w = int(fdraw_size / aspect + 0.5f);
  int h = int(fdraw_size / aspect + 0.5f);

  DrawInfo *di = icon_ensure_drawinfo(icon);

  /* We need to flush widget base first to ensure correct ordering. */
  UI_widgetbase_draw_cache_flush();

  if (di->type == ICON_TYPE_IMBUF) {
    ImBuf *ibuf = static_cast<ImBuf *>(icon->obj);

    GPU_blend(GPU_BLEND_ALPHA_PREMULT);
    icon_draw_rect(
        x, y, w, h, aspect, ibuf->x, ibuf->y, ibuf->byte_buffer.data, alpha, desaturate);
    GPU_blend(GPU_BLEND_ALPHA);
  }
  else if (di->type == ICON_TYPE_VECTOR) {
    /* vector icons use the uiBlock transformation, they are not drawn
     * with untransformed coordinates like the other icons */
    di->data.vector.func(int(x), int(y), w, h, 1.0f);
  }
  else if (di->type == ICON_TYPE_GEOM) {
#ifdef USE_UI_TOOLBAR_HACK
    /* TODO(@ideasman42): scale icons up for toolbar,
     * we need a way to detect larger buttons and do this automatic. */
    {
      float scale = float(ICON_DEFAULT_HEIGHT_TOOLBAR) / float(ICON_DEFAULT_HEIGHT);
      y = (y + (h / 2)) - ((h * scale) / 2);
      w *= scale;
      h *= scale;
    }
#endif

    /* If the theme is light, we will adjust the icon colors. */
    const bool invert = (rgb_to_grayscale_byte(btheme->tui.wcol_toolbar_item.inner) > 128);
    const bool geom_inverted = di->data.geom.inverted;

    /* This could re-generate often if rendered at different sizes in the one interface.
     * TODO(@ideasman42): support caching multiple sizes. */
    ImBuf *ibuf = di->data.geom.image_cache;
    if ((ibuf == nullptr) || (ibuf->x != w) || (ibuf->y != h) || (invert != geom_inverted)) {
      if (ibuf) {
        IMB_freeImBuf(ibuf);
      }
      if (invert != geom_inverted) {
        BKE_icon_geom_invert_lightness(static_cast<Icon_Geom *>(icon->obj));
      }
      ibuf = BKE_icon_geom_rasterize(static_cast<Icon_Geom *>(icon->obj), w, h);
      di->data.geom.image_cache = ibuf;
      di->data.geom.inverted = invert;
    }

    GPU_blend(GPU_BLEND_ALPHA_PREMULT);
    icon_draw_rect(x, y, w, h, aspect, w, h, ibuf->byte_buffer.data, alpha, desaturate);
    GPU_blend(GPU_BLEND_ALPHA);
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
                      float(w),
                      float(h),
                      di->data.texture.x,
                      di->data.texture.y,
                      di->data.texture.w,
                      di->data.texture.h,
                      alpha,
                      nullptr,
                      false,
                      text_overlay);
  }
  else if (di->type == ICON_TYPE_MONO_TEXTURE) {
    /* Monochrome icon that uses text or theme color. */
    const bool with_border = mono_border && (btheme->tui.icon_border_intensity > 0.0f);
    float color[4];
    if (mono_rgba) {
      rgba_uchar_to_float(color, (const uchar *)mono_rgba);
    }
    else {
      UI_GetThemeColor4fv(TH_TEXT, color);
    }

    mul_v4_fl(color, alpha);

    float border_outset = 0.0;
    uint border_texel = 0;
#ifndef WITH_HEADLESS
    if (with_border) {
      const float scale = float(ICON_GRID_W) / float(ICON_DEFAULT_WIDTH);
      border_texel = ICON_MONO_BORDER_OUTSET;
      border_outset = ICON_MONO_BORDER_OUTSET / (scale * aspect);
    }
#endif
    icon_draw_texture(x - border_outset,
                      y - border_outset,
                      float(w) + 2 * border_outset,
                      float(h) + 2 * border_outset,
                      di->data.texture.x - border_texel,
                      di->data.texture.y - border_texel,
                      di->data.texture.w + 2 * border_texel,
                      di->data.texture.h + 2 * border_texel,
                      color[3],
                      color,
                      with_border,
                      text_overlay);
  }

  else if (di->type == ICON_TYPE_BUFFER) {
    /* it is a builtin icon */
    IconImage *iimg = di->data.buffer.image;
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
                                              static_cast<PreviewImage *>(icon->obj);

    if (pi) {
      /* no create icon on this level in code */
      if (!pi->rect[size]) {
        /* Something has gone wrong! */
        return;
      }

      /* Preview images use premultiplied alpha. */
      GPU_blend(GPU_BLEND_ALPHA_PREMULT);
      icon_draw_rect(x,
                     y,
                     w,
                     h,
                     aspect,
                     pi->w[size],
                     pi->h[size],
                     reinterpret_cast<uint8_t *>(pi->rect[size]),
                     alpha,
                     desaturate);
      GPU_blend(GPU_BLEND_ALPHA);
    }
  }
  else if (di->type == ICON_TYPE_GPLAYER) {
    BLI_assert(icon->obj != nullptr);

    /* Just draw a colored rect - Like for vicon_colorset_draw() */
#ifndef WITH_HEADLESS
    vicon_gplayer_color_draw(icon, int(x), int(y), w, h);
#endif
  }
}

static void ui_id_preview_image_render_size(
    const bContext *C, Scene *scene, ID *id, PreviewImage *pi, int size, const bool use_job)
{
  /* changed only ever set by dynamic icons */
  if ((pi->flag[size] & PRV_CHANGED) || !pi->rect[size]) {
    /* create the rect if necessary */
    icon_set_image(C, scene, id, pi, eIconSizes(size), use_job);

    pi->flag[size] &= ~PRV_CHANGED;
  }
}

void UI_icon_render_id_ex(const bContext *C,
                          Scene *scene,
                          ID *id_to_render,
                          const enum eIconSizes size,
                          const bool use_job,
                          PreviewImage *r_preview_image)
{
  ui_id_preview_image_render_size(C, scene, id_to_render, r_preview_image, size, use_job);
}

void UI_icon_render_id(
    const bContext *C, Scene *scene, ID *id, const enum eIconSizes size, const bool use_job)
{
  PreviewImage *pi = BKE_previewimg_id_ensure(id);
  if (pi == nullptr) {
    return;
  }

  ID *id_to_render = id;

  /* For objects, first try if a preview can created via the object data. */
  if (GS(id->name) == ID_OB) {
    Object *ob = (Object *)id;
    if (ED_preview_id_is_supported(static_cast<const ID *>(ob->data))) {
      id_to_render = static_cast<ID *>(ob->data);
    }
  }

  if (!ED_preview_id_is_supported(id_to_render)) {
    return;
  }

  UI_icon_render_id_ex(C, scene, id_to_render, size, use_job, pi);
}

static void ui_id_icon_render(const bContext *C, ID *id, bool use_jobs)
{
  PreviewImage *pi = BKE_previewimg_id_ensure(id);

  if (!pi) {
    return;
  }

  for (int i = 0; i < NUM_ICON_SIZES; i++) {
    ui_id_preview_image_render_size(C, nullptr, id, pi, i, use_jobs);
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
    const EnumPropertyItem *items = nullptr;
    ePaintMode paint_mode = PAINT_MODE_INVALID;
    ScrArea *area = CTX_wm_area(C);
    char space_type = area->spacetype;
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
      else if (ob->mode & OB_MODE_SCULPT_CURVES) {
        paint_mode = PAINT_MODE_SCULPT_CURVES;
      }
    }
    else if (space_type == SPACE_IMAGE) {
      if (area->spacetype == space_type) {
        const SpaceImage *sima = static_cast<const SpaceImage *>(area->spacedata.first);
        if (sima->mode == SI_MODE_PAINT) {
          paint_mode = PAINT_MODE_TEXTURE_2D;
        }
      }
    }

    /* reset the icon */
    if ((ob != nullptr) && (ob->mode & OB_MODE_ALL_PAINT_GPENCIL) &&
        (br->gpencil_settings != nullptr)) {
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
        case GP_BRUSH_ICON_AIRBRUSH:
          br->id.icon_id = ICON_GPBRUSH_AIRBRUSH;
          break;
        case GP_BRUSH_ICON_CHISEL:
          br->id.icon_id = ICON_GPBRUSH_CHISEL;
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
        case GP_BRUSH_ICON_TINT:
          br->id.icon_id = ICON_BRUSH_TEXDRAW;
          break;
        case GP_BRUSH_ICON_VERTEX_DRAW:
          br->id.icon_id = ICON_BRUSH_MIX;
          break;
        case GP_BRUSH_ICON_VERTEX_BLUR:
          br->id.icon_id = ICON_BRUSH_BLUR;
          break;
        case GP_BRUSH_ICON_VERTEX_AVERAGE:
          br->id.icon_id = ICON_BRUSH_BLUR;
          break;
        case GP_BRUSH_ICON_VERTEX_SMEAR:
          br->id.icon_id = ICON_BRUSH_BLUR;
          break;
        case GP_BRUSH_ICON_VERTEX_REPLACE:
          br->id.icon_id = ICON_BRUSH_MIX;
          break;
        case GP_BRUSH_ICON_GPBRUSH_SMOOTH:
          br->id.icon_id = ICON_GPBRUSH_SMOOTH;
          break;
        case GP_BRUSH_ICON_GPBRUSH_THICKNESS:
          br->id.icon_id = ICON_GPBRUSH_THICKNESS;
          break;
        case GP_BRUSH_ICON_GPBRUSH_STRENGTH:
          br->id.icon_id = ICON_GPBRUSH_STRENGTH;
          break;
        case GP_BRUSH_ICON_GPBRUSH_RANDOMIZE:
          br->id.icon_id = ICON_GPBRUSH_RANDOMIZE;
          break;
        case GP_BRUSH_ICON_GPBRUSH_GRAB:
          br->id.icon_id = ICON_GPBRUSH_GRAB;
          break;
        case GP_BRUSH_ICON_GPBRUSH_PUSH:
          br->id.icon_id = ICON_GPBRUSH_PUSH;
          break;
        case GP_BRUSH_ICON_GPBRUSH_TWIST:
          br->id.icon_id = ICON_GPBRUSH_TWIST;
          break;
        case GP_BRUSH_ICON_GPBRUSH_PINCH:
          br->id.icon_id = ICON_GPBRUSH_PINCH;
          break;
        case GP_BRUSH_ICON_GPBRUSH_CLONE:
          br->id.icon_id = ICON_GPBRUSH_CLONE;
          break;
        case GP_BRUSH_ICON_GPBRUSH_WEIGHT:
          br->id.icon_id = ICON_GPBRUSH_WEIGHT;
          break;
        case GP_BRUSH_ICON_GPBRUSH_BLUR:
          br->id.icon_id = ICON_BRUSH_BLUR;
          break;
        case GP_BRUSH_ICON_GPBRUSH_AVERAGE:
          br->id.icon_id = ICON_BRUSH_BLUR;
          break;
        case GP_BRUSH_ICON_GPBRUSH_SMEAR:
          br->id.icon_id = ICON_BRUSH_BLUR;
          break;
        default:
          br->id.icon_id = ICON_GPBRUSH_PEN;
          break;
      }
      return id->icon_id;
    }

    if (paint_mode != PAINT_MODE_INVALID) {
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
  /* Don't use jobs here, off-screen rendering doesn't like this and crashes. */
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
      UI_icon_render_id(C, nullptr, id, big ? ICON_SIZE_PREVIEW : ICON_SIZE_ICON, true);
      break;
    case ID_SCR:
      iconid = ui_id_screen_get_icon(C, id);
      break;
    case ID_GR:
      iconid = UI_icon_color_from_collection((Collection *)id);
      break;
    default:
      break;
  }

  return iconid;
}

int UI_icon_from_library(const ID *id)
{
  if (ID_IS_LINKED(id)) {
    if (id->tag & LIB_TAG_MISSING) {
      return ICON_LIBRARY_DATA_BROKEN;
    }
    if (id->tag & LIB_TAG_INDIRECT) {
      return ICON_LIBRARY_DATA_INDIRECT;
    }
    return ICON_LIBRARY_DATA_DIRECT;
  }
  if (ID_IS_OVERRIDE_LIBRARY(id)) {
    if (!ID_IS_OVERRIDE_LIBRARY_REAL(id) ||
        (id->override_library->flag & LIBOVERRIDE_FLAG_SYSTEM_DEFINED) != 0)
    {
      return ICON_LIBRARY_DATA_OVERRIDE_NONEDITABLE;
    }
    return ICON_LIBRARY_DATA_OVERRIDE;
  }
  if (ID_IS_ASSET(id)) {
    return ICON_ASSET_MANAGER;
  }

  return ICON_NONE;
}

int UI_icon_from_rnaptr(const bContext *C, PointerRNA *ptr, int rnaicon, const bool big)
{
  ID *id = nullptr;

  if (!ptr->data) {
    return rnaicon;
  }

  /* Try ID, material, texture or dynamic-paint slot. */
  if (RNA_struct_is_ID(ptr->type)) {
    id = ptr->owner_id;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_MaterialSlot)) {
    id = static_cast<ID *>(RNA_pointer_get(ptr, "material").data);
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_TextureSlot)) {
    id = static_cast<ID *>(RNA_pointer_get(ptr, "texture").data);
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_FileBrowserFSMenuEntry)) {
    return RNA_int_get(ptr, "icon");
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_DynamicPaintSurface)) {
    DynamicPaintSurface *surface = static_cast<DynamicPaintSurface *>(ptr->data);

    if (surface->format == MOD_DPAINT_SURFACE_F_PTEX) {
      return ICON_SHADING_TEXTURE;
    }
    if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX) {
      return ICON_OUTLINER_DATA_MESH;
    }
    if (surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ) {
      return ICON_FILE_IMAGE;
    }
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_StudioLight)) {
    StudioLight *sl = static_cast<StudioLight *>(ptr->data);
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
    const int icon = ui_id_icon_get(C, id, big);

    return icon ? icon : rnaicon;
  }

  return rnaicon;
}

int UI_icon_from_idcode(const int idcode)
{
  switch ((ID_Type)idcode) {
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
    case ID_CU_LEGACY:
      return ICON_CURVE_DATA;
    case ID_GD_LEGACY:
      return ICON_OUTLINER_DATA_GREASEPENCIL;
    case ID_GR:
      return ICON_OUTLINER_COLLECTION;
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
      return ICON_MOD_MASK; /* TODO: this would need its own icon! */
    case ID_NT:
      return ICON_NODETREE;
    case ID_OB:
      return ICON_OBJECT_DATA;
    case ID_PA:
      return ICON_PARTICLE_DATA;
    case ID_PAL:
      return ICON_COLOR; /* TODO: this would need its own icon! */
    case ID_PC:
      return ICON_CURVE_BEZCURVE; /* TODO: this would need its own icon! */
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
    case ID_CV:
      return ICON_CURVES_DATA;
    case ID_PT:
      return ICON_POINTCLOUD_DATA;
    case ID_VO:
      return ICON_VOLUME_DATA;
    case ID_WO:
      return ICON_WORLD_DATA;
    case ID_WS:
      return ICON_WORKSPACE;
    case ID_SIM:
      /* TODO: Use correct icon. */
      return ICON_PHYSICS;
    case ID_GP:
      return ICON_OUTLINER_DATA_GREASEPENCIL;

    /* No icons for these ID-types. */
    case ID_LI:
    case ID_IP:
    case ID_KE:
    case ID_SCR:
    case ID_WM:
      break;
  }
  return ICON_NONE;
}

int UI_icon_from_object_mode(const int mode)
{
  switch ((eObjectMode)mode) {
    case OB_MODE_OBJECT:
      return ICON_OBJECT_DATAMODE;
    case OB_MODE_EDIT:
    case OB_MODE_EDIT_GPENCIL_LEGACY:
      return ICON_EDITMODE_HLT;
    case OB_MODE_SCULPT:
    case OB_MODE_SCULPT_GPENCIL_LEGACY:
    case OB_MODE_SCULPT_CURVES:
      return ICON_SCULPTMODE_HLT;
    case OB_MODE_VERTEX_PAINT:
    case OB_MODE_VERTEX_GPENCIL_LEGACY:
      return ICON_VPAINT_HLT;
    case OB_MODE_WEIGHT_PAINT:
    case OB_MODE_WEIGHT_GPENCIL_LEGACY:
      return ICON_WPAINT_HLT;
    case OB_MODE_TEXTURE_PAINT:
      return ICON_TPAINT_HLT;
    case OB_MODE_PARTICLE_EDIT:
      return ICON_PARTICLEMODE;
    case OB_MODE_POSE:
      return ICON_POSE_HLT;
    case OB_MODE_PAINT_GREASE_PENCIL:
    case OB_MODE_PAINT_GPENCIL_LEGACY:
      return ICON_GREASEPENCIL;
  }
  return ICON_NONE;
}

int UI_icon_color_from_collection(const Collection *collection)
{
  int icon = ICON_OUTLINER_COLLECTION;

  if (collection->color_tag != COLLECTION_COLOR_NONE) {
    icon = ICON_COLLECTION_COLOR_01 + collection->color_tag;
  }

  return icon;
}

void UI_icon_draw(float x, float y, int icon_id)
{
  UI_icon_draw_ex(
      x, y, icon_id, UI_INV_SCALE_FAC, 1.0f, 0.0f, nullptr, false, UI_NO_ICON_OVERLAY_TEXT);
}

void UI_icon_draw_alpha(float x, float y, int icon_id, float alpha)
{
  UI_icon_draw_ex(
      x, y, icon_id, UI_INV_SCALE_FAC, alpha, 0.0f, nullptr, false, UI_NO_ICON_OVERLAY_TEXT);
}

void UI_icon_draw_preview(float x, float y, int icon_id, float aspect, float alpha, int size)
{
  icon_draw_size(x,
                 y,
                 icon_id,
                 aspect,
                 alpha,
                 ICON_SIZE_PREVIEW,
                 size,
                 false,
                 nullptr,
                 false,
                 UI_NO_ICON_OVERLAY_TEXT);
}

void UI_icon_draw_ex(float x,
                     float y,
                     int icon_id,
                     float aspect,
                     float alpha,
                     float desaturate,
                     const uchar mono_color[4],
                     const bool mono_border,
                     const IconTextOverlay *text_overlay)
{
  const int draw_size = get_draw_size(ICON_SIZE_ICON);
  icon_draw_size(x,
                 y,
                 icon_id,
                 aspect,
                 alpha,
                 ICON_SIZE_ICON,
                 draw_size,
                 desaturate,
                 mono_color,
                 mono_border,
                 text_overlay);
}

void UI_icon_text_overlay_init_from_count(IconTextOverlay *text_overlay,
                                          const int icon_indicator_number)
{
  /* The icon indicator is used as an aggregator, no need to show if it is 1. */
  if (icon_indicator_number < 2) {
    text_overlay->text[0] = '\0';
    return;
  }
  BLI_str_format_integer_unit(text_overlay->text, icon_indicator_number);
}

/* ********** Alert Icons ********** */

ImBuf *UI_icon_alert_imbuf_get(eAlertIcon icon)
{
#ifdef WITH_HEADLESS
  UNUSED_VARS(icon);
  return nullptr;
#else
  const int ALERT_IMG_SIZE = 256;
  icon = eAlertIcon(MIN2(icon, ALERT_ICON_MAX - 1));
  const int left = icon * ALERT_IMG_SIZE;
  const rcti crop = {left, left + ALERT_IMG_SIZE - 1, 0, ALERT_IMG_SIZE - 1};
  ImBuf *ibuf = IMB_ibImageFromMemory((const uchar *)datatoc_alert_icons_png,
                                      datatoc_alert_icons_png_size,
                                      IB_rect,
                                      nullptr,
                                      "alert_icon");
  IMB_rect_crop(ibuf, &crop);
  IMB_premultiply_alpha(ibuf);
  return ibuf;
#endif
}
