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
 */

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_cachefile_types.h"
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

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BLO_readfile.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_view2d.h"
#include "interface_intern.h"

#include "PIL_time.h"

/* we may want to make this optional, disable for now. */
// #define USE_OP_RESET_BUT

/* defines for templateID/TemplateSearch */
#define TEMPLATE_SEARCH_TEXTBUT_WIDTH (UI_UNIT_X * 6)
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
  ID *id = (active_ptr.data && RNA_struct_is_ID(active_ptr.type)) ? active_ptr.data : NULL;
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
    uiLayout *col = NULL;

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
    uiDefBut(block, UI_BTYPE_LABEL, 0, "", 10, 26, w, h, NULL, 0, 0, 0, 0, NULL);

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
             NULL,
             0,
             0,
             0,
             0,
             NULL);
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
                         NULL,
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

typedef struct TemplateID {
  PointerRNA ptr;
  PropertyRNA *prop;

  ListBase *idlb;
  short idcode;
  short filter;
  int prv_rows, prv_cols;
  bool preview;
  float scale;
} TemplateID;

/* Search browse menu, assign  */
static void template_ID_set_property_exec_fn(bContext *C, void *arg_template, void *item)
{
  TemplateID *template_ui = (TemplateID *)arg_template;

  /* ID */
  if (item) {
    PointerRNA idptr;

    RNA_id_pointer_create(item, &idptr);
    RNA_property_pointer_set(&template_ui->ptr, template_ui->prop, idptr, NULL);
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
  const bool has_sep_char = (id->lib != NULL);

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
                          has_sep_char ? UI_BUT_HAS_SEP_CHAR : 0,
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
                         const bool UNUSED(is_first))
{
  TemplateID *template_ui = (TemplateID *)arg_template;
  ListBase *lb = template_ui->idlb;
  const int flag = RNA_property_flag(template_ui->prop);

  StringSearch *search = BLI_string_search_new();

  /* ID listbase */
  LISTBASE_FOREACH (ID *, id, lb) {
    if (id_search_allows_id(template_ui, flag, id, str)) {
      BLI_string_search_add(search, id->name + 2, id);
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
        BLI_string_search_add(search, id->name + 2, id);
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
                                            const bool UNUSED(is_first))
{
  TemplateID *template_ui = (TemplateID *)arg_template;
  ListBase *lb = template_ui->idlb;
  Scene *scene = NULL;
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
  TemplateID *template_ui = arg;
  ID *active_id = active;
  StructRNA *type = RNA_property_pointer_type(&template_ui->ptr, template_ui->prop);

  uiSearchItemTooltipData tooltip_data = {0};

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

/* This is for browsing and editing the ID-blocks used */

/* for new/open operators */
void UI_context_active_but_prop_get_templateID(bContext *C,
                                               PointerRNA *r_ptr,
                                               PropertyRNA **r_prop)
{
  uiBut *but = UI_context_active_but_get(C);

  memset(r_ptr, 0, sizeof(*r_ptr));
  *r_prop = NULL;

  if (but && but->func_argN) {
    TemplateID *template_ui = but->func_argN;
    *r_ptr = template_ui->ptr;
    *r_prop = template_ui->prop;
  }
}

static void template_id_cb(bContext *C, void *arg_litem, void *arg_event)
{
  TemplateID *template_ui = (TemplateID *)arg_litem;
  PointerRNA idptr = RNA_property_pointer_get(&template_ui->ptr, template_ui->prop);
  ID *id = idptr.data;
  const int event = POINTER_AS_INT(arg_event);
  const char *undo_push_label = NULL;

  switch (event) {
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
      RNA_property_pointer_set(&template_ui->ptr, template_ui->prop, idptr, NULL);
      RNA_property_update(C, &template_ui->ptr, template_ui->prop);

      if (id && CTX_wm_window(C)->eventstate->shift) {
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
        if (CTX_wm_window(C)->eventstate->shift) {
          if (ID_IS_OVERRIDABLE_LIBRARY(id)) {
            /* Only remap that specific ID usage to overriding local data-block. */
            ID *override_id = BKE_lib_override_library_create_from_id(bmain, id, false);
            if (override_id != NULL) {
              BKE_main_id_clear_newpoins(bmain);

              if (GS(override_id->name) == ID_OB) {
                Scene *scene = CTX_data_scene(C);
                if (!BKE_collection_has_object_recursive(scene->master_collection,
                                                         (Object *)override_id)) {
                  BKE_collection_object_add_from(
                      bmain, scene, (Object *)id, (Object *)override_id);
                }
              }

              /* Assign new pointer, takes care of updates/notifiers */
              RNA_id_pointer_create(override_id, &idptr);
            }
            undo_push_label = "Make Library Override";
          }
        }
        else {
          if (BKE_lib_id_make_local(bmain, id, false, 0)) {
            BKE_main_id_clear_newpoins(bmain);

            /* reassign to get get proper updates/notifiers */
            idptr = RNA_property_pointer_get(&template_ui->ptr, template_ui->prop);
            undo_push_label = "Make Local";
          }
        }
        if (undo_push_label != NULL) {
          RNA_property_pointer_set(&template_ui->ptr, template_ui->prop, idptr, NULL);
          RNA_property_update(C, &template_ui->ptr, template_ui->prop);
        }
      }
      break;
    case UI_ID_OVERRIDE:
      if (id && ID_IS_OVERRIDE_LIBRARY(id)) {
        BKE_lib_override_library_free(&id->override_library, true);
        /* reassign to get get proper updates/notifiers */
        idptr = RNA_property_pointer_get(&template_ui->ptr, template_ui->prop);
        RNA_property_pointer_set(&template_ui->ptr, template_ui->prop, idptr, NULL);
        RNA_property_update(C, &template_ui->ptr, template_ui->prop);
        undo_push_label = "Override Data-Block";
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
          ED_object_single_user(bmain, scene, (struct Object *)id);
          WM_event_add_notifier(C, NC_WINDOW, NULL);
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

  if (undo_push_label != NULL) {
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
      case ID_CU:
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
      case ID_HA:
        return N_("Browse Hair Data to be linked");
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
                            BLT_I18NCONTEXT_ID_CURVE,
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
                            BLT_I18NCONTEXT_ID_HAIR,
                            BLT_I18NCONTEXT_ID_POINTCLOUD,
                            BLT_I18NCONTEXT_ID_VOLUME,
                            BLT_I18NCONTEXT_ID_SIMULATION, );
  /* Note: BLT_I18N_MSGID_MULTI_CTXT takes a maximum number of parameters,
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
                            NULL);
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
                           NULL,
                           0,
                           0,
                           0,
                           0,
                           NULL);
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
  ID *id = idptr.data;
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

    // text_idbutton(id, name);
    name[0] = '\0';
    but = uiDefButR(block,
                    UI_BTYPE_TEXT,
                    0,
                    name,
                    0,
                    0,
                    TEMPLATE_SEARCH_TEXTBUT_WIDTH,
                    TEMPLATE_SEARCH_TEXTBUT_HEIGHT,
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

    if (id->lib) {
      if (id->tag & LIB_TAG_INDIRECT) {
        but = uiDefIconBut(block,
                           UI_BTYPE_BUT,
                           0,
                           ICON_LIBRARY_DATA_INDIRECT,
                           0,
                           0,
                           UI_UNIT_X,
                           UI_UNIT_Y,
                           NULL,
                           0,
                           0,
                           0,
                           0,
                           TIP_("Indirect library data-block, cannot change"));
        UI_but_flag_enable(but, UI_BUT_DISABLED);
      }
      else {
        const bool disabled = (!BKE_lib_id_make_local(CTX_data_main(C), id, true /* test */, 0) ||
                               (idfrom && idfrom->lib));
        but = uiDefIconBut(block,
                           UI_BTYPE_BUT,
                           0,
                           ICON_LIBRARY_DATA_DIRECT,
                           0,
                           0,
                           UI_UNIT_X,
                           UI_UNIT_Y,
                           NULL,
                           0,
                           0,
                           0,
                           0,
                           TIP_("Direct linked library data-block, click to make local, "
                                "Shift + Click to create a library override"));
        if (disabled) {
          UI_but_flag_enable(but, UI_BUT_DISABLED);
        }
        else {
          UI_but_funcN_set(
              but, template_id_cb, MEM_dupallocN(template_ui), POINTER_FROM_INT(UI_ID_LOCAL));
        }
      }
    }
    else if (ID_IS_OVERRIDE_LIBRARY(id)) {
      but = uiDefIconBut(block,
                         UI_BTYPE_BUT,
                         0,
                         ICON_LIBRARY_DATA_OVERRIDE,
                         0,
                         0,
                         UI_UNIT_X,
                         UI_UNIT_Y,
                         NULL,
                         0,
                         0,
                         0,
                         0,
                         TIP_("Library override of linked data-block, click to make fully local"));
      UI_but_funcN_set(
          but, template_id_cb, MEM_dupallocN(template_ui), POINTER_FROM_INT(UI_ID_OVERRIDE));
    }

    if ((ID_REAL_USERS(id) > 1) && (hide_buttons == false)) {
      char numstr[32];
      short numstr_len;

      numstr_len = BLI_snprintf(numstr, sizeof(numstr), "%d", ID_REAL_USERS(id));

      but = uiDefBut(
          block,
          UI_BTYPE_BUT,
          0,
          numstr,
          0,
          0,
          numstr_len * 0.2f * UI_UNIT_X + UI_UNIT_X,
          UI_UNIT_Y,
          NULL,
          0,
          0,
          0,
          0,
          TIP_("Display number of users of this data (click to make a single-user copy)"));
      but->flag |= UI_BUT_UNDO;

      UI_but_funcN_set(
          but, template_id_cb, MEM_dupallocN(template_ui), POINTER_FROM_INT(UI_ID_ALONE));
      if ((!BKE_id_copy_is_allowed(id)) || (idfrom && idfrom->lib) || (!editable) ||
          /* object in editmode - don't change data */
          (idfrom && GS(idfrom->name) == ID_OB && (((Object *)idfrom)->mode & OB_MODE_EDIT))) {
        UI_but_flag_enable(but, UI_BUT_DISABLED);
      }
    }

    if (user_alert) {
      UI_but_flag_enable(but, UI_BUT_REDALERT);
    }

    if (id->lib == NULL && !(ELEM(GS(id->name), ID_GR, ID_SCE, ID_SCR, ID_TXT, ID_OB, ID_WS)) &&
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
                    NULL);
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
                              NULL);
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
                             NULL,
                             0,
                             0,
                             0,
                             0,
                             NULL);
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
    but = NULL;

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
                          NULL);
      /* so we can access the template from operators, font unlinking needs this */
      UI_but_funcN_set(but, NULL, MEM_dupallocN(template_ui), NULL);
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
            NULL,
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
    return but->custom_data;
  }
  return NULL;
}

static void template_ID_tabs(const bContext *C,
                             uiLayout *layout,
                             TemplateID *template,
                             StructRNA *type,
                             int flag,
                             const char *newop,
                             const char *menu)
{
  const ARegion *region = CTX_wm_region(C);
  const PointerRNA active_ptr = RNA_property_pointer_get(&template->ptr, template->prop);
  MenuType *mt = menu ? WM_menutype_find(menu, false) : NULL;

  const int but_align = ui_but_align_opposite_to_area_align_get(region);
  const int but_height = UI_UNIT_Y * 1.1;

  uiBlock *block = uiLayoutGetBlock(layout);
  const uiStyle *style = UI_style_get_dpi();

  ListBase ordered;
  BKE_id_ordered_list(&ordered, template->idlb);

  LISTBASE_FOREACH (LinkData *, link, &ordered) {
    ID *id = link->data;
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
                                               &template->ptr,
                                               template->prop,
                                               0,
                                               0.0f,
                                               sizeof(id->name) - 2,
                                               0.0f,
                                               0.0f,
                                               "");
    UI_but_funcN_set(&tab->but, template_ID_set_property_exec_fn, MEM_dupallocN(template), id);
    tab->but.custom_data = (void *)id;
    tab->but.dragpoin = id;
    tab->menu = mt;

    UI_but_drawflag_enable(&tab->but, but_align);
  }

  BLI_freelistN(&ordered);

  if (flag & UI_ID_ADD_NEW) {
    const bool editable = RNA_property_editable(&template->ptr, template->prop);
    uiBut *but;

    if (active_ptr.type) {
      type = active_ptr.type;
    }

    but = template_id_def_new_but(block,
                                  active_ptr.data,
                                  template,
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

  TemplateID *template_ui = MEM_callocN(sizeof(TemplateID), "TemplateID");
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
                 NULL,
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
                 NULL,
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
                 NULL,
                 NULL,
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
                 NULL,
                 NULL,
                 NULL,
                 NULL,
                 NULL,
                 UI_ID_BROWSE | UI_ID_PREVIEWS | UI_ID_DELETE,
                 rows,
                 cols,
                 filter,
                 false,
                 scale < 0.5f ? 0.5f : scale,
                 false,
                 false);
}

