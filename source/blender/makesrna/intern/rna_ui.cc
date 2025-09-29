/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_screen_types.h"

#include "BLT_translation.hh"

#include "BKE_file_handler.hh"
#include "BKE_screen.hh"

#include "RNA_define.hh"

#include "RNA_enum_types.hh"
#include "rna_internal.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "WM_toolsystem.hh"
#include "WM_types.hh"

/* see WM_types.hh */
using wmOpCallContext = blender::wm::OpCallContext;

/* clang-format off */
const EnumPropertyItem rna_enum_operator_context_items[] = {
    {int(wmOpCallContext::InvokeDefault), "INVOKE_DEFAULT", 0, "Invoke Default", ""},
    {int(wmOpCallContext::InvokeRegionWin), "INVOKE_REGION_WIN", 0, "Invoke Region Window", ""},
    {int(wmOpCallContext::InvokeRegionChannels), "INVOKE_REGION_CHANNELS", 0, "Invoke Region Channels", ""},
    {int(wmOpCallContext::InvokeRegionPreview), "INVOKE_REGION_PREVIEW", 0, "Invoke Region Preview", ""},
    {int(wmOpCallContext::InvokeArea), "INVOKE_AREA", 0, "Invoke Area", ""},
    {int(wmOpCallContext::InvokeScreen), "INVOKE_SCREEN", 0, "Invoke Screen", ""},
    {int(wmOpCallContext::ExecDefault), "EXEC_DEFAULT", 0, "Exec Default", ""},
    {int(wmOpCallContext::ExecRegionWin), "EXEC_REGION_WIN", 0, "Exec Region Window", ""},
    {int(wmOpCallContext::ExecRegionChannels), "EXEC_REGION_CHANNELS", 0, "Exec Region Channels", ""},
    {int(wmOpCallContext::ExecRegionPreview), "EXEC_REGION_PREVIEW", 0, "Exec Region Preview", ""},
    {int(wmOpCallContext::ExecArea), "EXEC_AREA", 0, "Exec Area", ""},
    {int(wmOpCallContext::ExecScreen), "EXEC_SCREEN", 0, "Exec Screen", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
/* clang-format on */

const EnumPropertyItem rna_enum_uilist_layout_type_items[] = {
    {UILST_LAYOUT_DEFAULT, "DEFAULT", 0, "Default Layout", "Use the default, multi-rows layout"},
    {UILST_LAYOUT_COMPACT, "COMPACT", 0, "Compact Layout", "Use the compact, single-row layout"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include "MEM_guardedalloc.h"

#  include "DNA_space_types.h"

#  include "RNA_access.hh"

#  include "BLI_dynstr.h"

#  include "BKE_context.hh"
#  include "BKE_main.hh"
#  include "BKE_report.hh"
#  include "BKE_screen.hh"

#  include "ED_asset_library.hh"
#  include "ED_asset_shelf.hh"

#  include "WM_api.hh"

static ARegionType *region_type_find(ReportList *reports, int space_type, int region_type)
{
  SpaceType *st;
  ARegionType *art;

  st = BKE_spacetype_from_id(space_type);

  for (art = (st) ? static_cast<ARegionType *>(st->regiontypes.first) : nullptr; art;
       art = art->next)
  {
    if (art->regionid == region_type) {
      break;
    }
  }

  /* region type not found? abort */
  if (art == nullptr) {
    BKE_report(reports, RPT_ERROR, "Region not found in space type");
    return nullptr;
  }

  return art;
}

/* Panel */

static bool panel_poll(const bContext *C, PanelType *pt)
{
  extern FunctionRNA rna_Panel_poll_func;

  ParameterList list;
  FunctionRNA *func;
  void *ret;
  bool visible;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, pt->rna_ext.srna, nullptr); /* dummy */
  func = &rna_Panel_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  pt->rna_ext.call((bContext *)C, &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "visible", &ret);
  visible = *(bool *)ret;

  RNA_parameter_list_free(&list);

  return visible;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  extern FunctionRNA rna_Panel_draw_func;

  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(
      &CTX_wm_screen(C)->id, panel->type->rna_ext.srna, panel);
  func = &rna_Panel_draw_func; /* RNA_struct_find_function(&ptr, "draw"); */

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  panel->type->rna_ext.call((bContext *)C, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void panel_draw_header(const bContext *C, Panel *panel)
{
  extern FunctionRNA rna_Panel_draw_header_func;

  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(
      &CTX_wm_screen(C)->id, panel->type->rna_ext.srna, panel);
  func = &rna_Panel_draw_header_func; /* RNA_struct_find_function(&ptr, "draw_header"); */

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  panel->type->rna_ext.call((bContext *)C, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void panel_draw_header_preset(const bContext *C, Panel *panel)
{
  extern FunctionRNA rna_Panel_draw_header_preset_func;

  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(
      &CTX_wm_screen(C)->id, panel->type->rna_ext.srna, panel);
  func = &rna_Panel_draw_header_preset_func;

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  panel->type->rna_ext.call((bContext *)C, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void panel_type_clear_recursive(Panel *panel, const PanelType *type)
{
  if (panel->type == type) {
    panel->type = nullptr;
  }

  LISTBASE_FOREACH (Panel *, child_panel, &panel->children) {
    panel_type_clear_recursive(child_panel, type);
  }
}

static bool rna_Panel_unregister(Main *bmain, StructRNA *type)
{
  ARegionType *art;
  PanelType *pt = static_cast<PanelType *>(RNA_struct_blender_type_get(type));

  if (!pt) {
    return false;
  }
  if (!(art = region_type_find(nullptr, pt->space_type, pt->region_type))) {
    return false;
  }

  RNA_struct_free_extension(type, &pt->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  if (pt->parent) {
    LinkData *link = static_cast<LinkData *>(
        BLI_findptr(&pt->parent->children, pt, offsetof(LinkData, data)));
    BLI_freelinkN(&pt->parent->children, link);
  }

  WM_paneltype_remove(pt);

  LISTBASE_FOREACH (LinkData *, link, &pt->children) {
    PanelType *child_pt = static_cast<PanelType *>(link->data);
    child_pt->parent = nullptr;
  }

  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase : &sl->regionbase;
        LISTBASE_FOREACH (ARegion *, region, regionbase) {
          LISTBASE_FOREACH (Panel *, panel, &region->panels) {
            panel_type_clear_recursive(panel, pt);
          }
          /* The unregistered panel might have had a template that added instanced panels,
           * so remove them just in case. They can be re-added on redraw anyway. */
          UI_panels_free_instanced(nullptr, region);
        }
      }
    }
  }

  BLI_freelistN(&pt->children);
  BLI_freelinkN(&art->paneltypes, pt);

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, nullptr);
  return true;
}

static StructRNA *rna_Panel_register(Main *bmain,
                                     ReportList *reports,
                                     void *data,
                                     const char *identifier,
                                     StructValidateFunc validate,
                                     StructCallbackFunc call,
                                     StructFreeFunc free)
{
  const char *error_prefix = RPT_("Registering panel class:");
  ARegionType *art;
  PanelType *pt, *parent = nullptr, dummy_pt = {nullptr};
  Panel dummy_panel = {nullptr};
  bool have_function[4];
  size_t over_alloc = 0; /* Warning, if this becomes a mess, we better do another allocation. */
  char _panel_descr[RNA_DYN_DESCR_MAX];
  size_t description_size = 0;

  /* setup dummy panel & panel type to store static properties in */
  dummy_panel.type = &dummy_pt;
  _panel_descr[0] = '\0';
  dummy_panel.type->description = _panel_descr;
  PointerRNA dummy_panel_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Panel, &dummy_panel);

  /* We have to set default context! Else we get a void string... */
  STRNCPY(dummy_pt.translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);

  /* validate the python class */
  if (validate(&dummy_panel_ptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(dummy_pt.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s '%s' is too long, maximum length is %d",
                error_prefix,
                identifier,
                int(sizeof(dummy_pt.idname)));
    return nullptr;
  }

  if ((1 << dummy_pt.region_type) & RGN_TYPE_HAS_CATEGORY_MASK) {
    if (dummy_pt.category[0] == '\0') {
      /* Use a fallback, otherwise an empty value will draw the panel in every category. */
      STRNCPY(dummy_pt.category, PNL_CATEGORY_FALLBACK);
#  ifndef NDEBUG
      printf("%s '%s' misses category, please update the script\n", error_prefix, dummy_pt.idname);
#  endif
    }
  }
  else {
    if (dummy_pt.category[0] != '\0') {
      if ((1 << dummy_pt.space_type) & WM_TOOLSYSTEM_SPACE_MASK) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "%s '%s' has category '%s'",
                    error_prefix,
                    dummy_pt.idname,
                    dummy_pt.category);
        return nullptr;
      }
    }
  }

  if (!(art = region_type_find(reports, dummy_pt.space_type, dummy_pt.region_type))) {
    return nullptr;
  }

  /* check if we have registered this panel type before, and remove it */
  for (pt = static_cast<PanelType *>(art->paneltypes.first); pt; pt = pt->next) {
    if (STREQ(pt->idname, dummy_pt.idname)) {
      PanelType *pt_next = pt->next;
      StructRNA *srna = pt->rna_ext.srna;
      if (srna) {
        BKE_reportf(reports,
                    RPT_INFO,
                    "%s '%s', bl_idname '%s' has been registered before, unregistering previous",
                    error_prefix,
                    identifier,
                    dummy_pt.idname);
        if (!rna_Panel_unregister(bmain, srna)) {
          BKE_reportf(reports,
                      RPT_ERROR,
                      "%s '%s', bl_idname '%s' could not be unregistered",
                      error_prefix,
                      identifier,
                      dummy_pt.idname);
        }
      }
      else {
        BLI_freelinkN(&art->paneltypes, pt);
      }

      /* The order of panel types will be altered on re-registration. */
      if (dummy_pt.parent_id[0] && (parent == nullptr)) {
        for (pt = pt_next; pt; pt = pt->next) {
          if (STREQ(pt->idname, dummy_pt.parent_id)) {
            parent = pt;
            break;
          }
        }
      }

      break;
    }

    if (dummy_pt.parent_id[0] && STREQ(pt->idname, dummy_pt.parent_id)) {
      parent = pt;
    }
  }

  if (!RNA_struct_available_or_report(reports, dummy_pt.idname)) {
    return nullptr;
  }
  if (!RNA_struct_bl_idname_ok_or_report(reports, dummy_pt.idname, "_PT_")) {
    return nullptr;
  }
  if (dummy_pt.parent_id[0] && !parent) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s parent '%s' for '%s' not found",
                error_prefix,
                dummy_pt.parent_id,
                dummy_pt.idname);
    return nullptr;
  }

  /* create a new panel type */
  if (_panel_descr[0]) {
    description_size = strlen(_panel_descr) + 1;
    over_alloc += description_size;
  }
  pt = static_cast<PanelType *>(
      MEM_callocN(sizeof(PanelType) + over_alloc, "Python buttons panel"));
  memcpy(pt, &dummy_pt, sizeof(dummy_pt));

  if (_panel_descr[0]) {
    char *buf = (char *)(pt + 1);
    memcpy(buf, _panel_descr, description_size);
    pt->description = buf;
  }
  else {
    pt->description = nullptr;
  }

  pt->rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, pt->idname, &RNA_Panel);
  RNA_def_struct_translation_context(pt->rna_ext.srna, pt->translation_context);
  pt->rna_ext.data = data;
  pt->rna_ext.call = call;
  pt->rna_ext.free = free;
  RNA_struct_blender_type_set(pt->rna_ext.srna, pt);
  RNA_def_struct_flag(pt->rna_ext.srna, STRUCT_NO_IDPROPERTIES);

  pt->poll = (have_function[0]) ? panel_poll : nullptr;
  pt->draw = (have_function[1]) ? panel_draw : nullptr;
  pt->draw_header = (have_function[2]) ? panel_draw_header : nullptr;
  pt->draw_header_preset = (have_function[3]) ? panel_draw_header_preset : nullptr;

  /* Find position to insert panel based on order. */
  PanelType *pt_iter = static_cast<PanelType *>(art->paneltypes.last);

  for (; pt_iter; pt_iter = pt_iter->prev) {
    /* No header has priority. */
    if ((pt->flag & PANEL_TYPE_NO_HEADER) && !(pt_iter->flag & PANEL_TYPE_NO_HEADER)) {
      continue;
    }
    if (pt_iter->order <= pt->order) {
      break;
    }
  }

  /* Insert into list. */
  BLI_insertlinkafter(&art->paneltypes, pt_iter, pt);

  if (parent) {
    pt->parent = parent;
    LinkData *pt_child_iter = static_cast<LinkData *>(parent->children.last);
    for (; pt_child_iter; pt_child_iter = pt_child_iter->prev) {
      PanelType *pt_child = static_cast<PanelType *>(pt_child_iter->data);
      if (pt_child->order <= pt->order) {
        break;
      }
    }
    BLI_insertlinkafter(&parent->children, pt_child_iter, BLI_genericNodeN(pt));
  }

  {
    const char *owner_id = RNA_struct_state_owner_get();
    if (owner_id) {
      STRNCPY(pt->owner_id, owner_id);
    }
  }

  WM_paneltype_add(pt);

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, nullptr);

  return pt->rna_ext.srna;
}

