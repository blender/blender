/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Search available menu items via the user interface & key-maps.
 * Accessed via the #WM_OT_search_menu operator.
 */

#include <cstdio>
#include <cstring>
#include <variant>

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_node_types.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_matrix.h"
#include "BLI_resource_scope.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_screen.hh"

#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface_layout.hh"
#include "UI_string_search.hh"
#include "interface_intern.hh"

/* For key-map item access. */
#include "wm_event_system.hh"

#include <fmt/format.h>

using blender::ResourceScope;
using blender::StringRef;

/* -------------------------------------------------------------------- */
/** \name Menu Search Template Implementation
 * \{ */

/**
 * Use when #menu_items_from_ui_create is called with `include_all_areas`.
 * so we can run the menu item in the area it was extracted from.
 */
struct MenuSearch_Context {
  /**
   * Index into `Area.ui_type` #EnumPropertyItem or the top-bar when -1.
   * Needed to get the display-name to use as a prefix for each menu item.
   */
  int space_type_ui_index;

  ScrArea *area;
  ARegion *region;
};

struct MenuSearch_Parent {
  MenuSearch_Parent *parent;
  StringRef drawstr;

  /** Set while writing menu items only. */
  MenuSearch_Parent *temp_child;
};

struct MenuSearch_Item {
  StringRef drawstr;
  StringRef drawwstr_full;
  int icon = 0;
  int state = 0;
  float weight = 0.0f;

  MenuSearch_Parent *menu_parent = nullptr;
  MenuType *mt = nullptr;

  struct OperatorData {
    wmOperatorType *type;
    PointerRNA *opptr;
    blender::wm::OpCallContext opcontext;
    const bContextStore *context;
    ~OperatorData()
    {
      if (this->opptr != nullptr) {
        WM_operator_properties_free(this->opptr);
        MEM_delete(this->opptr);
      }
      MEM_delete(this->context);
    }
  };
  struct PropertyData {
    PointerRNA ptr;
    PropertyRNA *prop;
    int index;
    /** Only for enum buttons. */
    int enum_value;
  };
  std::variant<OperatorData, PropertyData> data;

  /** Set when we need each menu item to be able to set its own context. may be nullptr. */
  MenuSearch_Context *wm_context = nullptr;
};

struct MenuSearch_Data {
  /** MenuSearch_Item */
  blender::Vector<std::reference_wrapper<MenuSearch_Item>> items;
  /** Use for all small allocations. */
  blender::ResourceScope scope;

  /** Use for context menu, to fake a button to create a context menu. */
  struct {
    uiBut but;
    uiBlock block;
  } context_menu_data;
};

static bool menu_item_sort_by_drawstr_full(const MenuSearch_Item &menu_item_a,
                                           const MenuSearch_Item &menu_item_b)
{
  return menu_item_a.drawwstr_full < menu_item_b.drawwstr_full;
}

static bool menu_items_from_ui_create_item_from_button(MenuSearch_Data *data,
                                                       blender::ResourceScope &scope,
                                                       MenuType *mt,
                                                       uiBut *but,
                                                       MenuSearch_Context *wm_context,
                                                       MenuSearch_Parent *menu_parent)
{
  using namespace blender;
  MenuSearch_Item *item = nullptr;

  /* Use override if the name is empty, this can happen with popovers. */
  std::string drawstr_override;
  const size_t sep_index = (but->flag & UI_BUT_HAS_SEP_CHAR) ? but->drawstr.find(UI_SEP_CHAR) :
                                                               std::string::npos;
  const bool drawstr_is_empty = sep_index == 0 || but->drawstr.empty();

  if (but->optype != nullptr) {
    if (drawstr_is_empty) {
      drawstr_override = WM_operatortype_name(but->optype, but->opptr);
    }

    item = &scope.construct<MenuSearch_Item>();
    item->data = MenuSearch_Item::OperatorData();
    auto &op_data = std::get<MenuSearch_Item::OperatorData>(item->data);
    op_data.type = but->optype;
    op_data.opcontext = but->opcontext;
    op_data.context = but->context ? MEM_new<bContextStore>(__func__, *but->context) : nullptr;
    op_data.opptr = but->opptr;

    item->weight = but->search_weight;

    but->opptr = nullptr;
  }
  else if (but->rnaprop != nullptr) {
    const int prop_type = RNA_property_type(but->rnaprop);

    if (drawstr_is_empty) {
      if (prop_type == PROP_ENUM) {
        const int value_enum = int(but->hardmax);
        EnumPropertyItem enum_item;
        if (RNA_property_enum_item_from_value_gettexted((bContext *)but->block->evil_C,
                                                        &but->rnapoin,
                                                        but->rnaprop,
                                                        value_enum,
                                                        &enum_item))
        {
          drawstr_override = enum_item.name;
        }
        else {
          /* Should never happen. */
          drawstr_override = "Unknown";
        }
      }
      else {
        drawstr_override = RNA_property_ui_name(but->rnaprop);
      }
    }

    if (!ELEM(prop_type, PROP_BOOLEAN, PROP_ENUM)) {
      /* Note that these buttons are not prevented,
       * but aren't typically used in menus. */
      printf("Button '%s' in menu '%s' is a menu item with unsupported RNA type %d\n",
             but->drawstr.c_str(),
             mt->idname,
             prop_type);
    }
    else {
      item = &scope.construct<MenuSearch_Item>();
      item->weight = but->search_weight;

      item->data = MenuSearch_Item::PropertyData();
      auto &rna_data = std::get<MenuSearch_Item::PropertyData>(item->data);
      rna_data.ptr = but->rnapoin;
      rna_data.prop = but->rnaprop;
      rna_data.index = but->rnaindex;
      if (prop_type == PROP_ENUM) {
        rna_data.enum_value = int(but->hardmax);
      }
    }
  }

  if (item != nullptr) {
    /* Handle shared settings. */
    if (!drawstr_override.empty()) {
      const StringRef drawstr_suffix = sep_index == std::string::npos ?
                                           "" :
                                           StringRef(but->drawstr).drop_prefix(sep_index);
      std::string drawstr = std::string("(") + drawstr_override + ")" + drawstr_suffix;
      item->drawstr = scope.allocator().copy_string(drawstr);
    }
    else {
      item->drawstr = scope.allocator().copy_string(but->drawstr);
    }

    item->icon = ui_but_icon(but);
    item->state = (but->flag &
                   (UI_BUT_DISABLED | UI_BUT_INACTIVE | UI_BUT_REDALERT | UI_BUT_HAS_SEP_CHAR));
    item->mt = mt;

    item->wm_context = wm_context;
    item->menu_parent = menu_parent;

    data->items.append(*item);
    return true;
  }

  return false;
}