/**
 * Version of #uiTemplateID using tabs.
 */
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
                 NULL,
                 NULL,
                 menu,
                 NULL,
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

/**
 * This is for selecting the type of ID-block to use,
 * and then from the relevant type choosing the block to use.
 *
 * \param propname: property identifier for property that ID-pointer gets stored to.
 * \param proptypename: property identifier for property
 * used to determine the type of ID-pointer that can be used.
 */
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

typedef struct TemplateSearch {
  uiRNACollectionSearch search_data;

  bool use_previews;
  int preview_rows, preview_cols;
} TemplateSearch;

static void template_search_exec_fn(bContext *C, void *arg_template, void *item)
{
  TemplateSearch *template_search = arg_template;
  uiRNACollectionSearch *coll_search = &template_search->search_data;
  StructRNA *type = RNA_property_pointer_type(&coll_search->target_ptr, coll_search->target_prop);
  PointerRNA item_ptr;

  RNA_pointer_create(NULL, type, item, &item_ptr);
  RNA_property_pointer_set(&coll_search->target_ptr, coll_search->target_prop, item_ptr, NULL);
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
                                     NULL,
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
  uiDefAutoButR(block,
                active_ptr,
                RNA_struct_name_property(type),
                0,
                "",
                ICON_NONE,
                0,
                0,
                TEMPLATE_SEARCH_TEXTBUT_WIDTH,
                TEMPLATE_SEARCH_TEXTBUT_HEIGHT);
}

static void template_search_add_button_operator(uiBlock *block,
                                                const char *const operator_name,
                                                const int opcontext,
                                                const int icon,
                                                const bool editable)
{
  if (!operator_name) {
    return;
  }

  uiBut *but = uiDefIconButO(
      block, UI_BTYPE_BUT, operator_name, opcontext, icon, 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL);

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
    searchptr = NULL;
  }

  if (!searchptr && !searchpropname) {
    /* both NULL means we don't use a custom rna collection to search in */
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

  return NULL;
}

static TemplateSearch *template_search_setup(PointerRNA *ptr,
                                             const char *const propname,
                                             PointerRNA *searchptr,
                                             const char *const searchpropname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop || RNA_property_type(prop) != PROP_POINTER) {
    RNA_warning("pointer property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return NULL;
  }
  PropertyRNA *searchprop = template_search_get_searchprop(ptr, prop, searchptr, searchpropname);

  TemplateSearch *template_search = MEM_callocN(sizeof(*template_search), __func__);
  template_search->search_data.target_ptr = *ptr;
  template_search->search_data.target_prop = prop;
  template_search->search_data.search_ptr = *searchptr;
  template_search->search_data.search_prop = searchprop;

  return template_search;
}

/**
 * Search menu to pick an item from a collection.
 * A version of uiTemplateID that works for non-ID types.
 */
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
  if (template_search != NULL) {
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

  if (template_search != NULL) {
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

/**
 * This is creating/editing RNA-Paths
 *
 * - ptr: struct which holds the path property
 * - propname: property identifier for property that path gets stored to
 * - root_ptr: struct that path gets built from
 */
void uiTemplatePathBuilder(uiLayout *layout,
                           PointerRNA *ptr,
                           const char *propname,
                           PointerRNA *UNUSED(root_ptr),
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
  BKE_modifier_type_panel_id(md->type, r_name);
}

void uiTemplateModifiers(uiLayout *UNUSED(layout), bContext *C)
{
  ARegion *region = CTX_wm_region(C);

  Object *ob = ED_object_active_context(C);
  ListBase *modifiers = &ob->modifiers;

  const bool panels_match = UI_panel_list_matches_data(region, modifiers, modifier_panel_id);

  if (!panels_match) {
    UI_panels_free_instanced(C, region);
    ModifierData *md = modifiers->first;
    for (int i = 0; md; i++, md = md->next) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);
      if (mti->panelRegister == NULL) {
        continue;
      }

      char panel_idname[MAX_NAME];
      modifier_panel_id(md, panel_idname);

      /* Create custom data RNA pointer. */
      PointerRNA *md_ptr = MEM_mallocN(sizeof(PointerRNA), "panel customdata");
      RNA_pointer_create(&ob->id, &RNA_Modifier, md, md_ptr);

      UI_panel_add_instanced(C, region, &region->panels, panel_idname, md_ptr);
    }
  }
  else {
    /* Assuming there's only one group of instanced panels, update the custom data pointers. */
    Panel *panel = region->panels.first;
    LISTBASE_FOREACH (ModifierData *, md, modifiers) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);
      if (mti->panelRegister == NULL) {
        continue;
      }

      /* Move to the next instanced panel corresponding to the next modifier. */
      while ((panel->type == NULL) || !(panel->type->flag & PANEL_TYPE_INSTANCED)) {
        panel = panel->next;
        BLI_assert(panel != NULL); /* There shouldn't be fewer panels than modifiers with UIs. */
      }

      PointerRNA *md_ptr = MEM_mallocN(sizeof(PointerRNA), "panel customdata");
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
  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &props_ptr);
  WM_operator_properties_free(&props_ptr);
}

/**
 * Get the expand flag from the active constraint to use for the panel.
 */
static short get_constraint_expand_flag(const bContext *UNUSED(C), Panel *panel)
{
  PointerRNA *con_ptr = UI_panel_custom_data_get(panel);
  bConstraint *con = (bConstraint *)con_ptr->data;

  return con->ui_expand_flag;
}

/**
 * Save the expand flag for the panel and sub-panels to the constraint.
 */
static void set_constraint_expand_flag(const bContext *UNUSED(C), Panel *panel, short expand_flag)
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
  if (cti == NULL) {
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
  if (cti == NULL) {
    return;
  }

  strcpy(r_name, CONSTRAINT_BONE_TYPE_PANEL_PREFIX);
  strcat(r_name, cti->structName);
}

/**
 * Check if the constraint panels don't match the data and rebuild the panels if so.
 */
void uiTemplateConstraints(uiLayout *UNUSED(layout), bContext *C, bool use_bone_constraints)
{
  ARegion *region = CTX_wm_region(C);

  Object *ob = ED_object_active_context(C);
  ListBase *constraints = {NULL};
  if (use_bone_constraints) {
    constraints = ED_object_pose_constraint_list(C);
  }
  else if (ob != NULL) {
    constraints = &ob->constraints;
  }

  /* Switch between the bone panel ID function and the object panel ID function. */
  uiListPanelIDFromDataFunc panel_id_func = use_bone_constraints ? bone_constraint_panel_id :
                                                                   object_constraint_panel_id;

  const bool panels_match = UI_panel_list_matches_data(region, constraints, panel_id_func);

  if (!panels_match) {
    UI_panels_free_instanced(C, region);
    bConstraint *con = (constraints == NULL) ? NULL : constraints->first;
    for (int i = 0; con; i++, con = con->next) {
      /* Don't show invalid/legacy constraints. */
      if (con->type == CONSTRAINT_TYPE_NULL) {
        continue;
      }
      /* Don't show temporary constraints (AutoIK and target-less IK constraints). */
      if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
        bKinematicConstraint *data = con->data;
        if (data->flag & CONSTRAINT_IK_TEMP) {
          continue;
        }
      }

      char panel_idname[MAX_NAME];
      panel_id_func(con, panel_idname);

      /* Create custom data RNA pointer. */
      PointerRNA *con_ptr = MEM_mallocN(sizeof(PointerRNA), "panel customdata");
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
    Panel *panel = region->panels.first;
    LISTBASE_FOREACH (bConstraint *, con, constraints) {
      /* Don't show invalid/legacy constraints. */
      if (con->type == CONSTRAINT_TYPE_NULL) {
        continue;
      }
      /* Don't show temporary constraints (AutoIK and target-less IK constraints). */
      if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
        bKinematicConstraint *data = con->data;
        if (data->flag & CONSTRAINT_IK_TEMP) {
          continue;
        }
      }

      /* Move to the next instanced panel corresponding to the next constraint. */
      while ((panel->type == NULL) || !(panel->type->flag & PANEL_TYPE_INSTANCED)) {
        panel = panel->next;
        BLI_assert(panel != NULL); /* There shouldn't be fewer panels than constraint panels. */
      }

      PointerRNA *con_ptr = MEM_mallocN(sizeof(PointerRNA), "constraint panel customdata");
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
  BKE_gpencil_modifierType_panel_id(md->type, r_name);
}

void uiTemplateGpencilModifiers(uiLayout *UNUSED(layout), bContext *C)
{
  ARegion *region = CTX_wm_region(C);
  Object *ob = ED_object_active_context(C);
  ListBase *modifiers = &ob->greasepencil_modifiers;

  const bool panels_match = UI_panel_list_matches_data(
      region, modifiers, gpencil_modifier_panel_id);

  if (!panels_match) {
    UI_panels_free_instanced(C, region);
    GpencilModifierData *md = modifiers->first;
    for (int i = 0; md; i++, md = md->next) {
      const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);
      if (mti->panelRegister == NULL) {
        continue;
      }

      char panel_idname[MAX_NAME];
      gpencil_modifier_panel_id(md, panel_idname);

      /* Create custom data RNA pointer. */
      PointerRNA *md_ptr = MEM_mallocN(sizeof(PointerRNA), "panel customdata");
      RNA_pointer_create(&ob->id, &RNA_GpencilModifier, md, md_ptr);

      UI_panel_add_instanced(C, region, &region->panels, panel_idname, md_ptr);
    }
  }
  else {
    /* Assuming there's only one group of instanced panels, update the custom data pointers. */
    Panel *panel = region->panels.first;
    LISTBASE_FOREACH (ModifierData *, md, modifiers) {
      const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);
      if (mti->panelRegister == NULL) {
        continue;
      }

      /* Move to the next instanced panel corresponding to the next modifier. */
      while ((panel->type == NULL) || !(panel->type->flag & PANEL_TYPE_INSTANCED)) {
        panel = panel->next;
        BLI_assert(panel != NULL); /* There shouldn't be fewer panels than modifiers with UIs. */
      }

      PointerRNA *md_ptr = MEM_mallocN(sizeof(PointerRNA), "panel customdata");
      RNA_pointer_create(&ob->id, &RNA_GpencilModifier, md, md_ptr);
      UI_panel_custom_data_set(panel, md_ptr);

      panel = panel->next;
    }
  }
}

/** \} */

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
  BKE_shaderfxType_panel_id(fx->type, r_idname);
}

/**
 * Check if the shader effect panels don't match the data and rebuild the panels if so.
 */