static StructRNA *rna_Panel_refine(PointerRNA *ptr)
{
  Panel *menu = (Panel *)ptr->data;
  return (menu->type && menu->type->rna_ext.srna) ? menu->type->rna_ext.srna : &RNA_Panel;
}

static StructRNA *rna_Panel_custom_data_typef(PointerRNA *ptr)
{
  Panel *panel = (Panel *)ptr->data;

  return UI_panel_custom_data_get(panel)->type;
}

static PointerRNA rna_Panel_custom_data_get(PointerRNA *ptr)
{
  Panel *panel = (Panel *)ptr->data;

  /* Because the panel custom data is general we can't refine the pointer type here. */
  return *UI_panel_custom_data_get(panel);
}

/* UIList */
static int rna_UIList_filter_const_FILTER_ITEM_get(PointerRNA * /*ptr*/)
{
  return UILST_FLT_ITEM;
}

static int rna_UIList_item_never_show(PointerRNA * /*ptr*/)
{
  return UILST_FLT_ITEM_NEVER_SHOW;
}

static IDProperty **rna_UIList_idprops(PointerRNA *ptr)
{
  uiList *ui_list = (uiList *)ptr->data;
  return &ui_list->properties;
}

static void rna_UIList_list_id_get(PointerRNA *ptr, char *value)
{
  uiList *ui_list = (uiList *)ptr->data;
  if (!ui_list->type) {
    value[0] = '\0';
    return;
  }

  strcpy(value, WM_uilisttype_list_id_get(ui_list->type, ui_list));
}

static int rna_UIList_list_id_length(PointerRNA *ptr)
{
  uiList *ui_list = (uiList *)ptr->data;
  if (!ui_list->type) {
    return 0;
  }

  return strlen(WM_uilisttype_list_id_get(ui_list->type, ui_list));
}

