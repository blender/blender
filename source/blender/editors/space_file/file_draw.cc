/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <string>

#include <fmt/format.h>

#include "MEM_guardedalloc.h"

#include "AS_asset_representation.hh"

#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BIF_glutil.hh"

#include "BKE_blendfile.hh"
#include "BKE_context.hh"
#include "BKE_report.hh"

#include "BLO_readfile.hh"

#include "BLT_translation.hh"

#include "BLF_api.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_metadata.hh"
#include "IMB_thumbs.hh"

#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ED_asset.hh"
#include "ED_fileselect.hh"
#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_state.hh"

#include "filelist.hh"

#include "file_intern.hh" /* own include */

void ED_file_path_button(bScreen *screen,
                         const SpaceFile *sfile,
                         FileSelectParams *params,
                         uiBlock *block)
{
  uiBut *but;

  BLI_assert_msg(params != nullptr,
                 "File select parameters not set. The caller is expected to check this.");

  PointerRNA params_rna_ptr = RNA_pointer_create_discrete(
      &screen->id, &RNA_FileSelectParams, params);

  /* callbacks for operator check functions */
  UI_block_func_set(block, file_draw_check_cb, nullptr, nullptr);

  but = uiDefButR(block,
                  ButType::Text,
                  -1,
                  "",
                  0,
                  0,
                  UI_UNIT_X * 10,
                  UI_UNIT_Y,
                  &params_rna_ptr,
                  "directory",
                  0,
                  0.0f,
                  float(FILE_MAX),
                  TIP_("File path"));

  BLI_assert(!UI_but_flag_is_set(but, UI_BUT_UNDO));
  BLI_assert(!UI_but_is_utf8(but));

  UI_but_func_complete_set(but, autocomplete_directory, nullptr);
  UI_but_funcN_set(but, file_directory_enter_handle, nullptr, but);

  /* TODO: directory editing is non-functional while a library is loaded
   * until this is properly supported just disable it. */
  if (sfile && sfile->files && filelist_lib(sfile->files)) {
    UI_but_flag_enable(but, UI_BUT_DISABLED);
  }

  /* clear func */
  UI_block_func_set(block, nullptr, nullptr, nullptr);
}

struct FileTooltipData {
  const SpaceFile *sfile;
  const FileDirEntry *file;
};

static FileTooltipData *file_tooltip_data_create(const SpaceFile *sfile, const FileDirEntry *file)
{
  FileTooltipData *data = MEM_mallocN<FileTooltipData>(__func__);
  data->sfile = sfile;
  data->file = file;
  return data;
}

static void file_draw_tooltip_custom_func(bContext & /*C*/,
                                          uiTooltipData &tip,
                                          uiBut * /*but*/,
                                          void *argN)
{
  FileTooltipData *file_data = static_cast<FileTooltipData *>(argN);
  const SpaceFile *sfile = file_data->sfile;
  const FileList *files = sfile->files;
  const FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  const FileDirEntry *file = file_data->file;

  BLI_assert_msg(!file->asset, "Asset tooltip should never be overridden here.");

  /* Check the FileDirEntry first to see if the preview is already loaded. */
  ImBuf *thumb = filelist_file_get_preview_image(file);

  /* Only free if it is loaded later. */
  bool free_imbuf = (thumb == nullptr);

  UI_tooltip_text_field_add(tip, file->name, {}, UI_TIP_STYLE_HEADER, UI_TIP_LC_MAIN);
  UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);

  if (!(file->typeflag & FILE_TYPE_BLENDERLIB)) {

    char full_path[FILE_MAX_LIBEXTRA];
    filelist_file_get_full_path(files, file, full_path);

    if (params->recursion_level > 0) {
      char root[FILE_MAX];
      BLI_path_split_dir_part(full_path, root, FILE_MAX);
      UI_tooltip_text_field_add(tip, root, {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL);
    }

    if (file->redirection_path) {
      UI_tooltip_text_field_add(tip,
                                fmt::format("{}: {}", N_("Link target"), file->redirection_path),
                                {},
                                UI_TIP_STYLE_NORMAL,
                                UI_TIP_LC_NORMAL);
    }
    if (file->attributes & FILE_ATTR_OFFLINE) {
      UI_tooltip_text_field_add(
          tip, N_("This file is offline"), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_ALERT);
    }
    if (file->attributes & FILE_ATTR_READONLY) {
      UI_tooltip_text_field_add(
          tip, N_("This file is read-only"), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_ALERT);
    }
    if (file->attributes & (FILE_ATTR_SYSTEM | FILE_ATTR_RESTRICTED)) {
      UI_tooltip_text_field_add(
          tip, N_("This is a restricted system file"), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_ALERT);
    }

    if (file->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) {
      char version_str[128] = {0};
      if (!thumb) {
        /* Load the thumbnail from cache if existing, but don't create if not. */
        thumb = IMB_thumb_read(full_path, THB_LARGE);
      }
      if (thumb) {
        /* Look for version in existing thumbnail if available. */
        IMB_metadata_get_field(
            thumb->metadata, "Thumb::Blender::Version", version_str, sizeof(version_str));
      }

      if (!version_str[0] && !(file->attributes & FILE_ATTR_OFFLINE)) {
        /* Load Blender version directly from the file. */
        short version = BLO_version_from_file(full_path);
        if (version != 0) {
          SNPRINTF_UTF8(version_str, "%d.%01d", version / 100, version % 100);
        }
      }

      if (version_str[0]) {
        UI_tooltip_text_field_add(tip,
                                  fmt::format("Blender {}", version_str),
                                  {},
                                  UI_TIP_STYLE_NORMAL,
                                  UI_TIP_LC_NORMAL);
        UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
      }
    }
    else if (file->typeflag & FILE_TYPE_IMAGE) {
      if (!thumb) {
        /* Load the thumbnail from cache if existing, create if not. */
        thumb = IMB_thumb_manage(full_path, THB_LARGE, THB_SOURCE_IMAGE);
      }
      if (thumb) {
        char value1[128];
        char value2[128];
        if (IMB_metadata_get_field(
                thumb->metadata, "Thumb::Image::Width", value1, sizeof(value1)) &&
            IMB_metadata_get_field(
                thumb->metadata, "Thumb::Image::Height", value2, sizeof(value2)))
        {
          UI_tooltip_text_field_add(tip,
                                    fmt::format("{} \u00D7 {}", value1, value2),
                                    {},
                                    UI_TIP_STYLE_NORMAL,
                                    UI_TIP_LC_NORMAL);
          UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
        }
      }
    }
    else if (file->typeflag & FILE_TYPE_MOVIE) {
      if (!thumb) {
        /* This could possibly take a while. */
        thumb = IMB_thumb_manage(full_path, THB_LARGE, THB_SOURCE_MOVIE);
      }
      if (thumb) {
        char value1[128];
        char value2[128];
        char value3[128];
        if (IMB_metadata_get_field(
                thumb->metadata, "Thumb::Video::Width", value1, sizeof(value1)) &&
            IMB_metadata_get_field(
                thumb->metadata, "Thumb::Video::Height", value2, sizeof(value2)))
        {
          UI_tooltip_text_field_add(tip,
                                    fmt::format("{} \u00D7 {}", value1, value2),
                                    {},
                                    UI_TIP_STYLE_NORMAL,
                                    UI_TIP_LC_NORMAL);
        }
        if (IMB_metadata_get_field(
                thumb->metadata, "Thumb::Video::Frames", value1, sizeof(value1)) &&
            IMB_metadata_get_field(thumb->metadata, "Thumb::Video::FPS", value2, sizeof(value2)) &&
            IMB_metadata_get_field(
                thumb->metadata, "Thumb::Video::Duration", value3, sizeof(value3)))
        {
          UI_tooltip_text_field_add(
              tip,
              fmt::format("{} {} @ {} {}", value1, N_("Frames"), value2, N_("FPS")),
              {},
              UI_TIP_STYLE_NORMAL,
              UI_TIP_LC_NORMAL);
          UI_tooltip_text_field_add(tip,
                                    fmt::format("{} {}", value3, N_("seconds")),
                                    {},
                                    UI_TIP_STYLE_NORMAL,
                                    UI_TIP_LC_NORMAL);
          UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
        }
      }
    }
    else if (file->typeflag & FILE_TYPE_FTFONT) {
      float color[4];
      bTheme *btheme = UI_GetTheme();
      rgba_uchar_to_float(color, btheme->tui.wcol_tooltip.text);
      thumb = IMB_font_preview(full_path,
                               512 * UI_SCALE_FAC,
                               color,
                               TIP_("The five boxing wizards jump quickly! 0123456789"));
      free_imbuf = true;
    }

    char date_str[FILELIST_DIRENTRY_DATE_LEN], time_str[FILELIST_DIRENTRY_TIME_LEN];
    bool is_today, is_yesterday;
    std::string day_string;
    BLI_filelist_entry_datetime_to_string(
        nullptr, file->time, false, time_str, date_str, &is_today, &is_yesterday);
    if (is_today || is_yesterday) {
      day_string = (is_today ? N_("Today") : N_("Yesterday")) + std::string(" ");
    }
    UI_tooltip_text_field_add(tip,
                              fmt::format("{}: {}{}{}",
                                          N_("Modified"),
                                          day_string,
                                          (is_today || is_yesterday) ? "" : date_str,
                                          (is_today || is_yesterday) ? time_str : ""),
                              {},
                              UI_TIP_STYLE_NORMAL,
                              UI_TIP_LC_NORMAL);

    if (!(file->typeflag & FILE_TYPE_DIR) && file->size > 0) {
      char size[16];
      BLI_filelist_entry_size_to_string(nullptr, file->size, false, size);
      if (file->size < 10000) {
        char size_full[BLI_STR_FORMAT_UINT64_GROUPED_SIZE];
        BLI_str_format_uint64_grouped(size_full, file->size);
        UI_tooltip_text_field_add(
            tip,
            fmt::format("{}: {} ({} {})", N_("Size"), size, size_full, N_("bytes")),
            {},
            UI_TIP_STYLE_NORMAL,
            UI_TIP_LC_NORMAL);
      }
      else {
        UI_tooltip_text_field_add(tip,
                                  fmt::format("{}: {}", N_("Size"), size),
                                  {},
                                  UI_TIP_STYLE_NORMAL,
                                  UI_TIP_LC_NORMAL);
      }
    }
  }

  if (thumb && file->typeflag & FILE_TYPE_FTFONT) {
    const float scale = (512.0f * UI_SCALE_FAC) / float(std::max(thumb->x, thumb->y));
    uiTooltipImage image_data;
    image_data.ibuf = thumb;
    image_data.width = short(float(thumb->x) * scale);
    image_data.height = short(float(thumb->y) * scale);
    image_data.background = uiTooltipImageBackground::None;
    image_data.premultiplied = false;
    image_data.text_color = true;
    image_data.border = false;
    UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
    UI_tooltip_image_field_add(tip, image_data);
  }
  else if (thumb && params->display != FILE_IMGDISPLAY) {
    UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
    UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);

    uiTooltipImage image_data;
    float scale = (96.0f * UI_SCALE_FAC) / float(std::max(thumb->x, thumb->y));
    image_data.ibuf = thumb;
    image_data.width = short(float(thumb->x) * scale);
    image_data.height = short(float(thumb->y) * scale);
    image_data.border = true;
    image_data.background = uiTooltipImageBackground::Checkerboard_Themed;
    image_data.premultiplied = true;
    UI_tooltip_image_field_add(tip, image_data);
  }

  if (thumb && free_imbuf) {
    IMB_freeImBuf(thumb);
  }
}

