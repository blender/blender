/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Generic context popup menus.
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_idprop.hh"
#include "BKE_screen.hh"

#include "ED_asset.hh"
#include "ED_buttons.hh"
#include "ED_keyframing.hh"
#include "ED_screen.hh"

#include "UI_abstract_view.hh"
#include "UI_interface_layout.hh"

#include "interface_intern.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#  include "BPY_extern_run.hh"
#endif

#include "WM_api.hh"
#include "WM_types.hh"

/* This hack is needed because we don't have a good way to
 * re-reference keymap items once added: #42944 */
#define USE_KEYMAP_ADD_HACK

/* -------------------------------------------------------------------- */
/** \name Button Context Menu
 * \{ */

static IDProperty *shortcut_property_from_rna(bContext *C, uiBut *but)
{
  using namespace blender;
  /* Compute data path from context to property. */

  /* If this returns null, we won't be able to bind shortcuts to these RNA properties.
   * Support can be added at #wm_context_member_from_ptr. */
  std::optional<std::string> final_data_path = WM_context_path_resolve_property_full(
      C, &but->rnapoin, but->rnaprop, but->rnaindex);
  if (!final_data_path.has_value()) {
    return nullptr;
  }

  /* Create ID property of data path, to pass to the operator. */
  IDProperty *prop = bke::idprop::create_group(__func__).release();
  IDP_AddToGroup(prop, bke::idprop::create("data_path", final_data_path.value()).release());
  return prop;
}

static const char *shortcut_get_operator_property(bContext *C, uiBut *but, IDProperty **r_prop)
{
  using namespace blender;
  if (but->optype) {
    /* Operator */
    *r_prop = (but->opptr && but->opptr->data) ?
                  IDP_CopyProperty(static_cast<IDProperty *>(but->opptr->data)) :
                  nullptr;
    return but->optype->idname;
  }

  if (but->rnaprop) {
    const PropertyType rnaprop_type = RNA_property_type(but->rnaprop);

    if (rnaprop_type == PROP_BOOLEAN) {
      /* Boolean */
      *r_prop = shortcut_property_from_rna(C, but);
      if (*r_prop == nullptr) {
        return nullptr;
      }
      return "WM_OT_context_toggle";
    }
    if (rnaprop_type == PROP_ENUM) {
      /* Enum */
      *r_prop = shortcut_property_from_rna(C, but);
      if (*r_prop == nullptr) {
        return nullptr;
      }
      return "WM_OT_context_menu_enum";
    }
  }

  if (MenuType *mt = UI_but_menutype_get(but)) {
    IDProperty *prop = bke::idprop::create_group(__func__).release();
    IDP_AddToGroup(prop, bke::idprop::create("name", mt->idname).release());
    *r_prop = prop;
    return "WM_OT_call_menu";
  }

  if (std::optional asset_shelf_idname = UI_but_asset_shelf_type_idname_get(but)) {
    IDProperty *prop = blender::bke::idprop::create_group(__func__).release();
    IDP_AddToGroup(prop, bke::idprop::create("name", *asset_shelf_idname).release());
    *r_prop = prop;
    return "WM_OT_call_asset_shelf_popover";
  }

  if (PanelType *pt = UI_but_paneltype_get(but)) {
    IDProperty *prop = blender::bke::idprop::create_group(__func__).release();
    IDP_AddToGroup(prop, bke::idprop::create("name", pt->idname).release());
    *r_prop = prop;
    return "WM_OT_call_panel";
  }

  *r_prop = nullptr;
  return nullptr;
}

static void shortcut_free_operator_property(IDProperty *prop)
{
  if (prop) {
    IDP_FreeProperty(prop);
  }
}

static void but_shortcut_name_func(bContext *C, void *arg1, int /*event*/)
{
  uiBut *but = (uiBut *)arg1;

  IDProperty *prop;
  const char *idname = shortcut_get_operator_property(C, but, &prop);
  if (idname == nullptr) {
    return;
  }

  /* complex code to change name of button */
  if (std::optional<std::string> shortcut_str = WM_key_event_operator_string(
          C, idname, but->opcontext, prop, true))
  {
    ui_but_add_shortcut(but, shortcut_str->c_str(), true);
  }
  else {
    /* simply strip the shortcut */
    ui_but_add_shortcut(but, nullptr, true);
  }

  shortcut_free_operator_property(prop);
}

static uiBlock *menu_change_shortcut(bContext *C, ARegion *region, void *arg)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  uiBut *but = (uiBut *)arg;
  const uiStyle *style = UI_style_get_dpi();
  IDProperty *prop;
  const char *idname = shortcut_get_operator_property(C, but, &prop);

  wmKeyMap *km;
  wmKeyMapItem *kmi = WM_key_event_operator(C,
                                            idname,
                                            but->opcontext,
                                            prop,
                                            EVT_TYPE_MASK_HOTKEY_INCLUDE,
                                            EVT_TYPE_MASK_HOTKEY_EXCLUDE,
                                            &km);
  U.runtime.is_dirty = true;

  BLI_assert(kmi != nullptr);

  PointerRNA ptr = RNA_pointer_create_discrete(&wm->id, &RNA_KeyMapItem, kmi);

  uiBlock *block = UI_block_begin(C, region, "_popup", blender::ui::EmbossType::Emboss);
  UI_block_func_handle_set(block, but_shortcut_name_func, but);
  UI_block_flag_enable(block, UI_BLOCK_MOVEMOUSE_QUIT);
  UI_block_direction_set(block, UI_DIR_CENTER_Y);

  uiLayout &layout = blender::ui::block_layout(block,
                                               blender::ui::LayoutDirection::Vertical,
                                               blender::ui::LayoutType::Panel,
                                               0,
                                               0,
                                               U.widget_unit * 10,
                                               U.widget_unit * 2,
                                               0,
                                               style);

  layout.label(CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Change Shortcut"), ICON_HAND);
  layout.prop(&ptr, "type", UI_ITEM_R_FULL_EVENT | UI_ITEM_R_IMMEDIATE, "", ICON_NONE);

  const int bounds_offset[2] = {int(-100 * UI_SCALE_FAC), int(36 * UI_SCALE_FAC)};
  UI_block_bounds_set_popup(block, 6 * UI_SCALE_FAC, bounds_offset);

  shortcut_free_operator_property(prop);

  return block;
}