static void uilist_draw_item(uiList *ui_list,
                             const bContext *C,
                             uiLayout *layout,
                             PointerRNA *dataptr,
                             PointerRNA *itemptr,
                             int icon,
                             PointerRNA *active_dataptr,
                             const char *active_propname,
                             int index,
                             int flt_flag)
{
  extern FunctionRNA rna_UIList_draw_item_func;

  ParameterList list;
  FunctionRNA *func;

  PointerRNA ul_ptr = RNA_pointer_create_discrete(
      &CTX_wm_screen(C)->id, ui_list->type->rna_ext.srna, ui_list);
  func = &rna_UIList_draw_item_func; /* RNA_struct_find_function(&ul_ptr, "draw_item"); */

  RNA_parameter_list_create(&list, &ul_ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "layout", &layout);
  RNA_parameter_set_lookup(&list, "data", dataptr);
  RNA_parameter_set_lookup(&list, "item", itemptr);
  RNA_parameter_set_lookup(&list, "icon", &icon);
  RNA_parameter_set_lookup(&list, "active_data", active_dataptr);
  RNA_parameter_set_lookup(&list, "active_property", &active_propname);
  RNA_parameter_set_lookup(&list, "index", &index);
  RNA_parameter_set_lookup(&list, "flt_flag", &flt_flag);
  ui_list->type->rna_ext.call((bContext *)C, &ul_ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void uilist_draw_filter(uiList *ui_list, const bContext *C, uiLayout *layout)
{
  extern FunctionRNA rna_UIList_draw_filter_func;

  ParameterList list;
  FunctionRNA *func;

  PointerRNA ul_ptr = RNA_pointer_create_discrete(
      &CTX_wm_screen(C)->id, ui_list->type->rna_ext.srna, ui_list);
  func = &rna_UIList_draw_filter_func; /* RNA_struct_find_function(&ul_ptr, "draw_filter"); */

  RNA_parameter_list_create(&list, &ul_ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "layout", &layout);
  ui_list->type->rna_ext.call((bContext *)C, &ul_ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void uilist_filter_items(uiList *ui_list,
                                const bContext *C,
                                PointerRNA *dataptr,
                                const char *propname)
{
  extern FunctionRNA rna_UIList_filter_items_func;

  ParameterList list;
  FunctionRNA *func;
  PropertyRNA *parm;

  uiListDyn *flt_data = ui_list->dyn_data;
  int *filter_flags, *filter_neworder;
  void *ret1, *ret2;
  int ret_len;
  int len = flt_data->items_len = RNA_collection_length(dataptr, propname);

  PointerRNA ul_ptr = RNA_pointer_create_discrete(
      &CTX_wm_screen(C)->id, ui_list->type->rna_ext.srna, ui_list);
  func = &rna_UIList_filter_items_func; /* RNA_struct_find_function(&ul_ptr, "filter_items"); */

  RNA_parameter_list_create(&list, &ul_ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "data", dataptr);
  RNA_parameter_set_lookup(&list, "property", &propname);

  ui_list->type->rna_ext.call((bContext *)C, &ul_ptr, func, &list);

  parm = RNA_function_find_parameter(nullptr, func, "filter_flags");
  ret_len = RNA_parameter_dynamic_length_get(&list, parm);
  if (!ELEM(ret_len, len, 0)) {
    printf("%s: Error, py func returned %d items in %s, %d or none were expected.\n",
           __func__,
           RNA_parameter_dynamic_length_get(&list, parm),
           "filter_flags",
           len);
    /* NOTE: we cannot return here, we would let flt_data in inconsistent state... see #38356. */
    filter_flags = nullptr;
  }
  else {
    RNA_parameter_get(&list, parm, &ret1);
    filter_flags = (int *)ret1;
  }

  parm = RNA_function_find_parameter(nullptr, func, "filter_neworder");
  ret_len = RNA_parameter_dynamic_length_get(&list, parm);
  if (!ELEM(ret_len, len, 0)) {
    printf("%s: Error, py func returned %d items in %s, %d or none were expected.\n",
           __func__,
           RNA_parameter_dynamic_length_get(&list, parm),
           "filter_neworder",
           len);
    /* NOTE: we cannot return here, we would let flt_data in inconsistent state... see #38356. */
    filter_neworder = nullptr;
  }
  else {
    RNA_parameter_get(&list, parm, &ret2);
    filter_neworder = (int *)ret2;
  }

  /* We have to do some final checks and transforms... */
  {
    int i;
    if (filter_flags) {
      flt_data->items_filter_flags = MEM_malloc_arrayN<int>(size_t(len), __func__);
      memcpy(flt_data->items_filter_flags, filter_flags, sizeof(int) * len);

      if (filter_neworder) {
        /* For sake of simplicity, py filtering is expected to filter all items,
         * but we actually only want reordering data for shown items!
         */
        int items_shown, shown_idx;
        int t_idx, t_ni, prev_ni;
        flt_data->items_shown = 0;
        for (i = 0, shown_idx = 0; i < len; i++) {
          if (UI_list_item_index_is_filtered_visible(ui_list, i)) {
            filter_neworder[shown_idx++] = filter_neworder[i];
          }
        }
        items_shown = flt_data->items_shown = shown_idx;
        flt_data->items_filter_neworder = MEM_malloc_arrayN<int>(size_t(items_shown), __func__);
        /* And now, bring back new indices into the [0, items_shown[ range!
         * XXX This is O(N^2). :/
         */
        for (shown_idx = 0, prev_ni = -1; shown_idx < items_shown; shown_idx++) {
          for (i = 0, t_ni = len, t_idx = -1; i < items_shown; i++) {
            int ni = filter_neworder[i];
            if (ni > prev_ni && ni < t_ni) {
              t_idx = i;
              t_ni = ni;
            }
          }
          if (t_idx >= 0) {
            prev_ni = t_ni;
            flt_data->items_filter_neworder[t_idx] = shown_idx;
          }
        }
      }
      else {
        /* we still have to set flt_data->items_shown... */
        flt_data->items_shown = 0;
        for (i = 0; i < len; i++) {
          if (UI_list_item_index_is_filtered_visible(ui_list, i)) {
            flt_data->items_shown++;
          }
        }
      }
    }
    else {
      flt_data->items_shown = len;

      if (filter_neworder) {
        flt_data->items_filter_neworder = MEM_malloc_arrayN<int>(size_t(len), __func__);
        memcpy(flt_data->items_filter_neworder, filter_neworder, sizeof(int) * len);
      }
    }
  }

  RNA_parameter_list_free(&list);
}

static bool rna_UIList_unregister(Main *bmain, StructRNA *type)
{
  uiListType *ult = static_cast<uiListType *>(RNA_struct_blender_type_get(type));

  if (!ult) {
    return false;
  }

  RNA_struct_free_extension(type, &ult->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  WM_uilisttype_remove_ptr(bmain, ult);

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, nullptr);
  return true;
}

static StructRNA *rna_UIList_register(Main *bmain,
                                      ReportList *reports,
                                      void *data,
                                      const char *identifier,
                                      StructValidateFunc validate,
                                      StructCallbackFunc call,
                                      StructFreeFunc free)
{
  const char *error_prefix = "Registering uilist class:";
  uiListType *ult, dummy_ult = {nullptr};
  uiList dummy_uilist = {nullptr};
  bool have_function[3];

  /* setup dummy menu & menu type to store static properties in */
  dummy_uilist.type = &dummy_ult;
  PointerRNA dummy_ul_ptr = RNA_pointer_create_discrete(nullptr, &RNA_UIList, &dummy_uilist);

  /* validate the python class */
  if (validate(&dummy_ul_ptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(dummy_ult.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s '%s' is too long, maximum length is %d",
                error_prefix,
                identifier,
                int(sizeof(dummy_ult.idname)));
    return nullptr;
  }

  /* Check if we have registered this UI-list type before, and remove it. */
  ult = WM_uilisttype_find(dummy_ult.idname, true);
  if (ult) {
    BKE_reportf(reports,
                RPT_INFO,
                "%s '%s', bl_idname '%s' has been registered before, unregistering previous",
                error_prefix,
                identifier,
                dummy_ult.idname);

    StructRNA *srna = ult->rna_ext.srna;
    if (!(srna && rna_UIList_unregister(bmain, srna))) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "%s '%s', bl_idname '%s' %s",
                  error_prefix,
                  identifier,
                  dummy_ult.idname,
                  srna ? "is built-in" : "could not be unregistered");
      return nullptr;
    }
  }
  if (!RNA_struct_available_or_report(reports, dummy_ult.idname)) {
    return nullptr;
  }
  if (!RNA_struct_bl_idname_ok_or_report(reports, dummy_ult.idname, "_UL_")) {
    return nullptr;
  }

  /* create a new menu type */
  ult = MEM_callocN<uiListType>("python uilist");
  memcpy(ult, &dummy_ult, sizeof(dummy_ult));

  ult->rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, ult->idname, &RNA_UIList);
  ult->rna_ext.data = data;
  ult->rna_ext.call = call;
  ult->rna_ext.free = free;
  RNA_struct_blender_type_set(ult->rna_ext.srna, ult);

  ult->draw_item = (have_function[0]) ? uilist_draw_item : nullptr;
  ult->draw_filter = (have_function[1]) ? uilist_draw_filter : nullptr;
  ult->filter_items = (have_function[2]) ? uilist_filter_items : nullptr;

  WM_uilisttype_add(ult);

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, nullptr);

  return ult->rna_ext.srna;
}

static StructRNA *rna_UIList_refine(PointerRNA *ptr)
{
  uiList *ui_list = (uiList *)ptr->data;
  return (ui_list->type && ui_list->type->rna_ext.srna) ? ui_list->type->rna_ext.srna :
                                                          &RNA_UIList;
}

/* Header */

static void header_draw(const bContext *C, Header *hdr)
{
  extern FunctionRNA rna_Header_draw_func;

  ParameterList list;
  FunctionRNA *func;

  PointerRNA htr = RNA_pointer_create_discrete(
      &CTX_wm_screen(C)->id, hdr->type->rna_ext.srna, hdr);
  func = &rna_Header_draw_func; /* RNA_struct_find_function(&htr, "draw"); */

  RNA_parameter_list_create(&list, &htr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  hdr->type->rna_ext.call((bContext *)C, &htr, func, &list);

  RNA_parameter_list_free(&list);
}

static bool rna_Header_unregister(Main * /*bmain*/, StructRNA *type)
{
  ARegionType *art;
  HeaderType *ht = static_cast<HeaderType *>(RNA_struct_blender_type_get(type));

  if (!ht) {
    return false;
  }
  if (!(art = region_type_find(nullptr, ht->space_type, ht->region_type))) {
    return false;
  }

  RNA_struct_free_extension(type, &ht->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  BLI_freelinkN(&art->headertypes, ht);

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, nullptr);
  return true;
}

static StructRNA *rna_Header_register(Main *bmain,
                                      ReportList *reports,
                                      void *data,
                                      const char *identifier,
                                      StructValidateFunc validate,
                                      StructCallbackFunc call,
                                      StructFreeFunc free)
{
  const char *error_prefix = "Registering header class:";
  ARegionType *art;
  HeaderType *ht, dummy_ht = {nullptr};
  Header dummy_header = {nullptr};
  bool have_function[1];

  /* setup dummy header & header type to store static properties in */
  dummy_header.type = &dummy_ht;
  dummy_ht.region_type = RGN_TYPE_HEADER; /* RGN_TYPE_HEADER by default, may be overridden */
  PointerRNA dummy_header_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Header, &dummy_header);

  /* validate the python class */
  if (validate(&dummy_header_ptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(dummy_ht.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s '%s' is too long, maximum length is %d",
                error_prefix,
                identifier,
                int(sizeof(dummy_ht.idname)));
    return nullptr;
  }

  if (!(art = region_type_find(reports, dummy_ht.space_type, dummy_ht.region_type))) {
    return nullptr;
  }

  /* check if we have registered this header type before, and remove it */
  ht = static_cast<HeaderType *>(
      BLI_findstring(&art->headertypes, dummy_ht.idname, offsetof(HeaderType, idname)));
  if (ht) {
    BKE_reportf(reports,
                RPT_INFO,
                "%s '%s', bl_idname '%s' has been registered before, unregistering previous",
                error_prefix,
                identifier,
                dummy_ht.idname);

    StructRNA *srna = ht->rna_ext.srna;
    if (!(srna && rna_Header_unregister(bmain, srna))) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "%s '%s', bl_idname '%s' %s",
                  error_prefix,
                  identifier,
                  dummy_ht.idname,
                  srna ? "is built-in" : "could not be unregistered");
      return nullptr;
    }
  }

  if (!RNA_struct_available_or_report(reports, dummy_ht.idname)) {
    return nullptr;
  }
  if (!RNA_struct_bl_idname_ok_or_report(reports, dummy_ht.idname, "_HT_")) {
    return nullptr;
  }

  /* create a new header type */
  ht = MEM_callocN<HeaderType>(__func__);
  memcpy(ht, &dummy_ht, sizeof(dummy_ht));

  ht->rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, ht->idname, &RNA_Header);
  ht->rna_ext.data = data;
  ht->rna_ext.call = call;
  ht->rna_ext.free = free;
  RNA_struct_blender_type_set(ht->rna_ext.srna, ht);

  ht->draw = (have_function[0]) ? header_draw : nullptr;

  BLI_addtail(&art->headertypes, ht);

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, nullptr);

  return ht->rna_ext.srna;
}

static StructRNA *rna_Header_refine(PointerRNA *htr)
{
  Header *hdr = (Header *)htr->data;
  return (hdr->type && hdr->type->rna_ext.srna) ? hdr->type->rna_ext.srna : &RNA_Header;
}

/* Menu */

static bool menu_poll(const bContext *C, MenuType *pt)
{
  extern FunctionRNA rna_Menu_poll_func;

  ParameterList list;
  FunctionRNA *func;
  void *ret;
  bool visible;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, pt->rna_ext.srna, nullptr); /* dummy */
  func = &rna_Menu_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  pt->rna_ext.call((bContext *)C, &ptr, func, &list);

  RNA_parameter_get_lookup(&list, "visible", &ret);
  visible = *(bool *)ret;

  RNA_parameter_list_free(&list);

  return visible;
}

