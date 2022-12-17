/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_texture_types.h"

#include "BLI_alloca.h"
#include "BLI_fnmatch.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_search.h"
#include "BLI_timecode.h"
#include "BLI_utildefines.h"

#include "BLF_api.h"
#include "BLT_translation.h"

#include "BKE_action.h"
#include "BKE_colorband.h"
#include "BKE_colortools.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_curveprofile.h"
#include "BKE_global.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_linestyle.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_packedFile.h"
#include "BKE_particle.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_shader_fx.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_fileselect.h"
#include "ED_object.h"
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_undo.h"

#include "RE_engine.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BLO_readfile.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_view2d.h"
#include "interface_intern.hh"

#include "PIL_time.h"

/* we may want to make this optional, disable for now. */
// #define USE_OP_RESET_BUT

/* defines for templateID/TemplateSearch */
#define TEMPLATE_SEARCH_TEXTBUT_MIN_WIDTH (UI_UNIT_X * 6)
#define TEMPLATE_SEARCH_TEXTBUT_HEIGHT UI_UNIT_Y

void UI_template_fix_linking(void)
{
}

/* -------------------------------------------------------------------- */
/** \name Header Template
 * \{ */

void uiTemplateHeader(uiLayout *layout, bContext *C)
{
  uiBlock *block = uiLayoutAbsoluteBlock(layout);
  ED_area_header_switchbutton(C, block, 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Search Menu Helpers
 * \{ */

static int template_search_textbut_width(PointerRNA *ptr, PropertyRNA *name_prop)
{
  char str[UI_MAX_DRAW_STR];
  int buf_len = 0;

  BLI_assert(RNA_property_type(name_prop) == PROP_STRING);

  const char *name = RNA_property_string_get_alloc(ptr, name_prop, str, sizeof(str), &buf_len);

  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
  const int margin = UI_UNIT_X * 0.75f;
  const int estimated_width = UI_fontstyle_string_width(fstyle, name) + margin;

  if (name != str) {
    MEM_freeN((void *)name);
  }

  /* Clamp to some min/max width. */
  return CLAMPIS(
      estimated_width, TEMPLATE_SEARCH_TEXTBUT_MIN_WIDTH, TEMPLATE_SEARCH_TEXTBUT_MIN_WIDTH * 3);
}

static int template_search_textbut_height(void)
{
  return TEMPLATE_SEARCH_TEXTBUT_HEIGHT;
}

/**
 * Add a block button for the search menu for templateID and templateSearch.
 */
static void template_add_button_search_menu(const bContext *C,
                                            uiLayout *layout,
                                            uiBlock *block,
                                            PointerRNA *ptr,
                                            PropertyRNA *prop,
                                            uiBlockCreateFunc block_func,
                                            void *block_argN,
                                            const char *const tip,
                                            const bool use_previews,
                                            const bool editable,
                                            const bool live_icon)
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
      col = uiLayoutColumn(layout, true);
    }

    but = uiDefBlockButN(block, block_func, block_argN, "", 0, 0, width, height, tip);
    if (use_preview_icon) {
      const int icon = id ? ui_id_icon_get(C, id, use_big_size) : RNA_struct_ui_icon(type);
      ui_def_but_icon(but, icon, UI_HAS_ICON | UI_BUT_ICON_PREVIEW);
    }
    else {
      ui_def_but_icon(but, RNA_struct_ui_icon(type), UI_HAS_ICON);
      UI_but_drawflag_enable(but, UI_BUT_ICON_LEFT);
    }

    if ((idfrom && idfrom->lib) || !editable) {
      UI_but_flag_enable(but, UI_BUT_DISABLED);
    }
    if (use_big_size) {
      uiLayoutRow(col ? col : layout, true);
    }
  }
  else {
    but = uiDefBlockButN(block, block_func, block_argN, "", 0, 0, UI_UNIT_X * 1.6, UI_UNIT_Y, tip);

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

    if ((idfrom && idfrom->lib) || !editable) {
      UI_but_flag_enable(but, UI_BUT_DISABLED);
    }
  }
}