void uiTemplateShaderFx(uiLayout *UNUSED(layout), bContext *C)
{
  ARegion *region = CTX_wm_region(C);
  Object *ob = ED_object_active_context(C);
  ListBase *shaderfx = &ob->shader_fx;

  const bool panels_match = UI_panel_list_matches_data(region, shaderfx, shaderfx_panel_id);

  if (!panels_match) {
    UI_panels_free_instanced(C, region);
    ShaderFxData *fx = shaderfx->first;
    for (int i = 0; fx; i++, fx = fx->next) {
      char panel_idname[MAX_NAME];
      shaderfx_panel_id(fx, panel_idname);

      /* Create custom data RNA pointer. */
      PointerRNA *fx_ptr = MEM_mallocN(sizeof(PointerRNA), "panel customdata");
      RNA_pointer_create(&ob->id, &RNA_ShaderFx, fx, fx_ptr);

      UI_panel_add_instanced(C, region, &region->panels, panel_idname, fx_ptr);
    }
  }
  else {
    /* Assuming there's only one group of instanced panels, update the custom data pointers. */
    Panel *panel = region->panels.first;
    LISTBASE_FOREACH (ShaderFxData *, fx, shaderfx) {
      const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(fx->type);
      if (fxi->panelRegister == NULL) {
        continue;
      }

      /* Move to the next instanced panel corresponding to the next modifier. */
      while ((panel->type == NULL) || !(panel->type->flag & PANEL_TYPE_INSTANCED)) {
        panel = panel->next;
        BLI_assert(panel != NULL); /* There shouldn't be fewer panels than modifiers with UIs. */
      }

      PointerRNA *fx_ptr = MEM_mallocN(sizeof(PointerRNA), "panel customdata");
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

typedef struct uiTemplateOperatorPropertyPollParam {
  const bContext *C;
  wmOperator *op;
  short flag;
} uiTemplateOperatorPropertyPollParam;

#ifdef USE_OP_RESET_BUT
static void ui_layout_operator_buts__reset_cb(bContext *UNUSED(C),
                                              void *op_pt,
                                              void *UNUSED(arg_dummy2))
{
  WM_operator_properties_reset((wmOperator *)op_pt);
}
#endif

static bool ui_layout_operator_buts_poll_property(struct PointerRNA *UNUSED(ptr),
                                                  struct PropertyRNA *prop,
                                                  void *user_data)
{
  uiTemplateOperatorPropertyPollParam *params = user_data;

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
  eAutoPropButsReturn return_info = 0;

  if (!op->properties) {
    const IDPropertyTemplate val = {0};
    op->properties = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");
  }

  /* poll() on this operator may still fail,
   * at the moment there is no nice feedback when this happens just fails silently. */
  if (!WM_operator_repeat_check(C, op)) {
    UI_block_lock_set(block, true, "Operator can't' redo");
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
    uiItemM(row, "WM_MT_operator_presets", NULL, ICON_NONE);

    wmOperatorType *ot = WM_operatortype_find("WM_OT_operator_preset_add", false);
    uiItemFullO_ptr(row, ot, "", ICON_ADD, NULL, WM_OP_INVOKE_DEFAULT, 0, &op_ptr);
    RNA_string_set(&op_ptr, "operator", op->type->idname);

    uiItemFullO_ptr(row, ot, "", ICON_REMOVE, NULL, WM_OP_INVOKE_DEFAULT, 0, &op_ptr);
    RNA_string_set(&op_ptr, "operator", op->type->idname);
    RNA_boolean_set(&op_ptr, "remove_active", true);
  }

  if (op->type->ui) {
    op->layout = layout;
    op->type->ui((bContext *)C, op);
    op->layout = NULL;

    /* UI_LAYOUT_OP_SHOW_EMPTY ignored. retun_info is ignored too. We could
     * allow ot.ui callback to return this, but not needed right now. */
  }
  else {
    wmWindowManager *wm = CTX_wm_manager(C);
    uiTemplateOperatorPropertyPollParam user_data = {.C = C, .op = op, .flag = layout_flags};
    const bool use_prop_split = (layout_flags & UI_TEMPLATE_OP_PROPS_NO_SPLIT_LAYOUT) == 0;

    PointerRNA ptr;
    RNA_pointer_create(&wm->id, op->type->srna, op->properties, &ptr);

    uiLayoutSetPropSep(layout, use_prop_split);
    uiLayoutSetPropDecorate(layout, false);

    /* main draw call */
    return_info = uiDefAutoButsRNA(
        layout,
        &ptr,
        op->type->poll_property ? ui_layout_operator_buts_poll_property : NULL,
        op->type->poll_property ? &user_data : NULL,
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
                           NULL,
                           0.0,
                           0.0,
                           0.0,
                           0.0,
                           TIP_("Reset operator defaults"));
    UI_but_func_set(but, ui_layout_operator_buts__reset_cb, op, NULL);
  }
#endif

  /* set various special settings for buttons */

  /* Only do this if we're not refreshing an existing UI. */
  if (block->oldblock == NULL) {
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
    uiTemplateOperatorPropertyPollParam user_data = {.C = C, .op = op, .flag = layout_flags};
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

/**
 * Draw Operator property buttons for redoing execution with different settings.
 * This function does not initialize the layout,
 * functions can be called on the layout before and after.
 */
void uiTemplateOperatorPropertyButs(
    const bContext *C, uiLayout *layout, wmOperator *op, eButLabelAlign label_align, short flag)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  /* If there are only checkbox items, don't use split layout by default. It looks weird if the
   * checkboxes only use half the width. */
  if (ui_layout_operator_properties_only_booleans(C, wm, op, flag)) {
    flag |= UI_TEMPLATE_OP_PROPS_NO_SPLIT_LAYOUT;
  }

  template_operator_property_buts_draw_recursive(C, op, layout, label_align, flag, NULL);
}

void uiTemplateOperatorRedoProperties(uiLayout *layout, const bContext *C)
{
  wmOperator *op = WM_operator_last_redo(C);
  uiBlock *block = uiLayoutGetBlock(layout);

  if (op == NULL) {
    return;
  }

  /* Disable for now, doesn't fit well in popover. */
#if 0
  /* Repeat button with operator name as text. */
  uiItemFullO(layout,
              "SCREEN_OT_repeat_last",
              WM_operatortype_name(op->type, op->ptr),
              ICON_NONE,
              NULL,
              WM_OP_INVOKE_DEFAULT,
              0,
              NULL);
#endif

  if (WM_operator_repeat_check(C, op)) {
    int layout_flags = 0;
    if (block->panel == NULL) {
      layout_flags = UI_TEMPLATE_OP_PROPS_SHOW_TITLE;
    }
#if 0
    bool has_advanced = false;
#endif

    UI_block_func_handle_set(block, ED_undo_operator_repeat_cb_evt, op);
    template_operator_property_buts_draw_recursive(
        C, op, layout, UI_BUT_LABEL_ALIGN_NONE, layout_flags, NULL /* &has_advanced */);
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

static void constraint_active_func(bContext *UNUSED(C), void *ob_v, void *con_v)
{
  ED_object_constraint_active_set(ob_v, con_v);
}

static void draw_constraint_header(uiLayout *layout, Object *ob, bConstraint *con)
{
  bPoseChannel *pchan = BKE_pose_channel_active(ob);
  short proxy_protected, xco = 0, yco = 0;
  // int rb_col; // UNUSED

  /* determine whether constraint is proxy protected or not */
  if (BKE_constraints_proxylocked_owner(ob, pchan)) {
    proxy_protected = (con->flag & CONSTRAINT_PROXY_LOCAL) == 0;
  }
  else {
    proxy_protected = 0;
  }

  /* unless button has own callback, it adds this callback to button */
  uiBlock *block = uiLayoutGetBlock(layout);
  UI_block_func_set(block, constraint_active_func, ob, con);

  PointerRNA ptr;
  RNA_pointer_create(&ob->id, &RNA_Constraint, con, &ptr);

  uiLayoutSetContextPointer(layout, "constraint", &ptr);

  /* Constraint type icon. */
  uiLayout *sub = uiLayoutRow(layout, false);
  uiLayoutSetEmboss(sub, false);
  uiLayoutSetRedAlert(sub, (con->flag & CONSTRAINT_DISABLE));
  uiItemL(sub, "", RNA_struct_ui_icon(ptr.type));

  UI_block_emboss_set(block, UI_EMBOSS);

  if (proxy_protected == 0) {
    uiItemR(layout, &ptr, "name", 0, "", ICON_NONE);
  }
  else {
    uiItemL(layout, con->name, ICON_NONE);
  }

  /* proxy-protected constraints cannot be edited, so hide up/down + close buttons */
  if (proxy_protected) {
    UI_block_emboss_set(block, UI_EMBOSS_NONE);

    /* draw a ghost icon (for proxy) and also a lock beside it,
     * to show that constraint is "proxy locked" */
    uiDefIconBut(block,
                 UI_BTYPE_BUT,
                 0,
                 ICON_GHOST_ENABLED,
                 xco + 12.2f * UI_UNIT_X,
                 yco,
                 0.95f * UI_UNIT_X,
                 0.95f * UI_UNIT_Y,
                 NULL,
                 0.0,
                 0.0,
                 0.0,
                 0.0,
                 TIP_("Proxy Protected"));
    uiDefIconBut(block,
                 UI_BTYPE_BUT,
                 0,
                 ICON_LOCKED,
                 xco + 13.1f * UI_UNIT_X,
                 yco,
                 0.95f * UI_UNIT_X,
                 0.95f * UI_UNIT_Y,
                 NULL,
                 0.0,
                 0.0,
                 0.0,
                 0.0,
                 TIP_("Proxy Protected"));

    UI_block_emboss_set(block, UI_EMBOSS);
  }
  else {
    /* enabled */
    UI_block_emboss_set(block, UI_EMBOSS_NONE_OR_STATUS);
    uiItemR(layout, &ptr, "mute", 0, "", 0);
    UI_block_emboss_set(block, UI_EMBOSS);

    uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

    /* Close 'button' - emboss calls here disable drawing of 'button' behind X */
    UI_block_emboss_set(block, UI_EMBOSS_NONE);
    uiItemO(layout, "", ICON_X, "CONSTRAINT_OT_delete");
    UI_block_emboss_set(block, UI_EMBOSS);

    /* Some extra padding at the end, so the 'x' icon isn't too close to drag button. */
    uiItemS(layout);
  }

  /* Set but-locks for protected settings (magic numbers are used here!) */
  if (proxy_protected) {
    UI_block_lock_set(block, true, TIP_("Cannot edit Proxy-Protected Constraint"));
  }

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
  bConstraint *con = ptr->data;

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
  Material *ma = NULL;
  Tex *tex = (Tex *)id;
  short *pr_texture = NULL;
  PointerRNA material_ptr;
  PointerRNA texture_ptr;

  char _preview_id[UI_MAX_NAME_STR];

  if (id && !ELEM(GS(id->name), ID_MA, ID_TE, ID_WO, ID_LA, ID_LS)) {
    RNA_warning("Expected ID of type material, texture, light, world or line style");
    return;
  }

  /* decide what to render */
  ID *pid = id;
  ID *pparent = NULL;

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
  uiPreview *ui_preview = BLI_findstring(
      &region->ui_previews, preview_id, offsetof(uiPreview, preview_id));

  if (!ui_preview) {
    ui_preview = MEM_callocN(sizeof(uiPreview), "uiPreview");
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
  UI_block_func_handle_set(block, do_preview_buttons, NULL);

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
                  IFACE_("World"),
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
        uiItemR(row, &texture_ptr, "use_preview_alpha", 0, NULL, ICON_NONE);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ColorRamp Template
 * \{ */

typedef struct RNAUpdateCb {
  PointerRNA ptr;
  PropertyRNA *prop;
} RNAUpdateCb;

static void rna_update_cb(bContext *C, void *arg_cb, void *UNUSED(arg))
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

  /* may as well flip the cur*/
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
  ColorBand *coba = coba_v;

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
  ColorBand *coba = coba_v;
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
    RNA_pointer_create(NULL, &RNA_ColorRamp, coba, &coba_ptr);
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
                     NULL,
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
                     NULL,
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
                     NULL,
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
                     NULL,
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
  ColorBand *coba = coba_v;
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
    rna_update_cb(C, cb_v, NULL);
    ED_undo_push(C, "Add Color Ramp Stop");
  }
}

static void colorband_del_cb(bContext *C, void *cb_v, void *coba_v)
{
  ColorBand *coba = coba_v;

  if (BKE_colorband_element_remove(coba, coba->cur)) {
    ED_undo_push(C, "Delete Color Ramp Stop");
    rna_update_cb(C, cb_v, NULL);
  }
}

static void colorband_update_cb(bContext *UNUSED(C), void *bt_v, void *coba_v)
{
  uiBut *bt = bt_v;
  ColorBand *coba = coba_v;

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
                        NULL,
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
                        NULL,
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
  UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), NULL);

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
                     (float)(MAX2(0, coba->tot - 1)),
                     0,
                     0,
                     TIP_("Choose active color stop"));
      UI_but_number_step_size_set(bt, 1);

      row = uiLayoutRow(split, false);
      uiItemR(row, &ptr, "position", 0, IFACE_("Pos"), ICON_NONE);
      bt = block->buttons.last;
      UI_but_func_set(bt, colorband_update_cb, bt, coba);

      row = uiLayoutRow(layout, false);
      uiItemR(row, &ptr, "color", 0, "", ICON_NONE);
      bt = block->buttons.last;
      UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), NULL);
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
                     (float)(MAX2(0, coba->tot - 1)),
                     0,
                     0,
                     TIP_("Choose active color stop"));
      UI_but_number_step_size_set(bt, 1);

      row = uiLayoutRow(subsplit, false);
      uiItemR(row, &ptr, "position", UI_ITEM_R_SLIDER, IFACE_("Pos"), ICON_NONE);
      bt = block->buttons.last;
      UI_but_func_set(bt, colorband_update_cb, bt, coba);

      row = uiLayoutRow(split, false);
      uiItemR(row, &ptr, "color", 0, "", ICON_NONE);
      bt = block->buttons.last;
      UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), NULL);
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

  RNAUpdateCb *cb = MEM_callocN(sizeof(RNAUpdateCb), "RNAUpdateCb");
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

  colorband_buttons_layout(layout, block, cptr.data, &rect, cb, expand);

  UI_block_lock_clear(block);

  MEM_freeN(cb);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Icon Template
 * \{ */

/**
 * \param icon_scale: Scale of the icon, 1x == button height.
 */
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
                            NULL,
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

typedef struct IconViewMenuArgs {
  PointerRNA ptr;
  PropertyRNA *prop;
  bool show_labels;
  float icon_scale;
} IconViewMenuArgs;

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
  RNA_property_enum_items(C, &args.ptr, args.prop, &item, NULL, &free);

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
                                   NULL);
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
                               NULL);
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