static void file_draw_asset_tooltip_custom_func(bContext & /*C*/,
                                                uiTooltipData &tip,
                                                uiBut * /*but*/,
                                                void *argN)
{
  const auto *asset = static_cast<blender::asset_system::AssetRepresentation *>(argN);
  blender::ed::asset::asset_tooltip(*asset, tip);
}

static void draw_tile_background(const rcti *draw_rect, int colorid, int shade)
{
  float color[4];
  rctf draw_rect_fl;
  BLI_rctf_rcti_copy(&draw_rect_fl, draw_rect);

  UI_GetThemeColorShade4fv(colorid, shade, color);
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_aa(&draw_rect_fl, true, 5.0f, color);
}

static void file_but_enable_drag(uiBut *but,
                                 const SpaceFile *sfile,
                                 const FileDirEntry *file,
                                 const char *path,
                                 const ImBuf *preview_image,
                                 int icon,
                                 float scale)
{
  ID *id;

  if ((id = filelist_file_get_id(file))) {
    UI_but_drag_set_id(but, id);
    if (preview_image) {
      UI_but_drag_attach_image(but, preview_image, scale);
    }
  }
  else if (sfile->browse_mode == FILE_BROWSE_MODE_ASSETS &&
           (file->typeflag & FILE_TYPE_ASSET) != 0)
  {
    const int import_method = ED_fileselect_asset_import_method_get(sfile, file);
    BLI_assert(import_method > -1);
    if (import_method > -1) {
      AssetImportSettings import_settings{};
      import_settings.method = eAssetImportMethod(import_method);
      import_settings.use_instance_collections =
          (sfile->asset_params->import_flags &
           (ELEM(import_method, ASSET_IMPORT_LINK, ASSET_IMPORT_PACK) ?
                FILE_ASSET_IMPORT_INSTANCE_COLLECTIONS_ON_LINK :
                FILE_ASSET_IMPORT_INSTANCE_COLLECTIONS_ON_APPEND)) != 0;

      UI_but_drag_set_asset(but, file->asset, import_settings, icon, file->preview_icon_id);
    }
  }
  else if (preview_image) {
    UI_but_drag_set_image(but, path, icon, preview_image, scale);
  }
  else {
    /* path is no more static, cannot give it directly to but... */
    UI_but_drag_set_path(but, path);
  }
}

static void file_but_tooltip_func_set(const SpaceFile *sfile, const FileDirEntry *file, uiBut *but)
{
  if (file->asset) {
    UI_but_func_tooltip_custom_set(but, file_draw_asset_tooltip_custom_func, file->asset, nullptr);
  }
  else {
    UI_but_func_tooltip_custom_set(
        but, file_draw_tooltip_custom_func, file_tooltip_data_create(sfile, file), MEM_freeN);
  }
}

static uiBut *file_add_icon_but(const SpaceFile *sfile,
                                uiBlock *block,
                                const char * /*path*/,
                                const FileDirEntry *file,
                                const rcti *tile_draw_rect,
                                int icon,
                                int width,
                                int height,
                                int padx,
                                bool dimmed)
{
  uiBut *but;

  const int x = tile_draw_rect->xmin + padx;
  const int y = tile_draw_rect->ymin +
                round_fl_to_int((BLI_rcti_size_y(tile_draw_rect) - height) / 2.0f);

  if (icon < BIFICONID_LAST_STATIC) {
    /* Small built-in icon. Draw centered in given width. */
    but = uiDefIconBut(
        block, ButType::Label, 0, icon, x, y, width, height, nullptr, 0.0f, 0.0f, std::nullopt);
    /* Center the icon. */
    UI_but_drawflag_disable(but, UI_BUT_ICON_LEFT);
  }
  else {
    /* Larger preview icon. Fills available width/height. */
    but = uiDefIconPreviewBut(
        block, ButType::Label, 0, icon, x, y, width, height, nullptr, 0.0f, 0.0f, std::nullopt);
  }
  UI_but_label_alpha_factor_set(but, dimmed ? 0.3f : 1.0f);
  file_but_tooltip_func_set(sfile, file, but);

  return but;
}

