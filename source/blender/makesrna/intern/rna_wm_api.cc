/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cctype>
#include <cstdlib>

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "UI_interface_icons.hh"
#include "UI_interface_types.hh"

#include "wm_cursors.hh"
#include "wm_event_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "rna_internal.hh" /* own include */

/* confusing 2 enums mixed up here */
const EnumPropertyItem rna_enum_window_cursor_items[] = {
    {WM_CURSOR_DEFAULT, "DEFAULT", 0, "Default", ""},
    {WM_CURSOR_NONE, "NONE", 0, "None", ""},
    {WM_CURSOR_WAIT, "WAIT", 0, "Wait", ""},
    {WM_CURSOR_EDIT, "CROSSHAIR", 0, "Crosshair", ""},
    {WM_CURSOR_X_MOVE, "MOVE_X", 0, "Move-X", ""},
    {WM_CURSOR_Y_MOVE, "MOVE_Y", 0, "Move-Y", ""},

    /* new */
    {WM_CURSOR_KNIFE, "KNIFE", 0, "Knife", ""},
    {WM_CURSOR_TEXT_EDIT, "TEXT", 0, "Text", ""},
    {WM_CURSOR_PAINT_BRUSH, "PAINT_BRUSH", 0, "Paint Brush", ""},
    {WM_CURSOR_PAINT, "PAINT_CROSS", 0, "Paint Cross", ""},
    {WM_CURSOR_DOT, "DOT", 0, "Dot Cursor", ""},
    {WM_CURSOR_ERASER, "ERASER", 0, "Eraser", ""},
    {WM_CURSOR_HAND, "HAND", 0, "Open Hand", ""},
    {WM_CURSOR_HAND_POINT, "HAND_POINT", 0, "Pointing Hand", ""},
    {WM_CURSOR_HAND_CLOSED, "HAND_CLOSED", 0, "Closed Hand", ""},
    {WM_CURSOR_EW_SCROLL, "SCROLL_X", 0, "Scroll-X", ""},
    {WM_CURSOR_NS_SCROLL, "SCROLL_Y", 0, "Scroll-Y", ""},
    {WM_CURSOR_NSEW_SCROLL, "SCROLL_XY", 0, "Scroll-XY", ""},
    {WM_CURSOR_EYEDROPPER, "EYEDROPPER", 0, "Eyedropper", ""},
    {WM_CURSOR_PICK_AREA, "PICK_AREA", 0, "Pick Area", ""},
    {WM_CURSOR_STOP, "STOP", 0, "Stop", ""},
    {WM_CURSOR_COPY, "COPY", 0, "Copy", ""},
    {WM_CURSOR_CROSS, "CROSS", 0, "Cross", ""},
    {WM_CURSOR_MUTE, "MUTE", 0, "Mute", ""},
    {WM_CURSOR_ZOOM_IN, "ZOOM_IN", 0, "Zoom In", ""},
    {WM_CURSOR_ZOOM_OUT, "ZOOM_OUT", 0, "Zoom Out", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include "DNA_userdef_types.h"

#  include "BLI_string.h"
#  include "BLI_string_utf8.h"

#  include "BKE_context.hh"
#  include "BKE_global.hh"
#  include "BKE_main.hh"
#  include "BKE_report.hh"
#  include "BKE_undo_system.hh"

#  include "WM_types.hh"

/* Needed since RNA doesn't use `const` in function signatures. */
static bool rna_KeyMapItem_compare(wmKeyMapItem *k1, wmKeyMapItem *k2)
{
  return WM_keymap_item_compare(k1, k2);
}

static void rna_KeyMapItem_to_string(wmKeyMapItem *kmi, bool compact, char *result)
{
  BLI_strncpy(
      result, WM_keymap_item_to_string(kmi, compact).value_or("").c_str(), UI_MAX_SHORTCUT_STR);
}

static wmKeyMap *rna_keymap_active(wmKeyMap *km, bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  return WM_keymap_active(wm, km);
}

static void rna_keymap_restore_to_default(wmKeyMap *km, bContext *C)
{
  WM_keymap_restore_to_default(km, CTX_wm_manager(C));
}

static void rna_keymap_restore_item_to_default(wmKeyMap *km, bContext *C, wmKeyMapItem *kmi)
{
  WM_keymap_item_restore_to_default(CTX_wm_manager(C), km, kmi);
}

static void rna_Operator_report(wmOperator *op, int type, const char *msg)
{
  BKE_report(op->reports, eReportType(type), msg);
}

static bool rna_Operator_is_repeat(wmOperator *op, bContext *C)
{
  return WM_operator_is_repeat(C, op);
}

/* since event isn't needed... */
static void rna_Operator_enum_search_invoke(bContext *C, wmOperator *op)
{
  WM_enum_search_invoke(C, op, nullptr);
}

static int rna_Operator_ui_popup(bContext *C, wmOperator *op, int width)
{
  return wmOperatorStatus(WM_operator_ui_popup(C, op, width));
}

static bool rna_event_modal_handler_add(bContext *C, ReportList *reports, wmOperator *op)
{
  wmWindow *win = CTX_wm_window(C);
  if (win == nullptr) {
    BKE_report(reports, RPT_ERROR, "No active window in context!");
    return false;
  }
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  return WM_event_add_modal_handler_ex(win, area, region, op) != nullptr;
}

static wmTimer *rna_event_timer_add(wmWindowManager *wm, float time_step, wmWindow *win)
{
  /* NOTE: we need a way for Python to know event types, `TIMER` is hard coded. */
  return WM_event_timer_add(wm, win, TIMER, time_step);
}

static void rna_event_timer_remove(wmWindowManager *wm, wmTimer *timer)
{
  WM_event_timer_remove(wm, timer->win, timer);
}

static wmGizmoGroupType *wm_gizmogrouptype_find_for_add_remove(ReportList *reports,
                                                               const char *idname)
{
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, true);
  if (gzgt == nullptr) {
    BKE_reportf(reports, RPT_ERROR, "Gizmo group type '%s' not found!", idname);
    return nullptr;
  }
  if (gzgt->flag & WM_GIZMOGROUPTYPE_PERSISTENT) {
    BKE_reportf(reports, RPT_ERROR, "Gizmo group '%s' has 'PERSISTENT' option set!", idname);
    return nullptr;
  }
  return gzgt;
}

static void rna_gizmo_group_type_ensure(ReportList *reports, const char *idname)
{
  wmGizmoGroupType *gzgt = wm_gizmogrouptype_find_for_add_remove(reports, idname);
  if (gzgt != nullptr) {
    WM_gizmo_group_type_ensure_ptr(gzgt);
  }
}

static void rna_gizmo_group_type_unlink_delayed(ReportList *reports, const char *idname)
{
  wmGizmoGroupType *gzgt = wm_gizmogrouptype_find_for_add_remove(reports, idname);
  if (gzgt != nullptr) {
    WM_gizmo_group_type_unlink_delayed_ptr(gzgt);
  }
}

/* Placeholder data for final implementation of a true progress-bar. */
static struct wmStaticProgress {
  float min;
  float max;
  bool is_valid;
} wm_progress_state = {0, 0, false};

static void rna_progress_begin(wmWindowManager * /*wm*/, float min, float max)
{
  float range = max - min;
  if (range != 0) {
    wm_progress_state.min = min;
    wm_progress_state.max = max;
    wm_progress_state.is_valid = true;
  }
  else {
    wm_progress_state.is_valid = false;
  }
}

static void rna_progress_update(wmWindowManager *wm, float value)
{
  if (wm_progress_state.is_valid) {
    /* Map to factor 0..1. */
    wmWindow *win = wm->runtime->winactive;
    if (win) {
      const float progress_factor = (value - wm_progress_state.min) /
                                    (wm_progress_state.max - wm_progress_state.min);
      WM_cursor_progress(win, progress_factor);
    }
  }
}

static void rna_progress_end(wmWindowManager *wm)
{
  if (wm_progress_state.is_valid) {
    wmWindow *win = wm->runtime->winactive;
    if (win) {
      WM_cursor_modal_restore(win);
      wm_progress_state.is_valid = false;
    }
  }
}

/* wrap these because of 'const wmEvent *' */
static int rna_Operator_confirm(bContext *C,
                                wmOperator *op,
                                wmEvent * /*event*/,
                                const char *title,
                                const char *message,
                                const char *confirm_text,
                                const int icon,
                                const char *text_ctxt,
                                const bool translate)
{
  std::optional<blender::StringRefNull> title_str = RNA_translate_ui_text(
      title, text_ctxt, nullptr, nullptr, translate);
  std::optional<blender::StringRefNull> message_str = RNA_translate_ui_text(
      message, text_ctxt, nullptr, nullptr, translate);
  std::optional<blender::StringRefNull> confirm_text_str = RNA_translate_ui_text(
      confirm_text, text_ctxt, nullptr, nullptr, translate);
  return WM_operator_confirm_ex(C,
                                op,
                                title_str ? title_str->c_str() : nullptr,
                                message_str ? message_str->c_str() : nullptr,
                                confirm_text_str ? confirm_text_str->c_str() : nullptr,
                                icon);
}

static int rna_Operator_props_popup(bContext *C, wmOperator *op, wmEvent *event)
{
  return WM_operator_props_popup(C, op, event);
}

static int rna_Operator_props_dialog_popup(bContext *C,
                                           wmOperator *op,
                                           const int width,
                                           const char *title,
                                           const char *confirm_text,
                                           const bool cancel_default,
                                           const char *text_ctxt,
                                           const bool translate)
{
  std::optional<blender::StringRefNull> title_str = RNA_translate_ui_text(
      title, text_ctxt, nullptr, nullptr, translate);
  std::optional<blender::StringRefNull> confirm_text_str = RNA_translate_ui_text(
      confirm_text, text_ctxt, nullptr, nullptr, translate);
  return WM_operator_props_dialog_popup(
      C,
      op,
      width,
      title_str ? std::make_optional<std::string>(*title_str) : std::nullopt,
      confirm_text_str ? std::make_optional<std::string>(*confirm_text_str) : std::nullopt,
      cancel_default);
}

static int16_t keymap_item_modifier_flag_from_args(
    bool any, int shift, int ctrl, int alt, int oskey, int hyper)
{
  int16_t modifier = 0;
  if (any) {
    modifier = KM_ANY;
  }
  else {
#  define MOD_VAR_ASSIGN_FLAG(mod_var, mod_flag) \
    if (mod_var == KM_MOD_HELD) { \
      modifier |= mod_flag; \
    } \
    else if (mod_var == KM_ANY) { \
      modifier |= KMI_PARAMS_MOD_TO_ANY(mod_flag); \
    } \
    ((void)0)

    MOD_VAR_ASSIGN_FLAG(shift, KM_SHIFT);
    MOD_VAR_ASSIGN_FLAG(ctrl, KM_CTRL);
    MOD_VAR_ASSIGN_FLAG(alt, KM_ALT);
    MOD_VAR_ASSIGN_FLAG(oskey, KM_OSKEY);
    MOD_VAR_ASSIGN_FLAG(hyper, KM_HYPER);

#  undef MOD_VAR_ASSIGN_FLAG
  }
  return modifier;
}

static wmKeyMapItem *rna_KeyMap_item_new(wmKeyMap *km,
                                         ReportList *reports,
                                         const char *idname,
                                         int type,
                                         int value,
                                         bool any,
                                         int shift,
                                         int ctrl,
                                         int alt,
                                         int oskey,
                                         int hyper,
                                         int keymodifier,
                                         int direction,
                                         bool repeat,
                                         bool head)
{
  /* only on non-modal maps */
  if (km->flag & KEYMAP_MODAL) {
    BKE_report(reports, RPT_ERROR, "Not a non-modal keymap");
    return nullptr;
  }

  // wmWindowManager *wm = CTX_wm_manager(C);
  wmKeyMapItem *kmi = nullptr;
  char idname_bl[OP_MAX_TYPENAME];

  WM_operator_bl_idname(idname_bl, idname);

  KeyMapItem_Params params{};
  params.type = type;
  params.value = value;
  params.modifier = keymap_item_modifier_flag_from_args(any, shift, ctrl, alt, oskey, hyper);
  params.keymodifier = keymodifier;
  params.direction = direction;

  /* create keymap item */
  kmi = WM_keymap_add_item(km, idname_bl, &params);

  if (!repeat) {
    kmi->flag |= KMI_REPEAT_IGNORE;
  }

  /* #32437 allow scripts to define hotkeys that get added to start of keymap
   *          so that they stand a chance against catch-all defines later on
   */
  if (head) {
    BLI_remlink(&km->items, kmi);
    BLI_addhead(&km->items, kmi);
  }

  return kmi;
}

static wmKeyMapItem *rna_KeyMap_item_new_from_item(wmKeyMap *km,
                                                   ReportList *reports,
                                                   wmKeyMapItem *kmi_src,
                                                   bool head)
{
  // wmWindowManager *wm = CTX_wm_manager(C);

  if ((km->flag & KEYMAP_MODAL) == (kmi_src->idname[0] != '\0')) {
    BKE_report(reports, RPT_ERROR, "Cannot mix modal/non-modal items");
    return nullptr;
  }

  /* create keymap item */
  wmKeyMapItem *kmi = WM_keymap_add_item_copy(km, kmi_src);
  if (head) {
    BLI_remlink(&km->items, kmi);
    BLI_addhead(&km->items, kmi);
  }
  return kmi;
}

static wmKeyMapItem *rna_KeyMap_item_new_modal(wmKeyMap *km,
                                               ReportList *reports,
                                               const char *propvalue_str,
                                               int type,
                                               int value,
                                               bool any,
                                               int shift,
                                               int ctrl,
                                               int alt,
                                               int oskey,
                                               int hyper,
                                               int keymodifier,
                                               int direction,
                                               bool repeat)
{
  /* only modal maps */
  if ((km->flag & KEYMAP_MODAL) == 0) {
    BKE_report(reports, RPT_ERROR, "Not a modal keymap");
    return nullptr;
  }

  wmKeyMapItem *kmi = nullptr;
  int propvalue = 0;

  KeyMapItem_Params params{};
  params.type = type;
  params.value = value;
  params.modifier = keymap_item_modifier_flag_from_args(any, shift, ctrl, alt, oskey, hyper);
  params.keymodifier = keymodifier;
  params.direction = direction;

  /* not initialized yet, do delayed lookup */
  if (!km->modal_items) {
    kmi = WM_modalkeymap_add_item_str(km, &params, propvalue_str);
  }
  else {
    if (RNA_enum_value_from_id(static_cast<const EnumPropertyItem *>(km->modal_items),
                               propvalue_str,
                               &propvalue) == 0)
    {
      BKE_report(reports, RPT_WARNING, "Property value not in enumeration");
    }
    kmi = WM_modalkeymap_add_item(km, &params, propvalue);
  }

  if (!repeat) {
    kmi->flag |= KMI_REPEAT_IGNORE;
  }

  return kmi;
}

static void rna_KeyMap_item_remove(wmKeyMap *km, ReportList *reports, PointerRNA *kmi_ptr)
{
  wmKeyMapItem *kmi = static_cast<wmKeyMapItem *>(kmi_ptr->data);

  if (UNLIKELY(BLI_findindex(&km->items, kmi) == -1)) {
    BKE_reportf(
        reports, RPT_ERROR, "KeyMapItem '%s' not found in KeyMap '%s'", kmi->idname, km->idname);
    return;
  }

  WM_keymap_remove_item(km, kmi);
  kmi_ptr->invalidate();
}

static PointerRNA rna_KeyMap_item_find_match(
    ID *id, wmKeyMap *km_base, ReportList *reports, wmKeyMap *km_match, wmKeyMapItem *kmi_match)
{
  wmKeyMapItem *kmi_base = WM_keymap_item_find_match(km_base, km_match, kmi_match, reports);
  if (kmi_base) {
    return RNA_pointer_create_discrete(id, &RNA_KeyMapItem, kmi_base);
  }
  return PointerRNA_NULL;
}

static PointerRNA rna_KeyMap_item_find_from_operator(ID *id,
                                                     wmKeyMap *km,
                                                     const char *idname,
                                                     PointerRNA *properties,
                                                     int include_mask,
                                                     int exclude_mask)
{
  char idname_bl[OP_MAX_TYPENAME];
  WM_operator_bl_idname(idname_bl, idname);

  wmKeyMapItem *kmi = WM_key_event_operator_from_keymap(
      km, idname_bl, static_cast<IDProperty *>(properties->data), include_mask, exclude_mask);
  PointerRNA kmi_ptr = RNA_pointer_create_discrete(id, &RNA_KeyMapItem, kmi);
  return kmi_ptr;
}

static PointerRNA rna_KeyMap_item_match_event(ID *id, wmKeyMap *km, bContext *C, wmEvent *event)
{
  wmKeyMapItem *kmi = WM_event_match_keymap_item(C, km, event);
  PointerRNA kmi_ptr = RNA_pointer_create_discrete(id, &RNA_KeyMapItem, kmi);
  return kmi_ptr;
}

static wmKeyMap *rna_KeyMaps_new(wmKeyConfig *keyconf,
                                 ReportList *reports,
                                 const char *idname,
                                 int spaceid,
                                 int regionid,
                                 bool modal,
                                 bool tool)
{
  if (modal) {
    /* Sanity check: Don't allow add-ons to override internal modal key-maps
     * because this isn't supported, the restriction can be removed when
     * add-ons can define modal key-maps.
     * Currently this is only useful for add-ons to override built-in modal keymaps
     * which is not the intended use for add-on keymaps. */
    wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
    if (keyconf == wm->runtime->addonconf) {
      BKE_reportf(reports, RPT_ERROR, "Modal key-maps not supported for add-on key-config");
      return nullptr;
    }
  }

  wmKeyMap *keymap;

  if (modal == 0) {
    keymap = WM_keymap_ensure(keyconf, idname, spaceid, regionid);
  }
  else {
    keymap = WM_modalkeymap_ensure(keyconf, idname, nullptr); /* items will be lazy init */
  }

  if (keymap && tool) {
    keymap->flag |= KEYMAP_TOOL;
  }

  return keymap;
}

static wmKeyMap *rna_KeyMaps_find(wmKeyConfig *keyconf,
                                  const char *idname,
                                  int spaceid,
                                  int regionid)
{
  return WM_keymap_list_find(&keyconf->keymaps, idname, spaceid, regionid);
}

static wmKeyMap *rna_KeyMaps_find_match(wmKeyConfig *keyconf, wmKeyMap *km_match)
{
  return WM_keymap_list_find(
      &keyconf->keymaps, km_match->idname, km_match->spaceid, km_match->regionid);
}

static wmKeyMap *rna_KeyMaps_find_modal(wmKeyConfig * /*keyconf*/, const char *idname)
{
  wmOperatorType *ot = WM_operatortype_find(idname, false);

  if (!ot) {
    return nullptr;
  }
  return ot->modalkeymap;
}

static void rna_KeyMaps_remove(wmKeyConfig *keyconfig, ReportList *reports, PointerRNA *keymap_ptr)
{
  wmKeyMap *keymap = static_cast<wmKeyMap *>(keymap_ptr->data);

  if (UNLIKELY(BLI_findindex(&keyconfig->keymaps, keymap) == -1)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "KeyMap '%s' not found in KeyConfig '%s'",
                keymap->idname,
                keyconfig->idname);
    return;
  }

  WM_keymap_remove(keyconfig, keymap);
  keymap_ptr->invalidate();
}

