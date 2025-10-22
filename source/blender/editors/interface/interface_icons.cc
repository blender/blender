/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "MEM_guardedalloc.h"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "BLF_api.hh"

#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BLT_translation.hh"

#include "DNA_collection_types.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_object_force_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_icons.hh"
#include "BKE_paint.hh"
#include "BKE_preview_image.hh"
#include "BKE_studiolight.h"

#include "IMB_imbuf.hh"
#include "IMB_thumbs.hh"

#include "BIF_glutil.hh"

#include "ED_keyframes_draw.hh"
#include "ED_keyframes_keylist.hh"
#include "ED_node.hh"
#include "ED_render.hh"

#include "UI_interface_icons.hh"

#include "WM_api.hh"

#include "CLG_log.h"

#include "interface_intern.hh"

#include <fmt/format.h>

static CLG_LogRef LOG = {"ui.icon"};

struct IconImage {
  int w;
  int h;
  uint8_t *rect;
  const uchar *datatoc_rect;
  int datatoc_size;
};

using VectorDrawFunc =
    void (*)(float x, float y, float w, float h, float alpha, const uchar mono_rgba[4]);

#define ICON_TYPE_PREVIEW 0
#define ICON_TYPE_SVG_COLOR 1
#define ICON_TYPE_SVG_MONO 2
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

struct IconType {
  int type;
  int theme_color;
};

#ifndef WITH_HEADLESS

static const IconType icontypes[] = {
#  define DEF_ICON(name) {ICON_TYPE_SVG_MONO, 0},
#  define DEF_ICON_COLOR(name) {ICON_TYPE_SVG_COLOR, 0},
#  define DEF_ICON_SCENE(name) {ICON_TYPE_SVG_MONO, TH_ICON_SCENE},
#  define DEF_ICON_COLLECTION(name) {ICON_TYPE_SVG_MONO, TH_ICON_COLLECTION},
#  define DEF_ICON_OBJECT(name) {ICON_TYPE_SVG_MONO, TH_ICON_OBJECT},
#  define DEF_ICON_OBJECT_DATA(name) {ICON_TYPE_SVG_MONO, TH_ICON_OBJECT_DATA},
#  define DEF_ICON_MODIFIER(name) {ICON_TYPE_SVG_MONO, TH_ICON_MODIFIER},
#  define DEF_ICON_SHADING(name) {ICON_TYPE_SVG_MONO, TH_ICON_SHADING},
#  define DEF_ICON_FOLDER(name) {ICON_TYPE_SVG_MONO, TH_ICON_FOLDER},
#  define DEF_ICON_FUND(name) {ICON_TYPE_SVG_MONO, TH_ICON_FUND},
#  define DEF_ICON_VECTOR(name) {ICON_TYPE_VECTOR, 0},
#  define DEF_ICON_BLANK(name) {ICON_TYPE_BLANK, 0},
#  include "UI_icons.hh"
};

/* **************************************************** */

static DrawInfo *def_internal_icon(
    ImBuf *bbuf, int icon_id, int xofs, int yofs, int size, int type, int theme_color)
{
  Icon *new_icon = MEM_callocN<Icon>(__func__);

  new_icon->obj = nullptr; /* icon is not for library object */
  new_icon->id_type = 0;

  DrawInfo *di = MEM_callocN<DrawInfo>(__func__);
  di->type = type;

  if (type == ICON_TYPE_SVG_MONO) {
    di->data.texture.theme_color = theme_color;
  }
  else if (type == ICON_TYPE_BUFFER) {
    IconImage *iimg = MEM_callocN<IconImage>(__func__);
    iimg->w = size;
    iimg->h = size;

    /* icon buffers can get initialized runtime now, via datatoc */
    if (bbuf) {
      int y, imgsize;

      iimg->rect = MEM_malloc_arrayN<uint8_t>(size * size * sizeof(uint), __func__);

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
  Icon *new_icon = MEM_callocN<Icon>("texicon");

  new_icon->obj = nullptr; /* icon is not for library object */
  new_icon->id_type = 0;

  DrawInfo *di = MEM_callocN<DrawInfo>("drawinfo");
  di->type = ICON_TYPE_VECTOR;
  di->data.vector.func = drawFunc;

  new_icon->drawinfo_free = nullptr;
  new_icon->drawinfo = di;

  BKE_icon_set(icon_id, new_icon);
}

/* Vector Icon Drawing Routines */

static void vicon_rgb_color_draw(
    float x, float y, float w, float h, const float color[4], float bg_alpha)
{
  rctf rect = {x, x + w, y, y + h};
  const float color_bg[4] = {color[0], color[1], color[2], bg_alpha};
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_4fv_ex(&rect, color_bg, nullptr, 1.0f, color, U.pixelsize, 2.0f * UI_SCALE_FAC);
}

static void vicon_rgb_text_draw(
    float x, float y, float w, float h, const char *str, const uchar mono_rgba[4])
{
  const int font_id = BLF_default();
  const size_t len = strlen(str);
  BLF_size(font_id, float(h - 3 * UI_SCALE_FAC));
  float width, height;
  BLF_width_and_height(font_id, str, len, &width, &height);
  const float pos_x = x + (w - width) / 2.0f;
  const float pos_y = y + (h - height) / 2.0f;
  BLF_position(font_id, pos_x, pos_y, 0);
  BLF_color4ubv(font_id, mono_rgba);
  BLF_draw(font_id, str, len);
}

static void vicon_rgb_red_draw(
    float x, float y, float w, float h, float alpha, const uchar mono_rgba[4])
{
  const float color[4] = {0.5f, 0.0f, 0.0f, 1.0f * alpha};
  vicon_rgb_color_draw(x, y, w, h, color, 0.25f * alpha);
  const char *text = CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "R");
  vicon_rgb_text_draw(x, y, w, h, text, mono_rgba);
}

static void vicon_rgb_green_draw(
    float x, float y, float w, float h, float alpha, const uchar mono_rgba[4])
{
  const float color[4] = {0.0f, 0.4f, 0.0f, 1.0f * alpha};
  vicon_rgb_color_draw(x, y, w, h, color, 0.2f * alpha);
  const char *text = CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "G");
  vicon_rgb_text_draw(x, y, w, h, text, mono_rgba);
}

static void vicon_rgb_blue_draw(
    float x, float y, float w, float h, float alpha, const uchar mono_rgba[4])
{
  const float color[4] = {0.0f, 0.0f, 1.0f, 1.0f * alpha};
  vicon_rgb_color_draw(x, y, w, h, color, 0.3f * alpha);
  const char *text = CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "B");
  vicon_rgb_text_draw(x, y, w, h, text, mono_rgba);
}

/* Utilities */