/**
 * \param icon_scale: Scale of the icon, 1x == button height.
 */
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
  RNA_property_enum_items(block->evil_C, ptr, prop, &items, &tot_items, &free_items);
  const int value = RNA_property_enum_get(ptr, prop);
  int icon = ICON_NONE;
  RNA_enum_icon_from_value(items, value, &icon);

  uiBut *but;
  if (RNA_property_editable(ptr, prop)) {
    IconViewMenuArgs *cb_args = MEM_callocN(sizeof(IconViewMenuArgs), __func__);
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
                       NULL,
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

static void curvemap_buttons_zoom_in(bContext *C, void *cumap_v, void *UNUSED(arg))
{
  CurveMapping *cumap = cumap_v;

  /* we allow 20 times zoom */
  if (BLI_rctf_size_x(&cumap->curr) > 0.04f * BLI_rctf_size_x(&cumap->clipr)) {
    const float dx = 0.1154f * BLI_rctf_size_x(&cumap->curr);
    cumap->curr.xmin += dx;
    cumap->curr.xmax -= dx;
    const float dy = 0.1154f * BLI_rctf_size_y(&cumap->curr);
    cumap->curr.ymin += dy;
    cumap->curr.ymax -= dy;
  }

  ED_region_tag_redraw(CTX_wm_region(C));
}

static void curvemap_buttons_zoom_out(bContext *C, void *cumap_v, void *UNUSED(unused))
{
  CurveMapping *cumap = cumap_v;
  float d, d1;

  /* we allow 20 times zoom, but don't view outside clip */
  if (BLI_rctf_size_x(&cumap->curr) < 20.0f * BLI_rctf_size_x(&cumap->clipr)) {
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

static void curvemap_buttons_setclip(bContext *UNUSED(C), void *cumap_v, void *UNUSED(arg))
{
  CurveMapping *cumap = cumap_v;

  BKE_curvemapping_changed(cumap, false);
}

static void curvemap_buttons_delete(bContext *C, void *cb_v, void *cumap_v)
{
  CurveMapping *cumap = cumap_v;

  BKE_curvemap_remove(cumap->cm + cumap->cur, SELECT);
  BKE_curvemapping_changed(cumap, false);

  rna_update_cb(C, cb_v, NULL);
}

/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *curvemap_clipping_func(bContext *C, ARegion *region, void *cumap_v)
{
  CurveMapping *cumap = cumap_v;
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
  UI_but_func_set(bt, curvemap_buttons_setclip, cumap, NULL);

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
  CurveMapping *cumap = cumap_v;
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
    case UICURVE_FUNC_HANDLE_VECTOR: /* set vector */
      BKE_curvemap_handle_set(cuma, HD_VECT);
      BKE_curvemapping_changed(cumap, false);
      break;
    case UICURVE_FUNC_HANDLE_AUTO: /* set auto */
      BKE_curvemap_handle_set(cuma, HD_AUTO);
      BKE_curvemapping_changed(cumap, false);
      break;
    case UICURVE_FUNC_HANDLE_AUTO_ANIM: /* set auto-clamped */
      BKE_curvemap_handle_set(cuma, HD_AUTO_ANIM);
      BKE_curvemapping_changed(cumap, false);
      break;
    case UICURVE_FUNC_EXTEND_HOZ: /* extend horiz */
      cumap->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
      BKE_curvemapping_changed(cumap, false);
      break;
    case UICURVE_FUNC_EXTEND_EXP: /* extend extrapolate */
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
                     NULL,
                     0.0,
                     0.0,
                     0,
                     UICURVE_FUNC_RESET_VIEW,
                     "");
    uiDefIconTextBut(block,
                     UI_BTYPE_BUT_MENU,
                     1,
                     ICON_BLANK1,
                     IFACE_("Vector Handle"),
                     0,
                     yco -= UI_UNIT_Y,
                     menuwidth,
                     UI_UNIT_Y,
                     NULL,
                     0.0,
                     0.0,
                     0,
                     UICURVE_FUNC_HANDLE_VECTOR,
                     "");
    uiDefIconTextBut(block,
                     UI_BTYPE_BUT_MENU,
                     1,
                     ICON_BLANK1,
                     IFACE_("Auto Handle"),
                     0,
                     yco -= UI_UNIT_Y,
                     menuwidth,
                     UI_UNIT_Y,
                     NULL,
                     0.0,
                     0.0,
                     0,
                     UICURVE_FUNC_HANDLE_AUTO,
                     "");
    uiDefIconTextBut(block,
                     UI_BTYPE_BUT_MENU,
                     1,
                     ICON_BLANK1,
                     IFACE_("Auto Clamped Handle"),
                     0,
                     yco -= UI_UNIT_Y,
                     menuwidth,
                     UI_UNIT_Y,
                     NULL,
                     0.0,
                     0.0,
                     0,
                     UICURVE_FUNC_HANDLE_AUTO_ANIM,
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
                     NULL,
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
                     NULL,
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
                     NULL,
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
  return curvemap_tools_func(C, region, cumap_v, true, UICURVE_FUNC_RESET_POS);
}

static uiBlock *curvemap_tools_negslope_func(bContext *C, ARegion *region, void *cumap_v)
{
  return curvemap_tools_func(C, region, cumap_v, true, UICURVE_FUNC_RESET_NEG);
}

static uiBlock *curvemap_brush_tools_func(bContext *C, ARegion *region, void *cumap_v)
{
  return curvemap_tools_func(C, region, cumap_v, false, UICURVE_FUNC_RESET_NEG);
}

static uiBlock *curvemap_brush_tools_negslope_func(bContext *C, ARegion *region, void *cumap_v)
{
  return curvemap_tools_func(C, region, cumap_v, false, UICURVE_FUNC_RESET_POS);
}

static void curvemap_buttons_redraw(bContext *C, void *UNUSED(arg1), void *UNUSED(arg2))
{
  ED_region_tag_redraw(CTX_wm_region(C));
}

static void curvemap_buttons_update(bContext *C, void *arg1_v, void *cumap_v)
{
  CurveMapping *cumap = cumap_v;
  BKE_curvemapping_changed(cumap, true);
  rna_update_cb(C, arg1_v, NULL);
}

static void curvemap_buttons_reset(bContext *C, void *cb_v, void *cumap_v)
{
  CurveMapping *cumap = cumap_v;
  cumap->preset = CURVE_PRESET_LINE;
  for (int a = 0; a < CM_TOT; a++) {
    BKE_curvemap_reset(cumap->cm + a, &cumap->clipr, cumap->preset, CURVEMAP_SLOPE_POSITIVE);
  }

  cumap->black[0] = cumap->black[1] = cumap->black[2] = 0.0f;
  cumap->white[0] = cumap->white[1] = cumap->white[2] = 1.0f;
  BKE_curvemapping_set_black_white(cumap, NULL, NULL);

  BKE_curvemapping_changed(cumap, false);

  rna_update_cb(C, cb_v, NULL);
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
  CurveMapping *cumap = ptr->data;
  CurveMap *cm = &cumap->cm[cumap->cur];
  uiBut *bt;
  const float dx = UI_UNIT_X;
  int bg = -1;

  uiBlock *block = uiLayoutGetBlock(layout);

  if (tone) {
    uiLayout *split = uiLayoutSplit(layout, 0.0f, false);
    uiItemR(uiLayoutRow(split, false), ptr, "tone", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
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
      UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
    }
    if (cumap->cm[1].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "Y", 0, 0, dx, dx, &cumap->cur, 0.0, 1.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
    }
    if (cumap->cm[2].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "Z", 0, 0, dx, dx, &cumap->cur, 0.0, 2.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
    }
  }
  else if (labeltype == 'c') {
    /* color */
    uiLayout *sub = uiLayoutRow(row, true);
    uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);

    if (cumap->cm[3].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "C", 0, 0, dx, dx, &cumap->cur, 0.0, 3.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
    }
    if (cumap->cm[0].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "R", 0, 0, dx, dx, &cumap->cur, 0.0, 0.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
    }
    if (cumap->cm[1].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "G", 0, 0, dx, dx, &cumap->cur, 0.0, 1.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
    }
    if (cumap->cm[2].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "B", 0, 0, dx, dx, &cumap->cur, 0.0, 2.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
    }
  }
  else if (labeltype == 'h') {
    /* HSV */
    uiLayout *sub = uiLayoutRow(row, true);
    uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);

    if (cumap->cm[0].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "H", 0, 0, dx, dx, &cumap->cur, 0.0, 0.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
    }
    if (cumap->cm[1].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "S", 0, 0, dx, dx, &cumap->cur, 0.0, 1.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
    }
    if (cumap->cm[2].curve) {
      bt = uiDefButI(
          block, UI_BTYPE_ROW, 0, "V", 0, 0, dx, dx, &cumap->cur, 0.0, 2.0, 0.0, 0.0, "");
      UI_but_func_set(bt, curvemap_buttons_redraw, NULL, NULL);
    }
  }
  else {
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);
  }

  if (labeltype == 'h') {
    bg = UI_GRAD_H;
  }

  /* operation buttons */
  uiLayoutRow(row, true);

  UI_block_emboss_set(block, UI_EMBOSS_NONE);

  bt = uiDefIconBut(block,
                    UI_BTYPE_BUT,
                    0,
                    ICON_ZOOM_IN,
                    0,
                    0,
                    dx,
                    dx,
                    NULL,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    TIP_("Zoom in"));
  UI_but_func_set(bt, curvemap_buttons_zoom_in, cumap, NULL);

  bt = uiDefIconBut(block,
                    UI_BTYPE_BUT,
                    0,
                    ICON_ZOOM_OUT,
                    0,
                    0,
                    dx,
                    dx,
                    NULL,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    TIP_("Zoom out"));
  UI_but_func_set(bt, curvemap_buttons_zoom_out, cumap, NULL);

  if (brush && neg_slope) {
    bt = uiDefIconBlockBut(block,
                           curvemap_brush_tools_negslope_func,
                           cumap,
                           0,
                           ICON_DOWNARROW_HLT,
                           0,
                           0,
                           dx,
                           dx,
                           TIP_("Tools"));
  }
  else if (brush) {
    bt = uiDefIconBlockBut(block,
                           curvemap_brush_tools_func,
                           cumap,
                           0,
                           ICON_DOWNARROW_HLT,
                           0,
                           0,
                           dx,
                           dx,
                           TIP_("Tools"));
  }
  else if (neg_slope) {
    bt = uiDefIconBlockBut(block,
                           curvemap_tools_negslope_func,
                           cumap,
                           0,
                           ICON_DOWNARROW_HLT,
                           0,
                           0,
                           dx,
                           dx,
                           TIP_("Tools"));
  }
  else {
    bt = uiDefIconBlockBut(block,
                           curvemap_tools_posslope_func,
                           cumap,
                           0,
                           ICON_DOWNARROW_HLT,
                           0,
                           0,
                           dx,
                           dx,
                           TIP_("Tools"));
  }

  UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), NULL);

  const int icon = (cumap->flag & CUMA_DO_CLIP) ? ICON_CLIPUV_HLT : ICON_CLIPUV_DEHLT;
  bt = uiDefIconBlockBut(
      block, curvemap_clipping_func, cumap, 0, icon, 0, 0, dx, dx, TIP_("Clipping Options"));
  UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), NULL);

  bt = uiDefIconBut(block,
                    UI_BTYPE_BUT,
                    0,
                    ICON_X,
                    0,
                    0,
                    dx,
                    dx,
                    NULL,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    TIP_("Delete points"));
  UI_but_funcN_set(bt, curvemap_buttons_delete, MEM_dupallocN(cb), cumap);

  UI_block_emboss_set(block, UI_EMBOSS);

  UI_block_funcN_set(block, rna_update_cb, MEM_dupallocN(cb), NULL);

  /* curve itself */
  const int size = max_ii(uiLayoutGetWidth(layout), UI_UNIT_X);
  row = uiLayoutRow(layout, false);
  uiButCurveMapping *curve_but = (uiButCurveMapping *)uiDefBut(
      block, UI_BTYPE_CURVE, 0, "", 0, 0, size, 8.0f * UI_UNIT_X, cumap, 0.0f, 1.0f, 0, 0, "");
  curve_but->gradient_type = bg;

  /* sliders for selected point */
  CurveMapPoint *cmp = NULL;
  for (int i = 0; i < cm->totpoint; i++) {
    if (cm->curve[i].flag & CUMA_SELECT) {
      cmp = &cm->curve[i];
      break;
    }
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

    uiLayoutRow(layout, true);
    UI_block_funcN_set(block, curvemap_buttons_update, MEM_dupallocN(cb), cumap);
    bt = uiDefButF(block,
                   UI_BTYPE_NUM,
                   0,
                   "X",
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
                   "Y",
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
  }

  /* black/white levels */
  if (levels) {
    uiLayout *split = uiLayoutSplit(layout, 0.0f, false);
    uiItemR(uiLayoutColumn(split, false), ptr, "black_level", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
    uiItemR(uiLayoutColumn(split, false), ptr, "white_level", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

    uiLayoutRow(layout, false);
    bt = uiDefBut(block,
                  UI_BTYPE_BUT,
                  0,
                  IFACE_("Reset"),
                  0,
                  0,
                  UI_UNIT_X * 10,
                  UI_UNIT_Y,
                  NULL,
                  0.0f,
                  0.0f,
                  0,
                  0,
                  TIP_("Reset Black/White point and curves"));
    UI_but_funcN_set(bt, curvemap_buttons_reset, MEM_dupallocN(cb), cumap);
  }

  UI_block_funcN_set(block, NULL, NULL, NULL);
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

  RNAUpdateCb *cb = MEM_callocN(sizeof(RNAUpdateCb), "RNAUpdateCb");
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
  CurveProfile *profile = profile_v;

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
                   NULL,
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
                   NULL,
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
                   NULL,
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
                   NULL,
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
                   NULL,
                   0.0,
                   0.0,
                   0,
                   PROF_PRESET_STEPS,
                   "");

  UI_block_direction_set(block, UI_DIR_DOWN);
  UI_block_bounds_set_text(block, (int)(3.0f * UI_UNIT_X));

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
  CurveProfile *profile = profile_v;

  switch (event) {
    case UIPROFILE_FUNC_RESET: /* reset */
      BKE_curveprofile_reset(profile);
      BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
      break;
    case UIPROFILE_FUNC_RESET_VIEW: /* reset view to clipping rect */
      profile->view_rect = profile->clip_rect;
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
                   NULL,
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
                   NULL,
                   0.0,
                   0.0,
                   0,
                   UIPROFILE_FUNC_RESET,
                   "");

  UI_block_direction_set(block, UI_DIR_DOWN);
  UI_block_bounds_set_text(block, (int)(3.0f * UI_UNIT_X));

  return block;
}

static uiBlock *CurveProfile_buttons_tools(bContext *C, ARegion *region, void *profile_v)
{
  return CurveProfile_tools_func(C, region, (CurveProfile *)profile_v);
}

static void CurveProfile_buttons_zoom_in(bContext *C, void *profile_v, void *UNUSED(arg))
{
  CurveProfile *profile = profile_v;

  /* Allow a 20x zoom. */
  if (BLI_rctf_size_x(&profile->view_rect) > 0.04f * BLI_rctf_size_x(&profile->clip_rect)) {
    const float dx = 0.1154f * BLI_rctf_size_x(&profile->view_rect);
    profile->view_rect.xmin += dx;
    profile->view_rect.xmax -= dx;
    const float dy = 0.1154f * BLI_rctf_size_y(&profile->view_rect);
    profile->view_rect.ymin += dy;
    profile->view_rect.ymax -= dy;
  }

  ED_region_tag_redraw(CTX_wm_region(C));
}

static void CurveProfile_buttons_zoom_out(bContext *C, void *profile_v, void *UNUSED(arg))
{
  CurveProfile *profile = profile_v;

  /* Allow 20 times zoom, but don't view outside clip */
  if (BLI_rctf_size_x(&profile->view_rect) < 20.0f * BLI_rctf_size_x(&profile->clip_rect)) {
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
  CurveProfile *profile = profile_v;

  profile->flag ^= PROF_USE_CLIP;

  BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
  rna_update_cb(C, cb_v, NULL);
}

static void CurveProfile_buttons_reverse(bContext *C, void *cb_v, void *profile_v)
{
  CurveProfile *profile = profile_v;

  BKE_curveprofile_reverse(profile);
  BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
  rna_update_cb(C, cb_v, NULL);
}

static void CurveProfile_buttons_delete(bContext *C, void *cb_v, void *profile_v)
{
  CurveProfile *profile = profile_v;

  BKE_curveprofile_remove_by_flag(profile, SELECT);
  BKE_curveprofile_update(profile, PROF_UPDATE_NONE);

  rna_update_cb(C, cb_v, NULL);
}

static void CurveProfile_buttons_update(bContext *C, void *arg1_v, void *profile_v)
{
  CurveProfile *profile = profile_v;
  BKE_curveprofile_update(profile, PROF_UPDATE_REMOVE_DOUBLES | PROF_UPDATE_CLIP);
  rna_update_cb(C, arg1_v, NULL);
}

static void CurveProfile_buttons_reset(bContext *C, void *arg1_v, void *profile_v)
{
  CurveProfile *profile = profile_v;
  BKE_curveprofile_reset(profile);
  BKE_curveprofile_update(profile, PROF_UPDATE_NONE);
  rna_update_cb(C, arg1_v, NULL);
}

static void CurveProfile_buttons_layout(uiLayout *layout, PointerRNA *ptr, RNAUpdateCb *cb)
{
  CurveProfile *profile = ptr->data;
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
  UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), NULL);

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
                            NULL,
                            0.0,
                            0.0,
                            0.0,
                            0.0,
                            "Reapply and update the preset, removing changes");
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
                    NULL,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    TIP_("Zoom in"));
  UI_but_func_set(bt, CurveProfile_buttons_zoom_in, profile, NULL);

  /* Zoom out */
  bt = uiDefIconBut(block,
                    UI_BTYPE_BUT,
                    0,
                    ICON_ZOOM_OUT,
                    0,
                    0,
                    UI_UNIT_X,
                    UI_UNIT_X,
                    NULL,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    TIP_("Zoom out"));
  UI_but_func_set(bt, CurveProfile_buttons_zoom_out, profile, NULL);

  /* (Right aligned) */
  sub = uiLayoutRow(row, true);
  uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_RIGHT);

  /* Reset view, reset curve */
  bt = uiDefIconBlockBut(
      block, CurveProfile_buttons_tools, profile, 0, 0, 0, 0, UI_UNIT_X, UI_UNIT_X, TIP_("Tools"));
  UI_but_funcN_set(bt, rna_update_cb, MEM_dupallocN(cb), NULL);

  /* Flip path */
  bt = uiDefIconBut(block,
                    UI_BTYPE_BUT,
                    0,
                    ICON_ARROW_LEFTRIGHT,
                    0,
                    0,
                    UI_UNIT_X,
                    UI_UNIT_X,
                    NULL,
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
                    NULL,
                    0.0,
                    0.0,
                    0.0,
                    0.0,
                    TIP_("Toggle Profile Clipping"));
  UI_but_funcN_set(bt, CurveProfile_clipping_toggle, MEM_dupallocN(cb), profile);

  UI_block_funcN_set(block, rna_update_cb, MEM_dupallocN(cb), NULL);

  /* The path itself */
  int path_width = max_ii(uiLayoutGetWidth(layout), UI_UNIT_X);
  path_width = min_ii(path_width, (int)(16.0f * UI_UNIT_X));
  const int path_height = path_width;
  uiLayoutRow(layout, false);
  uiDefBut(block,
           UI_BTYPE_CURVEPROFILE,
           0,
           "",
           0,
           0,
           (short)path_width,
           (short)path_height,
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
  CurveProfilePoint *point = NULL;
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
                      NULL,
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

  uiItemR(layout, ptr, "use_sample_straight_edges", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "use_sample_even_lengths", 0, NULL, ICON_NONE);

  UI_block_funcN_set(block, NULL, NULL, NULL);
}