static void rna_KeyMaps_clear(wmKeyConfig *keyconfig)
{
  WM_keyconfig_clear(keyconfig);
}

wmKeyConfig *rna_KeyConfig_new(wmWindowManager *wm, const char *idname)
{
  return WM_keyconfig_ensure(wm, idname, true);
}

static void rna_KeyConfig_remove(wmWindowManager *wm, ReportList *reports, PointerRNA *keyconf_ptr)
{
  wmKeyConfig *keyconf = static_cast<wmKeyConfig *>(keyconf_ptr->data);
  if (UNLIKELY(BLI_findindex(&wm->runtime->keyconfigs, keyconf) == -1)) {
    BKE_reportf(reports, RPT_ERROR, "KeyConfig '%s' cannot be removed", keyconf->idname);
    return;
  }
  WM_keyconfig_remove(wm, keyconf);
  keyconf_ptr->invalidate();
}

static PointerRNA rna_KeyConfig_find_item_from_operator(wmWindowManager *wm,
                                                        bContext *C,
                                                        const char *idname,
                                                        int opcontext,
                                                        PointerRNA *properties,
                                                        int include_mask,
                                                        int exclude_mask,
                                                        PointerRNA *km_ptr)
{
  char idname_bl[OP_MAX_TYPENAME];
  WM_operator_bl_idname(idname_bl, idname);

  wmKeyMap *km = nullptr;
  wmKeyMapItem *kmi = WM_key_event_operator(C,
                                            idname_bl,
                                            blender::wm::OpCallContext(opcontext),
                                            static_cast<IDProperty *>(properties->data),
                                            include_mask,
                                            exclude_mask,
                                            &km);
  *km_ptr = RNA_pointer_create_discrete(&wm->id, &RNA_KeyMap, km);
  PointerRNA kmi_ptr = RNA_pointer_create_discrete(&wm->id, &RNA_KeyMapItem, kmi);
  return kmi_ptr;
}

