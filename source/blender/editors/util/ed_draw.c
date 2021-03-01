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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edutil
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_image.h"

#include "BLF_api.h"

#include "IMB_imbuf_types.h"
#include "IMB_metadata.h"

#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_util.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "WM_api.h"
#include "WM_types.h"

/**
 * Callback that draws a line between the mouse and a position given as the initial argument.
 */
void ED_region_draw_mouse_line_cb(const bContext *C, ARegion *region, void *arg_info)
{
  wmWindow *win = CTX_wm_window(C);
  const float *mval_src = (float *)arg_info;
  const float mval_dst[2] = {
      win->eventstate->x - region->winrct.xmin,
      win->eventstate->y - region->winrct.ymin,
  };

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  GPU_line_width(1.0f);

  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

  immUniform1i("colors_len", 0); /* "simple" mode */
  immUniformThemeColor3(TH_VIEW_OVERLAY);
  immUniform1f("dash_width", 6.0f);
  immUniform1f("dash_factor", 0.5f);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2fv(shdr_pos, mval_src);
  immVertex2fv(shdr_pos, mval_dst);
  immEnd();

  immUnbindProgram();
}

#define MAX_METADATA_STR 1024

static const char *meta_data_list[] = {
    "File",
    "Strip",
    "Date",
    "RenderTime",
    "Note",
    "Marker",
    "Time",
    "Frame",
    "Camera",
    "Scene",
};

BLI_INLINE bool metadata_is_valid(ImBuf *ibuf, char *r_str, short index, int offset)
{
  return (IMB_metadata_get_field(
              ibuf->metadata, meta_data_list[index], r_str + offset, MAX_METADATA_STR - offset) &&
          r_str[0]);
}

BLI_INLINE bool metadata_is_custom_drawable(const char *field)
{
  /* Metadata field stored by Blender for multilayer EXR images. Is rather
   * useless to be viewed all the time. Can still be seen in the Metadata
   * panel. */
  if (STREQ(field, "BlenderMultiChannel")) {
    return false;
  }
  /* Is almost always has value "scanlineimage", also useless to be seen
   * all the time. */
  if (STREQ(field, "type")) {
    return false;
  }
  return !BKE_stamp_is_known_field(field);
}

typedef struct MetadataCustomDrawContext {
  int fontid;
  int xmin, ymin;
  int vertical_offset;
  int current_y;
} MetadataCustomDrawContext;

static void metadata_custom_draw_fields(const char *field, const char *value, void *ctx_v)
{
  if (!metadata_is_custom_drawable(field)) {
    return;
  }
  MetadataCustomDrawContext *ctx = (MetadataCustomDrawContext *)ctx_v;
  char temp_str[MAX_METADATA_STR];
  BLI_snprintf(temp_str, MAX_METADATA_STR, "%s: %s", field, value);
  BLF_position(ctx->fontid, ctx->xmin, ctx->ymin + ctx->current_y, 0.0f);
  BLF_draw(ctx->fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);
  ctx->current_y += ctx->vertical_offset;
}

static void metadata_draw_imbuf(ImBuf *ibuf, const rctf *rect, int fontid, const bool is_top)
{
  char temp_str[MAX_METADATA_STR];
  int ofs_y = 0;
  const float height = BLF_height_max(fontid);
  const float margin = height / 8;
  const float vertical_offset = (height + margin);

  /* values taking margins into account */
  const float descender = BLF_descender(fontid);
  const float xmin = (rect->xmin + margin);
  const float xmax = (rect->xmax - margin);
  const float ymin = (rect->ymin + margin) - descender;
  const float ymax = (rect->ymax - margin) - descender;

  if (is_top) {
    for (int i = 0; i < 4; i++) {
      /* first line */
      if (i == 0) {
        bool do_newline = false;
        int len = BLI_snprintf_rlen(temp_str, MAX_METADATA_STR, "%s: ", meta_data_list[0]);
        if (metadata_is_valid(ibuf, temp_str, 0, len)) {
          BLF_position(fontid, xmin, ymax - vertical_offset, 0.0f);
          BLF_draw(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);
          do_newline = true;
        }

        len = BLI_snprintf_rlen(temp_str, MAX_METADATA_STR, "%s: ", meta_data_list[1]);
        if (metadata_is_valid(ibuf, temp_str, 1, len)) {
          int line_width = BLF_width(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);
          BLF_position(fontid, xmax - line_width, ymax - vertical_offset, 0.0f);
          BLF_draw(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);
          do_newline = true;
        }

        if (do_newline) {
          ofs_y += vertical_offset;
        }
      } /* Strip */
      else if (i == 1 || i == 2) {
        int len = BLI_snprintf_rlen(temp_str, MAX_METADATA_STR, "%s: ", meta_data_list[i + 1]);
        if (metadata_is_valid(ibuf, temp_str, i + 1, len)) {
          BLF_position(fontid, xmin, ymax - vertical_offset - ofs_y, 0.0f);
          BLF_draw(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);
          ofs_y += vertical_offset;
        }
      } /* Note (wrapped) */
      else if (i == 3) {
        int len = BLI_snprintf_rlen(temp_str, MAX_METADATA_STR, "%s: ", meta_data_list[i + 1]);
        if (metadata_is_valid(ibuf, temp_str, i + 1, len)) {
          struct ResultBLF info;
          BLF_enable(fontid, BLF_WORD_WRAP);
          BLF_wordwrap(fontid, ibuf->x - (margin * 2));
          BLF_position(fontid, xmin, ymax - vertical_offset - ofs_y, 0.0f);
          BLF_draw_ex(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX, &info);
          BLF_wordwrap(fontid, 0);
          BLF_disable(fontid, BLF_WORD_WRAP);
          ofs_y += vertical_offset * info.lines;
        }
      }
      else {
        int len = BLI_snprintf_rlen(temp_str, MAX_METADATA_STR, "%s: ", meta_data_list[i + 1]);
        if (metadata_is_valid(ibuf, temp_str, i + 1, len)) {
          int line_width = BLF_width(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);
          BLF_position(fontid, xmax - line_width, ymax - vertical_offset - ofs_y, 0.0f);
          BLF_draw(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);
          ofs_y += vertical_offset;
        }
      }
    }
  }
  else {
    MetadataCustomDrawContext ctx;
    ctx.fontid = fontid;
    ctx.xmin = xmin;
    ctx.ymin = ymin;
    ctx.current_y = ofs_y;
    ctx.vertical_offset = vertical_offset;
    IMB_metadata_foreach(ibuf, metadata_custom_draw_fields, &ctx);
    int ofs_x = 0;
    ofs_y = ctx.current_y;
    for (int i = 5; i < 10; i++) {
      int len = BLI_snprintf_rlen(temp_str, MAX_METADATA_STR, "%s: ", meta_data_list[i]);
      if (metadata_is_valid(ibuf, temp_str, i, len)) {
        BLF_position(fontid, xmin + ofs_x, ymin + ofs_y, 0.0f);
        BLF_draw(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);

        ofs_x += BLF_width(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX) + UI_UNIT_X;
      }
    }
  }
}