static uiBut *file_add_overlay_icon_but(uiBlock *block, int pos_x, int pos_y, int icon)
{
  uiBut *but = uiDefIconBut(block,
                            ButType::Label,
                            0,
                            icon,
                            pos_x,
                            pos_y,
                            ICON_DEFAULT_WIDTH_SCALE,
                            ICON_DEFAULT_HEIGHT_SCALE,
                            nullptr,
                            0.0f,
                            0.0f,
                            std::nullopt);
  /* Otherwise a left hand padding will be added. */
  UI_but_drawflag_disable(but, UI_BUT_ICON_LEFT);
  UI_but_label_alpha_factor_set(but, 0.6f);
  const uchar light[4] = {255, 255, 255, 255};
  UI_but_color_set(but, light);

  return but;
}

static void file_draw_string(int sx,
                             int sy,
                             const char *string,
                             float width,
                             int height,
                             eFontStyle_Align align,
                             const uchar col[4])
{
  uiFontStyle fs;
  rcti rect;
  char filename[FILE_MAXFILE];

  if (string[0] == '\0' || width < 1) {
    return;
  }

  const uiStyle *style = UI_style_get();
  fs = style->widget;

  STRNCPY(filename, string);
  UI_text_clip_middle_ex(&fs, filename, width, UI_ICON_SIZE, sizeof(filename), '\0');

  /* no text clipping needed, UI_fontstyle_draw does it but is a bit too strict
   * (for buttons it works) */
  rect.xmin = sx;
  rect.xmax = sx + round_fl_to_int(width);
  rect.ymin = sy - height;
  rect.ymax = sy;

  uiFontStyleDraw_Params font_style_params{};
  font_style_params.align = align;

  UI_fontstyle_draw(&fs, &rect, filename, sizeof(filename), col, &font_style_params);
}

/**
 * Draw the string over at max \a line_count lines, clipping in the middle so it fits.
 */
static void file_draw_string_mulitline_clipped(const rcti *rect,
                                               const char *string,
                                               eFontStyle_Align align,
                                               const uchar col[4])
{
  if (string[0] == '\0' || BLI_rcti_size_x(rect) < 1) {
    return;
  }

  const uiStyle *style = UI_style_get();
  uiFontStyle fs = style->widget;

  UI_fontstyle_draw_multiline_clipped(&fs, rect, string, col, align);
}

/**
 * \param r_sx, r_sy: The lower right corner of the last line drawn, plus the height of the last
 *                    line. This is the cursor position on completion to allow drawing more text
 *                    behind that.
 */
static void file_draw_string_multiline(int sx,
                                       int sy,
                                       const char *string,
                                       int wrap_width,
                                       int line_height,
                                       const uchar text_col[4],
                                       int *r_sx,
                                       int *r_sy)
{
  rcti rect;

  if (string[0] == '\0' || wrap_width < 1) {
    return;
  }

  const uiStyle *style = UI_style_get();
  int font_id = style->widget.uifont_id;
  int len = strlen(string);

  rcti textbox;
  BLF_wordwrap(font_id, wrap_width);
  BLF_enable(font_id, BLF_WORD_WRAP);
  BLF_boundbox(font_id, string, len, &textbox);
  BLF_disable(font_id, BLF_WORD_WRAP);

  /* no text clipping needed, UI_fontstyle_draw does it but is a bit too strict
   * (for buttons it works) */
  rect.xmin = sx;
  rect.xmax = sx + wrap_width;
  /* Need to increase the clipping rect by one more line, since the #UI_fontstyle_draw_ex() will
   * actually start drawing at (ymax - line-height). */
  rect.ymin = sy - BLI_rcti_size_y(&textbox) - line_height;
  rect.ymax = sy;

  uiFontStyleDraw_Params font_style_params{};
  font_style_params.align = UI_STYLE_TEXT_LEFT;
  font_style_params.word_wrap = true;

  ResultBLF result;
  UI_fontstyle_draw_ex(
      &style->widget, &rect, string, len, text_col, &font_style_params, nullptr, nullptr, &result);
  if (r_sx) {
    *r_sx = result.width;
  }
  if (r_sy) {
    *r_sy = rect.ymin + line_height;
  }
}

void file_calc_previews(const bContext *C, ARegion *region)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  View2D *v2d = &region->v2d;

  ED_fileselect_init_layout(sfile, region);
  UI_view2d_totRect_set(v2d, sfile->layout->width, sfile->layout->height);
}

static std::tuple<int, int, float> preview_image_scaled_dimensions_get(const int image_width,
                                                                       const int image_height,
                                                                       const FileLayout &layout)
{
  const float ui_imbx = image_width * UI_SCALE_FAC;
  const float ui_imby = image_height * UI_SCALE_FAC;

  float scale;
  float scaledx, scaledy;
  if (((ui_imbx > layout.prv_w) || (ui_imby > layout.prv_h)) ||
      ((ui_imbx < layout.prv_w) || (ui_imby < layout.prv_h)))
  {
    if (image_width > image_height) {
      scaledx = float(layout.prv_w);
      scaledy = (float(image_height) / float(image_width)) * layout.prv_w;
      scale = scaledx / image_width;
    }
    else {
      scaledy = float(layout.prv_h);
      scaledx = (float(image_width) / float(image_height)) * layout.prv_h;
      scale = scaledy / image_height;
    }
  }
  else {
    scaledx = ui_imbx;
    scaledy = ui_imby;
    scale = UI_SCALE_FAC;
  }

  return std::make_tuple(int(scaledx), int(scaledy), scale);
}

static void file_add_preview_drag_but(const SpaceFile *sfile,
                                      uiBlock *block,
                                      FileLayout *layout,
                                      const FileDirEntry *file,
                                      const char *path,
                                      const rcti *tile_draw_rect,
                                      const ImBuf *preview_image,
                                      const int file_type_icon)
{
  /* Invisible button for dragging. */
  rcti drag_rect = *tile_draw_rect;
  /* A bit smaller than the full tile, to increase the gap between items that users can drag from
   * for box select. */
  BLI_rcti_pad(&drag_rect, -layout->tile_border_x, -layout->tile_border_y);

  uiBut *but = uiDefBut(block,
                        ButType::Label,
                        0,
                        "",
                        drag_rect.xmin,
                        drag_rect.ymin,
                        BLI_rcti_size_x(&drag_rect),
                        BLI_rcti_size_y(&drag_rect),
                        nullptr,
                        0.0,
                        0.0,
                        std::nullopt);

  const ImBuf *drag_image = preview_image ? preview_image :
                                            /* Larger directory or document icon. */
                                            filelist_geticon_special_file_image_ex(file);
  const float scale = (PREVIEW_DRAG_DRAW_SIZE * UI_SCALE_FAC) /
                      std::max(drag_image->x, drag_image->y);
  file_but_enable_drag(but, sfile, file, path, drag_image, file_type_icon, scale);
  file_but_tooltip_func_set(sfile, file, but);
}

