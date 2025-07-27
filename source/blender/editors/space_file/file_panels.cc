/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "ED_fileselect.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "file_intern.hh"
#include "filelist.hh"

#include <cstring>

static bool file_panel_operator_poll(const bContext *C, PanelType * /*pt*/)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  return (sfile && sfile->op);
}

static bool file_panel_asset_browsing_poll(const bContext *C, PanelType * /*pt*/)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  return sfile && sfile->files && ED_fileselect_is_asset_browser(sfile);
}

static void file_panel_operator_header(const bContext *C, Panel *panel)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  wmOperator *op = sfile->op;

  const std::string opname = WM_operatortype_name(op->type, op->ptr);
  UI_panel_drawname_set(panel, opname);
}

static void file_panel_operator(const bContext *C, Panel *panel)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  wmOperator *op = sfile->op;

  UI_block_func_set(panel->layout->block(), file_draw_check_cb, nullptr, nullptr);

  /* Hack: temporary hide. */
  const char *hide[] = {"filepath", "files", "directory", "filename"};
  /* Track overridden properties with #PROP_HIDDEN flag. */
  bool hidden_override[ARRAY_SIZE(hide)] = {false};
  for (int i = 0; i < ARRAY_SIZE(hide); i++) {
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, hide[i]);
    if (prop && !(RNA_property_flag(prop) & PROP_HIDDEN)) {
      RNA_def_property_flag(prop, PROP_HIDDEN);
      hidden_override[i] = true;
    }
  }

  uiTemplateOperatorPropertyButs(
      C, panel->layout, op, UI_BUT_LABEL_ALIGN_NONE, UI_TEMPLATE_OP_PROPS_SHOW_EMPTY);

  /* Hack: temporary hide. */
  for (int i = 0; i < ARRAY_SIZE(hide); i++) {
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, hide[i]);
    if (prop && hidden_override[i]) {
      RNA_def_property_clear_flag(prop, PROP_HIDDEN);
    }
  }

  UI_block_func_set(panel->layout->block(), nullptr, nullptr, nullptr);
}

void file_tool_props_region_panels_register(ARegionType *art)
{
  PanelType *pt;

  pt = MEM_callocN<PanelType>("spacetype file operator properties");
  STRNCPY_UTF8(pt->idname, "FILE_PT_operator");
  STRNCPY_UTF8(pt->label, N_("Operator"));
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->flag = PANEL_TYPE_NO_HEADER;
  pt->poll = file_panel_operator_poll;
  pt->draw_header = file_panel_operator_header;
  pt->draw = file_panel_operator;
  BLI_addtail(&art->paneltypes, pt);
}

static void file_panel_execution_cancel_button(uiLayout *layout)
{
  uiLayout *row = &layout->row(false);
  row->scale_x_set(0.8f);
  row->fixed_size_set(true);
  row->op("FILE_OT_cancel", IFACE_("Cancel"), ICON_NONE);
}

static void file_panel_execution_execute_button(uiLayout *layout, const char *title)
{
  uiLayout *row = &layout->row(false);
  row->scale_x_set(0.8f);
  row->fixed_size_set(true);
  /* Just a display hint. */
  row->active_default_set(true);
  row->op("FILE_OT_execute", title, ICON_NONE);
}