static uiBlock *template_common_search_menu(const bContext *C,
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

  uiBlock *block = UI_block_begin(C, region, "_popup", UI_EMBOSS);
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_SEARCH_MENU);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  /* preview thumbnails */
  if (preview_rows > 0 && preview_cols > 0) {
    const int w = 4 * U.widget_unit * preview_cols * scale;
    const int h = 5 * U.widget_unit * preview_rows * scale;

    /* fake button, it holds space for search items */
    uiDefBut(block, UI_BTYPE_LABEL, 0, "", 10, 26, w, h, nullptr, 0, 0, 0, 0, nullptr);

    but = uiDefSearchBut(block,
                         search,
                         0,
                         ICON_VIEWZOOM,
                         sizeof(search),
                         10,
                         0,
                         w,
                         UI_UNIT_Y,
                         preview_rows,
                         preview_cols,
                         "");
  }
  /* list view */
  else {
    const int searchbox_width = UI_searchbox_size_x();
    const int searchbox_height = UI_searchbox_size_y();

    /* fake button, it holds space for search items */
    uiDefBut(block,
             UI_BTYPE_LABEL,
             0,
             "",
             10,
             15,
             searchbox_width,
             searchbox_height,
             nullptr,
             0,
             0,
             0,
             0,
             nullptr);
    but = uiDefSearchBut(block,
                         search,
                         0,
                         ICON_VIEWZOOM,
                         sizeof(search),
                         10,
                         0,
                         searchbox_width,
                         UI_UNIT_Y - 1,
                         0,
                         0,
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

  UI_block_bounds_set_normal(block, 0.3f * U.widget_unit);
  UI_block_direction_set(block, UI_DIR_DOWN);

  /* give search-field focus */
  UI_but_focus_on_enter_event(win, but);
  /* this type of search menu requires undo */
  but->flag |= UI_BUT_UNDO;

  return block;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Search Callbacks
 * \{ */

struct TemplateID {
  PointerRNA ptr;
  PropertyRNA *prop;

  ListBase *idlb;
  short idcode;
  short filter;
  int prv_rows, prv_cols;
  bool preview;
  float scale;
};

/* Search browse menu, assign. */
static void template_ID_set_property_exec_fn(bContext *C, void *arg_template, void *item)
{
  TemplateID *template_ui = (TemplateID *)arg_template;

  /* ID */
  if (item) {
    PointerRNA idptr;

    RNA_id_pointer_create(static_cast<ID *>(item), &idptr);
    RNA_property_pointer_set(&template_ui->ptr, template_ui->prop, idptr, nullptr);
    RNA_property_update(C, &template_ui->ptr, template_ui->prop);
  }
}

static bool id_search_allows_id(TemplateID *template_ui, const int flag, ID *id, const char *query)
{
  ID *id_from = template_ui->ptr.owner_id;

  /* Do self check. */
  if ((flag & PROP_ID_SELF_CHECK) && id == id_from) {
    return false;
  }

  /* Use filter. */
  if (RNA_property_type(template_ui->prop) == PROP_POINTER) {
    PointerRNA ptr;
    RNA_id_pointer_create(id, &ptr);
    if (RNA_property_pointer_poll(&template_ui->ptr, template_ui->prop, &ptr) == 0) {
      return false;
    }
  }

  /* Hide dot prefixed data-blocks, but only if filter does not force them visible. */
  if (U.uiflag & USER_HIDE_DOT) {
    if ((id->name[2] == '.') && (query[0] != '.')) {
      return false;
    }
  }

  return true;
}

static bool id_search_add(const bContext *C, TemplateID *template_ui, uiSearchItems *items, ID *id)
{
  /* +1 is needed because BKE_id_ui_prefix used 3 letter prefix
   * followed by ID_NAME-2 characters from id->name
   */
  char name_ui[MAX_ID_FULL_NAME_UI];
  int iconid = ui_id_icon_get(C, id, template_ui->preview);
  const bool use_lib_prefix = template_ui->preview || iconid;
  const bool has_sep_char = ID_IS_LINKED(id);

  /* When using previews, the library hint (linked, overridden, missing) is added with a
   * character prefix, otherwise we can use a icon. */
  int name_prefix_offset;
  BKE_id_full_name_ui_prefix_get(name_ui, id, use_lib_prefix, UI_SEP_CHAR, &name_prefix_offset);
  if (!use_lib_prefix) {
    iconid = UI_icon_from_library(id);
  }

  if (!UI_search_item_add(items,
                          name_ui,
                          id,
                          iconid,
                          has_sep_char ? int(UI_BUT_HAS_SEP_CHAR) : 0,
                          name_prefix_offset)) {
    return false;
  }

  return true;
}

/* ID Search browse menu, do the search */
static void id_search_cb(const bContext *C,
                         void *arg_template,
                         const char *str,
                         uiSearchItems *items,
                         const bool /*is_first*/)
{
  TemplateID *template_ui = (TemplateID *)arg_template;
  ListBase *lb = template_ui->idlb;
  const int flag = RNA_property_flag(template_ui->prop);

  StringSearch *search = BLI_string_search_new();

  /* ID listbase */
  LISTBASE_FOREACH (ID *, id, lb) {
    if (id_search_allows_id(template_ui, flag, id, str)) {
      BLI_string_search_add(search, id->name + 2, id, 0);
    }
  }

  ID **filtered_ids;
  const int filtered_amount = BLI_string_search_query(search, str, (void ***)&filtered_ids);

  for (int i = 0; i < filtered_amount; i++) {
    if (!id_search_add(C, template_ui, items, filtered_ids[i])) {
      break;
    }
  }

  MEM_freeN(filtered_ids);
  BLI_string_search_free(search);
}

/**
 * Use id tags for filtering.
 */
static void id_search_cb_tagged(const bContext *C,
                                void *arg_template,
                                const char *str,
                                uiSearchItems *items)
{
  TemplateID *template_ui = (TemplateID *)arg_template;
  ListBase *lb = template_ui->idlb;
  const int flag = RNA_property_flag(template_ui->prop);

  StringSearch *search = BLI_string_search_new();

  /* ID listbase */
  LISTBASE_FOREACH (ID *, id, lb) {
    if (id->tag & LIB_TAG_DOIT) {
      if (id_search_allows_id(template_ui, flag, id, str)) {
        BLI_string_search_add(search, id->name + 2, id, 0);
      }
      id->tag &= ~LIB_TAG_DOIT;
    }
  }

  ID **filtered_ids;
  const int filtered_amount = BLI_string_search_query(search, str, (void ***)&filtered_ids);

  for (int i = 0; i < filtered_amount; i++) {
    if (!id_search_add(C, template_ui, items, filtered_ids[i])) {
      break;
    }
  }

  MEM_freeN(filtered_ids);
  BLI_string_search_free(search);
}

/**
 * A version of 'id_search_cb' that lists scene objects.
 */
static void id_search_cb_objects_from_scene(const bContext *C,
                                            void *arg_template,
                                            const char *str,
                                            uiSearchItems *items,
                                            const bool /*is_first*/)
{
  TemplateID *template_ui = (TemplateID *)arg_template;
  ListBase *lb = template_ui->idlb;
  Scene *scene = nullptr;
  ID *id_from = template_ui->ptr.owner_id;

  if (id_from && GS(id_from->name) == ID_SCE) {
    scene = (Scene *)id_from;
  }
  else {
    scene = CTX_data_scene(C);
  }

  BKE_main_id_flag_listbase(lb, LIB_TAG_DOIT, false);

  FOREACH_SCENE_OBJECT_BEGIN (scene, ob_iter) {
    ob_iter->id.tag |= LIB_TAG_DOIT;
  }
  FOREACH_SCENE_OBJECT_END;
  id_search_cb_tagged(C, arg_template, str, items);
}

static ARegion *template_ID_search_menu_item_tooltip(
    bContext *C, ARegion *region, const rcti *item_rect, void *arg, void *active)
{
  TemplateID *template_ui = static_cast<TemplateID *>(arg);
  ID *active_id = static_cast<ID *>(active);
  StructRNA *type = RNA_property_pointer_type(&template_ui->ptr, template_ui->prop);

  uiSearchItemTooltipData tooltip_data = {{0}};

  tooltip_data.name = active_id->name + 2;
  BLI_snprintf(tooltip_data.description,
               sizeof(tooltip_data.description),
               TIP_("Choose %s data-block to be assigned to this user"),
               RNA_struct_ui_name(type));
  if (ID_IS_LINKED(active_id)) {
    BLI_snprintf(tooltip_data.hint,
                 sizeof(tooltip_data.hint),
                 TIP_("Source library: %s\n%s"),
                 active_id->lib->id.name + 2,
                 active_id->lib->filepath);
  }

  return UI_tooltip_create_from_search_item_generic(C, region, item_rect, &tooltip_data);
}

/* ID Search browse menu, open */
static uiBlock *id_search_menu(bContext *C, ARegion *region, void *arg_litem)
{
  static TemplateID template_ui;
  PointerRNA active_item_ptr;
  void (*id_search_update_fn)(
      const bContext *, void *, const char *, uiSearchItems *, const bool) = id_search_cb;

  /* arg_litem is malloced, can be freed by parent button */
  template_ui = *((TemplateID *)arg_litem);
  active_item_ptr = RNA_property_pointer_get(&template_ui.ptr, template_ui.prop);

  if (template_ui.filter) {
    /* Currently only used for objects. */
    if (template_ui.idcode == ID_OB) {
      if (template_ui.filter == UI_TEMPLATE_ID_FILTER_AVAILABLE) {
        id_search_update_fn = id_search_cb_objects_from_scene;
      }
    }
  }

  return template_common_search_menu(C,
                                     region,
                                     id_search_update_fn,
                                     &template_ui,
                                     template_ID_set_property_exec_fn,
                                     active_item_ptr.data,
                                     template_ID_search_menu_item_tooltip,
                                     template_ui.prv_rows,
                                     template_ui.prv_cols,
                                     template_ui.scale);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID Template
 * \{ */

static void template_id_cb(bContext *C, void *arg_litem, void *arg_event);

/**
 * This is for browsing and editing the ID-blocks used.
 */
void UI_context_active_but_prop_get_templateID(bContext *C,
                                               PointerRNA *r_ptr,
                                               PropertyRNA **r_prop)
{
  uiBut *but = UI_context_active_but_get(C);

  memset(r_ptr, 0, sizeof(*r_ptr));
  *r_prop = nullptr;

  if (but && (but->funcN == template_id_cb) && but->func_argN) {
    TemplateID *template_ui = static_cast<TemplateID *>(but->func_argN);
    *r_ptr = template_ui->ptr;
    *r_prop = template_ui->prop;
  }
}

static void template_id_liboverride_hierarchy_collection_root_find_recursive(
    Collection *collection,
    const int parent_level,
    Collection **r_collection_parent_best,
    int *r_parent_level_best)
{
  if (!ID_IS_LINKED(collection) && !ID_IS_OVERRIDE_LIBRARY_REAL(collection)) {
    return;
  }
  if (ID_IS_OVERRIDABLE_LIBRARY(collection) || ID_IS_OVERRIDE_LIBRARY_REAL(collection)) {
    if (parent_level > *r_parent_level_best) {
      *r_parent_level_best = parent_level;
      *r_collection_parent_best = collection;
    }
  }
  for (CollectionParent *iter = static_cast<CollectionParent *>(collection->parents.first);
       iter != nullptr;
       iter = iter->next) {
    if (iter->collection->id.lib != collection->id.lib && ID_IS_LINKED(iter->collection)) {
      continue;
    }
    template_id_liboverride_hierarchy_collection_root_find_recursive(
        iter->collection, parent_level + 1, r_collection_parent_best, r_parent_level_best);
  }
}

static void template_id_liboverride_hierarchy_collections_tag_recursive(
    Collection *root_collection, ID *target_id, const bool do_parents)
{
  root_collection->id.tag |= LIB_TAG_DOIT;

  /* Tag all local parents of the root collection, so that usages of the root collection and other
   * linked ones can be replaced by the local overrides in those parents too. */
  if (do_parents) {
    for (CollectionParent *iter = static_cast<CollectionParent *>(root_collection->parents.first);
         iter != nullptr;
         iter = iter->next) {
      if (ID_IS_LINKED(iter->collection)) {
        continue;
      }
      iter->collection->id.tag |= LIB_TAG_DOIT;
    }
  }

  for (CollectionChild *iter = static_cast<CollectionChild *>(root_collection->children.first);
       iter != nullptr;
       iter = iter->next) {
    if (iter->collection->id.lib != root_collection->id.lib && ID_IS_LINKED(root_collection)) {
      continue;
    }
    if (ID_IS_LINKED(iter->collection) && iter->collection->id.lib != target_id->lib) {
      continue;
    }
    if (GS(target_id->name) == ID_OB &&
        !BKE_collection_has_object_recursive(iter->collection, (Object *)target_id)) {
      continue;
    }
    if (GS(target_id->name) == ID_GR &&
        !BKE_collection_has_collection(iter->collection, (Collection *)target_id)) {
      continue;
    }
    template_id_liboverride_hierarchy_collections_tag_recursive(
        iter->collection, target_id, false);
  }
}

ID *ui_template_id_liboverride_hierarchy_make(
    bContext *C, Main *bmain, ID *owner_id, ID *id, const char **r_undo_push_label)
{
  const char *undo_push_label;
  if (r_undo_push_label == nullptr) {
    r_undo_push_label = &undo_push_label;
  }

  /* If this is called on an already local override, 'toggle' between user-editable state, and
   * system override with reset. */
  if (!ID_IS_LINKED(id) && ID_IS_OVERRIDE_LIBRARY(id)) {
    if (!ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
      BKE_lib_override_library_get(bmain, id, nullptr, &id);
    }
    if (id->override_library->flag & IDOVERRIDE_LIBRARY_FLAG_SYSTEM_DEFINED) {
      id->override_library->flag &= ~IDOVERRIDE_LIBRARY_FLAG_SYSTEM_DEFINED;
      *r_undo_push_label = "Make Library Override Hierarchy Editable";
    }
    else {
      BKE_lib_override_library_id_reset(bmain, id, true);
      *r_undo_push_label = "Clear Library Override Hierarchy";
    }

    WM_event_add_notifier(C, NC_WM | ND_DATACHANGED, nullptr);
    WM_event_add_notifier(C, NC_WM | ND_LIB_OVERRIDE_CHANGED, nullptr);
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
    return id;
  }

  /* Attempt to perform a hierarchy override, based on contextual data available.
   * NOTE: do not attempt to perform such hierarchy override at all cost, if there is not enough
   * context, better to abort than create random overrides all over the place. */
  if (!ID_IS_OVERRIDABLE_LIBRARY_HIERARCHY(id)) {
    WM_reportf(RPT_ERROR, "The data-block %s is not overridable", id->name);
    return nullptr;
  }

  Object *object_active = CTX_data_active_object(C);
  if (object_active == nullptr && GS(owner_id->name) == ID_OB) {
    object_active = (Object *)owner_id;
  }
  if (object_active != nullptr) {
    if (ID_IS_LINKED(object_active)) {
      if (object_active->id.lib != id->lib ||
          !ID_IS_OVERRIDABLE_LIBRARY_HIERARCHY(object_active)) {
        /* The active object is from a different library than the overridden ID, or otherwise
         * cannot be used in hierarchy. */
        object_active = nullptr;
      }
    }
    else if (!ID_IS_OVERRIDE_LIBRARY_REAL(object_active)) {
      /* Fully local object cannot be used in override hierarchy either. */
      object_active = nullptr;
    }
  }

  Collection *collection_active = CTX_data_collection(C);
  if (collection_active == nullptr && GS(owner_id->name) == ID_GR) {
    collection_active = (Collection *)owner_id;
  }
  if (collection_active != nullptr) {
    if (ID_IS_LINKED(collection_active)) {
      if (collection_active->id.lib != id->lib ||
          !ID_IS_OVERRIDABLE_LIBRARY_HIERARCHY(collection_active)) {
        /* The active collection is from a different library than the overridden ID, or otherwise
         * cannot be used in hierarchy. */
        collection_active = nullptr;
      }
      else {
        int parent_level_best = -1;
        Collection *collection_parent_best = nullptr;
        template_id_liboverride_hierarchy_collection_root_find_recursive(
            collection_active, 0, &collection_parent_best, &parent_level_best);
        collection_active = collection_parent_best;
      }
    }
    else if (!ID_IS_OVERRIDE_LIBRARY_REAL(collection_active)) {
      /* Fully local collection cannot be used in override hierarchy either. */
      collection_active = nullptr;
    }
  }
  if (collection_active == nullptr && object_active != nullptr &&
      (ID_IS_LINKED(object_active) || ID_IS_OVERRIDE_LIBRARY_REAL(object_active))) {
    /* If we failed to find a valid 'active' collection so far for our override hierarchy, but do
     * have a valid 'active' object, try to find a collection from that object. */
    LISTBASE_FOREACH (Collection *, collection_iter, &bmain->collections) {
      if (!(ID_IS_LINKED(collection_iter) || ID_IS_OVERRIDE_LIBRARY_REAL(collection_iter))) {
        continue;
      }
      if (!BKE_collection_has_object_recursive(collection_iter, object_active)) {
        continue;
      }
      int parent_level_best = -1;
      Collection *collection_parent_best = nullptr;
      template_id_liboverride_hierarchy_collection_root_find_recursive(
          collection_iter, 0, &collection_parent_best, &parent_level_best);
      collection_active = collection_parent_best;
      break;
    }
  }

  ID *id_override = nullptr;
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  switch (GS(id->name)) {
    case ID_GR:
      if (collection_active != nullptr &&
          BKE_collection_has_collection(collection_active, (Collection *)id)) {
        template_id_liboverride_hierarchy_collections_tag_recursive(collection_active, id, true);
        if (object_active != nullptr) {
          object_active->id.tag |= LIB_TAG_DOIT;
        }
        BKE_lib_override_library_create(bmain,
                                        scene,
                                        view_layer,
                                        nullptr,
                                        id,
                                        &collection_active->id,
                                        nullptr,
                                        &id_override,
                                        false);
      }
      else if (object_active != nullptr && !ID_IS_LINKED(object_active) &&
               &object_active->instance_collection->id == id) {
        object_active->id.tag |= LIB_TAG_DOIT;
        BKE_lib_override_library_create(bmain,
                                        scene,
                                        view_layer,
                                        id->lib,
                                        id,
                                        &object_active->id,
                                        &object_active->id,
                                        &id_override,
                                        false);
      }
      break;
    case ID_OB:
      if (collection_active != nullptr &&
          BKE_collection_has_object_recursive(collection_active, (Object *)id)) {
        template_id_liboverride_hierarchy_collections_tag_recursive(collection_active, id, true);
        if (object_active != nullptr) {
          object_active->id.tag |= LIB_TAG_DOIT;
        }
        BKE_lib_override_library_create(bmain,
                                        scene,
                                        view_layer,
                                        nullptr,
                                        id,
                                        &collection_active->id,
                                        nullptr,
                                        &id_override,
                                        false);
      }
      else {
        if (object_active != nullptr) {
          object_active->id.tag |= LIB_TAG_DOIT;
        }
        BKE_lib_override_library_create(
            bmain, scene, view_layer, nullptr, id, nullptr, nullptr, &id_override, false);
        BKE_scene_collections_object_remove(bmain, scene, (Object *)id, true);
        WM_event_add_notifier(C, NC_ID | NA_REMOVED, nullptr);
      }
      break;
    case ID_ME:
    case ID_CU_LEGACY:
    case ID_MB:
    case ID_LT:
    case ID_LA:
    case ID_CA:
    case ID_SPK:
    case ID_AR:
    case ID_GD:
    case ID_CV:
    case ID_PT:
    case ID_VO:
    case ID_NT: /* Essentially geometry nodes from modifier currently. */
      if (object_active != nullptr) {
        if (collection_active != nullptr &&
            BKE_collection_has_object_recursive(collection_active, object_active)) {
          template_id_liboverride_hierarchy_collections_tag_recursive(collection_active, id, true);
          if (object_active != nullptr) {
            object_active->id.tag |= LIB_TAG_DOIT;
          }
          BKE_lib_override_library_create(bmain,
                                          scene,
                                          view_layer,
                                          nullptr,
                                          id,
                                          &collection_active->id,
                                          nullptr,
                                          &id_override,
                                          false);
        }
        else {
          object_active->id.tag |= LIB_TAG_DOIT;
          BKE_lib_override_library_create(bmain,
                                          scene,
                                          view_layer,
                                          nullptr,
                                          id,
                                          &object_active->id,
                                          nullptr,
                                          &id_override,
                                          false);
        }
      }
      else {
        BKE_lib_override_library_create(
            bmain, scene, view_layer, nullptr, id, id, nullptr, &id_override, false);
      }
      break;
    case ID_MA:
    case ID_TE:
    case ID_IM:
      WM_reportf(RPT_WARNING, "The type of data-block %s is not yet implemented", id->name);
      break;
    case ID_WO:
      WM_reportf(RPT_WARNING, "The type of data-block %s is not yet implemented", id->name);
      break;
    case ID_PA:
      WM_reportf(RPT_WARNING, "The type of data-block %s is not yet implemented", id->name);
      break;
    default:
      WM_reportf(RPT_WARNING, "The type of data-block %s is not yet implemented", id->name);
      break;
  }

  if (id_override != nullptr) {
    id_override->override_library->flag &= ~IDOVERRIDE_LIBRARY_FLAG_SYSTEM_DEFINED;
    *r_undo_push_label = "Make Library Override Hierarchy";

    /* In theory we could rely on setting/updating the RNA ID pointer property (as done by calling
     * code) to be enough.
     *
     * However, some rare ID pointers properties (like the 'active object in viewlayer' one used
     * for the Object templateID in the Object properties) use notifiers that do not enforce a
     * rebuild of outliner trees, leading to crashes.
     *
     * So for now, add some extra notifiers here. */
    WM_event_add_notifier(C, NC_ID | NA_ADDED, nullptr);
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, nullptr);
  }
  return id_override;
}

static void template_id_liboverride_hierarchy_make(bContext *C,
                                                   Main *bmain,
                                                   TemplateID *template_ui,
                                                   PointerRNA *idptr,
                                                   const char **r_undo_push_label)
{
  ID *id = static_cast<ID *>(idptr->data);
  ID *owner_id = template_ui->ptr.owner_id;

  ID *id_override = ui_template_id_liboverride_hierarchy_make(
      C, bmain, owner_id, id, r_undo_push_label);

  if (id_override != nullptr) {
    /* `idptr` is re-assigned to owner property to ensure proper updates etc. Here we also use it
     * to ensure remapping of the owner property from the linked data to the newly created
     * liboverride (note that in theory this remapping has already been done by code above), but
     * only in case owner ID was already local ID (override or pure local data).
     *
     * Otherwise, owner ID will also have been overridden, and remapped already to use it's
     * override of the data too. */
    if (!ID_IS_LINKED(owner_id)) {
      RNA_id_pointer_create(id_override, idptr);
    }
  }
  else {
    WM_reportf(RPT_ERROR, "The data-block %s could not be overridden", id->name);
  }
}

static void template_id_cb(bContext *C, void *arg_litem, void *arg_event)
{
  TemplateID *template_ui = (TemplateID *)arg_litem;
  PointerRNA idptr = RNA_property_pointer_get(&template_ui->ptr, template_ui->prop);
  ID *id = static_cast<ID *>(idptr.data);
  const int event = POINTER_AS_INT(arg_event);
  const char *undo_push_label = nullptr;

  switch (event) {
    case UI_ID_NOP:
      /* Don't do anything, typically set for buttons that execute an operator instead. They may
       * still assign the callback so the button can be identified as part of an ID-template. See
       * #UI_context_active_but_prop_get_templateID(). */
      break;
    case UI_ID_BROWSE:
    case UI_ID_PIN:
      RNA_warning("warning, id event %d shouldn't come here", event);
      break;
    case UI_ID_OPEN:
    case UI_ID_ADD_NEW:
      /* these call UI_context_active_but_prop_get_templateID */
      break;
    case UI_ID_DELETE:
      memset(&idptr, 0, sizeof(idptr));
      RNA_property_pointer_set(&template_ui->ptr, template_ui->prop, idptr, nullptr);
      RNA_property_update(C, &template_ui->ptr, template_ui->prop);

      if (id && CTX_wm_window(C)->eventstate->modifier & KM_SHIFT) {
        /* only way to force-remove data (on save) */
        id_us_clear_real(id);
        id_fake_user_clear(id);
        id->us = 0;
        undo_push_label = "Delete Data-Block";
      }

      break;
    case UI_ID_FAKE_USER:
      if (id) {
        if (id->flag & LIB_FAKEUSER) {
          id_us_plus(id);
        }
        else {
          id_us_min(id);
        }
        undo_push_label = "Fake User";
      }
      else {
        return;
      }
      break;
    case UI_ID_LOCAL:
      if (id) {
        Main *bmain = CTX_data_main(C);
        if (CTX_wm_window(C)->eventstate->modifier & KM_SHIFT) {
          template_id_liboverride_hierarchy_make(C, bmain, template_ui, &idptr, &undo_push_label);
        }
        else {
          if (BKE_lib_id_make_local(bmain, id, 0)) {
            BKE_id_newptr_and_tag_clear(id);

            /* Reassign to get proper updates/notifiers. */
            idptr = RNA_property_pointer_get(&template_ui->ptr, template_ui->prop);
            undo_push_label = "Make Local";
          }
        }
        if (undo_push_label != nullptr) {
          RNA_property_pointer_set(&template_ui->ptr, template_ui->prop, idptr, nullptr);
          RNA_property_update(C, &template_ui->ptr, template_ui->prop);
        }
      }
      break;
    case UI_ID_OVERRIDE:
      if (id && ID_IS_OVERRIDE_LIBRARY(id)) {
        Main *bmain = CTX_data_main(C);
        if (CTX_wm_window(C)->eventstate->modifier & KM_SHIFT) {
          template_id_liboverride_hierarchy_make(C, bmain, template_ui, &idptr, &undo_push_label);
        }
        else {
          BKE_lib_override_library_make_local(id);
          /* Reassign to get proper updates/notifiers. */
          idptr = RNA_property_pointer_get(&template_ui->ptr, template_ui->prop);
          RNA_property_pointer_set(&template_ui->ptr, template_ui->prop, idptr, nullptr);
          RNA_property_update(C, &template_ui->ptr, template_ui->prop);
          undo_push_label = "Make Local";
        }
      }
      break;
    case UI_ID_ALONE:
      if (id) {
        const bool do_scene_obj = ((GS(id->name) == ID_OB) &&
                                   (template_ui->ptr.type == &RNA_LayerObjects));

        /* make copy */
        if (do_scene_obj) {
          Main *bmain = CTX_data_main(C);
          Scene *scene = CTX_data_scene(C);
          ED_object_single_user(bmain, scene, (Object *)id);
          WM_event_add_notifier(C, NC_WINDOW, nullptr);
          DEG_relations_tag_update(bmain);
        }
        else {
          Main *bmain = CTX_data_main(C);
          id_single_user(C, id, &template_ui->ptr, template_ui->prop);
          DEG_relations_tag_update(bmain);
        }
        undo_push_label = "Make Single User";
      }
      break;
#if 0
    case UI_ID_AUTO_NAME:
      break;
#endif
  }

  if (undo_push_label != nullptr) {
    ED_undo_push(C, undo_push_label);
  }
}

static const char *template_id_browse_tip(const StructRNA *type)
{
  if (type) {
    switch ((ID_Type)RNA_type_to_ID_code(type)) {
      case ID_SCE:
        return N_("Browse Scene to be linked");
      case ID_OB:
        return N_("Browse Object to be linked");
      case ID_ME:
        return N_("Browse Mesh Data to be linked");
      case ID_CU_LEGACY:
        return N_("Browse Curve Data to be linked");
      case ID_MB:
        return N_("Browse Metaball Data to be linked");
      case ID_MA:
        return N_("Browse Material to be linked");
      case ID_TE:
        return N_("Browse Texture to be linked");
      case ID_IM:
        return N_("Browse Image to be linked");
      case ID_LS:
        return N_("Browse Line Style Data to be linked");
      case ID_LT:
        return N_("Browse Lattice Data to be linked");
      case ID_LA:
        return N_("Browse Light Data to be linked");
      case ID_CA:
        return N_("Browse Camera Data to be linked");
      case ID_WO:
        return N_("Browse World Settings to be linked");
      case ID_SCR:
        return N_("Choose Screen layout");
      case ID_TXT:
        return N_("Browse Text to be linked");
      case ID_SPK:
        return N_("Browse Speaker Data to be linked");
      case ID_SO:
        return N_("Browse Sound to be linked");
      case ID_AR:
        return N_("Browse Armature data to be linked");
      case ID_AC:
        return N_("Browse Action to be linked");
      case ID_NT:
        return N_("Browse Node Tree to be linked");
      case ID_BR:
        return N_("Browse Brush to be linked");
      case ID_PA:
        return N_("Browse Particle Settings to be linked");
      case ID_GD:
        return N_("Browse Grease Pencil Data to be linked");
      case ID_MC:
        return N_("Browse Movie Clip to be linked");
      case ID_MSK:
        return N_("Browse Mask to be linked");
      case ID_PAL:
        return N_("Browse Palette Data to be linked");
      case ID_PC:
        return N_("Browse Paint Curve Data to be linked");
      case ID_CF:
        return N_("Browse Cache Files to be linked");
      case ID_WS:
        return N_("Browse Workspace to be linked");
      case ID_LP:
        return N_("Browse LightProbe to be linked");
      case ID_CV:
        return N_("Browse Curves Data to be linked");
      case ID_PT:
        return N_("Browse Point Cloud Data to be linked");
      case ID_VO:
        return N_("Browse Volume Data to be linked");
      case ID_SIM:
        return N_("Browse Simulation to be linked");

      /* Use generic text. */
      case ID_LI:
      case ID_IP:
      case ID_KE:
      case ID_VF:
      case ID_GR:
      case ID_WM:
        break;
    }
  }
  return N_("Browse ID data to be linked");
}

/**
 * Add a superimposed extra icon to \a but, for workspace pinning.
 * Rather ugly special handling, but this is really a special case at this point, nothing worth
 * generalizing.
 */
static void template_id_workspace_pin_extra_icon(const TemplateID *template_ui, uiBut *but)
{
  if ((template_ui->idcode != ID_SCE) || (template_ui->ptr.type != &RNA_Window)) {
    return;
  }

  const wmWindow *win = static_cast<const wmWindow *>(template_ui->ptr.data);
  const WorkSpace *workspace = WM_window_get_active_workspace(win);
  UI_but_extra_operator_icon_add(but,
                                 "WORKSPACE_OT_scene_pin_toggle",
                                 WM_OP_INVOKE_DEFAULT,
                                 (workspace->flags & WORKSPACE_USE_PIN_SCENE) ? ICON_PINNED :
                                                                                ICON_UNPINNED);
}

/**
 * \return a type-based i18n context, needed e.g. by "New" button.
 * In most languages, this adjective takes different form based on gender of type name...
 */
#ifdef WITH_INTERNATIONAL
static const char *template_id_context(StructRNA *type)
{
  if (type) {
    return BKE_idtype_idcode_to_translation_context(RNA_type_to_ID_code(type));
  }
  return BLT_I18NCONTEXT_DEFAULT;
}
#else
#  define template_id_context(type) 0
#endif

static uiBut *template_id_def_new_but(uiBlock *block,
                                      const ID *id,
                                      const TemplateID *template_ui,
                                      StructRNA *type,
                                      const char *const newop,
                                      const bool editable,
                                      const bool id_open,
                                      const bool use_tab_but,
                                      int but_height)
{
  ID *idfrom = template_ui->ptr.owner_id;
  uiBut *but;
  const int w = id ? UI_UNIT_X : id_open ? UI_UNIT_X * 3 : UI_UNIT_X * 6;
  const int but_type = use_tab_but ? UI_BTYPE_TAB : UI_BTYPE_BUT;

  /* i18n markup, does nothing! */
  BLT_I18N_MSGID_MULTI_CTXT("New",
                            BLT_I18NCONTEXT_DEFAULT,
                            BLT_I18NCONTEXT_ID_SCENE,
                            BLT_I18NCONTEXT_ID_OBJECT,
                            BLT_I18NCONTEXT_ID_MESH,
                            BLT_I18NCONTEXT_ID_CURVE_LEGACY,
                            BLT_I18NCONTEXT_ID_METABALL,
                            BLT_I18NCONTEXT_ID_MATERIAL,
                            BLT_I18NCONTEXT_ID_TEXTURE,
                            BLT_I18NCONTEXT_ID_IMAGE,
                            BLT_I18NCONTEXT_ID_LATTICE,
                            BLT_I18NCONTEXT_ID_LIGHT,
                            BLT_I18NCONTEXT_ID_CAMERA,
                            BLT_I18NCONTEXT_ID_WORLD,
                            BLT_I18NCONTEXT_ID_SCREEN,
                            BLT_I18NCONTEXT_ID_TEXT, );
  BLT_I18N_MSGID_MULTI_CTXT("New",
                            BLT_I18NCONTEXT_ID_SPEAKER,
                            BLT_I18NCONTEXT_ID_SOUND,
                            BLT_I18NCONTEXT_ID_ARMATURE,
                            BLT_I18NCONTEXT_ID_ACTION,
                            BLT_I18NCONTEXT_ID_NODETREE,
                            BLT_I18NCONTEXT_ID_BRUSH,
                            BLT_I18NCONTEXT_ID_PARTICLESETTINGS,
                            BLT_I18NCONTEXT_ID_GPENCIL,
                            BLT_I18NCONTEXT_ID_FREESTYLELINESTYLE,
                            BLT_I18NCONTEXT_ID_WORKSPACE,
                            BLT_I18NCONTEXT_ID_LIGHTPROBE,
                            BLT_I18NCONTEXT_ID_CURVES,
                            BLT_I18NCONTEXT_ID_POINTCLOUD,
                            BLT_I18NCONTEXT_ID_VOLUME,
                            BLT_I18NCONTEXT_ID_SIMULATION, );
  /* NOTE: BLT_I18N_MSGID_MULTI_CTXT takes a maximum number of parameters,
   * check the definition to see if a new call must be added when the limit
   * is exceeded. */

  if (newop) {
    but = uiDefIconTextButO(block,
                            but_type,
                            newop,
                            WM_OP_INVOKE_DEFAULT,
                            (id && !use_tab_but) ? ICON_DUPLICATE : ICON_ADD,
                            (id) ? "" : CTX_IFACE_(template_id_context(type), "New"),
                            0,
                            0,
                            w,
                            but_height,
                            nullptr);
    UI_but_funcN_set(
        but, template_id_cb, MEM_dupallocN(template_ui), POINTER_FROM_INT(UI_ID_ADD_NEW));
  }
  else {
    but = uiDefIconTextBut(block,
                           but_type,
                           0,
                           (id && !use_tab_but) ? ICON_DUPLICATE : ICON_ADD,
                           (id) ? "" : CTX_IFACE_(template_id_context(type), "New"),
                           0,
                           0,
                           w,
                           but_height,
                           nullptr,
                           0,
                           0,
                           0,
                           0,
                           nullptr);
    UI_but_funcN_set(
        but, template_id_cb, MEM_dupallocN(template_ui), POINTER_FROM_INT(UI_ID_ADD_NEW));
  }

  if ((idfrom && idfrom->lib) || !editable) {
    UI_but_flag_enable(but, UI_BUT_DISABLED);
  }

#ifndef WITH_INTERNATIONAL
  UNUSED_VARS(type);
#endif

  return but;
}

static void template_ID(const bContext *C,
                        uiLayout *layout,
                        TemplateID *template_ui,
                        StructRNA *type,
                        int flag,
                        const char *newop,
                        const char *openop,
                        const char *unlinkop,
                        const char *text,
                        const bool live_icon,
                        const bool hide_buttons)
{
  uiBut *but;
  const bool editable = RNA_property_editable(&template_ui->ptr, template_ui->prop);
  const bool use_previews = template_ui->preview = (flag & UI_ID_PREVIEWS) != 0;

  PointerRNA idptr = RNA_property_pointer_get(&template_ui->ptr, template_ui->prop);
  ID *id = static_cast<ID *>(idptr.data);
  ID *idfrom = template_ui->ptr.owner_id;
  // lb = template_ui->idlb;

  /* Allow operators to take the ID from context. */
  uiLayoutSetContextPointer(layout, "id", &idptr);

  uiBlock *block = uiLayoutGetBlock(layout);
  UI_block_align_begin(block);

  if (idptr.type) {
    type = idptr.type;
  }

  if (text) {
    /* Add label respecting the separated layout property split state. */
    uiItemL_respect_property_split(layout, text, ICON_NONE);
  }

  if (flag & UI_ID_BROWSE) {
    template_add_button_search_menu(C,
                                    layout,
                                    block,
                                    &template_ui->ptr,
                                    template_ui->prop,
                                    id_search_menu,
                                    MEM_dupallocN(template_ui),
                                    TIP_(template_id_browse_tip(type)),
                                    use_previews,
                                    editable,
                                    live_icon);
  }

  /* text button with name */
  if (id) {
    char name[UI_MAX_NAME_STR];
    const bool user_alert = (id->us <= 0);

    const int width = template_search_textbut_width(&idptr,
                                                    RNA_struct_find_property(&idptr, "name"));
    const int height = template_search_textbut_height();

    // text_idbutton(id, name);
    name[0] = '\0';
    but = uiDefButR(block,
                    UI_BTYPE_TEXT,
                    0,
                    name,
                    0,
                    0,
                    width,
                    height,
                    &idptr,
                    "name",
                    -1,
                    0,
                    0,
                    -1,
                    -1,
                    RNA_struct_ui_description(type));
    UI_but_funcN_set(
        but, template_id_cb, MEM_dupallocN(template_ui), POINTER_FROM_INT(UI_ID_RENAME));
    if (user_alert) {
      UI_but_flag_enable(but, UI_BUT_REDALERT);
    }

    template_id_workspace_pin_extra_icon(template_ui, but);

    if (ID_IS_LINKED(id)) {
      const bool disabled = !BKE_idtype_idcode_is_localizable(GS(id->name));
      if (id->tag & LIB_TAG_INDIRECT) {
        but = uiDefIconBut(block,
                           UI_BTYPE_BUT,
                           0,
                           ICON_LIBRARY_DATA_INDIRECT,
                           0,
                           0,
                           UI_UNIT_X,
                           UI_UNIT_Y,
                           nullptr,
                           0,
                           0,
                           0,
                           0,
                           TIP_("Indirect library data-block, cannot be made local, "
                                "Shift + Click to create a library override hierarchy"));
      }
      else {
        but = uiDefIconBut(block,
                           UI_BTYPE_BUT,
                           0,
                           ICON_LIBRARY_DATA_DIRECT,
                           0,
                           0,
                           UI_UNIT_X,
                           UI_UNIT_Y,
                           nullptr,
                           0,
                           0,
                           0,
                           0,
                           TIP_("Direct linked library data-block, click to make local, "
                                "Shift + Click to create a library override"));
      }
      if (disabled) {
        UI_but_flag_enable(but, UI_BUT_DISABLED);
      }
      else {
        UI_but_funcN_set(
            but, template_id_cb, MEM_dupallocN(template_ui), POINTER_FROM_INT(UI_ID_LOCAL));
      }
    }
    else if (ID_IS_OVERRIDE_LIBRARY(id)) {
      but = uiDefIconBut(
          block,
          UI_BTYPE_BUT,
          0,
          ICON_LIBRARY_DATA_OVERRIDE,
          0,
          0,
          UI_UNIT_X,
          UI_UNIT_Y,
          nullptr,
          0,
          0,
          0,
          0,
          TIP_("Library override of linked data-block, click to make fully local, "
               "Shift + Click to clear the library override and toggle if it can be edited"));
      UI_but_funcN_set(
          but, template_id_cb, MEM_dupallocN(template_ui), POINTER_FROM_INT(UI_ID_OVERRIDE));
    }

    if ((ID_REAL_USERS(id) > 1) && (hide_buttons == false)) {
      char numstr[32];
      short numstr_len;

      numstr_len = BLI_snprintf_rlen(numstr, sizeof(numstr), "%d", ID_REAL_USERS(id));

      but = uiDefBut(
          block,
          UI_BTYPE_BUT,
          0,
          numstr,
          0,
          0,
          numstr_len * 0.2f * UI_UNIT_X + UI_UNIT_X,
          UI_UNIT_Y,
          nullptr,
          0,
          0,
          0,
          0,
          TIP_("Display number of users of this data (click to make a single-user copy)"));
      but->flag |= UI_BUT_UNDO;

      UI_but_funcN_set(
          but, template_id_cb, MEM_dupallocN(template_ui), POINTER_FROM_INT(UI_ID_ALONE));
      if (!BKE_id_copy_is_allowed(id) || (idfrom && idfrom->lib) || (!editable) ||
          /* object in editmode - don't change data */
          (idfrom && GS(idfrom->name) == ID_OB && (((Object *)idfrom)->mode & OB_MODE_EDIT))) {
        UI_but_flag_enable(but, UI_BUT_DISABLED);
      }
    }

    if (user_alert) {
      UI_but_flag_enable(but, UI_BUT_REDALERT);
    }

    if (!ID_IS_LINKED(id)) {
      if (ID_IS_ASSET(id)) {
        uiDefIconButO(block,
                      /* Using `_N` version allows us to get the 'active' state by default. */
                      UI_BTYPE_ICON_TOGGLE_N,
                      "ASSET_OT_clear",
                      WM_OP_INVOKE_DEFAULT,
                      /* 'active' state of a toggle button uses icon + 1, so to get proper asset
                       * icon we need to pass its value - 1 here. */
                      ICON_ASSET_MANAGER - 1,
                      0,
                      0,
                      UI_UNIT_X,
                      UI_UNIT_Y,
                      nullptr);
      }
      else if (!ELEM(GS(id->name), ID_GR, ID_SCE, ID_SCR, ID_OB, ID_WS) &&
               (hide_buttons == false)) {
        uiDefIconButR(block,
                      UI_BTYPE_ICON_TOGGLE,
                      0,
                      ICON_FAKE_USER_OFF,
                      0,
                      0,
                      UI_UNIT_X,
                      UI_UNIT_Y,
                      &idptr,
                      "use_fake_user",
                      -1,
                      0,
                      0,
                      -1,
                      -1,
                      nullptr);
      }
    }
  }

  if ((flag & UI_ID_ADD_NEW) && (hide_buttons == false)) {
    template_id_def_new_but(
        block, id, template_ui, type, newop, editable, flag & UI_ID_OPEN, false, UI_UNIT_X);
  }

  /* Due to space limit in UI - skip the "open" icon for packed data, and allow to unpack.
   * Only for images, sound and fonts */
  if (id && BKE_packedfile_id_check(id)) {
    but = uiDefIconButO(block,
                        UI_BTYPE_BUT,
                        "FILE_OT_unpack_item",
                        WM_OP_INVOKE_REGION_WIN,
                        ICON_PACKAGE,
                        0,
                        0,
                        UI_UNIT_X,
                        UI_UNIT_Y,
                        TIP_("Packed File, click to unpack"));
    UI_but_operator_ptr_get(but);

    RNA_string_set(but->opptr, "id_name", id->name + 2);
    RNA_int_set(but->opptr, "id_type", GS(id->name));
  }
  else if (flag & UI_ID_OPEN) {
    const int w = id ? UI_UNIT_X : (flag & UI_ID_ADD_NEW) ? UI_UNIT_X * 3 : UI_UNIT_X * 6;

    if (openop) {
      but = uiDefIconTextButO(block,
                              UI_BTYPE_BUT,
                              openop,
                              WM_OP_INVOKE_DEFAULT,
                              ICON_FILEBROWSER,
                              (id) ? "" : IFACE_("Open"),
                              0,
                              0,
                              w,
                              UI_UNIT_Y,
                              nullptr);
      UI_but_funcN_set(
          but, template_id_cb, MEM_dupallocN(template_ui), POINTER_FROM_INT(UI_ID_OPEN));
    }
    else {
      but = uiDefIconTextBut(block,
                             UI_BTYPE_BUT,
                             0,
                             ICON_FILEBROWSER,
                             (id) ? "" : IFACE_("Open"),
                             0,
                             0,
                             w,
                             UI_UNIT_Y,
                             nullptr,
                             0,
                             0,
                             0,
                             0,
                             nullptr);
      UI_but_funcN_set(
          but, template_id_cb, MEM_dupallocN(template_ui), POINTER_FROM_INT(UI_ID_OPEN));
    }

    if ((idfrom && idfrom->lib) || !editable) {
      UI_but_flag_enable(but, UI_BUT_DISABLED);
    }
  }

  /* delete button */
  /* don't use RNA_property_is_unlink here */
  if (id && (flag & UI_ID_DELETE) && (hide_buttons == false)) {
    /* allow unlink if 'unlinkop' is passed, even when 'PROP_NEVER_UNLINK' is set */
    but = nullptr;

    if (unlinkop) {
      but = uiDefIconButO(block,
                          UI_BTYPE_BUT,
                          unlinkop,
                          WM_OP_INVOKE_DEFAULT,
                          ICON_X,
                          0,
                          0,
                          UI_UNIT_X,
                          UI_UNIT_Y,
                          nullptr);
      /* so we can access the template from operators, font unlinking needs this */
      UI_but_funcN_set(
          but, template_id_cb, MEM_dupallocN(template_ui), POINTER_FROM_INT(UI_ID_NOP));
    }
    else {
      if ((RNA_property_flag(template_ui->prop) & PROP_NEVER_UNLINK) == 0) {
        but = uiDefIconBut(
            block,
            UI_BTYPE_BUT,
            0,
            ICON_X,
            0,
            0,
            UI_UNIT_X,
            UI_UNIT_Y,
            nullptr,
            0,
            0,
            0,
            0,
            TIP_("Unlink data-block "
                 "(Shift + Click to set users to zero, data will then not be saved)"));
        UI_but_funcN_set(
            but, template_id_cb, MEM_dupallocN(template_ui), POINTER_FROM_INT(UI_ID_DELETE));

        if (RNA_property_flag(template_ui->prop) & PROP_NEVER_NULL) {
          UI_but_flag_enable(but, UI_BUT_DISABLED);
        }
      }
    }

    if (but) {
      if ((idfrom && idfrom->lib) || !editable) {
        UI_but_flag_enable(but, UI_BUT_DISABLED);
      }
    }
  }

  if (template_ui->idcode == ID_TE) {
    uiTemplateTextureShow(layout, C, &template_ui->ptr, template_ui->prop);
  }
  UI_block_align_end(block);
}

ID *UI_context_active_but_get_tab_ID(bContext *C)
{
  uiBut *but = UI_context_active_but_get(C);

  if (but && but->type == UI_BTYPE_TAB) {
    return static_cast<ID *>(but->custom_data);
  }
  return nullptr;
}

static void template_ID_tabs(const bContext *C,
                             uiLayout *layout,
                             TemplateID *template_id,
                             StructRNA *type,
                             int flag,
                             const char *newop,
                             const char *menu)
{
  const ARegion *region = CTX_wm_region(C);
  const PointerRNA active_ptr = RNA_property_pointer_get(&template_id->ptr, template_id->prop);
  MenuType *mt = menu ? WM_menutype_find(menu, false) : nullptr;

  const int but_align = ui_but_align_opposite_to_area_align_get(region);
  const int but_height = UI_UNIT_Y * 1.1;

  uiBlock *block = uiLayoutGetBlock(layout);
  const uiStyle *style = UI_style_get_dpi();

  ListBase ordered;
  BKE_id_ordered_list(&ordered, template_id->idlb);

  LISTBASE_FOREACH (LinkData *, link, &ordered) {
    ID *id = static_cast<ID *>(link->data);
    const int name_width = UI_fontstyle_string_width(&style->widget, id->name + 2);
    const int but_width = name_width + UI_UNIT_X;

    uiButTab *tab = (uiButTab *)uiDefButR_prop(block,
                                               UI_BTYPE_TAB,
                                               0,
                                               id->name + 2,
                                               0,
                                               0,
                                               but_width,
                                               but_height,
                                               &template_id->ptr,
                                               template_id->prop,
                                               0,
                                               0.0f,
                                               sizeof(id->name) - 2,
                                               0.0f,
                                               0.0f,
                                               "");
    UI_but_funcN_set(&tab->but, template_ID_set_property_exec_fn, MEM_dupallocN(template_id), id);
    UI_but_drag_set_id(&tab->but, id);
    tab->but.custom_data = (void *)id;
    tab->menu = mt;

    UI_but_drawflag_enable(&tab->but, but_align);
  }

  BLI_freelistN(&ordered);

  if (flag & UI_ID_ADD_NEW) {
    const bool editable = RNA_property_editable(&template_id->ptr, template_id->prop);
    uiBut *but;

    if (active_ptr.type) {
      type = active_ptr.type;
    }

    but = template_id_def_new_but(block,
                                  static_cast<const ID *>(active_ptr.data),
                                  template_id,
                                  type,
                                  newop,
                                  editable,
                                  flag & UI_ID_OPEN,
                                  true,
                                  but_height);
    UI_but_drawflag_enable(but, but_align);
  }
}

static void ui_template_id(uiLayout *layout,
                           const bContext *C,
                           PointerRNA *ptr,
                           const char *propname,
                           const char *newop,
                           const char *openop,
                           const char *unlinkop,
                           /* Only respected by tabs (use_tabs). */
                           const char *menu,
                           const char *text,
                           int flag,
                           int prv_rows,
                           int prv_cols,
                           int filter,
                           bool use_tabs,
                           float scale,
                           const bool live_icon,
                           const bool hide_buttons)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop || RNA_property_type(prop) != PROP_POINTER) {
    RNA_warning("pointer property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  TemplateID *template_ui = MEM_cnew<TemplateID>(__func__);
  template_ui->ptr = *ptr;
  template_ui->prop = prop;
  template_ui->prv_rows = prv_rows;
  template_ui->prv_cols = prv_cols;
  template_ui->scale = scale;

  if ((flag & UI_ID_PIN) == 0) {
    template_ui->filter = filter;
  }
  else {
    template_ui->filter = 0;
  }

  if (newop) {
    flag |= UI_ID_ADD_NEW;
  }
  if (openop) {
    flag |= UI_ID_OPEN;
  }

  StructRNA *type = RNA_property_pointer_type(ptr, prop);
  short idcode = RNA_type_to_ID_code(type);
  template_ui->idcode = idcode;
  template_ui->idlb = which_libbase(CTX_data_main(C), idcode);

  /* create UI elements for this template
   * - template_ID makes a copy of the template data and assigns it to the relevant buttons
   */
  if (template_ui->idlb) {
    if (use_tabs) {
      layout = uiLayoutRow(layout, true);
      template_ID_tabs(C, layout, template_ui, type, flag, newop, menu);
    }
    else {
      layout = uiLayoutRow(layout, true);
      template_ID(C,
                  layout,
                  template_ui,
                  type,
                  flag,
                  newop,
                  openop,
                  unlinkop,
                  text,
                  live_icon,
                  hide_buttons);
    }
  }

  MEM_freeN(template_ui);
}

void uiTemplateID(uiLayout *layout,
                  const bContext *C,
                  PointerRNA *ptr,
                  const char *propname,
                  const char *newop,
                  const char *openop,
                  const char *unlinkop,
                  int filter,
                  const bool live_icon,
                  const char *text)
{
  ui_template_id(layout,
                 C,
                 ptr,
                 propname,
                 newop,
                 openop,
                 unlinkop,
                 nullptr,
                 text,
                 UI_ID_BROWSE | UI_ID_RENAME | UI_ID_DELETE,
                 0,
                 0,
                 filter,
                 false,
                 1.0f,
                 live_icon,
                 false);
}

void uiTemplateIDBrowse(uiLayout *layout,
                        bContext *C,
                        PointerRNA *ptr,
                        const char *propname,
                        const char *newop,
                        const char *openop,
                        const char *unlinkop,
                        int filter,
                        const char *text)
{
  ui_template_id(layout,
                 C,
                 ptr,
                 propname,
                 newop,
                 openop,
                 unlinkop,
                 nullptr,
                 text,
                 UI_ID_BROWSE | UI_ID_RENAME,
                 0,
                 0,
                 filter,
                 false,
                 1.0f,
                 false,
                 false);
}

void uiTemplateIDPreview(uiLayout *layout,
                         bContext *C,
                         PointerRNA *ptr,
                         const char *propname,
                         const char *newop,
                         const char *openop,
                         const char *unlinkop,
                         int rows,
                         int cols,
                         int filter,
                         const bool hide_buttons)
{
  ui_template_id(layout,
                 C,
                 ptr,
                 propname,
                 newop,
                 openop,
                 unlinkop,
                 nullptr,
                 nullptr,
                 UI_ID_BROWSE | UI_ID_RENAME | UI_ID_DELETE | UI_ID_PREVIEWS,
                 rows,
                 cols,
                 filter,
                 false,
                 1.0f,
                 false,
                 hide_buttons);
}

void uiTemplateGpencilColorPreview(uiLayout *layout,
                                   bContext *C,
                                   PointerRNA *ptr,
                                   const char *propname,
                                   int rows,
                                   int cols,
                                   float scale,
                                   int filter)
{
  ui_template_id(layout,
                 C,
                 ptr,
                 propname,
                 nullptr,
                 nullptr,
                 nullptr,
                 nullptr,
                 nullptr,
                 UI_ID_BROWSE | UI_ID_PREVIEWS | UI_ID_DELETE,
                 rows,
                 cols,
                 filter,
                 false,
                 scale < 0.5f ? 0.5f : scale,
                 false,
                 false);
}

void uiTemplateIDTabs(uiLayout *layout,
                      bContext *C,
                      PointerRNA *ptr,
                      const char *propname,
                      const char *newop,
                      const char *menu,
                      int filter)
{
  ui_template_id(layout,
                 C,
                 ptr,
                 propname,
                 newop,
                 nullptr,
                 nullptr,
                 menu,
                 nullptr,
                 UI_ID_BROWSE | UI_ID_RENAME,
                 0,
                 0,
                 filter,
                 true,
                 1.0f,
                 false,
                 false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ID Chooser Template
 * \{ */

void uiTemplateAnyID(uiLayout *layout,
                     PointerRNA *ptr,
                     const char *propname,
                     const char *proptypename,
                     const char *text)
{
  /* get properties... */
  PropertyRNA *propID = RNA_struct_find_property(ptr, propname);
  PropertyRNA *propType = RNA_struct_find_property(ptr, proptypename);

  if (!propID || RNA_property_type(propID) != PROP_POINTER) {
    RNA_warning("pointer property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }
  if (!propType || RNA_property_type(propType) != PROP_ENUM) {
    RNA_warning(
        "pointer-type property not found: %s.%s", RNA_struct_identifier(ptr->type), proptypename);
    return;
  }

  /* Start drawing UI Elements using standard defines */

  /* NOTE: split amount here needs to be synced with normal labels */
  uiLayout *split = uiLayoutSplit(layout, 0.33f, false);

  /* FIRST PART ................................................ */
  uiLayout *row = uiLayoutRow(split, false);

  /* Label - either use the provided text, or will become "ID-Block:" */
  if (text) {
    if (text[0]) {
      uiItemL(row, text, ICON_NONE);
    }
  }
  else {
    uiItemL(row, IFACE_("ID-Block:"), ICON_NONE);
  }

  /* SECOND PART ................................................ */
  row = uiLayoutRow(split, true);

  /* ID-Type Selector - just have a menu of icons */

  /* HACK: special group just for the enum,
   * otherwise we get ugly layout with text included too... */
  uiLayout *sub = uiLayoutRow(row, true);
  uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);

  uiItemFullR(sub, ptr, propType, 0, 0, UI_ITEM_R_ICON_ONLY, "", ICON_NONE);

  /* ID-Block Selector - just use pointer widget... */

  /* HACK: special group to counteract the effects of the previous enum,
   * which now pushes everything too far right. */
  sub = uiLayoutRow(row, true);
  uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_EXPAND);

  uiItemFullR(sub, ptr, propID, 0, 0, 0, "", ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Search Template
 * \{ */

struct TemplateSearch {
  uiRNACollectionSearch search_data;

  bool use_previews;
  int preview_rows, preview_cols;
};

static void template_search_exec_fn(bContext *C, void *arg_template, void *item)
{
  TemplateSearch *template_search = static_cast<TemplateSearch *>(arg_template);
  uiRNACollectionSearch *coll_search = &template_search->search_data;
  StructRNA *type = RNA_property_pointer_type(&coll_search->target_ptr, coll_search->target_prop);
  PointerRNA item_ptr;

  RNA_pointer_create(nullptr, type, item, &item_ptr);
  RNA_property_pointer_set(&coll_search->target_ptr, coll_search->target_prop, item_ptr, nullptr);
  RNA_property_update(C, &coll_search->target_ptr, coll_search->target_prop);
}

static uiBlock *template_search_menu(bContext *C, ARegion *region, void *arg_template)
{
  static TemplateSearch template_search;

  /* arg_template is malloced, can be freed by parent button */
  template_search = *((TemplateSearch *)arg_template);
  PointerRNA active_ptr = RNA_property_pointer_get(&template_search.search_data.target_ptr,
                                                   template_search.search_data.target_prop);

  return template_common_search_menu(C,
                                     region,
                                     ui_rna_collection_search_update_fn,
                                     &template_search,
                                     template_search_exec_fn,
                                     active_ptr.data,
                                     nullptr,
                                     template_search.preview_rows,
                                     template_search.preview_cols,
                                     1.0f);
}

static void template_search_add_button_searchmenu(const bContext *C,
                                                  uiLayout *layout,
                                                  uiBlock *block,
                                                  TemplateSearch *template_search,
                                                  const bool editable,
                                                  const bool live_icon)
{
  const char *ui_description = RNA_property_ui_description(
      template_search->search_data.target_prop);

  template_add_button_search_menu(C,
                                  layout,
                                  block,
                                  &template_search->search_data.target_ptr,
                                  template_search->search_data.target_prop,
                                  template_search_menu,
                                  MEM_dupallocN(template_search),
                                  ui_description,
                                  template_search->use_previews,
                                  editable,
                                  live_icon);
}

static void template_search_add_button_name(uiBlock *block,
                                            PointerRNA *active_ptr,
                                            const StructRNA *type)
{
  PropertyRNA *name_prop = RNA_struct_name_property(type);
  const int width = template_search_textbut_width(active_ptr, name_prop);
  const int height = template_search_textbut_height();
  uiDefAutoButR(block, active_ptr, name_prop, 0, "", ICON_NONE, 0, 0, width, height);
}

static void template_search_add_button_operator(uiBlock *block,
                                                const char *const operator_name,
                                                const wmOperatorCallContext opcontext,
                                                const int icon,
                                                const bool editable)
{
  if (!operator_name) {
    return;
  }

  uiBut *but = uiDefIconButO(
      block, UI_BTYPE_BUT, operator_name, opcontext, icon, 0, 0, UI_UNIT_X, UI_UNIT_Y, nullptr);

  if (!editable) {
    UI_but_drawflag_enable(but, UI_BUT_DISABLED);
  }
}

static void template_search_buttons(const bContext *C,
                                    uiLayout *layout,
                                    TemplateSearch *template_search,
                                    const char *newop,
                                    const char *unlinkop)
{
  uiBlock *block = uiLayoutGetBlock(layout);
  uiRNACollectionSearch *search_data = &template_search->search_data;
  StructRNA *type = RNA_property_pointer_type(&search_data->target_ptr, search_data->target_prop);
  const bool editable = RNA_property_editable(&search_data->target_ptr, search_data->target_prop);
  PointerRNA active_ptr = RNA_property_pointer_get(&search_data->target_ptr,
                                                   search_data->target_prop);

  if (active_ptr.type) {
    /* can only get correct type when there is an active item */
    type = active_ptr.type;
  }

  uiLayoutRow(layout, true);
  UI_block_align_begin(block);

  template_search_add_button_searchmenu(C, layout, block, template_search, editable, false);
  template_search_add_button_name(block, &active_ptr, type);
  template_search_add_button_operator(
      block, newop, WM_OP_INVOKE_DEFAULT, ICON_DUPLICATE, editable);
  template_search_add_button_operator(block, unlinkop, WM_OP_INVOKE_REGION_WIN, ICON_X, editable);

  UI_block_align_end(block);
}

static PropertyRNA *template_search_get_searchprop(PointerRNA *targetptr,
                                                   PropertyRNA *targetprop,
                                                   PointerRNA *searchptr,
                                                   const char *const searchpropname)
{
  PropertyRNA *searchprop;

  if (searchptr && !searchptr->data) {
    searchptr = nullptr;
  }

  if (!searchptr && !searchpropname) {
    /* both nullptr means we don't use a custom rna collection to search in */
  }
  else if (!searchptr && searchpropname) {
    RNA_warning("searchpropname defined (%s) but searchptr is missing", searchpropname);
  }
  else if (searchptr && !searchpropname) {
    RNA_warning("searchptr defined (%s) but searchpropname is missing",
                RNA_struct_identifier(searchptr->type));
  }
  else if (!(searchprop = RNA_struct_find_property(searchptr, searchpropname))) {
    RNA_warning("search collection property not found: %s.%s",
                RNA_struct_identifier(searchptr->type),
                searchpropname);
  }
  else if (RNA_property_type(searchprop) != PROP_COLLECTION) {
    RNA_warning("search collection property is not a collection type: %s.%s",
                RNA_struct_identifier(searchptr->type),
                searchpropname);
  }
  /* check if searchprop has same type as targetprop */
  else if (RNA_property_pointer_type(searchptr, searchprop) !=
           RNA_property_pointer_type(targetptr, targetprop)) {
    RNA_warning("search collection items from %s.%s are not of type %s",
                RNA_struct_identifier(searchptr->type),
                searchpropname,
                RNA_struct_identifier(RNA_property_pointer_type(targetptr, targetprop)));
  }
  else {
    return searchprop;
  }

  return nullptr;
}

static TemplateSearch *template_search_setup(PointerRNA *ptr,
                                             const char *const propname,
                                             PointerRNA *searchptr,
                                             const char *const searchpropname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop || RNA_property_type(prop) != PROP_POINTER) {
    RNA_warning("pointer property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return nullptr;
  }
  PropertyRNA *searchprop = template_search_get_searchprop(ptr, prop, searchptr, searchpropname);

  TemplateSearch *template_search = MEM_cnew<TemplateSearch>(__func__);
  template_search->search_data.target_ptr = *ptr;
  template_search->search_data.target_prop = prop;
  template_search->search_data.search_ptr = *searchptr;
  template_search->search_data.search_prop = searchprop;

  return template_search;
}

void uiTemplateSearch(uiLayout *layout,
                      bContext *C,
                      PointerRNA *ptr,
                      const char *propname,
                      PointerRNA *searchptr,
                      const char *searchpropname,
                      const char *newop,
                      const char *unlinkop)
{
  TemplateSearch *template_search = template_search_setup(
      ptr, propname, searchptr, searchpropname);
  if (template_search != nullptr) {
    template_search_buttons(C, layout, template_search, newop, unlinkop);
    MEM_freeN(template_search);
  }
}

void uiTemplateSearchPreview(uiLayout *layout,
                             bContext *C,
                             PointerRNA *ptr,
                             const char *propname,
                             PointerRNA *searchptr,
                             const char *searchpropname,
                             const char *newop,
                             const char *unlinkop,
                             const int rows,
                             const int cols)
{
  TemplateSearch *template_search = template_search_setup(
      ptr, propname, searchptr, searchpropname);

  if (template_search != nullptr) {
    template_search->use_previews = true;
    template_search->preview_rows = rows;
    template_search->preview_cols = cols;

    template_search_buttons(C, layout, template_search, newop, unlinkop);

    MEM_freeN(template_search);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name RNA Path Builder Template
 * \{ */

/* ---------- */

void uiTemplatePathBuilder(uiLayout *layout,
                           PointerRNA *ptr,
                           const char *propname,
                           PointerRNA * /*root_ptr*/,
                           const char *text)
{
  /* check that properties are valid */
  PropertyRNA *propPath = RNA_struct_find_property(ptr, propname);
  if (!propPath || RNA_property_type(propPath) != PROP_STRING) {
    RNA_warning("path property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  /* Start drawing UI Elements using standard defines */
  uiLayout *row = uiLayoutRow(layout, true);

  /* Path (existing string) Widget */
  uiItemR(row, ptr, propname, 0, text, ICON_RNA);

  /* TODO: attach something to this to make allow
   * searching of nested properties to 'build' the path */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modifiers Template
 *
 *  Template for building the panel layout for the active object's modifiers.
 * \{ */

static void modifier_panel_id(void *md_link, char *r_name)
{
  ModifierData *md = (ModifierData *)md_link;
  BKE_modifier_type_panel_id(ModifierType(md->type), r_name);
}

void uiTemplateModifiers(uiLayout * /*layout*/, bContext *C)
{
  ARegion *region = CTX_wm_region(C);

  Object *ob = ED_object_active_context(C);
  ListBase *modifiers = &ob->modifiers;

  const bool panels_match = UI_panel_list_matches_data(region, modifiers, modifier_panel_id);

  if (!panels_match) {
    UI_panels_free_instanced(C, region);
    ModifierData *md = static_cast<ModifierData *>(modifiers->first);
    for (int i = 0; md; i++, md = md->next) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
      if (mti->panelRegister == nullptr) {
        continue;
      }

      char panel_idname[MAX_NAME];
      modifier_panel_id(md, panel_idname);

      /* Create custom data RNA pointer. */
      PointerRNA *md_ptr = static_cast<PointerRNA *>(MEM_mallocN(sizeof(PointerRNA), __func__));
      RNA_pointer_create(&ob->id, &RNA_Modifier, md, md_ptr);

      UI_panel_add_instanced(C, region, &region->panels, panel_idname, md_ptr);
    }
  }
  else {
    /* Assuming there's only one group of instanced panels, update the custom data pointers. */
    Panel *panel = static_cast<Panel *>(region->panels.first);
    LISTBASE_FOREACH (ModifierData *, md, modifiers) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
      if (mti->panelRegister == nullptr) {
        continue;
      }

      /* Move to the next instanced panel corresponding to the next modifier. */
      while ((panel->type == nullptr) || !(panel->type->flag & PANEL_TYPE_INSTANCED)) {
        panel = panel->next;
        BLI_assert(panel !=
                   nullptr); /* There shouldn't be fewer panels than modifiers with UIs. */
      }

      PointerRNA *md_ptr = static_cast<PointerRNA *>(MEM_mallocN(sizeof(PointerRNA), __func__));
      RNA_pointer_create(&ob->id, &RNA_Modifier, md, md_ptr);
      UI_panel_custom_data_set(panel, md_ptr);

      panel = panel->next;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Constraints Template
 *
 *  Template for building the panel layout for the active object or bone's constraints.
 * \{ */

/** For building the panel UI for constraints. */
#define CONSTRAINT_TYPE_PANEL_PREFIX "OBJECT_PT_"
#define CONSTRAINT_BONE_TYPE_PANEL_PREFIX "BONE_PT_"

/**
 * Check if the panel's ID starts with 'BONE', meaning it is a bone constraint.
 */
static bool constraint_panel_is_bone(Panel *panel)
{
  return (panel->panelname[0] == 'B') && (panel->panelname[1] == 'O') &&
         (panel->panelname[2] == 'N') && (panel->panelname[3] == 'E');
}

/**
 * Move a constraint to the index it's moved to after a drag and drop.
 */
static void constraint_reorder(bContext *C, Panel *panel, int new_index)
{
  const bool constraint_from_bone = constraint_panel_is_bone(panel);

  PointerRNA *con_ptr = UI_panel_custom_data_get(panel);
  bConstraint *con = (bConstraint *)con_ptr->data;

  PointerRNA props_ptr;
  wmOperatorType *ot = WM_operatortype_find("CONSTRAINT_OT_move_to_index", false);
  WM_operator_properties_create_ptr(&props_ptr, ot);
  RNA_string_set(&props_ptr, "constraint", con->name);
  RNA_int_set(&props_ptr, "index", new_index);
  /* Set owner to #EDIT_CONSTRAINT_OWNER_OBJECT or #EDIT_CONSTRAINT_OWNER_BONE. */
  RNA_enum_set(&props_ptr, "owner", constraint_from_bone ? 1 : 0);
  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &props_ptr, nullptr);
  WM_operator_properties_free(&props_ptr);
}

/**
 * Get the expand flag from the active constraint to use for the panel.
 */
static short get_constraint_expand_flag(const bContext * /*C*/, Panel *panel)
{
  PointerRNA *con_ptr = UI_panel_custom_data_get(panel);
  bConstraint *con = (bConstraint *)con_ptr->data;

  return con->ui_expand_flag;
}

/**
 * Save the expand flag for the panel and sub-panels to the constraint.
 */
static void set_constraint_expand_flag(const bContext * /*C*/, Panel *panel, short expand_flag)
{
  PointerRNA *con_ptr = UI_panel_custom_data_get(panel);
  bConstraint *con = (bConstraint *)con_ptr->data;
  con->ui_expand_flag = expand_flag;
}

/**
 * Function with void * argument for #uiListPanelIDFromDataFunc.
 *
 * \note Constraint panel types are assumed to be named with the struct name field
 * concatenated to the defined prefix.
 */
static void object_constraint_panel_id(void *md_link, char *r_name)
{
  bConstraint *con = (bConstraint *)md_link;
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_from_type(con->type);

  /* Cannot get TypeInfo for invalid/legacy constraints. */
  if (cti == nullptr) {
    return;
  }

  strcpy(r_name, CONSTRAINT_TYPE_PANEL_PREFIX);
  strcat(r_name, cti->structName);
}

static void bone_constraint_panel_id(void *md_link, char *r_name)
{
  bConstraint *con = (bConstraint *)md_link;
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_from_type(con->type);

  /* Cannot get TypeInfo for invalid/legacy constraints. */
  if (cti == nullptr) {
    return;
  }

  strcpy(r_name, CONSTRAINT_BONE_TYPE_PANEL_PREFIX);
  strcat(r_name, cti->structName);
}

void uiTemplateConstraints(uiLayout * /*layout*/, bContext *C, bool use_bone_constraints)
{
  ARegion *region = CTX_wm_region(C);

  Object *ob = ED_object_active_context(C);
  ListBase *constraints = {nullptr};
  if (use_bone_constraints) {
    constraints = ED_object_pose_constraint_list(C);
  }
  else if (ob != nullptr) {
    constraints = &ob->constraints;
  }

  /* Switch between the bone panel ID function and the object panel ID function. */
  uiListPanelIDFromDataFunc panel_id_func = use_bone_constraints ? bone_constraint_panel_id :
                                                                   object_constraint_panel_id;

  const bool panels_match = UI_panel_list_matches_data(region, constraints, panel_id_func);

  if (!panels_match) {
    UI_panels_free_instanced(C, region);
    bConstraint *con = (constraints == nullptr) ? nullptr :
                                                  static_cast<bConstraint *>(constraints->first);
    for (int i = 0; con; i++, con = con->next) {
      /* Don't show invalid/legacy constraints. */
      if (con->type == CONSTRAINT_TYPE_NULL) {
        continue;
      }
      /* Don't show temporary constraints (AutoIK and target-less IK constraints). */
      if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
        bKinematicConstraint *data = static_cast<bKinematicConstraint *>(con->data);
        if (data->flag & CONSTRAINT_IK_TEMP) {
          continue;
        }
      }

      char panel_idname[MAX_NAME];
      panel_id_func(con, panel_idname);

      /* Create custom data RNA pointer. */
      PointerRNA *con_ptr = static_cast<PointerRNA *>(MEM_mallocN(sizeof(PointerRNA), __func__));
      RNA_pointer_create(&ob->id, &RNA_Constraint, con, con_ptr);

      Panel *new_panel = UI_panel_add_instanced(C, region, &region->panels, panel_idname, con_ptr);

      if (new_panel) {
        /* Set the list panel functionality function pointers since we don't do it with python. */
        new_panel->type->set_list_data_expand_flag = set_constraint_expand_flag;
        new_panel->type->get_list_data_expand_flag = get_constraint_expand_flag;
        new_panel->type->reorder = constraint_reorder;
      }
    }
  }
  else {
    /* Assuming there's only one group of instanced panels, update the custom data pointers. */
    Panel *panel = static_cast<Panel *>(region->panels.first);
    LISTBASE_FOREACH (bConstraint *, con, constraints) {
      /* Don't show invalid/legacy constraints. */
      if (con->type == CONSTRAINT_TYPE_NULL) {
        continue;
      }
      /* Don't show temporary constraints (AutoIK and target-less IK constraints). */
      if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
        bKinematicConstraint *data = static_cast<bKinematicConstraint *>(con->data);
        if (data->flag & CONSTRAINT_IK_TEMP) {
          continue;
        }
      }

      /* Move to the next instanced panel corresponding to the next constraint. */
      while ((panel->type == nullptr) || !(panel->type->flag & PANEL_TYPE_INSTANCED)) {
        panel = panel->next;
        BLI_assert(panel != nullptr); /* There shouldn't be fewer panels than constraint panels. */
      }

      PointerRNA *con_ptr = static_cast<PointerRNA *>(MEM_mallocN(sizeof(PointerRNA), __func__));
      RNA_pointer_create(&ob->id, &RNA_Constraint, con, con_ptr);
      UI_panel_custom_data_set(panel, con_ptr);

      panel = panel->next;
    }
  }
}

#undef CONSTRAINT_TYPE_PANEL_PREFIX
#undef CONSTRAINT_BONE_TYPE_PANEL_PREFIX

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grease Pencil Modifiers Template
 * \{ */

/**
 * Function with void * argument for #uiListPanelIDFromDataFunc.
 */
static void gpencil_modifier_panel_id(void *md_link, char *r_name)
{
  ModifierData *md = (ModifierData *)md_link;
  BKE_gpencil_modifierType_panel_id(GpencilModifierType(md->type), r_name);
}

void uiTemplateGpencilModifiers(uiLayout * /*layout*/, bContext *C)
{
  ARegion *region = CTX_wm_region(C);
  Object *ob = ED_object_active_context(C);
  ListBase *modifiers = &ob->greasepencil_modifiers;

  const bool panels_match = UI_panel_list_matches_data(
      region, modifiers, gpencil_modifier_panel_id);

  if (!panels_match) {
    UI_panels_free_instanced(C, region);
    GpencilModifierData *md = static_cast<GpencilModifierData *>(modifiers->first);
    for (int i = 0; md; i++, md = md->next) {
      const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(
          GpencilModifierType(md->type));
      if (mti->panelRegister == nullptr) {
        continue;
      }

      char panel_idname[MAX_NAME];
      gpencil_modifier_panel_id(md, panel_idname);

      /* Create custom data RNA pointer. */
      PointerRNA *md_ptr = static_cast<PointerRNA *>(MEM_mallocN(sizeof(PointerRNA), __func__));
      RNA_pointer_create(&ob->id, &RNA_GpencilModifier, md, md_ptr);

      UI_panel_add_instanced(C, region, &region->panels, panel_idname, md_ptr);
    }
  }
  else {
    /* Assuming there's only one group of instanced panels, update the custom data pointers. */
    Panel *panel = static_cast<Panel *>(region->panels.first);
    LISTBASE_FOREACH (ModifierData *, md, modifiers) {
      const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(
          GpencilModifierType(md->type));
      if (mti->panelRegister == nullptr) {
        continue;
      }

      /* Move to the next instanced panel corresponding to the next modifier. */
      while ((panel->type == nullptr) || !(panel->type->flag & PANEL_TYPE_INSTANCED)) {
        panel = panel->next;
        BLI_assert(panel !=
                   nullptr); /* There shouldn't be fewer panels than modifiers with UIs. */
      }

      PointerRNA *md_ptr = static_cast<PointerRNA *>(MEM_mallocN(sizeof(PointerRNA), __func__));
      RNA_pointer_create(&ob->id, &RNA_GpencilModifier, md, md_ptr);
      UI_panel_custom_data_set(panel, md_ptr);

      panel = panel->next;
    }
  }
}

/** \} */

#define ERROR_LIBDATA_MESSAGE TIP_("Can't edit external library data")

/* -------------------------------------------------------------------- */
/** \name ShaderFx Template
 *
 *  Template for building the panel layout for the active object's grease pencil shader
 * effects.
 * \{ */

/**
 * Function with void * argument for #uiListPanelIDFromDataFunc.
 */
static void shaderfx_panel_id(void *fx_v, char *r_idname)
{
  ShaderFxData *fx = (ShaderFxData *)fx_v;
  BKE_shaderfxType_panel_id(ShaderFxType(fx->type), r_idname);
}

void uiTemplateShaderFx(uiLayout * /*layout*/, bContext *C)
{
  ARegion *region = CTX_wm_region(C);
  Object *ob = ED_object_active_context(C);
  ListBase *shaderfx = &ob->shader_fx;

  const bool panels_match = UI_panel_list_matches_data(region, shaderfx, shaderfx_panel_id);

  if (!panels_match) {
    UI_panels_free_instanced(C, region);
    ShaderFxData *fx = static_cast<ShaderFxData *>(shaderfx->first);
    for (int i = 0; fx; i++, fx = fx->next) {
      char panel_idname[MAX_NAME];
      shaderfx_panel_id(fx, panel_idname);

      /* Create custom data RNA pointer. */
      PointerRNA *fx_ptr = static_cast<PointerRNA *>(MEM_mallocN(sizeof(PointerRNA), __func__));
      RNA_pointer_create(&ob->id, &RNA_ShaderFx, fx, fx_ptr);

      UI_panel_add_instanced(C, region, &region->panels, panel_idname, fx_ptr);
    }
  }
  else {
    /* Assuming there's only one group of instanced panels, update the custom data pointers. */
    Panel *panel = static_cast<Panel *>(region->panels.first);
    LISTBASE_FOREACH (ShaderFxData *, fx, shaderfx) {
      const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx->type));
      if (fxi->panelRegister == nullptr) {
        continue;
      }

      /* Move to the next instanced panel corresponding to the next modifier. */
      while ((panel->type == nullptr) || !(panel->type->flag & PANEL_TYPE_INSTANCED)) {
        panel = panel->next;
        BLI_assert(panel !=
                   nullptr); /* There shouldn't be fewer panels than modifiers with UIs. */
      }

      PointerRNA *fx_ptr = static_cast<PointerRNA *>(MEM_mallocN(sizeof(PointerRNA), __func__));
      RNA_pointer_create(&ob->id, &RNA_ShaderFx, fx, fx_ptr);
      UI_panel_custom_data_set(panel, fx_ptr);

      panel = panel->next;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Property Buttons Template
 * \{ */

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
      (RNA_property_tags(prop) & OP_PROP_TAG_ADVANCED)) {
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
    const IDPropertyTemplate val = {0};
    op->properties = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");
  }

  /* poll() on this operator may still fail,
   * at the moment there is no nice feedback when this happens just fails silently. */
  if (!WM_operator_repeat_check(C, op)) {
    UI_block_lock_set(block, true, "Operator can't redo");
    return return_info;
  }

  /* useful for macros where only one of the steps can't be re-done */
  UI_block_lock_clear(block);

  if (layout_flags & UI_TEMPLATE_OP_PROPS_SHOW_TITLE) {
    uiItemL(layout, WM_operatortype_name(op->type, op->ptr), ICON_NONE);
  }

  /* menu */
  if (op->type->flag & OPTYPE_PRESET) {
    /* XXX, no simple way to get WM_MT_operator_presets.bl_label
     * from python! Label remains the same always! */
    PointerRNA op_ptr;
    uiLayout *row;

    block->ui_operator = op;

    row = uiLayoutRow(layout, true);
    uiItemM(row, "WM_MT_operator_presets", nullptr, ICON_NONE);

    wmOperatorType *ot = WM_operatortype_find("WM_OT_operator_preset_add", false);
    uiItemFullO_ptr(row, ot, "", ICON_ADD, nullptr, WM_OP_INVOKE_DEFAULT, 0, &op_ptr);
    RNA_string_set(&op_ptr, "operator", op->type->idname);

    uiItemFullO_ptr(row, ot, "", ICON_REMOVE, nullptr, WM_OP_INVOKE_DEFAULT, 0, &op_ptr);
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

    PointerRNA ptr;
    RNA_pointer_create(&wm->id, op->type->srna, op->properties, &ptr);

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
        (layout_flags & UI_TEMPLATE_OP_PROPS_SHOW_EMPTY)) {
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

  /* Only do this if we're not refreshing an existing UI. */
  if (block->oldblock == nullptr) {
    const bool is_popup = (block->flag & UI_BLOCK_KEEP_OPEN) != 0;

    LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
      /* no undo for buttons for operator redo panels */
      UI_but_flag_disable(but, UI_BUT_UNDO);

      /* only for popups, see T36109. */

      /* if button is operator's default property, and a text-field, enable focus for it
       * - this is used for allowing operators with popups to rename stuff with fewer clicks
       */
      if (is_popup) {
        if ((but->rnaprop == op->type->prop) && (but->type == UI_BTYPE_TEXT)) {
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
    PointerRNA ptr;

    RNA_pointer_create(&wm->id, op->type->srna, op->properties, &ptr);

    RNA_STRUCT_BEGIN (&ptr, prop) {
      if (RNA_property_flag(prop) & PROP_HIDDEN) {
        continue;
      }
      if (op->type->poll_property &&
          !ui_layout_operator_buts_poll_property(&ptr, prop, &user_data)) {
        continue;
      }
      if (RNA_property_type(prop) != PROP_BOOLEAN) {
        return false;
      }
    }
    RNA_STRUCT_END;
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Constraint Header Template
 * \{ */

#define ERROR_LIBDATA_MESSAGE TIP_("Can't edit external library data")

static void constraint_active_func(bContext * /*C*/, void *ob_v, void *con_v)
{
  ED_object_constraint_active_set(static_cast<Object *>(ob_v), static_cast<bConstraint *>(con_v));
}

static void constraint_ops_extra_draw(bContext *C, uiLayout *layout, void *con_v)
{
  PointerRNA op_ptr;
  uiLayout *row;
  bConstraint *con = (bConstraint *)con_v;

  PointerRNA ptr;
  Object *ob = ED_object_active_context(C);

  RNA_pointer_create(&ob->id, &RNA_Constraint, con, &ptr);
  uiLayoutSetContextPointer(layout, "constraint", &ptr);
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  uiLayoutSetUnitsX(layout, 4.0f);

  /* Apply. */
  uiItemO(layout,
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Apply"),
          ICON_CHECKMARK,
          "CONSTRAINT_OT_apply");

  /* Duplicate. */
  uiItemO(layout,
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Duplicate"),
          ICON_DUPLICATE,
          "CONSTRAINT_OT_copy");

  uiItemO(layout,
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Copy to Selected"),
          0,
          "CONSTRAINT_OT_copy_to_selected");

  uiItemS(layout);

  /* Move to first. */
  row = uiLayoutColumn(layout, false);
  uiItemFullO(row,
              "CONSTRAINT_OT_move_to_index",
              IFACE_("Move to First"),
              ICON_TRIA_UP,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              0,
              &op_ptr);
  RNA_int_set(&op_ptr, "index", 0);
  if (!con->prev) {
    uiLayoutSetEnabled(row, false);
  }

  /* Move to last. */
  row = uiLayoutColumn(layout, false);
  uiItemFullO(row,
              "CONSTRAINT_OT_move_to_index",
              IFACE_("Move to Last"),
              ICON_TRIA_DOWN,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              0,
              &op_ptr);
  ListBase *constraint_list = ED_object_constraint_list_from_constraint(ob, con, nullptr);
  RNA_int_set(&op_ptr, "index", BLI_listbase_count(constraint_list) - 1);
  if (!con->next) {
    uiLayoutSetEnabled(row, false);
  }
}

static void draw_constraint_header(uiLayout *layout, Object *ob, bConstraint *con)
{
  /* unless button has own callback, it adds this callback to button */
  uiBlock *block = uiLayoutGetBlock(layout);
  UI_block_func_set(block, constraint_active_func, ob, con);

  PointerRNA ptr;
  RNA_pointer_create(&ob->id, &RNA_Constraint, con, &ptr);

  if (block->panel) {
    UI_panel_context_pointer_set(block->panel, "constraint", &ptr);
  }
  else {
    uiLayoutSetContextPointer(layout, "constraint", &ptr);
  }

  /* Constraint type icon. */
  uiLayout *sub = uiLayoutRow(layout, false);
  uiLayoutSetEmboss(sub, UI_EMBOSS);
  uiLayoutSetRedAlert(sub, (con->flag & CONSTRAINT_DISABLE));
  uiItemL(sub, "", RNA_struct_ui_icon(ptr.type));

  UI_block_emboss_set(block, UI_EMBOSS);

  uiLayout *row = uiLayoutRow(layout, true);

  uiItemR(row, &ptr, "name", 0, "", ICON_NONE);

  /* Enabled eye icon. */
  uiItemR(row, &ptr, "enabled", 0, "", ICON_NONE);

  /* Extra operators menu. */
  uiItemMenuF(row, "", ICON_DOWNARROW_HLT, constraint_ops_extra_draw, con);

  /* Close 'button' - emboss calls here disable drawing of 'button' behind X */
  sub = uiLayoutRow(row, false);
  uiLayoutSetEmboss(sub, UI_EMBOSS_NONE);
  uiLayoutSetOperatorContext(sub, WM_OP_INVOKE_DEFAULT);
  uiItemO(sub, "", ICON_X, "CONSTRAINT_OT_delete");

  /* Some extra padding at the end, so the 'x' icon isn't too close to drag button. */
  uiItemS(layout);

  /* clear any locks set up for proxies/lib-linking */
  UI_block_lock_clear(block);
}

void uiTemplateConstraintHeader(uiLayout *layout, PointerRNA *ptr)
{
  /* verify we have valid data */
  if (!RNA_struct_is_a(ptr->type, &RNA_Constraint)) {
    RNA_warning("Expected constraint on object");
    return;
  }

  Object *ob = (Object *)ptr->owner_id;
  bConstraint *con = static_cast<bConstraint *>(ptr->data);

  if (!ob || !(GS(ob->id.name) == ID_OB)) {
    RNA_warning("Expected constraint on object");
    return;
  }

  UI_block_lock_set(uiLayoutGetBlock(layout), (ob && ID_IS_LINKED(ob)), ERROR_LIBDATA_MESSAGE);

  draw_constraint_header(layout, ob, con);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preview Template
 * \{ */

#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_world_types.h"

#define B_MATPRV 1

static void do_preview_buttons(bContext *C, void *arg, int event)
{
  switch (event) {
    case B_MATPRV:
      WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_PREVIEW, arg);
      break;
  }
}

void uiTemplatePreview(uiLayout *layout,
                       bContext *C,
                       ID *id,
                       bool show_buttons,
                       ID *parent,
                       MTex *slot,
                       const char *preview_id)
{
  Material *ma = nullptr;
  Tex *tex = (Tex *)id;
  short *pr_texture = nullptr;
  PointerRNA material_ptr;
  PointerRNA texture_ptr;

  char _preview_id[UI_MAX_NAME_STR];

  if (id && !ELEM(GS(id->name), ID_MA, ID_TE, ID_WO, ID_LA, ID_LS)) {
    RNA_warning("Expected ID of type material, texture, light, world or line style");
    return;
  }

  /* decide what to render */
  ID *pid = id;
  ID *pparent = nullptr;

  if (id && (GS(id->name) == ID_TE)) {
    if (parent && (GS(parent->name) == ID_MA)) {
      pr_texture = &((Material *)parent)->pr_texture;
    }
    else if (parent && (GS(parent->name) == ID_WO)) {
      pr_texture = &((World *)parent)->pr_texture;
    }
    else if (parent && (GS(parent->name) == ID_LA)) {
      pr_texture = &((Light *)parent)->pr_texture;
    }
    else if (parent && (GS(parent->name) == ID_LS)) {
      pr_texture = &((FreestyleLineStyle *)parent)->pr_texture;
    }

    if (pr_texture) {
      if (*pr_texture == TEX_PR_OTHER) {
        pid = parent;
      }
      else if (*pr_texture == TEX_PR_BOTH) {
        pparent = parent;
      }
    }
  }

  if (!preview_id || (preview_id[0] == '\0')) {
    /* If no identifier given, generate one from ID type. */
    BLI_snprintf(
        _preview_id, UI_MAX_NAME_STR, "uiPreview_%s", BKE_idtype_idcode_to_name(GS(id->name)));
    preview_id = _preview_id;
  }

  /* Find or add the uiPreview to the current Region. */
  ARegion *region = CTX_wm_region(C);
  uiPreview *ui_preview = static_cast<uiPreview *>(
      BLI_findstring(&region->ui_previews, preview_id, offsetof(uiPreview, preview_id)));

  if (!ui_preview) {
    ui_preview = MEM_cnew<uiPreview>(__func__);
    BLI_strncpy(ui_preview->preview_id, preview_id, sizeof(ui_preview->preview_id));
    ui_preview->height = (short)(UI_UNIT_Y * 7.6f);
    BLI_addtail(&region->ui_previews, ui_preview);
  }

  if (ui_preview->height < UI_UNIT_Y) {
    ui_preview->height = UI_UNIT_Y;
  }
  else if (ui_preview->height > UI_UNIT_Y * 50) { /* Rather high upper limit, yet not insane! */
    ui_preview->height = UI_UNIT_Y * 50;
  }

  /* layout */
  uiBlock *block = uiLayoutGetBlock(layout);
  uiLayout *row = uiLayoutRow(layout, false);
  uiLayout *col = uiLayoutColumn(row, false);
  uiLayoutSetKeepAspect(col, true);

  /* add preview */
  uiDefBut(block,
           UI_BTYPE_EXTRA,
           0,
           "",
           0,
           0,
           UI_UNIT_X * 10,
           ui_preview->height,
           pid,
           0.0,
           0.0,
           0,
           0,
           "");
  UI_but_func_drawextra_set(block, ED_preview_draw, pparent, slot);
  UI_block_func_handle_set(block, do_preview_buttons, nullptr);

  uiDefIconButS(block,
                UI_BTYPE_GRIP,
                0,
                ICON_GRIP,
                0,
                0,
                UI_UNIT_X * 10,
                (short)(UI_UNIT_Y * 0.3f),
                &ui_preview->height,
                UI_UNIT_Y,
                UI_UNIT_Y * 50.0f,
                0.0f,
                0.0f,
                "");

  /* add buttons */
  if (pid && show_buttons) {
    if (GS(pid->name) == ID_MA || (pparent && GS(pparent->name) == ID_MA)) {
      if (GS(pid->name) == ID_MA) {
        ma = (Material *)pid;
      }
      else {
        ma = (Material *)pparent;
      }

      /* Create RNA Pointer */
      RNA_pointer_create(&ma->id, &RNA_Material, ma, &material_ptr);

      col = uiLayoutColumn(row, true);
      uiLayoutSetScaleX(col, 1.5);
      uiItemR(col, &material_ptr, "preview_render_type", UI_ITEM_R_EXPAND, "", ICON_NONE);

      /* EEVEE preview file has baked lighting so use_preview_world has no effect,
       * just hide the option until this feature is supported. */
      if (!BKE_scene_uses_blender_eevee(CTX_data_scene(C))) {
        uiItemS(col);
        uiItemR(col, &material_ptr, "use_preview_world", 0, "", ICON_WORLD);
      }
    }

    if (pr_texture) {
      /* Create RNA Pointer */
      RNA_pointer_create(id, &RNA_Texture, tex, &texture_ptr);

      uiLayoutRow(layout, true);
      uiDefButS(block,
                UI_BTYPE_ROW,
                B_MATPRV,
                IFACE_("Texture"),
                0,
                0,
                UI_UNIT_X * 10,
                UI_UNIT_Y,
                pr_texture,
                10,
                TEX_PR_TEXTURE,
                0,
                0,
                "");
      if (GS(parent->name) == ID_MA) {
        uiDefButS(block,
                  UI_BTYPE_ROW,
                  B_MATPRV,
                  IFACE_("Material"),
                  0,
                  0,
                  UI_UNIT_X * 10,
                  UI_UNIT_Y,
                  pr_texture,
                  10,
                  TEX_PR_OTHER,
                  0,
                  0,
                  "");
      }
      else if (GS(parent->name) == ID_LA) {
        uiDefButS(block,
                  UI_BTYPE_ROW,
                  B_MATPRV,
                  CTX_IFACE_(BLT_I18NCONTEXT_ID_LIGHT, "Light"),
                  0,
                  0,
                  UI_UNIT_X * 10,
                  UI_UNIT_Y,
                  pr_texture,
                  10,
                  TEX_PR_OTHER,
                  0,
                  0,
                  "");
      }
      else if (GS(parent->name) == ID_WO) {
        uiDefButS(block,
                  UI_BTYPE_ROW,
                  B_MATPRV,
                  CTX_IFACE_(BLT_I18NCONTEXT_ID_WORLD, "World"),
                  0,
                  0,
                  UI_UNIT_X * 10,
                  UI_UNIT_Y,
                  pr_texture,
                  10,
                  TEX_PR_OTHER,
                  0,
                  0,
                  "");
      }
      else if (GS(parent->name) == ID_LS) {
        uiDefButS(block,
                  UI_BTYPE_ROW,
                  B_MATPRV,
                  IFACE_("Line Style"),
                  0,
                  0,
                  UI_UNIT_X * 10,
                  UI_UNIT_Y,
                  pr_texture,
                  10,
                  TEX_PR_OTHER,
                  0,
                  0,
                  "");
      }
      uiDefButS(block,
                UI_BTYPE_ROW,
                B_MATPRV,
                IFACE_("Both"),
                0,
                0,
                UI_UNIT_X * 10,
                UI_UNIT_Y,
                pr_texture,
                10,
                TEX_PR_BOTH,
                0,
                0,
                "");

      /* Alpha button for texture preview */
      if (*pr_texture != TEX_PR_OTHER) {
        row = uiLayoutRow(layout, false);
        uiItemR(row, &texture_ptr, "use_preview_alpha", 0, nullptr, ICON_NONE);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ColorRamp Template
 * \{ */

struct RNAUpdateCb {
  PointerRNA ptr;
  PropertyRNA *prop;
};

static void rna_update_cb(bContext *C, void *arg_cb, void * /*arg*/)
{
  RNAUpdateCb *cb = (RNAUpdateCb *)arg_cb;

  /* we call update here on the pointer property, this way the
   * owner of the curve mapping can still define its own update
   * and notifier, even if the CurveMapping struct is shared. */
  RNA_property_update(C, &cb->ptr, cb->prop);
}

enum {
  CB_FUNC_FLIP,
  CB_FUNC_DISTRIBUTE_LR,
  CB_FUNC_DISTRIBUTE_EVENLY,
  CB_FUNC_RESET,
};

static void colorband_flip_cb(bContext *C, ColorBand *coba)
{
  CBData data_tmp[MAXCOLORBAND];

  for (int a = 0; a < coba->tot; a++) {
    data_tmp[a] = coba->data[coba->tot - (a + 1)];
  }
  for (int a = 0; a < coba->tot; a++) {
    data_tmp[a].pos = 1.0f - data_tmp[a].pos;
    coba->data[a] = data_tmp[a];
  }

  /* May as well flip the `cur`. */
  coba->cur = coba->tot - (coba->cur + 1);

  ED_undo_push(C, "Flip Color Ramp");
}

static void colorband_distribute_cb(bContext *C, ColorBand *coba, bool evenly)
{
  if (coba->tot > 1) {
    const int tot = evenly ? coba->tot - 1 : coba->tot;
    const float gap = 1.0f / tot;
    float pos = 0.0f;
    for (int a = 0; a < coba->tot; a++) {
      coba->data[a].pos = pos;
      pos += gap;
    }
    ED_undo_push(C, evenly ? "Distribute Stops Evenly" : "Distribute Stops from Left");
  }
}

static void colorband_tools_dofunc(bContext *C, void *coba_v, int event)
{
  ColorBand *coba = static_cast<ColorBand *>(coba_v);

  switch (event) {
    case CB_FUNC_FLIP:
      colorband_flip_cb(C, coba);
      break;
    case CB_FUNC_DISTRIBUTE_LR:
      colorband_distribute_cb(C, coba, false);
      break;
    case CB_FUNC_DISTRIBUTE_EVENLY:
      colorband_distribute_cb(C, coba, true);
      break;
    case CB_FUNC_RESET:
      BKE_colorband_init(coba, true);
      ED_undo_push(C, "Reset Color Ramp");
      break;
  }
  ED_region_tag_redraw(CTX_wm_region(C));
}

static uiBlock *colorband_tools_func(bContext *C, ARegion *region, void *coba_v)
{
  const uiStyle *style = UI_style_get_dpi();
  ColorBand *coba = static_cast<ColorBand *>(coba_v);
  short yco = 0;
  const short menuwidth = 10 * UI_UNIT_X;

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS_PULLDOWN);
  UI_block_func_butmenu_set(block, colorband_tools_dofunc, coba);

  uiLayout *layout = UI_block_layout(block,
                                     UI_LAYOUT_VERTICAL,
                                     UI_LAYOUT_MENU,
                                     0,
                                     0,
                                     UI_MENU_WIDTH_MIN,
                                     0,
                                     UI_MENU_PADDING,
                                     style);
  UI_block_layout_set_current(block, layout);
  {
    PointerRNA coba_ptr;
    RNA_pointer_create(nullptr, &RNA_ColorRamp, coba, &coba_ptr);
    uiLayoutSetContextPointer(layout, "color_ramp", &coba_ptr);
  }

  /* We could move these to operators,
   * although this isn't important unless we want to assign key shortcuts to them. */
  {
    uiDefIconTextBut(block,
                     UI_BTYPE_BUT_MENU,
                     1,
                     ICON_BLANK1,
                     IFACE_("Flip Color Ramp"),
                     0,
                     yco -= UI_UNIT_Y,
                     menuwidth,
                     UI_UNIT_Y,
                     nullptr,
                     0.0,
                     0.0,
                     0,
                     CB_FUNC_FLIP,
                     "");
    uiDefIconTextBut(block,
                     UI_BTYPE_BUT_MENU,
                     1,
                     ICON_BLANK1,
                     IFACE_("Distribute Stops from Left"),
                     0,
                     yco -= UI_UNIT_Y,
                     menuwidth,
                     UI_UNIT_Y,
                     nullptr,
                     0.0,
                     0.0,
                     0,
                     CB_FUNC_DISTRIBUTE_LR,
                     "");
    uiDefIconTextBut(block,
                     UI_BTYPE_BUT_MENU,
                     1,
                     ICON_BLANK1,
                     IFACE_("Distribute Stops Evenly"),
                     0,
                     yco -= UI_UNIT_Y,
                     menuwidth,
                     UI_UNIT_Y,
                     nullptr,
                     0.0,
                     0.0,
                     0,
                     CB_FUNC_DISTRIBUTE_EVENLY,
                     "");

    uiItemO(layout, IFACE_("Eyedropper"), ICON_EYEDROPPER, "UI_OT_eyedropper_colorramp");

    uiDefIconTextBut(block,
                     UI_BTYPE_BUT_MENU,
                     1,
                     ICON_BLANK1,
                     IFACE_("Reset Color Ramp"),
                     0,
                     yco -= UI_UNIT_Y,
                     menuwidth,
                     UI_UNIT_Y,
                     nullptr,
                     0.0,
                     0.0,
                     0,
                     CB_FUNC_RESET,
                     "");
  }

  UI_block_direction_set(block, UI_DIR_DOWN);
  UI_block_bounds_set_text(block, 3.0f * UI_UNIT_X);

  return block;
}

static void colorband_add_cb(bContext *C, void *cb_v, void *coba_v)
{
  ColorBand *coba = static_cast<ColorBand *>(coba_v);
  float pos = 0.5f;

  if (coba->tot > 1) {
    if (coba->cur > 0) {
      pos = (coba->data[coba->cur - 1].pos + coba->data[coba->cur].pos) * 0.5f;
    }
    else {
      pos = (coba->data[coba->cur + 1].pos + coba->data[coba->cur].pos) * 0.5f;
    }
  }

  if (BKE_colorband_element_add(coba, pos)) {
    rna_update_cb(C, cb_v, nullptr);
    ED_undo_push(C, "Add Color Ramp Stop");
  }
}

static void colorband_del_cb(bContext *C, void *cb_v, void *coba_v)
{
  ColorBand *coba = static_cast<ColorBand *>(coba_v);

  if (BKE_colorband_element_remove(coba, coba->cur)) {
    ED_undo_push(C, "Delete Color Ramp Stop");
    rna_update_cb(C, cb_v, nullptr);
  }
}

static void colorband_update_cb(bContext * /*C*/, void *bt_v, void *coba_v)
{
  uiBut *bt = static_cast<uiBut *>(bt_v);
  ColorBand *coba = static_cast<ColorBand *>(coba_v);

  /* Sneaky update here, we need to sort the color-band points to be in order,
   * however the RNA pointer then is wrong, so we update it */
  BKE_colorband_update_sort(coba);
  bt->rnapoin.data = coba->data + coba->cur;
}

static void colorband_buttons_layout(uiLayout *layout,
                                     uiBlock *block,
                                     ColorBand *coba,
                                     const rctf *butr,
                                     RNAUpdateCb *cb,
                                     int expand)
{
  uiBut *bt;
  const float unit = BLI_rctf_size_x(butr) / 14.0f;
  const float xs = butr->xmin;
  const float ys = butr->ymin;

  PointerRNA ptr;
  RNA_pointer_create(cb->ptr.owner_id, &RNA_ColorRamp, coba, &ptr);

  uiLayout *split = uiLayoutSplit(layout, 0.4f, false);

  UI_block_emboss_set(block, UI_EMBOSS_NONE);
  UI_block_align_begin(block);
  uiLayout *row = uiLayoutRow(split, false);

  bt = uiDefIconTextBut(block,
                        UI_BTYPE_BUT,
                        0,
                        ICON_ADD,
                        "",
                        0,
                        0,
                        2.0f * unit,
                        UI_UNIT_Y,
                        nullptr,
                        0,
                        0,
                        0,
                        0,
                        TIP_("Add a new color stop to the color ramp"));
  UI_but_funcN_set(bt, colorband_add_cb, MEM_dupallocN(cb), coba);

  bt = uiDefIconTextBut(block,
                        UI_BTYPE_BUT,
                        0,
                        ICON_REMOVE,
                        "",
                        xs + 2.0f * unit,
                        ys + UI_UNIT_Y,
                        2.0f * unit,
                        UI_UNIT_Y,
                        nullptr,
                        0,
                        0,
                        0,
                        0,
                        TIP_("Delete the active position"));
  UI_but_funcN_set(bt, colorband_del_cb, MEM_dupallocN(cb), coba);

  bt = uiDefIconBlockBut(block,
                         colorband_tools_func,
                         coba,
                         0,
                         ICON_DOWNARROW_HLT,
                         xs + 4.0f * unit,
                         ys + UI_UNIT_Y,
                         2.0f * unit,
                         UI_UNIT_Y,
                         TIP_("Tools"));
  UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), coba);

  UI_block_align_end(block);
  UI_block_emboss_set(block, UI_EMBOSS);

  row = uiLayoutRow(split, false);

  UI_block_align_begin(block);
  uiItemR(row, &ptr, "color_mode", 0, "", ICON_NONE);
  if (ELEM(coba->color_mode, COLBAND_BLEND_HSV, COLBAND_BLEND_HSL)) {
    uiItemR(row, &ptr, "hue_interpolation", 0, "", ICON_NONE);
  }
  else { /* COLBAND_BLEND_RGB */
    uiItemR(row, &ptr, "interpolation", 0, "", ICON_NONE);
  }
  UI_block_align_end(block);

  row = uiLayoutRow(layout, false);

  bt = uiDefBut(block,
                UI_BTYPE_COLORBAND,
                0,
                "",
                xs,
                ys,
                BLI_rctf_size_x(butr),
                UI_UNIT_Y,
                coba,
                0,
                0,
                0,
                0,
                "");
  UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), nullptr);

  row = uiLayoutRow(layout, false);

  if (coba->tot) {
    CBData *cbd = coba->data + coba->cur;

    RNA_pointer_create(cb->ptr.owner_id, &RNA_ColorRampElement, cbd, &ptr);

    if (!expand) {
      split = uiLayoutSplit(layout, 0.3f, false);

      row = uiLayoutRow(split, false);
      bt = uiDefButS(block,
                     UI_BTYPE_NUM,
                     0,
                     "",
                     0,
                     0,
                     5.0f * UI_UNIT_X,
                     UI_UNIT_Y,
                     &coba->cur,
                     0.0,
                     float(MAX2(0, coba->tot - 1)),
                     0,
                     0,
                     TIP_("Choose active color stop"));
      UI_but_number_step_size_set(bt, 1);

      row = uiLayoutRow(split, false);
      uiItemR(row, &ptr, "position", 0, IFACE_("Pos"), ICON_NONE);

      row = uiLayoutRow(layout, false);
      uiItemR(row, &ptr, "color", 0, "", ICON_NONE);
    }
    else {
      split = uiLayoutSplit(layout, 0.5f, false);
      uiLayout *subsplit = uiLayoutSplit(split, 0.35f, false);

      row = uiLayoutRow(subsplit, false);
      bt = uiDefButS(block,
                     UI_BTYPE_NUM,
                     0,
                     "",
                     0,
                     0,
                     5.0f * UI_UNIT_X,
                     UI_UNIT_Y,
                     &coba->cur,
                     0.0,
                     float(MAX2(0, coba->tot - 1)),
                     0,
                     0,
                     TIP_("Choose active color stop"));
      UI_but_number_step_size_set(bt, 1);

      row = uiLayoutRow(subsplit, false);
      uiItemR(row, &ptr, "position", UI_ITEM_R_SLIDER, IFACE_("Pos"), ICON_NONE);

      row = uiLayoutRow(split, false);
      uiItemR(row, &ptr, "color", 0, "", ICON_NONE);
    }

    /* Some special (rather awkward) treatment to update UI state on certain property changes. */
    LISTBASE_FOREACH_BACKWARD (uiBut *, but, &block->buttons) {
      if (but->rnapoin.data != ptr.data) {
        continue;
      }
      if (!but->rnaprop) {
        continue;
      }

      const char *prop_identifier = RNA_property_identifier(but->rnaprop);
      if (STREQ(prop_identifier, "position")) {
        UI_but_func_set(but, colorband_update_cb, but, coba);
      }

      if (STREQ(prop_identifier, "color")) {
        UI_but_funcN_set(but, rna_update_cb, MEM_dupallocN(cb), nullptr);
      }
    }
  }
}

void uiTemplateColorRamp(uiLayout *layout, PointerRNA *ptr, const char *propname, bool expand)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop || RNA_property_type(prop) != PROP_POINTER) {
    return;
  }

  const PointerRNA cptr = RNA_property_pointer_get(ptr, prop);
  if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_ColorRamp)) {
    return;
  }

  RNAUpdateCb *cb = MEM_cnew<RNAUpdateCb>("RNAUpdateCb");
  cb->ptr = *ptr;
  cb->prop = prop;

  rctf rect;
  rect.xmin = 0;
  rect.xmax = 10.0f * UI_UNIT_X;
  rect.ymin = 0;
  rect.ymax = 19.5f * UI_UNIT_X;

  uiBlock *block = uiLayoutAbsoluteBlock(layout);

  ID *id = cptr.owner_id;
  UI_block_lock_set(block, (id && ID_IS_LINKED(id)), ERROR_LIBDATA_MESSAGE);

  colorband_buttons_layout(layout, block, static_cast<ColorBand *>(cptr.data), &rect, cb, expand);

  UI_block_lock_clear(block);

  MEM_freeN(cb);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Icon Template
 * \{ */

void uiTemplateIcon(uiLayout *layout, int icon_value, float icon_scale)
{
  uiBlock *block = uiLayoutAbsoluteBlock(layout);
  uiBut *but = uiDefIconBut(block,
                            UI_BTYPE_LABEL,
                            0,
                            ICON_X,
                            0,
                            0,
                            UI_UNIT_X * icon_scale,
                            UI_UNIT_Y * icon_scale,
                            nullptr,
                            0.0,
                            0.0,
                            0.0,
                            0.0,
                            "");
  ui_def_but_icon(but, icon_value, UI_HAS_ICON | UI_BUT_ICON_PREVIEW);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Icon viewer Template
 * \{ */

struct IconViewMenuArgs {
  PointerRNA ptr;
  PropertyRNA *prop;
  bool show_labels;
  float icon_scale;
};

/* ID Search browse menu, open */
static uiBlock *ui_icon_view_menu_cb(bContext *C, ARegion *region, void *arg_litem)
{
  static IconViewMenuArgs args;

  /* arg_litem is malloced, can be freed by parent button */
  args = *((IconViewMenuArgs *)arg_litem);
  const int w = UI_UNIT_X * (args.icon_scale);
  const int h = UI_UNIT_X * (args.icon_scale + args.show_labels);

  uiBlock *block = UI_block_begin(C, region, "_popup", UI_EMBOSS_PULLDOWN);
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_NO_FLIP);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  bool free;
  const EnumPropertyItem *item;
  RNA_property_enum_items(C, &args.ptr, args.prop, &item, nullptr, &free);

  for (int a = 0; item[a].identifier; a++) {
    const int x = (a % 8) * w;
    const int y = -(a / 8) * h;

    const int icon = item[a].icon;
    const int value = item[a].value;
    uiBut *but;
    if (args.show_labels) {
      but = uiDefIconTextButR_prop(block,
                                   UI_BTYPE_ROW,
                                   0,
                                   icon,
                                   item[a].name,
                                   x,
                                   y,
                                   w,
                                   h,
                                   &args.ptr,
                                   args.prop,
                                   -1,
                                   0,
                                   value,
                                   -1,
                                   -1,
                                   nullptr);
    }
    else {
      but = uiDefIconButR_prop(block,
                               UI_BTYPE_ROW,
                               0,
                               icon,
                               x,
                               y,
                               w,
                               h,
                               &args.ptr,
                               args.prop,
                               -1,
                               0,
                               value,
                               -1,
                               -1,
                               nullptr);
    }
    ui_def_but_icon(but, icon, UI_HAS_ICON | UI_BUT_ICON_PREVIEW);
  }

  UI_block_bounds_set_normal(block, 0.3f * U.widget_unit);
  UI_block_direction_set(block, UI_DIR_DOWN);

  if (free) {
    MEM_freeN((void *)item);
  }

  return block;
}

void uiTemplateIconView(uiLayout *layout,
                        PointerRNA *ptr,
                        const char *propname,
                        bool show_labels,
                        float icon_scale,
                        float icon_scale_popup)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop || RNA_property_type(prop) != PROP_ENUM) {
    RNA_warning(
        "property of type Enum not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  uiBlock *block = uiLayoutAbsoluteBlock(layout);

  int tot_items;
  bool free_items;
  const EnumPropertyItem *items;
  RNA_property_enum_items(
      static_cast<bContext *>(block->evil_C), ptr, prop, &items, &tot_items, &free_items);
  const int value = RNA_property_enum_get(ptr, prop);
  int icon = ICON_NONE;
  RNA_enum_icon_from_value(items, value, &icon);

  uiBut *but;
  if (RNA_property_editable(ptr, prop)) {
    IconViewMenuArgs *cb_args = MEM_cnew<IconViewMenuArgs>(__func__);
    cb_args->ptr = *ptr;
    cb_args->prop = prop;
    cb_args->show_labels = show_labels;
    cb_args->icon_scale = icon_scale_popup;

    but = uiDefBlockButN(block,
                         ui_icon_view_menu_cb,
                         cb_args,
                         "",
                         0,
                         0,
                         UI_UNIT_X * icon_scale,
                         UI_UNIT_Y * icon_scale,
                         "");
  }
  else {
    but = uiDefIconBut(block,
                       UI_BTYPE_LABEL,
                       0,
                       ICON_X,
                       0,
                       0,
                       UI_UNIT_X * icon_scale,
                       UI_UNIT_Y * icon_scale,
                       nullptr,
                       0.0,
                       0.0,
                       0.0,
                       0.0,
                       "");
  }

  ui_def_but_icon(but, icon, UI_HAS_ICON | UI_BUT_ICON_PREVIEW);

  if (free_items) {
    MEM_freeN((void *)items);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Histogram Template
 * \{ */

void uiTemplateHistogram(uiLayout *layout, PointerRNA *ptr, const char *propname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop || RNA_property_type(prop) != PROP_POINTER) {
    return;
  }

  const PointerRNA cptr = RNA_property_pointer_get(ptr, prop);
  if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_Histogram)) {
    return;
  }
  Histogram *hist = (Histogram *)cptr.data;

  if (hist->height < UI_UNIT_Y) {
    hist->height = UI_UNIT_Y;
  }
  else if (hist->height > UI_UNIT_Y * 20) {
    hist->height = UI_UNIT_Y * 20;
  }

  uiLayout *col = uiLayoutColumn(layout, true);
  uiBlock *block = uiLayoutGetBlock(col);

  uiDefBut(
      block, UI_BTYPE_HISTOGRAM, 0, "", 0, 0, UI_UNIT_X * 10, hist->height, hist, 0, 0, 0, 0, "");

  /* Resize grip. */
  uiDefIconButI(block,
                UI_BTYPE_GRIP,
                0,
                ICON_GRIP,
                0,
                0,
                UI_UNIT_X * 10,
                (short)(UI_UNIT_Y * 0.3f),
                &hist->height,
                UI_UNIT_Y,
                UI_UNIT_Y * 20.0f,
                0.0f,
                0.0f,
                "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Waveform Template
 * \{ */

void uiTemplateWaveform(uiLayout *layout, PointerRNA *ptr, const char *propname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop || RNA_property_type(prop) != PROP_POINTER) {
    return;
  }

  const PointerRNA cptr = RNA_property_pointer_get(ptr, prop);
  if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_Scopes)) {
    return;
  }
  Scopes *scopes = (Scopes *)cptr.data;

  uiLayout *col = uiLayoutColumn(layout, true);
  uiBlock *block = uiLayoutGetBlock(col);

  if (scopes->wavefrm_height < UI_UNIT_Y) {
    scopes->wavefrm_height = UI_UNIT_Y;
  }
  else if (scopes->wavefrm_height > UI_UNIT_Y * 20) {
    scopes->wavefrm_height = UI_UNIT_Y * 20;
  }

  uiDefBut(block,
           UI_BTYPE_WAVEFORM,
           0,
           "",
           0,
           0,
           UI_UNIT_X * 10,
           scopes->wavefrm_height,
           scopes,
           0,
           0,
           0,
           0,
           "");

  /* Resize grip. */
  uiDefIconButI(block,
                UI_BTYPE_GRIP,
                0,
                ICON_GRIP,
                0,
                0,
                UI_UNIT_X * 10,
                (short)(UI_UNIT_Y * 0.3f),
                &scopes->wavefrm_height,
                UI_UNIT_Y,
                UI_UNIT_Y * 20.0f,
                0.0f,
                0.0f,
                "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector-Scope Template
 * \{ */

void uiTemplateVectorscope(uiLayout *layout, PointerRNA *ptr, const char *propname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop || RNA_property_type(prop) != PROP_POINTER) {
    return;
  }

  const PointerRNA cptr = RNA_property_pointer_get(ptr, prop);
  if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_Scopes)) {
    return;
  }
  Scopes *scopes = (Scopes *)cptr.data;

  if (scopes->vecscope_height < UI_UNIT_Y) {
    scopes->vecscope_height = UI_UNIT_Y;
  }
  else if (scopes->vecscope_height > UI_UNIT_Y * 20) {
    scopes->vecscope_height = UI_UNIT_Y * 20;
  }

  uiLayout *col = uiLayoutColumn(layout, true);
  uiBlock *block = uiLayoutGetBlock(col);

  uiDefBut(block,
           UI_BTYPE_VECTORSCOPE,
           0,
           "",
           0,
           0,
           UI_UNIT_X * 10,
           scopes->vecscope_height,
           scopes,
           0,
           0,
           0,
           0,
           "");

  /* Resize grip. */
  uiDefIconButI(block,
                UI_BTYPE_GRIP,
                0,
                ICON_GRIP,
                0,
                0,
                UI_UNIT_X * 10,
                (short)(UI_UNIT_Y * 0.3f),
                &scopes->vecscope_height,
                UI_UNIT_Y,
                UI_UNIT_Y * 20.0f,
                0.0f,
                0.0f,
                "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name CurveMapping Template
 * \{ */

#define CURVE_ZOOM_MAX (1.0f / 25.0f)

static bool curvemap_can_zoom_out(CurveMapping *cumap)
{
  return BLI_rctf_size_x(&cumap->curr) < BLI_rctf_size_x(&cumap->clipr);
}

static bool curvemap_can_zoom_in(CurveMapping *cumap)
{
  return BLI_rctf_size_x(&cumap->curr) > CURVE_ZOOM_MAX * BLI_rctf_size_x(&cumap->clipr);
}

static void curvemap_buttons_zoom_in(bContext *C, void *cumap_v, void * /*arg*/)
{
  CurveMapping *cumap = static_cast<CurveMapping *>(cumap_v);

  if (curvemap_can_zoom_in(cumap)) {
    const float dx = 0.1154f * BLI_rctf_size_x(&cumap->curr);
    cumap->curr.xmin += dx;
    cumap->curr.xmax -= dx;
    const float dy = 0.1154f * BLI_rctf_size_y(&cumap->curr);
    cumap->curr.ymin += dy;
    cumap->curr.ymax -= dy;
  }

  ED_region_tag_redraw(CTX_wm_region(C));
}

static void curvemap_buttons_zoom_out(bContext *C, void *cumap_v, void * /*unused*/)
{
  CurveMapping *cumap = static_cast<CurveMapping *>(cumap_v);
  float d, d1;

  if (curvemap_can_zoom_out(cumap)) {
    d = d1 = 0.15f * BLI_rctf_size_x(&cumap->curr);

    if (cumap->flag & CUMA_DO_CLIP) {
      if (cumap->curr.xmin - d < cumap->clipr.xmin) {
        d1 = cumap->curr.xmin - cumap->clipr.xmin;
      }
    }
    cumap->curr.xmin -= d1;

    d1 = d;
    if (cumap->flag & CUMA_DO_CLIP) {
      if (cumap->curr.xmax + d > cumap->clipr.xmax) {
        d1 = -cumap->curr.xmax + cumap->clipr.xmax;
      }
    }
    cumap->curr.xmax += d1;

    d = d1 = 0.15f * BLI_rctf_size_y(&cumap->curr);

    if (cumap->flag & CUMA_DO_CLIP) {
      if (cumap->curr.ymin - d < cumap->clipr.ymin) {
        d1 = cumap->curr.ymin - cumap->clipr.ymin;
      }
    }
    cumap->curr.ymin -= d1;

    d1 = d;
    if (cumap->flag & CUMA_DO_CLIP) {
      if (cumap->curr.ymax + d > cumap->clipr.ymax) {
        d1 = -cumap->curr.ymax + cumap->clipr.ymax;
      }
    }
    cumap->curr.ymax += d1;
  }

  ED_region_tag_redraw(CTX_wm_region(C));
}

static void curvemap_buttons_setclip(bContext * /*C*/, void *cumap_v, void * /*arg*/)
{
  CurveMapping *cumap = static_cast<CurveMapping *>(cumap_v);

  BKE_curvemapping_changed(cumap, false);
}

static void curvemap_buttons_delete(bContext *C, void *cb_v, void *cumap_v)
{
  CurveMapping *cumap = static_cast<CurveMapping *>(cumap_v);

  BKE_curvemap_remove(cumap->cm + cumap->cur, SELECT);
  BKE_curvemapping_changed(cumap, false);

  rna_update_cb(C, cb_v, nullptr);
}

/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *curvemap_clipping_func(bContext *C, ARegion *region, void *cumap_v)
{
  CurveMapping *cumap = static_cast<CurveMapping *>(cumap_v);
  uiBut *bt;
  const float width = 8 * UI_UNIT_X;

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_MOVEMOUSE_QUIT);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  bt = uiDefButBitI(block,
                    UI_BTYPE_CHECKBOX,
                    CUMA_DO_CLIP,
                    1,
                    IFACE_("Use Clipping"),
                    0,
                    5 * UI_UNIT_Y,
                    width,
                    UI_UNIT_Y,
                    &cumap->flag,
                    0.0,
                    0.0,
                    10,
                    0,
                    "");
  UI_but_func_set(bt, curvemap_buttons_setclip, cumap, nullptr);

  UI_block_align_begin(block);
  bt = uiDefButF(block,
                 UI_BTYPE_NUM,
                 0,
                 IFACE_("Min X:"),
                 0,
                 4 * UI_UNIT_Y,
                 width,
                 UI_UNIT_Y,
                 &cumap->clipr.xmin,
                 -100.0,
                 cumap->clipr.xmax,
                 0,
                 0,
                 "");
  UI_but_number_step_size_set(bt, 10);
  UI_but_number_precision_set(bt, 2);
  bt = uiDefButF(block,
                 UI_BTYPE_NUM,
                 0,
                 IFACE_("Min Y:"),
                 0,
                 3 * UI_UNIT_Y,
                 width,
                 UI_UNIT_Y,
                 &cumap->clipr.ymin,
                 -100.0,
                 cumap->clipr.ymax,
                 0,
                 0,
                 "");
  UI_but_number_step_size_set(bt, 10);
  UI_but_number_precision_set(bt, 2);
  bt = uiDefButF(block,
                 UI_BTYPE_NUM,
                 0,
                 IFACE_("Max X:"),
                 0,
                 2 * UI_UNIT_Y,
                 width,
                 UI_UNIT_Y,
                 &cumap->clipr.xmax,
                 cumap->clipr.xmin,
                 100.0,
                 0,
                 0,
                 "");
  UI_but_number_step_size_set(bt, 10);
  UI_but_number_precision_set(bt, 2);
  bt = uiDefButF(block,
                 UI_BTYPE_NUM,
                 0,
                 IFACE_("Max Y:"),
                 0,
                 UI_UNIT_Y,
                 width,
                 UI_UNIT_Y,
                 &cumap->clipr.ymax,
                 cumap->clipr.ymin,
                 100.0,
                 0,
                 0,
                 "");
  UI_but_number_step_size_set(bt, 10);
  UI_but_number_precision_set(bt, 2);

  UI_block_bounds_set_normal(block, 0.3f * U.widget_unit);
  UI_block_direction_set(block, UI_DIR_DOWN);

  return block;
}

/* only for BKE_curvemap_tools_dofunc */
enum {
  UICURVE_FUNC_RESET_NEG,
  UICURVE_FUNC_RESET_POS,
  UICURVE_FUNC_RESET_VIEW,
  UICURVE_FUNC_HANDLE_VECTOR,
  UICURVE_FUNC_HANDLE_AUTO,
  UICURVE_FUNC_HANDLE_AUTO_ANIM,
  UICURVE_FUNC_EXTEND_HOZ,
  UICURVE_FUNC_EXTEND_EXP,
};

static void curvemap_tools_dofunc(bContext *C, void *cumap_v, int event)
{
  CurveMapping *cumap = static_cast<CurveMapping *>(cumap_v);
  CurveMap *cuma = cumap->cm + cumap->cur;

  switch (event) {
    case UICURVE_FUNC_RESET_NEG:
    case UICURVE_FUNC_RESET_POS: /* reset */
      BKE_curvemap_reset(cuma,
                         &cumap->clipr,
                         cumap->preset,
                         (event == UICURVE_FUNC_RESET_NEG) ? CURVEMAP_SLOPE_NEGATIVE :
                                                             CURVEMAP_SLOPE_POSITIVE);
      BKE_curvemapping_changed(cumap, false);
      break;
    case UICURVE_FUNC_RESET_VIEW:
      BKE_curvemapping_reset_view(cumap);
      break;
    case UICURVE_FUNC_HANDLE_VECTOR: /* Set vector. */
      BKE_curvemap_handle_set(cuma, HD_VECT);
      BKE_curvemapping_changed(cumap, false);
      break;
    case UICURVE_FUNC_HANDLE_AUTO: /* Set auto. */
      BKE_curvemap_handle_set(cuma, HD_AUTO);
      BKE_curvemapping_changed(cumap, false);
      break;
    case UICURVE_FUNC_HANDLE_AUTO_ANIM: /* Set auto-clamped. */
      BKE_curvemap_handle_set(cuma, HD_AUTO_ANIM);
      BKE_curvemapping_changed(cumap, false);
      break;
    case UICURVE_FUNC_EXTEND_HOZ: /* Extend horizontal. */
      cumap->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
      BKE_curvemapping_changed(cumap, false);
      break;
    case UICURVE_FUNC_EXTEND_EXP: /* Extend extrapolate. */
      cumap->flag |= CUMA_EXTEND_EXTRAPOLATE;
      BKE_curvemapping_changed(cumap, false);
      break;
  }
  ED_undo_push(C, "CurveMap tools");
  ED_region_tag_redraw(CTX_wm_region(C));
}

static uiBlock *curvemap_tools_func(
    bContext *C, ARegion *region, CurveMapping *cumap, bool show_extend, int reset_mode)
{
  short yco = 0;
  const short menuwidth = 10 * UI_UNIT_X;

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  UI_block_func_butmenu_set(block, curvemap_tools_dofunc, cumap);

  {
    uiDefIconTextBut(block,
                     UI_BTYPE_BUT_MENU,
                     1,
                     ICON_BLANK1,
                     IFACE_("Reset View"),
                     0,
                     yco -= UI_UNIT_Y,
                     menuwidth,
                     UI_UNIT_Y,
                     nullptr,
                     0.0,
                     0.0,
                     0,
                     UICURVE_FUNC_RESET_VIEW,
                     "");
  }

  if (show_extend) {
    uiDefIconTextBut(block,
                     UI_BTYPE_BUT_MENU,
                     1,
                     ICON_BLANK1,
                     IFACE_("Extend Horizontal"),
                     0,
                     yco -= UI_UNIT_Y,
                     menuwidth,
                     UI_UNIT_Y,
                     nullptr,
                     0.0,
                     0.0,
                     0,
                     UICURVE_FUNC_EXTEND_HOZ,
                     "");
    uiDefIconTextBut(block,
                     UI_BTYPE_BUT_MENU,
                     1,
                     ICON_BLANK1,
                     IFACE_("Extend Extrapolated"),
                     0,
                     yco -= UI_UNIT_Y,
                     menuwidth,
                     UI_UNIT_Y,
                     nullptr,
                     0.0,
                     0.0,
                     0,
                     UICURVE_FUNC_EXTEND_EXP,
                     "");
  }

  {
    uiDefIconTextBut(block,
                     UI_BTYPE_BUT_MENU,
                     1,
                     ICON_BLANK1,
                     IFACE_("Reset Curve"),
                     0,
                     yco -= UI_UNIT_Y,
                     menuwidth,
                     UI_UNIT_Y,
                     nullptr,
                     0.0,
                     0.0,
                     0,
                     reset_mode,
                     "");
  }

  UI_block_direction_set(block, UI_DIR_DOWN);
  UI_block_bounds_set_text(block, 3.0f * UI_UNIT_X);

  return block;
}

static uiBlock *curvemap_tools_posslope_func(bContext *C, ARegion *region, void *cumap_v)
{
  return curvemap_tools_func(
      C, region, static_cast<CurveMapping *>(cumap_v), true, UICURVE_FUNC_RESET_POS);
}

static uiBlock *curvemap_tools_negslope_func(bContext *C, ARegion *region, void *cumap_v)
{
  return curvemap_tools_func(
      C, region, static_cast<CurveMapping *>(cumap_v), true, UICURVE_FUNC_RESET_NEG);
}

static uiBlock *curvemap_brush_tools_func(bContext *C, ARegion *region, void *cumap_v)
{
  return curvemap_tools_func(
      C, region, static_cast<CurveMapping *>(cumap_v), false, UICURVE_FUNC_RESET_NEG);
}

static uiBlock *curvemap_brush_tools_negslope_func(bContext *C, ARegion *region, void *cumap_v)
{
  return curvemap_tools_func(
      C, region, static_cast<CurveMapping *>(cumap_v), false, UICURVE_FUNC_RESET_POS);
}

static void curvemap_tools_handle_vector(bContext *C, void *cumap_v, void * /*arg*/)
{
  curvemap_tools_dofunc(C, cumap_v, UICURVE_FUNC_HANDLE_VECTOR);
}

static void curvemap_tools_handle_auto(bContext *C, void *cumap_v, void * /*arg*/)
{
  curvemap_tools_dofunc(C, cumap_v, UICURVE_FUNC_HANDLE_AUTO);
}

static void curvemap_tools_handle_auto_clamped(bContext *C, void *cumap_v, void * /*arg*/)
{
  curvemap_tools_dofunc(C, cumap_v, UICURVE_FUNC_HANDLE_AUTO_ANIM);
}

static void curvemap_buttons_redraw(bContext *C, void * /*arg1*/, void * /*arg2*/)
{
  ED_region_tag_redraw(CTX_wm_region(C));
}

static void curvemap_buttons_update(bContext *C, void *arg1_v, void *cumap_v)
{
  CurveMapping *cumap = static_cast<CurveMapping *>(cumap_v);
  BKE_curvemapping_changed(cumap, true);
  rna_update_cb(C, arg1_v, nullptr);
}

static void curvemap_buttons_reset(bContext *C, void *cb_v, void *cumap_v)
{
  CurveMapping *cumap = static_cast<CurveMapping *>(cumap_v);
  cumap->preset = CURVE_PRESET_LINE;
  for (int a = 0; a < CM_TOT; a++) {
    BKE_curvemap_reset(cumap->cm + a, &cumap->clipr, cumap->preset, CURVEMAP_SLOPE_POSITIVE);
  }

  cumap->black[0] = cumap->black[1] = cumap->black[2] = 0.0f;
  cumap->white[0] = cumap->white[1] = cumap->white[2] = 1.0f;
  BKE_curvemapping_set_black_white(cumap, nullptr, nullptr);

  BKE_curvemapping_changed(cumap, false);

  rna_update_cb(C, cb_v, nullptr);
}

/**
 * \note Still unsure how this call evolves.
 *
 * \param labeltype: Used for defining which curve-channels to show.
 */
static void curvemap_buttons_layout(uiLayout *layout,
                                    PointerRNA *ptr,
                                    char labeltype,
                                    bool levels,
                                    bool brush,
                                    bool neg_slope,
                                    bool tone,
                                    RNAUpdateCb *cb)
{
  CurveMapping *cumap = static_cast<CurveMapping *>(ptr->data);
  CurveMap *cm = &cumap->cm[cumap->cur];
  uiBut *bt;
  const float dx = UI_UNIT_X;
  int bg = -1;

  uiBlock *block = uiLayoutGetBlock(layout);

  UI_block_emboss_set(block, UI_EMBOSS);

  if (tone) {
    uiLayout *split = uiLayoutSplit(layout, 0.0f, false);
    uiItemR(uiLayoutRow(split, false), ptr, "tone", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  }

  /* curve chooser */
  uiLayout *row = uiLayoutRow(layout, false);

  if (labeltype == 'v') {
    /* vector */
    uiLayout *sub = uiLayoutRow(row, true);
    uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);

    if (cumap->cm[0].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "X", 0, 0, dx, dx, &cumap->cur, 0.0, 0.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, nullptr, nullptr);
    }
    if (cumap->cm[1].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "Y", 0, 0, dx, dx, &cumap->cur, 0.0, 1.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, nullptr, nullptr);
    }
    if (cumap->cm[2].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "Z", 0, 0, dx, dx, &cumap->cur, 0.0, 2.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, nullptr, nullptr);
    }
  }
  else if (labeltype == 'c') {
    /* color */
    uiLayout *sub = uiLayoutRow(row, true);
    uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);

    if (cumap->cm[3].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "C", 0, 0, dx, dx, &cumap->cur, 0.0, 3.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, nullptr, nullptr);
    }
    if (cumap->cm[0].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "R", 0, 0, dx, dx, &cumap->cur, 0.0, 0.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, nullptr, nullptr);
    }
    if (cumap->cm[1].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "G", 0, 0, dx, dx, &cumap->cur, 0.0, 1.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, nullptr, nullptr);
    }
    if (cumap->cm[2].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "B", 0, 0, dx, dx, &cumap->cur, 0.0, 2.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, nullptr, nullptr);
    }
  }
  else if (labeltype == 'h') {
    /* HSV */
    uiLayout *sub = uiLayoutRow(row, true);
    uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);

    if (cumap->cm[0].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "H", 0, 0, dx, dx, &cumap->cur, 0.0, 0.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, nullptr, nullptr);
    }
    if (cumap->cm[1].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "S", 0, 0, dx, dx, &cumap->cur, 0.0, 1.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, nullptr, nullptr);
    }
    if (cumap->cm[2].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "V", 0, 0, dx, dx, &cumap->cur, 0.0, 2.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, nullptr, nullptr);
    }
  }
  else {
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);
  }

  if (labeltype == 'h') {
    bg = UI_GRAD_H;
  }

  /* operation buttons */
  /* (Right aligned) */
  uiLayout *sub = uiLayoutRow(row, true);
  uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_RIGHT);

  /* Zoom in */
  bt = uiDefIconBut(block,
                    UI_BTYPE_BUT,
                    0,
                    ICON_ZOOM_IN,
                    0,
                    0,
                    dx,
                    dx,
                    nullptr,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    TIP_("Zoom in"));
  UI_but_func_set(bt, curvemap_buttons_zoom_in, cumap, nullptr);
  if (!curvemap_can_zoom_in(cumap)) {
    UI_but_disable(bt, "");
  }

  /* Zoom out */
  bt = uiDefIconBut(block,
                    UI_BTYPE_BUT,
                    0,
                    ICON_ZOOM_OUT,
                    0,
                    0,
                    dx,
                    dx,
                    nullptr,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    TIP_("Zoom out"));
  UI_but_func_set(bt, curvemap_buttons_zoom_out, cumap, nullptr);
  if (!curvemap_can_zoom_out(cumap)) {
    UI_but_disable(bt, "");
  }

  /* Clipping button. */
  const int icon = (cumap->flag & CUMA_DO_CLIP) ? ICON_CLIPUV_HLT : ICON_CLIPUV_DEHLT;
  bt = uiDefIconBlockBut(
      block, curvemap_clipping_func, cumap, 0, icon, 0, 0, dx, dx, TIP_("Clipping Options"));
  bt->drawflag &= ~UI_BUT_ICON_LEFT;
  UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), nullptr);

  if (brush && neg_slope) {
    bt = uiDefIconBlockBut(
        block, curvemap_brush_tools_negslope_func, cumap, 0, 0, 0, 0, dx, dx, TIP_("Tools"));
  }
  else if (brush) {
    bt = uiDefIconBlockBut(
        block, curvemap_brush_tools_func, cumap, 0, 0, 0, 0, dx, dx, TIP_("Tools"));
  }
  else if (neg_slope) {
    bt = uiDefIconBlockBut(
        block, curvemap_tools_negslope_func, cumap, 0, 0, 0, 0, dx, dx, TIP_("Tools"));
  }
  else {
    bt = uiDefIconBlockBut(
        block, curvemap_tools_posslope_func, cumap, 0, 0, 0, 0, dx, dx, TIP_("Tools"));
  }
  UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), nullptr);

  UI_block_funcN_set(block, rna_update_cb, MEM_dupallocN(cb), nullptr);

  /* Curve itself. */
  const int size = max_ii(uiLayoutGetWidth(layout), UI_UNIT_X);
  row = uiLayoutRow(layout, false);
  uiButCurveMapping *curve_but = (uiButCurveMapping *)uiDefBut(
      block, UI_BTYPE_CURVE, 0, "", 0, 0, size, 8.0f * UI_UNIT_X, cumap, 0.0f, 1.0f, -1, 0, "");
  curve_but->gradient_type = eButGradientType(bg);

  /* Sliders for selected curve point. */
  int i;
  CurveMapPoint *cmp = nullptr;
  bool point_last_or_first = false;
  for (i = 0; i < cm->totpoint; i++) {
    if (cm->curve[i].flag & CUMA_SELECT) {
      cmp = &cm->curve[i];
      break;
    }
  }
  if (ELEM(i, 0, cm->totpoint - 1)) {
    point_last_or_first = true;
  }

  if (cmp) {
    rctf bounds;
    if (cumap->flag & CUMA_DO_CLIP) {
      bounds = cumap->clipr;
    }
    else {
      bounds.xmin = bounds.ymin = -1000.0;
      bounds.xmax = bounds.ymax = 1000.0;
    }

    UI_block_emboss_set(block, UI_EMBOSS);

    uiLayoutRow(layout, true);

    /* Curve handle buttons. */
    bt = uiDefIconBut(block,
                      UI_BTYPE_BUT,
                      1,
                      ICON_HANDLE_AUTO,
                      0,
                      UI_UNIT_Y,
                      UI_UNIT_X,
                      UI_UNIT_Y,
                      nullptr,
                      0.0,
                      0.0,
                      0.0,
                      0.0,
                      TIP_("Auto Handle"));
    UI_but_func_set(bt, curvemap_tools_handle_auto, cumap, nullptr);
    if (((cmp->flag & CUMA_HANDLE_AUTO_ANIM) == false) &&
        ((cmp->flag & CUMA_HANDLE_VECTOR) == false)) {
      bt->flag |= UI_SELECT_DRAW;
    }

    bt = uiDefIconBut(block,
                      UI_BTYPE_BUT,
                      1,
                      ICON_HANDLE_VECTOR,
                      0,
                      UI_UNIT_Y,
                      UI_UNIT_X,
                      UI_UNIT_Y,
                      nullptr,
                      0.0,
                      0.0,
                      0.0,
                      0.0,
                      TIP_("Vector Handle"));
    UI_but_func_set(bt, curvemap_tools_handle_vector, cumap, nullptr);
    if (cmp->flag & CUMA_HANDLE_VECTOR) {
      bt->flag |= UI_SELECT_DRAW;
    }

    bt = uiDefIconBut(block,
                      UI_BTYPE_BUT,
                      1,
                      ICON_HANDLE_AUTOCLAMPED,
                      0,
                      UI_UNIT_Y,
                      UI_UNIT_X,
                      UI_UNIT_Y,
                      nullptr,
                      0.0,
                      0.0,
                      0.0,
                      0.0,
                      TIP_("Auto Clamped"));
    UI_but_func_set(bt, curvemap_tools_handle_auto_clamped, cumap, nullptr);
    if (cmp->flag & CUMA_HANDLE_AUTO_ANIM) {
      bt->flag |= UI_SELECT_DRAW;
    }

    /* Curve handle position */
    UI_block_funcN_set(block, curvemap_buttons_update, MEM_dupallocN(cb), cumap);
    bt = uiDefButF(block,
                   UI_BTYPE_NUM,
                   0,
                   "X:",
                   0,
                   2 * UI_UNIT_Y,
                   UI_UNIT_X * 10,
                   UI_UNIT_Y,
                   &cmp->x,
                   bounds.xmin,
                   bounds.xmax,
                   0,
                   0,
                   "");
    UI_but_number_step_size_set(bt, 1);
    UI_but_number_precision_set(bt, 5);
    bt = uiDefButF(block,
                   UI_BTYPE_NUM,
                   0,
                   "Y:",
                   0,
                   1 * UI_UNIT_Y,
                   UI_UNIT_X * 10,
                   UI_UNIT_Y,
                   &cmp->y,
                   bounds.ymin,
                   bounds.ymax,
                   0,
                   0,
                   "");
    UI_but_number_step_size_set(bt, 1);
    UI_but_number_precision_set(bt, 5);

    /* Curve handle delete point */
    bt = uiDefIconBut(block,
                      UI_BTYPE_BUT,
                      0,
                      ICON_X,
                      0,
                      0,
                      dx,
                      dx,
                      nullptr,
                      0.0,
                      0.0,
                      0.0,
                      0.0,
                      TIP_("Delete points"));
    UI_but_funcN_set(bt, curvemap_buttons_delete, MEM_dupallocN(cb), cumap);
    if (point_last_or_first) {
      UI_but_flag_enable(bt, UI_BUT_DISABLED);
    }
  }

  /* black/white levels */
  if (levels) {
    uiLayout *split = uiLayoutSplit(layout, 0.0f, false);
    uiItemR(
        uiLayoutColumn(split, false), ptr, "black_level", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
    uiItemR(
        uiLayoutColumn(split, false), ptr, "white_level", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

    uiLayoutRow(layout, false);
    bt = uiDefBut(block,
                  UI_BTYPE_BUT,
                  0,
                  IFACE_("Reset"),
                  0,
                  0,
                  UI_UNIT_X * 10,
                  UI_UNIT_Y,
                  nullptr,
                  0.0f,
                  0.0f,
                  0,
                  0,
                  TIP_("Reset Black/White point and curves"));
    UI_but_funcN_set(bt, curvemap_buttons_reset, MEM_dupallocN(cb), cumap);
  }

  UI_block_funcN_set(block, nullptr, nullptr, nullptr);
}

void uiTemplateCurveMapping(uiLayout *layout,
                            PointerRNA *ptr,
                            const char *propname,
                            int type,
                            bool levels,
                            bool brush,
                            bool neg_slope,
                            bool tone)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
  uiBlock *block = uiLayoutGetBlock(layout);

  if (!prop) {
    RNA_warning("curve property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  if (RNA_property_type(prop) != PROP_POINTER) {
    RNA_warning("curve is not a pointer: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  PointerRNA cptr = RNA_property_pointer_get(ptr, prop);
  if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_CurveMapping)) {
    return;
  }

  RNAUpdateCb *cb = MEM_cnew<RNAUpdateCb>("RNAUpdateCb");
  cb->ptr = *ptr;
  cb->prop = prop;

  ID *id = cptr.owner_id;
  UI_block_lock_set(block, (id && ID_IS_LINKED(id)), ERROR_LIBDATA_MESSAGE);

  curvemap_buttons_layout(layout, &cptr, type, levels, brush, neg_slope, tone, cb);

  UI_block_lock_clear(block);

  MEM_freeN(cb);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve Profile Template
 * \{ */

static void CurveProfile_presets_dofunc(bContext *C, void *profile_v, int event)
{
  CurveProfile *profile = static_cast<CurveProfile *>(profile_v);

  profile->preset = event;
  BKE_curveprofile_reset(profile);
  BKE_curveprofile_update(profile, PROF_UPDATE_NONE);

  ED_undo_push(C, "CurveProfile tools");
  ED_region_tag_redraw(CTX_wm_region(C));
}

static uiBlock *CurveProfile_presets_func(bContext *C, ARegion *region, CurveProfile *profile)
{
  short yco = 0;

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  UI_block_func_butmenu_set(block, CurveProfile_presets_dofunc, profile);

  uiDefIconTextBut(block,
                   UI_BTYPE_BUT_MENU,
                   1,
                   ICON_BLANK1,
                   IFACE_("Default"),
                   0,
                   yco -= UI_UNIT_Y,
                   0,
                   UI_UNIT_Y,
                   nullptr,
                   0.0,
                   0.0,
                   0,
                   PROF_PRESET_LINE,
                   "");
  uiDefIconTextBut(block,
                   UI_BTYPE_BUT_MENU,
                   1,
                   ICON_BLANK1,
                   IFACE_("Support Loops"),
                   0,
                   yco -= UI_UNIT_Y,
                   0,
                   UI_UNIT_Y,
                   nullptr,
                   0.0,
                   0.0,
                   0,
                   PROF_PRESET_SUPPORTS,
                   "");
  uiDefIconTextBut(block,
                   UI_BTYPE_BUT_MENU,
                   1,
                   ICON_BLANK1,
                   IFACE_("Cornice Molding"),
                   0,
                   yco -= UI_UNIT_Y,
                   0,
                   UI_UNIT_Y,
                   nullptr,
                   0.0,
                   0.0,
                   0,
                   PROF_PRESET_CORNICE,
                   "");
  uiDefIconTextBut(block,
                   UI_BTYPE_BUT_MENU,
                   1,
                   ICON_BLANK1,
                   IFACE_("Crown Molding"),
                   0,
                   yco -= UI_UNIT_Y,
                   0,
                   UI_UNIT_Y,
                   nullptr,
                   0.0,
                   0.0,
                   0,
                   PROF_PRESET_CROWN,
                   "");
  uiDefIconTextBut(block,
                   UI_BTYPE_BUT_MENU,
                   1,
                   ICON_BLANK1,
                   IFACE_("Steps"),
                   0,
                   yco -= UI_UNIT_Y,
                   0,
                   UI_UNIT_Y,
                   nullptr,
                   0.0,
                   0.0,
                   0,
                   PROF_PRESET_STEPS,
                   "");

  UI_block_direction_set(block, UI_DIR_DOWN);
  UI_block_bounds_set_text(block, int(3.0f * UI_UNIT_X));

  return block;
}

static uiBlock *CurveProfile_buttons_presets(bContext *C, ARegion *region, void *profile_v)
{
  return CurveProfile_presets_func(C, region, (CurveProfile *)profile_v);
}

/* Only for CurveProfile tools block */
enum {
  UIPROFILE_FUNC_RESET,
  UIPROFILE_FUNC_RESET_VIEW,
};

static void CurveProfile_tools_dofunc(bContext *C, void *profile_v, int event)
{
  CurveProfile *profile = static_cast<CurveProfile *>(profile_v);

  switch (event) {
    case UIPROFILE_FUNC_RESET: /* reset */
      BKE_curveprofile_reset(profile);
      BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
      break;
    case UIPROFILE_FUNC_RESET_VIEW: /* reset view to clipping rect */
      BKE_curveprofile_reset_view(profile);
      break;
  }
  ED_undo_push(C, "CurveProfile tools");
  ED_region_tag_redraw(CTX_wm_region(C));
}

static uiBlock *CurveProfile_tools_func(bContext *C, ARegion *region, CurveProfile *profile)
{
  short yco = 0;

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  UI_block_func_butmenu_set(block, CurveProfile_tools_dofunc, profile);

  uiDefIconTextBut(block,
                   UI_BTYPE_BUT_MENU,
                   1,
                   ICON_BLANK1,
                   IFACE_("Reset View"),
                   0,
                   yco -= UI_UNIT_Y,
                   0,
                   UI_UNIT_Y,
                   nullptr,
                   0.0,
                   0.0,
                   0,
                   UIPROFILE_FUNC_RESET_VIEW,
                   "");
  uiDefIconTextBut(block,
                   UI_BTYPE_BUT_MENU,
                   1,
                   ICON_BLANK1,
                   IFACE_("Reset Curve"),
                   0,
                   yco -= UI_UNIT_Y,
                   0,
                   UI_UNIT_Y,
                   nullptr,
                   0.0,
                   0.0,
                   0,
                   UIPROFILE_FUNC_RESET,
                   "");

  UI_block_direction_set(block, UI_DIR_DOWN);
  UI_block_bounds_set_text(block, int(3.0f * UI_UNIT_X));

  return block;
}

static uiBlock *CurveProfile_buttons_tools(bContext *C, ARegion *region, void *profile_v)
{
  return CurveProfile_tools_func(C, region, (CurveProfile *)profile_v);
}

static bool CurveProfile_can_zoom_in(CurveProfile *profile)
{
  return BLI_rctf_size_x(&profile->view_rect) >
         CURVE_ZOOM_MAX * BLI_rctf_size_x(&profile->clip_rect);
}

static bool CurveProfile_can_zoom_out(CurveProfile *profile)
{
  return BLI_rctf_size_x(&profile->view_rect) < BLI_rctf_size_x(&profile->clip_rect);
}

static void CurveProfile_buttons_zoom_in(bContext *C, void *profile_v, void * /*arg*/)
{
  CurveProfile *profile = static_cast<CurveProfile *>(profile_v);

  if (CurveProfile_can_zoom_in(profile)) {
    const float dx = 0.1154f * BLI_rctf_size_x(&profile->view_rect);
    profile->view_rect.xmin += dx;
    profile->view_rect.xmax -= dx;
    const float dy = 0.1154f * BLI_rctf_size_y(&profile->view_rect);
    profile->view_rect.ymin += dy;
    profile->view_rect.ymax -= dy;
  }

  ED_region_tag_redraw(CTX_wm_region(C));
}

static void CurveProfile_buttons_zoom_out(bContext *C, void *profile_v, void * /*arg*/)
{
  CurveProfile *profile = static_cast<CurveProfile *>(profile_v);

  if (CurveProfile_can_zoom_out(profile)) {
    float d = 0.15f * BLI_rctf_size_x(&profile->view_rect);
    float d1 = d;

    if (profile->flag & PROF_USE_CLIP) {
      if (profile->view_rect.xmin - d < profile->clip_rect.xmin) {
        d1 = profile->view_rect.xmin - profile->clip_rect.xmin;
      }
    }
    profile->view_rect.xmin -= d1;

    d1 = d;
    if (profile->flag & PROF_USE_CLIP) {
      if (profile->view_rect.xmax + d > profile->clip_rect.xmax) {
        d1 = -profile->view_rect.xmax + profile->clip_rect.xmax;
      }
    }
    profile->view_rect.xmax += d1;

    d = d1 = 0.15f * BLI_rctf_size_y(&profile->view_rect);

    if (profile->flag & PROF_USE_CLIP) {
      if (profile->view_rect.ymin - d < profile->clip_rect.ymin) {
        d1 = profile->view_rect.ymin - profile->clip_rect.ymin;
      }
    }
    profile->view_rect.ymin -= d1;

    d1 = d;
    if (profile->flag & PROF_USE_CLIP) {
      if (profile->view_rect.ymax + d > profile->clip_rect.ymax) {
        d1 = -profile->view_rect.ymax + profile->clip_rect.ymax;
      }
    }
    profile->view_rect.ymax += d1;
  }

  ED_region_tag_redraw(CTX_wm_region(C));
}

static void CurveProfile_clipping_toggle(bContext *C, void *cb_v, void *profile_v)
{
  CurveProfile *profile = static_cast<CurveProfile *>(profile_v);

  profile->flag ^= PROF_USE_CLIP;

  BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
  rna_update_cb(C, cb_v, nullptr);
}

static void CurveProfile_buttons_reverse(bContext *C, void *cb_v, void *profile_v)
{
  CurveProfile *profile = static_cast<CurveProfile *>(profile_v);

  BKE_curveprofile_reverse(profile);
  BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
  rna_update_cb(C, cb_v, nullptr);
}

static void CurveProfile_buttons_delete(bContext *C, void *cb_v, void *profile_v)
{
  CurveProfile *profile = static_cast<CurveProfile *>(profile_v);

  BKE_curveprofile_remove_by_flag(profile, SELECT);
  BKE_curveprofile_update(profile, PROF_UPDATE_NONE);

  rna_update_cb(C, cb_v, nullptr);
}

static void CurveProfile_buttons_update(bContext *C, void *arg1_v, void *profile_v)
{
  CurveProfile *profile = static_cast<CurveProfile *>(profile_v);
  BKE_curveprofile_update(profile, PROF_UPDATE_REMOVE_DOUBLES | PROF_UPDATE_CLIP);
  rna_update_cb(C, arg1_v, nullptr);
}

static void CurveProfile_buttons_reset(bContext *C, void *arg1_v, void *profile_v)
{
  CurveProfile *profile = static_cast<CurveProfile *>(profile_v);
  BKE_curveprofile_reset(profile);
  BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
  rna_update_cb(C, arg1_v, nullptr);
}

static void CurveProfile_buttons_layout(uiLayout *layout, PointerRNA *ptr, RNAUpdateCb *cb)
{
  CurveProfile *profile = static_cast<CurveProfile *>(ptr->data);
  uiBut *bt;

  uiBlock *block = uiLayoutGetBlock(layout);

  UI_block_emboss_set(block, UI_EMBOSS);

  uiLayoutSetPropSep(layout, false);

  /* Preset selector */
  /* There is probably potential to use simpler "uiItemR" functions here, but automatic updating
   * after a preset is selected would be more complicated. */
  uiLayout *row = uiLayoutRow(layout, true);
  bt = uiDefBlockBut(
      block, CurveProfile_buttons_presets, profile, "Preset", 0, 0, UI_UNIT_X, UI_UNIT_X, "");
  UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), nullptr);

  /* Show a "re-apply" preset button when it has been changed from the preset. */
  if (profile->flag & PROF_DIRTY_PRESET) {
    /* Only for dynamic presets. */
    if (ELEM(profile->preset, PROF_PRESET_STEPS, PROF_PRESET_SUPPORTS)) {
      bt = uiDefIconTextBut(block,
                            UI_BTYPE_BUT,
                            0,
                            ICON_NONE,
                            "Apply Preset",
                            0,
                            0,
                            UI_UNIT_X,
                            UI_UNIT_X,
                            nullptr,
                            0.0,
                            0.0,
                            0.0,
                            0.0,
                            TIP_("Reapply and update the preset, removing changes"));
      UI_but_funcN_set(bt, CurveProfile_buttons_reset, MEM_dupallocN(cb), profile);
    }
  }

  row = uiLayoutRow(layout, false);

  /* (Left aligned) */
  uiLayout *sub = uiLayoutRow(row, true);
  uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);

  /* Zoom in */
  bt = uiDefIconBut(block,
                    UI_BTYPE_BUT,
                    0,
                    ICON_ZOOM_IN,
                    0,
                    0,
                    UI_UNIT_X,
                    UI_UNIT_X,
                    nullptr,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    TIP_("Zoom in"));
  UI_but_func_set(bt, CurveProfile_buttons_zoom_in, profile, nullptr);
  if (!CurveProfile_can_zoom_in(profile)) {
    UI_but_disable(bt, "");
  }

  /* Zoom out */
  bt = uiDefIconBut(block,
                    UI_BTYPE_BUT,
                    0,
                    ICON_ZOOM_OUT,
                    0,
                    0,
                    UI_UNIT_X,
                    UI_UNIT_X,
                    nullptr,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    TIP_("Zoom out"));
  UI_but_func_set(bt, CurveProfile_buttons_zoom_out, profile, nullptr);
  if (!CurveProfile_can_zoom_out(profile)) {
    UI_but_disable(bt, "");
  }

  /* (Right aligned) */
  sub = uiLayoutRow(row, true);
  uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_RIGHT);

  /* Flip path */
  bt = uiDefIconBut(block,
                    UI_BTYPE_BUT,
                    0,
                    ICON_ARROW_LEFTRIGHT,
                    0,
                    0,
                    UI_UNIT_X,
                    UI_UNIT_X,
                    nullptr,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    TIP_("Reverse Path"));
  UI_but_funcN_set(bt, CurveProfile_buttons_reverse, MEM_dupallocN(cb), profile);

  /* Clipping toggle */
  const int icon = (profile->flag & PROF_USE_CLIP) ? ICON_CLIPUV_HLT : ICON_CLIPUV_DEHLT;
  bt = uiDefIconBut(block,
                    UI_BTYPE_BUT,
                    0,
                    icon,
                    0,
                    0,
                    UI_UNIT_X,
                    UI_UNIT_X,
                    nullptr,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    TIP_("Toggle Profile Clipping"));
  UI_but_funcN_set(bt, CurveProfile_clipping_toggle, MEM_dupallocN(cb), profile);

  /* Reset view, reset curve */
  bt = uiDefIconBlockBut(
      block, CurveProfile_buttons_tools, profile, 0, 0, 0, 0, UI_UNIT_X, UI_UNIT_X, TIP_("Tools"));
  UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), nullptr);

  UI_block_funcN_set(block, rna_update_cb, MEM_dupallocN(cb), nullptr);

  /* The path itself */
  int path_width = max_ii(uiLayoutGetWidth(layout), UI_UNIT_X);
  path_width = min_ii(path_width, int(16.0f * UI_UNIT_X));
  const int path_height = path_width;
  uiLayoutRow(layout, false);
  uiDefBut(block,
           UI_BTYPE_CURVEPROFILE,
           0,
           "",
           0,
           0,
           short(path_width),
           short(path_height),
           profile,
           0.0f,
           1.0f,
           -1,
           0,
           "");

  /* Position sliders for (first) selected point */
  int i;
  float *selection_x, *selection_y;
  bool point_last_or_first = false;
  CurveProfilePoint *point = nullptr;
  for (i = 0; i < profile->path_len; i++) {
    if (profile->path[i].flag & PROF_SELECT) {
      point = &profile->path[i];
      selection_x = &point->x;
      selection_y = &point->y;
      break;
    }
    if (profile->path[i].flag & PROF_H1_SELECT) {
      point = &profile->path[i];
      selection_x = &point->h1_loc[0];
      selection_y = &point->h1_loc[1];
    }
    else if (profile->path[i].flag & PROF_H2_SELECT) {
      point = &profile->path[i];
      selection_x = &point->h2_loc[0];
      selection_y = &point->h2_loc[1];
    }
  }
  if (ELEM(i, 0, profile->path_len - 1)) {
    point_last_or_first = true;
  }

  /* Selected point data */
  rctf bounds;
  if (point) {
    if (profile->flag & PROF_USE_CLIP) {
      bounds = profile->clip_rect;
    }
    else {
      bounds.xmin = bounds.ymin = -1000.0;
      bounds.xmax = bounds.ymax = 1000.0;
    }

    row = uiLayoutRow(layout, true);

    PointerRNA point_ptr;
    RNA_pointer_create(ptr->owner_id, &RNA_CurveProfilePoint, point, &point_ptr);
    PropertyRNA *prop_handle_type = RNA_struct_find_property(&point_ptr, "handle_type_1");
    uiItemFullR(row,
                &point_ptr,
                prop_handle_type,
                RNA_NO_INDEX,
                0,
                UI_ITEM_R_EXPAND | UI_ITEM_R_ICON_ONLY,
                "",
                ICON_NONE);

    /* Position */
    bt = uiDefButF(block,
                   UI_BTYPE_NUM,
                   0,
                   "X:",
                   0,
                   2 * UI_UNIT_Y,
                   UI_UNIT_X * 10,
                   UI_UNIT_Y,
                   selection_x,
                   bounds.xmin,
                   bounds.xmax,
                   0,
                   0,
                   "");
    UI_but_number_step_size_set(bt, 1);
    UI_but_number_precision_set(bt, 5);
    UI_but_funcN_set(bt, CurveProfile_buttons_update, MEM_dupallocN(cb), profile);
    if (point_last_or_first) {
      UI_but_flag_enable(bt, UI_BUT_DISABLED);
    }
    bt = uiDefButF(block,
                   UI_BTYPE_NUM,
                   0,
                   "Y:",
                   0,
                   1 * UI_UNIT_Y,
                   UI_UNIT_X * 10,
                   UI_UNIT_Y,
                   selection_y,
                   bounds.ymin,
                   bounds.ymax,
                   0,
                   0,
                   "");
    UI_but_number_step_size_set(bt, 1);
    UI_but_number_precision_set(bt, 5);
    UI_but_funcN_set(bt, CurveProfile_buttons_update, MEM_dupallocN(cb), profile);
    if (point_last_or_first) {
      UI_but_flag_enable(bt, UI_BUT_DISABLED);
    }

    /* Delete points */
    bt = uiDefIconBut(block,
                      UI_BTYPE_BUT,
                      0,
                      ICON_X,
                      0,
                      0,
                      UI_UNIT_X,
                      UI_UNIT_X,
                      nullptr,
                      0.0,
                      0.0,
                      0.0,
                      0.0,
                      TIP_("Delete points"));
    UI_but_funcN_set(bt, CurveProfile_buttons_delete, MEM_dupallocN(cb), profile);
    if (point_last_or_first) {
      UI_but_flag_enable(bt, UI_BUT_DISABLED);
    }
  }

  uiItemR(layout, ptr, "use_sample_straight_edges", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_sample_even_lengths", 0, nullptr, ICON_NONE);

  UI_block_funcN_set(block, nullptr, nullptr, nullptr);
}

void uiTemplateCurveProfile(uiLayout *layout, PointerRNA *ptr, const char *propname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  uiBlock *block = uiLayoutGetBlock(layout);

  if (!prop) {
    RNA_warning(
        "Curve Profile property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  if (RNA_property_type(prop) != PROP_POINTER) {
    RNA_warning(
        "Curve Profile is not a pointer: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  PointerRNA cptr = RNA_property_pointer_get(ptr, prop);
  if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_CurveProfile)) {
    return;
  }

  /* Share update functionality with the CurveMapping widget template. */
  RNAUpdateCb *cb = MEM_cnew<RNAUpdateCb>("RNAUpdateCb");
  cb->ptr = *ptr;
  cb->prop = prop;

  ID *id = cptr.owner_id;
  UI_block_lock_set(block, (id && ID_IS_LINKED(id)), ERROR_LIBDATA_MESSAGE);

  CurveProfile_buttons_layout(layout, &cptr, cb);

  UI_block_lock_clear(block);

  MEM_freeN(cb);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ColorPicker Template
 * \{ */

#define WHEEL_SIZE (5 * U.widget_unit)

void uiTemplateColorPicker(uiLayout *layout,
                           PointerRNA *ptr,
                           const char *propname,
                           bool value_slider,
                           bool lock,
                           bool lock_luminosity,
                           bool cubic)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
  uiBlock *block = uiLayoutGetBlock(layout);
  ColorPicker *cpicker = ui_block_colorpicker_create(block);

  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  float softmin, softmax, step, precision;
  RNA_property_float_ui_range(ptr, prop, &softmin, &softmax, &step, &precision);

  uiLayout *col = uiLayoutColumn(layout, true);
  uiLayout *row = uiLayoutRow(col, true);

  uiBut *but = nullptr;
  uiButHSVCube *hsv_but;
  switch (U.color_picker_type) {
    case USER_CP_SQUARE_SV:
    case USER_CP_SQUARE_HS:
    case USER_CP_SQUARE_HV:
      hsv_but = (uiButHSVCube *)uiDefButR_prop(block,
                                               UI_BTYPE_HSVCUBE,
                                               0,
                                               "",
                                               0,
                                               0,
                                               WHEEL_SIZE,
                                               WHEEL_SIZE,
                                               ptr,
                                               prop,
                                               -1,
                                               0.0,
                                               0.0,
                                               0,
                                               0,
                                               "");
      switch (U.color_picker_type) {
        case USER_CP_SQUARE_SV:
          hsv_but->gradient_type = UI_GRAD_SV;
          break;
        case USER_CP_SQUARE_HS:
          hsv_but->gradient_type = UI_GRAD_HS;
          break;
        case USER_CP_SQUARE_HV:
          hsv_but->gradient_type = UI_GRAD_HV;
          break;
      }
      but = &hsv_but->but;
      break;

    /* user default */
    case USER_CP_CIRCLE_HSV:
    case USER_CP_CIRCLE_HSL:
    default:
      but = uiDefButR_prop(block,
                           UI_BTYPE_HSVCIRCLE,
                           0,
                           "",
                           0,
                           0,
                           WHEEL_SIZE,
                           WHEEL_SIZE,
                           ptr,
                           prop,
                           -1,
                           0.0,
                           0.0,
                           0,
                           0,
                           "");
      break;
  }

  but->custom_data = cpicker;

  cpicker->use_color_lock = lock;
  cpicker->use_color_cubic = cubic;
  cpicker->use_luminosity_lock = lock_luminosity;

  if (lock_luminosity) {
    float color[4]; /* in case of alpha */
    RNA_property_float_get_array(ptr, prop, color);
    but->a2 = len_v3(color);
    cpicker->luminosity_lock_value = len_v3(color);
  }

  if (value_slider) {
    switch (U.color_picker_type) {
      case USER_CP_CIRCLE_HSL:
        uiItemS(row);
        hsv_but = (uiButHSVCube *)uiDefButR_prop(block,
                                                 UI_BTYPE_HSVCUBE,
                                                 0,
                                                 "",
                                                 WHEEL_SIZE + 6,
                                                 0,
                                                 14 * UI_DPI_FAC,
                                                 WHEEL_SIZE,
                                                 ptr,
                                                 prop,
                                                 -1,
                                                 softmin,
                                                 softmax,
                                                 0,
                                                 0,
                                                 "");
        hsv_but->gradient_type = UI_GRAD_L_ALT;
        break;
      case USER_CP_SQUARE_SV:
        uiItemS(col);
        hsv_but = (uiButHSVCube *)uiDefButR_prop(block,
                                                 UI_BTYPE_HSVCUBE,
                                                 0,
                                                 "",
                                                 0,
                                                 4,
                                                 WHEEL_SIZE,
                                                 18 * UI_DPI_FAC,
                                                 ptr,
                                                 prop,
                                                 -1,
                                                 softmin,
                                                 softmax,
                                                 0,
                                                 0,
                                                 "");
        hsv_but->gradient_type = eButGradientType(UI_GRAD_SV + 3);
        break;
      case USER_CP_SQUARE_HS:
        uiItemS(col);
        hsv_but = (uiButHSVCube *)uiDefButR_prop(block,
                                                 UI_BTYPE_HSVCUBE,
                                                 0,
                                                 "",
                                                 0,
                                                 4,
                                                 WHEEL_SIZE,
                                                 18 * UI_DPI_FAC,
                                                 ptr,
                                                 prop,
                                                 -1,
                                                 softmin,
                                                 softmax,
                                                 0,
                                                 0,
                                                 "");
        hsv_but->gradient_type = eButGradientType(UI_GRAD_HS + 3);
        break;
      case USER_CP_SQUARE_HV:
        uiItemS(col);
        hsv_but = (uiButHSVCube *)uiDefButR_prop(block,
                                                 UI_BTYPE_HSVCUBE,
                                                 0,
                                                 "",
                                                 0,
                                                 4,
                                                 WHEEL_SIZE,
                                                 18 * UI_DPI_FAC,
                                                 ptr,
                                                 prop,
                                                 -1,
                                                 softmin,
                                                 softmax,
                                                 0,
                                                 0,
                                                 "");
        hsv_but->gradient_type = eButGradientType(UI_GRAD_HV + 3);
        break;

      /* user default */
      case USER_CP_CIRCLE_HSV:
      default:
        uiItemS(row);
        hsv_but = (uiButHSVCube *)uiDefButR_prop(block,
                                                 UI_BTYPE_HSVCUBE,
                                                 0,
                                                 "",
                                                 WHEEL_SIZE + 6,
                                                 0,
                                                 14 * UI_DPI_FAC,
                                                 WHEEL_SIZE,
                                                 ptr,
                                                 prop,
                                                 -1,
                                                 softmin,
                                                 softmax,
                                                 0,
                                                 0,
                                                 "");
        hsv_but->gradient_type = UI_GRAD_V_ALT;
        break;
    }

    hsv_but->but.custom_data = cpicker;
  }
}

static void ui_template_palette_menu(bContext * /*C*/, uiLayout *layout, void * /*but_p*/)
{
  uiLayout *row;

  uiItemL(layout, IFACE_("Sort By:"), ICON_NONE);
  row = uiLayoutRow(layout, false);
  uiItemEnumO_value(row, IFACE_("Hue"), ICON_NONE, "PALETTE_OT_sort", "type", 1);
  row = uiLayoutRow(layout, false);
  uiItemEnumO_value(row, IFACE_("Saturation"), ICON_NONE, "PALETTE_OT_sort", "type", 2);
  row = uiLayoutRow(layout, false);
  uiItemEnumO_value(row, IFACE_("Value"), ICON_NONE, "PALETTE_OT_sort", "type", 3);
  row = uiLayoutRow(layout, false);
  uiItemEnumO_value(row, IFACE_("Luminance"), ICON_NONE, "PALETTE_OT_sort", "type", 4);
}

void uiTemplatePalette(uiLayout *layout, PointerRNA *ptr, const char *propname, bool /*colors*/)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
  uiBut *but = nullptr;

  const int cols_per_row = MAX2(uiLayoutGetWidth(layout) / UI_UNIT_X, 1);

  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  const PointerRNA cptr = RNA_property_pointer_get(ptr, prop);
  if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_Palette)) {
    return;
  }

  uiBlock *block = uiLayoutGetBlock(layout);

  Palette *palette = static_cast<Palette *>(cptr.data);

  uiLayout *col = uiLayoutColumn(layout, true);
  uiLayoutRow(col, true);
  uiDefIconButO(block,
                UI_BTYPE_BUT,
                "PALETTE_OT_color_add",
                WM_OP_INVOKE_DEFAULT,
                ICON_ADD,
                0,
                0,
                UI_UNIT_X,
                UI_UNIT_Y,
                nullptr);
  uiDefIconButO(block,
                UI_BTYPE_BUT,
                "PALETTE_OT_color_delete",
                WM_OP_INVOKE_DEFAULT,
                ICON_REMOVE,
                0,
                0,
                UI_UNIT_X,
                UI_UNIT_Y,
                nullptr);
  if (palette->colors.first != nullptr) {
    but = uiDefIconButO(block,
                        UI_BTYPE_BUT,
                        "PALETTE_OT_color_move",
                        WM_OP_INVOKE_DEFAULT,
                        ICON_TRIA_UP,
                        0,
                        0,
                        UI_UNIT_X,
                        UI_UNIT_Y,
                        nullptr);
    UI_but_operator_ptr_get(but);
    RNA_enum_set(but->opptr, "type", -1);

    but = uiDefIconButO(block,
                        UI_BTYPE_BUT,
                        "PALETTE_OT_color_move",
                        WM_OP_INVOKE_DEFAULT,
                        ICON_TRIA_DOWN,
                        0,
                        0,
                        UI_UNIT_X,
                        UI_UNIT_Y,
                        nullptr);
    UI_but_operator_ptr_get(but);
    RNA_enum_set(but->opptr, "type", 1);

    /* Menu. */
    uiDefIconMenuBut(
        block, ui_template_palette_menu, nullptr, ICON_SORTSIZE, 0, 0, UI_UNIT_X, UI_UNIT_Y, "");
  }

  col = uiLayoutColumn(layout, true);
  uiLayoutRow(col, true);

  int row_cols = 0, col_id = 0;
  LISTBASE_FOREACH (PaletteColor *, color, &palette->colors) {
    if (row_cols >= cols_per_row) {
      uiLayoutRow(col, true);
      row_cols = 0;
    }

    PointerRNA color_ptr;
    RNA_pointer_create(&palette->id, &RNA_PaletteColor, color, &color_ptr);
    uiButColor *color_but = (uiButColor *)uiDefButR(block,
                                                    UI_BTYPE_COLOR,
                                                    0,
                                                    "",
                                                    0,
                                                    0,
                                                    UI_UNIT_X,
                                                    UI_UNIT_Y,
                                                    &color_ptr,
                                                    "color",
                                                    -1,
                                                    0.0,
                                                    1.0,
                                                    0.0,
                                                    0.0,
                                                    "");
    color_but->is_pallete_color = true;
    color_but->palette_color_index = col_id;
    row_cols++;
    col_id++;
  }
}

void uiTemplateCryptoPicker(uiLayout *layout, PointerRNA *ptr, const char *propname, int icon)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  uiBlock *block = uiLayoutGetBlock(layout);

  uiBut *but = uiDefIconTextButO(block,
                                 UI_BTYPE_BUT,
                                 "UI_OT_eyedropper_color",
                                 WM_OP_INVOKE_DEFAULT,
                                 icon,
                                 "",
                                 0,
                                 0,
                                 UI_UNIT_X,
                                 UI_UNIT_Y,
                                 RNA_property_ui_description(prop));
  but->rnapoin = *ptr;
  but->rnaprop = prop;
  but->rnaindex = -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Layer Buttons Template
 * \{ */

static void handle_layer_buttons(bContext *C, void *arg1, void *arg2)
{
  uiBut *but = static_cast<uiBut *>(arg1);
  const int cur = POINTER_AS_INT(arg2);
  wmWindow *win = CTX_wm_window(C);
  const bool shift = win->eventstate->modifier & KM_SHIFT;

  if (!shift) {
    const int tot = RNA_property_array_length(&but->rnapoin, but->rnaprop);

    /* Normally clicking only selects one layer */
    RNA_property_boolean_set_index(&but->rnapoin, but->rnaprop, cur, true);
    for (int i = 0; i < tot; i++) {
      if (i != cur) {
        RNA_property_boolean_set_index(&but->rnapoin, but->rnaprop, i, false);
      }
    }
  }

  /* view3d layer change should update depsgraph (invisible object changed maybe) */
  /* see view3d_header.c */
}

void uiTemplateLayers(uiLayout *layout,
                      PointerRNA *ptr,
                      const char *propname,
                      PointerRNA *used_ptr,
                      const char *used_propname,
                      int active_layer)
{
  const int cols_per_group = 5;

  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
  if (!prop) {
    RNA_warning("layers property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  /* the number of layers determines the way we group them
   * - we want 2 rows only (for now)
   * - The number of columns (cols) is the total number of buttons per row the 'remainder'
   *   is added to this, as it will be ok to have first row slightly wider if need be.
   * - For now, only split into groups if group will have at least 5 items.
   */
  const int layers = RNA_property_array_length(ptr, prop);
  const int cols = (layers / 2) + (layers % 2);
  const int groups = ((cols / 2) < cols_per_group) ? (1) : (cols / cols_per_group);

  PropertyRNA *used_prop = nullptr;
  if (used_ptr && used_propname) {
    used_prop = RNA_struct_find_property(used_ptr, used_propname);
    if (!used_prop) {
      RNA_warning("used layers property not found: %s.%s",
                  RNA_struct_identifier(ptr->type),
                  used_propname);
      return;
    }

    if (RNA_property_array_length(used_ptr, used_prop) < layers) {
      used_prop = nullptr;
    }
  }

  /* layers are laid out going across rows, with the columns being divided into groups */

  for (int group = 0; group < groups; group++) {
    uiLayout *uCol = uiLayoutColumn(layout, true);

    for (int row = 0; row < 2; row++) {
      uiLayout *uRow = uiLayoutRow(uCol, true);
      uiBlock *block = uiLayoutGetBlock(uRow);
      int layer = groups * cols_per_group * row + cols_per_group * group;

      /* add layers as toggle buts */
      for (int col = 0; (col < cols_per_group) && (layer < layers); col++, layer++) {
        int icon = 0;
        const int butlay = 1 << layer;

        if (active_layer & butlay) {
          icon = ICON_LAYER_ACTIVE;
        }
        else if (used_prop && RNA_property_boolean_get_index(used_ptr, used_prop, layer)) {
          icon = ICON_LAYER_USED;
        }

        uiBut *but = uiDefAutoButR(
            block, ptr, prop, layer, "", icon, 0, 0, UI_UNIT_X / 2, UI_UNIT_Y / 2);
        UI_but_func_set(but, handle_layer_buttons, but, POINTER_FROM_INT(layer));
        but->type = UI_BTYPE_TOGGLE;
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Running Jobs Template
 * \{ */

#define B_STOPRENDER 1
#define B_STOPCAST 2
#define B_STOPANIM 3
#define B_STOPCOMPO 4
#define B_STOPSEQ 5
#define B_STOPCLIP 6
#define B_STOPFILE 7
#define B_STOPOTHER 8

static void do_running_jobs(bContext *C, void * /*arg*/, int event)
{
  switch (event) {
    case B_STOPRENDER:
      G.is_break = true;
      break;
    case B_STOPCAST:
      WM_jobs_stop(CTX_wm_manager(C), CTX_wm_screen(C), nullptr);
      break;
    case B_STOPANIM:
      WM_operator_name_call(C, "SCREEN_OT_animation_play", WM_OP_INVOKE_SCREEN, nullptr, nullptr);
      break;
    case B_STOPCOMPO:
      WM_jobs_stop(CTX_wm_manager(C), CTX_data_scene(C), nullptr);
      break;
    case B_STOPSEQ:
      WM_jobs_stop(CTX_wm_manager(C), CTX_data_scene(C), nullptr);
      break;
    case B_STOPCLIP:
      WM_jobs_stop(CTX_wm_manager(C), CTX_data_scene(C), nullptr);
      break;
    case B_STOPFILE:
      WM_jobs_stop(CTX_wm_manager(C), CTX_data_scene(C), nullptr);
      break;
    case B_STOPOTHER:
      G.is_break = true;
      break;
  }
}

struct ProgressTooltip_Store {
  wmWindowManager *wm;
  void *owner;
};

static char *progress_tooltip_func(bContext * /*C*/, void *argN, const char * /*tip*/)
{
  ProgressTooltip_Store *arg = static_cast<ProgressTooltip_Store *>(argN);
  wmWindowManager *wm = arg->wm;
  void *owner = arg->owner;

  const float progress = WM_jobs_progress(wm, owner);

  /* create tooltip text and associate it with the job */
  char elapsed_str[32];
  char remaining_str[32] = "Unknown";
  const double elapsed = PIL_check_seconds_timer() - WM_jobs_starttime(wm, owner);
  BLI_timecode_string_from_time_simple(elapsed_str, sizeof(elapsed_str), elapsed);

  if (progress) {
    const double remaining = (elapsed / double(progress)) - elapsed;
    BLI_timecode_string_from_time_simple(remaining_str, sizeof(remaining_str), remaining);
  }

  return BLI_sprintfN(
      "Time Remaining: %s\n"
      "Time Elapsed: %s",
      remaining_str,
      elapsed_str);
}

void uiTemplateRunningJobs(uiLayout *layout, bContext *C)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  ScrArea *area = CTX_wm_area(C);
  void *owner = nullptr;
  int handle_event, icon = 0;

  uiBlock *block = uiLayoutGetBlock(layout);
  UI_block_layout_set_current(block, layout);

  UI_block_func_handle_set(block, do_running_jobs, nullptr);

  /* another scene can be rendering too, for example via compositor */
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_ANY)) {
      handle_event = B_STOPOTHER;
      icon = ICON_NONE;
      owner = scene;
    }
    else {
      continue;
    }

    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_SEQ_BUILD_PROXY)) {
      handle_event = B_STOPSEQ;
      icon = ICON_SEQUENCE;
      owner = scene;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_SEQ_BUILD_PREVIEW)) {
      handle_event = B_STOPSEQ;
      icon = ICON_SEQUENCE;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_SEQ_DRAW_THUMBNAIL)) {
      handle_event = B_STOPSEQ;
      icon = ICON_SEQUENCE;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_CLIP_BUILD_PROXY)) {
      handle_event = B_STOPCLIP;
      icon = ICON_TRACKER;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_CLIP_PREFETCH)) {
      handle_event = B_STOPCLIP;
      icon = ICON_TRACKER;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_CLIP_TRACK_MARKERS)) {
      handle_event = B_STOPCLIP;
      icon = ICON_TRACKER;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_CLIP_SOLVE_CAMERA)) {
      handle_event = B_STOPCLIP;
      icon = ICON_TRACKER;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_FILESEL_READDIR)) {
      handle_event = B_STOPFILE;
      icon = ICON_FILEBROWSER;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_RENDER)) {
      handle_event = B_STOPRENDER;
      icon = ICON_SCENE;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_COMPOSITE)) {
      handle_event = B_STOPCOMPO;
      icon = ICON_RENDERLAYERS;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_OBJECT_BAKE_TEXTURE) ||
        WM_jobs_test(wm, scene, WM_JOB_TYPE_OBJECT_BAKE)) {
      /* Skip bake jobs in compositor to avoid compo header displaying
       * progress bar which is not being updated (bake jobs only need
       * to update NC_IMAGE context.
       */
      if (area->spacetype != SPACE_NODE) {
        handle_event = B_STOPOTHER;
        icon = ICON_IMAGE;
        break;
      }
      continue;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_DPAINT_BAKE)) {
      handle_event = B_STOPOTHER;
      icon = ICON_MOD_DYNAMICPAINT;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_POINTCACHE)) {
      handle_event = B_STOPOTHER;
      icon = ICON_PHYSICS;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_OBJECT_SIM_FLUID)) {
      handle_event = B_STOPOTHER;
      icon = ICON_MOD_FLUIDSIM;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_OBJECT_SIM_OCEAN)) {
      handle_event = B_STOPOTHER;
      icon = ICON_MOD_OCEAN;
      break;
    }
  }

  if (owner) {
    const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
    const bool active = !(G.is_break || WM_jobs_is_stopped(wm, owner));

    uiLayout *row = uiLayoutRow(layout, false);
    block = uiLayoutGetBlock(row);

    /* get percentage done and set it as the UI text */
    const float progress = WM_jobs_progress(wm, owner);
    char text[8];
    BLI_snprintf(text, 8, "%d%%", int(progress * 100));

    const char *name = active ? WM_jobs_name(wm, owner) : "Canceling...";

    /* job name and icon */
    const int textwidth = UI_fontstyle_string_width(fstyle, name);
    uiDefIconTextBut(block,
                     UI_BTYPE_LABEL,
                     0,
                     icon,
                     name,
                     0,
                     0,
                     textwidth + UI_UNIT_X * 1.5f,
                     UI_UNIT_Y,
                     nullptr,
                     0.0f,
                     0.0f,
                     0.0f,
                     0.0f,
                     "");

    /* stick progress bar and cancel button together */
    row = uiLayoutRow(layout, true);
    uiLayoutSetActive(row, active);
    block = uiLayoutGetBlock(row);

    {
      ProgressTooltip_Store *tip_arg = static_cast<ProgressTooltip_Store *>(
          MEM_mallocN(sizeof(*tip_arg), __func__));
      tip_arg->wm = wm;
      tip_arg->owner = owner;
      uiButProgressbar *but_progress = (uiButProgressbar *)uiDefIconTextBut(block,
                                                                            UI_BTYPE_PROGRESS_BAR,
                                                                            0,
                                                                            0,
                                                                            text,
                                                                            UI_UNIT_X,
                                                                            0,
                                                                            UI_UNIT_X * 6.0f,
                                                                            UI_UNIT_Y,
                                                                            nullptr,
                                                                            0.0f,
                                                                            0.0f,
                                                                            0.0f,
                                                                            0,
                                                                            nullptr);

      but_progress->progress = progress;
      UI_but_func_tooltip_set(&but_progress->but, progress_tooltip_func, tip_arg, MEM_freeN);
    }

    if (!wm->is_interface_locked) {
      uiDefIconTextBut(block,
                       UI_BTYPE_BUT,
                       handle_event,
                       ICON_PANEL_CLOSE,
                       "",
                       0,
                       0,
                       UI_UNIT_X,
                       UI_UNIT_Y,
                       nullptr,
                       0.0f,
                       0.0f,
                       0,
                       0,
                       TIP_("Stop this job"));
    }
  }

  if (ED_screen_animation_no_scrub(wm)) {
    uiDefIconTextBut(block,
                     UI_BTYPE_BUT,
                     B_STOPANIM,
                     ICON_CANCEL,
                     IFACE_("Anim Player"),
                     0,
                     0,
                     UI_UNIT_X * 5.0f,
                     UI_UNIT_Y,
                     nullptr,
                     0.0f,
                     0.0f,
                     0,
                     0,
                     TIP_("Stop animation playback"));
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reports for Last Operator Template
 * \{ */

void uiTemplateReportsBanner(uiLayout *layout, bContext *C)
{
  ReportList *reports = CTX_wm_reports(C);
  Report *report = BKE_reports_last_displayable(reports);
  const uiStyle *style = UI_style_get();

  uiBut *but;

  /* if the report display has timed out, don't show */
  if (!reports->reporttimer) {
    return;
  }

  ReportTimerInfo *rti = (ReportTimerInfo *)reports->reporttimer->customdata;

  if (!rti || rti->widthfac == 0.0f || !report) {
    return;
  }

  uiLayout *ui_abs = uiLayoutAbsolute(layout, false);
  uiBlock *block = uiLayoutGetBlock(ui_abs);
  eUIEmbossType previous_emboss = UI_block_emboss_get(block);

  UI_fontstyle_set(&style->widgetlabel);
  int width = BLF_width(style->widgetlabel.uifont_id, report->message, report->len);
  width = min_ii(int(rti->widthfac * width), width);
  width = max_ii(width, 10 * UI_DPI_FAC);

  UI_block_align_begin(block);

  /* Background for icon. */
  but = uiDefBut(block,
                 UI_BTYPE_ROUNDBOX,
                 0,
                 "",
                 0,
                 0,
                 UI_UNIT_X + (6 * UI_DPI_FAC),
                 UI_UNIT_Y,
                 nullptr,
                 0.0f,
                 0.0f,
                 0,
                 0,
                 "");
  /* UI_BTYPE_ROUNDBOX's bg color is set in but->col. */
  UI_GetThemeColorType4ubv(UI_icon_colorid_from_report_type(report->type), SPACE_INFO, but->col);

  /* Background for the rest of the message. */
  but = uiDefBut(block,
                 UI_BTYPE_ROUNDBOX,
                 0,
                 "",
                 UI_UNIT_X + (6 * UI_DPI_FAC),
                 0,
                 UI_UNIT_X + width,
                 UI_UNIT_Y,
                 nullptr,
                 0.0f,
                 0.0f,
                 0,
                 0,
                 "");

  /* Use icon background at low opacity to highlight, but still contrasting with area TH_TEXT. */
  UI_GetThemeColorType4ubv(UI_icon_colorid_from_report_type(report->type), SPACE_INFO, but->col);
  but->col[3] = 64;

  UI_block_align_end(block);
  UI_block_emboss_set(block, UI_EMBOSS_NONE);

  /* The report icon itself. */
  but = uiDefIconButO(block,
                      UI_BTYPE_BUT,
                      "SCREEN_OT_info_log_show",
                      WM_OP_INVOKE_REGION_WIN,
                      UI_icon_from_report_type(report->type),
                      (3 * UI_DPI_FAC),
                      0,
                      UI_UNIT_X,
                      UI_UNIT_Y,
                      TIP_("Click to see the remaining reports in text block: 'Recent Reports'"));
  UI_GetThemeColorType4ubv(UI_text_colorid_from_report_type(report->type), SPACE_INFO, but->col);
  but->col[3] = 255; /* This theme color is RBG only, so have to set alpha here. */

  /* The report message. */
  but = uiDefButO(block,
                  UI_BTYPE_BUT,
                  "SCREEN_OT_info_log_show",
                  WM_OP_INVOKE_REGION_WIN,
                  report->message,
                  UI_UNIT_X,
                  0,
                  width + UI_UNIT_X,
                  UI_UNIT_Y,
                  TIP_("Show in Info Log"));

  UI_block_emboss_set(block, previous_emboss);
}

void uiTemplateInputStatus(uiLayout *layout, bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  WorkSpace *workspace = CTX_wm_workspace(C);

  /* Workspace status text has priority. */
  if (workspace->status_text) {
    uiItemL(layout, workspace->status_text, ICON_NONE);
    return;
  }

  if (WM_window_modal_keymap_status_draw(C, win, layout)) {
    return;
  }

  /* Otherwise should cursor keymap status. */
  for (int i = 0; i < 3; i++) {
    uiLayout *box = uiLayoutRow(layout, false);
    uiLayout *col = uiLayoutColumn(box, false);
    uiLayout *row = uiLayoutRow(col, true);
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_LEFT);

    const char *msg = CTX_TIP_(BLT_I18NCONTEXT_OPERATOR_DEFAULT,
                               WM_window_cursor_keymap_status_get(win, i, 0));
    const char *msg_drag = CTX_TIP_(BLT_I18NCONTEXT_OPERATOR_DEFAULT,
                                    WM_window_cursor_keymap_status_get(win, i, 1));

    if (msg || (msg_drag == nullptr)) {
      uiItemL(row, msg ? msg : "", (ICON_MOUSE_LMB + i));
    }

    if (msg_drag) {
      uiItemL(row, msg_drag, (ICON_MOUSE_LMB_DRAG + i));
    }

    /* Use trick with empty string to keep icons in same position. */
    row = uiLayoutRow(col, false);
    uiItemL(row, "                                                                   ", ICON_NONE);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keymap Template
 * \{ */

static void keymap_item_modified(bContext * /*C*/, void *kmi_p, void * /*unused*/)
{
  wmKeyMapItem *kmi = (wmKeyMapItem *)kmi_p;
  WM_keyconfig_update_tag(nullptr, kmi);
}

static void template_keymap_item_properties(uiLayout *layout, const char *title, PointerRNA *ptr)
{
  uiItemS(layout);

  if (title) {
    uiItemL(layout, title, ICON_NONE);
  }

  uiLayout *flow = uiLayoutColumnFlow(layout, 2, false);

  RNA_STRUCT_BEGIN_SKIP_RNA_TYPE (ptr, prop) {
    const bool is_set = RNA_property_is_set(ptr, prop);
    uiBut *but;

    /* recurse for nested properties */
    if (RNA_property_type(prop) == PROP_POINTER) {
      PointerRNA propptr = RNA_property_pointer_get(ptr, prop);

      if (propptr.data && RNA_struct_is_a(propptr.type, &RNA_OperatorProperties)) {
        const char *name = RNA_property_ui_name(prop);
        template_keymap_item_properties(layout, name, &propptr);
        continue;
      }
    }

    uiLayout *box = uiLayoutBox(flow);
    uiLayoutSetActive(box, is_set);
    uiLayout *row = uiLayoutRow(box, false);

    /* property value */
    uiItemFullR(row, ptr, prop, -1, 0, 0, nullptr, ICON_NONE);

    if (is_set) {
      /* unset operator */
      uiBlock *block = uiLayoutGetBlock(row);
      UI_block_emboss_set(block, UI_EMBOSS_NONE);
      but = uiDefIconButO(block,
                          UI_BTYPE_BUT,
                          "UI_OT_unset_property_button",
                          WM_OP_EXEC_DEFAULT,
                          ICON_X,
                          0,
                          0,
                          UI_UNIT_X,
                          UI_UNIT_Y,
                          nullptr);
      but->rnapoin = *ptr;
      but->rnaprop = prop;
      UI_block_emboss_set(block, UI_EMBOSS);
    }
  }
  RNA_STRUCT_END;
}

void uiTemplateKeymapItemProperties(uiLayout *layout, PointerRNA *ptr)
{
  PointerRNA propptr = RNA_pointer_get(ptr, "properties");

  if (propptr.data) {
    uiBut *but = static_cast<uiBut *>(uiLayoutGetBlock(layout)->buttons.last);

    WM_operator_properties_sanitize(&propptr, false);
    template_keymap_item_properties(layout, nullptr, &propptr);

    /* attach callbacks to compensate for missing properties update,
     * we don't know which keymap (item) is being modified there */
    for (; but; but = but->next) {
      /* operator buttons may store props for use (file selector, T36492) */
      if (but->rnaprop) {
        UI_but_func_set(but, keymap_item_modified, ptr->data, nullptr);

        /* Otherwise the keymap will be re-generated which we're trying to edit,
         * see: T47685 */
        UI_but_flag_enable(but, UI_BUT_UPDATE_DELAY);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event Icon Template
 * \{ */

bool uiTemplateEventFromKeymapItem(uiLayout *layout,
                                   const char *text,
                                   const wmKeyMapItem *kmi,
                                   bool text_fallback)
{
  bool ok = false;

  int icon_mod[4];
#ifdef WITH_HEADLESS
  int icon = 0;
#else
  const int icon = UI_icon_from_keymap_item(kmi, icon_mod);
#endif
  if (icon != 0) {
    for (int j = 0; j < ARRAY_SIZE(icon_mod) && icon_mod[j]; j++) {
      uiItemL(layout, "", icon_mod[j]);
    }
    uiItemL(layout, CTX_TIP_(BLT_I18NCONTEXT_ID_WINDOWMANAGER, text), icon);
    ok = true;
  }
  else if (text_fallback) {
    const char *event_text = WM_key_event_string(kmi->type, true);
    uiItemL(layout, event_text, ICON_NONE);
    uiItemL(layout, CTX_TIP_(BLT_I18NCONTEXT_ID_WINDOWMANAGER, text), ICON_NONE);
    ok = true;
  }
  return ok;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Management Template
 * \{ */

void uiTemplateColorspaceSettings(uiLayout *layout, PointerRNA *ptr, const char *propname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    printf(
        "%s: property not found: %s.%s\n", __func__, RNA_struct_identifier(ptr->type), propname);
    return;
  }

  PointerRNA colorspace_settings_ptr = RNA_property_pointer_get(ptr, prop);

  uiItemR(layout, &colorspace_settings_ptr, "name", 0, IFACE_("Color Space"), ICON_NONE);
}

void uiTemplateColormanagedViewSettings(uiLayout *layout,
                                        bContext * /*C*/,
                                        PointerRNA *ptr,
                                        const char *propname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    printf(
        "%s: property not found: %s.%s\n", __func__, RNA_struct_identifier(ptr->type), propname);
    return;
  }

  PointerRNA view_transform_ptr = RNA_property_pointer_get(ptr, prop);
  ColorManagedViewSettings *view_settings = static_cast<ColorManagedViewSettings *>(
      view_transform_ptr.data);

  uiLayout *col = uiLayoutColumn(layout, false);
  uiItemR(col, &view_transform_ptr, "view_transform", 0, IFACE_("View"), ICON_NONE);
  uiItemR(col, &view_transform_ptr, "look", 0, IFACE_("Look"), ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, &view_transform_ptr, "exposure", 0, nullptr, ICON_NONE);
  uiItemR(col, &view_transform_ptr, "gamma", 0, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, &view_transform_ptr, "use_curve_mapping", 0, nullptr, ICON_NONE);
  if (view_settings->flag & COLORMANAGE_VIEW_USE_CURVES) {
    uiTemplateCurveMapping(
        col, &view_transform_ptr, "curve_mapping", 'c', true, false, false, false);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Component Menu
 * \{ */

struct ComponentMenuArgs {
  PointerRNA ptr;
  char propname[64]; /* XXX arbitrary */
};
/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *component_menu(bContext *C, ARegion *region, void *args_v)
{
  ComponentMenuArgs *args = (ComponentMenuArgs *)args_v;

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN);

  uiLayout *layout = uiLayoutColumn(UI_block_layout(block,
                                                    UI_LAYOUT_VERTICAL,
                                                    UI_LAYOUT_PANEL,
                                                    0,
                                                    0,
                                                    UI_UNIT_X * 6,
                                                    UI_UNIT_Y,
                                                    0,
                                                    UI_style_get()),
                                    0);

  uiItemR(layout, &args->ptr, args->propname, UI_ITEM_R_EXPAND, "", ICON_NONE);

  UI_block_bounds_set_normal(block, 0.3f * U.widget_unit);
  UI_block_direction_set(block, UI_DIR_DOWN);

  return block;
}
void uiTemplateComponentMenu(uiLayout *layout,
                             PointerRNA *ptr,
                             const char *propname,
                             const char *name)
{
  ComponentMenuArgs *args = MEM_cnew<ComponentMenuArgs>(__func__);

  args->ptr = *ptr;
  BLI_strncpy(args->propname, propname, sizeof(args->propname));

  uiBlock *block = uiLayoutGetBlock(layout);
  UI_block_align_begin(block);

  uiBut *but = uiDefBlockButN(
      block, component_menu, args, name, 0, 0, UI_UNIT_X * 6, UI_UNIT_Y, "");
  /* set rna directly, uiDefBlockButN doesn't do this */
  but->rnapoin = *ptr;
  but->rnaprop = RNA_struct_find_property(ptr, propname);
  but->rnaindex = 0;

  UI_block_align_end(block);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Socket Icon Template
 * \{ */

void uiTemplateNodeSocket(uiLayout *layout, bContext * /*C*/, float color[4])
{
  uiBlock *block = uiLayoutGetBlock(layout);
  UI_block_align_begin(block);

  /* XXX using explicit socket colors is not quite ideal.
   * Eventually it should be possible to use theme colors for this purpose,
   * but this requires a better design for extendable color palettes in user prefs.
   */
  uiBut *but = uiDefBut(
      block, UI_BTYPE_NODE_SOCKET, 0, "", 0, 0, UI_UNIT_X, UI_UNIT_Y, nullptr, 0, 0, 0, 0, "");
  rgba_float_to_uchar(but->col, color);

  UI_block_align_end(block);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cache File Template
 * \{ */

void uiTemplateCacheFileVelocity(uiLayout *layout, PointerRNA *fileptr)
{
  if (RNA_pointer_is_null(fileptr)) {
    return;
  }

  /* Ensure that the context has a CacheFile as this may not be set inside of modifiers panels. */
  uiLayoutSetContextPointer(layout, "edit_cachefile", fileptr);

  uiItemR(layout, fileptr, "velocity_name", 0, nullptr, ICON_NONE);
  uiItemR(layout, fileptr, "velocity_unit", 0, nullptr, ICON_NONE);
}

void uiTemplateCacheFileProcedural(uiLayout *layout, const bContext *C, PointerRNA *fileptr)
{
  if (RNA_pointer_is_null(fileptr)) {
    return;
  }

  /* Ensure that the context has a CacheFile as this may not be set inside of modifiers panels. */
  uiLayoutSetContextPointer(layout, "edit_cachefile", fileptr);

  uiLayout *row, *sub;

  /* Only enable render procedural option if the active engine supports it. */
  const RenderEngineType *engine_type = CTX_data_engine_type(C);

  Scene *scene = CTX_data_scene(C);
  const bool engine_supports_procedural = RE_engine_supports_alembic_procedural(engine_type,
                                                                                scene);

  if (!engine_supports_procedural) {
    row = uiLayoutRow(layout, false);
    /* For Cycles, verify that experimental features are enabled. */
    if (BKE_scene_uses_cycles(scene) && !BKE_scene_uses_cycles_experimental_features(scene)) {
      uiItemL(
          row,
          TIP_(
              "The Cycles Alembic Procedural is only available with the experimental feature set"),
          ICON_INFO);
    }
    else {
      uiItemL(
          row, TIP_("The active render engine does not have an Alembic Procedural"), ICON_INFO);
    }
  }

  row = uiLayoutRow(layout, false);
  uiLayoutSetActive(row, engine_supports_procedural);
  uiItemR(row, fileptr, "use_render_procedural", 0, nullptr, ICON_NONE);

  const bool use_render_procedural = RNA_boolean_get(fileptr, "use_render_procedural");
  const bool use_prefetch = RNA_boolean_get(fileptr, "use_prefetch");

  row = uiLayoutRow(layout, false);
  uiLayoutSetEnabled(row, use_render_procedural);
  uiItemR(row, fileptr, "use_prefetch", 0, nullptr, ICON_NONE);

  sub = uiLayoutRow(layout, false);
  uiLayoutSetEnabled(sub, use_prefetch && use_render_procedural);
  uiItemR(sub, fileptr, "prefetch_cache_size", 0, nullptr, ICON_NONE);
}

void uiTemplateCacheFileTimeSettings(uiLayout *layout, PointerRNA *fileptr)
{
  if (RNA_pointer_is_null(fileptr)) {
    return;
  }

  /* Ensure that the context has a CacheFile as this may not be set inside of modifiers panels. */
  uiLayoutSetContextPointer(layout, "edit_cachefile", fileptr);

  uiLayout *row, *sub, *subsub;

  row = uiLayoutRow(layout, false);
  uiItemR(row, fileptr, "is_sequence", 0, nullptr, ICON_NONE);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Override Frame"));
  sub = uiLayoutRow(row, true);
  uiLayoutSetPropDecorate(sub, false);
  uiItemR(sub, fileptr, "override_frame", 0, "", ICON_NONE);
  subsub = uiLayoutRow(sub, true);
  uiLayoutSetActive(subsub, RNA_boolean_get(fileptr, "override_frame"));
  uiItemR(subsub, fileptr, "frame", 0, "", ICON_NONE);
  uiItemDecoratorR(row, fileptr, "frame", 0);

  row = uiLayoutRow(layout, false);
  uiItemR(row, fileptr, "frame_offset", 0, nullptr, ICON_NONE);
  uiLayoutSetActive(row, !RNA_boolean_get(fileptr, "is_sequence"));
}

static void cache_file_layer_item(uiList * /*ui_list*/,
                                  const bContext * /*C*/,
                                  uiLayout *layout,
                                  PointerRNA * /*dataptr*/,
                                  PointerRNA *itemptr,
                                  int /*icon*/,
                                  PointerRNA * /*active_dataptr*/,
                                  const char * /*active_propname*/,
                                  int /*index*/,
                                  int /*flt_flag*/)
{
  uiLayout *row = uiLayoutRow(layout, true);
  uiItemR(row, itemptr, "hide_layer", UI_ITEM_R_NO_BG, "", ICON_NONE);
  uiItemR(row, itemptr, "filepath", UI_ITEM_R_NO_BG, "", ICON_NONE);
}

uiListType *UI_UL_cache_file_layers()
{
  uiListType *list_type = (uiListType *)MEM_callocN(sizeof(*list_type), __func__);

  BLI_strncpy(list_type->idname, "UI_UL_cache_file_layers", sizeof(list_type->idname));
  list_type->draw_item = cache_file_layer_item;

  return list_type;
}

void uiTemplateCacheFileLayers(uiLayout *layout, const bContext *C, PointerRNA *fileptr)
{
  if (RNA_pointer_is_null(fileptr)) {
    return;
  }

  /* Ensure that the context has a CacheFile as this may not be set inside of modifiers panels. */
  uiLayoutSetContextPointer(layout, "edit_cachefile", fileptr);

  uiLayout *row = uiLayoutRow(layout, false);
  uiLayout *col = uiLayoutColumn(row, true);

  uiTemplateList(col,
                 (bContext *)C,
                 "UI_UL_cache_file_layers",
                 "cache_file_layers",
                 fileptr,
                 "layers",
                 fileptr,
                 "active_index",
                 "",
                 1,
                 5,
                 UILST_LAYOUT_DEFAULT,
                 1,
                 UI_TEMPLATE_LIST_FLAG_NONE);

  col = uiLayoutColumn(row, true);
  uiItemO(col, "", ICON_ADD, "cachefile.layer_add");
  uiItemO(col, "", ICON_REMOVE, "cachefile.layer_remove");

  CacheFile *file = static_cast<CacheFile *>(fileptr->data);
  if (BLI_listbase_count(&file->layers) > 1) {
    uiItemS_ex(col, 1.0f);
    uiItemO(col, "", ICON_TRIA_UP, "cachefile.layer_move");
    uiItemO(col, "", ICON_TRIA_DOWN, "cachefile.layer_move");
  }
}

bool uiTemplateCacheFilePointer(PointerRNA *ptr, const char *propname, PointerRNA *r_file_ptr)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    printf(
        "%s: property not found: %s.%s\n", __func__, RNA_struct_identifier(ptr->type), propname);
    return false;
  }

  if (RNA_property_type(prop) != PROP_POINTER) {
    printf("%s: expected pointer property for %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname);
    return false;
  }

  *r_file_ptr = RNA_property_pointer_get(ptr, prop);
  return true;
}

void uiTemplateCacheFile(uiLayout *layout,
                         const bContext *C,
                         PointerRNA *ptr,
                         const char *propname)
{
  if (!ptr->data) {
    return;
  }

  PointerRNA fileptr;
  if (!uiTemplateCacheFilePointer(ptr, propname, &fileptr)) {
    return;
  }

  CacheFile *file = static_cast<CacheFile *>(fileptr.data);

  uiLayoutSetContextPointer(layout, "edit_cachefile", &fileptr);

  uiTemplateID(layout,
               C,
               ptr,
               propname,
               nullptr,
               "CACHEFILE_OT_open",
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);

  if (!file) {
    return;
  }

  SpaceProperties *sbuts = CTX_wm_space_properties(C);

  uiLayout *row, *sub;

  uiLayoutSetPropSep(layout, true);

  row = uiLayoutRow(layout, true);
  uiItemR(row, &fileptr, "filepath", 0, nullptr, ICON_NONE);
  sub = uiLayoutRow(row, true);
  uiItemO(sub, "", ICON_FILE_REFRESH, "cachefile.reload");

  if (sbuts->mainb == BCONTEXT_CONSTRAINT) {
    row = uiLayoutRow(layout, false);
    uiItemR(row, &fileptr, "scale", 0, IFACE_("Manual Scale"), ICON_NONE);
  }

  /* TODO: unused for now, so no need to expose. */
#if 0
  row = uiLayoutRow(layout, false);
  uiItemR(row, &fileptr, "forward_axis", 0, "Forward Axis", ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemR(row, &fileptr, "up_axis", 0, "Up Axis", ICON_NONE);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recent Files Template
 * \{ */

int uiTemplateRecentFiles(uiLayout *layout, int rows)
{
  int i;
  LISTBASE_FOREACH_INDEX (RecentFile *, recent, &G.recent_files, i) {
    if (i >= rows) {
      break;
    }

    const char *filename = BLI_path_basename(recent->filepath);
    PointerRNA ptr;
    uiItemFullO(layout,
                "WM_OT_open_mainfile",
                filename,
                BLO_has_bfile_extension(filename) ? ICON_FILE_BLEND : ICON_FILE_BACKUP,
                nullptr,
                WM_OP_INVOKE_DEFAULT,
                0,
                &ptr);
    RNA_string_set(&ptr, "filepath", recent->filepath);
    RNA_boolean_set(&ptr, "display_file_selector", false);
  }

  return i;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name FileSelectParams Path Button Template
 * \{ */

void uiTemplateFileSelectPath(uiLayout *layout, bContext *C, FileSelectParams *params)
{
  bScreen *screen = CTX_wm_screen(C);
  SpaceFile *sfile = CTX_wm_space_file(C);

  ED_file_path_button(screen, sfile, params, uiLayoutGetBlock(layout));
}

/** \} */