static void file_draw_preview(const FileDirEntry *file,
                              const rcti *tile_draw_rect,
                              const ImBuf *imb,
                              FileLayout *layout,
                              const bool dimmed)
{
  BLI_assert(imb != nullptr);

  const auto [scaled_width, scaled_height, scale] = preview_image_scaled_dimensions_get(
      imb->x, imb->y, *layout);

  /* Additional offset to keep the scaled image centered. Difference between maximum
   * width/height and the actual width/height, divided by two for centering. */
  const float ofs_x = (float(layout->prv_w) - float(scaled_width)) / 2.0f;
  const float ofs_y = (float(layout->prv_h) - float(scaled_height)) / 2.0f;
  const int xmin = tile_draw_rect->xmin + layout->prv_border_x + int(ofs_x + 0.5f);
  const int ymin = tile_draw_rect->ymax - layout->prv_border_y - layout->prv_h + int(ofs_y + 0.5f);

  GPU_blend(GPU_BLEND_ALPHA);

  float document_img_col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  if (file->typeflag & FILE_TYPE_FTFONT) {
    UI_GetThemeColor4fv(TH_TEXT, document_img_col);
  }
  if (dimmed) {
    document_img_col[3] *= 0.3f;
  }

  if (ELEM(file->typeflag, FILE_TYPE_IMAGE, FILE_TYPE_OBJECT_IO)) {
    /* Draw checker pattern behind image previews in case they have transparency. */
    imm_draw_box_checker_2d(
        float(xmin), float(ymin), float(xmin + scaled_width), float(ymin + scaled_height));
  }

  if (file->typeflag & FILE_TYPE_BLENDERLIB) {
    /* Datablock preview images use premultiplied alpha. */
    GPU_blend(GPU_BLEND_ALPHA_PREMULT);
  }

  IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_3D_IMAGE_COLOR);
  immDrawPixelsTexTiled_scaling(&state,
                                float(xmin),
                                float(ymin),
                                imb->x,
                                imb->y,
                                blender::gpu::TextureFormat::UNORM_8_8_8_8,
                                true,
                                imb->byte_buffer.data,
                                scale,
                                scale,
                                1.0f,
                                1.0f,
                                document_img_col);

  const bool show_outline = (file->typeflag & (FILE_TYPE_IMAGE | FILE_TYPE_OBJECT_IO |
                                               FILE_TYPE_MOVIE | FILE_TYPE_BLENDER));
  /* Contrasting outline around some preview types. */
  if (show_outline) {
    GPU_blend(GPU_BLEND_ALPHA);

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    float border_color[4] = {1.0f, 1.0f, 1.0f, 0.15f};
    float bgcolor[4];
    UI_GetThemeColor4fv(TH_BACK, bgcolor);
    if (srgb_to_grayscale(bgcolor) > 0.5f) {
      border_color[0] = 0.0f;
      border_color[1] = 0.0f;
      border_color[2] = 0.0f;
    }
    immUniformColor4fv(border_color);
    imm_draw_box_wire_2d(pos,
                         float(xmin),
                         float(ymin),
                         float(xmin + scaled_width + 1),
                         float(ymin + scaled_height + 1));
    immUnbindProgram();
  }

  GPU_blend(GPU_BLEND_NONE);
}

static void file_draw_special_image(const FileDirEntry *file,
                                    const rcti *tile_draw_rect,
                                    const int file_type_icon,
                                    const float icon_aspect,
                                    const FileLayout *layout,
                                    const bool dimmed)
{
  float document_img_col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  if (file->typeflag & FILE_TYPE_DIR) {
    UI_GetThemeColor4fv(TH_ICON_FOLDER, document_img_col);
  }
  else {
    UI_GetThemeColor4fv(TH_TEXT, document_img_col);
  }

  if (dimmed) {
    document_img_col[3] *= 0.3f;
  }

  GPU_blend(GPU_BLEND_ALPHA);

  const int cent_x = tile_draw_rect->xmin + layout->prv_border_x + (layout->prv_w / 2.0f) + 0.5f;
  const int cent_y = tile_draw_rect->ymax - layout->prv_border_y - (layout->prv_h / 2.0f) + 0.5f;
  const float aspect = icon_aspect / UI_SCALE_FAC;

  {
    /* Draw large folder or document icon. */
    const int icon_large = (file->typeflag & FILE_TYPE_DIR) ? ICON_FILE_FOLDER_LARGE :
                                                              ICON_FILE_LARGE;

    uchar icon_col[4];
    rgba_float_to_uchar(icon_col, document_img_col);

    const float scale = 4.0f;
    const float ofs_y = (file->typeflag & FILE_TYPE_DIR ? -0.02f : 0.0f) * layout->prv_h;

    UI_icon_draw_ex(cent_x - (ICON_DEFAULT_WIDTH * scale / aspect / 2.0f),
                    cent_y - (ICON_DEFAULT_HEIGHT * scale / aspect / 2.0f) + ofs_y,
                    icon_large,
                    icon_aspect / UI_SCALE_FAC / scale,
                    document_img_col[3],
                    0.0f,
                    icon_col,
                    false,
                    UI_NO_ICON_OVERLAY_TEXT);
  }

  if (file_type_icon) {
    /* Small icon in the middle of large image, scaled to fit container and UI scale */
    float icon_opacity = 0.4f;
    uchar icon_color[4] = {0, 0, 0, 255};
    if (srgb_to_grayscale(document_img_col) < 0.5f) {
      icon_color[0] = 255;
      icon_color[1] = 255;
      icon_color[2] = 255;
    }

    const float scale = file->typeflag & FILE_TYPE_DIR ? 1.5f : 2.0f;
    const float ofs_y = (file->typeflag & FILE_TYPE_DIR ? -0.035f : -0.135f) * layout->prv_h;

    UI_icon_draw_ex(cent_x - (ICON_DEFAULT_WIDTH * scale / aspect / 2.0f),
                    cent_y - (ICON_DEFAULT_HEIGHT * scale / aspect / 2.0f) + ofs_y,
                    file_type_icon,
                    icon_aspect / UI_SCALE_FAC / scale,
                    icon_opacity,
                    0.0f,
                    icon_color,
                    false,
                    UI_NO_ICON_OVERLAY_TEXT);
  }

  GPU_blend(GPU_BLEND_NONE);
}

static void file_draw_loading_icon(const rcti *tile_draw_rect,
                                   const float preview_icon_aspect,
                                   const FileLayout *layout)
{
  uchar icon_color[4] = {0, 0, 0, 255};
  /* Contrast with background since we are not showing the large document image. */
  UI_GetThemeColor4ubv(TH_TEXT, icon_color);

  const int cent_x = tile_draw_rect->xmin + layout->prv_border_x + (layout->prv_w / 2.0f) + 0.5f;
  const int cent_y = tile_draw_rect->ymax - layout->prv_border_y - (layout->prv_h / 2.0f) + 0.5f;
  const float aspect = preview_icon_aspect / UI_SCALE_FAC;

  UI_icon_draw_ex(cent_x - (ICON_DEFAULT_WIDTH / aspect / 2.0f),
                  cent_y - (ICON_DEFAULT_HEIGHT / aspect / 2.0f),
                  ICON_PREVIEW_LOADING,
                  aspect,
                  1.0f,
                  0.0f,
                  icon_color,
                  false,
                  UI_NO_ICON_OVERLAY_TEXT);
}

static void file_draw_indicator_icons(const FileList *files,
                                      const FileDirEntry *file,
                                      const FileLayout *layout,
                                      const rcti *tile_draw_rect,
                                      const float preview_icon_aspect,
                                      const int file_type_icon,
                                      const bool has_special_file_image)
{
  const bool is_offline = (file->attributes & FILE_ATTR_OFFLINE);
  const bool is_link = (file->attributes & FILE_ATTR_ANY_LINK);
  const bool is_loading = filelist_file_is_preview_pending(files, file);

  /* Don't draw these icons if the preview image is small. They are just indicators and shouldn't
   * cover the preview. */
  if (preview_icon_aspect < 2.0f) {
    const float icon_x = float(tile_draw_rect->xmin) + (3.0f * UI_SCALE_FAC);
    const float icon_y = float(tile_draw_rect->ymax) - layout->prv_border_y - layout->prv_h;
    const uchar light[4] = {255, 255, 255, 255};
    if (is_offline) {
      /* Icon at bottom to indicate the file is offline. */
      UI_icon_draw_ex(icon_x,
                      icon_y,
                      ICON_INTERNET,
                      1.0f / UI_SCALE_FAC,
                      0.6f,
                      0.0f,
                      light,
                      true,
                      UI_NO_ICON_OVERLAY_TEXT);
    }
    else if (is_link) {
      /* Icon at bottom to indicate it is a shortcut, link, or alias. */
      UI_icon_draw_ex(icon_x,
                      icon_y,
                      ICON_FILE_ALIAS,
                      1.0f / UI_SCALE_FAC,
                      0.6f,
                      0.0f,
                      nullptr,
                      false,
                      UI_NO_ICON_OVERLAY_TEXT);
    }
    else if (file_type_icon) {
      /* Smaller, fainter type icon at bottom-left.
       *
       * Always draw while loading, the preview shows a loading icon and doesn't indicate the type
       * yet then. After loading, the special file image may already draw the type icon in
       * #file_draw_preview(), don't draw it again here. Also don't draw it for font files, they
       * render a font preview already, the type indicator would be redundant.
       */
      if (is_loading || !(has_special_file_image || (file->typeflag & FILE_TYPE_FTFONT))) {
        UI_icon_draw_ex(icon_x,
                        icon_y,
                        file_type_icon,
                        1.0f / UI_SCALE_FAC,
                        0.6f,
                        0.0f,
                        light,
                        true,
                        UI_NO_ICON_OVERLAY_TEXT);
      }
    }
  }

  const bool is_current_main_data = filelist_file_get_id(file) != nullptr;
  if (is_current_main_data) {
    /* Smaller, fainter icon at the top-right indicating that the file represents data from the
     * current file (from current #Main in fact). */
    float icon_x, icon_y;
    const uchar light[4] = {255, 255, 255, 255};
    icon_x = float(tile_draw_rect->xmax) - (16.0f * UI_SCALE_FAC);
    icon_y = float(tile_draw_rect->ymax) - (20.0f * UI_SCALE_FAC);
    UI_icon_draw_ex(icon_x,
                    icon_y,
                    ICON_CURRENT_FILE,
                    1.0f / UI_SCALE_FAC,
                    0.6f,
                    0.0f,
                    light,
                    true,
                    UI_NO_ICON_OVERLAY_TEXT);
  }
}