#ifdef USE_KEYMAP_ADD_HACK
static int g_kmi_id_hack;
#endif

static uiBlock *menu_add_shortcut(bContext *C, ARegion *region, void *arg)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  uiBut *but = (uiBut *)arg;
  const uiStyle *style = UI_style_get_dpi();
  IDProperty *prop;
  const char *idname = shortcut_get_operator_property(C, but, &prop);

  /* XXX this guess_opname can potentially return a different keymap
   * than being found on adding later... */
  wmKeyMap *km = WM_keymap_guess_opname(C, idname);
  KeyMapItem_Params params{};
  params.type = EVT_AKEY;
  params.value = KM_PRESS;
  params.modifier = 0;
  params.direction = KM_ANY;
  wmKeyMapItem *kmi = WM_keymap_add_item(km, idname, &params);
  const int kmi_id = kmi->id;

  /* This takes ownership of prop, or prop can be nullptr for reset. */
  WM_keymap_item_properties_reset(kmi, prop);

  /* update and get pointers again */
  WM_keyconfig_update(wm);
  U.runtime.is_dirty = true;

  km = WM_keymap_guess_opname(C, idname);
  kmi = WM_keymap_item_find_id(km, kmi_id);

  PointerRNA ptr = RNA_pointer_create_discrete(&wm->id, &RNA_KeyMapItem, kmi);

  uiBlock *block = UI_block_begin(C, region, "_popup", blender::ui::EmbossType::Emboss);
  UI_block_func_handle_set(block, but_shortcut_name_func, but);
  UI_block_direction_set(block, UI_DIR_CENTER_Y);

  uiLayout &layout = blender::ui::block_layout(block,
                                               blender::ui::LayoutDirection::Vertical,
                                               blender::ui::LayoutType::Panel,
                                               0,
                                               0,
                                               U.widget_unit * 10,
                                               U.widget_unit * 2,
                                               0,
                                               style);

  layout.label(CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Assign Shortcut"), ICON_HAND);
  layout.prop(&ptr, "type", UI_ITEM_R_FULL_EVENT | UI_ITEM_R_IMMEDIATE, "", ICON_NONE);

  const int bounds_offset[2] = {int(-100 * UI_SCALE_FAC), int(36 * UI_SCALE_FAC)};
  UI_block_bounds_set_popup(block, 6 * UI_SCALE_FAC, bounds_offset);

#ifdef USE_KEYMAP_ADD_HACK
  g_kmi_id_hack = kmi_id;
#endif

  return block;
}

static void menu_add_shortcut_cancel(bContext *C, void *arg1)
{
  uiBut *but = (uiBut *)arg1;

  IDProperty *prop;
  const char *idname = shortcut_get_operator_property(C, but, &prop);

#ifdef USE_KEYMAP_ADD_HACK
  wmKeyMap *km = WM_keymap_guess_opname(C, idname);
  const int kmi_id = g_kmi_id_hack;
  UNUSED_VARS(but);
#else
  int kmi_id = WM_key_event_operator_id(C, idname, but->opcontext, prop, true, &km);
#endif

  shortcut_free_operator_property(prop);

  wmKeyMapItem *kmi = WM_keymap_item_find_id(km, kmi_id);
  WM_keymap_remove_item(km, kmi);
}

static void remove_shortcut_func(bContext *C, uiBut *but)
{
  IDProperty *prop;
  const char *idname = shortcut_get_operator_property(C, but, &prop);

  wmKeyMap *km;
  wmKeyMapItem *kmi = WM_key_event_operator(C,
                                            idname,
                                            but->opcontext,
                                            prop,
                                            EVT_TYPE_MASK_HOTKEY_INCLUDE,
                                            EVT_TYPE_MASK_HOTKEY_EXCLUDE,
                                            &km);
  BLI_assert(kmi != nullptr);

  WM_keymap_remove_item(km, kmi);
  U.runtime.is_dirty = true;

  shortcut_free_operator_property(prop);
  but_shortcut_name_func(C, but, 0);
}

static bool ui_but_is_user_menu_compatible(bContext *C, uiBut *but)
{
  bool result = false;
  if (but->optype) {
    result = true;
  }
  else if (but->rnaprop) {
    if (RNA_property_type(but->rnaprop) == PROP_BOOLEAN) {
      std::optional<std::string> data_path = WM_context_path_resolve_full(C, &but->rnapoin);
      if (data_path.has_value()) {
        result = true;
      }
    }
  }
  else if (UI_but_menutype_get(but)) {
    result = true;
  }
  else if (UI_but_operatortype_get_from_enum_menu(but, nullptr)) {
    result = true;
  }

  return result;
}