/**
 * Populate a fake button from a menu item (use for context menu).
 */
static bool menu_items_to_ui_button(MenuSearch_Item *item, uiBut *but)
{
  bool changed = false;
  if (auto *op_data = std::get_if<MenuSearch_Item::OperatorData>(&item->data)) {
    but->optype = op_data->type;
    but->opcontext = op_data->opcontext;
    but->context = op_data->context;
    but->opptr = op_data->opptr;
    changed = true;
  }
  else if (auto *rna_data = std::get_if<MenuSearch_Item::PropertyData>(&item->data)) {
    const int prop_type = RNA_property_type(rna_data->prop);

    but->rnapoin = rna_data->ptr;
    but->rnaprop = rna_data->prop;
    but->rnaindex = rna_data->index;

    if (prop_type == PROP_ENUM) {
      but->hardmax = rna_data->enum_value;
    }
    changed = true;
  }

  if (changed) {
    but->drawstr = item->drawstr;
    const size_t sep_index = but->drawstr.find(UI_SEP_CHAR);

    if (sep_index != std::string::npos) {
      but->drawstr.resize(sep_index);
    }

    but->icon = item->icon;
  }

  return changed;
}

struct MenuStackEntry {
  MenuType *mt = nullptr;
  /** Used as parent in submenus. */
  MenuSearch_Parent *self_as_parent = nullptr;
  /** The menu might be context dependent. */
  std::optional<bContextStore> context;
};

/**
 * Populate \a menu_stack with menus from inspecting active key-maps for this context.
 */
static void menu_types_add_from_keymap_items(bContext *C,
                                             wmWindow *win,
                                             ScrArea *area,
                                             ARegion *region,
                                             blender::Stack<MenuStackEntry> &menu_stack,
                                             blender::Map<MenuType *, wmKeyMapItem *> &menu_to_kmi,
                                             blender::Set<MenuType *> &menu_tagged)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  ListBase *handlers[] = {
      region ? &region->runtime->handlers : nullptr,
      area ? &area->handlers : nullptr,
      &win->handlers,
  };

  for (int handler_index = 0; handler_index < ARRAY_SIZE(handlers); handler_index++) {
    if (handlers[handler_index] == nullptr) {
      continue;
    }
    LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers[handler_index]) {
      /* During this loop, UI handlers for nested menus can tag multiple handlers free. */
      if (handler_base->flag & WM_HANDLER_DO_FREE) {
        continue;
      }
      if (handler_base->type != WM_HANDLER_TYPE_KEYMAP) {
        continue;
      }

      if (handler_base->poll == nullptr || handler_base->poll(win, area, region, win->eventstate))
      {
        wmEventHandler_Keymap *handler = (wmEventHandler_Keymap *)handler_base;
        wmEventHandler_KeymapResult km_result;
        WM_event_get_keymaps_from_handler(wm, win, handler, &km_result);
        for (int km_index = 0; km_index < km_result.keymaps_len; km_index++) {
          wmKeyMap *keymap = km_result.keymaps[km_index];
          if (keymap && WM_keymap_poll(C, keymap)) {
            LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
              if (kmi->flag & KMI_INACTIVE) {
                continue;
              }
              if (STR_ELEM(kmi->idname, "WM_OT_call_menu", "WM_OT_call_menu_pie")) {
                char menu_idname[MAX_NAME];
                RNA_string_get(kmi->ptr, "name", menu_idname);
                MenuType *mt = WM_menutype_find(menu_idname, false);

                if (mt && menu_tagged.add(mt)) {
                  /* Unlikely, but possible this will be included twice. */
                  menu_stack.push({mt});
                  menu_to_kmi.add(mt, kmi);
                }
              }
            }
          }
        }
      }
    }
  }
}