typedef struct MetadataCustomCountContext {
  int count;
} MetadataCustomCountContext;

static void metadata_custom_count_fields(const char *field, const char *UNUSED(value), void *ctx_v)
{
  if (!metadata_is_custom_drawable(field)) {
    return;
  }
  MetadataCustomCountContext *ctx = (MetadataCustomCountContext *)ctx_v;
  ctx->count++;
}

static float metadata_box_height_get(ImBuf *ibuf, int fontid, const bool is_top)
{
  const float height = BLF_height_max(fontid);
  const float margin = (height / 8);
  char str[MAX_METADATA_STR] = "";
  short count = 0;

  if (is_top) {
    if (metadata_is_valid(ibuf, str, 0, 0) || metadata_is_valid(ibuf, str, 1, 0)) {
      count++;
    }
    for (int i = 2; i < 5; i++) {
      if (metadata_is_valid(ibuf, str, i, 0)) {
        if (i == 4) {
          struct {
            struct ResultBLF info;
            rctf rect;
          } wrap;

          BLF_enable(fontid, BLF_WORD_WRAP);
          BLF_wordwrap(fontid, ibuf->x - (margin * 2));
          BLF_boundbox_ex(fontid, str, sizeof(str), &wrap.rect, &wrap.info);
          BLF_wordwrap(fontid, 0);
          BLF_disable(fontid, BLF_WORD_WRAP);

          count += wrap.info.lines;
        }
        else {
          count++;
        }
      }
    }
  }
  else {
    for (int i = 5; i < 10; i++) {
      if (metadata_is_valid(ibuf, str, i, 0)) {
        count = 1;
        break;
      }
    }
    MetadataCustomCountContext ctx;
    ctx.count = 0;
    IMB_metadata_foreach(ibuf, metadata_custom_count_fields, &ctx);
    count += ctx.count;
  }

  if (count) {
    return (height + margin) * count;
  }

  return 0;
}

/* Should be kept in sync with BKE_image_stamp_buf */
void ED_region_image_metadata_draw(
    int x, int y, ImBuf *ibuf, const rctf *frame, float zoomx, float zoomy)
{
  const uiStyle *style = UI_style_get_dpi();

  if (!ibuf->metadata) {
    return;
  }

  /* find window pixel coordinates of origin */
  GPU_matrix_push();

  /* offset and zoom using ogl */
  GPU_matrix_translate_2f(x, y);
  GPU_matrix_scale_2f(zoomx, zoomy);

  BLF_size(blf_mono_font, style->widgetlabel.points * 1.5f * U.pixelsize, U.dpi);

  /* *** upper box*** */

  /* get needed box height */
  float box_y = metadata_box_height_get(ibuf, blf_mono_font, true);

  if (box_y) {
    /* set up rect */
    rctf rect;
    BLI_rctf_init(&rect, frame->xmin, frame->xmax, frame->ymax, frame->ymax + box_y);
    /* draw top box */
    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    immUniformThemeColor(TH_METADATA_BG);
    immRectf(pos, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
    immUnbindProgram();

    BLF_clipping(blf_mono_font, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
    BLF_enable(blf_mono_font, BLF_CLIPPING);

    UI_FontThemeColor(blf_mono_font, TH_METADATA_TEXT);
    metadata_draw_imbuf(ibuf, &rect, blf_mono_font, true);

    BLF_disable(blf_mono_font, BLF_CLIPPING);
  }

  /* *** lower box*** */

  box_y = metadata_box_height_get(ibuf, blf_mono_font, false);

  if (box_y) {
    /* set up box rect */
    rctf rect;
    BLI_rctf_init(&rect, frame->xmin, frame->xmax, frame->ymin - box_y, frame->ymin);
    /* draw top box */
    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    immUniformThemeColor(TH_METADATA_BG);
    immRectf(pos, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
    immUnbindProgram();

    BLF_clipping(blf_mono_font, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
    BLF_enable(blf_mono_font, BLF_CLIPPING);

    UI_FontThemeColor(blf_mono_font, TH_METADATA_TEXT);
    metadata_draw_imbuf(ibuf, &rect, blf_mono_font, false);

    BLF_disable(blf_mono_font, BLF_CLIPPING);
  }

  GPU_matrix_pop();
}

#undef MAX_METADATA_STR