static bUserMenuItem *ui_but_user_menu_find(bContext *C, uiBut *but, bUserMenu *um)
{
  if (but->optype) {
    IDProperty *prop = (but->opptr) ? static_cast<IDProperty *>(but->opptr->data) : nullptr;
    return (bUserMenuItem *)ED_screen_user_menu_item_find_operator(
        &um->items, but->optype, prop, "", but->opcontext);
  }
  if (but->rnaprop) {
    std::optional<std::string> member_id_data_path = WM_context_path_resolve_full(C,
                                                                                  &but->rnapoin);
    /* NOTE(@ideasman42): It's highly unlikely a this ever occurs since the path must be resolved
     * for this to be added in the first place, there might be some cases where manually
     * constructed RNA paths don't resolve and in this case a crash should be avoided. */
    if (UNLIKELY(!member_id_data_path.has_value())) {
      /* Assert because this should never happen for typical usage. */
      BLI_assert_unreachable();
      return nullptr;
    }
    /* Ignore the actual array index [pass -1] since the index is handled separately. */
    const std::string prop_id = RNA_property_is_idprop(but->rnaprop) ?
                                    RNA_path_property_py(&but->rnapoin, but->rnaprop, -1) :
                                    RNA_property_identifier(but->rnaprop);
    bUserMenuItem *umi = (bUserMenuItem *)ED_screen_user_menu_item_find_prop(
        &um->items, member_id_data_path->c_str(), prop_id.c_str(), but->rnaindex);
    return umi;
  }

  wmOperatorType *ot = nullptr;
  PropertyRNA *prop_enum = nullptr;
  if ((ot = UI_but_operatortype_get_from_enum_menu(but, &prop_enum))) {
    return (bUserMenuItem *)ED_screen_user_menu_item_find_operator(
        &um->items, ot, nullptr, RNA_property_identifier(prop_enum), but->opcontext);
  }

  MenuType *mt = UI_but_menutype_get(but);
  if (mt != nullptr) {
    return (bUserMenuItem *)ED_screen_user_menu_item_find_menu(&um->items, mt);
  }
  return nullptr;
}

static void ui_but_user_menu_add(bContext *C, uiBut *but, bUserMenu *um)
{
  BLI_assert(ui_but_is_user_menu_compatible(C, but));

  std::string drawstr = ui_but_drawstr_without_sep_char(but);

  /* Used for USER_MENU_TYPE_MENU. */
  MenuType *mt = nullptr;
  /* Used for USER_MENU_TYPE_OPERATOR (property enum used). */
  wmOperatorType *ot = nullptr;
  PropertyRNA *prop = nullptr;
  if (but->optype) {
    if (drawstr[0] == '\0') {
      /* Hard code overrides for generic operators. */
      if (UI_but_is_tool(but)) {
        char idname[64];
        RNA_string_get(but->opptr, "name", idname);
#ifdef WITH_PYTHON
        {
          const char *expr_imports[] = {"bpy", "bl_ui", nullptr};
          char expr[256];
          SNPRINTF_UTF8(expr,
                        "bl_ui.space_toolsystem_common.item_from_id("
                        "bpy.context, "
                        "bpy.context.space_data.type, "
                        "'%s').label",
                        idname);
          char *expr_result = nullptr;
          if (BPY_run_string_as_string(C, expr_imports, expr, nullptr, &expr_result)) {
            drawstr = expr_result;
            MEM_freeN(expr_result);
          }
          else {
            BLI_assert(0);
            drawstr = idname;
          }
        }
#else
        drawstr = idname;
#endif
      }
      else if (but->tip_quick_func) {
        /* The "quick tooltip" often contains a short string that can be used as a fallback. */
        drawstr = but->tip_quick_func(but);
      }
    }
    ED_screen_user_menu_item_add_operator(
        &um->items,
        drawstr.c_str(),
        but->optype,
        but->opptr ? static_cast<const IDProperty *>(but->opptr->data) : nullptr,
        "",
        but->opcontext);
  }
  else if (but->rnaprop) {
    /* NOTE: 'member_id' may be a path. */
    std::optional<std::string> member_id_data_path = WM_context_path_resolve_full(C,
                                                                                  &but->rnapoin);
    if (!member_id_data_path.has_value()) {
      /* See #ui_but_user_menu_find code-comment. */
      BLI_assert_unreachable();
    }
    else {
      /* Ignore the actual array index [pass -1] since the index is handled separately. */
      const std::string prop_id = RNA_property_is_idprop(but->rnaprop) ?
                                      RNA_path_property_py(&but->rnapoin, but->rnaprop, -1) :
                                      RNA_property_identifier(but->rnaprop);
      /* NOTE: ignore 'drawstr', use property idname always. */
      ED_screen_user_menu_item_add_prop(
          &um->items, "", member_id_data_path->c_str(), prop_id.c_str(), but->rnaindex);
    }
  }
  else if ((mt = UI_but_menutype_get(but))) {
    ED_screen_user_menu_item_add_menu(&um->items, drawstr.c_str(), mt);
  }
  else if ((ot = UI_but_operatortype_get_from_enum_menu(but, &prop))) {
    ED_screen_user_menu_item_add_operator(&um->items,
                                          WM_operatortype_name(ot, nullptr).c_str(),
                                          ot,
                                          nullptr,
                                          RNA_property_identifier(prop),
                                          but->opcontext);
  }
}

static bool ui_but_menu_add_path_operators(uiLayout *layout, PointerRNA *ptr, PropertyRNA *prop)
{
  const PropertySubType subtype = RNA_property_subtype(prop);
  wmOperatorType *ot = WM_operatortype_find("WM_OT_path_open", true);
  char filepath[FILE_MAX];
  char dir[FILE_MAXDIR];
  char file[FILE_MAXFILE];
  PointerRNA props_ptr;

  BLI_assert(ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH));
  UNUSED_VARS_NDEBUG(subtype);

  RNA_property_string_get(ptr, prop, filepath);

  if (!BLI_exists(filepath)) {
    return false;
  }

  if (BLI_is_file(filepath)) {
    BLI_assert(subtype == PROP_FILEPATH);
    props_ptr = layout->op(ot,
                           CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Open File Externally"),
                           ICON_NONE,
                           blender::wm::OpCallContext::InvokeDefault,
                           UI_ITEM_NONE);
    RNA_string_set(&props_ptr, "filepath", filepath);
  }
  else {
    /* This is a directory, so ensure it ends in a slash. */
    BLI_path_slash_ensure(filepath, ARRAY_SIZE(filepath));
  }

  BLI_path_split_dir_file(filepath, dir, sizeof(dir), file, sizeof(file));

  props_ptr = layout->op(ot,
                         CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Open Location Externally"),
                         ICON_NONE,
                         blender::wm::OpCallContext::InvokeDefault,
                         UI_ITEM_NONE);
  RNA_string_set(&props_ptr, "filepath", dir);

  return true;
}