/**
 * Template for a path creation widget intended for custom bevel profiles.
 * This section is quite similar to #uiTemplateCurveMapping, but with reduced complexity.
 */
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
  RNAUpdateCb *cb = MEM_callocN(sizeof(RNAUpdateCb), "RNAUpdateCb");
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

/* This template now follows User Preference for type - name is not correct anymore... */
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

  uiBut *but = NULL;
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
        hsv_but->gradient_type = UI_GRAD_SV + 3;
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
        hsv_but->gradient_type = UI_GRAD_HS + 3;
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
        hsv_but->gradient_type = UI_GRAD_HV + 3;
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

static void ui_template_palette_menu(bContext *UNUSED(C), uiLayout *layout, void *UNUSED(but_p))
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

void uiTemplatePalette(uiLayout *layout,
                       PointerRNA *ptr,
                       const char *propname,
                       bool UNUSED(colors))
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
  uiBut *but = NULL;

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

  Palette *palette = cptr.data;

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
                NULL);
  uiDefIconButO(block,
                UI_BTYPE_BUT,
                "PALETTE_OT_color_delete",
                WM_OP_INVOKE_DEFAULT,
                ICON_REMOVE,
                0,
                0,
                UI_UNIT_X,
                UI_UNIT_Y,
                NULL);
  if (palette->colors.first != NULL) {
    but = uiDefIconButO(block,
                        UI_BTYPE_BUT,
                        "PALETTE_OT_color_move",
                        WM_OP_INVOKE_DEFAULT,
                        ICON_TRIA_UP,
                        0,
                        0,
                        UI_UNIT_X,
                        UI_UNIT_Y,
                        NULL);
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
                        NULL);
    UI_but_operator_ptr_get(but);
    RNA_enum_set(but->opptr, "type", 1);

    /* Menu. */
    uiDefIconMenuBut(
        block, ui_template_palette_menu, NULL, ICON_SORTSIZE, 0, 0, UI_UNIT_X, UI_UNIT_Y, "");
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
                                 RNA_property_ui_name(prop),
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
  uiBut *but = arg1;
  const int cur = POINTER_AS_INT(arg2);
  wmWindow *win = CTX_wm_window(C);
  const int shift = win->eventstate->shift;

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