/**
 * Display all operators (last). Developer-only convenience feature.
 */
static void menu_items_from_all_operators(bContext *C, MenuSearch_Data *data)
{
  /* Add to temporary list so we can sort them separately. */
  blender::Vector<std::reference_wrapper<MenuSearch_Item>> operator_items;

  ResourceScope &scope = data->scope;
  for (wmOperatorType *ot : WM_operatortypes_registered_get()) {
    if ((ot->flag & OPTYPE_INTERNAL) && (G.debug & G_DEBUG_WM) == 0) {
      continue;
    }

    if (WM_operator_poll(C, ot)) {
      const char *ot_ui_name = CTX_IFACE_(ot->translation_context, ot->name);

      MenuSearch_Item &item = scope.construct<MenuSearch_Item>();
      item.data = MenuSearch_Item::OperatorData();
      auto &op_data = std::get<MenuSearch_Item::OperatorData>(item.data);
      op_data.type = ot;
      op_data.opcontext = blender::wm::OpCallContext::InvokeDefault;
      op_data.context = nullptr;

      char idname_as_py[OP_MAX_TYPENAME];
      char uiname[256];
      WM_operator_py_idname(idname_as_py, ot->idname);

      SNPRINTF_UTF8(uiname, "%s " UI_MENU_ARROW_SEP " %s", idname_as_py, ot_ui_name);

      item.drawwstr_full = scope.allocator().copy_string(uiname);
      item.drawstr = ot_ui_name;

      item.wm_context = nullptr;

      operator_items.append(item);
    }
  }

  std::sort(operator_items.begin(), operator_items.end(), menu_item_sort_by_drawstr_full);

  data->items.extend(operator_items);
}

/**
 * Create #MenuSearch_Data by inspecting the current context, this uses two methods:
 *
 * - Look up predefined editor-menus.
 * - Look up key-map items which call menus.
 */
