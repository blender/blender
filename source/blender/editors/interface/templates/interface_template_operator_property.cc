/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.hh"
#include "BKE_file_handler.hh"
#include "BKE_idprop.hh"
#include "BKE_screen.hh"

#include "BLI_string.h"

#include "BLT_translation.hh"

#include "DNA_collection_types.h"

#include "ED_undo.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"

#include "UI_interface.hh"
#include "interface_intern.hh"

/* we may want to make this optional, disable for now. */
// #define USE_OP_RESET_BUT

struct uiTemplateOperatorPropertyPollParam {
  const bContext *C;
  wmOperator *op;
  short flag;
};

#ifdef USE_OP_RESET_BUT
static void ui_layout_operator_buts__reset_cb(bContext * /*C*/, void *op_pt, void * /*arg_dummy2*/)
{
  WM_operator_properties_reset((wmOperator *)op_pt);
}
#endif

static bool ui_layout_operator_buts_poll_property(PointerRNA * /*ptr*/,
                                                  PropertyRNA *prop,
                                                  void *user_data)
{
  uiTemplateOperatorPropertyPollParam *params = static_cast<uiTemplateOperatorPropertyPollParam *>(
      user_data);

  if ((params->flag & UI_TEMPLATE_OP_PROPS_HIDE_ADVANCED) &&
      (RNA_property_tags(prop) & OP_PROP_TAG_ADVANCED))
  {
    return false;
  }
  return params->op->type->poll_property(params->C, params->op, prop);
}