static void vicon_keytype_draw_wrapper(const float x,
                                       const float y,
                                       const float w,
                                       const float h,
                                       const float alpha,
                                       const eBezTriple_KeyframeType key_type,
                                       const short handle_type)
{
  /* Initialize dummy theme state for Action Editor - where these colors are defined
   * (since we're doing this off-screen, free from any particular space_id). */
  bThemeState theme_state;

  UI_Theme_Store(&theme_state);
  UI_SetTheme(SPACE_ACTION, RGN_TYPE_WINDOW);

  /* The "x" and "y" given are the bottom-left coordinates of the icon,
   * while the #draw_keyframe_shape() function needs the midpoint for the keyframe. */
  const float xco = x + (w / 2.0f);
  const float yco = y + (h / 2.0f);

  GPUVertFormat *format = immVertexFormat();
  KeyframeShaderBindings sh_bindings;
  sh_bindings.pos_id = GPU_vertformat_attr_add(
      format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  sh_bindings.size_id = GPU_vertformat_attr_add(
      format, "size", blender::gpu::VertAttrType::SFLOAT_32);
  sh_bindings.color_id = GPU_vertformat_attr_add(
      format, "color", blender::gpu::VertAttrType::UNORM_8_8_8_8);
  sh_bindings.outline_color_id = GPU_vertformat_attr_add(
      format, "outlineColor", blender::gpu::VertAttrType::UNORM_8_8_8_8);
  sh_bindings.flags_id = GPU_vertformat_attr_add(
      format, "flags", blender::gpu::VertAttrType::UINT_32);

  GPU_program_point_size(true);
  immBindBuiltinProgram(GPU_SHADER_KEYFRAME_SHAPE);
  immUniform1f("outline_scale", 1.0f);
  immUniform2f("ViewportSize", -1.0f, -1.0f);
  immBegin(GPU_PRIM_POINTS, 1);

  /* draw keyframe
   * - size: (default icon size == 16, default dope-sheet icon size == 10)
   * - sel: true unless in handle-type icons
   *   (so that "keyframe" state shows the iconic yellow icon).
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

static void vicon_keytype_keyframe_draw(
    float x, float y, float w, float h, float alpha, const uchar * /*mono_rgba[4]*/)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_KEYFRAME, KEYFRAME_HANDLE_NONE);
}

static void vicon_keytype_breakdown_draw(
    float x, float y, float w, float h, float alpha, const uchar * /*mono_rgba[4]*/)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_BREAKDOWN, KEYFRAME_HANDLE_NONE);
}

static void vicon_keytype_extreme_draw(
    float x, float y, float w, float h, float alpha, const uchar * /*mono_rgba[4]*/)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_EXTREME, KEYFRAME_HANDLE_NONE);
}

static void vicon_keytype_jitter_draw(
    float x, float y, float w, float h, float alpha, const uchar * /*mono_rgba[4]*/)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_JITTER, KEYFRAME_HANDLE_NONE);
}

static void vicon_keytype_moving_hold_draw(
    float x, float y, float w, float h, float alpha, const uchar * /*mono_rgba[4]*/)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_MOVEHOLD, KEYFRAME_HANDLE_NONE);
}

static void vicon_keytype_generated_draw(
    float x, float y, float w, float h, float alpha, const uchar * /*mono_rgba[4]*/)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_GENERATED, KEYFRAME_HANDLE_NONE);
}

static void vicon_handletype_free_draw(
    float x, float y, float w, float h, float alpha, const uchar * /*mono_rgba[4]*/)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_KEYFRAME, KEYFRAME_HANDLE_FREE);
}

static void vicon_handletype_aligned_draw(
    float x, float y, float w, float h, float alpha, const uchar * /*mono_rgba[4]*/)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_KEYFRAME, KEYFRAME_HANDLE_ALIGNED);
}

static void vicon_handletype_vector_draw(
    float x, float y, float w, float h, float alpha, const uchar * /*mono_rgba[4]*/)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_KEYFRAME, KEYFRAME_HANDLE_VECTOR);
}

static void vicon_handletype_auto_draw(
    float x, float y, float w, float h, float alpha, const uchar * /*mono_rgba[4]*/)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_KEYFRAME, KEYFRAME_HANDLE_AUTO);
}

static void vicon_handletype_auto_clamp_draw(
    float x, float y, float w, float h, float alpha, const uchar * /*mono_rgba[4]*/)
{
  vicon_keytype_draw_wrapper(x, y, w, h, alpha, BEZT_KEYTYPE_KEYFRAME, KEYFRAME_HANDLE_AUTO_CLAMP);
}

static void icon_node_socket_draw(
    int socket_type, float x, float y, float w, float h, float /*alpha*/)
{
  /* Factor to account for the draw function of the node socket being based on the widget unit,
   * which is 10 pixels by default, which differs from icons. */
  constexpr float size_factor = 10.0f / float(ICON_DEFAULT_WIDTH);

  const float socket_radius = w * 0.5f * size_factor;
  const blender::float2 center = {x + 0.5f * w, y + 0.5f * h};
  const rctf rect = {
      center.x - socket_radius,
      center.x + socket_radius,
      center.y - socket_radius,
      center.y + socket_radius,
  };

  float color_inner[4];
  blender::ed::space_node::std_node_socket_colors_get(socket_type, color_inner);

  float color_outer[4] = {0};
  UI_GetThemeColorType4fv(TH_WIRE, SPACE_NODE, color_outer);
  color_outer[3] = 1.0f;

  blender::ed::space_node::node_draw_nodesocket(
      &rect, color_inner, color_outer, U.pixelsize, SOCK_DISPLAY_SHAPE_CIRCLE, 1.0f);
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
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* XXX: Include alpha into this... */
  /* normal */
  immUniformColor3ubv(cs->solid);
  immRectf(pos, x, y, a, y + h);

  /* selected */
  immUniformColor3ubv(cs->select);
  immRectf(pos, a, y, b, y + h);

  /* active */
  immUniformColor3ubv(cs->active);
  immRectf(pos, b, y, c, y + h);

  immUnbindProgram();
}

#  define DEF_ICON_VECTOR_COLORSET_DRAW_NTH(prefix, index) \
    static void vicon_colorset_draw_##prefix( \
        float x, float y, float w, float h, float alpha, const uchar * /*mono_rgba[4]*/) \
    { \
      vicon_colorset_draw(index, int(x), int(y), int(w), int(h), alpha); \
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

static void vicon_strip_color_draw(
    short color_tag, float x, float y, float w, float /*h*/, float /*alpha*/)
{
  bTheme *btheme = UI_GetTheme();
  const ThemeStripColor *strip_color = &btheme->strip_color[color_tag];

  const float aspect = float(ICON_DEFAULT_WIDTH) / w;

  UI_icon_draw_ex(x,
                  y,
                  ICON_SNAP_FACE,
                  aspect,
                  1.0f,
                  0.0f,
                  strip_color->color,
                  btheme->tui.icon_border_intensity > 0.0f,
                  UI_NO_ICON_OVERLAY_TEXT);
}

#  define DEF_ICON_STRIP_COLOR_DRAW(index, color) \
    static void vicon_strip_color_draw_##index( \
        float x, float y, float w, float h, float alpha, const uchar * /*mono_rgba[4]*/) \
    { \
      vicon_strip_color_draw(color, x, y, w, h, alpha); \
    }