static void rna_KeyConfig_update(wmWindowManager *wm, bool keep_properties)
{
  WM_keyconfig_update_ex(wm, keep_properties);
}

/** Check the context that popup is can be used. */
static bool rna_popup_context_ok_or_report(bContext *C, ReportList *reports)
{
  if (CTX_wm_window(C) == nullptr) {
    BKE_report(reports, RPT_ERROR, "context \"window\" is None");
    return false;
  }
  return true;
}

/* popup menu wrapper */
static PointerRNA rna_PopMenuBegin(bContext *C,
                                   ReportList *reports,
                                   const char *title,
                                   const int icon)
{
  if (!rna_popup_context_ok_or_report(C, reports)) {
    return PointerRNA_NULL;
  }

  void *data = (void *)UI_popup_menu_begin(C, title, icon);
  PointerRNA ptr_result = RNA_pointer_create_discrete(nullptr, &RNA_UIPopupMenu, data);
  return ptr_result;
}

static void rna_PopMenuEnd(bContext *C, PointerRNA *handle)
{
  UI_popup_menu_end(C, static_cast<uiPopupMenu *>(handle->data));
}

/* popover wrapper */
static PointerRNA rna_PopoverBegin(bContext *C,
                                   ReportList *reports,
                                   const int ui_units_x,
                                   const bool from_active_button)
{
  if (!rna_popup_context_ok_or_report(C, reports)) {
    return PointerRNA_NULL;
  }

  void *data = (void *)UI_popover_begin(C, U.widget_unit * ui_units_x, from_active_button);
  PointerRNA ptr_result = RNA_pointer_create_discrete(nullptr, &RNA_UIPopover, data);
  return ptr_result;
}