static void file_panel_execution_buttons_draw(const bContext *C, Panel *panel)
{
  bScreen *screen = CTX_wm_screen(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  uiBlock *block = panel->layout->block();
  uiBut *but;
  uiLayout *row;
  PointerRNA *but_extra_rna_ptr;

  const bool overwrite_alert = file_draw_check_exists(sfile);
  const bool windows_layout =
#ifdef _WIN32
      true;
#else
      false;
#endif

  PointerRNA params_rna_ptr = RNA_pointer_create_discrete(
      &screen->id, &RNA_FileSelectParams, params);

  row = &panel->layout->row(false);
  row->scale_y_set(1.3f);

  /* callbacks for operator check functions */
  UI_block_func_set(block, file_draw_check_cb, nullptr, nullptr);

  but = uiDefButR(block,
                  ButType::Text,
                  -1,
                  "",
                  0,
                  0,
                  UI_UNIT_X * 5,
                  UI_UNIT_Y,
                  &params_rna_ptr,
                  "filename",
                  0,
                  0.0f,
                  float(FILE_MAXFILE),
                  overwrite_alert ? TIP_("File name, overwrite existing") : TIP_("File name"));

  BLI_assert(!UI_but_flag_is_set(but, UI_BUT_UNDO));
  BLI_assert(!UI_but_is_utf8(but));

  UI_but_func_complete_set(but, autocomplete_file, nullptr);
  /* silly workaround calling NFunc to ensure this does not get called
   * immediate ui_apply_but_func but only after button deactivates */
  UI_but_funcN_set(but, file_filename_enter_handle, nullptr, but);

  if (params->flag & FILE_CHECK_EXISTING) {
    but_extra_rna_ptr = UI_but_extra_operator_icon_add(
        but, "FILE_OT_filenum", blender::wm::OpCallContext::ExecRegionWin, ICON_REMOVE);
    RNA_int_set(but_extra_rna_ptr, "increment", -1);
    but_extra_rna_ptr = UI_but_extra_operator_icon_add(
        but, "FILE_OT_filenum", blender::wm::OpCallContext::ExecRegionWin, ICON_ADD);
    RNA_int_set(but_extra_rna_ptr, "increment", 1);
  }

  /* check if this overrides a file and if the operator option is used */
  if (overwrite_alert) {
    UI_but_flag_enable(but, UI_BUT_REDALERT);
  }
  UI_block_func_set(block, nullptr, nullptr, nullptr);

  {
    uiLayout *sub = &row->row(false);
    sub->operator_context_set(blender::wm::OpCallContext::ExecRegionWin);

    if (windows_layout) {
      file_panel_execution_execute_button(sub, params->title);
      file_panel_execution_cancel_button(sub);
    }
    else {
      file_panel_execution_cancel_button(sub);
      file_panel_execution_execute_button(sub, params->title);
    }
  }
}

void file_execute_region_panels_register(ARegionType *art)
{
  PanelType *pt;

  pt = MEM_callocN<PanelType>("spacetype file execution buttons");
  STRNCPY_UTF8(pt->idname, "FILE_PT_execution_buttons");
  STRNCPY_UTF8(pt->label, N_("Execute Buttons"));
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->flag = PANEL_TYPE_NO_HEADER;
  pt->poll = file_panel_operator_poll;
  pt->draw = file_panel_execution_buttons_draw;
  BLI_addtail(&art->paneltypes, pt);
}

static void file_panel_asset_catalog_buttons_draw(const bContext *C, Panel *panel)
{
  bScreen *screen = CTX_wm_screen(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  /* May be null if the library wasn't loaded yet. */
  blender::asset_system::AssetLibrary *asset_library = filelist_asset_library(sfile->files);
  FileAssetSelectParams *params = ED_fileselect_get_asset_params(sfile);
  BLI_assert(params != nullptr);

  uiLayout *col = &panel->layout->column(false);
  uiLayout *row = &col->row(true);

  PointerRNA params_ptr = RNA_pointer_create_discrete(
      &screen->id, &RNA_FileAssetSelectParams, params);

  row->prop(&params_ptr, "asset_library_reference", UI_ITEM_NONE, "", ICON_NONE);
  if (params->asset_library_ref.type == ASSET_LIBRARY_LOCAL) {
    bContext *mutable_ctx = CTX_copy(C);
    if (WM_operator_name_poll(mutable_ctx, "asset.bundle_install")) {
      col->separator();
      col->op_menu_enum(C,
                        "asset.bundle_install",
                        "asset_library_reference",
                        IFACE_("Copy Bundle to Asset Library..."),
                        ICON_IMPORT);
    }
    CTX_free(mutable_ctx);
  }
  else {
    row->op("ASSET_OT_library_refresh", "", ICON_FILE_REFRESH);
  }

  col->separator();

  blender::ed::asset_browser::file_create_asset_catalog_tree_view_in_layout(
      C, asset_library, col, sfile, params);
}

void file_tools_region_panels_register(ARegionType *art)
{
  PanelType *pt;

  pt = MEM_callocN<PanelType>("spacetype file asset catalog buttons");
  STRNCPY_UTF8(pt->idname, "FILE_PT_asset_catalog_buttons");
  STRNCPY_UTF8(pt->label, N_("Asset Catalogs"));
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->flag = PANEL_TYPE_NO_HEADER;
  pt->poll = file_panel_asset_browsing_poll;
  pt->draw = file_panel_asset_catalog_buttons_draw;
  BLI_addtail(&art->paneltypes, pt);
}