DEF_ICON_STRIP_COLOR_DRAW(01, STRIP_COLOR_01);
DEF_ICON_STRIP_COLOR_DRAW(02, STRIP_COLOR_02);
DEF_ICON_STRIP_COLOR_DRAW(03, STRIP_COLOR_03);
DEF_ICON_STRIP_COLOR_DRAW(04, STRIP_COLOR_04);
DEF_ICON_STRIP_COLOR_DRAW(05, STRIP_COLOR_05);
DEF_ICON_STRIP_COLOR_DRAW(06, STRIP_COLOR_06);
DEF_ICON_STRIP_COLOR_DRAW(07, STRIP_COLOR_07);
DEF_ICON_STRIP_COLOR_DRAW(08, STRIP_COLOR_08);
DEF_ICON_STRIP_COLOR_DRAW(09, STRIP_COLOR_09);

#  undef DEF_ICON_STRIP_COLOR_DRAW

#  define ICON_INDIRECT_DATA_ALPHA 0.6f

static void vicon_strip_color_draw_library_data_indirect(
    float x, float y, float w, float /*h*/, float alpha, const uchar * /*mono_rgba[4]*/)
{
  const float aspect = float(ICON_DEFAULT_WIDTH) / w;

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
    float x, float y, float w, float /*h*/, float alpha, const uchar * /*mono_rgba[4]*/)
{
  const float aspect = float(ICON_DEFAULT_WIDTH) / w;

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

static void vicon_layergroup_color_draw(
    short color_tag, float x, float y, float w, float /*h*/, float /*alpha*/)
{
  bTheme *btheme = UI_GetTheme();
  const ThemeCollectionColor *layergroup_color = &btheme->collection_color[color_tag];

  const float aspect = float(ICON_DEFAULT_WIDTH) / w;

  UI_icon_draw_ex(x,
                  y,
                  ICON_GREASEPENCIL_LAYER_GROUP,
                  aspect,
                  1.0f,
                  0.0f,
                  layergroup_color->color,
                  btheme->tui.icon_border_intensity > 0.0f,
                  UI_NO_ICON_OVERLAY_TEXT);
}

#  define DEF_ICON_LAYERGROUP_COLOR_DRAW(index, color) \
    static void vicon_layergroup_color_draw_##index( \
        float x, float y, float w, float h, float alpha, const uchar * /*mono_rgba[4]*/) \
    { \
      vicon_layergroup_color_draw(color, x, y, w, h, alpha); \
    }

DEF_ICON_LAYERGROUP_COLOR_DRAW(01, LAYERGROUP_COLOR_01);
DEF_ICON_LAYERGROUP_COLOR_DRAW(02, LAYERGROUP_COLOR_02);
DEF_ICON_LAYERGROUP_COLOR_DRAW(03, LAYERGROUP_COLOR_03);
DEF_ICON_LAYERGROUP_COLOR_DRAW(04, LAYERGROUP_COLOR_04);
DEF_ICON_LAYERGROUP_COLOR_DRAW(05, LAYERGROUP_COLOR_05);
DEF_ICON_LAYERGROUP_COLOR_DRAW(06, LAYERGROUP_COLOR_06);
DEF_ICON_LAYERGROUP_COLOR_DRAW(07, LAYERGROUP_COLOR_07);
DEF_ICON_LAYERGROUP_COLOR_DRAW(08, LAYERGROUP_COLOR_08);

#  undef DEF_ICON_LAYERGROUP_COLOR_DRAW

#  define DEF_ICON_NODE_SOCKET_DRAW(name, socket_type) \
    static void icon_node_socket_draw_##name( \
        float x, float y, float w, float h, float alpha, const uchar * /*mono_rgba[4]*/) \
    { \
      icon_node_socket_draw(socket_type, x, y, w, h, alpha); \
    }

DEF_ICON_NODE_SOCKET_DRAW(float, eNodeSocketDatatype::SOCK_FLOAT)
DEF_ICON_NODE_SOCKET_DRAW(vector, eNodeSocketDatatype::SOCK_VECTOR)
DEF_ICON_NODE_SOCKET_DRAW(rgba, eNodeSocketDatatype::SOCK_RGBA)
DEF_ICON_NODE_SOCKET_DRAW(shader, eNodeSocketDatatype::SOCK_SHADER)
DEF_ICON_NODE_SOCKET_DRAW(boolean, eNodeSocketDatatype::SOCK_BOOLEAN)
DEF_ICON_NODE_SOCKET_DRAW(int, eNodeSocketDatatype::SOCK_INT)
DEF_ICON_NODE_SOCKET_DRAW(string, eNodeSocketDatatype::SOCK_STRING)
DEF_ICON_NODE_SOCKET_DRAW(object, eNodeSocketDatatype::SOCK_OBJECT)
DEF_ICON_NODE_SOCKET_DRAW(image, eNodeSocketDatatype::SOCK_IMAGE)
DEF_ICON_NODE_SOCKET_DRAW(geometry, eNodeSocketDatatype::SOCK_GEOMETRY)
DEF_ICON_NODE_SOCKET_DRAW(collection, eNodeSocketDatatype::SOCK_COLLECTION)
DEF_ICON_NODE_SOCKET_DRAW(texture, eNodeSocketDatatype::SOCK_TEXTURE)
DEF_ICON_NODE_SOCKET_DRAW(material, eNodeSocketDatatype::SOCK_MATERIAL)
DEF_ICON_NODE_SOCKET_DRAW(rotation, eNodeSocketDatatype::SOCK_ROTATION)
DEF_ICON_NODE_SOCKET_DRAW(menu, eNodeSocketDatatype::SOCK_MENU)
DEF_ICON_NODE_SOCKET_DRAW(matrix, eNodeSocketDatatype::SOCK_MATRIX)
DEF_ICON_NODE_SOCKET_DRAW(bundle, eNodeSocketDatatype::SOCK_BUNDLE)
DEF_ICON_NODE_SOCKET_DRAW(closure, eNodeSocketDatatype::SOCK_CLOSURE)

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
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformColor3fv(gpl->color);
  immRectf(pos, x, y, x + w - 1, y + h - 1);

  immUnbindProgram();
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
    if (event_value == KM_DBL_CLICK) {
      return ICON_MOUSE_LMB_2X;
    }
    return (event_value == KM_PRESS_DRAG) ? ICON_MOUSE_LMB_DRAG : ICON_MOUSE_LMB;
  }
  if (event_type == MIDDLEMOUSE) {
    return (event_value == KM_PRESS_DRAG) ? ICON_MOUSE_MMB_DRAG : ICON_MOUSE_MMB;
  }
  if (event_type == RIGHTMOUSE) {
    return (event_value == KM_PRESS_DRAG) ? ICON_MOUSE_MMB_DRAG : ICON_MOUSE_RMB;
  }

  return ICON_NONE;
}

