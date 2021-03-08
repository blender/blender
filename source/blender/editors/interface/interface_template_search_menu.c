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
 */

/** \file
 * \ingroup edinterface
 *
 * Search available menu items via the user interface & key-maps.
 * Accessed via the #WM_OT_search_menu operator.
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_texture_types.h"

#include "BLI_alloca.h"
#include "BLI_dynstr.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_memarena.h"
#include "BLI_string.h"
#include "BLI_string_search.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_screen.h"

#include "ED_screen.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "interface_intern.h"

/* For key-map item access. */
#include "wm_event_system.h"

/* -------------------------------------------------------------------- */
/** \name Menu Search Template Implementation
 * \{ */

/* Unicode arrow. */
#define MENU_SEP "\xe2\x96\xb6"

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
  struct MenuSearch_Parent *parent;
  MenuType *parent_mt;
  const char *drawstr;

  /** Set while writing menu items only. */
  struct MenuSearch_Parent *temp_child;
};

struct MenuSearch_Item {
  struct MenuSearch_Item *next, *prev;
  const char *drawstr;
  const char *drawwstr_full;
  /** Support a single level sub-menu nesting (for operator buttons that expand). */
  const char *drawstr_submenu;
  int icon;
  int state;

  struct MenuSearch_Parent *menu_parent;
  MenuType *mt;

  enum {
    MENU_SEARCH_TYPE_OP = 1,
    MENU_SEARCH_TYPE_RNA = 2,
  } type;

  union {
    /** Operator menu item. */
    struct {
      wmOperatorType *type;
      PointerRNA *opptr;
      short opcontext;
      bContextStore *context;
    } op;

    /** Property (only for check-box/boolean). */
    struct {
      PointerRNA ptr;
      PropertyRNA *prop;
      int index;
      /** Only for enum buttons. */
      int enum_value;
    } rna;
  };

  /** Set when we need each menu item to be able to set its own context. may be NULL. */
  struct MenuSearch_Context *wm_context;
};

struct MenuSearch_Data {
  /** MenuSearch_Item */
  ListBase items;
  /** Use for all small allocations. */
  MemArena *memarena;

  /** Use for context menu, to fake a button to create a context menu. */
  struct {
    uiBut but;
    uiBlock block;
  } context_menu_data;
};

static int menu_item_sort_by_drawstr_full(const void *menu_item_a_v, const void *menu_item_b_v)
{
  const struct MenuSearch_Item *menu_item_a = menu_item_a_v;
  const struct MenuSearch_Item *menu_item_b = menu_item_b_v;
  return strcmp(menu_item_a->drawwstr_full, menu_item_b->drawwstr_full);
}

static const char *strdup_memarena(MemArena *memarena, const char *str)
{
  const uint str_size = strlen(str) + 1;
  char *str_dst = BLI_memarena_alloc(memarena, str_size);
  memcpy(str_dst, str, str_size);
  return str_dst;
}

static const char *strdup_memarena_from_dynstr(MemArena *memarena, DynStr *dyn_str)
{
  const uint str_size = BLI_dynstr_get_len(dyn_str) + 1;
  char *str_dst = BLI_memarena_alloc(memarena, str_size);
  BLI_dynstr_get_cstring_ex(dyn_str, str_dst);
  return str_dst;
}

static bool menu_items_from_ui_create_item_from_button(struct MenuSearch_Data *data,
                                                       MemArena *memarena,
                                                       struct MenuType *mt,
                                                       const char *drawstr_submenu,
                                                       uiBut *but,
                                                       struct MenuSearch_Context *wm_context)
{
  struct MenuSearch_Item *item = NULL;

  /* Use override if the name is empty, this can happen with popovers. */
  const char *drawstr_override = NULL;
  const char *drawstr_sep = (but->flag & UI_BUT_HAS_SEP_CHAR) ?
                                strrchr(but->drawstr, UI_SEP_CHAR) :
                                NULL;
  const bool drawstr_is_empty = (drawstr_sep == but->drawstr) || (but->drawstr[0] == '\0');