static void renamebutton_cb(bContext *C, void * /*arg1*/, char *oldname)
{
  char newname[FILE_MAX + 12];
  char orgname[FILE_MAX + 12];
  char filename[FILE_MAX + 12];
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  SpaceFile *sfile = (SpaceFile *)CTX_wm_space_data(C);
  ARegion *region = CTX_wm_region(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  BLI_path_join(orgname, sizeof(orgname), params->dir, oldname);
  STRNCPY(filename, params->renamefile);
  BLI_path_make_safe_filename(filename);
  BLI_path_join(newname, sizeof(newname), params->dir, filename);

  if (!STREQ(orgname, newname)) {
    errno = 0;
    if ((BLI_rename(orgname, newname) != 0) || !BLI_exists(newname)) {
      WM_global_reportf(
          RPT_ERROR, "Could not rename: %s", errno ? strerror(errno) : "unknown error");
      WM_report_banner_show(wm, win);
      /* Renaming failed, reset the name for further renaming handling. */
      STRNCPY(params->renamefile, oldname);
    }
    else {
      /* If rename is successful, set renamefile to newly renamed entry.
       * This is used later to select and scroll to the file.
       */
      STRNCPY(params->renamefile, filename);
    }

    /* Ensure we select and scroll to the renamed file.
     * This is done even if the rename fails as we want to make sure that the file we tried to
     * rename is still selected and in view. (it can move if something added files/folders to the
     * directory while we were renaming.
     */
    file_params_invoke_rename_postscroll(wm, win, sfile);
    /* to make sure we show what is on disk */
    ED_fileselect_clear(wm, sfile);
    ED_region_tag_redraw(region);
  }
}

static void draw_background(FileLayout *layout, View2D *v2d)
{
  const int item_height = layout->tile_h + (2 * layout->tile_border_y);
  int i;
  int sy;

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  float col_alternating[4];
  UI_GetThemeColor4fv(TH_ROW_ALTERNATE, col_alternating);
  immUniformThemeColorBlend(TH_BACK, TH_ROW_ALTERNATE, col_alternating[3]);

  /* alternating flat shade background */
  for (i = 2; (i <= layout->rows + 1); i += 2) {
    sy = int(v2d->cur.ymax) - layout->offset_top - i * item_height - layout->list_padding_top;

    /* Offset pattern slightly to add scroll effect. */
    sy += round_fl_to_int(item_height * (v2d->tot.ymax - v2d->cur.ymax) / item_height);

    immRectf(pos,
             v2d->cur.xmin,
             float(sy),
             v2d->cur.xmax,
             float(sy + layout->tile_h + 2 * layout->tile_border_y));
  }

  immUnbindProgram();
}

static void draw_dividers(FileLayout *layout, View2D *v2d)
{
  /* vertical column dividers */

  const int step = (layout->tile_w + 2 * layout->tile_border_x);

  uint vertex_len = 0;
  int sx = int(v2d->tot.xmin);
  while (sx < v2d->cur.xmax) {
    sx += step;
    vertex_len += 4; /* vertex_count = 2 points per line * 2 lines per divider */
  }

  if (vertex_len > 0) {
    float v1[2], v2[2];
    float col_hi[3], col_lo[3];

    UI_GetThemeColorShade3fv(TH_BACK, 30, col_hi);
    UI_GetThemeColorShade3fv(TH_BACK, -30, col_lo);

    v1[1] = v2d->cur.ymax - layout->tile_border_y;
    v2[1] = v2d->cur.ymin;

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
    uint color = GPU_vertformat_attr_add(
        format, "color", blender::gpu::VertAttrType::SFLOAT_32_32_32);

    immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);
    immBegin(GPU_PRIM_LINES, vertex_len);

    sx = int(v2d->tot.xmin);
    while (sx < v2d->cur.xmax) {
      sx += step;

      v1[0] = v2[0] = sx;
      immAttrSkip(color);
      immVertex2fv(pos, v1);
      immAttr3fv(color, col_lo);
      immVertex2fv(pos, v2);

      v1[0] = v2[0] = sx + 1;
      immAttrSkip(color);
      immVertex2fv(pos, v1);
      immAttr3fv(color, col_hi);
      immVertex2fv(pos, v2);
    }

    immEnd();
    immUnbindProgram();
  }
}

static void draw_columnheader_background(const FileLayout *layout, const View2D *v2d)
{
  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformThemeColorShade(TH_BACK, 11);

  immRectf(pos,
           v2d->cur.xmin,
           v2d->cur.ymax - layout->attribute_column_header_h,
           v2d->cur.xmax,
           v2d->cur.ymax);

  immUnbindProgram();
}

static void draw_columnheader_columns(const FileSelectParams *params,
                                      FileLayout *layout,
                                      const View2D *v2d,
                                      const uchar text_col[4])
{
  const float divider_pad = 0.2 * layout->attribute_column_header_h;
  int sx = v2d->cur.xmin, sy = v2d->cur.ymax;

  for (int column_type = 0; column_type < ATTRIBUTE_COLUMN_MAX; column_type++) {
    if (!file_attribute_column_type_enabled(params, FileAttributeColumnType(column_type), layout))
    {
      continue;
    }
    const FileAttributeColumn *column = &layout->attribute_columns[column_type];

    /* Active sort type triangle */
    if (params->sort == column->sort_type) {
      float tri_color[4];

      rgba_uchar_to_float(tri_color, text_col);
      UI_draw_icon_tri(sx + column->width - (0.3f * U.widget_unit) -
                           ATTRIBUTE_COLUMN_PADDING / 2.0f,
                       sy + (0.1f * U.widget_unit) - (layout->attribute_column_header_h / 2),
                       (params->flag & FILE_SORT_INVERT) ? 't' : 'v',
                       tri_color);
    }

    file_draw_string(sx + ATTRIBUTE_COLUMN_PADDING,
                     sy - layout->tile_border_y,
                     IFACE_(column->name),
                     column->width - 2 * ATTRIBUTE_COLUMN_PADDING,
                     layout->attribute_column_header_h - layout->tile_border_y,
                     UI_STYLE_TEXT_LEFT,
                     text_col);

    /* Separator line */
    if (column_type != COLUMN_NAME) {
      uint pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      immUniformThemeColorShade(TH_BACK, -10);
      immBegin(GPU_PRIM_LINES, 2);
      immVertex2f(pos, sx - 1, sy - divider_pad);
      immVertex2f(pos, sx - 1, sy - layout->attribute_column_header_h + divider_pad);
      immEnd();
      immUnbindProgram();
    }

    sx += column->width;
  }

  /* Vertical separator lines line */
  {
    uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformThemeColorShade(TH_BACK, -10);
    immBegin(GPU_PRIM_LINES, 4);
    immVertex2f(pos, v2d->cur.xmin, sy);
    immVertex2f(pos, v2d->cur.xmax, sy);
    immVertex2f(pos, v2d->cur.xmin, sy - layout->attribute_column_header_h);
    immVertex2f(pos, v2d->cur.xmax, sy - layout->attribute_column_header_h);
    immEnd();
    immUnbindProgram();
  }
}