static eAutoPropButsReturn template_operator_property_buts_draw_single(
    const bContext *C,
    wmOperator *op,
    uiLayout *layout,
    const eButLabelAlign label_align,
    int layout_flags)
{
  uiBlock *block = uiLayoutGetBlock(layout);
  eAutoPropButsReturn return_info = eAutoPropButsReturn(0);

  if (!op->properties) {
    op->properties = blender::bke::idprop::create_group("wmOperatorProperties").release();
  }

  /* poll() on this operator may still fail,
   * at the moment there is no nice feedback when this happens just fails silently. */
  if (!WM_operator_repeat_check(C, op)) {
    UI_block_lock_set(block, true, N_("Operator cannot redo"));
    return return_info;
  }

  /* useful for macros where only one of the steps can't be re-done */
  UI_block_lock_clear(block);

  if (layout_flags & UI_TEMPLATE_OP_PROPS_SHOW_TITLE) {
    uiItemL(layout, WM_operatortype_name(op->type, op->ptr), ICON_NONE);
  }

  /* menu */
  if ((op->type->flag & OPTYPE_PRESET) && !(layout_flags & UI_TEMPLATE_OP_PROPS_HIDE_PRESETS)) {
    /* XXX, no simple way to get WM_MT_operator_presets.bl_label
     * from python! Label remains the same always! */
    PointerRNA op_ptr;
    uiLayout *row;

    UI_block_set_active_operator(block, op, false);

    row = uiLayoutRow(layout, true);
    uiItemM(row, "WM_MT_operator_presets", std::nullopt, ICON_NONE);

    wmOperatorType *ot = WM_operatortype_find("WM_OT_operator_preset_add", false);
    uiItemFullO_ptr(row, ot, "", ICON_ADD, nullptr, WM_OP_INVOKE_DEFAULT, UI_ITEM_NONE, &op_ptr);
    RNA_string_set(&op_ptr, "operator", op->type->idname);

    uiItemFullO_ptr(
        row, ot, "", ICON_REMOVE, nullptr, WM_OP_INVOKE_DEFAULT, UI_ITEM_NONE, &op_ptr);
    RNA_string_set(&op_ptr, "operator", op->type->idname);
    RNA_boolean_set(&op_ptr, "remove_active", true);
  }

  if (op->type->ui) {
    op->layout = layout;
    op->type->ui((bContext *)C, op);
    op->layout = nullptr;

    /* #UI_LAYOUT_OP_SHOW_EMPTY ignored. retun_info is ignored too.
     * We could allow #wmOperatorType.ui callback to return this, but not needed right now. */
  }
  else {
    wmWindowManager *wm = CTX_wm_manager(C);
    uiTemplateOperatorPropertyPollParam user_data{};
    user_data.C = C;
    user_data.op = op;
    user_data.flag = layout_flags;
    const bool use_prop_split = (layout_flags & UI_TEMPLATE_OP_PROPS_NO_SPLIT_LAYOUT) == 0;

    PointerRNA ptr = RNA_pointer_create_discrete(&wm->id, op->type->srna, op->properties);

    uiLayoutSetPropSep(layout, use_prop_split);
    uiLayoutSetPropDecorate(layout, false);

    /* main draw call */
    return_info = uiDefAutoButsRNA(
        layout,
        &ptr,
        op->type->poll_property ? ui_layout_operator_buts_poll_property : nullptr,
        op->type->poll_property ? &user_data : nullptr,
        op->type->prop,
        label_align,
        (layout_flags & UI_TEMPLATE_OP_PROPS_COMPACT));

    if ((return_info & UI_PROP_BUTS_NONE_ADDED) &&
        (layout_flags & UI_TEMPLATE_OP_PROPS_SHOW_EMPTY))
    {
      uiItemL(layout, IFACE_("No Properties"), ICON_NONE);
    }
  }

#ifdef USE_OP_RESET_BUT
  /* its possible that reset can do nothing if all have PROP_SKIP_SAVE enabled
   * but this is not so important if this button is drawn in those cases
   * (which isn't all that likely anyway) - campbell */
  if (op->properties->len) {
    uiBut *but;
    uiLayout *col; /* needed to avoid alignment errors with previous buttons */

    col = uiLayoutColumn(layout, false);
    block = uiLayoutGetBlock(col);
    but = uiDefIconTextBut(block,
                           UI_BTYPE_BUT,
                           0,
                           ICON_FILE_REFRESH,
                           IFACE_("Reset"),
                           0,
                           0,
                           UI_UNIT_X,
                           UI_UNIT_Y,
                           nullptr,
                           0.0,
                           0.0,
                           0.0,
                           0.0,
                           TIP_("Reset operator defaults"));
    UI_but_func_set(but, ui_layout_operator_buts__reset_cb, op, nullptr);
  }
#endif

  /* set various special settings for buttons */

  const bool is_popup = (block->flag & UI_BLOCK_KEEP_OPEN) != 0;

  LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
    /* no undo for buttons for operator redo panels */
    UI_but_flag_disable(but, UI_BUT_UNDO);

    /* Only do this if we're not refreshing an existing UI. */
    if (block->oldblock == nullptr) {
      /* only for popups, see #36109. */

      /* if button is operator's default property, and a text-field, enable focus for it
       * - this is used for allowing operators with popups to rename stuff with fewer clicks
       */
      if (is_popup) {
        if ((but->rnaprop == op->type->prop) && ELEM(but->type, UI_BTYPE_TEXT, UI_BTYPE_NUM)) {
          UI_but_focus_on_enter_event(CTX_wm_window(C), but);
        }
      }
    }
  }

  return return_info;
}

static void template_operator_property_buts_draw_recursive(const bContext *C,
                                                           wmOperator *op,
                                                           uiLayout *layout,
                                                           const eButLabelAlign label_align,
                                                           int layout_flags,
                                                           bool *r_has_advanced)
{
  if (op->type->flag & OPTYPE_MACRO) {
    LISTBASE_FOREACH (wmOperator *, macro_op, &op->macro) {
      template_operator_property_buts_draw_recursive(
          C, macro_op, layout, label_align, layout_flags, r_has_advanced);
    }
  }
  else {
    /* Might want to make label_align adjustable somehow. */
    eAutoPropButsReturn return_info = template_operator_property_buts_draw_single(
        C, op, layout, label_align, layout_flags);
    if (return_info & UI_PROP_BUTS_ANY_FAILED_CHECK) {
      if (r_has_advanced) {
        *r_has_advanced = true;
      }
    }
  }
}