static void rna_PopoverEnd(bContext *C, PointerRNA *handle, wmKeyMap *keymap)
{
  UI_popover_end(C, static_cast<uiPopover *>(handle->data), keymap);
}

/* pie menu wrapper */
static PointerRNA rna_PieMenuBegin(
    bContext *C, ReportList *reports, const char *title, const int icon, PointerRNA *event)
{
  if (!rna_popup_context_ok_or_report(C, reports)) {
    return PointerRNA_NULL;
  }

  void *data = (void *)UI_pie_menu_begin(
      C, title, icon, static_cast<const wmEvent *>(event->data));

  PointerRNA ptr_result = RNA_pointer_create_discrete(nullptr, &RNA_UIPieMenu, data);
  return ptr_result;
}

static void rna_PieMenuEnd(bContext *C, PointerRNA *handle)
{
  UI_pie_menu_end(C, static_cast<uiPieMenu *>(handle->data));
}

static void rna_WindowManager_print_undo_steps(wmWindowManager *wm)
{
  BKE_undosys_print(wm->runtime->undo_stack);
}

static void rna_WindowManager_tag_script_reload()
{
  WM_script_tag_reload();
  WM_main_add_notifier(NC_WINDOW, nullptr);
}

static PointerRNA rna_WindoManager_operator_properties_last(const char *idname)
{
  wmOperatorType *ot = WM_operatortype_find(idname, true);

  if (ot != nullptr) {
    PointerRNA ptr;
    WM_operator_last_properties_ensure(ot, &ptr);
    return ptr;
  }
  return PointerRNA_NULL;
}

static wmEvent *rna_Window_event_add_simulate(wmWindow *win,
                                              ReportList *reports,
                                              int type,
                                              int value,
                                              const char *unicode,
                                              int x,
                                              int y,
                                              bool shift,
                                              bool ctrl,
                                              bool alt,
                                              bool oskey,
                                              bool hyper)
{
  if ((G.f & G_FLAG_EVENT_SIMULATE) == 0) {
    BKE_report(reports, RPT_ERROR, "Not running with '--enable-event-simulate' enabled");
    return nullptr;
  }

  if (!ELEM(value, KM_PRESS, KM_RELEASE, KM_NOTHING)) {
    BKE_report(reports, RPT_ERROR, "Value: only 'PRESS/RELEASE/NOTHING' are supported");
    return nullptr;
  }
  if (ISKEYBOARD(type) || ISMOUSE_BUTTON(type)) {
    if (!ELEM(value, KM_PRESS, KM_RELEASE)) {
      BKE_report(reports, RPT_ERROR, "Value: must be 'PRESS/RELEASE' for keyboard/buttons");
      return nullptr;
    }
  }
  if (ISMOUSE_MOTION(type)) {
    if (value != KM_NOTHING) {
      BKE_report(reports, RPT_ERROR, "Value: must be 'NOTHING' for motion");
      return nullptr;
    }
  }
  if (unicode != nullptr) {
    if (value != KM_PRESS) {
      BKE_report(reports, RPT_ERROR, "Value: must be 'PRESS' when unicode is set");
      return nullptr;
    }
  }
  /* TODO: validate NDOF. */

  if (unicode != nullptr) {
    int len = BLI_str_utf8_size_or_error(unicode);
    if (len == -1 || unicode[len] != '\0') {
      BKE_report(reports, RPT_ERROR, "Only a single character supported");
      return nullptr;
    }
  }

  wmEvent e = *win->eventstate;
  e.type = wmEventType(type);
  e.val = value;
  e.flag = eWM_EventFlag(0);
  e.xy[0] = x;
  e.xy[1] = y;

  e.modifier = wmEventModifierFlag(0);
  if (shift) {
    e.modifier |= KM_SHIFT;
  }
  if (ctrl) {
    e.modifier |= KM_CTRL;
  }
  if (alt) {
    e.modifier |= KM_ALT;
  }
  if (oskey) {
    e.modifier |= KM_OSKEY;
  }
  if (hyper) {
    e.modifier |= KM_HYPER;
  }

  e.utf8_buf[0] = '\0';
  if (unicode != nullptr) {
    STRNCPY(e.utf8_buf, unicode);
  }

  /* Until we expose setting tablet values here. */
  WM_event_tablet_data_default_set(&e.tablet);

  return WM_event_add_simulate(win, &e);
}