int UI_icon_from_keymap_item(const wmKeyMapItem *kmi, int r_icon_mod[KM_MOD_NUM])
{
  if (r_icon_mod) {
    memset(r_icon_mod, 0x0, sizeof(int[KM_MOD_NUM]));
    int i = 0;
    if (kmi->ctrl == KM_MOD_HELD) {
      r_icon_mod[i++] = ICON_EVENT_CTRL;
    }
    if (kmi->alt == KM_MOD_HELD) {
      r_icon_mod[i++] = ICON_EVENT_ALT;
    }
    if (kmi->shift == KM_MOD_HELD) {
      r_icon_mod[i++] = ICON_EVENT_SHIFT;
    }
    if (kmi->oskey == KM_MOD_HELD) {
      r_icon_mod[i++] = ICON_EVENT_OS;
    }
    if (!ELEM(kmi->hyper, KM_NOTHING, KM_ANY)) {
      r_icon_mod[i++] = ICON_EVENT_HYPER;
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
  INIT_EVENT_ICON(ICON_EVENT_HYPER, EVT_HYPER, KM_ANY);
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

  INIT_EVENT_ICON(ICON_EVENT_ZEROKEY, EVT_ZEROKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_ONEKEY, EVT_ONEKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_TWOKEY, EVT_TWOKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_THREEKEY, EVT_THREEKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_FOURKEY, EVT_FOURKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_FIVEKEY, EVT_FIVEKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_SIXKEY, EVT_SIXKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_SEVENKEY, EVT_SEVENKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_EIGHTKEY, EVT_EIGHTKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NINEKEY, EVT_NINEKEY, KM_ANY);

  INIT_EVENT_ICON(ICON_EVENT_PAD0, EVT_PAD0, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAD1, EVT_PAD1, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAD2, EVT_PAD2, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAD3, EVT_PAD3, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAD4, EVT_PAD4, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAD5, EVT_PAD5, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAD6, EVT_PAD6, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAD7, EVT_PAD7, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAD8, EVT_PAD8, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAD9, EVT_PAD9, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PADASTER, EVT_PADASTERKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PADSLASH, EVT_PADSLASHKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PADMINUS, EVT_PADMINUS, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PADENTER, EVT_PADENTER, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PADPLUS, EVT_PADPLUSKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PADPERIOD, EVT_PADPERIOD, KM_ANY);

  INIT_EVENT_ICON(ICON_EVENT_MOUSE_4, BUTTON4MOUSE, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_MOUSE_5, BUTTON5MOUSE, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_MOUSE_6, BUTTON6MOUSE, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_MOUSE_7, BUTTON7MOUSE, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_TABLET_STYLUS, TABLET_STYLUS, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_TABLET_ERASER, TABLET_ERASER, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_LEFT_ARROW, EVT_LEFTARROWKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_DOWN_ARROW, EVT_DOWNARROWKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_RIGHT_ARROW, EVT_RIGHTARROWKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_UP_ARROW, EVT_UPARROWKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAUSE, EVT_PAUSEKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_INSERT, EVT_INSERTKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_HOME, EVT_HOMEKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_END, EVT_ENDKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_UNKNOWN, EVT_UNKNOWNKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_GRLESS, EVT_GRLESSKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_MEDIAPLAY, EVT_MEDIAPLAY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_MEDIASTOP, EVT_MEDIASTOP, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_MEDIAFIRST, EVT_MEDIAFIRST, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_MEDIALAST, EVT_MEDIALAST, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_APP, EVT_APPKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_CAPSLOCK, EVT_CAPSLOCKKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_BACKSPACE, EVT_BACKSPACEKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_DEL, EVT_DELKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_SEMICOLON, EVT_SEMICOLONKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PERIOD, EVT_PERIODKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_COMMA, EVT_COMMAKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_QUOTE, EVT_QUOTEKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_ACCENTGRAVE, EVT_ACCENTGRAVEKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_MINUS, EVT_MINUSKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PLUS, EVT_PLUSKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_SLASH, EVT_SLASHKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_BACKSLASH, EVT_BACKSLASHKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_EQUAL, EVT_EQUALKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_LEFTBRACKET, EVT_LEFTBRACKETKEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_RIGHTBRACKET, EVT_RIGHTBRACKETKEY, KM_ANY);

  INIT_EVENT_ICON(ICON_EVENT_PAD_PAN, MOUSEPAN, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAD_ROTATE, MOUSEROTATE, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_PAD_ZOOM, MOUSEZOOM, KM_ANY);

  INIT_EVENT_ICON(ICON_EVENT_F13, EVT_F13KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F14, EVT_F14KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F15, EVT_F15KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F16, EVT_F16KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F17, EVT_F17KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F18, EVT_F18KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F19, EVT_F19KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F20, EVT_F20KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F21, EVT_F21KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F22, EVT_F22KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F23, EVT_F23KEY, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_F24, EVT_F24KEY, KM_ANY);

  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_V1, NDOF_BUTTON_V1, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_V2, NDOF_BUTTON_V2, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_V3, NDOF_BUTTON_V3, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_SAVE_V1, NDOF_BUTTON_SAVE_V1, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_SAVE_V2, NDOF_BUTTON_SAVE_V2, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_SAVE_V3, NDOF_BUTTON_SAVE_V3, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_1, NDOF_BUTTON_1, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_2, NDOF_BUTTON_2, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_3, NDOF_BUTTON_3, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_4, NDOF_BUTTON_4, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_5, NDOF_BUTTON_5, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_6, NDOF_BUTTON_6, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_7, NDOF_BUTTON_7, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_8, NDOF_BUTTON_8, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_9, NDOF_BUTTON_9, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_10, NDOF_BUTTON_10, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_11, NDOF_BUTTON_11, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_12, NDOF_BUTTON_12, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_MENU, NDOF_BUTTON_MENU, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_FIT, NDOF_BUTTON_FIT, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_TOP, NDOF_BUTTON_TOP, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_BOTTOM, NDOF_BUTTON_BOTTOM, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_LEFT, NDOF_BUTTON_LEFT, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_RIGHT, NDOF_BUTTON_RIGHT, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_FRONT, NDOF_BUTTON_FRONT, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_BACK, NDOF_BUTTON_BACK, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_ISO1, NDOF_BUTTON_ISO1, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_ISO2, NDOF_BUTTON_ISO2, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_ROLL_CW, NDOF_BUTTON_ROLL_CW, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_ROLL_CCW, NDOF_BUTTON_ROLL_CCW, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_SPIN_CW, NDOF_BUTTON_SPIN_CW, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_SPIN_CCW, NDOF_BUTTON_SPIN_CCW, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_TILT_CW, NDOF_BUTTON_TILT_CW, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_TILT_CCW, NDOF_BUTTON_TILT_CCW, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_ROTATE, NDOF_BUTTON_ROTATE, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_PANZOOM, NDOF_BUTTON_PANZOOM, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_DOMINANT, NDOF_BUTTON_DOMINANT, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_PLUS, NDOF_BUTTON_PLUS, KM_ANY);
  INIT_EVENT_ICON(ICON_EVENT_NDOF_BUTTON_MINUS, NDOF_BUTTON_MINUS, KM_ANY);

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
    ImBuf *bbuf = IMB_load_image_from_memory(
        iimg->datatoc_rect, iimg->datatoc_size, IB_byte_data, "<matcap icon>");
    /* w and h were set on initialize */
    if (bbuf->x != iimg->h && bbuf->y != iimg->w) {
      IMB_scale(bbuf, iimg->w, iimg->h, IMBScaleFilter::Box, false);
    }

    iimg->rect = IMB_steal_byte_buffer(bbuf);
    IMB_freeImBuf(bbuf);
  }
}