static void menu_draw(const bContext *C, Menu *menu)
{
  extern FunctionRNA rna_Menu_draw_func;

  ParameterList list;
  FunctionRNA *func;

  PointerRNA mtr = RNA_pointer_create_discrete(
      &CTX_wm_screen(C)->id, menu->type->rna_ext.srna, menu);
  func = &rna_Menu_draw_func; /* RNA_struct_find_function(&mtr, "draw"); */

  RNA_parameter_list_create(&list, &mtr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  menu->type->rna_ext.call((bContext *)C, &mtr, func, &list);

  RNA_parameter_list_free(&list);
}

static bool rna_Menu_unregister(Main * /*bmain*/, StructRNA *type)
{
  MenuType *mt = static_cast<MenuType *>(RNA_struct_blender_type_get(type));

  if (!mt) {
    return false;
  }

  RNA_struct_free_extension(type, &mt->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  WM_menutype_freelink(mt);

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, nullptr);
  return true;
}

static StructRNA *rna_Menu_register(Main *bmain,
                                    ReportList *reports,
                                    void *data,
                                    const char *identifier,
                                    StructValidateFunc validate,
                                    StructCallbackFunc call,
                                    StructFreeFunc free)
{
  const char *error_prefix = "Registering menu class:";
  MenuType *mt, dummy_mt = {nullptr};
  Menu dummy_menu = {nullptr};
  bool have_function[2];
  size_t over_alloc = 0; /* Warning, if this becomes a mess, we better do another allocation. */
  size_t description_size = 0;
  char _menu_descr[RNA_DYN_DESCR_MAX];

  /* setup dummy menu & menu type to store static properties in */
  dummy_menu.type = &dummy_mt;
  _menu_descr[0] = '\0';
  dummy_menu.type->description = _menu_descr;
  PointerRNA dummy_menu_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Menu, &dummy_menu);

  /* We have to set default context! Else we get a void string... */
  STRNCPY(dummy_mt.translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);

  /* validate the python class */
  if (validate(&dummy_menu_ptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(dummy_mt.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s '%s' is too long, maximum length is %d",
                error_prefix,
                identifier,
                int(sizeof(dummy_mt.idname)));
    return nullptr;
  }

  /* check if we have registered this menu type before, and remove it */
  mt = WM_menutype_find(dummy_mt.idname, true);
  if (mt) {
    BKE_reportf(reports,
                RPT_INFO,
                "%s '%s', bl_idname '%s' has been registered before, unregistering previous",
                error_prefix,
                identifier,
                dummy_mt.idname);

    StructRNA *srna = mt->rna_ext.srna;
    if (!(srna && rna_Menu_unregister(bmain, srna))) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "%s '%s', bl_idname '%s' %s",
                  error_prefix,
                  identifier,
                  dummy_mt.idname,
                  srna ? "is built-in" : "could not be unregistered");
      return nullptr;
    }
  }
  if (!RNA_struct_available_or_report(reports, dummy_mt.idname)) {
    return nullptr;
  }
  if (!RNA_struct_bl_idname_ok_or_report(reports, dummy_mt.idname, "_MT_")) {
    return nullptr;
  }

  /* create a new menu type */
  if (_menu_descr[0]) {
    description_size = strlen(_menu_descr) + 1;
    over_alloc += description_size;
  }

  mt = static_cast<MenuType *>(MEM_callocN(sizeof(MenuType) + over_alloc, "Python buttons menu"));
  memcpy(mt, &dummy_mt, sizeof(dummy_mt));

  if (_menu_descr[0]) {
    char *buf = (char *)(mt + 1);
    memcpy(buf, _menu_descr, description_size);
    mt->description = buf;
  }
  else {
    mt->description = nullptr;
  }

  mt->rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, mt->idname, &RNA_Menu);
  RNA_def_struct_translation_context(mt->rna_ext.srna, mt->translation_context);
  mt->rna_ext.data = data;
  mt->rna_ext.call = call;
  mt->rna_ext.free = free;
  RNA_struct_blender_type_set(mt->rna_ext.srna, mt);
  RNA_def_struct_flag(mt->rna_ext.srna, STRUCT_NO_IDPROPERTIES);

  mt->poll = (have_function[0]) ? menu_poll : nullptr;
  mt->draw = (have_function[1]) ? menu_draw : nullptr;

  {
    const char *owner_id = RNA_struct_state_owner_get();
    if (owner_id) {
      STRNCPY(mt->owner_id, owner_id);
    }
  }

  WM_menutype_add(mt);

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, nullptr);

  return mt->rna_ext.srna;
}

static StructRNA *rna_Menu_refine(PointerRNA *mtr)
{
  Menu *menu = (Menu *)mtr->data;
  return (menu->type && menu->type->rna_ext.srna) ? menu->type->rna_ext.srna : &RNA_Menu;
}

/* Asset Shelf */

static bool asset_shelf_asset_poll(const AssetShelfType *shelf_type,
                                   const AssetRepresentationHandle *asset)
{
  extern FunctionRNA rna_AssetShelf_asset_poll_func;

  PointerRNA ptr = RNA_pointer_create_discrete(
      nullptr, shelf_type->rna_ext.srna, nullptr); /* dummy */
  FunctionRNA *func = &rna_AssetShelf_asset_poll_func;

  ParameterList list;
  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "asset", &asset);
  shelf_type->rna_ext.call(nullptr, &ptr, func, &list);

  void *ret;
  RNA_parameter_get_lookup(&list, "visible", &ret);
  /* Get the value before freeing. */
  const bool is_visible = *(bool *)ret;

  RNA_parameter_list_free(&list);

  return is_visible;
}

static bool asset_shelf_poll(const bContext *C, const AssetShelfType *shelf_type)
{
  extern FunctionRNA rna_AssetShelf_poll_func;

  PointerRNA ptr = RNA_pointer_create_discrete(
      nullptr, shelf_type->rna_ext.srna, nullptr); /* dummy */
  FunctionRNA *func = &rna_AssetShelf_poll_func;   /* RNA_struct_find_function(&ptr, "poll"); */

  ParameterList list;
  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  shelf_type->rna_ext.call((bContext *)C, &ptr, func, &list);

  void *ret;
  RNA_parameter_get_lookup(&list, "visible", &ret);
  /* Get the value before freeing. */
  const bool is_visible = *(bool *)ret;

  RNA_parameter_list_free(&list);

  return is_visible;
}

static const AssetWeakReference *asset_shelf_get_active_asset(const AssetShelfType *shelf_type)
{
  extern FunctionRNA rna_AssetShelf_get_active_asset_func;

  PointerRNA ptr = RNA_pointer_create_discrete(
      nullptr, shelf_type->rna_ext.srna, nullptr); /* dummy */

  FunctionRNA *func = &rna_AssetShelf_get_active_asset_func;

  ParameterList list;
  RNA_parameter_list_create(&list, &ptr, func);
  shelf_type->rna_ext.call(nullptr, &ptr, func, &list);

  void *ret;
  RNA_parameter_get_lookup(&list, "asset_reference", &ret);
  /* Get the value before freeing. */
  AssetWeakReference *active_asset = *(AssetWeakReference **)ret;

  RNA_parameter_list_free(&list);

  return active_asset;
}