#else

#  define WM_GEN_INVOKE_EVENT (1 << 0)
#  define WM_GEN_INVOKE_SIZE (1 << 1)
#  define WM_GEN_INVOKE_RETURN (1 << 2)

static void rna_generic_op_invoke(FunctionRNA *func, int flag)
{
  PropertyRNA *parm;

  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "operator", "Operator", "", "Operator to call");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  if (flag & WM_GEN_INVOKE_EVENT) {
    parm = RNA_def_pointer(func, "event", "Event", "", "Event");
    RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  }

  if (flag & WM_GEN_INVOKE_SIZE) {
    RNA_def_int(func, "width", 300, 0, INT_MAX, "", "Width of the popup", 0, INT_MAX);
  }

  if (flag & WM_GEN_INVOKE_RETURN) {
    parm = RNA_def_enum_flag(
        func, "result", rna_enum_operator_return_items, OPERATOR_FINISHED, "result", "");
    RNA_def_function_return(func, parm);
  }
}

void RNA_api_window(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "cursor_warp", "WM_cursor_warp");
  parm = RNA_def_int(func, "x", 0, INT_MIN, INT_MAX, "", "", INT_MIN, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "y", 0, INT_MIN, INT_MAX, "", "", INT_MIN, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_function_ui_description(func, "Set the cursor position");

  func = RNA_def_function(srna, "cursor_set", "WM_cursor_set");
  parm = RNA_def_property(func, "cursor", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(parm, rna_enum_window_cursor_items);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_function_ui_description(func, "Set the cursor");

  func = RNA_def_function(srna, "cursor_modal_set", "WM_cursor_modal_set");
  parm = RNA_def_property(func, "cursor", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(parm, rna_enum_window_cursor_items);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_function_ui_description(func, "Set the cursor, so the previous cursor can be restored");

  func = RNA_def_function(srna, "cursor_modal_restore", "WM_cursor_modal_restore");
  RNA_def_function_ui_description(
      func, "Restore the previous cursor after calling ``cursor_modal_set``");

  /* Arguments match 'rna_KeyMap_item_new'. */
  func = RNA_def_function(srna, "event_simulate", "rna_Window_event_add_simulate");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_enum(func, "type", rna_enum_event_type_items, 0, "Type", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func, "value", rna_enum_event_value_items, 0, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "unicode", nullptr, 0, "", "");
  RNA_def_parameter_clear_flags(parm, PROP_NEVER_NULL, ParameterFlag(0));

  RNA_def_int(func, "x", 0, INT_MIN, INT_MAX, "", "", INT_MIN, INT_MAX);
  RNA_def_int(func, "y", 0, INT_MIN, INT_MAX, "", "", INT_MIN, INT_MAX);

  RNA_def_boolean(func, "shift", false, "Shift", "");
  RNA_def_boolean(func, "ctrl", false, "Ctrl", "");
  RNA_def_boolean(func, "alt", false, "Alt", "");
  RNA_def_boolean(func, "oskey", false, "OS Key", "");
  RNA_def_boolean(func, "hyper", false, "Hyper", "");
  parm = RNA_def_pointer(func, "event", "Event", "Item", "Added key map item");
  RNA_def_function_return(func, parm);
}

const EnumPropertyItem rna_operator_popup_icon_items[] = {
    {ALERT_ICON_NONE, "NONE", 0, "None", ""},
    {ALERT_ICON_WARNING, "WARNING", 0, "Warning", ""},
    {ALERT_ICON_QUESTION, "QUESTION", 0, "Question", ""},
    {ALERT_ICON_ERROR, "ERROR", 0, "Error", ""},
    {ALERT_ICON_INFO, "INFO", 0, "Info", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

void RNA_api_wm(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "fileselect_add", "WM_event_add_fileselect");
  /* Note that a full description is located at:
   * `doc/python_api/examples/bpy.types.WindowManager.fileselect_add.py`. */
  RNA_def_function_ui_description(func, "Opens a file selector with an operator.");
  rna_generic_op_invoke(func, 0);

  func = RNA_def_function(srna, "modal_handler_add", "rna_event_modal_handler_add");
  RNA_def_function_ui_description(
      func,
      "Add a modal handler to the window manager, for the given modal operator "
      "(called by invoke() with self, just before returning {'RUNNING_MODAL'})");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "operator", "Operator", "", "Operator to call");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_function_return(
      func,
      RNA_def_boolean(func, "handle", true, "", "Whether adding the handler was successful"));

  func = RNA_def_function(srna, "event_timer_add", "rna_event_timer_add");
  RNA_def_function_ui_description(
      func, "Add a timer to the given window, to generate periodic 'TIMER' events");
  parm = RNA_def_property(func, "time_step", PROP_FLOAT, PROP_NONE);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_property_range(parm, 0.0, FLT_MAX);
  RNA_def_property_ui_text(parm, "Time Step", "Interval in seconds between timer events");
  RNA_def_pointer(func, "window", "Window", "", "Window to attach the timer to, or None");
  parm = RNA_def_pointer(func, "result", "Timer", "", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "event_timer_remove", "rna_event_timer_remove");
  parm = RNA_def_pointer(func, "timer", "Timer", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "gizmo_group_type_ensure", "rna_gizmo_group_type_ensure");
  RNA_def_function_ui_description(
      func, "Activate an existing widget group (when the persistent option isn't set)");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "identifier", nullptr, 0, "", "Gizmo group type name");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(
      srna, "gizmo_group_type_unlink_delayed", "rna_gizmo_group_type_unlink_delayed");
  RNA_def_function_ui_description(func,
                                  "Unlink a widget group (when the persistent option is set)");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "identifier", nullptr, 0, "", "Gizmo group type name");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* Progress bar interface */
  func = RNA_def_function(srna, "progress_begin", "rna_progress_begin");
  RNA_def_function_ui_description(func, "Start progress report");
  parm = RNA_def_property(func, "min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(parm, "min", "any value in range [0,9999]");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_property(func, "max", PROP_FLOAT, PROP_NONE);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_property_ui_text(parm, "max", "any value in range [min+1,9998]");

  func = RNA_def_function(srna, "progress_update", "rna_progress_update");
  RNA_def_function_ui_description(func, "Update the progress feedback");
  parm = RNA_def_property(func, "value", PROP_FLOAT, PROP_NONE);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_property_ui_text(
      parm, "value", "Any value between min and max as set in progress_begin()");

  func = RNA_def_function(srna, "progress_end", "rna_progress_end");
  RNA_def_function_ui_description(func, "Terminate progress report");

  /* invoke functions, for use with python */
  func = RNA_def_function(srna, "invoke_props_popup", "rna_Operator_props_popup");
  RNA_def_function_ui_description(
      func,
      "Operator popup invoke "
      "(show operator properties and execute it automatically on changes)");
  rna_generic_op_invoke(func, WM_GEN_INVOKE_EVENT | WM_GEN_INVOKE_RETURN);

  /* invoked dialog opens popup with OK button, does not auto-exec operator. */
  func = RNA_def_function(srna, "invoke_props_dialog", "rna_Operator_props_dialog_popup");
  RNA_def_function_ui_description(
      func,
      "Operator dialog (non-autoexec popup) invoke "
      "(show operator properties and only execute it on click on OK button)");
  rna_generic_op_invoke(func, WM_GEN_INVOKE_SIZE | WM_GEN_INVOKE_RETURN);

  parm = RNA_def_property(func, "title", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(parm, "Title", "Optional text to show as title of the popup");
  parm = RNA_def_property(func, "confirm_text", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      parm,
      "Confirm Text",
      "Optional text to show instead to the default \"OK\" confirmation button text");
  RNA_def_property(func, "cancel_default", PROP_BOOLEAN, PROP_NONE);
  api_ui_item_common_translation(func);

  /* invoke enum */
  func = RNA_def_function(srna, "invoke_search_popup", "rna_Operator_enum_search_invoke");
  RNA_def_function_ui_description(
      func,
      "Operator search popup invoke which "
      "searches values of the operator's :class:`bpy.types.Operator.bl_property` "
      "(which must be an EnumProperty), executing it on confirmation");
  rna_generic_op_invoke(func, 0);

  /* invoke functions, for use with python */
  func = RNA_def_function(srna, "invoke_popup", "rna_Operator_ui_popup");
  RNA_def_function_ui_description(func,
                                  "Operator popup invoke "
                                  "(only shows operator's properties, without executing it)");
  rna_generic_op_invoke(func, WM_GEN_INVOKE_SIZE | WM_GEN_INVOKE_RETURN);

  func = RNA_def_function(srna, "invoke_confirm", "rna_Operator_confirm");
  RNA_def_function_ui_description(
      func,
      "Operator confirmation popup "
      "(only to let user confirm the execution, no operator properties shown)");
  rna_generic_op_invoke(func, WM_GEN_INVOKE_EVENT | WM_GEN_INVOKE_RETURN);

  parm = RNA_def_property(func, "title", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(parm, "Title", "Optional text to show as title of the popup");

  parm = RNA_def_property(func, "message", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(parm, "Message", "Optional first line of content text");

  parm = RNA_def_property(func, "confirm_text", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      parm,
      "Confirm Text",
      "Optional text to show instead to the default \"OK\" confirmation button text");

  parm = RNA_def_property(func, "icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(parm, rna_operator_popup_icon_items);
  RNA_def_property_enum_default(parm, ALERT_ICON_NONE);
  RNA_def_property_ui_text(parm, "Icon", "Optional icon displayed in the dialog");

  api_ui_item_common_translation(func);

  /* wrap UI_popup_menu_begin */
  func = RNA_def_function(srna, "popmenu_begin__internal", "rna_PopMenuBegin");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "title", nullptr, 0, "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_property(func, "icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(parm, rna_enum_icon_items);
  /* return */
  parm = RNA_def_pointer(func, "menu", "UIPopupMenu", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  /* wrap UI_popup_menu_end */
  func = RNA_def_function(srna, "popmenu_end__internal", "rna_PopMenuEnd");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "menu", "UIPopupMenu", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_RNAPTR | PARM_REQUIRED);

  /* wrap UI_popover_begin */
  func = RNA_def_function(srna, "popover_begin__internal", "rna_PopoverBegin");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_property(func, "ui_units_x", PROP_INT, PROP_UNSIGNED);
  /* return */
  parm = RNA_def_pointer(func, "menu", "UIPopover", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_RNAPTR);
  RNA_def_function_return(func, parm);
  RNA_def_boolean(
      func, "from_active_button", false, "Use Button", "Use the active button for positioning");

  /* wrap UI_popover_end */
  func = RNA_def_function(srna, "popover_end__internal", "rna_PopoverEnd");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "menu", "UIPopover", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_RNAPTR | PARM_REQUIRED);
  RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Active key map");

  /* wrap uiPieMenuBegin */
  func = RNA_def_function(srna, "piemenu_begin__internal", "rna_PieMenuBegin");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "title", nullptr, 0, "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_property(func, "icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(parm, rna_enum_icon_items);
  parm = RNA_def_pointer(func, "event", "Event", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_RNAPTR);
  /* return */
  parm = RNA_def_pointer(func, "menu_pie", "UIPieMenu", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  /* wrap uiPieMenuEnd */
  func = RNA_def_function(srna, "piemenu_end__internal", "rna_PieMenuEnd");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "menu", "UIPieMenu", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_RNAPTR | PARM_REQUIRED);

  /* access last operator options (optionally create). */
  func = RNA_def_function(
      srna, "operator_properties_last", "rna_WindoManager_operator_properties_last");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_string(func, "operator", nullptr, 0, "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return */
  parm = RNA_def_pointer(func, "result", "OperatorProperties", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  RNA_def_function(srna, "print_undo_steps", "rna_WindowManager_print_undo_steps");

  /* Used by (#SCRIPT_OT_reload). */
  func = RNA_def_function(srna, "tag_script_reload", "rna_WindowManager_tag_script_reload");
  RNA_def_function_ui_description(
      func, "Tag for refreshing the interface after scripts have been reloaded");
  RNA_def_function_flag(func, FUNC_NO_SELF);

  parm = RNA_def_property(srna, "is_interface_locked", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(parm, nullptr, "runtime->is_interface_locked", 0);
  RNA_def_property_ui_text(
      parm,
      "Is Interface Locked",
      "If true, the interface is currently locked by a running job and data should not be "
      "modified from application timers. Otherwise, the running job might conflict with the "
      "handler causing unexpected results or even crashes.");
  RNA_def_property_clear_flag(parm, PROP_EDITABLE);
}

void RNA_api_operator(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  /* utility, not for registering */
  func = RNA_def_function(srna, "report", "rna_Operator_report");
  parm = RNA_def_enum_flag(func, "type", rna_enum_wm_report_items, 0, "Type", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "message", nullptr, 0, "Report Message", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* utility, not for registering */
  func = RNA_def_function(srna, "is_repeat", "rna_Operator_is_repeat");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  /* return */
  parm = RNA_def_boolean(func, "result", false, "result", "");
  RNA_def_function_return(func, parm);

  /* Registration */

  /* poll */
  func = RNA_def_function(srna, "poll", nullptr);
  RNA_def_function_ui_description(func, "Test if the operator can be called or not");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* exec */
  func = RNA_def_function(srna, "execute", nullptr);
  RNA_def_function_ui_description(func, "Execute the operator");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* better name? */
  parm = RNA_def_enum_flag(
      func, "result", rna_enum_operator_return_items, OPERATOR_FINISHED, "result", "");
  RNA_def_function_return(func, parm);

  /* check */
  func = RNA_def_function(srna, "check", nullptr);
  RNA_def_function_ui_description(
      func, "Check the operator settings, return True to signal a change to redraw");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  parm = RNA_def_boolean(func, "result", false, "result", ""); /* better name? */
  RNA_def_function_return(func, parm);

  /* invoke */
  func = RNA_def_function(srna, "invoke", nullptr);
  RNA_def_function_ui_description(func, "Invoke the operator");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "event", "Event", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* better name? */
  parm = RNA_def_enum_flag(
      func, "result", rna_enum_operator_return_items, OPERATOR_FINISHED, "result", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "modal", nullptr); /* same as invoke */
  RNA_def_function_ui_description(func, "Modal operator function");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "event", "Event", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* better name? */
  parm = RNA_def_enum_flag(
      func, "result", rna_enum_operator_return_items, OPERATOR_FINISHED, "result", "");
  RNA_def_function_return(func, parm);

  /* draw */
  func = RNA_def_function(srna, "draw", nullptr);
  RNA_def_function_ui_description(func, "Draw function for the operator");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* cancel */
  func = RNA_def_function(srna, "cancel", nullptr);
  RNA_def_function_ui_description(func, "Called when the operator is canceled");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* description */
  func = RNA_def_function(srna, "description", nullptr);
  RNA_def_function_ui_description(func, "Compute a description string that depends on parameters");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_string(func, "result", nullptr, 4096, "result", "");
  RNA_def_parameter_clear_flags(parm, PROP_NEVER_NULL, ParameterFlag(0));
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "properties", "OperatorProperties", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
}

void RNA_api_macro(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  /* utility, not for registering */
  func = RNA_def_function(srna, "report", "rna_Operator_report");
  parm = RNA_def_enum_flag(func, "type", rna_enum_wm_report_items, 0, "Type", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "message", nullptr, 0, "Report Message", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* Registration */

  /* poll */
  func = RNA_def_function(srna, "poll", nullptr);
  RNA_def_function_ui_description(func, "Test if the operator can be called or not");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* draw */
  func = RNA_def_function(srna, "draw", nullptr);
  RNA_def_function_ui_description(func, "Draw function for the operator");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
}

void RNA_api_keyconfig(StructRNA * /*srna*/)
{
  // FunctionRNA *func;
  // PropertyRNA *parm;
}

void RNA_api_keymap(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "active", "rna_keymap_active");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Active key map");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "restore_to_default", "rna_keymap_restore_to_default");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);

  func = RNA_def_function(srna, "restore_item_to_default", "rna_keymap_restore_item_to_default");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "item", "KeyMapItem", "Item", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
}

void RNA_api_keymapitem(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "compare", "rna_KeyMapItem_compare");
  parm = RNA_def_pointer(func, "item", "KeyMapItem", "Item", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_boolean(func, "result", false, "Comparison result", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "to_string", "rna_KeyMapItem_to_string");
  RNA_def_boolean(func, "compact", false, "Compact", "");
  parm = RNA_def_string(func, "result", nullptr, UI_MAX_SHORTCUT_STR, "result", "");
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);
}

void RNA_api_keymapitems(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "new", "rna_KeyMap_item_new");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "idname", nullptr, 0, "Operator Identifier", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func, "type", rna_enum_event_type_items, 0, "Type", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func, "value", rna_enum_event_value_items, 0, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func, "any", false, "Any", "");
  RNA_def_int(func, "shift", KM_NOTHING, KM_ANY, KM_MOD_HELD, "Shift", "", KM_ANY, KM_MOD_HELD);
  RNA_def_int(func, "ctrl", KM_NOTHING, KM_ANY, KM_MOD_HELD, "Ctrl", "", KM_ANY, KM_MOD_HELD);
  RNA_def_int(func, "alt", KM_NOTHING, KM_ANY, KM_MOD_HELD, "Alt", "", KM_ANY, KM_MOD_HELD);
  RNA_def_int(func, "oskey", KM_NOTHING, KM_ANY, KM_MOD_HELD, "OS Key", "", KM_ANY, KM_MOD_HELD);
  RNA_def_int(func, "hyper", KM_NOTHING, KM_ANY, KM_MOD_HELD, "Hyper", "", KM_ANY, KM_MOD_HELD);
  RNA_def_enum(func, "key_modifier", rna_enum_event_type_items, 0, "Key Modifier", "");
  RNA_def_enum(func, "direction", rna_enum_event_direction_items, KM_ANY, "Direction", "");
  RNA_def_boolean(func, "repeat", false, "Repeat", "When set, accept key-repeat events");
  RNA_def_boolean(func,
                  "head",
                  false,
                  "At Head",
                  "Force item to be added at start (not end) of key map so that "
                  "it doesn't get blocked by an existing key map item");
  parm = RNA_def_pointer(func, "item", "KeyMapItem", "Item", "Added key map item");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_modal", "rna_KeyMap_item_new_modal");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "propvalue", nullptr, 0, "Property Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func, "type", rna_enum_event_type_items, 0, "Type", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func, "value", rna_enum_event_value_items, 0, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func, "any", false, "Any", "");
  RNA_def_int(func, "shift", KM_NOTHING, KM_ANY, KM_MOD_HELD, "Shift", "", KM_ANY, KM_MOD_HELD);
  RNA_def_int(func, "ctrl", KM_NOTHING, KM_ANY, KM_MOD_HELD, "Ctrl", "", KM_ANY, KM_MOD_HELD);
  RNA_def_int(func, "alt", KM_NOTHING, KM_ANY, KM_MOD_HELD, "Alt", "", KM_ANY, KM_MOD_HELD);
  RNA_def_int(func, "oskey", KM_NOTHING, KM_ANY, KM_MOD_HELD, "OS Key", "", KM_ANY, KM_MOD_HELD);
  RNA_def_int(func, "hyper", KM_NOTHING, KM_ANY, KM_MOD_HELD, "Hyper", "", KM_ANY, KM_MOD_HELD);
  RNA_def_enum(func, "key_modifier", rna_enum_event_type_items, 0, "Key Modifier", "");
  RNA_def_enum(func, "direction", rna_enum_event_direction_items, KM_ANY, "Direction", "");
  RNA_def_boolean(func, "repeat", false, "Repeat", "When set, accept key-repeat events");
  parm = RNA_def_pointer(func, "item", "KeyMapItem", "Item", "Added key map item");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_from_item", "rna_KeyMap_item_new_from_item");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "item", "KeyMapItem", "Item", "Item to use as a reference");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_boolean(func, "head", false, "At Head", "");
  parm = RNA_def_pointer(func, "result", "KeyMapItem", "Item", "Added key map item");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_KeyMap_item_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "item", "KeyMapItem", "Item", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "from_id", "WM_keymap_item_find_id");
  parm = RNA_def_property(func, "id", PROP_INT, PROP_NONE);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_property_ui_text(parm, "id", "ID of the item");
  parm = RNA_def_pointer(func, "item", "KeyMapItem", "Item", "");
  RNA_def_function_return(func, parm);

  /* Keymap introspection
   * Args follow: KeyConfigs.find_item_from_operator */
  func = RNA_def_function(srna, "find_from_operator", "rna_KeyMap_item_find_from_operator");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_string(func, "idname", nullptr, 0, "Operator Identifier", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "properties", "OperatorProperties", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_enum_flag(
      func, "include", rna_enum_event_type_mask_items, EVT_TYPE_MASK_ALL, "Include", "");
  RNA_def_enum_flag(func, "exclude", rna_enum_event_type_mask_items, 0, "Exclude", "");
  parm = RNA_def_pointer(func, "item", "KeyMapItem", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "find_match", "rna_KeyMap_item_find_match");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "keymap", "KeyMap", "", "The matching keymap");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "item", "KeyMapItem", "", "The matching keymap item");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func,
                         "result",
                         "KeyMapItem",
                         "",
                         "The keymap item from this keymap which matches the keymap item from the "
                         "arguments passed in");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "match_event", "rna_KeyMap_item_match_event");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "event", "Event", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "item", "KeyMapItem", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_function_return(func, parm);
}

