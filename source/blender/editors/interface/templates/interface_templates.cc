/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_library.hh"
#include "BKE_screen.hh"

#include "BLI_math_color.h"
#include "BLI_string_ref.hh"

#include "ED_fileselect.hh"
#include "ED_screen.hh"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "interface_intern.hh"
#include "interface_templates_intern.hh"

using blender::StringRefNull;

/* -------------------------------------------------------------------- */
/** \name Search Menu Helpers
 * \{ */

int template_search_textbut_width(PointerRNA *ptr, PropertyRNA *name_prop)
{
  char str[UI_MAX_DRAW_STR];
  int buf_len = 0;

  BLI_assert(RNA_property_type(name_prop) == PROP_STRING);

  const char *name = RNA_property_string_get_alloc(ptr, name_prop, str, sizeof(str), &buf_len);

  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
  const int margin = UI_UNIT_X * 0.75f;
  const int estimated_width = UI_fontstyle_string_width(fstyle, name) + margin;

  if (name != str) {
    MEM_freeN(name);
  }

  /* Clamp to some min/max width. */
  return std::clamp(
      estimated_width, TEMPLATE_SEARCH_TEXTBUT_MIN_WIDTH, TEMPLATE_SEARCH_TEXTBUT_MIN_WIDTH * 4);
}

int template_search_textbut_height()
{
  return TEMPLATE_SEARCH_TEXTBUT_HEIGHT;
}

void template_add_button_search_menu(const bContext *C,
                                     uiLayout *layout,
                                     uiBlock *block,
                                     PointerRNA *ptr,
                                     PropertyRNA *prop,
                                     uiBlockCreateFunc block_func,
                                     void *block_argN,
                                     const std::optional<blender::StringRef> tip,
                                     const bool use_previews,
                                     const bool editable,
                                     const bool live_icon,
                                     uiButArgNFree func_argN_free_fn,
                                     uiButArgNCopy func_argN_copy_fn)
{
  const PointerRNA active_ptr = RNA_property_pointer_get(ptr, prop);
  ID *id = (active_ptr.data && RNA_struct_is_ID(active_ptr.type)) ?
               static_cast<ID *>(active_ptr.data) :
               nullptr;
  const ID *idfrom = ptr->owner_id;
  const StructRNA *type = active_ptr.type ? active_ptr.type : RNA_property_pointer_type(ptr, prop);
  uiBut *but;

  if (use_previews) {
    ARegion *region = CTX_wm_region(C);
    /* Ugly tool header exception. */
    const bool use_big_size = (region->regiontype != RGN_TYPE_TOOL_HEADER);
    /* Ugly exception for screens here,
     * drawing their preview in icon size looks ugly/useless */
    const bool use_preview_icon = use_big_size || (id && (GS(id->name) != ID_SCR));
    const short width = UI_UNIT_X * (use_big_size ? 6 : 1.6f);
    const short height = UI_UNIT_Y * (use_big_size ? 6 : 1);
    uiLayout *col = nullptr;

    if (use_big_size) {
      /* Assume column layout here. To be more correct, we should check if the layout passed to
       * template_id is a column one, but this should work well in practice. */
      col = &layout->column(true);
    }

    but = uiDefBlockButN(block,
                         block_func,
                         block_argN,
                         "",
                         0,
                         0,
                         width,
                         height,
                         tip,
                         func_argN_free_fn,
                         func_argN_copy_fn);
    if (use_preview_icon) {
      const int icon = id ? ui_id_icon_get(C, id, use_big_size) : RNA_struct_ui_icon(type);
      ui_def_but_icon(but, icon, UI_HAS_ICON | UI_BUT_ICON_PREVIEW);
    }
    else {
      ui_def_but_icon(but, RNA_struct_ui_icon(type), UI_HAS_ICON);
      UI_but_drawflag_enable(but, UI_BUT_ICON_LEFT);
    }

    if ((idfrom && !ID_IS_EDITABLE(idfrom)) || !editable) {
      UI_but_flag_enable(but, UI_BUT_DISABLED);
    }
    if (use_big_size) {
      (col ? col : layout)->row(true);
    }
  }
  else {
    but = uiDefBlockButN(block,
                         block_func,
                         block_argN,
                         "",
                         0,
                         0,
                         UI_UNIT_X * 1.6,
                         UI_UNIT_Y,
                         tip,
                         func_argN_free_fn,
                         func_argN_copy_fn);

    if (live_icon) {
      const int icon = id ? ui_id_icon_get(C, id, false) : RNA_struct_ui_icon(type);
      ui_def_but_icon(but, icon, UI_HAS_ICON | UI_BUT_ICON_PREVIEW);
    }
    else {
      ui_def_but_icon(but, RNA_struct_ui_icon(type), UI_HAS_ICON);
    }
    if (id) {
      /* default dragging of icon for id browse buttons */
      UI_but_drag_set_id(but, id);
    }
    UI_but_drawflag_enable(but, UI_BUT_ICON_LEFT);

    if ((idfrom && !ID_IS_EDITABLE(idfrom)) || !editable) {
      UI_but_flag_enable(but, UI_BUT_DISABLED);
    }
  }
}