static MenuSearch_Data *menu_items_from_ui_create(bContext *C,
                                                  wmWindow *win,
                                                  ScrArea *area_init,
                                                  ARegion *region_init,
                                                  bool include_all_areas,
                                                  const char *single_menu_idname)
{
  blender::Map<MenuType *, const char *> menu_display_name_map;
  const uiStyle *style = UI_style_get_dpi();

  const bContextStore *old_context_store = CTX_store_get(C);
  BLI_SCOPED_DEFER([&]() { CTX_store_set(C, old_context_store); });
  bContextStore context_store;
  if (old_context_store) {
    context_store = *old_context_store;
  }
  context_store.entries.append({"is_menu_search", true});
  CTX_store_set(C, &context_store);

  /* Convert into non-ui structure. */
  MenuSearch_Data *data = MEM_new<MenuSearch_Data>(__func__);
  ResourceScope &scope = data->scope;

  fmt::memory_buffer str_buf;

  /* Use a stack of menus to handle and discover new menus in passes. */
  blender::Stack<MenuStackEntry> menu_stack;

  /* Tag menu types not to add, either because they have already been added
   * or they have been blacklisted. */
  blender::Set<MenuType *> menu_tagged;
  blender::Map<MenuType *, wmKeyMapItem *> menu_to_kmi;

  /* Blacklist menus we don't want to show. */
  {
    const char *idname_array[] = {
        /* While we could include this, it's just showing filenames to load. */
        (single_menu_idname && STREQ(single_menu_idname, "TOPBAR_MT_file_open_recent")) ?
            nullptr :
            "TOPBAR_MT_file_open_recent",
        /* Showing undo history is not helpful since users may accidentally undo
         * an action they intend to run. */
        "TOPBAR_MT_undo_history",
    };
    for (int i = 0; i < ARRAY_SIZE(idname_array); i++) {
      if (!idname_array[i]) {
        continue;
      }
      MenuType *mt = WM_menutype_find(idname_array[i], false);
      if (mt != nullptr) {
        menu_tagged.add(mt);
      }
    }
  }

  if (!single_menu_idname) {
    /* Exclude context menus (when not searching in a specific single menu) because:
     * - The menu items are available elsewhere (and will show up multiple times).
     * - Menu items depend on exact context, making search results unpredictable
     *   (exact number of items selected for example). See design doc #74158.
     * There is one exception,
     * as the outliner only exposes functionality via the context menu. */
    for (MenuType *mt : WM_menutypes_registered_get()) {
      if (BLI_str_endswith(mt->idname, "_context_menu")) {
        menu_tagged.add(mt);
      }
    }
    const char *idname_array[] = {
        /* Add back some context menus. */
        "OUTLINER_MT_context_menu",
    };
    for (int i = 0; i < ARRAY_SIZE(idname_array); i++) {
      MenuType *mt = WM_menutype_find(idname_array[i], false);
      if (mt != nullptr) {
        menu_tagged.remove(mt);
      }
    }
  }

  /* Collect contexts, one for each 'ui_type'. */
  MenuSearch_Context *wm_contexts = nullptr;

  const EnumPropertyItem *space_type_ui_items = nullptr;
  int space_type_ui_items_len = 0;
  bool space_type_ui_items_free = false;

  /* Text used as prefix for top-bar menu items. */
  const char *global_menu_prefix = nullptr;

  if (include_all_areas) {
    bScreen *screen = WM_window_get_active_screen(win);

    /* First create arrays for ui_type. */
    PropertyRNA *prop_ui_type = nullptr;
    {
      /* This must be a valid pointer, with only it's type checked. */
      ScrArea area_dummy{};
      /* Anything besides #SPACE_EMPTY is fine,
       * as this value is only included in the enum when set. */
      area_dummy.spacetype = SPACE_TOPBAR;
      PointerRNA ptr = RNA_pointer_create_discrete(&screen->id, &RNA_Area, &area_dummy);
      prop_ui_type = RNA_struct_find_property(&ptr, "ui_type");
      RNA_property_enum_items(C,
                              &ptr,
                              prop_ui_type,
                              &space_type_ui_items,
                              &space_type_ui_items_len,
                              &space_type_ui_items_free);

      wm_contexts =
          scope.construct<blender::Array<MenuSearch_Context>>(space_type_ui_items_len).data();
      for (int i = 0; i < space_type_ui_items_len; i++) {
        wm_contexts[i].space_type_ui_index = -1;
      }
    }

    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
      if (region != nullptr) {
        PointerRNA ptr = RNA_pointer_create_discrete(&screen->id, &RNA_Area, area);
        const int space_type_ui = RNA_property_enum_get(&ptr, prop_ui_type);

        const int space_type_ui_index = RNA_enum_from_value(space_type_ui_items, space_type_ui);
        if (space_type_ui_index == -1) {
          continue;
        }

        if (wm_contexts[space_type_ui_index].space_type_ui_index != -1) {
          ScrArea *area_best = wm_contexts[space_type_ui_index].area;
          const uint value_best = uint(area_best->winx) * uint(area_best->winy);
          const uint value_test = uint(area->winx) * uint(area->winy);
          if (value_best > value_test) {
            continue;
          }
        }

        wm_contexts[space_type_ui_index].space_type_ui_index = space_type_ui_index;
        wm_contexts[space_type_ui_index].area = area;
        wm_contexts[space_type_ui_index].region = region;
      }
    }

    global_menu_prefix = CTX_IFACE_(RNA_property_translation_context(prop_ui_type), "Top Bar");
  }

  for (int space_type_ui_index = -1; space_type_ui_index < space_type_ui_items_len;
       space_type_ui_index += 1)
  {

    ScrArea *area = nullptr;
    ARegion *region = nullptr;
    MenuSearch_Context *wm_context = nullptr;

    if (include_all_areas) {
      if (space_type_ui_index == -1) {
        /* First run without any context, to populate the top-bar without. */
        wm_context = nullptr;
        area = nullptr;
        region = nullptr;
      }
      else {
        wm_context = &wm_contexts[space_type_ui_index];
        if (wm_context->space_type_ui_index == -1) {
          continue;
        }

        area = wm_context->area;
        region = wm_context->region;

        CTX_wm_area_set(C, area);
        CTX_wm_region_set(C, region);
      }
    }
    else {
      area = area_init;
      region = region_init;
    }

    if (single_menu_idname) {
      if (MenuType *mt = WM_menutype_find(single_menu_idname, false)) {
        if (menu_tagged.add(mt)) {
          menu_stack.push({mt});
        }
      }
    }
    else {
      /* Populate menus from the editors,
       * note that we could create a fake header, draw the header and extract the menus
       * from the buttons, however this is quite involved and can be avoided as by convention
       * each space-type has a single root-menu that headers use. */
      const char *idname_array[2] = {nullptr};
      int idname_array_len = 0;

      /* Use negative for global (no area) context, populate the top-bar. */
      if (space_type_ui_index == -1) {
        idname_array[idname_array_len++] = "TOPBAR_MT_editor_menus";
      }

#define SPACE_MENU_MAP(space_type, menu_id) \
  case space_type: \
    idname_array[idname_array_len++] = menu_id; \
    break
#define SPACE_MENU_NOP(space_type) \
  case space_type: \
    break

      if (area != nullptr) {
        SpaceLink *sl = (SpaceLink *)area->spacedata.first;
        switch ((eSpace_Type)area->spacetype) {
          SPACE_MENU_MAP(SPACE_VIEW3D, "VIEW3D_MT_editor_menus");
          SPACE_MENU_MAP(SPACE_GRAPH, "GRAPH_MT_editor_menus");
          SPACE_MENU_MAP(SPACE_OUTLINER, "OUTLINER_MT_editor_menus");
          SPACE_MENU_NOP(SPACE_PROPERTIES);
          SPACE_MENU_MAP(SPACE_FILE, "FILEBROWSER_MT_editor_menus");
          SPACE_MENU_MAP(SPACE_IMAGE, "IMAGE_MT_editor_menus");
          SPACE_MENU_MAP(SPACE_INFO, "INFO_MT_editor_menus");
          SPACE_MENU_MAP(SPACE_SEQ, "SEQUENCER_MT_editor_menus");
          SPACE_MENU_MAP(SPACE_TEXT, "TEXT_MT_editor_menus");
          SPACE_MENU_MAP(SPACE_ACTION, "DOPESHEET_MT_editor_menus");
          SPACE_MENU_MAP(SPACE_NLA, "NLA_MT_editor_menus");
          SPACE_MENU_MAP(SPACE_NODE, "NODE_MT_editor_menus");
          SPACE_MENU_MAP(SPACE_CONSOLE, "CONSOLE_MT_editor_menus");
          SPACE_MENU_MAP(SPACE_USERPREF, "USERPREF_MT_editor_menus");
          SPACE_MENU_MAP(SPACE_CLIP,
                         (((const SpaceClip *)sl)->mode == SC_MODE_TRACKING) ?
                             "CLIP_MT_tracking_editor_menus" :
                             "CLIP_MT_masking_editor_menus");
          SPACE_MENU_NOP(SPACE_EMPTY);
          SPACE_MENU_NOP(SPACE_SCRIPT);
          SPACE_MENU_NOP(SPACE_STATUSBAR);
          SPACE_MENU_NOP(SPACE_TOPBAR);
          SPACE_MENU_NOP(SPACE_SPREADSHEET);
        }
      }
      for (int i = 0; i < idname_array_len; i++) {
        MenuType *mt = WM_menutype_find(idname_array[i], false);
        if (mt != nullptr) {
          /* Check if this exists because of 'include_all_areas'. */
          if (menu_tagged.add(mt)) {
            menu_stack.push({mt});
          }
        }
      }
    }
#undef SPACE_MENU_MAP
#undef SPACE_MENU_NOP

    bool has_keymap_menu_items = false;

    while (!menu_stack.is_empty()) {
      MenuStackEntry current_menu = menu_stack.pop();
      MenuType *mt = current_menu.mt;
      if (!WM_menutype_poll(C, mt)) {
        continue;
      }

      uiBlock *block = UI_block_begin(C, region, __func__, blender::ui::EmbossType::Emboss);
      uiLayout &layout = blender::ui::block_layout(block,
                                                   blender::ui::LayoutDirection::Vertical,
                                                   blender::ui::LayoutType::Menu,
                                                   0,
                                                   0,
                                                   200,
                                                   0,
                                                   UI_MENU_PADDING,
                                                   style);

      UI_block_flag_enable(block, UI_BLOCK_SHOW_SHORTCUT_ALWAYS);

      if (current_menu.context.has_value()) {
        layout.context_copy(&*current_menu.context);
      }
      layout.operator_context_set(blender::wm::OpCallContext::InvokeRegionWin);
      UI_menutype_draw(C, mt, &layout);

      UI_block_end(C, block);

      for (const int i : block->buttons.index_range()) {
        const std::unique_ptr<uiBut> &but = block->buttons[i];
        MenuType *mt_from_but = nullptr;
        /* Support menu titles with dynamic from initial labels
         * (used by edit-mesh context menu). */
        if (but->type == ButType::Label) {

          /* Check if the label is the title. */
          const std::unique_ptr<uiBut> *but_test = block->buttons.begin() + i - 1;
          while (but_test >= block->buttons.begin() && (*but_test)->type == ButType::Sepr) {
            but_test--;
          }

          if (but_test < block->buttons.begin()) {
            menu_display_name_map.add(mt, scope.allocator().copy_string(but->drawstr).c_str());
          }
        }
        else if (menu_items_from_ui_create_item_from_button(
                     data, scope, mt, but.get(), wm_context, current_menu.self_as_parent))
        {
          /* pass */
        }
        else if ((mt_from_but = UI_but_menutype_get(but.get()))) {
          const bool uses_context = but->context &&
                                    flag_is_set(mt_from_but->flag, MenuTypeFlag::ContextDependent);
          const bool tagged_first_time = menu_tagged.add(mt_from_but);
          const bool scan_submenu = tagged_first_time || uses_context;

          if (scan_submenu) {
            MenuSearch_Parent *menu_parent = &scope.construct<MenuSearch_Parent>();
            /* Use brackets for menu key shortcuts,
             * converting "Text|Some-Shortcut" to "Text (Some-Shortcut)".
             * This is needed so we don't right align sub-menu contents
             * we only want to do that for the last menu item, not the path that leads to it.
             */
            const char *drawstr_sep = but->flag & UI_BUT_HAS_SEP_CHAR ?
                                          strrchr(but->drawstr.c_str(), UI_SEP_CHAR) :
                                          nullptr;
            bool drawstr_is_empty = false;
            if (drawstr_sep != nullptr) {
              BLI_assert(str_buf.size() == 0);
              /* Detect empty string, fall back to menu name. */
              const char *drawstr = but->drawstr.c_str();
              int drawstr_len = drawstr_sep - but->drawstr.c_str();
              if (UNLIKELY(drawstr_len == 0)) {
                drawstr = CTX_IFACE_(mt_from_but->translation_context, mt_from_but->label);
                drawstr_len = strlen(drawstr);
                if (drawstr[0] == '\0') {
                  drawstr_is_empty = true;
                }
              }
              str_buf.append(StringRef(drawstr, drawstr_len));
              fmt::format_to(fmt::appender(str_buf), " ({})", drawstr_sep + 1);
              menu_parent->drawstr = scope.allocator().copy_string(
                  StringRef(str_buf.data(), str_buf.size()));
              str_buf.clear();
            }
            else {
              const char *drawstr = but->drawstr.c_str();
              if (UNLIKELY(drawstr[0] == '\0')) {
                drawstr = CTX_IFACE_(mt_from_but->translation_context, mt_from_but->label);
                if (drawstr[0] == '\0') {
                  drawstr_is_empty = true;
                }
              }
              menu_parent->drawstr = scope.allocator().copy_string(drawstr);
            }
            menu_parent->parent = current_menu.self_as_parent;

            if (drawstr_is_empty) {
              printf("Warning: '%s' menu has empty 'bl_label'.\n", mt_from_but->idname);
            }

            if (uses_context) {
              menu_stack.push({mt_from_but, menu_parent, *but->context});
            }
            else {
              menu_stack.push({mt_from_but, menu_parent});
            }
          }
        }
        else if (but->menu_create_func != nullptr) {
          /* A non 'MenuType' menu button. */

          /* +1 to avoid overlap with the current 'block'. */
          uiBlock *sub_block = UI_block_begin(
              C, region, __func__ + 1, blender::ui::EmbossType::Emboss);
          uiLayout &sub_layout = blender::ui::block_layout(sub_block,
                                                           blender::ui::LayoutDirection::Vertical,
                                                           blender::ui::LayoutType::Menu,
                                                           0,
                                                           0,
                                                           200,
                                                           0,
                                                           UI_MENU_PADDING,
                                                           style);

          UI_block_flag_enable(sub_block, UI_BLOCK_SHOW_SHORTCUT_ALWAYS);

          sub_layout.operator_context_set(blender::wm::OpCallContext::InvokeRegionWin);

          /* If this is a panel, check it's poll function succeeds before drawing.
           * otherwise draw(..) may be called in an unsupported context and crash, see: #130744.
           *
           * NOTE(@ideasman42): it would be good if the buttons #UI_BUT_DISABLED flag
           * could be used as a more general way to know if poll succeeded,
           * at this point it's not set - this could be further investigated. */
          bool poll_success = true;
          if (PanelType *pt = UI_but_paneltype_get(but.get())) {
            if (pt->poll && (pt->poll(C, pt) == false)) {
              poll_success = false;
            }
          }

          if (poll_success) {
            but->menu_create_func(C, &sub_layout, but->poin);
          }

          UI_block_end(C, sub_block);

          if (poll_success) {
            MenuSearch_Parent *menu_parent = &scope.construct<MenuSearch_Parent>();
            menu_parent->drawstr = scope.allocator().copy_string(but->drawstr);
            menu_parent->parent = current_menu.self_as_parent;

            for (const std::unique_ptr<uiBut> &sub_but : sub_block->buttons) {
              menu_items_from_ui_create_item_from_button(
                  data, scope, mt, sub_but.get(), wm_context, menu_parent);
            }
          }

          if (region) {
            region->runtime->block_name_map.remove(sub_block->name);
            BLI_remlink(&region->runtime->uiblocks, sub_block);
          }
          UI_block_free(nullptr, sub_block);
        }
      }
      if (region) {
        region->runtime->block_name_map.remove(block->name);
        BLI_remlink(&region->runtime->uiblocks, block);
      }
      UI_block_free(nullptr, block);

      if (single_menu_idname == nullptr) {
        /* Add key-map items as a second pass, so all menus are accessed from the header & top-bar
         * before key shortcuts are expanded. */
        if (menu_stack.is_empty() && (has_keymap_menu_items == false)) {
          has_keymap_menu_items = true;
          menu_types_add_from_keymap_items(
              C, win, area, region, menu_stack, menu_to_kmi, menu_tagged);
        }
      }
    }
  }

  /* NOTE: currently this builds the full path for each menu item,
   * that could be moved into the parent menu. */

  /* Set names as full paths. */
  for (MenuSearch_Item &item : data->items) {
    BLI_assert(str_buf.size() == 0);

    if (include_all_areas) {
      fmt::format_to(fmt::appender(str_buf),
                     "{}: ",
                     (item.wm_context != nullptr) ?
                         space_type_ui_items[item.wm_context->space_type_ui_index].name :
                         global_menu_prefix);
    }

    if (item.menu_parent != nullptr) {
      MenuSearch_Parent *menu_parent = item.menu_parent;
      menu_parent->temp_child = nullptr;
      while (menu_parent && menu_parent->parent) {
        menu_parent->parent->temp_child = menu_parent;
        menu_parent = menu_parent->parent;
      }
      while (menu_parent) {
        str_buf.append(menu_parent->drawstr);
        str_buf.append(StringRef(" " UI_MENU_ARROW_SEP " "));
        menu_parent = menu_parent->temp_child;
      }
    }
    else {
      const char *drawstr = menu_display_name_map.lookup_default(item.mt, nullptr);
      if (drawstr == nullptr) {
        drawstr = CTX_IFACE_(item.mt->translation_context, item.mt->label);
      }
      str_buf.append(StringRef(drawstr));

      wmKeyMapItem *kmi = menu_to_kmi.lookup_default(item.mt, nullptr);
      if (kmi != nullptr) {
        std::string kmi_str = WM_keymap_item_to_string(kmi, false).value_or("");
        fmt::format_to(fmt::appender(str_buf), " ({})", kmi_str);
      }

      str_buf.append(StringRef(" " UI_MENU_ARROW_SEP " "));
    }

    str_buf.append(item.drawstr);

    item.drawwstr_full = scope.allocator().copy_string(StringRef(str_buf.data(), str_buf.size()));
    str_buf.clear();
  }

  /* Finally sort menu items.
   *
   * NOTE: we might want to keep the in-menu order, for now sort all. */
  std::sort(data->items.begin(), data->items.end(), menu_item_sort_by_drawstr_full);

  if (include_all_areas) {
    CTX_wm_area_set(C, area_init);
    CTX_wm_region_set(C, region_init);

    if (space_type_ui_items_free) {
      MEM_freeN(space_type_ui_items);
    }
  }

  /* Include all operators for developers,
   * since it can be handy to have a quick way to access any operator,
   * including operators being developed which haven't yet been added into the interface.
   *
   * These are added after all menu items so developers still get normal behavior by default,
   * unless searching for something that isn't already in a menu (or scroll down).
   *
   * Keep this behind a developer only check:
   * - Many operators need options to be set to give useful results, see: #74157.
   * - User who really prefer to list all operators can use #WM_OT_search_operator.
   */
  if ((U.flag & USER_DEVELOPER_UI) && single_menu_idname == nullptr) {
    menu_items_from_all_operators(C, data);
  }

  return data;
}