static void asset_shelf_draw_context_menu(const bContext *C,
                                          const AssetShelfType *shelf_type,
                                          const AssetRepresentationHandle *asset,
                                          uiLayout *layout)
{
  extern FunctionRNA rna_AssetShelf_draw_context_menu_func;

  PointerRNA ptr = RNA_pointer_create_discrete(
      nullptr, shelf_type->rna_ext.srna, nullptr); /* dummy */

  FunctionRNA *func = &rna_AssetShelf_draw_context_menu_func;
  // RNA_struct_find_function(&ptr, "draw_context_menu");

  ParameterList list;
  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  RNA_parameter_set_lookup(&list, "asset", &asset);
  RNA_parameter_set_lookup(&list, "layout", &layout);
  shelf_type->rna_ext.call((bContext *)C, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static bool rna_AssetShelf_unregister(Main *bmain, StructRNA *type)
{
  AssetShelfType *shelf_type = static_cast<AssetShelfType *>(RNA_struct_blender_type_get(type));

  if (!shelf_type) {
    return false;
  }

  blender::ed::asset::shelf::type_unlink(*bmain, *shelf_type);

  RNA_struct_free_extension(type, &shelf_type->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  blender::ed::asset::shelf::type_unregister(*shelf_type);

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, nullptr);

  return true;
}

static StructRNA *rna_AssetShelf_register(Main *bmain,
                                          ReportList *reports,
                                          void *data,
                                          const char *identifier,
                                          StructValidateFunc validate,
                                          StructCallbackFunc call,
                                          StructFreeFunc free)
{
  std::unique_ptr<AssetShelfType> shelf_type = std::make_unique<AssetShelfType>();

  /* setup dummy shelf & shelf type to store static properties in */
  AssetShelf dummy_shelf = {};
  dummy_shelf.type = shelf_type.get();
  PointerRNA dummy_shelf_ptr = RNA_pointer_create_discrete(nullptr, &RNA_AssetShelf, &dummy_shelf);

  bool have_function[4];

  /* validate the python class */
  if (validate(&dummy_shelf_ptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(shelf_type->idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering asset shelf class: '%s' is too long, maximum length is %d",
                identifier,
                int(sizeof(shelf_type->idname)));
    return nullptr;
  }

  /* Check if we have registered this asset shelf type before, and remove it. */
  {
    AssetShelfType *existing_shelf_type = blender::ed::asset::shelf::type_find_from_idname(
        shelf_type->idname);
    if (existing_shelf_type && existing_shelf_type->rna_ext.srna) {
      BKE_reportf(reports,
                  RPT_INFO,
                  "Registering asset shelf class: '%s' has been registered before, "
                  "unregistering previous",
                  shelf_type->idname);

      rna_AssetShelf_unregister(bmain, existing_shelf_type->rna_ext.srna);
    }
  }

  if (!RNA_struct_available_or_report(reports, shelf_type->idname)) {
    return nullptr;
  }
  if (!RNA_struct_bl_idname_ok_or_report(reports, shelf_type->idname, "_AST_")) {
    return nullptr;
  }

  /* Create the new shelf type. */
  shelf_type->rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, shelf_type->idname, &RNA_AssetShelf);
  shelf_type->rna_ext.data = data;
  shelf_type->rna_ext.call = call;
  shelf_type->rna_ext.free = free;
  RNA_struct_blender_type_set(shelf_type->rna_ext.srna, shelf_type.get());

  shelf_type->poll = have_function[0] ? asset_shelf_poll : nullptr;
  shelf_type->asset_poll = have_function[1] ? asset_shelf_asset_poll : nullptr;
  shelf_type->get_active_asset = have_function[2] ? asset_shelf_get_active_asset : nullptr;
  shelf_type->draw_context_menu = have_function[3] ? asset_shelf_draw_context_menu : nullptr;

  StructRNA *srna = shelf_type->rna_ext.srna;

  blender::ed::asset::shelf::type_register(std::move(shelf_type));

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, nullptr);

  return srna;
}

static void rna_AssetShelf_activate_operator_get(PointerRNA *ptr, char *value)
{
  AssetShelf *shelf = static_cast<AssetShelf *>(ptr->data);
  strcpy(value, shelf->type->activate_operator.c_str());
}

static int rna_AssetShelf_activate_operator_length(PointerRNA *ptr)
{
  AssetShelf *shelf = static_cast<AssetShelf *>(ptr->data);
  return shelf->type->activate_operator.size();
}

static void rna_AssetShelf_activate_operator_set(PointerRNA *ptr, const char *value)
{
  AssetShelf *shelf = static_cast<AssetShelf *>(ptr->data);
  shelf->type->activate_operator = value;
}

static void rna_AssetShelf_drag_operator_get(PointerRNA *ptr, char *value)
{
  AssetShelf *shelf = static_cast<AssetShelf *>(ptr->data);
  strcpy(value, shelf->type->drag_operator.c_str());
}

static int rna_AssetShelf_drag_operator_length(PointerRNA *ptr)
{
  AssetShelf *shelf = static_cast<AssetShelf *>(ptr->data);
  return shelf->type->drag_operator.size();
}

static void rna_AssetShelf_drag_operator_set(PointerRNA *ptr, const char *value)
{
  AssetShelf *shelf = static_cast<AssetShelf *>(ptr->data);
  shelf->type->drag_operator = value;
}

static StructRNA *rna_AssetShelf_refine(PointerRNA *shelf_ptr)
{
  AssetShelf *shelf = (AssetShelf *)shelf_ptr->data;
  return (shelf->type && shelf->type->rna_ext.srna) ? shelf->type->rna_ext.srna : &RNA_AssetShelf;
}

static int rna_AssetShelf_asset_library_get(PointerRNA *ptr)
{
  AssetShelf *shelf = static_cast<AssetShelf *>(ptr->data);
  return blender::ed::asset::library_reference_to_enum_value(
      &shelf->settings.asset_library_reference);
}

static void rna_AssetShelf_asset_library_set(PointerRNA *ptr, int value)
{
  AssetShelf *shelf = static_cast<AssetShelf *>(ptr->data);
  shelf->settings.asset_library_reference = blender::ed::asset::library_reference_from_enum_value(
      value);
}

static int rna_AssetShelf_preview_size_default(PointerRNA *ptr, PropertyRNA * /*prop*/)
{
  AssetShelf *shelf = static_cast<AssetShelf *>(ptr->data);
  if (shelf->type && shelf->type->default_preview_size) {
    return shelf->type->default_preview_size;
  }
  return ASSET_SHELF_PREVIEW_SIZE_DEFAULT;
}

static void rna_Panel_bl_description_set(PointerRNA *ptr, const char *value)
{
  Panel *data = (Panel *)(ptr->data);
  char *str = (char *)data->type->description;
  if (!str[0]) {
    BLI_strncpy_utf8(str, value, RNA_DYN_DESCR_MAX);
  }
  else {
    BLI_assert_msg(0, "setting the bl_description on a non-builtin panel");
  }
}

static void rna_Menu_bl_description_set(PointerRNA *ptr, const char *value)
{
  Menu *data = (Menu *)(ptr->data);
  char *str = (char *)data->type->description;
  if (!str[0]) {
    BLI_strncpy_utf8(str, value, RNA_DYN_DESCR_MAX);
  }
  else {
    BLI_assert_msg(0, "setting the bl_description on a non-builtin menu");
  }
}

/* UILayout */

static bool rna_UILayout_active_get(PointerRNA *ptr)
{
  return static_cast<uiLayout *>(ptr->data)->active();
}

static void rna_UILayout_active_set(PointerRNA *ptr, bool value)
{
  static_cast<uiLayout *>(ptr->data)->active_set(value);
}

static bool rna_UILayout_active_default_get(PointerRNA *ptr)
{
  return static_cast<uiLayout *>(ptr->data)->active_default();
}

static void rna_UILayout_active_default_set(PointerRNA *ptr, bool value)
{
  static_cast<uiLayout *>(ptr->data)->active_default_set(value);
}

static bool rna_UILayout_activate_init_get(PointerRNA *ptr)
{
  return static_cast<uiLayout *>(ptr->data)->activate_init();
}

static void rna_UILayout_activate_init_set(PointerRNA *ptr, bool value)
{
  static_cast<uiLayout *>(ptr->data)->activate_init_set(value);
}

static bool rna_UILayout_alert_get(PointerRNA *ptr)
{
  return static_cast<uiLayout *>(ptr->data)->red_alert();
}

static void rna_UILayout_alert_set(PointerRNA *ptr, bool value)
{
  static_cast<uiLayout *>(ptr->data)->red_alert_set(value);
}

static void rna_UILayout_op_context_set(PointerRNA *ptr, int value)
{
  static_cast<uiLayout *>(ptr->data)->operator_context_set(blender::wm::OpCallContext(value));
}

static int rna_UILayout_op_context_get(PointerRNA *ptr)
{
  return int(static_cast<uiLayout *>(ptr->data)->operator_context());
}

static bool rna_UILayout_enabled_get(PointerRNA *ptr)
{
  return static_cast<uiLayout *>(ptr->data)->enabled();
}

static void rna_UILayout_enabled_set(PointerRNA *ptr, bool value)
{
  static_cast<uiLayout *>(ptr->data)->enabled_set(value);
}

#  if 0
static int rna_UILayout_red_alert_get(PointerRNA *ptr)
{
  return static_cast<uiLayout *>(ptr->data)->red_alert();
}

static void rna_UILayout_red_alert_set(PointerRNA *ptr, bool value)
{
  static_cast<uiLayout *>(ptr->data)->red_alert_set(value);
}
#  endif

static int rna_UILayout_alignment_get(PointerRNA *ptr)
{
  return int(static_cast<uiLayout *>(ptr->data)->alignment());
}

static void rna_UILayout_alignment_set(PointerRNA *ptr, int value)
{
  static_cast<uiLayout *>(ptr->data)->alignment_set(blender::ui::LayoutAlign(value));
}

static int rna_UILayout_direction_get(PointerRNA *ptr)
{
  return int(ptr->data_as<uiLayout>()->local_direction());
}

static float rna_UILayout_scale_x_get(PointerRNA *ptr)
{
  return static_cast<uiLayout *>(ptr->data)->scale_x();
}

static void rna_UILayout_scale_x_set(PointerRNA *ptr, float value)
{
  static_cast<uiLayout *>(ptr->data)->scale_x_set(value);
}

static float rna_UILayout_scale_y_get(PointerRNA *ptr)
{
  return static_cast<uiLayout *>(ptr->data)->scale_y();
}

static void rna_UILayout_scale_y_set(PointerRNA *ptr, float value)
{
  static_cast<uiLayout *>(ptr->data)->scale_y_set(value);
}

static float rna_UILayout_units_x_get(PointerRNA *ptr)
{
  return static_cast<uiLayout *>(ptr->data)->ui_units_x();
}

static void rna_UILayout_units_x_set(PointerRNA *ptr, float value)
{
  static_cast<uiLayout *>(ptr->data)->ui_units_x_set(value);
}

static float rna_UILayout_units_y_get(PointerRNA *ptr)
{
  return static_cast<uiLayout *>(ptr->data)->ui_units_y();
}

static void rna_UILayout_units_y_set(PointerRNA *ptr, float value)
{
  static_cast<uiLayout *>(ptr->data)->ui_units_y_set(value);
}

static int rna_UILayout_emboss_get(PointerRNA *ptr)
{
  return int(static_cast<uiLayout *>(ptr->data)->emboss());
}

static void rna_UILayout_emboss_set(PointerRNA *ptr, int value)
{
  static_cast<uiLayout *>(ptr->data)->emboss_set(blender::ui::EmbossType(value));
}

static bool rna_UILayout_property_split_get(PointerRNA *ptr)
{
  return static_cast<const uiLayout *>(ptr->data)->use_property_split();
}

static void rna_UILayout_property_split_set(PointerRNA *ptr, bool value)
{
  static_cast<uiLayout *>(ptr->data)->use_property_split_set(value);
}

static bool rna_UILayout_property_decorate_get(PointerRNA *ptr)
{
  return static_cast<const uiLayout *>(ptr->data)->use_property_decorate();
}

static void rna_UILayout_property_decorate_set(PointerRNA *ptr, bool value)
{
  static_cast<uiLayout *>(ptr->data)->use_property_decorate_set(value);
}

/* File Handler */

static bool file_handler_poll_drop(const bContext *C,
                                   blender::bke::FileHandlerType *file_handler_type)
{
  extern FunctionRNA rna_FileHandler_poll_drop_func;

  PointerRNA ptr = RNA_pointer_create_discrete(
      nullptr, file_handler_type->rna_ext.srna, nullptr); /* dummy */
  FunctionRNA *func = &rna_FileHandler_poll_drop_func;

  ParameterList list;
  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);
  file_handler_type->rna_ext.call((bContext *)C, &ptr, func, &list);

  void *ret;
  RNA_parameter_get_lookup(&list, "is_usable", &ret);
  /* Get the value before freeing. */
  const bool is_usable = *(bool *)ret;

  RNA_parameter_list_free(&list);

  return is_usable;
}