uiBlock *template_common_search_menu(const bContext *C,
                                     ARegion *region,
                                     uiButSearchUpdateFn search_update_fn,
                                     void *search_arg,
                                     uiButHandleFunc search_exec_fn,
                                     void *active_item,
                                     uiButSearchTooltipFn item_tooltip_fn,
                                     const int preview_rows,
                                     const int preview_cols,
                                     float scale)
{
  static char search[256];
  wmWindow *win = CTX_wm_window(C);
  uiBut *but;

  /* clear initial search string, then all items show */
  search[0] = 0;

  uiBlock *block = UI_block_begin(C, region, "_popup", blender::ui::EmbossType::Emboss);
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_SEARCH_MENU);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  /* preview thumbnails */
  if (preview_rows > 0 && preview_cols > 0) {
    const int w = 4 * U.widget_unit * preview_cols * scale;
    const int h = 5 * U.widget_unit * preview_rows * scale + 2 * UI_SEARCHBOX_TRIA_H -
                  UI_SEARCHBOX_BOUNDS;

    /* fake button, it holds space for search items */
    uiDefBut(block, ButType::Label, 0, "", 0, UI_UNIT_Y, w, h, nullptr, 0, 0, std::nullopt);
    but = uiDefSearchBut(block, search, 0, ICON_VIEWZOOM, sizeof(search), 0, 0, w, UI_UNIT_Y, "");
    UI_but_search_preview_grid_size_set(but, preview_rows, preview_cols);
  }
  /* list view */
  else {
    const int searchbox_width = UI_searchbox_size_x_guess(C, search_update_fn, search_arg);
    const int searchbox_height = UI_searchbox_size_y();
    const int search_but_height = UI_UNIT_Y - 1.0f * UI_SCALE_FAC;

    /* fake button, it holds space for search items */
    uiDefBut(block,
             ButType::Label,
             0,
             "",
             0,
             search_but_height,
             searchbox_width,
             searchbox_height - UI_SEARCHBOX_BOUNDS,
             nullptr,
             0,
             0,
             std::nullopt);
    but = uiDefSearchBut(block,
                         search,
                         0,
                         ICON_VIEWZOOM,
                         sizeof(search),
                         0,
                         0,
                         searchbox_width,
                         search_but_height,
                         "");
  }
  UI_but_func_search_set(but,
                         ui_searchbox_create_generic,
                         search_update_fn,
                         search_arg,
                         false,
                         nullptr,
                         search_exec_fn,
                         active_item);
  UI_but_func_search_set_tooltip(but, item_tooltip_fn);

  UI_block_bounds_set_normal(block, UI_SEARCHBOX_BOUNDS);
  UI_block_direction_set(block, UI_DIR_DOWN);

  /* give search-field focus */
  UI_but_focus_on_enter_event(win, but);
  /* this type of search menu requires undo */
  but->flag |= UI_BUT_UNDO;

  return block;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Header Template
 * \{ */

void uiTemplateHeader(uiLayout *layout, bContext *C)
{
  uiBlock *block = layout->absolute_block();
  ED_area_header_switchbutton(C, block, 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name RNA Path Builder Template
 * \{ */

void uiTemplatePathBuilder(uiLayout *layout,
                           PointerRNA *ptr,
                           const StringRefNull propname,
                           PointerRNA * /*root_ptr*/,
                           const std::optional<StringRefNull> text)
{
  /* check that properties are valid */
  PropertyRNA *propPath = RNA_struct_find_property(ptr, propname.c_str());
  if (!propPath || RNA_property_type(propPath) != PROP_STRING) {
    RNA_warning(
        "path property not found: %s.%s", RNA_struct_identifier(ptr->type), propname.c_str());
    return;
  }

  /* Start drawing UI Elements using standard defines */
  uiLayout *row = &layout->row(true);

  /* Path (existing string) Widget */
  row->prop(ptr, propname, UI_ITEM_NONE, text, ICON_RNA);

  /* TODO: attach something to this to make allow
   * searching of nested properties to 'build' the path */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Socket Icon Template
 * \{ */

void uiTemplateNodeSocket(uiLayout *layout, bContext * /*C*/, const float color[4])
{
  uiBlock *block = layout->block();
  UI_block_align_begin(block);

  /* XXX using explicit socket colors is not quite ideal.
   * Eventually it should be possible to use theme colors for this purpose,
   * but this requires a better design for extendable color palettes in user preferences. */
  uiBut *but = uiDefBut(
      block, ButType::NodeSocket, 0, "", 0, 0, UI_UNIT_X, UI_UNIT_Y, nullptr, 0, 0, "");
  rgba_float_to_uchar(but->col, color);

  UI_block_align_end(block);
}

/* -------------------------------------------------------------------- */
/** \name FileSelectParams Path Button Template
 * \{ */

void uiTemplateFileSelectPath(uiLayout *layout, bContext *C, FileSelectParams *params)
{
  bScreen *screen = CTX_wm_screen(C);
  SpaceFile *sfile = CTX_wm_space_file(C);

  ED_file_path_button(screen, sfile, params, layout->block());
}

/** \} */