static void menu_search_arg_free_fn(void *data_v)
{
  MEM_delete(static_cast<MenuSearch_Data *>(data_v));
}

static void menu_search_exec_fn(bContext *C, void * /*arg1*/, void *arg2)
{
  MenuSearch_Item *item = (MenuSearch_Item *)arg2;
  if (item == nullptr) {
    return;
  }
  if (item->state & UI_BUT_DISABLED) {
    return;
  }

  ScrArea *area_prev = CTX_wm_area(C);
  ARegion *region_prev = CTX_wm_region(C);

  if (item->wm_context != nullptr) {
    CTX_wm_area_set(C, item->wm_context->area);
    CTX_wm_region_set(C, item->wm_context->region);
  }

  if (auto *op_data = std::get_if<MenuSearch_Item::OperatorData>(&item->data)) {
    CTX_store_set(C, op_data->context);
    WM_operator_name_call_ptr_with_depends_on_cursor(
        C, op_data->type, op_data->opcontext, op_data->opptr, nullptr, item->drawstr);
    CTX_store_set(C, nullptr);
  }
  else if (auto *rna_data = std::get_if<MenuSearch_Item::PropertyData>(&item->data)) {
    PointerRNA *ptr = &rna_data->ptr;
    PropertyRNA *prop = rna_data->prop;
    const int index = rna_data->index;
    const int prop_type = RNA_property_type(prop);
    bool changed = false;

    if (prop_type == PROP_BOOLEAN) {
      const bool is_array = RNA_property_array_check(prop);
      if (is_array) {
        const bool value = RNA_property_boolean_get_index(ptr, prop, index);
        RNA_property_boolean_set_index(ptr, prop, index, !value);
      }
      else {
        const bool value = RNA_property_boolean_get(ptr, prop);
        RNA_property_boolean_set(ptr, prop, !value);
      }
      changed = true;
    }
    else if (prop_type == PROP_ENUM) {
      RNA_property_enum_set(ptr, prop, rna_data->enum_value);
      changed = true;
    }

    if (changed) {
      RNA_property_update(C, ptr, prop);
    }
  }

  if (item->wm_context != nullptr) {
    CTX_wm_area_set(C, area_prev);
    CTX_wm_region_set(C, region_prev);
  }
}