/**
 * \todo for now, grouping of layers is determined by dividing up the length of
 * the array of layer bitflags
 */
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

  PropertyRNA *used_prop = NULL;
  if (used_ptr && used_propname) {
    used_prop = RNA_struct_find_property(used_ptr, used_propname);
    if (!used_prop) {
      RNA_warning("used layers property not found: %s.%s",
                  RNA_struct_identifier(ptr->type),
                  used_propname);
      return;
    }

    if (RNA_property_array_length(used_ptr, used_prop) < layers) {
      used_prop = NULL;
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
/** \name List Template
 * \{ */

static void uilist_draw_item_default(struct uiList *ui_list,
                                     struct bContext *UNUSED(C),
                                     struct uiLayout *layout,
                                     struct PointerRNA *UNUSED(dataptr),
                                     struct PointerRNA *itemptr,
                                     int icon,
                                     struct PointerRNA *UNUSED(active_dataptr),
                                     const char *UNUSED(active_propname),
                                     int UNUSED(index),
                                     int UNUSED(flt_flag))
{
  PropertyRNA *nameprop = RNA_struct_name_property(itemptr->type);

  /* Simplest one! */
  switch (ui_list->layout_type) {
    case UILST_LAYOUT_GRID:
      uiItemL(layout, "", icon);
      break;
    case UILST_LAYOUT_DEFAULT:
    case UILST_LAYOUT_COMPACT:
    default:
      if (nameprop) {
        uiItemFullR(layout, itemptr, nameprop, RNA_NO_INDEX, 0, UI_ITEM_R_NO_BG, "", icon);
      }
      else {
        uiItemL(layout, "", icon);
      }
      break;
  }
}

static void uilist_draw_filter_default(struct uiList *ui_list,
                                       struct bContext *UNUSED(C),
                                       struct uiLayout *layout)
{
  PointerRNA listptr;
  RNA_pointer_create(NULL, &RNA_UIList, ui_list, &listptr);

  uiLayout *row = uiLayoutRow(layout, false);

  uiLayout *subrow = uiLayoutRow(row, true);
  uiItemR(subrow, &listptr, "filter_name", 0, "", ICON_NONE);
  uiItemR(subrow,
          &listptr,
          "use_filter_invert",
          UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
          "",
          ICON_ARROW_LEFTRIGHT);

  if ((ui_list->filter_sort_flag & UILST_FLT_SORT_LOCK) == 0) {
    subrow = uiLayoutRow(row, true);
    uiItemR(subrow,
            &listptr,
            "use_filter_sort_alpha",
            UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
            "",
            ICON_NONE);
    uiItemR(subrow,
            &listptr,
            "use_filter_sort_reverse",
            UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
            "",
            (ui_list->filter_sort_flag & UILST_FLT_SORT_REVERSE) ? ICON_SORT_DESC : ICON_SORT_ASC);
  }
}

typedef struct {
  char name[MAX_IDPROP_NAME];
  int org_idx;
} StringCmp;

static int cmpstringp(const void *p1, const void *p2)
{
  /* Case-insensitive comparison. */
  return BLI_strcasecmp(((StringCmp *)p1)->name, ((StringCmp *)p2)->name);
}

static void uilist_filter_items_default(struct uiList *ui_list,
                                        struct bContext *UNUSED(C),
                                        struct PointerRNA *dataptr,
                                        const char *propname)
{
  uiListDyn *dyn_data = ui_list->dyn_data;
  PropertyRNA *prop = RNA_struct_find_property(dataptr, propname);

  const char *filter_raw = ui_list->filter_byname;
  char *filter = (char *)filter_raw, filter_buff[32], *filter_dyn = NULL;
  const bool filter_exclude = (ui_list->filter_flag & UILST_FLT_EXCLUDE) != 0;
  const bool order_by_name = (ui_list->filter_sort_flag & UILST_FLT_SORT_MASK) ==
                             UILST_FLT_SORT_ALPHA;
  const int len = RNA_property_collection_length(dataptr, prop);

  dyn_data->items_shown = dyn_data->items_len = len;

  if (len && (order_by_name || filter_raw[0])) {
    StringCmp *names = NULL;
    int order_idx = 0, i = 0;

    if (order_by_name) {
      names = MEM_callocN(sizeof(StringCmp) * len, "StringCmp");
    }
    if (filter_raw[0]) {
      const size_t slen = strlen(filter_raw);

      dyn_data->items_filter_flags = MEM_callocN(sizeof(int) * len, "items_filter_flags");
      dyn_data->items_shown = 0;

      /* Implicitly add heading/trailing wildcards if needed. */
      if (slen + 3 <= sizeof(filter_buff)) {
        filter = filter_buff;
      }
      else {
        filter = filter_dyn = MEM_mallocN((slen + 3) * sizeof(char), "filter_dyn");
      }
      BLI_strncpy_ensure_pad(filter, filter_raw, '*', slen + 3);
    }

    RNA_PROP_BEGIN (dataptr, itemptr, prop) {
      bool do_order = false;

      char *namebuf = RNA_struct_name_get_alloc(&itemptr, NULL, 0, NULL);
      const char *name = namebuf ? namebuf : "";

      if (filter[0]) {
        /* Case-insensitive! */
        if (fnmatch(filter, name, FNM_CASEFOLD) == 0) {
          dyn_data->items_filter_flags[i] = UILST_FLT_ITEM;
          if (!filter_exclude) {
            dyn_data->items_shown++;
            do_order = order_by_name;
          }
          // printf("%s: '%s' matches '%s'\n", __func__, name, filter);
        }
        else if (filter_exclude) {
          dyn_data->items_shown++;
          do_order = order_by_name;
        }
      }
      else {
        do_order = order_by_name;
      }

      if (do_order) {
        names[order_idx].org_idx = order_idx;
        BLI_strncpy(names[order_idx++].name, name, MAX_IDPROP_NAME);
      }

      /* free name */
      if (namebuf) {
        MEM_freeN(namebuf);
      }
      i++;
    }
    RNA_PROP_END;

    if (order_by_name) {
      int new_idx;
      /* note: order_idx equals either to ui_list->items_len if no filtering done,
       *       or to ui_list->items_shown if filter is enabled,
       *       or to (ui_list->items_len - ui_list->items_shown) if filtered items are excluded.
       *       This way, we only sort items we actually intend to draw!
       */
      qsort(names, order_idx, sizeof(StringCmp), cmpstringp);

      dyn_data->items_filter_neworder = MEM_mallocN(sizeof(int) * order_idx,
                                                    "items_filter_neworder");
      for (new_idx = 0; new_idx < order_idx; new_idx++) {
        dyn_data->items_filter_neworder[names[new_idx].org_idx] = new_idx;
      }
    }

    if (filter_dyn) {
      MEM_freeN(filter_dyn);
    }
    if (names) {
      MEM_freeN(names);
    }
  }
}

typedef struct {
  PointerRNA item;
  int org_idx;
  int flt_flag;
} _uilist_item;

typedef struct {
  int visual_items; /* Visual number of items (i.e. number of items we have room to display). */
  int start_idx;    /* Index of first item to display. */
  int end_idx;      /* Index of last item to display + 1. */
} uiListLayoutdata;

static void uilist_prepare(uiList *ui_list,
                           int len,
                           int activei,
                           int rows,
                           int maxrows,
                           int columns,
                           uiListLayoutdata *layoutdata)
{
  uiListDyn *dyn_data = ui_list->dyn_data;
  const bool use_auto_size = (ui_list->list_grip < (rows - UI_LIST_AUTO_SIZE_THRESHOLD));

  /* default rows */
  if (rows <= 0) {
    rows = 5;
  }
  dyn_data->visual_height_min = rows;
  if (maxrows < rows) {
    maxrows = max_ii(rows, 5);
  }
  if (columns <= 0) {
    columns = 9;
  }

  int activei_row;
  if (columns > 1) {
    dyn_data->height = (int)ceil((double)len / (double)columns);
    activei_row = (int)floor((double)activei / (double)columns);
  }
  else {
    dyn_data->height = len;
    activei_row = activei;
  }

  if (!use_auto_size) {
    /* No auto-size, yet we clamp at min size! */
    maxrows = rows = max_ii(ui_list->list_grip, rows);
  }
  else if ((rows != maxrows) && (dyn_data->height > rows)) {
    /* Expand size if needed and possible. */
    rows = min_ii(dyn_data->height, maxrows);
  }

  /* If list length changes or list is tagged to check this,
   * and active is out of view, scroll to it .*/
  if (ui_list->list_last_len != len || ui_list->flag & UILST_SCROLL_TO_ACTIVE_ITEM) {
    if (activei_row < ui_list->list_scroll) {
      ui_list->list_scroll = activei_row;
    }
    else if (activei_row >= ui_list->list_scroll + rows) {
      ui_list->list_scroll = activei_row - rows + 1;
    }
    ui_list->flag &= ~UILST_SCROLL_TO_ACTIVE_ITEM;
  }

  const int max_scroll = max_ii(0, dyn_data->height - rows);
  CLAMP(ui_list->list_scroll, 0, max_scroll);
  ui_list->list_last_len = len;
  dyn_data->visual_height = rows;
  layoutdata->visual_items = rows * columns;
  layoutdata->start_idx = ui_list->list_scroll * columns;
  layoutdata->end_idx = min_ii(layoutdata->start_idx + rows * columns, len);
}

static void uilist_resize_update_cb(bContext *C, void *arg1, void *UNUSED(arg2))
{
  uiList *ui_list = arg1;
  uiListDyn *dyn_data = ui_list->dyn_data;

  /* This way we get diff in number of additional items to show (positive) or hide (negative). */
  const int diff = round_fl_to_int((float)(dyn_data->resize - dyn_data->resize_prev) /
                                   (float)UI_UNIT_Y);

  if (diff != 0) {
    ui_list->list_grip += diff;
    dyn_data->resize_prev += diff * UI_UNIT_Y;
    ui_list->flag |= UILST_SCROLL_TO_ACTIVE_ITEM;
  }

  /* In case uilist is in popup, we need special refreshing */
  ED_region_tag_refresh_ui(CTX_wm_menu(C));
}

static void *uilist_item_use_dynamic_tooltip(PointerRNA *itemptr, const char *propname)
{
  if (propname && propname[0] && itemptr && itemptr->data) {
    PropertyRNA *prop = RNA_struct_find_property(itemptr, propname);

    if (prop && (RNA_property_type(prop) == PROP_STRING)) {
      return RNA_property_string_get_alloc(itemptr, prop, NULL, 0, NULL);
    }
  }
  return NULL;
}

static char *uilist_item_tooltip_func(bContext *UNUSED(C), void *argN, const char *tip)
{
  char *dyn_tooltip = argN;
  return BLI_sprintfN("%s - %s", tip, dyn_tooltip);
}

void uiTemplateList(uiLayout *layout,
                    bContext *C,
                    const char *listtype_name,
                    const char *list_id,
                    PointerRNA *dataptr,
                    const char *propname,
                    PointerRNA *active_dataptr,
                    const char *active_propname,
                    const char *item_dyntip_propname,
                    int rows,
                    int maxrows,
                    int layout_type,
                    int columns,
                    bool sort_reverse,
                    bool sort_lock)
{
  PropertyRNA *prop = NULL, *activeprop;
  _uilist_item *items_ptr = NULL;
  uiLayout *glob = NULL, *box, *row, *col, *subrow, *sub, *overlap;
  uiBut *but;

  uiListLayoutdata layoutdata;
  char ui_list_id[UI_MAX_NAME_STR];
  char numstr[32];
  int rnaicon = ICON_NONE, icon = ICON_NONE;
  int i = 0, activei = 0;
  int len = 0;

  /* validate arguments */
  /* Forbid default UI_UL_DEFAULT_CLASS_NAME list class without a custom list_id! */
  if (STREQ(UI_UL_DEFAULT_CLASS_NAME, listtype_name) && !(list_id && list_id[0])) {
    RNA_warning("template_list using default '%s' UIList class must provide a custom list_id",
                UI_UL_DEFAULT_CLASS_NAME);
    return;
  }

  uiBlock *block = uiLayoutGetBlock(layout);

  if (!active_dataptr->data) {
    RNA_warning("No active data");
    return;
  }

  if (dataptr->data) {
    prop = RNA_struct_find_property(dataptr, propname);
    if (!prop) {
      RNA_warning("Property not found: %s.%s", RNA_struct_identifier(dataptr->type), propname);
      return;
    }
  }

  activeprop = RNA_struct_find_property(active_dataptr, active_propname);
  if (!activeprop) {
    RNA_warning(
        "Property not found: %s.%s", RNA_struct_identifier(active_dataptr->type), active_propname);
    return;
  }

  if (prop) {
    const PropertyType type = RNA_property_type(prop);
    if (type != PROP_COLLECTION) {
      RNA_warning("Expected a collection data property");
      return;
    }
  }

  const PropertyType activetype = RNA_property_type(activeprop);
  if (activetype != PROP_INT) {
    RNA_warning("Expected an integer active data property");
    return;
  }

  /* get icon */
  if (dataptr->data && prop) {
    StructRNA *ptype = RNA_property_pointer_type(dataptr, prop);
    rnaicon = RNA_struct_ui_icon(ptype);
  }

  /* get active data */
  activei = RNA_property_int_get(active_dataptr, activeprop);

  /* Find the uiList type. */
  uiListType *ui_list_type = WM_uilisttype_find(listtype_name, false);

  if (ui_list_type == NULL) {
    RNA_warning("List type %s not found", listtype_name);
    return;
  }

  uiListDrawItemFunc draw_item = ui_list_type->draw_item ? ui_list_type->draw_item :
                                                           uilist_draw_item_default;
  uiListDrawFilterFunc draw_filter = ui_list_type->draw_filter ? ui_list_type->draw_filter :
                                                                 uilist_draw_filter_default;
  uiListFilterItemsFunc filter_items = ui_list_type->filter_items ? ui_list_type->filter_items :
                                                                    uilist_filter_items_default;

  /* Find or add the uiList to the current Region. */
  /* We tag the list id with the list type... */
  BLI_snprintf(
      ui_list_id, sizeof(ui_list_id), "%s_%s", ui_list_type->idname, list_id ? list_id : "");

  /* Allows to work in popups. */
  ARegion *region = CTX_wm_menu(C);
  if (region == NULL) {
    region = CTX_wm_region(C);
  }
  uiList *ui_list = BLI_findstring(&region->ui_lists, ui_list_id, offsetof(uiList, list_id));

  if (!ui_list) {
    ui_list = MEM_callocN(sizeof(uiList), "uiList");
    BLI_strncpy(ui_list->list_id, ui_list_id, sizeof(ui_list->list_id));
    BLI_addtail(&region->ui_lists, ui_list);
    ui_list->list_grip = -UI_LIST_AUTO_SIZE_THRESHOLD; /* Force auto size by default. */
    if (sort_reverse) {
      ui_list->filter_sort_flag |= UILST_FLT_SORT_REVERSE;
    }
    if (sort_lock) {
      ui_list->filter_sort_flag |= UILST_FLT_SORT_LOCK;
    }
  }

  if (!ui_list->dyn_data) {
    ui_list->dyn_data = MEM_callocN(sizeof(uiListDyn), "uiList.dyn_data");
  }
  uiListDyn *dyn_data = ui_list->dyn_data;

  /* Because we can't actually pass type across save&load... */
  ui_list->type = ui_list_type;
  ui_list->layout_type = layout_type;

  /* Reset filtering data. */
  MEM_SAFE_FREE(dyn_data->items_filter_flags);
  MEM_SAFE_FREE(dyn_data->items_filter_neworder);
  dyn_data->items_len = dyn_data->items_shown = -1;

  /* When active item changed since last draw, scroll to it. */
  if (activei != ui_list->list_last_activei) {
    ui_list->flag |= UILST_SCROLL_TO_ACTIVE_ITEM;
    ui_list->list_last_activei = activei;
  }

  /* Filter list items! (not for compact layout, though) */
  if (dataptr->data && prop) {
    const int filter_exclude = ui_list->filter_flag & UILST_FLT_EXCLUDE;
    const bool order_reverse = (ui_list->filter_sort_flag & UILST_FLT_SORT_REVERSE) != 0;
    int items_shown, idx = 0;
#if 0
    int prev_ii = -1, prev_i;
#endif

    if (layout_type == UILST_LAYOUT_COMPACT) {
      dyn_data->items_len = dyn_data->items_shown = RNA_property_collection_length(dataptr, prop);
    }
    else {
      // printf("%s: filtering...\n", __func__);
      filter_items(ui_list, C, dataptr, propname);
      // printf("%s: filtering done.\n", __func__);
    }

    items_shown = dyn_data->items_shown;
    if (items_shown >= 0) {
      bool activei_mapping_pending = true;
      items_ptr = MEM_mallocN(sizeof(_uilist_item) * items_shown, __func__);
      // printf("%s: items shown: %d.\n", __func__, items_shown);
      RNA_PROP_BEGIN (dataptr, itemptr, prop) {
        if (!dyn_data->items_filter_flags ||
            ((dyn_data->items_filter_flags[i] & UILST_FLT_ITEM) ^ filter_exclude)) {
          int ii;
          if (dyn_data->items_filter_neworder) {
            ii = dyn_data->items_filter_neworder[idx++];
            ii = order_reverse ? items_shown - ii - 1 : ii;
          }
          else {
            ii = order_reverse ? items_shown - ++idx : idx++;
          }
          // printf("%s: ii: %d\n", __func__, ii);
          items_ptr[ii].item = itemptr;
          items_ptr[ii].org_idx = i;
          items_ptr[ii].flt_flag = dyn_data->items_filter_flags ? dyn_data->items_filter_flags[i] :
                                                                  0;

          if (activei_mapping_pending && activei == i) {
            activei = ii;
            /* So that we do not map again activei! */
            activei_mapping_pending = false;
          }
#if 0 /* For now, do not alter active element, even if it will be hidden... */
          else if (activei < i) {
            /* We do not want an active but invisible item!
             * Only exception is when all items are filtered out...
             */
            if (prev_ii >= 0) {
              activei = prev_ii;
              RNA_property_int_set(active_dataptr, activeprop, prev_i);
            }
            else {
              activei = ii;
              RNA_property_int_set(active_dataptr, activeprop, i);
            }
          }
          prev_i = i;
          prev_ii = ii;
#endif
        }
        i++;
      }
      RNA_PROP_END;

      if (activei_mapping_pending) {
        /* No active item found, set to 'invalid' -1 value... */
        activei = -1;
      }
    }
    if (dyn_data->items_shown >= 0) {
      len = dyn_data->items_shown;
    }
    else {
      len = dyn_data->items_len;
    }
  }

  switch (layout_type) {
    case UILST_LAYOUT_DEFAULT:
      /* layout */
      box = uiLayoutListBox(layout, ui_list, active_dataptr, activeprop);
      glob = uiLayoutColumn(box, true);
      row = uiLayoutRow(glob, false);
      col = uiLayoutColumn(row, true);

      /* init numbers */
      uilist_prepare(ui_list, len, activei, rows, maxrows, 1, &layoutdata);

      if (dataptr->data && prop) {
        /* create list items */
        for (i = layoutdata.start_idx; i < layoutdata.end_idx; i++) {
          PointerRNA *itemptr = &items_ptr[i].item;
          void *dyntip_data;
          const int org_i = items_ptr[i].org_idx;
          const int flt_flag = items_ptr[i].flt_flag;
          uiBlock *subblock = uiLayoutGetBlock(col);

          overlap = uiLayoutOverlap(col);

          UI_block_flag_enable(subblock, UI_BLOCK_LIST_ITEM);

          /* list item behind label & other buttons */
          sub = uiLayoutRow(overlap, false);

          but = uiDefButR_prop(subblock,
                               UI_BTYPE_LISTROW,
                               0,
                               "",
                               0,
                               0,
                               UI_UNIT_X * 10,
                               UI_UNIT_Y,
                               active_dataptr,
                               activeprop,
                               0,
                               0,
                               org_i,
                               0,
                               0,
                               TIP_("Double click to rename"));
          if ((dyntip_data = uilist_item_use_dynamic_tooltip(itemptr, item_dyntip_propname))) {
            UI_but_func_tooltip_set(but, uilist_item_tooltip_func, dyntip_data);
          }

          sub = uiLayoutRow(overlap, false);

          icon = UI_icon_from_rnaptr(C, itemptr, rnaicon, false);
          if (icon == ICON_DOT) {
            icon = ICON_NONE;
          }
          draw_item(ui_list,
                    C,
                    sub,
                    dataptr,
                    itemptr,
                    icon,
                    active_dataptr,
                    active_propname,
                    org_i,
                    flt_flag);

          /* Items should be able to set context pointers for the layout. But the list-row button
           * swallows events, so it needs the context storage too for handlers to see it. */
          but->context = uiLayoutGetContextStore(sub);

          /* If we are "drawing" active item, set all labels as active. */
          if (i == activei) {
            ui_layout_list_set_labels_active(sub);
          }

          UI_block_flag_disable(subblock, UI_BLOCK_LIST_ITEM);
        }
      }

      /* add dummy buttons to fill space */
      for (; i < layoutdata.start_idx + layoutdata.visual_items; i++) {
        uiItemL(col, "", ICON_NONE);
      }

      /* add scrollbar */
      if (len > layoutdata.visual_items) {
        col = uiLayoutColumn(row, false);
        uiDefButI(block,
                  UI_BTYPE_SCROLL,
                  0,
                  "",
                  0,
                  0,
                  V2D_SCROLL_WIDTH,
                  UI_UNIT_Y * dyn_data->visual_height,
                  &ui_list->list_scroll,
                  0,
                  dyn_data->height - dyn_data->visual_height,
                  dyn_data->visual_height,
                  0,
                  "");
      }
      break;
    case UILST_LAYOUT_COMPACT:
      row = uiLayoutRow(layout, true);

      if ((dataptr->data && prop) && (dyn_data->items_shown > 0) && (activei >= 0) &&
          (activei < dyn_data->items_shown)) {
        PointerRNA *itemptr = &items_ptr[activei].item;
        const int org_i = items_ptr[activei].org_idx;

        icon = UI_icon_from_rnaptr(C, itemptr, rnaicon, false);
        if (icon == ICON_DOT) {
          icon = ICON_NONE;
        }
        draw_item(
            ui_list, C, row, dataptr, itemptr, icon, active_dataptr, active_propname, org_i, 0);
      }
      /* if list is empty, add in dummy button */
      else {
        uiItemL(row, "", ICON_NONE);
      }

      /* next/prev button */
      BLI_snprintf(numstr, sizeof(numstr), "%d :", dyn_data->items_shown);
      but = uiDefIconTextButR_prop(block,
                                   UI_BTYPE_NUM,
                                   0,
                                   0,
                                   numstr,
                                   0,
                                   0,
                                   UI_UNIT_X * 5,
                                   UI_UNIT_Y,
                                   active_dataptr,
                                   activeprop,
                                   0,
                                   0,
                                   0,
                                   0,
                                   0,
                                   "");
      if (dyn_data->items_shown == 0) {
        UI_but_flag_enable(but, UI_BUT_DISABLED);
      }
      break;
    case UILST_LAYOUT_GRID:
      box = uiLayoutListBox(layout, ui_list, active_dataptr, activeprop);
      glob = uiLayoutColumn(box, true);
      row = uiLayoutRow(glob, false);
      col = uiLayoutColumn(row, true);
      subrow = NULL; /* Quite gcc warning! */

      uilist_prepare(ui_list, len, activei, rows, maxrows, columns, &layoutdata);

      if (dataptr->data && prop) {
        /* create list items */
        for (i = layoutdata.start_idx; i < layoutdata.end_idx; i++) {
          PointerRNA *itemptr = &items_ptr[i].item;
          const int org_i = items_ptr[i].org_idx;
          const int flt_flag = items_ptr[i].flt_flag;

          /* create button */
          if (!(i % columns)) {
            subrow = uiLayoutRow(col, false);
          }

          uiBlock *subblock = uiLayoutGetBlock(subrow);
          overlap = uiLayoutOverlap(subrow);

          UI_block_flag_enable(subblock, UI_BLOCK_LIST_ITEM);

          /* list item behind label & other buttons */
          sub = uiLayoutRow(overlap, false);

          but = uiDefButR_prop(subblock,
                               UI_BTYPE_LISTROW,
                               0,
                               "",
                               0,
                               0,
                               UI_UNIT_X * 10,
                               UI_UNIT_Y,
                               active_dataptr,
                               activeprop,
                               0,
                               0,
                               org_i,
                               0,
                               0,
                               NULL);
          UI_but_drawflag_enable(but, UI_BUT_NO_TOOLTIP);

          sub = uiLayoutRow(overlap, false);

          icon = UI_icon_from_rnaptr(C, itemptr, rnaicon, false);
          draw_item(ui_list,
                    C,
                    sub,
                    dataptr,
                    itemptr,
                    icon,
                    active_dataptr,
                    active_propname,
                    org_i,
                    flt_flag);

          /* If we are "drawing" active item, set all labels as active. */
          if (i == activei) {
            ui_layout_list_set_labels_active(sub);
          }

          UI_block_flag_disable(subblock, UI_BLOCK_LIST_ITEM);
        }
      }

      /* add dummy buttons to fill space */
      for (; i < layoutdata.start_idx + layoutdata.visual_items; i++) {
        if (!(i % columns)) {
          subrow = uiLayoutRow(col, false);
        }
        uiItemL(subrow, "", ICON_NONE);
      }

      /* add scrollbar */
      if (len > layoutdata.visual_items) {
        /* col = */ uiLayoutColumn(row, false);
        uiDefButI(block,
                  UI_BTYPE_SCROLL,
                  0,
                  "",
                  0,
                  0,
                  V2D_SCROLL_WIDTH,
                  UI_UNIT_Y * dyn_data->visual_height,
                  &ui_list->list_scroll,
                  0,
                  dyn_data->height - dyn_data->visual_height,
                  dyn_data->visual_height,
                  0,
                  "");
      }
      break;
  }

  if (glob) {
    /* About #UI_BTYPE_GRIP drag-resize:
     * We can't directly use results from a grip button, since we have a
     * rather complex behavior here (sizing by discrete steps and, overall, auto-size feature).
     * Since we *never* know whether we are grip-resizing or not
     * (because there is no callback for when a button enters/leaves its "edit mode"),
     * we use the fact that grip-controlled value (dyn_data->resize) is completely handled
     * by the grip during the grab resize, so settings its value here has no effect at all.
     *
     * It is only meaningful when we are not resizing,
     * in which case this gives us the correct "init drag" value.
     * Note we cannot affect `dyn_data->resize_prev here`,
     * since this value is not controlled by the grip!
     */
    dyn_data->resize = dyn_data->resize_prev +
                       (dyn_data->visual_height - ui_list->list_grip) * UI_UNIT_Y;

    row = uiLayoutRow(glob, true);
    uiBlock *subblock = uiLayoutGetBlock(row);
    UI_block_emboss_set(subblock, UI_EMBOSS_NONE);

    if (ui_list->filter_flag & UILST_FLT_SHOW) {
      but = uiDefIconButBitI(subblock,
                             UI_BTYPE_TOGGLE,
                             UILST_FLT_SHOW,
                             0,
                             ICON_DISCLOSURE_TRI_DOWN,
                             0,
                             0,
                             UI_UNIT_X,
                             UI_UNIT_Y * 0.5f,
                             &(ui_list->filter_flag),
                             0,
                             0,
                             0,
                             0,
                             TIP_("Hide filtering options"));
      UI_but_flag_disable(but, UI_BUT_UNDO); /* skip undo on screen buttons */

      but = uiDefIconButI(subblock,
                          UI_BTYPE_GRIP,
                          0,
                          ICON_GRIP,
                          0,
                          0,
                          UI_UNIT_X * 10.0f,
                          UI_UNIT_Y * 0.5f,
                          &dyn_data->resize,
                          0.0,
                          0.0,
                          0,
                          0,
                          "");
      UI_but_func_set(but, uilist_resize_update_cb, ui_list, NULL);

      UI_block_emboss_set(subblock, UI_EMBOSS);

      col = uiLayoutColumn(glob, false);
      subblock = uiLayoutGetBlock(col);
      uiDefBut(subblock,
               UI_BTYPE_SEPR,
               0,
               "",
               0,
               0,
               UI_UNIT_X,
               UI_UNIT_Y * 0.05f,
               NULL,
               0.0,
               0.0,
               0,
               0,
               "");

      draw_filter(ui_list, C, col);
    }
    else {
      but = uiDefIconButBitI(subblock,
                             UI_BTYPE_TOGGLE,
                             UILST_FLT_SHOW,
                             0,
                             ICON_DISCLOSURE_TRI_RIGHT,
                             0,
                             0,
                             UI_UNIT_X,
                             UI_UNIT_Y * 0.5f,
                             &(ui_list->filter_flag),
                             0,
                             0,
                             0,
                             0,
                             TIP_("Show filtering options"));
      UI_but_flag_disable(but, UI_BUT_UNDO); /* skip undo on screen buttons */

      but = uiDefIconButI(subblock,
                          UI_BTYPE_GRIP,
                          0,
                          ICON_GRIP,
                          0,
                          0,
                          UI_UNIT_X * 10.0f,
                          UI_UNIT_Y * 0.5f,
                          &dyn_data->resize,
                          0.0,
                          0.0,
                          0,
                          0,
                          "");
      UI_but_func_set(but, uilist_resize_update_cb, ui_list, NULL);

      UI_block_emboss_set(subblock, UI_EMBOSS);
    }
  }

  if (items_ptr) {
    MEM_freeN(items_ptr);
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

static void do_running_jobs(bContext *C, void *UNUSED(arg), int event)
{
  switch (event) {
    case B_STOPRENDER:
      G.is_break = true;
      break;
    case B_STOPCAST:
      WM_jobs_stop(CTX_wm_manager(C), CTX_wm_screen(C), NULL);
      break;
    case B_STOPANIM:
      WM_operator_name_call(C, "SCREEN_OT_animation_play", WM_OP_INVOKE_SCREEN, NULL);
      break;
    case B_STOPCOMPO:
      WM_jobs_stop(CTX_wm_manager(C), CTX_data_scene(C), NULL);
      break;
    case B_STOPSEQ:
      WM_jobs_stop(CTX_wm_manager(C), CTX_data_scene(C), NULL);
      break;
    case B_STOPCLIP:
      WM_jobs_stop(CTX_wm_manager(C), CTX_data_scene(C), NULL);
      break;
    case B_STOPFILE:
      WM_jobs_stop(CTX_wm_manager(C), CTX_data_scene(C), NULL);
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

static char *progress_tooltip_func(bContext *UNUSED(C), void *argN, const char *UNUSED(tip))
{
  struct ProgressTooltip_Store *arg = argN;
  wmWindowManager *wm = arg->wm;
  void *owner = arg->owner;

  const float progress = WM_jobs_progress(wm, owner);

  /* create tooltip text and associate it with the job */
  char elapsed_str[32];
  char remaining_str[32] = "Unknown";
  const double elapsed = PIL_check_seconds_timer() - WM_jobs_starttime(wm, owner);
  BLI_timecode_string_from_time_simple(elapsed_str, sizeof(elapsed_str), elapsed);

  if (progress) {
    const double remaining = (elapsed / (double)progress) - elapsed;
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
  void *owner = NULL;
  int handle_event, icon = 0;

  uiBlock *block = uiLayoutGetBlock(layout);
  UI_block_layout_set_current(block, layout);

  UI_block_func_handle_set(block, do_running_jobs, NULL);

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
    BLI_snprintf(text, 8, "%d%%", (int)(progress * 100));

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
                     NULL,
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
      struct ProgressTooltip_Store *tip_arg = MEM_mallocN(sizeof(*tip_arg), __func__);
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
                                                                            NULL,
                                                                            0.0f,
                                                                            0.0f,
                                                                            0.0f,
                                                                            0,
                                                                            NULL);

      but_progress->progress = progress;
      UI_but_func_tooltip_set(&but_progress->but, progress_tooltip_func, tip_arg);
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
                       NULL,
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
                     NULL,
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

  UI_fontstyle_set(&style->widgetlabel);
  int width = BLF_width(style->widgetlabel.uifont_id, report->message, report->len);
  width = min_ii((int)(rti->widthfac * width), width);
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
                 NULL,
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
                 NULL,
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
                  "Show in Info Log");
}

void uiTemplateInputStatus(uiLayout *layout, struct bContext *C)
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

    const char *msg = WM_window_cursor_keymap_status_get(win, i, 0);
    const char *msg_drag = WM_window_cursor_keymap_status_get(win, i, 1);

    if (msg || (msg_drag == NULL)) {
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

static void keymap_item_modified(bContext *UNUSED(C), void *kmi_p, void *UNUSED(unused))
{
  wmKeyMapItem *kmi = (wmKeyMapItem *)kmi_p;
  WM_keyconfig_update_tag(NULL, kmi);
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
    uiItemFullR(row, ptr, prop, -1, 0, 0, NULL, ICON_NONE);

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
                          NULL);
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
    uiBut *but = uiLayoutGetBlock(layout)->buttons.last;

    WM_operator_properties_sanitize(&propptr, false);
    template_keymap_item_properties(layout, NULL, &propptr);

    /* attach callbacks to compensate for missing properties update,
     * we don't know which keymap (item) is being modified there */
    for (; but; but = but->next) {
      /* operator buttons may store props for use (file selector, T36492) */
      if (but->rnaprop) {
        UI_but_func_set(but, keymap_item_modified, ptr->data, NULL);

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

bool uiTemplateEventFromKeymapItem(struct uiLayout *layout,
                                   const char *text,
                                   const struct wmKeyMapItem *kmi,
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
    uiItemL(layout, text, icon);
    ok = true;
  }
  else if (text_fallback) {
    const char *event_text = WM_key_event_string(kmi->type, true);
    uiItemL(layout, event_text, ICON_NONE);
    uiItemL(layout, text, ICON_NONE);
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
                                        bContext *UNUSED(C),
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
  ColorManagedViewSettings *view_settings = view_transform_ptr.data;

  uiLayout *col = uiLayoutColumn(layout, false);

  uiLayout *row = uiLayoutRow(col, false);
  uiItemR(row, &view_transform_ptr, "view_transform", 0, IFACE_("View"), ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, &view_transform_ptr, "exposure", 0, NULL, ICON_NONE);
  uiItemR(col, &view_transform_ptr, "gamma", 0, NULL, ICON_NONE);

  uiItemR(col, &view_transform_ptr, "look", 0, IFACE_("Look"), ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, &view_transform_ptr, "use_curve_mapping", 0, NULL, ICON_NONE);
  if (view_settings->flag & COLORMANAGE_VIEW_USE_CURVES) {
    uiTemplateCurveMapping(
        col, &view_transform_ptr, "curve_mapping", 'c', true, false, false, false);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Component Menu
 * \{ */

typedef struct ComponentMenuArgs {
  PointerRNA ptr;
  char propname[64]; /* XXX arbitrary */
} ComponentMenuArgs;
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
  ComponentMenuArgs *args = MEM_callocN(sizeof(ComponentMenuArgs), "component menu template args");

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

void uiTemplateNodeSocket(uiLayout *layout, bContext *UNUSED(C), float *color)
{
  uiBlock *block = uiLayoutGetBlock(layout);
  UI_block_align_begin(block);

  /* XXX using explicit socket colors is not quite ideal.
   * Eventually it should be possible to use theme colors for this purpose,
   * but this requires a better design for extendable color palettes in user prefs.
   */
  uiBut *but = uiDefBut(
      block, UI_BTYPE_NODE_SOCKET, 0, "", 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
  rgba_float_to_uchar(but->col, color);

  UI_block_align_end(block);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cache File Template
 * \{ */

void uiTemplateCacheFile(uiLayout *layout,
                         const bContext *C,
                         PointerRNA *ptr,
                         const char *propname)
{
  if (!ptr->data) {
    return;
  }

  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    printf(
        "%s: property not found: %s.%s\n", __func__, RNA_struct_identifier(ptr->type), propname);
    return;
  }

  if (RNA_property_type(prop) != PROP_POINTER) {
    printf("%s: expected pointer property for %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname);
    return;
  }

  PointerRNA fileptr = RNA_property_pointer_get(ptr, prop);
  CacheFile *file = fileptr.data;

  uiLayoutSetContextPointer(layout, "edit_cachefile", &fileptr);

  uiTemplateID(layout,
               C,
               ptr,
               propname,
               NULL,
               "CACHEFILE_OT_open",
               NULL,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               NULL);

  if (!file) {
    return;
  }

  SpaceProperties *sbuts = CTX_wm_space_properties(C);

  uiLayout *row, *sub, *subsub;

  uiLayoutSetPropSep(layout, true);

  row = uiLayoutRow(layout, true);
  uiItemR(row, &fileptr, "filepath", 0, NULL, ICON_NONE);
  sub = uiLayoutRow(row, true);
  uiItemO(sub, "", ICON_FILE_REFRESH, "cachefile.reload");

  row = uiLayoutRow(layout, false);
  uiItemR(row, &fileptr, "is_sequence", 0, NULL, ICON_NONE);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Override Frame"));
  sub = uiLayoutRow(row, true);
  uiLayoutSetPropDecorate(sub, false);
  uiItemR(sub, &fileptr, "override_frame", 0, "", ICON_NONE);
  subsub = uiLayoutRow(sub, true);
  uiLayoutSetActive(subsub, RNA_boolean_get(&fileptr, "override_frame"));
  uiItemR(subsub, &fileptr, "frame", 0, "", ICON_NONE);
  uiItemDecoratorR(row, &fileptr, "frame", 0);

  row = uiLayoutRow(layout, false);
  uiItemR(row, &fileptr, "frame_offset", 0, NULL, ICON_NONE);
  uiLayoutSetActive(row, !RNA_boolean_get(&fileptr, "is_sequence"));

  if (sbuts->mainb == BCONTEXT_CONSTRAINT) {
    row = uiLayoutRow(layout, false);
    uiItemR(row, &fileptr, "scale", 0, IFACE_("Manual Scale"), ICON_NONE);
  }

  uiItemR(layout, &fileptr, "velocity_name", 0, NULL, ICON_NONE);
  uiItemR(layout, &fileptr, "velocity_unit", 0, NULL, ICON_NONE);

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
                NULL,
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