static void init_internal_icons()
{
  /* Define icons. */
  for (int x = ICON_NONE; x < ICON_BLANK_LAST_SVG_ITEM; x++) {
    const IconType icontype = icontypes[x];
    if (!ELEM(icontype.type, ICON_TYPE_SVG_MONO, ICON_TYPE_SVG_COLOR)) {
      continue;
    }
    def_internal_icon(nullptr, x, 0, 0, 0, icontype.type, icontype.theme_color);
  }

  def_internal_vicon(ICON_RGB_RED, vicon_rgb_red_draw);
  def_internal_vicon(ICON_RGB_GREEN, vicon_rgb_green_draw);
  def_internal_vicon(ICON_RGB_BLUE, vicon_rgb_blue_draw);

  def_internal_vicon(ICON_KEYTYPE_KEYFRAME_VEC, vicon_keytype_keyframe_draw);
  def_internal_vicon(ICON_KEYTYPE_BREAKDOWN_VEC, vicon_keytype_breakdown_draw);
  def_internal_vicon(ICON_KEYTYPE_EXTREME_VEC, vicon_keytype_extreme_draw);
  def_internal_vicon(ICON_KEYTYPE_JITTER_VEC, vicon_keytype_jitter_draw);
  def_internal_vicon(ICON_KEYTYPE_MOVING_HOLD_VEC, vicon_keytype_moving_hold_draw);
  def_internal_vicon(ICON_KEYTYPE_GENERATED_VEC, vicon_keytype_generated_draw);

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

  def_internal_vicon(ICON_STRIP_COLOR_01, vicon_strip_color_draw_01);
  def_internal_vicon(ICON_STRIP_COLOR_02, vicon_strip_color_draw_02);
  def_internal_vicon(ICON_STRIP_COLOR_03, vicon_strip_color_draw_03);
  def_internal_vicon(ICON_STRIP_COLOR_04, vicon_strip_color_draw_04);
  def_internal_vicon(ICON_STRIP_COLOR_05, vicon_strip_color_draw_05);
  def_internal_vicon(ICON_STRIP_COLOR_06, vicon_strip_color_draw_06);
  def_internal_vicon(ICON_STRIP_COLOR_07, vicon_strip_color_draw_07);
  def_internal_vicon(ICON_STRIP_COLOR_08, vicon_strip_color_draw_08);
  def_internal_vicon(ICON_STRIP_COLOR_09, vicon_strip_color_draw_09);

  def_internal_vicon(ICON_LIBRARY_DATA_INDIRECT, vicon_strip_color_draw_library_data_indirect);
  def_internal_vicon(ICON_LIBRARY_DATA_OVERRIDE_NONEDITABLE,
                     vicon_strip_color_draw_library_data_override_noneditable);

  def_internal_vicon(ICON_LAYERGROUP_COLOR_01, vicon_layergroup_color_draw_01);
  def_internal_vicon(ICON_LAYERGROUP_COLOR_02, vicon_layergroup_color_draw_02);
  def_internal_vicon(ICON_LAYERGROUP_COLOR_03, vicon_layergroup_color_draw_03);
  def_internal_vicon(ICON_LAYERGROUP_COLOR_04, vicon_layergroup_color_draw_04);
  def_internal_vicon(ICON_LAYERGROUP_COLOR_05, vicon_layergroup_color_draw_05);
  def_internal_vicon(ICON_LAYERGROUP_COLOR_06, vicon_layergroup_color_draw_06);
  def_internal_vicon(ICON_LAYERGROUP_COLOR_07, vicon_layergroup_color_draw_07);
  def_internal_vicon(ICON_LAYERGROUP_COLOR_08, vicon_layergroup_color_draw_08);

  def_internal_vicon(ICON_NODE_SOCKET_FLOAT, icon_node_socket_draw_float);
  def_internal_vicon(ICON_NODE_SOCKET_VECTOR, icon_node_socket_draw_vector);
  def_internal_vicon(ICON_NODE_SOCKET_RGBA, icon_node_socket_draw_rgba);
  def_internal_vicon(ICON_NODE_SOCKET_SHADER, icon_node_socket_draw_shader);
  def_internal_vicon(ICON_NODE_SOCKET_BOOLEAN, icon_node_socket_draw_boolean);
  def_internal_vicon(ICON_NODE_SOCKET_INT, icon_node_socket_draw_int);
  def_internal_vicon(ICON_NODE_SOCKET_STRING, icon_node_socket_draw_string);
  def_internal_vicon(ICON_NODE_SOCKET_OBJECT, icon_node_socket_draw_object);
  def_internal_vicon(ICON_NODE_SOCKET_IMAGE, icon_node_socket_draw_image);
  def_internal_vicon(ICON_NODE_SOCKET_GEOMETRY, icon_node_socket_draw_geometry);
  def_internal_vicon(ICON_NODE_SOCKET_COLLECTION, icon_node_socket_draw_collection);
  def_internal_vicon(ICON_NODE_SOCKET_TEXTURE, icon_node_socket_draw_texture);
  def_internal_vicon(ICON_NODE_SOCKET_MATERIAL, icon_node_socket_draw_material);
  def_internal_vicon(ICON_NODE_SOCKET_ROTATION, icon_node_socket_draw_rotation);
  def_internal_vicon(ICON_NODE_SOCKET_MENU, icon_node_socket_draw_menu);
  def_internal_vicon(ICON_NODE_SOCKET_MATRIX, icon_node_socket_draw_matrix);
  def_internal_vicon(ICON_NODE_SOCKET_BUNDLE, icon_node_socket_draw_bundle);
  def_internal_vicon(ICON_NODE_SOCKET_CLOSURE, icon_node_socket_draw_closure);
}

#else

#endif /* WITH_HEADLESS */

void UI_icons_free()
{
  BKE_icons_free();
  BKE_preview_images_free();
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

  DrawInfo *di = MEM_callocN<DrawInfo>("di_icon");

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
  init_internal_icons();
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
      CLOG_WARN(&LOG, "%s, error: requested preview image does not exist", __func__);
    }
  }
  else if (!prv_img->rect[size]) {
    prv_img->flag[size] |= PRV_CHANGED;
    prv_img->changed_timestamp[size] = 0;
    if (!ED_preview_use_image_size(prv_img, size)) {
      prv_img->w[size] = render_size;
      prv_img->h[size] = render_size;
      prv_img->rect[size] = MEM_calloc_arrayN<uint>(render_size * render_size, "prv_rect");
    }
  }
}

