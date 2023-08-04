/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "BLT_translation.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_prototypes.h"

#include "ED_fileselect.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "file_intern.hh"
#include "filelist.hh"
#include "fsmenu.h"

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

  STRNCPY(panel->drawname, WM_operatortype_name(op->type, op->ptr));
}

static void file_panel_operator(const bContext *C, Panel *panel)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  wmOperator *op = sfile->op;

  UI_block_func_set(uiLayoutGetBlock(panel->layout), file_draw_check_cb, nullptr, nullptr);

  /* Hack: temporary hide. */
  const char *hide[] = {"filepath", "files", "directory", "filename"};
  for (int i = 0; i < ARRAY_SIZE(hide); i++) {
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, hide[i]);
    if (prop) {
      RNA_def_property_flag(prop, PROP_HIDDEN);
    }
  }

  uiTemplateOperatorPropertyButs(
      C, panel->layout, op, UI_BUT_LABEL_ALIGN_NONE, UI_TEMPLATE_OP_PROPS_SHOW_EMPTY);

  /* Hack: temporary hide. */
  for (int i = 0; i < ARRAY_SIZE(hide); i++) {
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, hide[i]);
    if (prop) {
      RNA_def_property_clear_flag(prop, PROP_HIDDEN);
    }
  }

  UI_block_func_set(uiLayoutGetBlock(panel->layout), nullptr, nullptr, nullptr);
}

void file_tool_props_region_panels_register(ARegionType *art)
{
  PanelType *pt;

  pt = static_cast<PanelType *>(
      MEM_callocN(sizeof(PanelType), "spacetype file operator properties"));
  STRNCPY(pt->idname, "FILE_PT_operator");
  STRNCPY(pt->label, N_("Operator"));
  STRNCPY(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->flag = PANEL_TYPE_NO_HEADER;
  pt->poll = file_panel_operator_poll;
  pt->draw_header = file_panel_operator_header;
  pt->draw = file_panel_operator;
  BLI_addtail(&art->paneltypes, pt);
}

static void file_panel_execution_cancel_button(uiLayout *layout)
{
  uiLayout *row = uiLayoutRow(layout, false);
  uiLayoutSetScaleX(row, 0.8f);
  uiLayoutSetFixedSize(row, true);
  uiItemO(row, IFACE_("Cancel"), ICON_NONE, "FILE_OT_cancel");
}

static void file_panel_execution_execute_button(uiLayout *layout, const char *title)
{
  uiLayout *row = uiLayoutRow(layout, false);
  uiLayoutSetScaleX(row, 0.8f);
  uiLayoutSetFixedSize(row, true);
  /* Just a display hint. */
  uiLayoutSetActiveDefault(row, true);
  uiItemO(row, title, ICON_NONE, "FILE_OT_execute");
}

static void file_panel_execution_buttons_draw(const bContext *C, Panel *panel)
{
  bScreen *screen = CTX_wm_screen(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  uiBlock *block = uiLayoutGetBlock(panel->layout);
  uiBut *but;
  uiLayout *row;
  PointerRNA params_rna_ptr, *but_extra_rna_ptr;

  const bool overwrite_alert = file_draw_check_exists(sfile);
  const bool windows_layout =
#ifdef _WIN32
      true;
#else
      false;
#endif

  RNA_pointer_create(&screen->id, &RNA_FileSelectParams, params, &params_rna_ptr);

  row = uiLayoutRow(panel->layout, false);
  uiLayoutSetScaleY(row, 1.3f);

  /* callbacks for operator check functions */
  UI_block_func_set(block, file_draw_check_cb, nullptr, nullptr);

  but = uiDefButR(block,
                  UI_BTYPE_TEXT,
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
                  0,
                  0,
                  TIP_(overwrite_alert ? N_("File name, overwrite existing") : N_("File name")));

  BLI_assert(!UI_but_flag_is_set(but, UI_BUT_UNDO));
  BLI_assert(!UI_but_is_utf8(but));

  UI_but_func_complete_set(but, autocomplete_file, nullptr);
  /* silly workaround calling NFunc to ensure this does not get called
   * immediate ui_apply_but_func but only after button deactivates */
  UI_but_funcN_set(but, file_filename_enter_handle, nullptr, but);

  if (params->flag & FILE_CHECK_EXISTING) {
    but_extra_rna_ptr = UI_but_extra_operator_icon_add(
        but, "FILE_OT_filenum", WM_OP_EXEC_REGION_WIN, ICON_REMOVE);
    RNA_int_set(but_extra_rna_ptr, "increment", -1);
    but_extra_rna_ptr = UI_but_extra_operator_icon_add(
        but, "FILE_OT_filenum", WM_OP_EXEC_REGION_WIN, ICON_ADD);
    RNA_int_set(but_extra_rna_ptr, "increment", 1);
  }

  /* check if this overrides a file and if the operator option is used */
  if (overwrite_alert) {
    UI_but_flag_enable(but, UI_BUT_REDALERT);
  }
  UI_block_func_set(block, nullptr, nullptr, nullptr);

  {
    uiLayout *sub = uiLayoutRow(row, false);
    uiLayoutSetOperatorContext(sub, WM_OP_EXEC_REGION_WIN);

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

  pt = static_cast<PanelType *>(
      MEM_callocN(sizeof(PanelType), "spacetype file execution buttons"));
  STRNCPY(pt->idname, "FILE_PT_execution_buttons");
  STRNCPY(pt->label, N_("Execute Buttons"));
  STRNCPY(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
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
  AssetLibrary *asset_library = filelist_asset_library(sfile->files);
  FileAssetSelectParams *params = ED_fileselect_get_asset_params(sfile);
  BLI_assert(params != nullptr);

  uiLayout *col = uiLayoutColumn(panel->layout, false);
  uiLayout *row = uiLayoutRow(col, true);

  PointerRNA params_ptr;
  RNA_pointer_create(&screen->id, &RNA_FileAssetSelectParams, params, &params_ptr);

  uiItemR(row, &params_ptr, "asset_library_ref", UI_ITEM_NONE, "", ICON_NONE);
  if (params->asset_library_ref.type == ASSET_LIBRARY_LOCAL) {
    bContext *mutable_ctx = CTX_copy(C);
    if (WM_operator_name_poll(mutable_ctx, "asset.bundle_install")) {
      uiItemS(col);
      uiItemMenuEnumO(col,
                      C,
                      "asset.bundle_install",
                      "asset_library_ref",
                      "Copy Bundle to Asset Library...",
                      ICON_IMPORT);
    }
    CTX_free(mutable_ctx);
  }
  else {
    uiItemO(row, "", ICON_FILE_REFRESH, "ASSET_OT_library_refresh");
  }

  uiItemS(col);

  file_create_asset_catalog_tree_view_in_layout(asset_library, col, sfile, params);
}

void file_tools_region_panels_register(ARegionType *art)
{
  PanelType *pt;

  pt = static_cast<PanelType *>(
      MEM_callocN(sizeof(PanelType), "spacetype file asset catalog buttons"));
  STRNCPY(pt->idname, "FILE_PT_asset_catalog_buttons");
  STRNCPY(pt->label, N_("Asset Catalogs"));
  STRNCPY(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->flag = PANEL_TYPE_NO_HEADER;
  pt->poll = file_panel_asset_browsing_poll;
  pt->draw = file_panel_asset_catalog_buttons_draw;
  BLI_addtail(&art->paneltypes, pt);
}