static bool ui_layout_operator_properties_only_booleans(const bContext *C,
                                                        wmWindowManager *wm,
                                                        wmOperator *op,
                                                        int layout_flags)
{
  if (op->type->flag & OPTYPE_MACRO) {
    LISTBASE_FOREACH (wmOperator *, macro_op, &op->macro) {
      if (!ui_layout_operator_properties_only_booleans(C, wm, macro_op, layout_flags)) {
        return false;
      }
    }
  }
  else {
    uiTemplateOperatorPropertyPollParam user_data{};
    user_data.C = C;
    user_data.op = op;
    user_data.flag = layout_flags;

    PointerRNA ptr = RNA_pointer_create_discrete(&wm->id, op->type->srna, op->properties);

    bool all_booleans = true;
    RNA_STRUCT_BEGIN (&ptr, prop) {
      if (RNA_property_flag(prop) & PROP_HIDDEN) {
        continue;
      }
      if (op->type->poll_property &&
          !ui_layout_operator_buts_poll_property(&ptr, prop, &user_data))
      {
        continue;
      }
      if (RNA_property_type(prop) != PROP_BOOLEAN) {
        all_booleans = false;
        break;
      }
    }
    RNA_STRUCT_END;
    if (all_booleans == false) {
      return false;
    }
  }

  return true;
}

void uiTemplateOperatorPropertyButs(
    const bContext *C, uiLayout *layout, wmOperator *op, eButLabelAlign label_align, short flag)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  /* If there are only checkbox items, don't use split layout by default. It looks weird if the
   * check-boxes only use half the width. */
  if (ui_layout_operator_properties_only_booleans(C, wm, op, flag)) {
    flag |= UI_TEMPLATE_OP_PROPS_NO_SPLIT_LAYOUT;
  }

  template_operator_property_buts_draw_recursive(C, op, layout, label_align, flag, nullptr);
}

void uiTemplateOperatorRedoProperties(uiLayout *layout, const bContext *C)
{
  wmOperator *op = WM_operator_last_redo(C);
  uiBlock *block = uiLayoutGetBlock(layout);

  if (op == nullptr) {
    return;
  }

  /* Disable for now, doesn't fit well in popover. */
#if 0
  /* Repeat button with operator name as text. */
  uiItemFullO(layout,
              "SCREEN_OT_repeat_last",
              WM_operatortype_name(op->type, op->ptr),
              ICON_NONE,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              0,
              nullptr);
#endif

  if (WM_operator_repeat_check(C, op)) {
    int layout_flags = 0;
    if (block->panel == nullptr) {
      layout_flags = UI_TEMPLATE_OP_PROPS_SHOW_TITLE;
    }
#if 0
    bool has_advanced = false;
#endif

    UI_block_func_handle_set(block, ED_undo_operator_repeat_cb_evt, op);
    template_operator_property_buts_draw_recursive(
        C, op, layout, UI_BUT_LABEL_ALIGN_NONE, layout_flags, nullptr /* &has_advanced */);
    /* Warning! this leaves the handle function for any other users of this block. */

#if 0
    if (has_advanced) {
      uiItemO(layout, IFACE_("More..."), ICON_NONE, "SCREEN_OT_redo_last");
    }
#endif
  }
}

static wmOperator *minimal_operator_create(wmOperatorType *ot, PointerRNA *properties)
{
  /* Copied from #wm_operator_create.
   * Create a slimmed down operator suitable only for UI drawing. */
  wmOperator *op = MEM_cnew<wmOperator>(ot->rna_ext.srna ? __func__ : ot->idname);
  STRNCPY(op->idname, ot->idname);
  op->type = ot;

  /* Initialize properties but do not assume ownership of them.
   * This "minimal" operator owns nothing. */
  op->ptr = MEM_new<PointerRNA>("wmOperatorPtrRNA");
  op->properties = static_cast<IDProperty *>(properties->data);
  *op->ptr = *properties;

  return op;
}

static void draw_export_controls(
    bContext *C, uiLayout *layout, const std::string &label, int index, bool valid)
{
  uiItemL(layout, label, ICON_NONE);
  if (valid) {
    uiLayout *row = uiLayoutRow(layout, false);
    uiLayoutSetEmboss(row, UI_EMBOSS_NONE);
    uiItemPopoverPanel(row, C, "WM_PT_operator_presets", "", ICON_PRESET);
    uiItemIntO(row, "", ICON_EXPORT, "COLLECTION_OT_exporter_export", "index", index);
  }
}