static void set_layout_context_from_button(bContext *C, uiLayout *layout, uiBut *but)
{
  if (!but->context) {
    return;
  }
  layout->context_copy(but->context);
  CTX_store_set(C, layout->context_store());
}

bool ui_popup_context_menu_for_button(bContext *C, uiBut *but, const wmEvent *event)
{
  using namespace blender::ed;
  /* ui_but_is_interactive() may let some buttons through that should not get a context menu - it
   * doesn't make sense for them. */
  if (ELEM(but->type, ButType::Label, ButType::Image)) {
    return false;
  }

  uiPopupMenu *pup;
  uiLayout *layout;
  const bContextStore *previous_ctx = CTX_store_get(C);
  {
    pup = UI_popup_menu_begin(C, UI_but_context_menu_title_from_button(*but).c_str(), ICON_NONE);
    layout = UI_popup_menu_layout(pup);

    set_layout_context_from_button(C, layout, but);
    layout->operator_context_set(blender::wm::OpCallContext::InvokeDefault);
  }

  const bool is_disabled = but->flag & UI_BUT_DISABLED;

  if (is_disabled) {
    /* Suppress editing commands. */
  }
  else if (but->type == ButType::Tab) {
    uiButTab *tab = (uiButTab *)but;
    if (tab->menu) {
      UI_menutype_draw(C, tab->menu, layout);
      layout->separator();
    }
  }
  else if (but->rnapoin.data && but->rnaprop) {
    PointerRNA *ptr = &but->rnapoin;
    PropertyRNA *prop = but->rnaprop;
    const PropertyType type = RNA_property_type(prop);
    const PropertySubType subtype = RNA_property_subtype(prop);
    bool is_anim = RNA_property_anim_editable(ptr, prop);
    const bool is_idprop = RNA_property_is_idprop(prop);

    /* second slower test,
     * saved people finding keyframe items in menus when its not possible */
    if (is_anim) {
      is_anim = RNA_property_path_from_ID_check(&but->rnapoin, but->rnaprop);
    }

    /* determine if we can key a single component of an array */
    const bool is_array = RNA_property_array_length(&but->rnapoin, but->rnaprop) != 0;
    const bool is_array_component = (is_array && but->rnaindex != -1);
    const bool is_whole_array = (is_array && but->rnaindex == -1);

    const uint override_status = RNA_property_override_library_status(
        CTX_data_main(C), ptr, prop, -1);
    const bool is_overridable = (override_status & RNA_OVERRIDE_STATUS_OVERRIDABLE) != 0;

    /* Set the (button_pointer, button_prop)
     * and pointer data for Python access to the hovered UI element. */
    layout->context_set_from_but(but);

    /* Keyframes */
    if (but->flag & UI_BUT_ANIMATED_KEY) {
      /* Replace/delete keyframes. */
      if (is_array_component) {
        PointerRNA op_ptr = layout->op(
            "ANIM_OT_keyframe_insert_button",
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Replace Keyframes"),
            ICON_KEY_HLT);
        RNA_boolean_set(&op_ptr, "all", true);
        op_ptr = layout->op(
            "ANIM_OT_keyframe_insert_button",
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Replace Single Keyframe"),
            ICON_NONE);
        RNA_boolean_set(&op_ptr, "all", false);
        op_ptr = layout->op("ANIM_OT_keyframe_delete_button",
                            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Keyframes"),
                            ICON_NONE);
        RNA_boolean_set(&op_ptr, "all", true);
        op_ptr = layout->op("ANIM_OT_keyframe_delete_button",
                            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Single Keyframe"),
                            ICON_NONE);
        RNA_boolean_set(&op_ptr, "all", false);
      }
      else {
        PointerRNA op_ptr = layout->op(
            "ANIM_OT_keyframe_insert_button",
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Replace Keyframe"),
            ICON_KEY_HLT);
        RNA_boolean_set(&op_ptr, "all", true);
        op_ptr = layout->op("ANIM_OT_keyframe_delete_button",
                            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Keyframe"),
                            ICON_NONE);
        RNA_boolean_set(&op_ptr, "all", true);
      }

      /* keyframe settings */
      layout->separator();
    }
    else if (but->flag & UI_BUT_DRIVEN) {
      /* pass */
    }
    else if (is_anim) {
      if (is_array_component) {
        PointerRNA op_ptr = layout->op(
            "ANIM_OT_keyframe_insert_button",
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Insert Keyframes"),
            ICON_KEY_HLT);
        RNA_boolean_set(&op_ptr, "all", true);
        op_ptr = layout->op("ANIM_OT_keyframe_insert_button",
                            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Insert Single Keyframe"),
                            ICON_NONE);
        RNA_boolean_set(&op_ptr, "all", false);
      }
      else {
        PointerRNA op_ptr = layout->op(
            "ANIM_OT_keyframe_insert_button",
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Insert Keyframe"),
            ICON_KEY_HLT);
        RNA_boolean_set(&op_ptr, "all", true);
      }
    }

    if ((but->flag & UI_BUT_ANIMATED) && (but->rnapoin.type != &RNA_NlaStrip)) {
      if (is_array_component) {
        PointerRNA op_ptr = layout->op(
            "ANIM_OT_keyframe_clear_button",
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Clear Keyframes"),
            ICON_KEY_DEHLT);
        RNA_boolean_set(&op_ptr, "all", true);
        op_ptr = layout->op("ANIM_OT_keyframe_clear_button",
                            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Clear Single Keyframes"),
                            ICON_NONE);
        RNA_boolean_set(&op_ptr, "all", false);
      }
      else {
        PointerRNA op_ptr = layout->op(
            "ANIM_OT_keyframe_clear_button",
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Clear Keyframes"),
            ICON_KEY_DEHLT);
        RNA_boolean_set(&op_ptr, "all", true);
      }
    }

    if (but->flag & UI_BUT_ANIMATED) {
      layout->separator();
      if (is_array_component) {
        PointerRNA op_ptr;
        wmOperatorType *ot;
        ot = WM_operatortype_find("ANIM_OT_view_curve_in_graph_editor", false);
        op_ptr = layout->op(
            ot,
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "View All in Graph Editor"),
            ICON_GRAPH,
            blender::wm::OpCallContext::InvokeDefault,
            UI_ITEM_NONE);
        RNA_boolean_set(&op_ptr, "all", true);

        op_ptr = layout->op(
            ot,
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "View Single in Graph Editor"),
            ICON_NONE,
            blender::wm::OpCallContext::InvokeDefault,
            UI_ITEM_NONE);
        RNA_boolean_set(&op_ptr, "all", false);
      }
      else {
        PointerRNA op_ptr;
        wmOperatorType *ot;
        ot = WM_operatortype_find("ANIM_OT_view_curve_in_graph_editor", false);

        op_ptr = layout->op(ot,
                            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "View in Graph Editor"),
                            ICON_NONE,
                            blender::wm::OpCallContext::InvokeDefault,
                            UI_ITEM_NONE);
        RNA_boolean_set(&op_ptr, "all", false);
      }
    }

    /* Drivers */
    if (but->flag & UI_BUT_DRIVEN) {
      layout->separator();

      if (is_array_component) {
        PointerRNA op_ptr = layout->op(
            "ANIM_OT_driver_button_remove",
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Drivers"),
            ICON_X);
        RNA_boolean_set(&op_ptr, "all", true);
        op_ptr = layout->op("ANIM_OT_driver_button_remove",
                            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Single Driver"),
                            ICON_NONE);
        RNA_boolean_set(&op_ptr, "all", false);
      }
      else {
        PointerRNA op_ptr = layout->op(
            "ANIM_OT_driver_button_remove",
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Driver"),
            ICON_X);
        RNA_boolean_set(&op_ptr, "all", true);
      }

      if (is_whole_array) {
        PointerRNA op_ptr = layout->op(
            "UI_OT_copy_driver_to_selected_button",
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Copy Drivers to Selected"),
            ICON_NONE);
        RNA_boolean_set(&op_ptr, "all", true);
      }
      else {
        layout->op("ANIM_OT_copy_driver_button",
                   CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Copy Driver"),
                   ICON_NONE);
        if (ANIM_driver_can_paste()) {
          layout->op("ANIM_OT_paste_driver_button",
                     CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Paste Driver"),
                     ICON_NONE);
        }
        PointerRNA op_ptr = layout->op(
            "UI_OT_copy_driver_to_selected_button",
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Copy Driver to Selected"),
            ICON_NONE);
        RNA_boolean_set(&op_ptr, "all", false);
        if (is_array_component) {
          PointerRNA op_ptr = layout->op(
              "UI_OT_copy_driver_to_selected_button",
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Copy All Drivers to Selected"),
              ICON_NONE);
          RNA_boolean_set(&op_ptr, "all", true);
        }

        layout->op("ANIM_OT_driver_button_edit",
                   CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Edit Driver"),
                   ICON_DRIVER);
      }

      layout->op("SCREEN_OT_drivers_editor_show",
                 CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Open Drivers Editor"),
                 ICON_NONE);
    }
    else if (but->flag & (UI_BUT_ANIMATED_KEY | UI_BUT_ANIMATED)) {
      /* pass */
    }
    else if (is_anim) {
      layout->separator();

      layout->op("ANIM_OT_driver_button_add",
                 CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Add Driver"),
                 ICON_DRIVER);

      if (!is_whole_array) {
        if (ANIM_driver_can_paste()) {
          layout->op("ANIM_OT_paste_driver_button",
                     CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Paste Driver"),
                     ICON_NONE);
        }
      }

      layout->op("SCREEN_OT_drivers_editor_show",
                 CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Open Drivers Editor"),
                 ICON_NONE);
    }

    /* Keying Sets */
    /* TODO: check on modifiability of Keying Set when doing this. */
    if (is_anim) {
      layout->separator();

      if (is_array_component) {
        PointerRNA op_ptr = layout->op(
            "ANIM_OT_keyingset_button_add",
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Add All to Keying Set"),
            ICON_KEYINGSET);
        RNA_boolean_set(&op_ptr, "all", true);
        op_ptr = layout->op(
            "ANIM_OT_keyingset_button_add",
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Add Single to Keying Set"),
            ICON_NONE);
        RNA_boolean_set(&op_ptr, "all", false);
        layout->op("ANIM_OT_keyingset_button_remove",
                   CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Remove from Keying Set"),
                   ICON_NONE);
      }
      else {
        PointerRNA op_ptr = layout->op(
            "ANIM_OT_keyingset_button_add",
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Add to Keying Set"),
            ICON_KEYINGSET);
        RNA_boolean_set(&op_ptr, "all", true);
        layout->op("ANIM_OT_keyingset_button_remove",
                   CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Remove from Keying Set"),
                   ICON_NONE);
      }
    }

    if (is_overridable) {
      wmOperatorType *ot;
      PointerRNA op_ptr;
      /* Override Operators */
      layout->separator();

      if (but->flag & UI_BUT_OVERRIDDEN) {
        if (is_array_component) {
#if 0 /* Disabled for now. */
          ot = WM_operatortype_find("UI_OT_override_type_set_button", false);
          op_ptr = layout->op(ot, "Overrides Type", ICON_NONE, blender::wm::OpCallContext::InvokeDefault, 0);
          RNA_boolean_set(&op_ptr, "all", true);
          op_ptr = layout->op(ot, "Single Override Type", ICON_NONE, blender::wm::OpCallContext::InvokeDefault, 0);
          RNA_boolean_set(&op_ptr, "all", false);
#endif
          PointerRNA op_ptr = layout->op(
              "UI_OT_override_remove_button",
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Remove Overrides"),
              ICON_X);
          RNA_boolean_set(&op_ptr, "all", true);
          op_ptr = layout->op(
              "UI_OT_override_remove_button",
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Remove Single Override"),
              ICON_X);
          RNA_boolean_set(&op_ptr, "all", false);
        }
        else {
#if 0 /* Disabled for now. */
          op_ptr = layout->op("UI_OT_override_type_set_button",
                              "Override Type",
                              ICON_NONE,
                              blender::wm::OpCallContext::InvokeDefault,
                              0);
          RNA_boolean_set(&op_ptr, "all", false);
#endif
          PointerRNA op_ptr = layout->op(
              "UI_OT_override_remove_button",
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Remove Override"),
              ICON_X);
          RNA_boolean_set(&op_ptr, "all", true);
        }
      }
      else {
        if (is_array_component) {
          ot = WM_operatortype_find("UI_OT_override_type_set_button", false);
          op_ptr = layout->op(ot,
                              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Define Overrides"),
                              ICON_NONE,
                              blender::wm::OpCallContext::InvokeDefault,
                              UI_ITEM_NONE);
          RNA_boolean_set(&op_ptr, "all", true);
          op_ptr = layout->op(
              ot,
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Define Single Override"),
              ICON_NONE,
              blender::wm::OpCallContext::InvokeDefault,
              UI_ITEM_NONE);
          RNA_boolean_set(&op_ptr, "all", false);
        }
        else {
          op_ptr = layout->op("UI_OT_override_type_set_button",
                              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Define Override"),
                              ICON_NONE,
                              blender::wm::OpCallContext::InvokeDefault,
                              UI_ITEM_NONE);
          RNA_boolean_set(&op_ptr, "all", false);
        }
      }
    }

    layout->separator();

    /* Property Operators */

    /* Copy Property Value
     * Paste Property Value */

    if (is_array_component) {
      PointerRNA op_ptr = layout->op(
          "UI_OT_reset_default_button",
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Reset All to Default Values"),
          ICON_LOOP_BACK);
      RNA_boolean_set(&op_ptr, "all", true);
      op_ptr = layout->op(
          "UI_OT_reset_default_button",
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Reset Single to Default Value"),
          ICON_NONE);
      RNA_boolean_set(&op_ptr, "all", false);
    }
    else {
      PointerRNA op_ptr = layout->op(
          "UI_OT_reset_default_button",
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Reset to Default Value"),
          ICON_LOOP_BACK);
      RNA_boolean_set(&op_ptr, "all", true);
    }

    if (is_idprop && !is_array && ELEM(type, PROP_INT, PROP_FLOAT)) {
      layout->op("UI_OT_assign_default_button",
                 CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Assign Value as Default"),
                 ICON_NONE);

      layout->separator();
    }

    if (is_array_component) {
      PointerRNA op_ptr = layout->op(
          "UI_OT_copy_to_selected_button",
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Copy All to Selected"),
          ICON_NONE);
      RNA_boolean_set(&op_ptr, "all", true);
      op_ptr = layout->op("UI_OT_copy_to_selected_button",
                          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Copy Single to Selected"),
                          ICON_NONE);
      RNA_boolean_set(&op_ptr, "all", false);
    }
    else {
      PointerRNA op_ptr = layout->op(
          "UI_OT_copy_to_selected_button",
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Copy to Selected"),
          ICON_NONE);
      RNA_boolean_set(&op_ptr, "all", true);
    }

    layout->op("UI_OT_copy_data_path_button",
               CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Copy Data Path"),
               ICON_NONE);
    PointerRNA op_ptr = layout->op(
        "UI_OT_copy_data_path_button",
        CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Copy Full Data Path"),
        ICON_NONE);
    RNA_boolean_set(&op_ptr, "full_path", true);

    if (ptr->owner_id && !is_whole_array &&
        ELEM(type, PROP_BOOLEAN, PROP_INT, PROP_FLOAT, PROP_ENUM))
    {
      layout->op("UI_OT_copy_as_driver_button",
                 CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Copy as New Driver"),
                 ICON_NONE);
    }

    layout->separator();

    if (type == PROP_STRING && ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH)) {
      if (ui_but_menu_add_path_operators(layout, ptr, prop)) {
        layout->separator();
      }
    }
  }
  else if (but->optype && but->opptr && RNA_struct_property_is_set(but->opptr, "filepath")) {
    /* Operator with "filepath" string property of PROP_FILEPATH subtype. */
    PropertyRNA *prop = RNA_struct_find_property(but->opptr, "filepath");
    const PropertySubType subtype = RNA_property_subtype(prop);

    if (prop && RNA_property_type(prop) == PROP_STRING &&
        subtype == PropertySubType::PROP_FILEPATH)
    {
      char filepath[FILE_MAX] = {0};
      RNA_property_string_get(but->opptr, prop, filepath);
      if (filepath[0] && BLI_exists(filepath)) {
        wmOperatorType *ot = WM_operatortype_find("WM_OT_path_open", true);
        PointerRNA props_ptr;
        char dir[FILE_MAXDIR];
        BLI_path_split_dir_part(filepath, dir, sizeof(dir));
        props_ptr = layout->op(ot,
                               CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Open File Location"),
                               ICON_NONE,
                               blender::wm::OpCallContext::InvokeDefault,
                               UI_ITEM_NONE);
        RNA_string_set(&props_ptr, "filepath", dir);
        layout->separator();
      }
    }
  }

  {
    const ARegion *region = CTX_wm_region_popup(C) ? CTX_wm_region_popup(C) : CTX_wm_region(C);
    uiButViewItem *view_item_but = (but->type == ButType::ViewItem) ?
                                       static_cast<uiButViewItem *>(but) :
                                       static_cast<uiButViewItem *>(
                                           ui_view_item_find_mouse_over(region, event->xy));
    if (view_item_but) {
      BLI_assert(view_item_but->type == ButType::ViewItem);

      const bContextStore *prev_ctx = CTX_store_get(C);
      /* Sub-layout for context override. */
      uiLayout *sub = &layout->column(false);
      set_layout_context_from_button(C, sub, view_item_but);
      view_item_but->view_item->build_context_menu(*C, *sub);

      /* Reset context. */
      CTX_store_set(C, prev_ctx);

      layout->separator();
    }
  }

  /* Expose id specific operators in context menu when button has no operator associated. Otherwise
   * they would appear in nested context menus, see: #126006. */
  if ((but->optype == nullptr) && (but->apply_func == nullptr) &&
      (but->menu_create_func == nullptr))
  {
    /* If the button represents an id, it can set the "id" context pointer. */
    if (asset::can_mark_single_from_context(C)) {
      const ID *id = static_cast<const ID *>(CTX_data_pointer_get_type(C, "id", &RNA_ID).data);

      /* Gray out items depending on if data-block is an asset. Preferably this could be done via
       * operator poll, but that doesn't work since the operator also works with "selected_ids",
       * which isn't cheap to check. */
      uiLayout *sub = &layout->column(true);
      sub->enabled_set(!id->asset_data);
      sub->op("ASSET_OT_mark_single",
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Mark as Asset"),
              ICON_ASSET_MANAGER);
      sub = &layout->column(true);
      sub->enabled_set(id->asset_data);
      sub->op("ASSET_OT_clear_single",
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Clear Asset"),
              ICON_NONE);
      layout->separator();
    }

    MenuType *mt_idtemplate_liboverride = WM_menutype_find("UI_MT_idtemplate_liboverride", true);
    if (mt_idtemplate_liboverride && mt_idtemplate_liboverride->poll(C, mt_idtemplate_liboverride))
    {
      layout->menu(mt_idtemplate_liboverride, IFACE_("Library Override"), ICON_NONE);
      layout->separator();
    }
  }

  /* Pointer properties and string properties with
   * prop_search support jumping to target object/bone. */
  if (but->rnapoin.data && but->rnaprop) {
    const PropertyType prop_type = RNA_property_type(but->rnaprop);
    if (((prop_type == PROP_POINTER) ||
         (prop_type == PROP_STRING && but->type == ButType::SearchMenu &&
          ((uiButSearch *)but)->items_update_fn == ui_rna_collection_search_update_fn)) &&
        ui_jump_to_target_button_poll(C))
    {
      layout->op("UI_OT_jump_to_target_button",
                 CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Jump to Target"),
                 ICON_NONE);
      layout->separator();
    }
  }

  /* Favorites Menu */
  if (ui_but_is_user_menu_compatible(C, but)) {
    uiBlock *block = layout->block();
    const int w = layout->width();
    bool item_found = false;

    uint um_array_len;
    bUserMenu **um_array = ED_screen_user_menus_find(C, &um_array_len);
    for (int um_index = 0; um_index < um_array_len; um_index++) {
      bUserMenu *um = um_array[um_index];
      if (um == nullptr) {
        continue;
      }
      bUserMenuItem *umi = ui_but_user_menu_find(C, but, um);
      if (umi != nullptr) {
        uiBut *but2 = uiDefIconTextBut(
            block,
            ButType::But,
            0,
            ICON_MENU_PANEL,
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Remove from Quick Favorites"),
            0,
            0,
            w,
            UI_UNIT_Y,
            nullptr,
            "");
        item_found = true;
        UI_but_func_set(but2, [um, umi](bContext &) {
          U.runtime.is_dirty = true;
          ED_screen_user_menu_item_remove(&um->items, umi);
        });
      }
    }
    if (um_array) {
      MEM_freeN(um_array);
    }

    if (!item_found) {
      uiBut *but2 = uiDefIconTextBut(
          block,
          ButType::But,
          0,
          ICON_MENU_PANEL,
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Add to Quick Favorites"),
          0,
          0,
          w,
          UI_UNIT_Y,
          nullptr,
          TIP_("Add to a user defined context menu (stored in the user preferences)"));
      UI_but_func_set(but2, [but](bContext &C) {
        bUserMenu *um = ED_screen_user_menu_ensure(&C);
        U.runtime.is_dirty = true;
        ui_but_user_menu_add(&C, but, um);
      });
    }

    layout->separator();
  }

  /* Shortcut menu */
  IDProperty *prop;
  const char *idname = shortcut_get_operator_property(C, but, &prop);
  if (idname != nullptr) {
    uiBlock *block = layout->block();
    const int w = layout->width();

    /* We want to know if this op has a shortcut, be it hotkey or not. */
    wmKeyMap *km;
    wmKeyMapItem *kmi = WM_key_event_operator(
        C, idname, but->opcontext, prop, EVT_TYPE_MASK_ALL, 0, &km);

    /* We do have a shortcut, but only keyboard ones are editable that way... */
    if (kmi) {
      if (ISKEYBOARD(kmi->type) || ISNDOF_BUTTON(kmi->type)) {
#if 0 /* would rather use a block but, but gets weirdly positioned... */
        uiDefBlockBut(block,
                      menu_change_shortcut,
                      but,
                      "Change Shortcut",
                      0,
                      0,
                      layout->width(),
                      UI_UNIT_Y,
                      "");
#endif

        uiBut *but2 = uiDefIconTextBut(
            block,
            ButType::But,
            0,
            ICON_HAND,
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Change Shortcut"),
            0,
            0,
            w,
            UI_UNIT_Y,
            nullptr,
            "");
        UI_but_func_set(but2, [but](bContext &C) {
          UI_popup_block_invoke(&C, menu_change_shortcut, but, nullptr);
        });
      }
      else {
        uiBut *but2 = uiDefIconTextBut(block,
                                       ButType::But,
                                       0,
                                       ICON_HAND,
                                       IFACE_("Non-Keyboard Shortcut"),
                                       0,
                                       0,
                                       w,
                                       UI_UNIT_Y,
                                       nullptr,
                                       TIP_("Only keyboard shortcuts can be edited that way, "
                                            "please use User Preferences otherwise"));
        UI_but_flag_enable(but2, UI_BUT_DISABLED);
      }

      uiBut *but2 = uiDefIconTextBut(
          block,
          ButType::But,
          0,
          ICON_BLANK1,
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Remove Shortcut"),
          0,
          0,
          w,
          UI_UNIT_Y,
          nullptr,
          "");
      UI_but_func_set(but2, [but](bContext &C) { remove_shortcut_func(&C, but); });
    }
    /* only show 'assign' if there's a suitable key map for it to go in */
    else if (WM_keymap_guess_opname(C, idname)) {
      uiBut *but2 = uiDefIconTextBut(
          block,
          ButType::But,
          0,
          ICON_HAND,
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Assign Shortcut"),
          0,
          0,
          w,
          UI_UNIT_Y,
          nullptr,
          "");
      UI_but_func_set(but2, [but](bContext &C) {
        UI_popup_block_ex(&C, menu_add_shortcut, nullptr, menu_add_shortcut_cancel, but, nullptr);
      });
    }

    shortcut_free_operator_property(prop);

    /* Set the operator pointer for python access */
    layout->context_set_from_but(but);

    layout->separator();
  }

  { /* Docs */
    if (std::optional<std::string> manual_id = UI_but_online_manual_id(but)) {
      PointerRNA ptr_props;
      layout->op("WM_OT_doc_view_manual_ui_context",
                 CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Online Manual"),
                 ICON_URL);

      if (U.flag & USER_DEVELOPER_UI) {
        ptr_props = layout->op(
            "WM_OT_doc_view",
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Online Python Reference"),
            ICON_NONE,
            blender::wm::OpCallContext::ExecDefault,
            UI_ITEM_NONE);
        RNA_string_set(&ptr_props, "doc_id", manual_id.value().c_str());
      }
    }
  }

  if (but->optype && U.flag & USER_DEVELOPER_UI) {
    layout->op("UI_OT_copy_python_command_button", std::nullopt, ICON_NONE);
  }

  /* perhaps we should move this into (G.debug & G_DEBUG) - campbell */
  if (U.flag & USER_DEVELOPER_UI) {
    if (ui_block_is_menu(but->block) == false) {
      layout->op("UI_OT_editsource",
                 std::nullopt,
                 ICON_NONE,
                 blender::wm::OpCallContext::InvokeDefault,
                 UI_ITEM_NONE);
    }
  }

  /* Show header tools for header buttons. */
  if (ui_block_is_popup_any(but->block) == false) {
    const ARegion *region = CTX_wm_region(C);

    if (!region) {
      /* skip */
    }
    else if (ELEM(region->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
      layout->menu_fn(IFACE_("Header"), ICON_NONE, ED_screens_header_tools_menu_create, nullptr);
    }
    else if (region->regiontype == RGN_TYPE_NAV_BAR) {
      layout->menu_fn(IFACE_("Navigation Bar"), ICON_NONE, ED_buttons_navbar_menu, nullptr);
      const ScrArea *area = CTX_wm_area(C);
      if (area && area->spacetype == SPACE_PROPERTIES) {
        layout->menu_fn(IFACE_("Visible Tabs"), ICON_NONE, ED_buttons_visible_tabs_menu, nullptr);
      }
    }
    else if (region->regiontype == RGN_TYPE_FOOTER) {
      layout->menu_fn(IFACE_("Footer"), ICON_NONE, ED_screens_footer_tools_menu_create, nullptr);
    }
  }

  /* UI List item context menu. Scripts can add items to it, by default there's nothing shown. */
  const ARegion *region = CTX_wm_region_popup(C) ? CTX_wm_region_popup(C) : CTX_wm_region(C);
  const bool is_inside_listbox = ui_list_find_mouse_over(region, event) != nullptr;
  const bool is_inside_listrow = is_inside_listbox ?
                                     ui_list_row_find_mouse_over(region, event->xy) != nullptr :
                                     false;
  if (is_inside_listrow) {
    MenuType *mt = WM_menutype_find("UI_MT_list_item_context_menu", true);
    if (mt) {
      UI_menutype_draw(C, mt, &layout->column(false));
    }
  }

  MenuType *mt = WM_menutype_find("UI_MT_button_context_menu", true);
  if (mt) {
    UI_menutype_draw(C, mt, &layout->column(false));
  }

  if (but->context) {
    CTX_store_set(C, previous_ctx);
  }

  return UI_popup_menu_end_or_cancel(C, pup);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Panel Context Menu
 * \{ */

void ui_popup_context_menu_for_panel(bContext *C, ARegion *region, Panel *panel)
{
  bScreen *screen = CTX_wm_screen(C);
  const bool has_panel_category = UI_panel_category_is_visible(region);
  const bool any_item_visible = has_panel_category;

  if (!any_item_visible) {
    return;
  }
  if (panel->type->parent != nullptr) {
    return;
  }
  if (!UI_panel_can_be_pinned(panel)) {
    return;
  }

  PointerRNA ptr = RNA_pointer_create_discrete(&screen->id, &RNA_Panel, panel);

  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Panel"), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  if (has_panel_category) {
    char tmpstr[80];
    SNPRINTF_UTF8(tmpstr, "%s" UI_SEP_CHAR_S "%s", IFACE_("Pin"), IFACE_("Shift Left Mouse"));
    layout->prop(&ptr, "use_pin", UI_ITEM_NONE, tmpstr, ICON_NONE);

    /* evil, force shortcut flag */
    {
      uiBlock *block = layout->block();
      uiBut *but = block->buttons.last().get();
      but->flag |= UI_BUT_HAS_SEP_CHAR;
    }
  }
  UI_popup_menu_end(C, pup);
}

/** \} */