  if (but->optype != NULL) {
    if (drawstr_is_empty) {
      drawstr_override = WM_operatortype_name(but->optype, but->opptr);
    }

    item = BLI_memarena_calloc(memarena, sizeof(*item));
    item->type = MENU_SEARCH_TYPE_OP;

    item->op.type = but->optype;
    item->op.opcontext = but->opcontext;
    item->op.context = but->context;
    item->op.opptr = but->opptr;
    but->opptr = NULL;
  }
  else if (but->rnaprop != NULL) {
    const int prop_type = RNA_property_type(but->rnaprop);

    if (drawstr_is_empty) {
      if (prop_type == PROP_ENUM) {
        const int value_enum = (int)but->hardmax;
        EnumPropertyItem enum_item;
        if (RNA_property_enum_item_from_value_gettexted(
                but->block->evil_C, &but->rnapoin, but->rnaprop, value_enum, &enum_item)) {
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
             but->drawstr,
             mt->idname,
             prop_type);
    }
    else {
      item = BLI_memarena_calloc(memarena, sizeof(*item));
      item->type = MENU_SEARCH_TYPE_RNA;

      item->rna.ptr = but->rnapoin;
      item->rna.prop = but->rnaprop;
      item->rna.index = but->rnaindex;

      if (prop_type == PROP_ENUM) {
        item->rna.enum_value = (int)but->hardmax;
      }
    }
  }

  if (item != NULL) {
    /* Handle shared settings. */
    if (drawstr_override != NULL) {
      const char *drawstr_suffix = drawstr_sep ? drawstr_sep : "";
      char *drawstr_alloc = BLI_string_joinN("(", drawstr_override, ")", drawstr_suffix);
      item->drawstr = strdup_memarena(memarena, drawstr_alloc);
      MEM_freeN(drawstr_alloc);
    }
    else {
      item->drawstr = strdup_memarena(memarena, but->drawstr);
    }

    item->icon = ui_but_icon(but);
    item->state = (but->flag &
                   (UI_BUT_DISABLED | UI_BUT_INACTIVE | UI_BUT_REDALERT | UI_BUT_HAS_SEP_CHAR));
    item->mt = mt;
    item->drawstr_submenu = drawstr_submenu ? strdup_memarena(memarena, drawstr_submenu) : NULL;

    item->wm_context = wm_context;

    BLI_addtail(&data->items, item);
    return true;
  }

  return false;
}

/**
 * Populate a fake button from a menu item (use for context menu).
 */
static bool menu_items_to_ui_button(struct MenuSearch_Item *item, uiBut *but)
{
  bool changed = false;
  switch (item->type) {
    case MENU_SEARCH_TYPE_OP: {
      but->optype = item->op.type;
      but->opcontext = item->op.opcontext;
      but->context = item->op.context;
      but->opptr = item->op.opptr;
      changed = true;
      break;
    }
    case MENU_SEARCH_TYPE_RNA: {
      const int prop_type = RNA_property_type(item->rna.prop);

      but->rnapoin = item->rna.ptr;
      but->rnaprop = item->rna.prop;
      but->rnaindex = item->rna.index;

      if (prop_type == PROP_ENUM) {
        but->hardmax = item->rna.enum_value;
      }
      changed = true;
      break;
    }
  }

  if (changed) {
    STRNCPY(but->drawstr, item->drawstr);
    char *drawstr_sep = (item->state & UI_BUT_HAS_SEP_CHAR) ? strrchr(but->drawstr, UI_SEP_CHAR) :
                                                              NULL;
    if (drawstr_sep) {
      *drawstr_sep = '\0';
    }

    but->icon = item->icon;
    but->str = but->strdata;
  }

  return changed;
}

/**
 * Populate \a menu_stack with menus from inspecting active key-maps for this context.
 */
static void menu_types_add_from_keymap_items(bContext *C,
                                             wmWindow *win,
                                             ScrArea *area,
                                             ARegion *region,
                                             LinkNode **menuid_stack_p,
                                             GHash *menu_to_kmi,
                                             GSet *menu_tagged)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  ListBase *handlers[] = {
      region ? &region->handlers : NULL,
      area ? &area->handlers : NULL,
      &win->handlers,
  };