void RNA_api_keymaps(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "new", "rna_KeyMaps_new");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(
      func,
      "Ensure the keymap exists. This will return the one with the given name/space type/region "
      "type, or create a new one if it does not exist yet.");

  parm = RNA_def_string(func, "name", nullptr, 0, "Name", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_enum(func, "space_type", rna_enum_space_type_items, SPACE_EMPTY, "Space Type", "");
  RNA_def_enum(
      func, "region_type", rna_enum_region_type_items, RGN_TYPE_WINDOW, "Region Type", "");
  RNA_def_boolean(func, "modal", false, "Modal", "Keymap for modal operators");
  RNA_def_boolean(func, "tool", false, "Tool", "Keymap for active tools");
  parm = RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Added key map");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_KeyMaps_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Removed key map");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "clear", "rna_KeyMaps_clear");
  RNA_def_function_ui_description(func, "Remove all keymaps.");

  func = RNA_def_function(srna, "find", "rna_KeyMaps_find");
  parm = RNA_def_string(func, "name", nullptr, 0, "Name", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_enum(func, "space_type", rna_enum_space_type_items, SPACE_EMPTY, "Space Type", "");
  RNA_def_enum(
      func, "region_type", rna_enum_region_type_items, RGN_TYPE_WINDOW, "Region Type", "");
  parm = RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Corresponding key map");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "find_match", "rna_KeyMaps_find_match");
  parm = RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "The key map for comparison");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "result", "KeyMap", "Key Map", "Corresponding key map");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "find_modal", "rna_KeyMaps_find_modal");
  parm = RNA_def_string(func, "name", nullptr, 0, "Operator Name", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Corresponding key map");
  RNA_def_function_return(func, parm);
}