static void menu_search_update_fn(const bContext * /*C*/,
                                  void *arg,
                                  const char *str,
                                  uiSearchItems *items,
                                  const bool /*is_first*/)
{
  MenuSearch_Data *data = (MenuSearch_Data *)arg;

  blender::ui::string_search::StringSearch<MenuSearch_Item> search;

  for (MenuSearch_Item &item : data->items) {
    search.add(item.drawwstr_full, &item, item.weight);
  }

  const blender::Vector<MenuSearch_Item *> filtered_items = search.query(str);

  for (MenuSearch_Item *item : filtered_items) {
    if (!UI_search_item_add(items, item->drawwstr_full, item, item->icon, item->state, 0)) {
      break;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context Menu
 *
 * This uses a fake button to create a context menu,
 * if this ever causes hard to solve bugs we may need to create
 * a separate context menu just for the search, however this is fairly involved.
 * \{ */

static bool ui_search_menu_create_context_menu(bContext *C,
                                               void *arg,
                                               void *active,
                                               const wmEvent *event)
{
  MenuSearch_Data *data = (MenuSearch_Data *)arg;
  MenuSearch_Item *item = (MenuSearch_Item *)active;
  bool has_menu = false;

  new (&data->context_menu_data.but) uiBut();
  new (&data->context_menu_data.block) uiBlock();
  uiBut *but = &data->context_menu_data.but;
  uiBlock *block = &data->context_menu_data.block;

  but->block = block;

  if (menu_items_to_ui_button(item, but)) {
    ScrArea *area_prev = CTX_wm_area(C);
    ARegion *region_prev = CTX_wm_region(C);

    if (item->wm_context != nullptr) {
      CTX_wm_area_set(C, item->wm_context->area);
      CTX_wm_region_set(C, item->wm_context->region);
    }

    if (ui_popup_context_menu_for_button(C, but, event)) {
      has_menu = true;
    }

    if (item->wm_context != nullptr) {
      CTX_wm_area_set(C, area_prev);
      CTX_wm_region_set(C, region_prev);
    }
  }

  return has_menu;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tooltip
 * \{ */

static ARegion *ui_search_menu_create_tooltip(
    bContext *C, ARegion *region, const rcti * /*item_rect*/, void *arg, void *active)
{
  MenuSearch_Data *data = (MenuSearch_Data *)arg;
  MenuSearch_Item *item = (MenuSearch_Item *)active;

  new (&data->context_menu_data.but) uiBut();
  new (&data->context_menu_data.block) uiBlock();
  uiBut *but = &data->context_menu_data.but;
  uiBlock *block = &data->context_menu_data.block;
  unit_m4(block->winmat);
  block->aspect = 1;

  but->block = block;

  /* Place the fake button at the cursor so the tool-tip is places properly. */
  float tip_init[2];
  const wmEvent *event = CTX_wm_window(C)->eventstate;
  tip_init[0] = event->xy[0];
  tip_init[1] = event->xy[1] - (UI_UNIT_Y / 2);
  ui_window_to_block_fl(region, block, &tip_init[0], &tip_init[1]);

  but->rect.xmin = tip_init[0];
  but->rect.xmax = tip_init[0];
  but->rect.ymin = tip_init[1];
  but->rect.ymax = tip_init[1];

  if (menu_items_to_ui_button(item, but)) {
    ScrArea *area_prev = CTX_wm_area(C);
    ARegion *region_prev = CTX_wm_region(C);

    if (item->wm_context != nullptr) {
      CTX_wm_area_set(C, item->wm_context->area);
      CTX_wm_region_set(C, item->wm_context->region);
    }

    ARegion *region_tip = UI_tooltip_create_from_button(C, region, but, false);

    if (item->wm_context != nullptr) {
      CTX_wm_area_set(C, area_prev);
      CTX_wm_region_set(C, region_prev);
    }
    return region_tip;
  }

  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu Search Template Public API
 * \{ */

void UI_but_func_menu_search(uiBut *but, const char *single_menu_idname)
{
  bContext *C = (bContext *)but->block->evil_C;
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  /* When run from top-bar scan all areas in the current window. */
  const bool include_all_areas = (area && (area->spacetype == SPACE_TOPBAR)) &&
                                 !single_menu_idname;
  MenuSearch_Data *data = menu_items_from_ui_create(
      C, win, area, region, include_all_areas, single_menu_idname);
  UI_but_func_search_set(but,
                         /* Generic callback. */
                         ui_searchbox_create_menu,
                         menu_search_update_fn,
                         data,
                         false,
                         menu_search_arg_free_fn,
                         menu_search_exec_fn,
                         nullptr);

  UI_but_func_search_set_context_menu(but, ui_search_menu_create_context_menu);
  UI_but_func_search_set_tooltip(but, ui_search_menu_create_tooltip);
  UI_but_func_search_set_sep_string(but, UI_MENU_ARROW_SEP);
}

void uiTemplateMenuSearch(uiLayout *layout)
{
  uiBlock *block;
  uiBut *but;
  static char search[256] = "";

  block = layout->block();
  blender::ui::block_layout_set_current(block, layout);

  but = uiDefSearchBut(
      block, search, 0, ICON_VIEWZOOM, sizeof(search), 0, 0, UI_UNIT_X * 6, UI_UNIT_Y, "");
  UI_but_func_menu_search(but);
}

/** \} */