/**
 * Updates the stat string stored in file->entry if necessary.
 */
static const char *filelist_get_details_column_string(
    FileAttributeColumnType column,
    /* Generated string will be cached in the file, so non-const. */
    FileDirEntry *file,
    const bool compact,
    const bool update_stat_strings)
{
  switch (column) {
    case COLUMN_DATETIME:
      if (!(file->typeflag & FILE_TYPE_BLENDERLIB) && !FILENAME_IS_CURRPAR(file->relpath)) {
        if (file->draw_data.datetime_str[0] == '\0' || update_stat_strings) {
          char date[FILELIST_DIRENTRY_DATE_LEN], time[FILELIST_DIRENTRY_TIME_LEN];
          bool is_today, is_yesterday;

          BLI_filelist_entry_datetime_to_string(
              nullptr, file->time, compact, time, date, &is_today, &is_yesterday);

          if (!compact && (is_today || is_yesterday)) {
            STRNCPY_UTF8(date, is_today ? IFACE_("Today") : IFACE_("Yesterday"));
          }
          SNPRINTF_UTF8(file->draw_data.datetime_str, compact ? "%s" : "%s %s", date, time);
        }

        return file->draw_data.datetime_str;
      }
      break;
    case COLUMN_SIZE:
      if ((file->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) ||
          !(file->typeflag & (FILE_TYPE_DIR | FILE_TYPE_BLENDERLIB)))
      {
        if (file->draw_data.size_str[0] == '\0' || update_stat_strings) {
          BLI_filelist_entry_size_to_string(
              nullptr, file->size, compact, file->draw_data.size_str);
        }

        return file->draw_data.size_str;
      }
      break;
    default:
      break;
  }

  return nullptr;
}

static void draw_details_columns(const FileSelectParams *params,
                                 const FileLayout *layout,
                                 FileDirEntry *file,
                                 const rcti *tile_draw_rect,
                                 const uchar text_col[4])
{
  const bool compact = FILE_LAYOUT_COMPACT(layout);
  const bool update_stat_strings = layout->width != layout->curr_size;
  int sx = tile_draw_rect->xmin - layout->tile_border_x;

  for (int column_type = 0; column_type < ATTRIBUTE_COLUMN_MAX; column_type++) {
    const FileAttributeColumn *column = &layout->attribute_columns[column_type];

    /* Name column is not a detail column (should already be drawn), always skip here. */
    if (column_type == COLUMN_NAME) {
      sx += column->width;
      continue;
    }
    if (!file_attribute_column_type_enabled(params, FileAttributeColumnType(column_type), layout))
    {
      continue;
    }

    const char *str = filelist_get_details_column_string(
        FileAttributeColumnType(column_type), file, compact, update_stat_strings);

    if (str) {
      file_draw_string(sx + ATTRIBUTE_COLUMN_PADDING,
                       tile_draw_rect->ymax,
                       IFACE_(str),
                       column->width - 2 * ATTRIBUTE_COLUMN_PADDING,
                       layout->tile_h,
                       eFontStyle_Align(column->text_align),
                       text_col);
    }

    sx += column->width;
  }
}

static rcti tile_draw_rect_get(const View2D *v2d, const FileLayout *layout, const int file_idx)
{
  int tile_pos_x, tile_pos_y;
  ED_fileselect_layout_tilepos(layout, file_idx, &tile_pos_x, &tile_pos_y);
  tile_pos_x += int(v2d->tot.xmin);
  tile_pos_y = int(v2d->tot.ymax - tile_pos_y);

  rcti rect;
  rect.xmin = tile_pos_x;
  rect.xmax = rect.xmin + layout->tile_w;
  rect.ymax = tile_pos_y;
  rect.ymin = rect.ymax - layout->tile_h;

  return rect;
}

/**
 * Get the boundaries to display the name label in (this isn't the rectangle of the text itself).
 */
static rcti text_draw_rect_get(const View2D *v2d,
                               const eFileDisplayType display_type,
                               const FileLayout *layout,
                               const int file_idx,
                               const int icon_ofs_x)
{
  rcti tile_rect = tile_draw_rect_get(v2d, layout, file_idx);

  rcti rect = tile_rect;
  if (display_type == FILE_IMGDISPLAY) {
    rect.ymin += round_fl_to_int(layout->prv_border_y * 0.5f);
    rect.ymax = rect.ymin + layout->text_line_height * layout->text_lines_count;
  }
  else {
    rect.xmin += icon_ofs_x + 1;
    rect.xmax = tile_rect.xmin + round_fl_to_int(layout->attribute_columns[COLUMN_NAME].width) -
                layout->tile_border_x;
  }

  return rect;
}