void RNA_api_keyconfigs(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "new", "rna_KeyConfig_new"); /* add_keyconfig */
  parm = RNA_def_string(func, "name", nullptr, 0, "Name", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "keyconfig", "KeyConfig", "Key Configuration", "Added key configuration");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_KeyConfig_remove"); /* remove_keyconfig */
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(
      func, "keyconfig", "KeyConfig", "Key Configuration", "Removed key configuration");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  /* Helper functions */

  /* Keymap introspection */
  func = RNA_def_function(
      srna, "find_item_from_operator", "rna_KeyConfig_find_item_from_operator");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_string(func, "idname", nullptr, 0, "Operator Identifier", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_property(func, "context", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(parm, rna_enum_operator_context_items);
  parm = RNA_def_pointer(func, "properties", "OperatorProperties", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_enum_flag(
      func, "include", rna_enum_event_type_mask_items, EVT_TYPE_MASK_ALL, "Include", "");
  RNA_def_enum_flag(func, "exclude", rna_enum_event_type_mask_items, 0, "Exclude", "");
  parm = RNA_def_pointer(func, "keymap", "KeyMap", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR | PARM_OUTPUT);
  parm = RNA_def_pointer(func, "item", "KeyMapItem", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "update", "rna_KeyConfig_update"); /* WM_keyconfig_update */
  RNA_def_boolean(
      func,
      "keep_properties",
      false,
      "Keep Properties",
      "Operator properties are kept to allow the operators to be registered again in the future");
}

#endif