static bool rna_FileHandler_unregister(Main * /*bmain*/, StructRNA *type)
{
  using namespace blender;
  bke::FileHandlerType *file_handler_type = static_cast<bke::FileHandlerType *>(
      RNA_struct_blender_type_get(type));

  if (!file_handler_type) {
    return false;
  }

  RNA_struct_free_extension(type, &file_handler_type->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  bke::file_handler_remove(file_handler_type);

  return true;
}

static StructRNA *rna_FileHandler_register(Main *bmain,
                                           ReportList *reports,
                                           void *data,
                                           const char *identifier,
                                           StructValidateFunc validate,
                                           StructCallbackFunc call,
                                           StructFreeFunc free)
{
  using namespace blender;
  bke::FileHandlerType dummy_file_handler_type{};
  FileHandler dummy_file_handler{};

  dummy_file_handler.type = &dummy_file_handler_type;

  /* Setup dummy file handler type to store static properties in. */
  PointerRNA dummy_file_handler_ptr = RNA_pointer_create_discrete(
      nullptr, &RNA_FileHandler, &dummy_file_handler);

  bool have_function[1];

  /* Validate the python class. */
  if (validate(&dummy_file_handler_ptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(dummy_file_handler_type.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering file handler class: '%s' is too long, maximum length is %d",
                identifier,
                (int)sizeof(dummy_file_handler_type.idname));
    return nullptr;
  }

  /* Check if there is a file handler registered with the same `idname`, and remove it. */
  auto registered_file_handler = bke::file_handler_find(dummy_file_handler_type.idname);
  if (registered_file_handler) {
    rna_FileHandler_unregister(bmain, registered_file_handler->rna_ext.srna);
  }

  if (!RNA_struct_available_or_report(reports, dummy_file_handler_type.idname)) {
    return nullptr;
  }
  if (!RNA_struct_bl_idname_ok_or_report(reports, dummy_file_handler_type.idname, "_FH_")) {
    return nullptr;
  }

  /* Create the new file handler type. */
  auto file_handler_type = std::make_unique<bke::FileHandlerType>();
  *file_handler_type = dummy_file_handler_type;

  file_handler_type->rna_ext.srna = RNA_def_struct_ptr(
      &BLENDER_RNA, file_handler_type->idname, &RNA_FileHandler);
  file_handler_type->rna_ext.data = data;
  file_handler_type->rna_ext.call = call;
  file_handler_type->rna_ext.free = free;
  RNA_struct_blender_type_set(file_handler_type->rna_ext.srna, file_handler_type.get());

  file_handler_type->poll_drop = have_function[0] ? file_handler_poll_drop : nullptr;

  auto srna = file_handler_type->rna_ext.srna;
  bke::file_handler_add(std::move(file_handler_type));

  return srna;
}

static StructRNA *rna_FileHandler_refine(PointerRNA *file_handler_ptr)
{
  FileHandler *file_handler = (FileHandler *)file_handler_ptr->data;
  return (file_handler->type && file_handler->type->rna_ext.srna) ?
             file_handler->type->rna_ext.srna :
             &RNA_FileHandler;
}

#else /* RNA_RUNTIME */

static void rna_def_ui_layout(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem alignment_items[] = {
      {int(blender::ui::LayoutAlign::Expand), "EXPAND", 0, "Expand", ""},
      {int(blender::ui::LayoutAlign::Left), "LEFT", 0, "Left", ""},
      {int(blender::ui::LayoutAlign::Center), "CENTER", 0, "Center", ""},
      {int(blender::ui::LayoutAlign::Right), "RIGHT", 0, "Right", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem direction_items[] = {
      {int(blender::ui::LayoutDirection::Horizontal), "HORIZONTAL", 0, "Horizontal", ""},
      {int(blender::ui::LayoutDirection::Vertical), "VERTICAL", 0, "Vertical", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem emboss_items[] = {
      {int(blender::ui::EmbossType::Emboss),
       "NORMAL",
       0,
       "Regular",
       "Draw standard button emboss style"},
      {int(blender::ui::EmbossType::None), "NONE", 0, "None", "Draw only text and icons"},
      {int(blender::ui::EmbossType::Pulldown),
       "PULLDOWN_MENU",
       0,
       "Pull-down Menu",
       "Draw pull-down menu style"},
      {int(blender::ui::EmbossType::PieMenu), "PIE_MENU", 0, "Pie Menu", "Draw radial menu style"},
      {int(blender::ui::EmbossType::NoneOrStatus),
       "NONE_OR_STATUS",
       0,
       "None or Status",
       "Draw with no emboss unless the button has a coloring status like an animation state"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* layout */

  srna = RNA_def_struct(brna, "UILayout", nullptr);
  RNA_def_struct_sdna(srna, "uiLayout");
  RNA_def_struct_ui_text(srna, "UI Layout", "User interface layout in a panel or header");

  prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_UILayout_active_get", "rna_UILayout_active_set");

  prop = RNA_def_property(srna, "active_default", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_UILayout_active_default_get", "rna_UILayout_active_default_set");
  RNA_def_property_ui_text(
      prop,
      "Active Default",
      "When true, an operator button defined after this will be activated when pressing return"
      "(use with popup dialogs)");

  prop = RNA_def_property(srna, "activate_init", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_UILayout_activate_init_get", "rna_UILayout_activate_init_set");
  RNA_def_property_ui_text(
      prop,
      "Activate on Init",
      "When true, buttons defined in popups will be activated on first display "
      "(use so you can type into a field without having to click on it first)");

  prop = RNA_def_property(srna, "operator_context", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_operator_context_items);
  RNA_def_property_enum_funcs(
      prop, "rna_UILayout_op_context_get", "rna_UILayout_op_context_set", nullptr);
  RNA_def_property_ui_text(prop,
                           "Operator Context",
                           "Typically set to 'INVOKE_REGION_WIN', except some cases "
                           "in :class:`bpy.types.Menu` when it's set to 'EXEC_REGION_WIN'.");

  prop = RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_UILayout_enabled_get", "rna_UILayout_enabled_set");
  RNA_def_property_ui_text(prop, "Enabled", "When false, this (sub)layout is grayed out");

  prop = RNA_def_property(srna, "alert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_UILayout_alert_get", "rna_UILayout_alert_set");

  prop = RNA_def_property(srna, "alignment", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, alignment_items);
  RNA_def_property_enum_funcs(
      prop, "rna_UILayout_alignment_get", "rna_UILayout_alignment_set", nullptr);

  prop = RNA_def_property(srna, "direction", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, direction_items);
  RNA_def_property_enum_funcs(prop, "rna_UILayout_direction_get", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "scale_x", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_funcs(
      prop, "rna_UILayout_scale_x_get", "rna_UILayout_scale_x_set", nullptr);
  RNA_def_property_ui_text(
      prop, "Scale X", "Scale factor along the X for items in this (sub)layout");

  prop = RNA_def_property(srna, "scale_y", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_funcs(
      prop, "rna_UILayout_scale_y_get", "rna_UILayout_scale_y_set", nullptr);
  RNA_def_property_ui_text(
      prop, "Scale Y", "Scale factor along the Y for items in this (sub)layout");

  prop = RNA_def_property(srna, "ui_units_x", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_funcs(
      prop, "rna_UILayout_units_x_get", "rna_UILayout_units_x_set", nullptr);
  RNA_def_property_ui_text(
      prop, "Units X", "Fixed size along the X for items in this (sub)layout");

  prop = RNA_def_property(srna, "ui_units_y", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_funcs(
      prop, "rna_UILayout_units_y_get", "rna_UILayout_units_y_set", nullptr);
  RNA_def_property_ui_text(
      prop, "Units Y", "Fixed size along the Y for items in this (sub)layout");
  RNA_api_ui_layout(srna);

  prop = RNA_def_property(srna, "emboss", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, emboss_items);
  RNA_def_property_enum_funcs(prop, "rna_UILayout_emboss_get", "rna_UILayout_emboss_set", nullptr);

  prop = RNA_def_property(srna, "use_property_split", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_UILayout_property_split_get", "rna_UILayout_property_split_set");

  prop = RNA_def_property(srna, "use_property_decorate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_UILayout_property_decorate_get", "rna_UILayout_property_decorate_set");
}

static void rna_def_panel(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  PropertyRNA *parm;
  FunctionRNA *func;

  static const EnumPropertyItem panel_flag_items[] = {
      {PANEL_TYPE_DEFAULT_CLOSED,
       "DEFAULT_CLOSED",
       0,
       "Default Closed",
       "Defines if the panel has to be open or collapsed at the time of its creation"},
      {PANEL_TYPE_NO_HEADER,
       "HIDE_HEADER",
       0,
       "Hide Header",
       "If set to False, the panel shows a header, which contains a clickable "
       "arrow to collapse the panel and the label (see bl_label)"},
      {PANEL_TYPE_INSTANCED,
       "INSTANCED",
       0,
       "Instanced Panel",
       "Multiple panels with this type can be used as part of a list depending on data external "
       "to the UI. Used to create panels for the modifiers and other stacks."},
      {PANEL_TYPE_HEADER_EXPAND,
       "HEADER_LAYOUT_EXPAND",
       0,
       "Expand Header Layout",
       "Allow buttons in the header to stretch and shrink to fill the entire layout width"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "Panel", nullptr);
  RNA_def_struct_ui_text(srna, "Panel", "Panel containing UI elements");
  RNA_def_struct_sdna(srna, "Panel");
  RNA_def_struct_refine_func(srna, "rna_Panel_refine");
  RNA_def_struct_register_funcs(srna, "rna_Panel_register", "rna_Panel_unregister", nullptr);
  RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  RNA_def_struct_flag(srna, STRUCT_PUBLIC_NAMESPACE_INHERIT);

  /* poll */
  func = RNA_def_function(srna, "poll", nullptr);
  RNA_def_function_ui_description(
      func, "If this method returns a non-null output, then the panel can be drawn");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* draw */
  func = RNA_def_function(srna, "draw", nullptr);
  RNA_def_function_ui_description(func, "Draw UI elements into the panel UI layout");
  RNA_def_function_flag(func, FUNC_REGISTER);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "draw_header", nullptr);
  RNA_def_function_ui_description(func, "Draw UI elements into the panel's header UI layout");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "draw_header_preset", nullptr);
  RNA_def_function_ui_description(func, "Draw UI elements for presets in the panel's header");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  prop = RNA_def_property(srna, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "UILayout");
  RNA_def_property_ui_text(prop, "Layout", "Defines the structure of the panel in the UI");

  prop = RNA_def_property(srna, "text", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "drawname");
  RNA_def_property_ui_text(prop, "Text", "XXX todo");

  prop = RNA_def_property(srna, "custom_data", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Constraint");
  RNA_def_property_pointer_sdna(prop, nullptr, "runtime.custom_data_ptr");
  RNA_def_property_pointer_funcs(
      prop, "rna_Panel_custom_data_get", nullptr, "rna_Panel_custom_data_typef", nullptr);
  RNA_def_property_ui_text(prop, "Custom Data", "Panel data");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* registration */
  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop,
                           "ID Name",
                           "If this is set, the panel gets a custom ID, otherwise it takes the "
                           "name of the class used to define the panel. For example, if the "
                           "class name is \"OBJECT_PT_hello\", and bl_idname is not set by the "
                           "script, then bl_idname = \"OBJECT_PT_hello\".");

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->label");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop,
                           "Label",
                           "The panel label, shows up in the panel header at the right of the "
                           "triangle used to collapse the panel");

  prop = RNA_def_property(srna, "bl_translation_context", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->translation_context");
  RNA_def_property_string_default(prop, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop,
                           "",
                           "Specific translation context, only define when the label needs to be "
                           "disambiguated from others using the exact same label");

  RNA_define_verify_sdna(true);

  prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->description");
  RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_Panel_bl_description_set");
  // RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_clear_flag(prop, PROP_NEVER_NULL); /* check for nullptr */
  RNA_def_property_ui_text(prop, "", "The panel tooltip");

  prop = RNA_def_property(srna, "bl_category", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->category");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop, "", "The category (tab) in which the panel will be displayed, when applicable");

  prop = RNA_def_property(srna, "bl_owner_id", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->owner_id");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop, "", "The ID owning the data displayed in the panel, if any");

  prop = RNA_def_property(srna, "bl_space_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type->space_type");
  RNA_def_property_enum_items(prop, rna_enum_space_type_items);
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "Space Type", "The space where the panel is going to be used in");

  prop = RNA_def_property(srna, "bl_region_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type->region_type");
  RNA_def_property_enum_items(prop, rna_enum_region_type_items);
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(
      prop, "Region Type", "The region where the panel is going to be used in");

  prop = RNA_def_property(srna, "bl_context", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->context");
  RNA_def_property_flag(
      prop, PROP_REGISTER_OPTIONAL); /* Only used in Properties Editor and 3D View - Thomas */
  RNA_def_property_ui_text(prop,
                           "Context",
                           "The context in which the panel belongs to. (TODO: explain the "
                           "possible combinations bl_context/bl_region_type/bl_space_type)");

  prop = RNA_def_property(srna, "bl_options", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL | PROP_ENUM_FLAG);
  RNA_def_property_enum_sdna(prop, nullptr, "type->flag");
  RNA_def_property_enum_items(prop, panel_flag_items);
  RNA_def_property_ui_text(prop, "Options", "Options for this panel type");

  prop = RNA_def_property(srna, "bl_parent_id", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->parent_id");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop, "Parent ID Name", "If this is set, the panel becomes a sub-panel");

  prop = RNA_def_property(srna, "bl_ui_units_x", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "type->ui_units_x");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop, "Units X", "When set, defines popup panel width");

  prop = RNA_def_property(srna, "bl_order", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "type->order");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop,
      "Order",
      "Panels with lower numbers are default ordered before panels with higher numbers");

  prop = RNA_def_property(srna, "use_pin", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PNL_PIN);
  RNA_def_property_ui_text(prop, "Pin", "Show the panel on all tabs");
  /* XXX, should only tag region for redraw */
  RNA_def_property_update(prop, NC_WINDOW, nullptr);

  prop = RNA_def_property(srna, "is_popover", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PNL_POPOVER);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Popover", "");
}

static void rna_def_uilist(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  PropertyRNA *parm;
  FunctionRNA *func;

  srna = RNA_def_struct(brna, "UIList", nullptr);
  RNA_def_struct_ui_text(srna, "UIList", "UI list containing the elements of a collection");
  RNA_def_struct_sdna(srna, "uiList");
  RNA_def_struct_refine_func(srna, "rna_UIList_refine");
  RNA_def_struct_register_funcs(srna, "rna_UIList_register", "rna_UIList_unregister", nullptr);
  RNA_def_struct_system_idprops_func(srna, "rna_UIList_idprops");
  RNA_def_struct_flag(srna, STRUCT_NO_DATABLOCK_IDPROPERTIES | STRUCT_PUBLIC_NAMESPACE_INHERIT);

  /* Registration */
  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop,
                           "ID Name",
                           "If this is set, the uilist gets a custom ID, otherwise it takes the "
                           "name of the class used to define the uilist (for example, if the "
                           "class name is \"OBJECT_UL_vgroups\", and bl_idname is not set by the "
                           "script, then bl_idname = \"OBJECT_UL_vgroups\")");

  /* Data */
  /* Note that this is the "non-full" list-ID as obtained through #WM_uilisttype_list_id_get(),
   * which differs from the (internal) `uiList.list_id`. */
  prop = RNA_def_property(srna, "list_id", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_UIList_list_id_get", "rna_UIList_list_id_length", nullptr);
  RNA_def_property_ui_text(prop,
                           "List Name",
                           "Identifier of the list, if any was passed to the \"list_id\" "
                           "parameter of \"template_list()\"");

  prop = RNA_def_property(srna, "layout_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_uilist_layout_type_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* Filter options */
  prop = RNA_def_property(srna, "use_filter_show", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter_flag", UILST_FLT_SHOW);
  RNA_def_property_ui_text(prop, "Show Filter", "Show filtering options");

  prop = RNA_def_property(srna, "filter_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "filter_byname");
  RNA_def_property_flag(prop, PROP_TEXTEDIT_UPDATE);
  RNA_def_property_ui_text(
      prop, "Filter by Name", "Only show items matching this name (use '*' as wildcard)");

  prop = RNA_def_property(srna, "use_filter_invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter_flag", UILST_FLT_EXCLUDE);
  RNA_def_property_ui_text(prop, "Invert", "Invert filtering (show hidden items, and vice versa)");

  /* WARNING: This is sort of an abuse, sort-by-alpha is actually a value,
   * should even be an enum in full logic (of two values, sort by index and sort by name).
   * But for default UIList, it's nicer (better UI-wise) to show this as a boolean bit-flag option,
   * avoids having to define custom setters/getters using UILST_FLT_SORT_MASK to mask out
   * actual bitflags on same var, etc.
   */
  prop = RNA_def_property(srna, "use_filter_sort_alpha", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter_sort_flag", UILST_FLT_SORT_ALPHA);
  RNA_def_property_ui_icon(prop, ICON_SORTALPHA, 0);
  RNA_def_property_ui_text(prop, "Sort by Name", "Sort items by their name");

  prop = RNA_def_property(srna, "use_filter_sort_reverse", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter_sort_flag", UILST_FLT_SORT_REVERSE);
  RNA_def_property_ui_text(prop, "Reverse", "Reverse the order of shown items");

  prop = RNA_def_property(srna, "use_filter_sort_lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter_sort_flag", UILST_FLT_SORT_LOCK);
  RNA_def_property_ui_text(
      prop, "Lock Order", "Lock the order of shown items (user cannot change it)");

  /* draw_item */
  func = RNA_def_function(srna, "draw_item", nullptr);
  RNA_def_function_ui_description(
      func,
      "Draw an item in the list (NOTE: when you define your own draw_item "
      "function, you may want to check given 'item' is of the right type...)");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "layout", "UILayout", "", "Layout to draw the item");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "data", "AnyType", "", "Data from which to take Collection property");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_pointer(func, "item", "AnyType", "", "Item of the collection property");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_int(
      func, "icon", 0, 0, INT_MAX, "", "Icon of the item in the collection", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func,
                         "active_data",
                         "AnyType",
                         "",
                         "Data from which to take property for the active element");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_string(func,
                        "active_property",
                        nullptr,
                        0,
                        "",
                        "Identifier of property in active_data, for the active element");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_PYFUNC_REGISTER_OPTIONAL);
  parm = RNA_def_int(
      func, "index", 0, 0, INT_MAX, "", "Index of the item in the collection", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_property(func, "flt_flag", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(parm, "", "The filter-flag result for this item");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* draw_filter */
  func = RNA_def_function(srna, "draw_filter", nullptr);
  RNA_def_function_ui_description(func, "Draw filtering options");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "layout", "UILayout", "", "Layout to draw the item");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* filter */
  func = RNA_def_function(srna, "filter_items", nullptr);
  RNA_def_function_ui_description(
      func,
      "Filter and/or re-order items of the collection (output filter results in "
      "filter_flags, and reorder results in filter_neworder arrays)");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "data", "AnyType", "", "Data from which to take Collection property");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_string(
      func, "property", nullptr, 0, "", "Identifier of property in data, for the collection");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  prop = RNA_def_property(func, "filter_flags", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_array(prop, 1); /* XXX Dummy value, default 0 does not work */
  RNA_def_property_ui_text(prop,
                           "",
                           "An array of filter flags, one for each item in the collection (NOTE: "
                           "The upper 16 bits, including FILTER_ITEM, are reserved, only use the "
                           "lower 16 bits for custom usages)");
  RNA_def_function_output(func, prop);
  prop = RNA_def_property(func, "filter_neworder", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_array(prop, 1); /* XXX Dummy value, default 0 does not work */
  RNA_def_property_ui_text(
      prop,
      "",
      "An array of indices, one for each item in the collection, mapping the org "
      "index to the new one");
  RNA_def_function_output(func, prop);

  /* "Constants"! */
  RNA_define_verify_sdna(false); /* not in sdna */

  prop = RNA_def_property(srna, "bitflag_filter_item", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(
      prop,
      "FILTER_ITEM",
      "The value of the reserved bitflag 'FILTER_ITEM' (in filter_flags values)");
  RNA_def_property_int_funcs(prop, "rna_UIList_filter_const_FILTER_ITEM_get", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "bitflag_item_never_show", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "ITEM_NEVER_SHOW", "Skip the item from displaying in the list");
  RNA_def_property_int_funcs(prop, "rna_UIList_item_never_show", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_header(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  PropertyRNA *parm;
  FunctionRNA *func;

  srna = RNA_def_struct(brna, "Header", nullptr);
  RNA_def_struct_ui_text(srna, "Header", "Editor header containing UI elements");
  RNA_def_struct_sdna(srna, "Header");
  RNA_def_struct_refine_func(srna, "rna_Header_refine");
  RNA_def_struct_register_funcs(srna, "rna_Header_register", "rna_Header_unregister", nullptr);
  RNA_def_struct_flag(srna, STRUCT_PUBLIC_NAMESPACE_INHERIT);

  /* draw */
  func = RNA_def_function(srna, "draw", nullptr);
  RNA_def_function_ui_description(func, "Draw UI elements into the header UI layout");
  RNA_def_function_flag(func, FUNC_REGISTER);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  RNA_define_verify_sdna(false); /* not in sdna */

  prop = RNA_def_property(srna, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "layout");
  RNA_def_property_struct_type(prop, "UILayout");
  RNA_def_property_ui_text(prop, "Layout", "Structure of the header in the UI");

  /* registration */
  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop,
                           "ID Name",
                           "If this is set, the header gets a custom ID, otherwise it takes the "
                           "name of the class used to define the header; for example, if the "
                           "class name is \"OBJECT_HT_hello\", and bl_idname is not set by the "
                           "script, then bl_idname = \"OBJECT_HT_hello\"");

  prop = RNA_def_property(srna, "bl_space_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type->space_type");
  RNA_def_property_enum_items(prop, rna_enum_space_type_items);
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(
      prop, "Space Type", "The space where the header is going to be used in");

  prop = RNA_def_property(srna, "bl_region_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type->region_type");
  RNA_def_property_enum_default(prop, RGN_TYPE_HEADER);
  RNA_def_property_enum_items(prop, rna_enum_region_type_items);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop,
                           "Region Type",
                           "The region where the header is going to be used in "
                           "(defaults to header region)");

  RNA_define_verify_sdna(true);
}

static void rna_def_menu(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  PropertyRNA *parm;
  FunctionRNA *func;

  static const EnumPropertyItem menu_flag_items[] = {
      {int(MenuTypeFlag::SearchOnKeyPress),
       "SEARCH_ON_KEY_PRESS",
       0,
       "Search on Key Press",
       "Open a menu search when a key pressed while the menu is open"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "Menu", nullptr);
  RNA_def_struct_ui_text(srna, "Menu", "Editor menu containing buttons");
  RNA_def_struct_sdna(srna, "Menu");
  RNA_def_struct_refine_func(srna, "rna_Menu_refine");
  RNA_def_struct_register_funcs(srna, "rna_Menu_register", "rna_Menu_unregister", nullptr);
  RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  RNA_def_struct_flag(srna, STRUCT_PUBLIC_NAMESPACE_INHERIT);

  /* poll */
  func = RNA_def_function(srna, "poll", nullptr);
  RNA_def_function_ui_description(
      func, "If this method returns a non-null output, then the menu can be drawn");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* draw */
  func = RNA_def_function(srna, "draw", nullptr);
  RNA_def_function_ui_description(func, "Draw UI elements into the menu UI layout");
  RNA_def_function_flag(func, FUNC_REGISTER);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  RNA_define_verify_sdna(false); /* not in sdna */

  prop = RNA_def_property(srna, "layout", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "layout");
  RNA_def_property_struct_type(prop, "UILayout");
  RNA_def_property_ui_text(prop, "Layout", "Defines the structure of the menu in the UI");

  /* registration */
  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop,
                           "ID Name",
                           "If this is set, the menu gets a custom ID, otherwise it takes the "
                           "name of the class used to define the menu (for example, if the "
                           "class name is \"OBJECT_MT_hello\", and bl_idname is not set by the "
                           "script, then bl_idname = \"OBJECT_MT_hello\")");

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->label");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "Label", "The menu label");

  prop = RNA_def_property(srna, "bl_translation_context", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->translation_context");
  RNA_def_property_string_default(prop, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

  prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->description");
  RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_Menu_bl_description_set");
  // RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_clear_flag(prop, PROP_NEVER_NULL); /* check for nullptr */

  prop = RNA_def_property(srna, "bl_owner_id", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->owner_id");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

  prop = RNA_def_property(srna, "bl_options", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL | PROP_ENUM_FLAG);
  RNA_def_property_enum_sdna(prop, nullptr, "type->flag");
  RNA_def_property_enum_items(prop, menu_flag_items);
  RNA_def_property_ui_text(prop, "Options", "Options for this menu type");

  RNA_define_verify_sdna(true);
}

static void rna_def_asset_shelf(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem asset_shelf_flag_items[] = {
      {ASSET_SHELF_TYPE_FLAG_NO_ASSET_DRAG,
       "NO_ASSET_DRAG",
       0,
       "No Asset Dragging",
       "Disable the default asset dragging on drag events. Useful for implementing custom "
       "dragging via custom key-map items."},
      {ASSET_SHELF_TYPE_FLAG_DEFAULT_VISIBLE,
       "DEFAULT_VISIBLE",
       0,
       "Visible by Default",
       "Unhide the asset shelf when it's available for the first time, otherwise it will be "
       "hidden"},
      {ASSET_SHELF_TYPE_FLAG_STORE_CATALOGS_IN_PREFS,
       "STORE_ENABLED_CATALOGS_IN_PREFERENCES",
       0,
       "Store Enabled Catalogs in Preferences",
       "Store the shelf's enabled catalogs in the preferences rather than the local asset shelf "
       "settings"},
      {ASSET_SHELF_TYPE_FLAG_ACTIVATE_FOR_CONTEXT_MENU,
       "ACTIVATE_FOR_CONTEXT_MENU",
       0,
       "",
       "When spawning a context menu for an asset, activate the asset and call "
       "`bl_activate_operator` if present, rather than just highlighting the asset"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "AssetShelf", nullptr);
  RNA_def_struct_ui_text(srna, "Asset Shelf", "Regions for quick access to assets");
  RNA_def_struct_refine_func(srna, "rna_AssetShelf_refine");
  RNA_def_struct_register_funcs(
      srna, "rna_AssetShelf_register", "rna_AssetShelf_unregister", nullptr);
  RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  RNA_def_struct_flag(srna, STRUCT_PUBLIC_NAMESPACE_INHERIT);

  /* registration */

  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop,
                           "ID Name",
                           "If this is set, the asset gets a custom ID, otherwise it takes the "
                           "name of the class used to define the asset (for example, if the "
                           "class name is \"OBJECT_AST_hello\", and bl_idname is not set by the "
                           "script, then bl_idname = \"OBJECT_AST_hello\")");

  prop = RNA_def_property(srna, "bl_space_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type->space_type");
  RNA_def_property_enum_items(prop, rna_enum_space_type_items);
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop,
                           "Space Type",
                           "The space where the asset shelf will show up in. Ignored for popup "
                           "asset shelves which can be displayed in any space.");

  prop = RNA_def_property(srna, "bl_options", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL | PROP_ENUM_FLAG);
  RNA_def_property_enum_sdna(prop, nullptr, "type->flag");
  RNA_def_property_enum_items(prop, asset_shelf_flag_items);
  RNA_def_property_ui_text(prop, "Options", "Options for this asset shelf type");

  prop = RNA_def_property(srna, "bl_activate_operator", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_AssetShelf_activate_operator_get",
                                "rna_AssetShelf_activate_operator_length",
                                "rna_AssetShelf_activate_operator_set");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop,
      "Activate Operator",
      "Operator to call when activating an item with asset reference properties");

  prop = RNA_def_property(srna, "bl_drag_operator", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_AssetShelf_drag_operator_get",
                                "rna_AssetShelf_drag_operator_length",
                                "rna_AssetShelf_drag_operator_set");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop,
      "Drag Operator",
      "Operator to call when dragging an item with asset reference properties");

  prop = RNA_def_property(srna, "bl_default_preview_size", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "type->default_preview_size");
  RNA_def_property_range(prop, 32, 256);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop, "Default Preview Size", "Default size of the asset preview thumbnails in pixels");

  PropertyRNA *parm;
  FunctionRNA *func;

  func = RNA_def_function(srna, "poll", nullptr);
  RNA_def_function_ui_description(
      func, "If this method returns a non-null output, the asset shelf will be visible");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "asset_poll", nullptr);
  RNA_def_function_ui_description(
      func,
      "Determine if an asset should be visible in the asset shelf. If this method returns a "
      "non-null output, the asset will be visible.");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));
  parm = RNA_def_pointer(func, "asset", "AssetRepresentation", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "get_active_asset", nullptr);
  RNA_def_function_ui_description(
      func,
      "Return a reference to the asset that should be highlighted as active in the asset shelf");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  /* return type */
  parm = RNA_def_pointer(func,
                         "asset_reference",
                         "AssetWeakReference",
                         "",
                         "The weak reference to the asset to be highlighted as active, or None");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "draw_context_menu", nullptr);
  RNA_def_function_ui_description(
      func, "Draw UI elements into the context menu UI layout displayed on right click");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "asset", "AssetRepresentation", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "layout", "UILayout", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  prop = rna_def_asset_library_reference_common(
      srna, "rna_AssetShelf_asset_library_get", "rna_AssetShelf_asset_library_set");
  RNA_def_property_ui_text(
      prop, "Asset Library", "Choose the asset library to display assets from");
  RNA_def_property_update(prop, NC_SPACE | ND_REGIONS_ASSET_SHELF, nullptr);

  prop = RNA_def_property(srna, "show_names", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "settings.display_flag", ASSETSHELF_SHOW_NAMES);
  RNA_def_property_ui_text(prop,
                           "Show Names",
                           "Show the asset name together with the preview. Otherwise only the "
                           "preview will be visible.");
  RNA_def_property_update(prop, NC_SPACE | ND_REGIONS_ASSET_SHELF, nullptr);

  prop = RNA_def_property(srna, "preview_size", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "settings.preview_size");
  RNA_def_property_range(prop, 32, 256);
  RNA_def_property_int_default_func(prop, "rna_AssetShelf_preview_size_default");
  RNA_def_property_ui_text(prop, "Preview Size", "Size of the asset preview thumbnails in pixels");
  RNA_def_property_update(prop, NC_SPACE | ND_REGIONS_ASSET_SHELF, nullptr);

  prop = RNA_def_property(srna, "search_filter", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "settings.search_string");
  RNA_def_property_ui_text(prop, "Display Filter", "Filter assets by name");
  RNA_def_property_flag(prop, PROP_TEXTEDIT_UPDATE);
  RNA_def_property_update(prop, NC_SPACE | ND_REGIONS_ASSET_SHELF, nullptr);
}

static void rna_def_file_handler(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "FileHandler", nullptr);
  RNA_def_struct_ui_text(srna,
                         "File Handler Type",
                         "Extends functionality to operators that manages files, such as adding "
                         "drag and drop support");
  RNA_def_struct_refine_func(srna, "rna_FileHandler_refine");
  RNA_def_struct_register_funcs(
      srna, "rna_FileHandler_register", "rna_FileHandler_unregister", nullptr);

  RNA_def_struct_translation_context(srna, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  RNA_def_struct_flag(srna, STRUCT_PUBLIC_NAMESPACE_INHERIT);

  /* registration */

  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(
      prop,
      "ID Name",
      "If this is set, the file handler gets a custom ID, otherwise it takes the "
      "name of the class used to define the file handler (for example, if the "
      "class name is \"OBJECT_FH_hello\", and bl_idname is not set by the "
      "script, then bl_idname = \"OBJECT_FH_hello\")");

  prop = RNA_def_property(srna, "bl_import_operator", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->import_operator");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop,
      "Operator",
      "Operator that can handle import for files with the extensions given in bl_file_extensions");
  prop = RNA_def_property(srna, "bl_export_operator", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->export_operator");
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(
      prop,
      "Operator",
      "Operator that can handle export for files with the extensions given in bl_file_extensions");

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->label");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "Label", "The file handler label");

  prop = RNA_def_property(srna, "bl_file_extensions", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "type->file_extensions_str");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(
      prop,
      "File Extensions",
      "Formatted string of file extensions supported by the file handler, each extension should "
      "start with a \".\" and be separated by \";\"."
      "\nFor Example: ``\".blend;.ble\"``");

  PropertyRNA *parm;
  FunctionRNA *func;

  func = RNA_def_function(srna, "poll_drop", nullptr);
  RNA_def_function_ui_description(
      func,
      "If this method returns True, can be used to handle the drop of a drag-and-drop action");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
  RNA_def_function_return(func, RNA_def_boolean(func, "is_usable", false, "", ""));
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void rna_def_layout_panel_state(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "LayoutPanelState", nullptr);

  prop = RNA_def_property(srna, "is_open", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LAYOUT_PANEL_STATE_FLAG_OPEN);
  RNA_def_property_ui_text(prop, "Is Open", "");
}

void RNA_def_ui(BlenderRNA *brna)
{
  rna_def_ui_layout(brna);
  rna_def_panel(brna);
  rna_def_uilist(brna);
  rna_def_header(brna);
  rna_def_menu(brna);
  rna_def_asset_shelf(brna);
  rna_def_file_handler(brna);
  rna_def_layout_panel_state(brna);
}

#endif /* RNA_RUNTIME */