void file_draw_list(const bContext *C, ARegion *region)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  FileLayout *layout = ED_fileselect_get_layout(sfile, region);
  View2D *v2d = &region->v2d;
  FileList *files = sfile->files;
  FileDirEntry *file;
  uiBlock *block = UI_block_begin(C, region, __func__, blender::ui::EmbossType::Emboss);
  int numfiles;
  int numfiles_layout;
  int offset;
  int i;
  eFontStyle_Align align;
  bool do_drag;
  uchar text_col[4];
  const bool draw_columnheader = (params->display == FILE_VERTICALDISPLAY);
  const float thumb_icon_aspect = std::min(64.0f / float(params->thumbnail_size), 4.0f);

  numfiles = filelist_files_ensure(files);

  if (params->display != FILE_IMGDISPLAY) {
    draw_background(layout, v2d);
    draw_dividers(layout, v2d);
  }

  offset = ED_fileselect_layout_offset(
      layout, int(region->v2d.cur.xmin), int(-region->v2d.cur.ymax) + layout->offset_top);
  offset = std::max(offset, 0);

  numfiles_layout = ED_fileselect_layout_numfiles(layout, region);

  /* adjust, so the next row is already drawn when scrolling */
  if (layout->flag & FILE_LAYOUT_HOR) {
    numfiles_layout += layout->rows;
  }
  else {
    numfiles_layout += layout->flow_columns;
  }

  filelist_file_cache_slidingwindow_set(files, numfiles_layout);

  align = (FILE_IMGDISPLAY == params->display) ? UI_STYLE_TEXT_CENTER : UI_STYLE_TEXT_LEFT;

  if (numfiles > 0) {
    const bool success = filelist_file_cache_block(
        files, min_ii(offset + (numfiles_layout / 2), numfiles - 1));
    BLI_assert(success);
    UNUSED_VARS_NDEBUG(success);

    filelist_cache_previews_update(files);

    /* Handle preview timer here,
     * since it's filelist_file_cache_block() and filelist_cache_previews_update()
     * which controls previews task. */
    {
      const bool previews_running = filelist_cache_previews_running(files) &&
                                    !filelist_cache_previews_done(files);
      //          printf("%s: preview task: %d\n", __func__, previews_running);
      if (previews_running && !sfile->previews_timer) {
        sfile->previews_timer = WM_event_timer_add_notifier(
            wm, win, NC_SPACE | ND_SPACE_FILE_PREVIEW, 0.01);
      }
      if (!previews_running && sfile->previews_timer) {
        /* Preview is not running, no need to keep generating update events! */
        //              printf("%s: Inactive preview task, sleeping!\n", __func__);
        WM_event_timer_remove_notifier(wm, win, sfile->previews_timer);
        sfile->previews_timer = nullptr;
      }
    }
  }

  BLF_batch_draw_begin();

  UI_GetThemeColor4ubv(TH_TEXT, text_col);

  for (i = offset; (i < numfiles) && (i < offset + numfiles_layout); i++) {
    eDirEntry_SelectFlag file_selflag;
    const int padx = 0.1f * UI_UNIT_X;
    int icon_ofs = 0;

    const rcti tile_draw_rect = tile_draw_rect_get(v2d, layout, i);

    file = filelist_file(files, i);
    file_selflag = filelist_entry_select_get(sfile->files, file, CHECK_ALL);

    char path[FILE_MAX_LIBEXTRA];
    filelist_file_get_full_path(files, file, path);

    if (!(file_selflag & FILE_SEL_EDITING)) {
      if ((params->highlight_file == i) || (file_selflag & FILE_SEL_HIGHLIGHTED) ||
          (file_selflag & FILE_SEL_SELECTED))
      {
        int colorid = (file_selflag & FILE_SEL_SELECTED) ? TH_HILITE : TH_BACK;
        int shade = (params->highlight_file == i) || (file_selflag & FILE_SEL_HIGHLIGHTED) ? 35 :
                                                                                             0;
        BLI_assert(i == 0 || !FILENAME_IS_CURRPAR(file->relpath));

        draw_tile_background(&tile_draw_rect, colorid, shade);
      }
    }
    UI_draw_roundbox_corner_set(UI_CNR_NONE);

    /* don't drag parent or refresh items */
    do_drag = !FILENAME_IS_CURRPAR(file->relpath);
    const bool is_hidden = (file->attributes & FILE_ATTR_HIDDEN);

    if (FILE_IMGDISPLAY == params->display) {
      const int file_type_icon = filelist_geticon_file_type(files, i, false);
      const ImBuf *preview_imb = filelist_get_preview_image(files, i);

      bool has_special_file_image = false;

      const bool is_loading = filelist_file_is_preview_pending(files, file);
      if (is_loading) {
        file_draw_loading_icon(&tile_draw_rect, thumb_icon_aspect, layout);
      }
      else if (preview_imb) {
        file_draw_preview(file, &tile_draw_rect, preview_imb, layout, is_hidden);
      }
      else {
        /* Larger folder or document icon, with file/folder type icon in the middle (if any). */
        file_draw_special_image(
            file, &tile_draw_rect, file_type_icon, thumb_icon_aspect, layout, is_hidden);
        has_special_file_image = true;
      }

      file_draw_indicator_icons(files,
                                file,
                                layout,
                                &tile_draw_rect,
                                thumb_icon_aspect,
                                file_type_icon,
                                has_special_file_image);

      if (do_drag) {
        file_add_preview_drag_but(
            sfile, block, layout, file, path, &tile_draw_rect, preview_imb, file_type_icon);
      }
    }
    else {
      const bool filelist_loading = !filelist_is_ready(files);
      const BIFIconID icon = [&]() {
        if (file->asset) {
          file->asset->ensure_previewable();

          if (filelist_loading) {
            return BIFIconID(ICON_PREVIEW_LOADING);
          }
          return blender::ed::asset::asset_preview_or_icon(*file->asset);
        }
        return filelist_geticon_file_type(files, i, true);
      }();

      icon_ofs += layout->prv_w + 2 * padx;

      /* Add dummy draggable button covering the icon and the label. */
      if (do_drag) {
        const uiStyle *style = UI_style_get();
        const int str_width = UI_fontstyle_string_width(&style->widget, file->name);
        const int drag_width = std::min(
            str_width + icon_ofs,
            int(layout->attribute_columns[COLUMN_NAME].width - ATTRIBUTE_COLUMN_PADDING));
        if (drag_width > 0) {
          /* Uses full row height (tile height plus 2 * tile border padding) so there's no space
           * between rows. */
          uiBut *drag_but = uiDefBut(block,
                                     ButType::Label,
                                     0,
                                     "",
                                     tile_draw_rect.xmin,
                                     tile_draw_rect.ymin - layout->tile_border_y,
                                     drag_width,
                                     layout->tile_h + layout->tile_border_y * 2,
                                     nullptr,
                                     0,
                                     0,
                                     std::nullopt);
          UI_but_dragflag_enable(drag_but, UI_BUT_DRAG_FULL_BUT);
          file_but_enable_drag(drag_but, sfile, file, path, nullptr, icon, UI_SCALE_FAC);
          file_but_tooltip_func_set(sfile, file, drag_but);
        }
      }

      /* Add this after the fake draggable button, so the icon button tooltip is displayed. */
      uiBut *icon_but = file_add_icon_but(sfile,
                                          block,
                                          path,
                                          file,
                                          &tile_draw_rect,
                                          icon,
                                          layout->prv_w,
                                          layout->prv_h,
                                          padx,
                                          is_hidden);
      if (do_drag) {
        /* For some reason the dragging is unreliable for the icon button if we don't explicitly
         * enable dragging, even though the dummy drag button above covers the same area. */
        file_but_enable_drag(icon_but, sfile, file, path, nullptr, icon, UI_SCALE_FAC);
      }

      if (layout->prv_w >= round_fl_to_int(ICON_DEFAULT_WIDTH_SCALE * 2) &&
          (filelist_loading || icon >= BIFICONID_LAST_STATIC))
      {
        const BIFIconID type_icon = filelist_geticon_file_type(files, i, true);
        file_add_overlay_icon_but(block,
                                  tile_draw_rect.xmin + padx - 2,
                                  tile_draw_rect.ymin - 2 * UI_SCALE_FAC,
                                  type_icon);
      }
    }

    const rcti text_rect = text_draw_rect_get(
        v2d, eFileDisplayType(params->display), layout, i, icon_ofs);

    if (file_selflag & FILE_SEL_EDITING) {
      const int but_height =
          (params->display == FILE_IMGDISPLAY) ?
              layout->text_line_height * 1.4f :
              /* Just a little smaller than the tile height, clamped to #UI_UNIT_Y as maximum. */
              std::min(short(BLI_rcti_size_y(&text_rect) - 1.0f * UI_SCALE_FAC), UI_UNIT_Y);
      uiBut *but = uiDefBut(block,
                            ButType::Text,
                            1,
                            "",
                            text_rect.xmin,
                            /* First line only, when name is displayed in multiple lines. */
                            text_rect.ymax - but_height,
                            BLI_rcti_size_x(&text_rect),
                            but_height,
                            params->renamefile,
                            1.0f,
                            float(sizeof(params->renamefile)),
                            "");
      UI_but_func_rename_set(but, renamebutton_cb, file);
      UI_but_flag_enable(but, UI_BUT_NO_UTF8); /* Allow non UTF8 names. */
      UI_but_flag_disable(but, UI_BUT_UNDO);
      if (false == UI_but_active_only(C, region, block, but)) {
        /* Note that this is the only place where we can also handle a cancelled renaming. */

        file_params_rename_end(wm, win, sfile, file);

        /* After the rename button is removed, we need to make sure the view is redrawn once more,
         * in case selection changed. Usually UI code would trigger that redraw, but the rename
         * operator may have been called from a different region.
         * Tagging regions for redrawing while drawing is rightfully prevented. However, this
         * active button removing basically introduces handling logic to drawing code. So a
         * notifier should be an acceptable workaround. */
        WM_event_add_notifier_ex(wm, win, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

        file_selflag = filelist_entry_select_get(sfile->files, file, CHECK_ALL);
      }
    }

    /* file_selflag might have been modified by branch above. */
    if ((file_selflag & FILE_SEL_EDITING) == 0) {
      if (layout->text_lines_count == 1) {
        file_draw_string(text_rect.xmin,
                         text_rect.ymax,
                         file->name,
                         BLI_rcti_size_x(&text_rect),
                         BLI_rcti_size_y(&text_rect),
                         align,
                         text_col);
      }
      else {
        file_draw_string_mulitline_clipped(&text_rect, file->name, align, text_col);
      }
    }

    if (params->display != FILE_IMGDISPLAY) {
      draw_details_columns(params, layout, file, &tile_draw_rect, text_col);
    }
  }

  if (numfiles < 1) {
    const rcti tile_draw_rect = tile_draw_rect_get(v2d, layout, 0);
    const uiStyle *style = UI_style_get();

    const bool is_filtered = params->filter_search[0] != '\0';

    uchar text_col_mod[4];
    copy_v4_v4_uchar(text_col_mod, text_col);
    if (!is_filtered) {
      text_col_mod[3] /= 2;
    }

    const char *message = [&]() {
      if (!filelist_is_ready(files)) {
        return IFACE_("Loading...");
      }
      if (is_filtered) {
        return IFACE_("No results match the search filter");
      }
      return IFACE_("No items");
    }();

    UI_fontstyle_draw_simple(&style->widget,
                             tile_draw_rect.xmin + UI_UNIT_X,
                             tile_draw_rect.ymax - UI_UNIT_Y,
                             message,
                             text_col_mod);
  }

  BLF_batch_draw_end();

  UI_block_end(C, block);
  UI_block_draw(C, block);

  /* Draw last, on top of file list. */
  if (draw_columnheader) {
    draw_columnheader_background(layout, v2d);
    draw_columnheader_columns(params, layout, v2d, text_col);
  }

  if (numfiles != -1) {
    /* Only save current size if there is something to show. */
    layout->curr_size = layout->width;
  }
}