static void ui_id_preview_image_render_size(
    const bContext *C, Scene *scene, ID *id, PreviewImage *pi, int size, const bool use_job);

static void ui_studiolight_icon_job_exec(void *customdata, wmJobWorkerStatus * /*worker_status*/)
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

        if (id || prv->runtime->deferred_loading_data) {
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
          IconImage *img = MEM_callocN<IconImage>(__func__);

          img->w = STUDIOLIGHT_ICON_SIZE;
          img->h = STUDIOLIGHT_ICON_SIZE;
          const size_t size = STUDIOLIGHT_ICON_SIZE * STUDIOLIGHT_ICON_SIZE * sizeof(uint);
          img->rect = MEM_malloc_arrayN<uint8_t>(size, __func__);
          memset(img->rect, 0, size);
          di->data.buffer.image = img;

          wmJob *wm_job = WM_jobs_get(wm,
                                      CTX_wm_window(C),
                                      icon,
                                      "Generating StudioLight icon...",
                                      eWM_JobFlag(0),
                                      WM_JOB_TYPE_STUDIOLIGHT);
          Icon **tmp = MEM_callocN<Icon *>(__func__);
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

bool ui_icon_is_preview_deferred_loading(const int icon_id, const bool big)
{
  const Icon *icon = BKE_icon_get(icon_id);
  if (icon == nullptr) {
    return false;
  }

  const DrawInfo *di = static_cast<DrawInfo *>(icon->drawinfo);
  if (icon->drawinfo == nullptr) {
    return false;
  }

  if (di->type == ICON_TYPE_PREVIEW) {
    const ID *id = (icon->id_type != 0) ? static_cast<ID *>(icon->obj) : nullptr;
    const PreviewImage *prv = id ? BKE_previewimg_id_get(id) :
                                   static_cast<PreviewImage *>(icon->obj);

    if (prv) {
      const int size = big ? ICON_SIZE_PREVIEW : ICON_SIZE_ICON;
      return (prv->flag[size] & PRV_RENDERING) != 0;
    }
  }

  return false;
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
      CLOG_WARN(&LOG, "%s: no preview image for this ID: %s", __func__, id->name);
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
    const PreviewImage *prv = (icon->id_type != 0) ? BKE_previewimg_id_ensure((ID *)icon->obj) :
                                                     static_cast<const PreviewImage *>(icon->obj);

    if (prv) {
      return BKE_previewimg_copy(prv);
    }
  }
  else if (di->data.buffer.image) {
    ImBuf *bbuf;

    bbuf = IMB_load_image_from_memory(di->data.buffer.image->datatoc_rect,
                                      di->data.buffer.image->datatoc_size,
                                      IB_byte_data,
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
                           int rw,
                           int rh,
                           const uint8_t *rect,
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
  /* `rect` contains image in render-size, we only scale if needed. */
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
  GPUBuiltinShader shader;
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

  immDrawPixelsTexScaledFullSize(&state,
                                 draw_x,
                                 draw_y,
                                 rw,
                                 rh,
                                 blender::gpu::TextureFormat::UNORM_8_8_8_8,
                                 true,
                                 rect,
                                 scale_x,
                                 scale_y,
                                 1.0f,
                                 1.0f,
                                 col);
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

static void svg_replace_color_attributes(std::string &svg,
                                         const std::string &name,
                                         const size_t start,
                                         const size_t end)
{
  bTheme *btheme = UI_GetTheme();

  uchar white[] = {255, 255, 255, 255};
  uchar black[] = {0, 0, 0, 255};
  uchar logo_orange[] = {232, 125, 13, 255};
  uchar logo_blue[] = {38, 87, 135, 255};

  /* Tool colors hardcoded for now. */
  uchar tool_add[] = {117, 255, 175, 255};
  uchar tool_remove[] = {245, 107, 91, 255};
  uchar tool_select[] = {255, 176, 43, 255};
  uchar tool_transform[] = {217, 175, 245, 255};
  uchar tool_white[] = {255, 255, 255, 255};
  uchar tool_red[] = {214, 45, 48, 255};

  const struct ColorItem {
    const char *name;
    uchar *col = nullptr;
    int colorid = TH_UNDEFINED;
    int spacetype = SPACE_TYPE_ANY;
  } items[] = {
      {"blender_white", white},
      {"blender_black", black},
      {"blender_logo_orange", logo_orange},
      {"blender_logo_blue", logo_blue},
      {"blender_selected", btheme->tui.wcol_regular.inner},
      {"blender_mesh_selected", btheme->space_view3d.vertex_select},
      {"blender_back", nullptr, TH_BACK},
      {"blender_text", nullptr, TH_TEXT},
      {"blender_text_hi", nullptr, TH_TEXT_HI},
      {"blender_red_alert", nullptr, TH_ERROR},
      {"blender_error", nullptr, TH_ERROR},
      {"blender_warning", nullptr, TH_WARNING},
      {"blender_info", nullptr, TH_INFO},
      {"blender_scene", nullptr, TH_ICON_SCENE},
      {"blender_collection", nullptr, TH_ICON_COLLECTION},
      {"blender_collection_color_01", btheme->collection_color[0].color},
      {"blender_collection_color_02", btheme->collection_color[1].color},
      {"blender_collection_color_03", btheme->collection_color[2].color},
      {"blender_collection_color_04", btheme->collection_color[3].color},
      {"blender_collection_color_05", btheme->collection_color[4].color},
      {"blender_collection_color_06", btheme->collection_color[5].color},
      {"blender_collection_color_07", btheme->collection_color[6].color},
      {"blender_collection_color_08", btheme->collection_color[7].color},
      {"blender_object", nullptr, TH_ICON_OBJECT},
      {"blender_object_data", nullptr, TH_ICON_OBJECT_DATA},
      {"blender_modifier", nullptr, TH_ICON_MODIFIER},
      {"blender_shading", nullptr, TH_ICON_SHADING},
      {"blender_folder", nullptr, TH_ICON_FOLDER},
      {"blender_fund", nullptr, TH_ICON_FUND},
      {"blender_autokey", nullptr, TH_ICON_AUTOKEY},
      {"blender_tool_add", tool_add},
      {"blender_tool_remove", tool_remove},
      {"blender_tool_select", tool_select},
      {"blender_tool_transform", tool_transform},
      {"blender_tool_white", tool_white},
      {"blender_tool_red", tool_red},
      {"blender_bevel", nullptr, TH_BEVEL},
      {"blender_crease", nullptr, TH_CREASE},
      {"blender_seam", nullptr, TH_SEAM},
      {"blender_sharp", nullptr, TH_SHARP},
      {"blender_ipo_linear", btheme->space_action.anim_interpolation_linear},
      {"blender_ipo_constant", btheme->space_action.anim_interpolation_constant},
      {"blender_ipo_other", btheme->space_action.anim_interpolation_other},
  };

  for (const ColorItem &item : items) {
    if (name != item.name) {
      continue;
    }

    uchar color[4];
    if (item.col) {
      memcpy(color, item.col, sizeof(color));
    }
    else if (item.colorid != TH_UNDEFINED) {
      if (item.spacetype != SPACE_TYPE_ANY) {
        UI_GetThemeColorType4ubv(item.colorid, item.spacetype, color);
      }
      else {
        UI_GetThemeColor4ubv(item.colorid, color);
      }
    }
    else {
      continue;
    }

    std::string hexcolor = fmt::format(
        "{:02x}{:02x}{:02x}{:02x}", color[0], color[1], color[2], color[3]);

    size_t att_start = start;
    while (true) {
      constexpr static blender::StringRef key = "fill=\"";
      att_start = svg.find(key, att_start);
      if (att_start == std::string::npos || att_start > end) {
        break;
      }
      const size_t att_end = svg.find("\"", att_start + key.size());
      if (att_end != std::string::npos && att_end < end) {
        svg.replace(att_start, att_end - att_start, key + "#" + hexcolor);
      }
      att_start += blender::StringRef(key + "#rrggbbaa\"").size();
    }

    att_start = start;
    while (true) {
      constexpr static blender::StringRef key = "fill:";
      att_start = svg.find(key, att_start);
      if (att_start == std::string::npos || att_start > end) {
        break;
      }
      const size_t att_end = svg.find(";", att_start + key.size());
      if (att_end != std::string::npos && att_end - att_start < end) {
        svg.replace(att_start, att_end - att_start, key + "#" + hexcolor);
      }
      att_start += blender::StringRef(key + "#rrggbbaa").size();
    }
  }
}

static void icon_source_edit_cb(std::string &svg)
{
  size_t g_start = 0;

  /* Scan string, processing only groups with our keyword ids. */

  while (true) {
    /* Look for a blender id, quick exit if not found. */
    constexpr static blender::StringRef key = "id=\"";
    const size_t id_start = svg.find(key + "blender_", g_start);
    if (id_start == std::string::npos) {
      return;
    }

    /* Scan back to beginning of this group element. */
    g_start = svg.rfind("<g", id_start);
    if (g_start == std::string::npos) {
      /* Malformed. */
      return;
    }

    /* Scan forward to end of the group. */
    const size_t g_end = svg.find("</g>", id_start);
    if (g_end == std::string::npos) {
      /* Malformed. */
      return;
    }

    /* Get group id name. */
    const size_t id_end = svg.find("\"", id_start + key.size());
    if (id_end != std::string::npos) {
      std::string id_name = svg.substr(id_start + key.size(), id_end - id_start - key.size());
      /* Replace this group's colors. */
      svg_replace_color_attributes(svg, id_name, g_start, g_end);
    }

    g_start = g_end;
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
                           const IconTextOverlay *text_overlay,
                           const bool inverted = false)
{
  if (icon_id == ICON_NONE) {
    return;
  }

  bTheme *btheme = UI_GetTheme();
  const float fdraw_size = float(draw_size);

  Icon *icon = BKE_icon_get(icon_id);

  if (icon == nullptr) {
    if (G.debug & G_DEBUG) {
      CLOG_WARN(&LOG, "%s: Internal error, no icon for icon ID: %d", __func__, icon_id);
    }
    icon_id = ICON_NOT_FOUND;
    icon = BKE_icon_get(icon_id);
  }

  if (icon->obj_type != ICON_DATA_STUDIOLIGHT) {
    /* Icon alpha should not apply to MatCap/Studio lighting. #80356. */
    alpha *= btheme->tui.icon_alpha;
  }

  /* scale width and height according to aspect */
  int w = int(fdraw_size / aspect + 0.5f);
  int h = int(fdraw_size / aspect + 0.5f);

  DrawInfo *di = icon_ensure_drawinfo(icon);

  /* We need to flush widget base first to ensure correct ordering. */
  UI_widgetbase_draw_cache_flush();

  if (di->type == ICON_TYPE_IMBUF) {
    const ImBuf *ibuf = static_cast<const ImBuf *>(icon->obj);

    GPU_blend(GPU_BLEND_ALPHA_PREMULT);
    icon_draw_rect(x, y, w, h, ibuf->x, ibuf->y, ibuf->byte_buffer.data, alpha, desaturate);
    GPU_blend(GPU_BLEND_ALPHA);
  }
  else if (di->type == ICON_TYPE_VECTOR) {
    /* vector icons use the uiBlock transformation, they are not drawn
     * with untransformed coordinates like the other icons */
    di->data.vector.func(
        x, y, float(draw_size) / aspect, float(draw_size) / aspect, alpha, mono_rgba);
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
    const bool invert = (srgb_to_grayscale_byte(btheme->tui.wcol_toolbar_item.inner) > 128);
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
    icon_draw_rect(x, y, w, h, w, h, ibuf->byte_buffer.data, alpha, desaturate);
    GPU_blend(GPU_BLEND_ALPHA);
  }
  else if (di->type == ICON_TYPE_EVENT) {
    icon_draw_rect_input(x, y, w, h, icon_id, aspect, alpha, inverted);
  }
  else if (ELEM(di->type, ICON_TYPE_SVG_MONO, ICON_TYPE_SVG_COLOR)) {
    /* The alpha may be over 1.0, however `outline_intensity` must be in the [0..1] range. */
    const float outline_intensity = mono_border ?
                                        std::min(1.0f,
                                                 (btheme->tui.icon_border_intensity > 0.0f ?
                                                      btheme->tui.icon_border_intensity :
                                                      0.3f) *
                                                     alpha) :
                                        0.0f;

    float color[4];
    if (icon_id == ICON_NOT_FOUND) {
      UI_GetThemeColor4fv(TH_ERROR, color);
    }
    else if (mono_rgba) {
      rgba_uchar_to_float(color, mono_rgba);
    }
    else {
      UI_GetThemeColor4fv(TH_TEXT, color);
    }

    color[3] *= alpha;

    if (di->type == ICON_TYPE_SVG_COLOR) {
      BLF_draw_svg_icon(uint(icon_id),
                        x,
                        y,
                        float(draw_size) / aspect,
                        color,
                        outline_intensity,
                        true,
                        icon_source_edit_cb);
    }
    else {
      BLF_draw_svg_icon(uint(icon_id),
                        x,
                        y,
                        float(draw_size) / aspect,
                        color,
                        outline_intensity,
                        false,
                        nullptr);
    }

    if (text_overlay && text_overlay->text[0] != '\0') {
      /* Handle the little numbers on top of the icon. */
      uchar text_color[4];
      if (text_overlay->color[3]) {
        copy_v4_v4_uchar(text_color, text_overlay->color);
      }
      else {
        UI_GetThemeColor4ubv(TH_TEXT, text_color);
      }
      const bool is_light = srgb_to_grayscale_byte(text_color) > 96;
      const float zoom_factor = w / UI_ICON_SIZE;
      uiFontStyle fstyle_small = *UI_FSTYLE_WIDGET;
      fstyle_small.points *= zoom_factor * 0.8f;
      fstyle_small.shadow = short(is_light ? FontShadowType::Outline : FontShadowType::None);
      fstyle_small.shadx = 0;
      fstyle_small.shady = 0;
      rcti text_rect = {int(x), int(x + UI_UNIT_X * zoom_factor), int(y), int(y)};
      uiFontStyleDraw_Params params = {UI_STYLE_TEXT_RIGHT, 0};
      UI_fontstyle_draw(&fstyle_small,
                        &text_rect,
                        text_overlay->text,
                        sizeof(text_overlay->text),
                        text_color,
                        &params);
    }
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

    icon_draw_rect(x, y, w, h, iimg->w, iimg->h, iimg->rect, alpha, desaturate);
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
                     pi->w[size],
                     pi->h[size],
                     reinterpret_cast<const uint8_t *>(pi->rect[size]),
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
  if ((pi->flag[size] & PRV_CHANGED) || (!pi->rect[size] && !BKE_previewimg_is_invalid(pi))) {
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
    case ID_OB:
      iconid = UI_icon_from_object_type((Object *)id);
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
    if (ID_IS_PACKED(id)) {
      return ICON_PACKAGE;
    }
    if (id->tag & ID_TAG_MISSING) {
      return ICON_LIBRARY_DATA_BROKEN;
    }
    if (id->tag & ID_TAG_INDIRECT) {
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
    const DynamicPaintSurface *surface = static_cast<const DynamicPaintSurface *>(ptr->data);

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
    case ID_GP:
      return ICON_OUTLINER_DATA_GREASEPENCIL;
    case ID_KE:
      return ICON_SHAPEKEY_DATA;

    /* No icons for these ID-types. */
    case ID_LI:
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
    case OB_MODE_SCULPT_GREASE_PENCIL:
    case OB_MODE_SCULPT_CURVES:
      return ICON_SCULPTMODE_HLT;
    case OB_MODE_VERTEX_PAINT:
    case OB_MODE_VERTEX_GREASE_PENCIL:
      return ICON_VPAINT_HLT;
    case OB_MODE_WEIGHT_PAINT:
    case OB_MODE_WEIGHT_GREASE_PENCIL:
      return ICON_WPAINT_HLT;
    case OB_MODE_TEXTURE_PAINT:
      return ICON_TPAINT_HLT;
    case OB_MODE_PARTICLE_EDIT:
      return ICON_PARTICLEMODE;
    case OB_MODE_POSE:
      return ICON_POSE_HLT;
    case OB_MODE_PAINT_GREASE_PENCIL:
      return ICON_GREASEPENCIL;
  }
  return ICON_NONE;
}

int UI_icon_from_object_type(const Object *object)
{
  switch (object->type) {
    case OB_LAMP:
      return ICON_OUTLINER_OB_LIGHT;
    case OB_MESH:
      return ICON_OUTLINER_OB_MESH;
    case OB_CAMERA:
      return ICON_OUTLINER_OB_CAMERA;
    case OB_CURVES_LEGACY:
      return ICON_OUTLINER_OB_CURVE;
    case OB_MBALL:
      return ICON_OUTLINER_OB_META;
    case OB_LATTICE:
      return ICON_OUTLINER_OB_LATTICE;
    case OB_ARMATURE:
      return ICON_OUTLINER_OB_ARMATURE;
    case OB_FONT:
      return ICON_OUTLINER_OB_FONT;
    case OB_SURF:
      return ICON_OUTLINER_OB_SURFACE;
    case OB_SPEAKER:
      return ICON_OUTLINER_OB_SPEAKER;
    case OB_LIGHTPROBE:
      return ICON_OUTLINER_OB_LIGHTPROBE;
    case OB_CURVES:
      return ICON_OUTLINER_OB_CURVES;
    case OB_POINTCLOUD:
      return ICON_OUTLINER_OB_POINTCLOUD;
    case OB_VOLUME:
      return ICON_OUTLINER_OB_VOLUME;
    case OB_EMPTY:
      if (object->instance_collection && (object->transflag & OB_DUPLICOLLECTION)) {
        return ICON_OUTLINER_OB_GROUP_INSTANCE;
      }
      else if (object->empty_drawtype == OB_EMPTY_IMAGE) {
        return ICON_OUTLINER_OB_IMAGE;
      }
      else if (object->pd && object->pd->forcefield) {
        return ICON_OUTLINER_OB_FORCE_FIELD;
      }
      else {
        return ICON_OUTLINER_OB_EMPTY;
      }
    case OB_GREASE_PENCIL:
      return ICON_OUTLINER_OB_GREASEPENCIL;
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
                     const IconTextOverlay *text_overlay,
                     const bool inverted)
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
                 text_overlay,
                 inverted);
}

ImBuf *UI_svg_icon_bitmap(uint icon_id, float size, bool multicolor)
{
  if (icon_id >= ICON_BLANK_LAST_SVG_ITEM) {
    return nullptr;
  }

  ImBuf *ibuf = nullptr;
  int width;
  int height;
  blender::Array<uchar> bitmap;

  if (multicolor) {
    bitmap = BLF_svg_icon_bitmap(icon_id, size, &width, &height, true, icon_source_edit_cb);
  }
  else {
    bitmap = BLF_svg_icon_bitmap(icon_id, size, &width, &height, false, nullptr);
  }

  if (!bitmap.is_empty()) {
    ibuf = IMB_allocFromBuffer(bitmap.data(), nullptr, width, height, 4);
  }

  if (ibuf) {
    IMB_flipy(ibuf);
    if (multicolor) {
      IMB_premultiply_alpha(ibuf);
    }
  }

  return ibuf;
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

ImBuf *UI_icon_alert_imbuf_get(eAlertIcon icon, float size)
{
#ifdef WITH_HEADLESS
  UNUSED_VARS(icon, size);
  return nullptr;
#else

  int icon_id = ICON_NONE;
  switch (icon) {
    case ALERT_ICON_WARNING:
      icon_id = ICON_WARNING_LARGE;
      break;
    case ALERT_ICON_QUESTION:
      icon_id = ICON_QUESTION_LARGE;
      break;
    case ALERT_ICON_ERROR:
      icon_id = ICON_CANCEL_LARGE;
      break;
    case ALERT_ICON_INFO:
      icon_id = ICON_INFO_LARGE;
      break;
    default:
      icon_id = ICON_NONE;
  }

  if (icon_id == ICON_NONE) {
    return nullptr;
  }

  return UI_svg_icon_bitmap(icon_id, size, false);
#endif
}