static void draw_export_properties(bContext *C,
                                   uiLayout *layout,
                                   wmOperator *op,
                                   const std::string &filename)
{
  uiLayout *col = uiLayoutColumn(layout, false);

  uiLayoutSetPropSep(col, true);
  uiLayoutSetPropDecorate(col, false);

  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "filepath");
  std::string placeholder = "//" + filename;
  uiItemFullR(col,
              op->ptr,
              prop,
              RNA_NO_INDEX,
              0,
              UI_ITEM_NONE,
              std::nullopt,
              ICON_NONE,
              placeholder.c_str());

  template_operator_property_buts_draw_single(
      C, op, layout, UI_BUT_LABEL_ALIGN_NONE, UI_TEMPLATE_OP_PROPS_HIDE_PRESETS);
}

static void draw_exporter_item(uiList * /*ui_list*/,
                               const bContext * /*C*/,
                               uiLayout *layout,
                               PointerRNA * /*idataptr*/,
                               PointerRNA *itemptr,
                               int /*icon*/,
                               PointerRNA * /*active_dataptr*/,
                               const char * /*active_propname*/,
                               int /*index*/,
                               int /*flt_flag*/)
{
  uiLayout *row = uiLayoutRow(layout, false);
  uiLayoutSetEmboss(row, UI_EMBOSS_NONE);
  uiItemR(row, itemptr, "name", UI_ITEM_NONE, "", ICON_NONE);
}

void uiTemplateCollectionExporters(uiLayout *layout, bContext *C)
{
  Collection *collection = CTX_data_collection(C);
  ListBase *exporters = &collection->exporters;
  const int index = collection->active_exporter_index;

  /* Register the exporter list type on first use. */
  static const uiListType *exporter_item_list = []() {
    uiListType *lt = MEM_cnew<uiListType>(__func__);
    STRNCPY(lt->idname, "COLLECTION_UL_exporter_list");
    lt->draw_item = draw_exporter_item;
    WM_uilisttype_add(lt);
    return lt;
  }();

  /* Draw exporter list and controls. */
  PointerRNA collection_ptr = RNA_id_pointer_create(&collection->id);
  uiLayout *row = uiLayoutRow(layout, false);
  uiTemplateList(row,
                 C,
                 exporter_item_list->idname,
                 "",
                 &collection_ptr,
                 "exporters",
                 &collection_ptr,
                 "active_exporter_index",
                 nullptr,
                 3,
                 5,
                 UILST_LAYOUT_DEFAULT,
                 1,
                 UI_TEMPLATE_LIST_FLAG_NONE);

  uiLayout *col = uiLayoutColumn(row, true);
  uiItemM(col, "COLLECTION_MT_exporter_add", "", ICON_ADD);
  uiItemIntO(col, "", ICON_REMOVE, "COLLECTION_OT_exporter_remove", "index", index);

  col = uiLayoutColumn(layout, true);
  uiItemO(col, std::nullopt, ICON_EXPORT, "COLLECTION_OT_export_all");
  uiLayoutSetEnabled(col, !BLI_listbase_is_empty(exporters));

  /* Draw the active exporter. */
  CollectionExport *data = (CollectionExport *)BLI_findlink(exporters, index);
  if (!data) {
    return;
  }

  using namespace blender;
  PointerRNA exporter_ptr = RNA_pointer_create_discrete(
      &collection->id, &RNA_CollectionExport, data);
  PanelLayout panel = uiLayoutPanelProp(C, layout, &exporter_ptr, "is_open");

  bke::FileHandlerType *fh = bke::file_handler_find(data->fh_idname);
  if (!fh) {
    std::string label = std::string(IFACE_("Undefined")) + " " + data->fh_idname;
    draw_export_controls(C, panel.header, label, index, false);
    return;
  }

  wmOperatorType *ot = WM_operatortype_find(fh->export_operator, false);
  if (!ot) {
    std::string label = std::string(IFACE_("Undefined")) + " " + fh->export_operator;
    draw_export_controls(C, panel.header, label, index, false);
    return;
  }

  /* Assign temporary operator to uiBlock, which takes ownership. */
  PointerRNA properties = RNA_pointer_create_discrete(
      &collection->id, ot->srna, data->export_properties);
  wmOperator *op = minimal_operator_create(ot, &properties);
  UI_block_set_active_operator(uiLayoutGetBlock(panel.header), op, true);

  /* Draw panel header and contents. */
  std::string label(fh->label);
  draw_export_controls(C, panel.header, label, index, true);
  if (panel.body) {
    draw_export_properties(C, panel.body, op, fh->get_default_filename(collection->id.name + 2));
  }
}