static void file_draw_invalid_asset_library_hint(const bContext *C,
                                                 const SpaceFile *sfile,
                                                 ARegion *region,
                                                 FileAssetSelectParams *asset_params)
{
  char library_ui_path[FILE_MAX_LIBEXTRA];
  file_path_to_ui_path(asset_params->base_params.dir, library_ui_path, sizeof(library_ui_path));

  uchar text_col[4];
  UI_GetThemeColor4ubv(TH_TEXT, text_col);

  const View2D *v2d = &region->v2d;
  const int pad = sfile->layout->tile_border_x;
  const int width = BLI_rctf_size_x(&v2d->tot) - (2 * pad);
  const int line_height = sfile->layout->text_line_height;
  int sx = v2d->tot.xmin + pad;
  /* For some reason no padding needed. */
  int sy = v2d->tot.ymax;

  {
    const char *message = RPT_("Path to asset library does not exist:");
    file_draw_string_multiline(sx, sy, message, width, line_height, text_col, nullptr, &sy);

    sy -= line_height;
    file_draw_string(sx, sy, library_ui_path, width, line_height, UI_STYLE_TEXT_LEFT, text_col);
  }

  /* Separate a bit further. */
  sy -= line_height * 2.2f;

  {
    UI_icon_draw(sx, sy - UI_UNIT_Y, ICON_INFO);

    const char *suggestion = RPT_(
        "Asset Libraries are local directories that can contain .blend files with assets inside.\n"
        "Manage Asset Libraries from the File Paths section in Preferences");
    file_draw_string_multiline(
        sx + UI_UNIT_X, sy, suggestion, width - UI_UNIT_X, line_height, text_col, nullptr, &sy);

    uiBlock *block = UI_block_begin(C, region, __func__, blender::ui::EmbossType::Emboss);
    wmOperatorType *ot = WM_operatortype_find("SCREEN_OT_userpref_show", false);
    uiBut *but = uiDefIconTextButO_ptr(block,
                                       ButType::But,
                                       ot,
                                       blender::wm::OpCallContext::InvokeDefault,
                                       ICON_PREFERENCES,
                                       WM_operatortype_name(ot, nullptr),
                                       sx + UI_UNIT_X,
                                       sy - line_height - UI_UNIT_Y * 1.2f,
                                       UI_UNIT_X * 8,
                                       UI_UNIT_Y,
                                       std::nullopt);
    PointerRNA *but_opptr = UI_but_operator_ptr_ensure(but);
    RNA_enum_set(but_opptr, "section", USER_SECTION_FILE_PATHS);

    UI_block_end(C, block);
    UI_block_draw(C, block);
  }
}

static void file_draw_invalid_library_hint(const bContext * /*C*/,
                                           const SpaceFile *sfile,
                                           ARegion *region,
                                           const char *blendfile_path,
                                           ReportList *reports)
{
  uchar text_col[4];
  UI_GetThemeColor4ubv(TH_TEXT, text_col);

  const View2D *v2d = &region->v2d;
  const int pad = sfile->layout->tile_border_x;
  const int width = BLI_rctf_size_x(&v2d->tot) - (2 * pad);
  const int line_height = sfile->layout->text_line_height;
  int sx = v2d->tot.xmin + pad;
  /* For some reason no padding needed. */
  int sy = v2d->tot.ymax;

  {
    const char *message = RPT_("Unreadable Blender library file:");
    file_draw_string_multiline(sx, sy, message, width, line_height, text_col, nullptr, &sy);

    sy -= line_height;
    file_draw_string(sx, sy, blendfile_path, width, line_height, UI_STYLE_TEXT_LEFT, text_col);
  }

  /* Separate a bit further. */
  sy -= line_height * 2.2f;

  LISTBASE_FOREACH (Report *, report, &reports->list) {
    const short report_type = report->type;
    if (report_type <= RPT_INFO) {
      continue;
    }

    int icon = ICON_INFO;
    if (report_type > RPT_WARNING) {
      icon = ICON_ERROR;
    }
    UI_icon_draw(sx, sy - UI_UNIT_Y, icon);

    file_draw_string_multiline(sx + UI_UNIT_X,
                               sy,
                               RPT_(report->message),
                               width - UI_UNIT_X,
                               line_height,
                               text_col,
                               nullptr,
                               &sy);
    sy -= line_height;
  }
}

bool file_draw_hint_if_invalid(const bContext *C, const SpaceFile *sfile, ARegion *region)
{
  char blendfile_path[FILE_MAX_LIBEXTRA];
  const bool is_asset_browser = ED_fileselect_is_asset_browser(sfile);
  const bool is_library_browser = !is_asset_browser &&
                                  filelist_islibrary(sfile->files, blendfile_path, nullptr);

  if (is_asset_browser) {
    FileAssetSelectParams *asset_params = ED_fileselect_get_asset_params(sfile);

    /* Check if the asset library exists. */
    if (!((asset_params->asset_library_ref.type == ASSET_LIBRARY_LOCAL) ||
          filelist_is_dir(sfile->files, asset_params->base_params.dir)))
    {
      file_draw_invalid_asset_library_hint(C, sfile, region, asset_params);
      return true;
    }
  }

  /* Check if the blendfile library is valid (has entries). */
  if (is_library_browser) {
    if (!filelist_is_ready(sfile->files)) {
      return false;
    }

    const int numfiles = filelist_files_num_entries(sfile->files);
    if (numfiles > 0) {
      return false;
    }

    /* This could all be part of the file-list loading:
     *   - When loading fails this could be saved in the file-list, e.g. when
     *     `BLO_blendhandle_from_file()` returns null in `filelist_readjob_list_lib()`, a
     *     `FL_HAS_INVALID_LIBRARY` file-list flag could be set.
     *   - Reports from it could also be stored in `FileList` rather than being ignored
     *     (`RPT_STORE` must be set!).
     *   - Then we could just check for `is_library_browser` and the `FL_HAS_INVALID_LIBRARY` flag
     *     here, and draw the hint with the reports in the file-list. (We would not draw a hint for
     *     recursive loading, even if the file-list has the "has invalid library" flag set, which
     *     seems like the wanted behavior.)
     *   - The call to BKE_blendfile_is_readable() would not be needed then.
     */
    if (!sfile->runtime->is_blendfile_status_set) {
      BKE_reports_clear(&sfile->runtime->is_blendfile_readable_reports);
      sfile->runtime->is_blendfile_readable = BKE_blendfile_is_readable(
          blendfile_path, &sfile->runtime->is_blendfile_readable_reports);
      sfile->runtime->is_blendfile_status_set = true;
    }
    if (!sfile->runtime->is_blendfile_readable) {
      file_draw_invalid_library_hint(
          C, sfile, region, blendfile_path, &sfile->runtime->is_blendfile_readable_reports);
      return true;
    }
  }

  return false;
}