  for (int handler_index = 0; handler_index < ARRAY_SIZE(handlers); handler_index++) {
    if (handlers[handler_index] == NULL) {
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

      if (handler_base->poll == NULL || handler_base->poll(region, win->eventstate)) {
        wmEventHandler_Keymap *handler = (wmEventHandler_Keymap *)handler_base;
        wmKeyMap *keymap = WM_event_get_keymap_from_handler(wm, handler);
        if (keymap && WM_keymap_poll(C, keymap)) {
          LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
            if (kmi->flag & KMI_INACTIVE) {
              continue;
            }
            if (STR_ELEM(kmi->idname, "WM_OT_call_menu", "WM_OT_call_menu_pie")) {
              char menu_idname[MAX_NAME];
              RNA_string_get(kmi->ptr, "name", menu_idname);
              MenuType *mt = WM_menutype_find(menu_idname, false);

              if (mt && BLI_gset_add(menu_tagged, mt)) {
                /* Unlikely, but possible this will be included twice. */
                BLI_linklist_prepend(menuid_stack_p, mt);

                void **kmi_p;
                if (!BLI_ghash_ensure_p(menu_to_kmi, mt, &kmi_p)) {
                  *kmi_p = kmi;
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
static void menu_items_from_all_operators(bContext *C, struct MenuSearch_Data *data)
{
  /* Add to temporary list so we can sort them separately. */
  ListBase operator_items = {NULL, NULL};

  MemArena *memarena = data->memarena;
  GHashIterator iter;
  for (WM_operatortype_iter(&iter); !BLI_ghashIterator_done(&iter);
       BLI_ghashIterator_step(&iter)) {
    wmOperatorType *ot = BLI_ghashIterator_getValue(&iter);

    if ((ot->flag & OPTYPE_INTERNAL) && (G.debug & G_DEBUG_WM) == 0) {
      continue;
    }

    if (WM_operator_poll((bContext *)C, ot)) {
      const char *ot_ui_name = CTX_IFACE_(ot->translation_context, ot->name);

      struct MenuSearch_Item *item = NULL;
      item = BLI_memarena_calloc(memarena, sizeof(*item));
      item->type = MENU_SEARCH_TYPE_OP;

      item->op.type = ot;
      item->op.opcontext = WM_OP_INVOKE_DEFAULT;
      item->op.context = NULL;

      char idname_as_py[OP_MAX_TYPENAME];
      char uiname[256];
      WM_operator_py_idname(idname_as_py, ot->idname);

      SNPRINTF(uiname, "%s " MENU_SEP "%s", idname_as_py, ot_ui_name);

      item->drawwstr_full = strdup_memarena(memarena, uiname);
      item->drawstr = ot_ui_name;

      item->wm_context = NULL;

      BLI_addtail(&operator_items, item);
    }
  }

  BLI_listbase_sort(&operator_items, menu_item_sort_by_drawstr_full);

  BLI_movelisttolist(&data->items, &operator_items);
}

/**
 * Create #MenuSearch_Data by inspecting the current context, this uses two methods:
 *
 * - Look up predefined editor-menus.
 * - Look up key-map items which call menus.
 */
static struct MenuSearch_Data *menu_items_from_ui_create(
    bContext *C, wmWindow *win, ScrArea *area_init, ARegion *region_init, bool include_all_areas)
{
  MemArena *memarena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
  /** Map (#MenuType to #MenuSearch_Parent) */
  GHash *menu_parent_map = BLI_ghash_ptr_new(__func__);
  GHash *menu_display_name_map = BLI_ghash_ptr_new(__func__);
  const uiStyle *style = UI_style_get_dpi();

  /* Convert into non-ui structure. */
  struct MenuSearch_Data *data = MEM_callocN(sizeof(*data), __func__);

  DynStr *dyn_str = BLI_dynstr_new_memarena();

  /* Use a stack of menus to handle and discover new menus in passes. */
  LinkNode *menu_stack = NULL;

  /* Tag menu types not to add, either because they have already been added
   * or they have been blacklisted.
   * Set of #MenuType. */
  GSet *menu_tagged = BLI_gset_ptr_new(__func__);
  /** Map (#MenuType -> #wmKeyMapItem). */
  GHash *menu_to_kmi = BLI_ghash_ptr_new(__func__);

  /* Blacklist menus we don't want to show. */
  {
    const char *idname_array[] = {
        /* While we could include this, it's just showing filenames to load. */
        "TOPBAR_MT_file_open_recent",
    };
    for (int i = 0; i < ARRAY_SIZE(idname_array); i++) {
      MenuType *mt = WM_menutype_find(idname_array[i], false);
      if (mt != NULL) {
        BLI_gset_add(menu_tagged, mt);
      }
    }
  }

  {
    /* Exclude context menus because:
     * - The menu items are available elsewhere (and will show up multiple times).
     * - Menu items depend on exact context, making search results unpredictable
     *   (exact number of items selected for example). See design doc T74158.
     * There is one exception,
     * as the outliner only exposes functionality via the context menu. */
    GHashIterator iter;

    for (WM_menutype_iter(&iter); (!BLI_ghashIterator_done(&iter));
         (BLI_ghashIterator_step(&iter))) {
      MenuType *mt = BLI_ghashIterator_getValue(&iter);
      if (BLI_str_endswith(mt->idname, "_context_menu")) {
        BLI_gset_add(menu_tagged, mt);
      }
    }
    const char *idname_array[] = {
        /* Add back some context menus. */
        "OUTLINER_MT_context_menu",
    };
    for (int i = 0; i < ARRAY_SIZE(idname_array); i++) {
      MenuType *mt = WM_menutype_find(idname_array[i], false);
      if (mt != NULL) {
        BLI_gset_remove(menu_tagged, mt, NULL);
      }
    }
  }

  /* Collect contexts, one for each 'ui_type'. */
  struct MenuSearch_Context *wm_contexts = NULL;

  const EnumPropertyItem *space_type_ui_items = NULL;
  int space_type_ui_items_len = 0;
  bool space_type_ui_items_free = false;

  /* Text used as prefix for top-bar menu items. */
  const char *global_menu_prefix = NULL;

  if (include_all_areas) {
    /* First create arrays for ui_type. */
    PropertyRNA *prop_ui_type = NULL;
    {
      PointerRNA ptr;
      RNA_pointer_create(NULL, &RNA_Area, NULL, &ptr);
      prop_ui_type = RNA_struct_find_property(&ptr, "ui_type");
      RNA_property_enum_items(C,
                              &ptr,
                              prop_ui_type,
                              &space_type_ui_items,
                              &space_type_ui_items_len,
                              &space_type_ui_items_free);

      wm_contexts = BLI_memarena_calloc(memarena, sizeof(*wm_contexts) * space_type_ui_items_len);
      for (int i = 0; i < space_type_ui_items_len; i++) {
        wm_contexts[i].space_type_ui_index = -1;
      }
    }

    bScreen *screen = WM_window_get_active_screen(win);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
      if (region != NULL) {
        PointerRNA ptr;
        RNA_pointer_create(&screen->id, &RNA_Area, area, &ptr);
        const int space_type_ui = RNA_property_enum_get(&ptr, prop_ui_type);

        const int space_type_ui_index = RNA_enum_from_value(space_type_ui_items, space_type_ui);
        if (space_type_ui_index == -1) {
          continue;
        }

        if (wm_contexts[space_type_ui_index].space_type_ui_index != -1) {
          ScrArea *area_best = wm_contexts[space_type_ui_index].area;
          const uint value_best = (uint)area_best->winx * (uint)area_best->winy;
          const uint value_test = (uint)area->winx * (uint)area->winy;
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

  GHashIterator iter;

  for (int space_type_ui_index = -1; space_type_ui_index < space_type_ui_items_len;
       space_type_ui_index += 1) {

    ScrArea *area = NULL;
    ARegion *region = NULL;
    struct MenuSearch_Context *wm_context = NULL;

    if (include_all_areas) {
      if (space_type_ui_index == -1) {
        /* First run without any context, to populate the top-bar without. */
        wm_context = NULL;
        area = NULL;
        region = NULL;
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

    /* Populate menus from the editors,
     * note that we could create a fake header, draw the header and extract the menus
     * from the buttons, however this is quite involved and can be avoided as by convention
     * each space-type has a single root-menu that headers use. */
    {
      const char *idname_array[2] = {NULL};
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

      if (area != NULL) {
        SpaceLink *sl = area->spacedata.first;
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
          SPACE_MENU_MAP(SPACE_ACTION,
                         (((const SpaceAction *)sl)->mode == SACTCONT_TIMELINE) ?
                             "TIME_MT_editor_menus" :
                             "DOPESHEET_MT_editor_menus");
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
        if (mt != NULL) {
          /* Check if this exists because of 'include_all_areas'. */
          if (BLI_gset_add(menu_tagged, mt)) {
            BLI_linklist_prepend(&menu_stack, mt);
          }
        }
      }
    }
#undef SPACE_MENU_MAP
#undef SPACE_MENU_NOP

    bool has_keymap_menu_items = false;

    while (menu_stack != NULL) {
      MenuType *mt = BLI_linklist_pop(&menu_stack);
      if (!WM_menutype_poll(C, mt)) {
        continue;
      }

      uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
      uiLayout *layout = UI_block_layout(
          block, UI_LAYOUT_VERTICAL, UI_LAYOUT_MENU, 0, 0, 200, 0, UI_MENU_PADDING, style);

      UI_block_flag_enable(block, UI_BLOCK_SHOW_SHORTCUT_ALWAYS);

      uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
      UI_menutype_draw(C, mt, layout);

      UI_block_end(C, block);

      LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
        MenuType *mt_from_but = NULL;
        /* Support menu titles with dynamic from initial labels
         * (used by edit-mesh context menu). */
        if (but->type == UI_BTYPE_LABEL) {

          /* Check if the label is the title. */
          uiBut *but_test = but->prev;
          while (but_test && but_test->type == UI_BTYPE_SEPR) {
            but_test = but_test->prev;
          }

          if (but_test == NULL) {
            BLI_ghash_insert(
                menu_display_name_map, mt, (void *)strdup_memarena(memarena, but->drawstr));
          }
        }
        else if (menu_items_from_ui_create_item_from_button(
                     data, memarena, mt, NULL, but, wm_context)) {
          /* pass */
        }
        else if ((mt_from_but = UI_but_menutype_get(but))) {

          if (BLI_gset_add(menu_tagged, mt_from_but)) {
            BLI_linklist_prepend(&menu_stack, mt_from_but);
          }

          if (!BLI_ghash_haskey(menu_parent_map, mt_from_but)) {
            struct MenuSearch_Parent *menu_parent = BLI_memarena_calloc(memarena,
                                                                        sizeof(*menu_parent));
            /* Use brackets for menu key shortcuts,
             * converting "Text|Some-Shortcut" to "Text (Some-Shortcut)".
             * This is needed so we don't right align sub-menu contents
             * we only want to do that for the last menu item, not the path that leads to it.
             */
            const char *drawstr_sep = but->flag & UI_BUT_HAS_SEP_CHAR ?
                                          strrchr(but->drawstr, UI_SEP_CHAR) :
                                          NULL;
            bool drawstr_is_empty = false;
            if (drawstr_sep != NULL) {
              BLI_assert(BLI_dynstr_get_len(dyn_str) == 0);
              /* Detect empty string, fallback to menu name. */
              const char *drawstr = but->drawstr;
              int drawstr_len = drawstr_sep - but->drawstr;
              if (UNLIKELY(drawstr_len == 0)) {
                drawstr = CTX_IFACE_(mt_from_but->translation_context, mt_from_but->label);
                drawstr_len = strlen(drawstr);
                if (drawstr[0] == '\0') {
                  drawstr_is_empty = true;
                }
              }
              BLI_dynstr_nappend(dyn_str, drawstr, drawstr_len);
              BLI_dynstr_appendf(dyn_str, " (%s)", drawstr_sep + 1);
              menu_parent->drawstr = strdup_memarena_from_dynstr(memarena, dyn_str);
              BLI_dynstr_clear(dyn_str);
            }
            else {
              const char *drawstr = but->drawstr;
              if (UNLIKELY(drawstr[0] == '\0')) {
                drawstr = CTX_IFACE_(mt_from_but->translation_context, mt_from_but->label);
                if (drawstr[0] == '\0') {
                  drawstr_is_empty = true;
                }
              }
              menu_parent->drawstr = strdup_memarena(memarena, drawstr);
            }
            menu_parent->parent_mt = mt;
            BLI_ghash_insert(menu_parent_map, mt_from_but, menu_parent);

            if (drawstr_is_empty) {
              printf("Warning: '%s' menu has empty 'bl_label'.\n", mt_from_but->idname);
            }
          }
        }
        else if (but->menu_create_func != NULL) {
          /* A non 'MenuType' menu button. */

          /* Only expand one level deep, this is mainly for expanding operator menus. */
          const char *drawstr_submenu = but->drawstr;

          /* +1 to avoid overlap with the current 'block'. */
          uiBlock *sub_block = UI_block_begin(C, region, __func__ + 1, UI_EMBOSS);
          uiLayout *sub_layout = UI_block_layout(
              sub_block, UI_LAYOUT_VERTICAL, UI_LAYOUT_MENU, 0, 0, 200, 0, UI_MENU_PADDING, style);

          UI_block_flag_enable(sub_block, UI_BLOCK_SHOW_SHORTCUT_ALWAYS);

          uiLayoutSetOperatorContext(sub_layout, WM_OP_INVOKE_REGION_WIN);

          but->menu_create_func(C, sub_layout, but->poin);

          UI_block_end(C, sub_block);

          LISTBASE_FOREACH (uiBut *, sub_but, &sub_block->buttons) {
            menu_items_from_ui_create_item_from_button(
                data, memarena, mt, drawstr_submenu, sub_but, wm_context);
          }

          if (region) {
            BLI_remlink(&region->uiblocks, sub_block);
          }
          UI_block_free(NULL, sub_block);
        }
      }
      if (region) {
        BLI_remlink(&region->uiblocks, block);
      }
      UI_block_free(NULL, block);

      /* Add key-map items as a second pass,
       * so all menus are accessed from the header & top-bar before key shortcuts are expanded. */
      if ((menu_stack == NULL) && (has_keymap_menu_items == false)) {
        has_keymap_menu_items = true;
        menu_types_add_from_keymap_items(
            C, win, area, region, &menu_stack, menu_to_kmi, menu_tagged);
      }
    }
  }

  LISTBASE_FOREACH (struct MenuSearch_Item *, item, &data->items) {
    item->menu_parent = BLI_ghash_lookup(menu_parent_map, item->mt);
  }

  GHASH_ITER (iter, menu_parent_map) {
    struct MenuSearch_Parent *menu_parent = BLI_ghashIterator_getValue(&iter);
    menu_parent->parent = BLI_ghash_lookup(menu_parent_map, menu_parent->parent_mt);
  }

  /* NOTE: currently this builds the full path for each menu item,
   * that could be moved into the parent menu. */

  /* Set names as full paths. */
  LISTBASE_FOREACH (struct MenuSearch_Item *, item, &data->items) {
    BLI_assert(BLI_dynstr_get_len(dyn_str) == 0);

    if (include_all_areas) {
      BLI_dynstr_appendf(dyn_str,
                         "%s: ",
                         (item->wm_context != NULL) ?
                             space_type_ui_items[item->wm_context->space_type_ui_index].name :
                             global_menu_prefix);
    }

    if (item->menu_parent != NULL) {
      struct MenuSearch_Parent *menu_parent = item->menu_parent;
      menu_parent->temp_child = NULL;
      while (menu_parent && menu_parent->parent) {
        menu_parent->parent->temp_child = menu_parent;
        menu_parent = menu_parent->parent;
      }
      while (menu_parent) {
        BLI_dynstr_append(dyn_str, menu_parent->drawstr);
        BLI_dynstr_append(dyn_str, " " MENU_SEP " ");
        menu_parent = menu_parent->temp_child;
      }
    }
    else {
      const char *drawstr = BLI_ghash_lookup(menu_display_name_map, item->mt);
      if (drawstr == NULL) {
        drawstr = CTX_IFACE_(item->mt->translation_context, item->mt->label);
      }
      BLI_dynstr_append(dyn_str, drawstr);

      wmKeyMapItem *kmi = BLI_ghash_lookup(menu_to_kmi, item->mt);
      if (kmi != NULL) {
        char kmi_str[128];
        WM_keymap_item_to_string(kmi, false, kmi_str, sizeof(kmi_str));
        BLI_dynstr_appendf(dyn_str, " (%s)", kmi_str);
      }

      BLI_dynstr_append(dyn_str, " " MENU_SEP " ");
    }

    /* Optional nested menu. */
    if (item->drawstr_submenu != NULL) {
      BLI_dynstr_append(dyn_str, item->drawstr_submenu);
      BLI_dynstr_append(dyn_str, " " MENU_SEP " ");
    }

    BLI_dynstr_append(dyn_str, item->drawstr);

    item->drawwstr_full = strdup_memarena_from_dynstr(memarena, dyn_str);
    BLI_dynstr_clear(dyn_str);
  }
  BLI_dynstr_free(dyn_str);

  /* Finally sort menu items.
   *
   * Note: we might want to keep the in-menu order, for now sort all. */
  BLI_listbase_sort(&data->items, menu_item_sort_by_drawstr_full);

  BLI_ghash_free(menu_parent_map, NULL, NULL);
  BLI_ghash_free(menu_display_name_map, NULL, NULL);

  BLI_ghash_free(menu_to_kmi, NULL, NULL);

  BLI_gset_free(menu_tagged, NULL);

  data->memarena = memarena;

  if (include_all_areas) {
    CTX_wm_area_set(C, area_init);
    CTX_wm_region_set(C, region_init);

    if (space_type_ui_items_free) {
      MEM_freeN((void *)space_type_ui_items);
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
   * - Many operators need options to be set to give useful results, see: T74157.
   * - User who really prefer to list all operators can use #WM_OT_search_operator.
   */
  if (U.flag & USER_DEVELOPER_UI) {
    menu_items_from_all_operators(C, data);
  }

  return data;
}

static void menu_search_arg_free_fn(void *data_v)
{
  struct MenuSearch_Data *data = data_v;
  LISTBASE_FOREACH (struct MenuSearch_Item *, item, &data->items) {
    switch (item->type) {
      case MENU_SEARCH_TYPE_OP: {
        if (item->op.opptr != NULL) {
          WM_operator_properties_free(item->op.opptr);
          MEM_freeN(item->op.opptr);
        }
      }
      case MENU_SEARCH_TYPE_RNA: {
        break;
      }
    }
  }

  BLI_memarena_free(data->memarena);

  MEM_freeN(data);
}

static void menu_search_exec_fn(bContext *C, void *UNUSED(arg1), void *arg2)
{
  struct MenuSearch_Item *item = arg2;
  if (item == NULL) {
    return;
  }
  if (item->state & UI_BUT_DISABLED) {
    return;
  }

  ScrArea *area_prev = CTX_wm_area(C);
  ARegion *region_prev = CTX_wm_region(C);

  if (item->wm_context != NULL) {
    CTX_wm_area_set(C, item->wm_context->area);
    CTX_wm_region_set(C, item->wm_context->region);
  }

  switch (item->type) {
    case MENU_SEARCH_TYPE_OP: {
      CTX_store_set(C, item->op.context);
      WM_operator_name_call_ptr(C, item->op.type, item->op.opcontext, item->op.opptr);
      CTX_store_set(C, NULL);
      break;
    }
    case MENU_SEARCH_TYPE_RNA: {
      PointerRNA *ptr = &item->rna.ptr;
      PropertyRNA *prop = item->rna.prop;
      const int index = item->rna.index;
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
        RNA_property_enum_set(ptr, prop, item->rna.enum_value);
        changed = true;
      }

      if (changed) {
        RNA_property_update(C, ptr, prop);
      }
      break;
    }
  }

  if (item->wm_context != NULL) {
    CTX_wm_area_set(C, area_prev);
    CTX_wm_region_set(C, region_prev);
  }
}

static void menu_search_update_fn(const bContext *UNUSED(C),
                                  void *arg,
                                  const char *str,
                                  uiSearchItems *items,
                                  const bool UNUSED(is_first))
{
  struct MenuSearch_Data *data = arg;

  StringSearch *search = BLI_string_search_new();

  LISTBASE_FOREACH (struct MenuSearch_Item *, item, &data->items) {
    BLI_string_search_add(search, item->drawwstr_full, item);
  }

  struct MenuSearch_Item **filtered_items;
  const int filtered_amount = BLI_string_search_query(search, str, (void ***)&filtered_items);

  for (int i = 0; i < filtered_amount; i++) {
    struct MenuSearch_Item *item = filtered_items[i];
    if (!UI_search_item_add(items, item->drawwstr_full, item, item->icon, item->state, 0)) {
      break;
    }
  }

  MEM_freeN(filtered_items);
  BLI_string_search_free(search);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context Menu
 *
 * This uses a fake button to create a context menu,
 * if this ever causes hard to solve bugs we may need to create
 * a separate context menu just for the search, however this is fairly involved.
 * \{ */

static bool ui_search_menu_create_context_menu(struct bContext *C,
                                               void *arg,
                                               void *active,
                                               const struct wmEvent *UNUSED(event))
{
  struct MenuSearch_Data *data = arg;
  struct MenuSearch_Item *item = active;
  bool has_menu = false;

  memset(&data->context_menu_data, 0x0, sizeof(data->context_menu_data));
  uiBut *but = &data->context_menu_data.but;
  uiBlock *block = &data->context_menu_data.block;

  but->block = block;

  if (menu_items_to_ui_button(item, but)) {
    ScrArea *area_prev = CTX_wm_area(C);
    ARegion *region_prev = CTX_wm_region(C);

    if (item->wm_context != NULL) {
      CTX_wm_area_set(C, item->wm_context->area);
      CTX_wm_region_set(C, item->wm_context->region);
    }

    if (ui_popup_context_menu_for_button(C, but)) {
      has_menu = true;
    }

    if (item->wm_context != NULL) {
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

static struct ARegion *ui_search_menu_create_tooltip(struct bContext *C,
                                                     struct ARegion *region,
                                                     const rcti *UNUSED(item_rect),
                                                     void *arg,
                                                     void *active)
{
  struct MenuSearch_Data *data = arg;
  struct MenuSearch_Item *item = active;

  memset(&data->context_menu_data, 0x0, sizeof(data->context_menu_data));
  uiBut *but = &data->context_menu_data.but;
  uiBlock *block = &data->context_menu_data.block;
  unit_m4(block->winmat);
  block->aspect = 1;

  but->block = block;

  /* Place the fake button at the cursor so the tool-tip is places properly. */
  float tip_init[2];
  const wmEvent *event = CTX_wm_window(C)->eventstate;
  tip_init[0] = event->x;
  tip_init[1] = event->y - (UI_UNIT_Y / 2);
  ui_window_to_block_fl(region, block, &tip_init[0], &tip_init[1]);

  but->rect.xmin = tip_init[0];
  but->rect.xmax = tip_init[0];
  but->rect.ymin = tip_init[1];
  but->rect.ymax = tip_init[1];

  if (menu_items_to_ui_button(item, but)) {
    ScrArea *area_prev = CTX_wm_area(C);
    ARegion *region_prev = CTX_wm_region(C);

    if (item->wm_context != NULL) {
      CTX_wm_area_set(C, item->wm_context->area);
      CTX_wm_region_set(C, item->wm_context->region);
    }

    ARegion *region_tip = UI_tooltip_create_from_button(C, region, but, false);

    if (item->wm_context != NULL) {
      CTX_wm_area_set(C, area_prev);
      CTX_wm_region_set(C, region_prev);
    }
    return region_tip;
  }

  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu Search Template Public API
 * \{ */

void UI_but_func_menu_search(uiBut *but)
{
  bContext *C = but->block->evil_C;
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  /* When run from top-bar scan all areas in the current window. */
  const bool include_all_areas = (area && (area->spacetype == SPACE_TOPBAR));
  struct MenuSearch_Data *data = menu_items_from_ui_create(
      C, win, area, region, include_all_areas);
  UI_but_func_search_set(but,
                         /* Generic callback. */
                         ui_searchbox_create_menu,
                         menu_search_update_fn,
                         data,
                         menu_search_arg_free_fn,
                         menu_search_exec_fn,
                         NULL);

  UI_but_func_search_set_context_menu(but, ui_search_menu_create_context_menu);
  UI_but_func_search_set_tooltip(but, ui_search_menu_create_tooltip);
  UI_but_func_search_set_sep_string(but, MENU_SEP);
}

void uiTemplateMenuSearch(uiLayout *layout)
{
  uiBlock *block;
  uiBut *but;
  static char search[256] = "";

  block = uiLayoutGetBlock(layout);
  UI_block_layout_set_current(block, layout);

  but = uiDefSearchBut(
      block, search, 0, ICON_VIEWZOOM, sizeof(search), 0, 0, UI_UNIT_X * 6, UI_UNIT_Y, 0, 0, "");
  UI_but_func_menu_search(but);
}

#undef MENU_SEP

/** \} */
