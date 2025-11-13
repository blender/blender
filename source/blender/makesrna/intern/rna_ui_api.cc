/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "DNA_screen_types.h"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "rna_internal.hh"

#define DEF_ICON(name) {ICON_##name, (#name), 0, (#name), ""},
#define DEF_ICON_VECTOR(name) {ICON_##name, (#name), 0, (#name), ""},
#define DEF_ICON_COLOR(name) {ICON_##name, (#name), 0, (#name), ""},
#define DEF_ICON_BLANK(name)
const EnumPropertyItem rna_enum_icon_items[] = {
#include "UI_icons.hh"
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include "BLT_translation.hh"

#  include "DNA_asset_types.h"

#  include "BKE_report.hh"

#  include "ED_asset_filter.hh"
#  include "ED_geometry.hh"
#  include "ED_node.hh"
#  include "ED_object.hh"

#  include "WM_api.hh"

using blender::StringRefNull;

std::optional<StringRefNull> rna_translate_ui_text(
    const char *text, const char *text_ctxt, StructRNA *type, PropertyRNA *prop, bool translate)
{
  if (!text) {
    return std::nullopt;
  }
  /* Also return text if UI labels translation is disabled. */
  if (!text[0] || !translate || !BLT_translate_iface()) {
    return text;
  }

  /* If a text_ctxt is specified, use it! */
  if (text_ctxt && text_ctxt[0]) {
    return BLT_pgettext(text_ctxt, text);
  }

  /* Else, if an RNA type or property is specified, use its context. */
#  if 0
  /* XXX Disabled for now. Unfortunately, their is absolutely no way from py code to get the RNA
   *     struct corresponding to the 'data' (in functions like prop() & co),
   *     as this is pure runtime data. Hence, messages extraction script can't determine the
   *     correct context it should use for such 'text' messages...
   *     So for now, one have to explicitly specify the 'text_ctxt' when using prop() etc.
   *     functions, if default context is not suitable.
   */
  if (prop) {
    return BLT_pgettext(RNA_property_translation_context(prop), text);
  }
#  else
  (void)prop;
#  endif
  if (type) {
    return BLT_pgettext(RNA_struct_translation_context(type), text);
  }

  /* Else, default context! */
  return BLT_pgettext(BLT_I18NCONTEXT_DEFAULT, text);
}

static void rna_uiItemR(uiLayout *layout,
                        PointerRNA *ptr,
                        const char *propname,
                        const char *name,
                        const char *text_ctxt,
                        bool translate,
                        int icon,
                        const char *placeholder,
                        bool expand,
                        bool slider,
                        int toggle,
                        bool icon_only,
                        bool event,
                        bool full_event,
                        bool emboss,
                        int index,
                        int icon_value,
                        bool invert_checkbox)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
  eUI_Item_Flag flag = UI_ITEM_NONE;

  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  if (icon_value && !icon) {
    icon = icon_value;
  }

  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, prop, translate);
  std::optional<StringRefNull> placeholder_str = rna_translate_ui_text(
      placeholder, text_ctxt, nullptr, prop, translate);

  if (slider) {
    flag |= UI_ITEM_R_SLIDER;
  }
  if (expand) {
    flag |= UI_ITEM_R_EXPAND;
  }

  if (toggle == 1) {
    flag |= UI_ITEM_R_TOGGLE;
  }
  else if (toggle == 0) {
    flag |= UI_ITEM_R_ICON_NEVER;
  }

  if (icon_only) {
    flag |= UI_ITEM_R_ICON_ONLY;
  }
  if (event) {
    flag |= UI_ITEM_R_EVENT;
  }
  if (full_event) {
    flag |= UI_ITEM_R_FULL_EVENT;
  }
  if (emboss == false) {
    flag |= UI_ITEM_R_NO_BG;
  }
  if (invert_checkbox) {
    flag |= UI_ITEM_R_CHECKBOX_INVERT;
  }

  layout->prop(ptr, prop, index, 0, flag, text, icon, placeholder_str);
}

static void rna_uiItemR_with_popover(uiLayout *layout,
                                     PointerRNA *ptr,
                                     const char *propname,
                                     const char *name,
                                     const char *text_ctxt,
                                     bool translate,
                                     int icon,
                                     bool icon_only,
                                     const char *panel_type)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }
  if ((RNA_property_type(prop) != PROP_ENUM) &&
      !ELEM(RNA_property_subtype(prop), PROP_COLOR, PROP_COLOR_GAMMA))
  {
    RNA_warning(
        "property is not an enum or color: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }
  eUI_Item_Flag flag = UI_ITEM_NONE;
  if (icon_only) {
    flag |= UI_ITEM_R_ICON_ONLY;
  }

  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, prop, translate);
  layout->prop_with_popover(ptr, prop, -1, 0, flag, text, icon, panel_type);
}

static void rna_uiItemR_with_menu(uiLayout *layout,
                                  PointerRNA *ptr,
                                  const char *propname,
                                  const char *name,
                                  const char *text_ctxt,
                                  bool translate,
                                  int icon,
                                  bool icon_only,
                                  const char *menu_type)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }
  if (RNA_property_type(prop) != PROP_ENUM) {
    RNA_warning("property is not an enum: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }
  eUI_Item_Flag flag = UI_ITEM_NONE;
  if (icon_only) {
    flag |= UI_ITEM_R_ICON_ONLY;
  }

  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, prop, translate);
  layout->prop_with_menu(ptr, prop, -1, 0, flag, text, icon, menu_type);
}

static void rna_uiItemMenuEnumR(uiLayout *layout,
                                PointerRNA *ptr,
                                const char *propname,
                                const char *name,
                                const char *text_ctxt,
                                bool translate,
                                int icon)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, prop, translate);
  layout->prop_menu_enum(ptr, prop, text, icon);
}

static void rna_uiItemTabsEnumR(uiLayout *layout,
                                bContext *C,
                                PointerRNA *ptr,
                                const char *propname,
                                PointerRNA *ptr_highlight,
                                const char *propname_highlight,
                                bool icon_only)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }
  if (RNA_property_type(prop) != PROP_ENUM) {
    RNA_warning("property is not an enum: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  /* Get the highlight property used to gray out some of the tabs. */
  PropertyRNA *prop_highlight = nullptr;
  if (!RNA_pointer_is_null(ptr_highlight)) {
    prop_highlight = RNA_struct_find_property(ptr_highlight, propname_highlight);
    if (!prop_highlight) {
      RNA_warning("property not found: %s.%s",
                  RNA_struct_identifier(ptr_highlight->type),
                  propname_highlight);
      return;
    }
    if (RNA_property_type(prop_highlight) != PROP_BOOLEAN) {
      RNA_warning("property is not a boolean: %s.%s",
                  RNA_struct_identifier(ptr_highlight->type),
                  propname_highlight);
      return;
    }
    if (!RNA_property_array_check(prop_highlight)) {
      RNA_warning("property is not an array: %s.%s",
                  RNA_struct_identifier(ptr_highlight->type),
                  propname_highlight);
      return;
    }
  }

  layout->prop_tabs_enum(C, ptr, prop, ptr_highlight, prop_highlight, icon_only);
}

static void rna_uiItemEnumR_string(uiLayout *layout,
                                   PointerRNA *ptr,
                                   const char *propname,
                                   const char *value,
                                   const char *name,
                                   const char *text_ctxt,
                                   bool translate,
                                   int icon)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, prop, translate);

  layout->prop_enum(ptr, prop, value, text, icon);
}

static void rna_uiItemsEnumR(uiLayout *layout, PointerRNA *ptr, const char *propname)
{
  layout->props_enum(ptr, propname);
}

static void rna_uiItemPointerR(uiLayout *layout,
                               PointerRNA *ptr,
                               const char *propname,
                               PointerRNA *searchptr,
                               const char *searchpropname,
                               const char *name,
                               const char *text_ctxt,
                               bool translate,
                               int icon,
                               const bool results_are_suggestions,
                               const char *item_searchpropname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }
  PropertyRNA *searchprop = RNA_struct_find_property(searchptr, searchpropname);
  if (!searchprop) {
    RNA_warning(
        "property not found: %s.%s", RNA_struct_identifier(searchptr->type), searchpropname);
    return;
  }

  PropertyRNA *item_searchprop = nullptr;
  if (item_searchpropname && item_searchpropname[0]) {
    StructRNA *collection_item_type = RNA_property_pointer_type(searchptr, searchprop);
    item_searchprop = RNA_struct_type_find_property(collection_item_type, item_searchpropname);
    if (!item_searchprop) {
      RNA_warning("Collection items search property not found: %s.%s",
                  RNA_struct_identifier(collection_item_type),
                  item_searchpropname);
    }
  }

  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, prop, translate);

  layout->prop_search(
      ptr, prop, searchptr, searchprop, item_searchprop, text, icon, results_are_suggestions);
}

void rna_uiLayoutDecorator(uiLayout *layout, PointerRNA *ptr, const char *propname, int index)
{
  layout->decorator(ptr, propname, index);
}

static PointerRNA rna_uiItemO(uiLayout *layout,
                              const char *opname,
                              const char *name,
                              const char *text_ctxt,
                              bool translate,
                              int icon,
                              bool emboss,
                              bool depress,
                              int icon_value,
                              const float search_weight)
{
  wmOperatorType *ot;

  ot = WM_operatortype_find(opname, false); /* print error next */
  if (!ot || !ot->srna) {
    RNA_warning("%s '%s'", ot ? "operator missing srna" : "unknown operator", opname);
    return PointerRNA_NULL;
  }

  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, ot->srna, nullptr, translate);

  if (icon_value && !icon) {
    icon = icon_value;
  }
  eUI_Item_Flag flag = UI_ITEM_NONE;
  if (emboss == false) {
    flag |= UI_ITEM_R_NO_BG;
  }
  if (depress) {
    flag |= UI_ITEM_O_DEPRESS;
  }

  const float prev_weight = layout->search_weight();
  layout->search_weight_set(search_weight);

  PointerRNA opptr = layout->op(ot, text, icon, layout->operator_context(), flag);

  layout->search_weight_set(prev_weight);
  return opptr;
}

static PointerRNA rna_uiItemOMenuHold(uiLayout *layout,
                                      const char *opname,
                                      const char *name,
                                      const char *text_ctxt,
                                      bool translate,
                                      int icon,
                                      bool emboss,
                                      bool depress,
                                      int icon_value,
                                      const char *menu)
{
  wmOperatorType *ot = WM_operatortype_find(opname, false); /* print error next */
  if (!ot || !ot->srna) {
    RNA_warning("%s '%s'", ot ? "operator missing srna" : "unknown operator", opname);
    return PointerRNA_NULL;
  }

  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, ot->srna, nullptr, translate);
  if (icon_value && !icon) {
    icon = icon_value;
  }
  eUI_Item_Flag flag = UI_ITEM_NONE;
  if (emboss == false) {
    flag |= UI_ITEM_R_NO_BG;
  }
  if (depress) {
    flag |= UI_ITEM_O_DEPRESS;
  }

  return layout->op_menu_hold(ot, text, icon, layout->operator_context(), flag, menu);
}

static void rna_uiItemsEnumO(uiLayout *layout,
                             const char *opname,
                             const char *propname,
                             const bool icon_only)
{
  eUI_Item_Flag flag = icon_only ? UI_ITEM_R_ICON_ONLY : UI_ITEM_NONE;
  layout->op_enum(opname, propname, nullptr, layout->operator_context(), flag);
}

static PointerRNA rna_uiItemMenuEnumO(uiLayout *layout,
                                      bContext *C,
                                      const char *opname,
                                      const char *propname,
                                      const char *name,
                                      const char *text_ctxt,
                                      bool translate,
                                      int icon)
{
  wmOperatorType *ot = WM_operatortype_find(opname, false); /* print error next */

  if (!ot || !ot->srna) {
    RNA_warning("%s '%s'", ot ? "operator missing srna" : "unknown operator", opname);
    return PointerRNA_NULL;
  }

  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, ot->srna, nullptr, translate);

  return layout->op_menu_enum(C, ot, propname, text, icon);
}

static void rna_uiItemL(uiLayout *layout,
                        const char *name,
                        const char *text_ctxt,
                        bool translate,
                        int icon,
                        int icon_value)
{
  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, nullptr, translate);

  if (icon_value && !icon) {
    icon = icon_value;
  }

  layout->label(text.value_or(""), icon);
}

static void rna_uiItemM(uiLayout *layout,
                        const char *menuname,
                        const char *name,
                        const char *text_ctxt,
                        bool translate,
                        int icon,
                        int icon_value)
{
  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, nullptr, translate);

  if (icon_value && !icon) {
    icon = icon_value;
  }

  layout->menu(menuname, text, icon);
}

static void rna_uiItemM_contents(uiLayout *layout, const char *menuname)
{
  layout->menu_contents(menuname);
}

static void rna_uiItemPopoverPanel(uiLayout *layout,
                                   bContext *C,
                                   const char *panel_type,
                                   const char *name,
                                   const char *text_ctxt,
                                   bool translate,
                                   int icon,
                                   int icon_value)
{
  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, nullptr, translate);

  if (icon_value && !icon) {
    icon = icon_value;
  }

  layout->popover(C, panel_type, text, icon);
}

static void rna_uiItemPopoverPanelFromGroup(uiLayout *layout,
                                            bContext *C,
                                            int space_id,
                                            int region_id,
                                            const char *context,
                                            const char *category)
{
  layout->popover_group(C, space_id, region_id, context, category);
}

static void rna_uiItemProgress(uiLayout *layout,
                               const char *text,
                               const char *text_ctxt,
                               bool translate,
                               float factor,
                               int progress_type)
{
  if (translate && BLT_translate_iface()) {
    text = BLT_pgettext((text_ctxt && text_ctxt[0]) ? text_ctxt : BLT_I18NCONTEXT_DEFAULT, text);
  }

  layout->progress_indicator(text, factor, blender::ui::ButProgressType(progress_type));
}

static void rna_uiItemSeparator(uiLayout *layout, float factor, int type)
{
  layout->separator(factor, LayoutSeparatorType(type));
}

static void rna_uiLayoutContextPointerSet(uiLayout *layout, const char *name, PointerRNA *ptr)
{
  layout->context_ptr_set(name, ptr);
}

static void rna_uiLayoutContextStringSet(uiLayout *layout, const char *name, const char *value)
{
  layout->context_string_set(name, value);
}

static void rna_uiLayoutSeparatorSpacer(uiLayout *layout)
{
  layout->separator_spacer();
}

static void rna_uiTemplateID(uiLayout *layout,
                             bContext *C,
                             PointerRNA *ptr,
                             const char *propname,
                             const char *newop,
                             const char *openop,
                             const char *unlinkop,
                             int filter,
                             const bool live_icon,
                             const char *name,
                             const char *text_ctxt,
                             bool translate)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, prop, translate);

  uiTemplateID(layout, C, ptr, propname, newop, openop, unlinkop, filter, live_icon, text);
}

static void rna_uiTemplateAnyID(uiLayout *layout,
                                PointerRNA *ptr,
                                const char *propname,
                                const char *proptypename,
                                const char *name,
                                const char *text_ctxt,
                                bool translate)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, prop, translate);

  /* XXX This will search property again :( */
  uiTemplateAnyID(layout, ptr, propname, proptypename, text);
}

static void rna_uiTemplateAction(uiLayout *layout,
                                 bContext *C,
                                 ID *id,
                                 const char *newop,
                                 const char *unlinkop,
                                 const char *name,
                                 const char *text_ctxt,
                                 const bool translate)
{
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, nullptr, translate);
  uiTemplateAction(layout, C, id, newop, unlinkop, text);
}

static void rna_uiTemplateSearch(uiLayout *layout,
                                 const bContext *C,
                                 PointerRNA *ptr,
                                 const char *propname,
                                 PointerRNA *searchptr,
                                 const char *searchpropname,
                                 const char *newop,
                                 const char *unlinkop,
                                 const char *name,
                                 const char *text_ctxt,
                                 bool translate)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, prop, translate);

  uiTemplateSearch(layout, C, ptr, propname, searchptr, searchpropname, newop, unlinkop, text);
}

static void rna_uiTemplateSearchPreview(uiLayout *layout,
                                        bContext *C,
                                        PointerRNA *ptr,
                                        const char *propname,
                                        PointerRNA *searchptr,
                                        const char *searchpropname,
                                        const char *newop,
                                        const char *unlinkop,
                                        const char *name,
                                        const char *text_ctxt,
                                        bool translate,
                                        const int rows,
                                        const int cols)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, prop, translate);

  uiTemplateSearchPreview(
      layout, C, ptr, propname, searchptr, searchpropname, newop, unlinkop, rows, cols, text);
}

void rna_uiTemplateList(uiLayout *layout,
                        bContext *C,
                        const char *listtype_name,
                        const char *list_id,
                        PointerRNA *dataptr,
                        const char *propname,
                        PointerRNA *active_dataptr,
                        const char *active_propname,
                        const char *item_dyntip_propname,
                        const int rows,
                        const int maxrows,
                        const int layout_type,
                        const int columns,
                        const bool sort_reverse,
                        const bool sort_lock)
{
  uiTemplateListFlags flags = UI_TEMPLATE_LIST_FLAG_NONE;
  if (sort_reverse) {
    flags |= UI_TEMPLATE_LIST_SORT_REVERSE;
  }
  if (sort_lock) {
    flags |= UI_TEMPLATE_LIST_SORT_LOCK;
  }

  uiTemplateList(layout,
                 C,
                 listtype_name,
                 list_id,
                 dataptr,
                 propname,
                 active_dataptr,
                 active_propname,
                 item_dyntip_propname,
                 rows,
                 maxrows,
                 layout_type,
                 columns,
                 flags);
}

static void rna_uiTemplateCacheFile(uiLayout *layout,
                                    bContext *C,
                                    PointerRNA *ptr,
                                    const char *propname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  uiTemplateCacheFile(layout, C, ptr, propname);
}

static void rna_uiTemplateCacheFileVelocity(uiLayout *layout,
                                            PointerRNA *ptr,
                                            const char *propname)
{
  PointerRNA fileptr;
  if (!uiTemplateCacheFilePointer(ptr, propname, &fileptr)) {
    return;
  }

  uiTemplateCacheFileVelocity(layout, &fileptr);
}

static void rna_uiTemplateCacheFileTimeSettings(uiLayout *layout,
                                                PointerRNA *ptr,
                                                const char *propname)
{
  PointerRNA fileptr;
  if (!uiTemplateCacheFilePointer(ptr, propname, &fileptr)) {
    return;
  }

  uiTemplateCacheFileTimeSettings(layout, &fileptr);
}

static void rna_uiTemplateCacheFileLayers(uiLayout *layout,
                                          bContext *C,
                                          PointerRNA *ptr,
                                          const char *propname)
{
  PointerRNA fileptr;
  if (!uiTemplateCacheFilePointer(ptr, propname, &fileptr)) {
    return;
  }

  uiTemplateCacheFileLayers(layout, C, &fileptr);
}

static void rna_uiTemplatePathBuilder(uiLayout *layout,
                                      PointerRNA *ptr,
                                      const char *propname,
                                      PointerRNA *root_ptr,
                                      const char *name,
                                      const char *text_ctxt,
                                      bool translate)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  if (!prop) {
    RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return;
  }

  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, prop, translate);

  /* XXX This will search property again :( */
  uiTemplatePathBuilder(layout, ptr, propname, root_ptr, text);
}

static void rna_uiTemplateEventFromKeymapItem(
    uiLayout *layout, wmKeyMapItem *kmi, const char *name, const char *text_ctxt, bool translate)
{
  /* Get translated name (label). */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      name, text_ctxt, nullptr, nullptr, translate);
  uiTemplateEventFromKeymapItem(layout, text.value_or(""), kmi, true);
}

static uiLayout *rna_uiLayoutBox(uiLayout *layout)
{
  return &layout->box();
}

static uiLayout *rna_uiLayoutSplit(uiLayout *layout, float factor, bool align)
{
  return &layout->split(factor, align);
}

static uiLayout *rna_uiLayoutRowWithHeading(
    uiLayout *layout, bool align, const char *heading, const char *heading_ctxt, bool translate)
{
  /* Get translated heading. */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      heading, heading_ctxt, nullptr, nullptr, translate);
  return &layout->row(align, text.value_or(""));
}

static uiLayout *rna_uiLayoutColumnWithHeading(
    uiLayout *layout, bool align, const char *heading, const char *heading_ctxt, bool translate)
{
  /* Get translated heading. */
  std::optional<StringRefNull> text = rna_translate_ui_text(
      heading, heading_ctxt, nullptr, nullptr, translate);
  return &layout->column(align, text.value_or(""));
}

static uiLayout *rna_uiLayoutColumnFlow(uiLayout *layout, int number, bool align)
{
  return &layout->column_flow(number, align);
}

static uiLayout *rna_uiLayoutGridFlow(uiLayout *layout,
                                      bool row_major,
                                      int columns_len,
                                      bool even_columns,
                                      bool even_rows,
                                      bool align)
{
  return &layout->grid_flow(row_major, columns_len, even_columns, even_rows, align);
}

static uiLayout *rna_uiLayoutMenuPie(uiLayout *layout)
{
  return &layout->menu_pie();
}

void rna_uiLayoutPanelProp(uiLayout *layout,
                           bContext *C,
                           ReportList *reports,
                           PointerRNA *data,
                           const char *property,
                           uiLayout **r_layout_header,
                           uiLayout **r_layout_body)
{
  Panel *panel = layout->root_panel();
  if (panel == nullptr) {
    BKE_reportf(reports, RPT_ERROR, "Layout panels can not be used in this context");
    *r_layout_header = nullptr;
    *r_layout_body = nullptr;
    return;
  }

  PanelLayout panel_layout = layout->panel_prop(C, data, property);
  *r_layout_header = panel_layout.header;
  *r_layout_body = panel_layout.body;
}

void rna_uiLayoutPanel(uiLayout *layout,
                       bContext *C,
                       ReportList *reports,
                       const char *idname,
                       const bool default_closed,
                       uiLayout **r_layout_header,
                       uiLayout **r_layout_body)
{
  Panel *panel = layout->root_panel();
  if (panel == nullptr) {
    BKE_reportf(reports, RPT_ERROR, "Layout panels can not be used in this context");
    *r_layout_header = nullptr;
    *r_layout_body = nullptr;
    return;
  }
  PanelLayout panel_layout = layout->panel(C, idname, default_closed);
  *r_layout_header = panel_layout.header;
  *r_layout_body = panel_layout.body;
}

static void rna_uiLayout_template_node_asset_menu_items(uiLayout *layout,
                                                        bContext *C,
                                                        const char *catalog_path,
                                                        const int operator_type)
{
  using namespace blender;
  ed::space_node::ui_template_node_asset_menu_items(
      *layout, *C, StringRef(catalog_path), NodeAssetMenuOperatorType(operator_type));
}

static void rna_uiLayout_template_node_operator_asset_menu_items(uiLayout *layout,
                                                                 bContext *C,
                                                                 const char *catalog_path)
{
  using namespace blender;
  ed::geometry::ui_template_node_operator_asset_menu_items(*layout, *C, StringRef(catalog_path));
}

static void rna_uiLayout_template_modifier_asset_menu_items(uiLayout *layout,
                                                            const char *catalog_path,
                                                            const bool skip_essentials)
{
  using namespace blender;
  ed::object::ui_template_modifier_asset_menu_items(
      *layout, StringRef(catalog_path), skip_essentials);
}

static void rna_uiLayout_template_node_operator_root_items(uiLayout *layout, bContext *C)
{
  blender::ed::geometry::ui_template_node_operator_asset_root_items(*layout, *C);
}

static int rna_ui_get_rnaptr_icon(bContext *C, PointerRNA *ptr_icon)
{
  return UI_icon_from_rnaptr(C, ptr_icon, RNA_struct_ui_icon(ptr_icon->type), false);
}

static const char *rna_ui_get_enum_name(bContext *C,
                                        PointerRNA *ptr,
                                        const char *propname,
                                        const char *identifier)
{
  PropertyRNA *prop = nullptr;
  const EnumPropertyItem *items = nullptr;
  bool free;
  const char *name = "";

  prop = RNA_struct_find_property(ptr, propname);
  if (!prop || (RNA_property_type(prop) != PROP_ENUM)) {
    RNA_warning(
        "Property not found or not an enum: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return name;
  }

  RNA_property_enum_items_gettexted(C, ptr, prop, &items, nullptr, &free);

  if (items) {
    const int index = RNA_enum_from_identifier(items, identifier);
    if (index != -1) {
      name = items[index].name;
    }
    if (free) {
      MEM_freeN(items);
    }
  }

  return name;
}

static const char *rna_ui_get_enum_description(bContext *C,
                                               PointerRNA *ptr,
                                               const char *propname,
                                               const char *identifier)
{
  PropertyRNA *prop = nullptr;
  const EnumPropertyItem *items = nullptr;
  bool free;
  const char *desc = "";

  prop = RNA_struct_find_property(ptr, propname);
  if (!prop || (RNA_property_type(prop) != PROP_ENUM)) {
    RNA_warning(
        "Property not found or not an enum: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return desc;
  }

  RNA_property_enum_items_gettexted(C, ptr, prop, &items, nullptr, &free);

  if (items) {
    const int index = RNA_enum_from_identifier(items, identifier);
    if (index != -1) {
      desc = items[index].description;
    }
    if (free) {
      MEM_freeN(items);
    }
  }

  return desc;
}

static int rna_ui_get_enum_icon(bContext *C,
                                PointerRNA *ptr,
                                const char *propname,
                                const char *identifier)
{
  PropertyRNA *prop = nullptr;
  const EnumPropertyItem *items = nullptr;
  bool free;
  int icon = ICON_NONE;

  prop = RNA_struct_find_property(ptr, propname);
  if (!prop || (RNA_property_type(prop) != PROP_ENUM)) {
    RNA_warning(
        "Property not found or not an enum: %s.%s", RNA_struct_identifier(ptr->type), propname);
    return icon;
  }

  RNA_property_enum_items(C, ptr, prop, &items, nullptr, &free);

  if (items) {
    const int index = RNA_enum_from_identifier(items, identifier);
    if (index != -1) {
      icon = items[index].icon;
    }
    if (free) {
      MEM_freeN(items);
    }
  }

  return icon;
}

void rna_uiTemplateAssetShelfPopover(uiLayout *layout,
                                     bContext *C,
                                     const char *asset_shelf_id,
                                     const char *name,
                                     BIFIconID icon,
                                     int icon_value)
{
  if (icon_value && !icon) {
    icon = icon_value;
  }

  blender::ui::template_asset_shelf_popover(*layout, *C, asset_shelf_id, name ? name : "", icon);
}

PointerRNA rna_uiTemplatePopupConfirm(uiLayout *layout,
                                      ReportList *reports,
                                      const char *opname,
                                      const char *text,
                                      const char *text_ctxt,
                                      bool translate,
                                      int icon,
                                      const char *cancel_text,
                                      bool cancel_default)
{
  PointerRNA opptr = PointerRNA_NULL;

  /* This allows overriding buttons in `WM_operator_props_dialog_popup` and other popups. */
  wmOperatorType *ot = nullptr;
  if (opname[0]) {
    /* Confirming is optional. */
    ot = WM_operatortype_find(opname, false); /* print error next */
  }
  else {
    text = "";
  }

  if (opname[0] ? (!ot || !ot->srna) : false) {
    RNA_warning("%s '%s'", ot ? "operator missing srna" : "unknown operator", opname);
  }
  else if (!UI_popup_block_template_confirm_is_supported(layout->block())) {
    BKE_reportf(reports, RPT_ERROR, "template_popup_confirm used outside of a popup");
  }
  else {
    std::optional<StringRefNull> text_str = rna_translate_ui_text(
        text, text_ctxt, nullptr, nullptr, translate);
    std::optional<StringRefNull> cancel_text_str;
    if (cancel_text && cancel_text[0]) {
      cancel_text_str = rna_translate_ui_text(cancel_text, text_ctxt, nullptr, nullptr, translate);
    }

    UI_popup_block_template_confirm_op(
        layout, ot, text_str, cancel_text_str, icon, cancel_default, &opptr);
  }
  return opptr;
}

#else

static void api_ui_item_common_heading(FunctionRNA *func)
{
  RNA_def_string(func,
                 "heading",
                 nullptr,
                 UI_MAX_NAME_STR,
                 "Heading",
                 "Label to insert into the layout for this sub-layout");
  RNA_def_string(func,
                 "heading_ctxt",
                 nullptr,
                 0,
                 "",
                 "Override automatic translation context of the given heading");
  RNA_def_boolean(
      func, "translate", true, "", "Translate the given heading, when UI translation is enabled");
}

void api_ui_item_common_translation(FunctionRNA *func)
{
  PropertyRNA *prop = RNA_def_string(func,
                                     "text_ctxt",
                                     nullptr,
                                     0,
                                     "",
                                     "Override automatic translation context of the given text");
  RNA_def_property_clear_flag(prop, PROP_NEVER_NULL);
  RNA_def_boolean(
      func, "translate", true, "", "Translate the given text, when UI translation is enabled");
}

static void api_ui_item_common_text(FunctionRNA *func)
{
  PropertyRNA *prop;

  prop = RNA_def_string(func, "text", nullptr, 0, "", "Override automatic text of the item");
  RNA_def_property_clear_flag(prop, PROP_NEVER_NULL);
  api_ui_item_common_translation(func);
}

static void api_ui_item_common(FunctionRNA *func)
{
  PropertyRNA *prop;

  api_ui_item_common_text(func);

  prop = RNA_def_property(func, "icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_icon_items);
  RNA_def_property_ui_text(prop, "Icon", "Override automatic icon of the item");
}

static void api_ui_item_op(FunctionRNA *func)
{
  PropertyRNA *parm;
  parm = RNA_def_string(func, "operator", nullptr, 0, "", "Identifier of the operator");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void api_ui_item_op_common(FunctionRNA *func)
{
  api_ui_item_op(func);
  api_ui_item_common(func);
}

static void api_ui_item_rna_common(FunctionRNA *func)
{
  PropertyRNA *parm;

  parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_string(func, "property", nullptr, 0, "", "Identifier of property in data");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_api_ui_layout(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  static const EnumPropertyItem curve_type_items[] = {
      {0, "NONE", 0, "None", ""},
      {'v', "VECTOR", 0, "Vector", ""},
      {'c', "COLOR", 0, "Color", ""},
      {'h', "HUE", 0, "Hue", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem id_template_filter_items[] = {
      {UI_TEMPLATE_ID_FILTER_ALL, "ALL", 0, "All", ""},
      {UI_TEMPLATE_ID_FILTER_AVAILABLE, "AVAILABLE", 0, "Available", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem progress_type_items[] = {
      {int(blender::ui::ButProgressType::Bar), "BAR", 0, "Bar", ""},
      {int(blender::ui::ButProgressType::Ring), "RING", 0, "Ring", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem rna_enum_separator_type_items[] = {
      {int(LayoutSeparatorType::Auto),
       "AUTO",
       0,
       "Auto",
       "Best guess at what type of separator is needed."},
      {int(LayoutSeparatorType::Space),
       "SPACE",
       0,
       "Empty space",
       "Horizontal or Vertical empty space, depending on layout direction."},
      {int(LayoutSeparatorType::Line),
       "LINE",
       0,
       "Line",
       "Horizontal or Vertical line, depending on layout direction."},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem rna_enum_template_node_operator_type[] = {
      {int(NodeAssetMenuOperatorType::Add),
       "ADD",
       0,
       "Add Node",
       "Add a node to the active tree."},
      {int(NodeAssetMenuOperatorType::Swap),
       "SWAP",
       0,
       "Swap Node",
       "Replace the selected nodes with the specified type."},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const float node_socket_color_default[] = {0.0f, 0.0f, 0.0f, 1.0f};

  /* simple layout specifiers */
  func = RNA_def_function(srna, "row", "rna_uiLayoutRowWithHeading");
  parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
  RNA_def_function_return(func, parm);
  RNA_def_function_ui_description(
      func,
      "Sub-layout. Items placed in this sublayout are placed next to each other "
      "in a row.");
  RNA_def_boolean(func, "align", false, "", "Align buttons to each other");
  api_ui_item_common_heading(func);

  func = RNA_def_function(srna, "column", "rna_uiLayoutColumnWithHeading");
  parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
  RNA_def_function_return(func, parm);
  RNA_def_function_ui_description(
      func,
      "Sub-layout. Items placed in this sublayout are placed under each other "
      "in a column.");
  RNA_def_boolean(func, "align", false, "", "Align buttons to each other");
  api_ui_item_common_heading(func);

  func = RNA_def_function(srna, "panel", "rna_uiLayoutPanel");
  RNA_def_function_ui_description(
      func,
      "Creates a collapsible panel. Whether it is open or closed is stored in the region using "
      "the given idname. This can only be used when the panel has the full width of the panel "
      "region available to it. So it can't be used in e.g. in a box or columns.");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "idname", nullptr, 0, "", "Identifier of the panel");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_boolean(func,
                  "default_closed",
                  false,
                  "Open by Default",
                  "When true, the panel will be open the first time it is shown");
  parm = RNA_def_pointer(func, "layout_header", "UILayout", "", "Sub-layout to put items in");
  RNA_def_function_output(func, parm);
  parm = RNA_def_pointer(func,
                         "layout_body",
                         "UILayout",
                         "",
                         "Sub-layout to put items in. Will be none if the panel is collapsed.");
  RNA_def_function_output(func, parm);

  func = RNA_def_function(srna, "panel_prop", "rna_uiLayoutPanelProp");
  RNA_def_function_ui_description(
      func,
      "Similar to ``.panel(...)`` but instead of storing whether it is open or closed in the "
      "region, it is stored in the provided boolean property. This should be used when multiple "
      "instances of the same panel can exist. For example one for every item in a collection "
      "property or list. This can only be used when the panel has the full width of the panel "
      "region available to it. So it can't be used in e.g. in a box or columns.");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(
      func, "data", "AnyType", "", "Data from which to take the open-state property");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_string(
      func,
      "property",
      nullptr,
      0,
      "",
      "Identifier of the boolean property that determines whether the panel is open or closed");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "layout_header", "UILayout", "", "Sub-layout to put items in");
  RNA_def_function_output(func, parm);
  parm = RNA_def_pointer(func,
                         "layout_body",
                         "UILayout",
                         "",
                         "Sub-layout to put items in. Will be none if the panel is collapsed.");
  RNA_def_function_output(func, parm);

  func = RNA_def_function(srna, "column_flow", "rna_uiLayoutColumnFlow");
  RNA_def_int(func, "columns", 0, 0, INT_MAX, "", "Number of columns, 0 is automatic", 0, INT_MAX);
  parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
  RNA_def_function_return(func, parm);
  RNA_def_boolean(func, "align", false, "", "Align buttons to each other");

  func = RNA_def_function(srna, "grid_flow", "rna_uiLayoutGridFlow");
  RNA_def_boolean(func, "row_major", false, "", "Fill row by row, instead of column by column");
  RNA_def_int(
      func,
      "columns",
      0,
      INT_MIN,
      INT_MAX,
      "",
      "Number of columns, positive are absolute fixed numbers, 0 is automatic, negative are "
      "automatic multiple numbers along major axis (e.g. -2 will only produce 2, 4, 6 etc. "
      "columns for row major layout, and 2, 4, 6 etc. rows for column major layout).",
      INT_MIN,
      INT_MAX);
  RNA_def_boolean(func, "even_columns", false, "", "All columns will have the same width");
  RNA_def_boolean(func, "even_rows", false, "", "All rows will have the same height");
  RNA_def_boolean(func, "align", false, "", "Align buttons to each other");
  parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
  RNA_def_function_return(func, parm);

  /* box layout */
  func = RNA_def_function(srna, "box", "rna_uiLayoutBox");
  parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
  RNA_def_function_return(func, parm);
  RNA_def_function_ui_description(func,
                                  "Sublayout (items placed in this sublayout are placed "
                                  "under each other in a column and are surrounded by a box)");

  /* split layout */
  func = RNA_def_function(srna, "split", "rna_uiLayoutSplit");
  parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
  RNA_def_function_return(func, parm);
  RNA_def_float(func,
                "factor",
                0.0f,
                0.0f,
                1.0f,
                "Percentage",
                "Percentage of width to split at (leave unset for automatic calculation)",
                0.0f,
                1.0f);
  RNA_def_boolean(func, "align", false, "", "Align buttons to each other");

  /* radial/pie layout */
  func = RNA_def_function(srna, "menu_pie", "rna_uiLayoutMenuPie");
  parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
  RNA_def_function_return(func, parm);
  RNA_def_function_ui_description(func,
                                  "Sublayout. Items placed in this sublayout are placed "
                                  "in a radial fashion around the menu center).");

  /* Icon of a rna pointer */
  func = RNA_def_function(srna, "icon", "rna_ui_get_rnaptr_icon");
  parm = RNA_def_int(func, "icon_value", ICON_NONE, 0, INT_MAX, "", "Icon identifier", 0, INT_MAX);
  RNA_def_function_return(func, parm);
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take the icon");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_function_ui_description(func,
                                  "Return the custom icon for this data, "
                                  "use it e.g. to get materials or texture icons.");

  /* UI name, description and icon of an enum item */
  func = RNA_def_function(srna, "enum_item_name", "rna_ui_get_enum_name");
  parm = RNA_def_string(func, "name", nullptr, 0, "", "UI name of the enum item");
  RNA_def_function_return(func, parm);
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
  api_ui_item_rna_common(func);
  parm = RNA_def_string(func, "identifier", nullptr, 0, "", "Identifier of the enum item");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_function_ui_description(func, "Return the UI name for this enum item");

  func = RNA_def_function(srna, "enum_item_description", "rna_ui_get_enum_description");
  parm = RNA_def_string(func, "description", nullptr, 0, "", "UI description of the enum item");
  RNA_def_function_return(func, parm);
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
  api_ui_item_rna_common(func);
  parm = RNA_def_string(func, "identifier", nullptr, 0, "", "Identifier of the enum item");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_function_ui_description(func, "Return the UI description for this enum item");

  func = RNA_def_function(srna, "enum_item_icon", "rna_ui_get_enum_icon");
  parm = RNA_def_int(func, "icon_value", ICON_NONE, 0, INT_MAX, "", "Icon identifier", 0, INT_MAX);
  RNA_def_function_return(func, parm);
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
  api_ui_item_rna_common(func);
  parm = RNA_def_string(func, "identifier", nullptr, 0, "", "Identifier of the enum item");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_function_ui_description(func, "Return the icon for this enum item");

  /* items */
  func = RNA_def_function(srna, "prop", "rna_uiItemR");
  RNA_def_function_ui_description(func,
                                  "Item. Exposes an RNA item and places it into the layout.");
  api_ui_item_rna_common(func);
  api_ui_item_common(func);
  PropertyRNA *prop = RNA_def_string(
      func, "placeholder", nullptr, 0, "", "Hint describing the expected value when empty");
  RNA_def_property_clear_flag(prop, PROP_NEVER_NULL);
  RNA_def_boolean(func, "expand", false, "", "Expand button to show more detail");
  RNA_def_boolean(func, "slider", false, "", "Use slider widget for numeric values");
  RNA_def_int(func,
              "toggle",
              -1,
              -1,
              1,
              "",
              "Use toggle widget for boolean values, "
              "or a checkbox when disabled "
              "(the default is -1 which uses toggle only when an icon is displayed)",
              -1,
              1);
  RNA_def_boolean(func, "icon_only", false, "", "Draw only icons in buttons, no text");
  RNA_def_boolean(func, "event", false, "", "Use button to input key events");
  RNA_def_boolean(
      func, "full_event", false, "", "Use button to input full events including modifiers");
  RNA_def_boolean(func,
                  "emboss",
                  true,
                  "",
                  "Draw the button itself, not just the icon/text. When false, corresponds to the "
                  "'NONE_OR_STATUS' layout emboss type.");
  RNA_def_int(func,
              "index",
              /* RNA_NO_INDEX == -1 */
              -1,
              -2,
              INT_MAX,
              "",
              "The index of this button, when set a single member of an array can be accessed, "
              "when set to -1 all array members are used",
              -2,
              INT_MAX);
  parm = RNA_def_property(func, "icon_value", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(parm, "Icon Value", "Override automatic icon of the item");
  RNA_def_boolean(func, "invert_checkbox", false, "", "Draw checkbox value inverted");

  func = RNA_def_function(srna, "props_enum", "rna_uiItemsEnumR");
  api_ui_item_rna_common(func);

  func = RNA_def_function(srna, "prop_menu_enum", "rna_uiItemMenuEnumR");
  api_ui_item_rna_common(func);
  api_ui_item_common(func);

  func = RNA_def_function(srna, "prop_with_popover", "rna_uiItemR_with_popover");
  api_ui_item_rna_common(func);
  api_ui_item_common(func);
  RNA_def_boolean(func, "icon_only", false, "", "Draw only icons in tabs, no text");
  parm = RNA_def_string(func, "panel", nullptr, 0, "", "Identifier of the panel");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "prop_with_menu", "rna_uiItemR_with_menu");
  api_ui_item_rna_common(func);
  api_ui_item_common(func);
  RNA_def_boolean(func, "icon_only", false, "", "Draw only icons in tabs, no text");
  parm = RNA_def_string(func, "menu", nullptr, 0, "", "Identifier of the menu");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "prop_tabs_enum", "rna_uiItemTabsEnumR");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  api_ui_item_rna_common(func);
  parm = RNA_def_pointer(
      func, "data_highlight", "AnyType", "", "Data from which to take highlight property");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_RNAPTR);
  parm = RNA_def_string(
      func, "property_highlight", nullptr, 0, "", "Identifier of highlight property in data");
  RNA_def_boolean(func, "icon_only", false, "", "Draw only icons in tabs, no text");

  func = RNA_def_function(srna, "prop_enum", "rna_uiItemEnumR_string");
  api_ui_item_rna_common(func);
  parm = RNA_def_string(func, "value", nullptr, 0, "", "Enum property value");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  api_ui_item_common(func);

  func = RNA_def_function(srna, "prop_search", "rna_uiItemPointerR");
  api_ui_item_rna_common(func);
  parm = RNA_def_pointer(
      func, "search_data", "AnyType", "", "Data from which to take collection to search in");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_string(
      func, "search_property", nullptr, 0, "", "Identifier of search collection property");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  api_ui_item_common(func);
  RNA_def_boolean(
      func, "results_are_suggestions", false, "", "Accept inputs that do not match any item");
  parm = RNA_def_string(func,
                        "item_search_property",
                        nullptr,
                        0,
                        "",
                        "Identifier of the string property in each collection's items to use for "
                        "searching (defaults to the items' type 'name property')");

  func = RNA_def_function(srna, "prop_decorator", "rna_uiLayoutDecorator");
  api_ui_item_rna_common(func);
  RNA_def_int(func,
              "index",
              /* RNA_NO_INDEX == -1 */
              -1,
              -2,
              INT_MAX,
              "",
              "The index of this button, when set a single member of an array can be accessed, "
              "when set to -1 all array members are used",
              -2,
              INT_MAX);

  for (int is_menu_hold = 0; is_menu_hold < 2; is_menu_hold++) {
    func = (is_menu_hold) ? RNA_def_function(srna, "operator_menu_hold", "rna_uiItemOMenuHold") :
                            RNA_def_function(srna, "operator", "rna_uiItemO");
    api_ui_item_op_common(func);
    RNA_def_boolean(func, "emboss", true, "", "Draw the button itself, not just the icon/text");
    RNA_def_boolean(func, "depress", false, "", "Draw pressed in");
    parm = RNA_def_property(func, "icon_value", PROP_INT, PROP_UNSIGNED);
    RNA_def_property_ui_text(parm, "Icon Value", "Override automatic icon of the item");
    if (is_menu_hold) {
      parm = RNA_def_string(func, "menu", nullptr, 0, "", "Identifier of the menu");
      RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
    }
    else {
      RNA_def_float(func,
                    "search_weight",
                    0.0f,
                    -FLT_MAX,
                    FLT_MAX,
                    "Search Weight",
                    "Influences the sorting when using menu-seach",
                    -FLT_MAX,
                    FLT_MAX);
    }
    parm = RNA_def_pointer(
        func, "properties", "OperatorProperties", "", "Operator properties to fill in");
    RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
    RNA_def_function_return(func, parm);
    RNA_def_function_ui_description(func,
                                    "Item. Places a button into the layout to call an Operator.");
  }

  func = RNA_def_function(srna, "operator_enum", "rna_uiItemsEnumO");
  parm = RNA_def_string(func, "operator", nullptr, 0, "", "Identifier of the operator");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "property", nullptr, 0, "", "Identifier of property in operator");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func, "icon_only", false, "", "Draw only icons in buttons, no text");

  func = RNA_def_function(srna, "operator_menu_enum", "rna_uiItemMenuEnumO");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  /* Can't use #api_ui_item_op_common because property must come right after. */
  api_ui_item_op(func);
  parm = RNA_def_string(func, "property", nullptr, 0, "", "Identifier of property in operator");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  api_ui_item_common(func);
  parm = RNA_def_pointer(
      func, "properties", "OperatorProperties", "", "Operator properties to fill in");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "label", "rna_uiItemL");
  RNA_def_function_ui_description(func, "Item. Displays text and/or icon in the layout.");
  api_ui_item_common(func);
  parm = RNA_def_property(func, "icon_value", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(parm, "Icon Value", "Override automatic icon of the item");

  func = RNA_def_function(srna, "menu", "rna_uiItemM");
  parm = RNA_def_string(func, "menu", nullptr, 0, "", "Identifier of the menu");
  api_ui_item_common(func);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_property(func, "icon_value", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(parm, "Icon Value", "Override automatic icon of the item");

  func = RNA_def_function(srna, "menu_contents", "rna_uiItemM_contents");
  parm = RNA_def_string(func, "menu", nullptr, 0, "", "Identifier of the menu");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "popover", "rna_uiItemPopoverPanel");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_string(func, "panel", nullptr, 0, "", "Identifier of the panel");
  api_ui_item_common(func);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_property(func, "icon_value", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(parm, "Icon Value", "Override automatic icon of the item");

  func = RNA_def_function(srna, "popover_group", "rna_uiItemPopoverPanelFromGroup");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_enum(func, "space_type", rna_enum_space_type_items, 0, "Space Type", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(
      func, "region_type", rna_enum_region_type_items, RGN_TYPE_WINDOW, "Region Type", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "context", nullptr, 0, "", "panel type context");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "category", nullptr, 0, "", "panel type category");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "separator", "rna_uiItemSeparator");
  RNA_def_function_ui_description(func,
                                  "Item. Inserts empty space into the layout between items.");
  RNA_def_float(func,
                "factor",
                1.0f,
                0.0f,
                FLT_MAX,
                "Percentage",
                "Percentage of width to space (leave unset for default space)",
                0.0f,
                FLT_MAX);
  RNA_def_enum(func,
               "type",
               rna_enum_separator_type_items,
               int(LayoutSeparatorType::Auto),
               "Type",
               "The type of the separator");

  func = RNA_def_function(srna, "separator_spacer", "rna_uiLayoutSeparatorSpacer");
  RNA_def_function_ui_description(
      func, "Item. Inserts horizontal spacing empty space into the layout between items.");

  func = RNA_def_function(srna, "progress", "rna_uiItemProgress");
  RNA_def_function_ui_description(func, "Progress indicator");
  api_ui_item_common_text(func);
  RNA_def_float(func,
                "factor",
                0.0f,
                0.0f,
                1.0f,
                "Factor",
                "Amount of progress from 0.0f to 1.0f",
                0.0f,
                1.0f);
  RNA_def_enum(func,
               "type",
               progress_type_items,
               int(blender::ui::ButProgressType::Bar),
               "Type",
               "The type of progress indicator");

  /* context */
  func = RNA_def_function(srna, "context_pointer_set", "rna_uiLayoutContextPointerSet");
  parm = RNA_def_string(func, "name", nullptr, 0, "Name", "Name of entry in the context");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "data", "AnyType", "", "Pointer to put in context");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);

  func = RNA_def_function(srna, "context_string_set", "rna_uiLayoutContextStringSet");
  parm = RNA_def_string(func, "name", nullptr, 0, "Name", "Name of entry in the context");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "value", nullptr, 0, "Value", "String to put in context");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);

  /* templates */
  func = RNA_def_function(srna, "template_header", "uiTemplateHeader");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Inserts common Space header UI (editor type selector)");

  func = RNA_def_function(srna, "template_ID", "rna_uiTemplateID");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  api_ui_item_rna_common(func);
  RNA_def_string(func, "new", nullptr, 0, "", "Operator identifier to create a new ID block");
  RNA_def_string(func,
                 "open",
                 nullptr,
                 0,
                 "",
                 "Operator identifier to open a file for creating a new ID block");
  RNA_def_string(func, "unlink", nullptr, 0, "", "Operator identifier to unlink the ID block");
  RNA_def_enum(func,
               "filter",
               id_template_filter_items,
               UI_TEMPLATE_ID_FILTER_ALL,
               "",
               "Optionally limit the items which can be selected");
  RNA_def_boolean(func, "live_icon", false, "", "Show preview instead of fixed icon");
  api_ui_item_common_text(func);

  func = RNA_def_function(srna, "template_ID_preview", "uiTemplateIDPreview");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  api_ui_item_rna_common(func);
  RNA_def_string(func, "new", nullptr, 0, "", "Operator identifier to create a new ID block");
  RNA_def_string(func,
                 "open",
                 nullptr,
                 0,
                 "",
                 "Operator identifier to open a file for creating a new ID block");
  RNA_def_string(func, "unlink", nullptr, 0, "", "Operator identifier to unlink the ID block");
  RNA_def_int(
      func, "rows", 0, 0, INT_MAX, "Number of thumbnail preview rows to display", "", 0, INT_MAX);
  RNA_def_int(func,
              "cols",
              0,
              0,
              INT_MAX,
              "Number of thumbnail preview columns to display",
              "",
              0,
              INT_MAX);
  RNA_def_enum(func,
               "filter",
               id_template_filter_items,
               UI_TEMPLATE_ID_FILTER_ALL,
               "",
               "Optionally limit the items which can be selected");
  RNA_def_boolean(func, "hide_buttons", false, "", "Show only list, no buttons");

  func = RNA_def_function(srna, "template_matrix", "uiTemplateMatrix");
  RNA_def_function_ui_description(
      func,
      "Insert a readonly Matrix UI. "
      "The UI displays the matrix components - translation, rotation and scale. "
      "The **property** argument must be the identifier of an existing 4x4 float vector "
      "property of subtype 'MATRIX'.");
  api_ui_item_rna_common(func);

  func = RNA_def_function(srna, "template_any_ID", "rna_uiTemplateAnyID");
  parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_string(func, "property", nullptr, 0, "", "Identifier of property in data");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func,
                        "type_property",
                        nullptr,
                        0,
                        "",
                        "Identifier of property in data giving the type of the ID-blocks to use");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  api_ui_item_common_text(func);

  func = RNA_def_function(srna, "template_ID_tabs", "uiTemplateIDTabs");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  api_ui_item_rna_common(func);
  RNA_def_string(func, "new", nullptr, 0, "", "Operator identifier to create a new ID block");
  RNA_def_string(func, "menu", nullptr, 0, "", "Context menu identifier");
  RNA_def_enum(func,
               "filter",
               id_template_filter_items,
               UI_TEMPLATE_ID_FILTER_ALL,
               "",
               "Optionally limit the items which can be selected");

  func = RNA_def_function(srna, "template_action", "rna_uiTemplateAction");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "id", "ID", "", "The data-block for which to select an Action");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_string(func, "new", nullptr, 0, "", "Operator identifier to create a new ID block");
  RNA_def_string(func, "unlink", nullptr, 0, "", "Operator identifier to unlink the ID block");
  api_ui_item_common_text(func);

  func = RNA_def_function(srna, "template_search", "rna_uiTemplateSearch");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  api_ui_item_rna_common(func);
  parm = RNA_def_pointer(
      func, "search_data", "AnyType", "", "Data from which to take collection to search in");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_string(
      func, "search_property", nullptr, 0, "", "Identifier of search collection property");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_string(
      func, "new", nullptr, 0, "", "Operator identifier to create a new item for the collection");
  RNA_def_string(func,
                 "unlink",
                 nullptr,
                 0,
                 "",
                 "Operator identifier to unlink or delete the active "
                 "item from the collection");
  api_ui_item_common_text(func);

  func = RNA_def_function(srna, "template_search_preview", "rna_uiTemplateSearchPreview");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  api_ui_item_rna_common(func);
  parm = RNA_def_pointer(
      func, "search_data", "AnyType", "", "Data from which to take collection to search in");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_string(
      func, "search_property", nullptr, 0, "", "Identifier of search collection property");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_string(
      func, "new", nullptr, 0, "", "Operator identifier to create a new item for the collection");
  RNA_def_string(func,
                 "unlink",
                 nullptr,
                 0,
                 "",
                 "Operator identifier to unlink or delete the active "
                 "item from the collection");
  api_ui_item_common_text(func);
  RNA_def_int(
      func, "rows", 0, 0, INT_MAX, "Number of thumbnail preview rows to display", "", 0, INT_MAX);
  RNA_def_int(func,
              "cols",
              0,
              0,
              INT_MAX,
              "Number of thumbnail preview columns to display",
              "",
              0,
              INT_MAX);

  func = RNA_def_function(srna, "template_path_builder", "rna_uiTemplatePathBuilder");
  parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_string(func, "property", nullptr, 0, "", "Identifier of property in data");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "root", "ID", "", "ID-block from which path is evaluated from");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
  api_ui_item_common_text(func);

  func = RNA_def_function(srna, "template_modifiers", "uiTemplateModifiers");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Generates the UI layout for the modifier stack");

  func = RNA_def_function(srna, "template_strip_modifiers", "uiTemplateStripModifiers");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Generates the UI layout for the strip modifier stack");

  func = RNA_def_function(srna, "template_collection_exporters", "uiTemplateCollectionExporters");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Generates the UI layout for collection exporters");

  func = RNA_def_function(srna, "template_constraints", "uiTemplateConstraints");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Generates the panels for the constraint stack");
  RNA_def_boolean(func,
                  "use_bone_constraints",
                  true,
                  "",
                  "Add panels for bone constraints instead of object constraints");

  func = RNA_def_function(srna, "template_shaderfx", "uiTemplateShaderFx");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Generates the panels for the shader effect stack");

  func = RNA_def_function(srna, "template_greasepencil_color", "uiTemplateGpencilColorPreview");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  api_ui_item_rna_common(func);
  RNA_def_int(
      func, "rows", 0, 0, INT_MAX, "Number of thumbnail preview rows to display", "", 0, INT_MAX);
  RNA_def_int(func,
              "cols",
              0,
              0,
              INT_MAX,
              "Number of thumbnail preview columns to display",
              "",
              0,
              INT_MAX);
  RNA_def_float(func, "scale", 1.0f, 0.1f, 1.5f, "Scale of the image thumbnails", "", 0.5f, 1.0f);
  RNA_def_enum(func,
               "filter",
               id_template_filter_items,
               UI_TEMPLATE_ID_FILTER_ALL,
               "",
               "Optionally limit the items which can be selected");

  func = RNA_def_function(srna, "template_constraint_header", "uiTemplateConstraintHeader");
  RNA_def_function_ui_description(func, "Generates the header for constraint panels");
  parm = RNA_def_pointer(func, "data", "Constraint", "", "Constraint data");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);

  func = RNA_def_function(srna, "template_preview", "uiTemplatePreview");
  RNA_def_function_ui_description(
      func, "Item. A preview window for materials, textures, lights or worlds.");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "id", "ID", "", "ID data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func, "show_buttons", true, "", "Show preview buttons?");
  RNA_def_pointer(func, "parent", "ID", "", "ID data-block");
  RNA_def_pointer(func, "slot", "TextureSlot", "", "Texture slot");
  RNA_def_string(
      func,
      "preview_id",
      nullptr,
      0,
      "",
      "Identifier of this preview widget, if not set the ID type will be used "
      "(i.e. all previews of materials without explicit ID will have the same size...).");

  func = RNA_def_function(srna, "template_curve_mapping", "uiTemplateCurveMapping");
  RNA_def_function_ui_description(
      func, "Item. A curve mapping widget used for e.g falloff curves for lights.");
  api_ui_item_rna_common(func);
  RNA_def_enum(func, "type", curve_type_items, 0, "Type", "Type of curves to display");
  RNA_def_boolean(func, "levels", false, "", "Show black/white levels");
  RNA_def_boolean(func, "brush", false, "", "Show brush options");
  RNA_def_boolean(func, "use_negative_slope", false, "", "Use a negative slope by default");
  RNA_def_boolean(func, "show_tone", false, "", "Show tone options");
  RNA_def_boolean(func, "show_presets", false, "", "Show preset options");

  func = RNA_def_function(srna, "template_curveprofile", "uiTemplateCurveProfile");
  RNA_def_function_ui_description(func, "A profile path editor used for custom profiles");
  api_ui_item_rna_common(func);

  func = RNA_def_function(srna, "template_color_ramp", "uiTemplateColorRamp");
  RNA_def_function_ui_description(func, "Item. A color ramp widget.");
  api_ui_item_rna_common(func);
  RNA_def_boolean(func, "expand", false, "", "Expand button to show more detail");

  func = RNA_def_function(srna, "template_icon", "uiTemplateIcon");
  RNA_def_function_ui_description(func, "Display a large icon");
  parm = RNA_def_int(func, "icon_value", 0, 0, INT_MAX, "Icon to display", "", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_float(func,
                "scale",
                1.0f,
                1.0f,
                100.0f,
                "Scale",
                "Scale the icon size (by the button size)",
                1.0f,
                100.0f);

  func = RNA_def_function(srna, "template_icon_view", "uiTemplateIconView");
  RNA_def_function_ui_description(func, "Enum. Large widget showing Icon previews.");
  api_ui_item_rna_common(func);
  RNA_def_boolean(func, "show_labels", false, "", "Show enum label in preview buttons");
  RNA_def_float(func,
                "scale",
                6.0f,
                1.0f,
                100.0f,
                "UI Units",
                "Scale the button icon size (by the button size)",
                1.0f,
                100.0f);
  RNA_def_float(func,
                "scale_popup",
                5.0f,
                1.0f,
                100.0f,
                "Scale",
                "Scale the popup icon size (by the button size)",
                1.0f,
                100.0f);

  func = RNA_def_function(srna, "template_histogram", "uiTemplateHistogram");
  RNA_def_function_ui_description(func, "Item. A histogramm widget to analyze imaga data.");
  api_ui_item_rna_common(func);

  func = RNA_def_function(srna, "template_waveform", "uiTemplateWaveform");
  RNA_def_function_ui_description(func, "Item. A waveform widget to analyze imaga data.");
  api_ui_item_rna_common(func);

  func = RNA_def_function(srna, "template_vectorscope", "uiTemplateVectorscope");
  RNA_def_function_ui_description(func, "Item. A vectorscope widget to analyze imaga data.");
  api_ui_item_rna_common(func);

  func = RNA_def_function(srna, "template_layers", "uiTemplateLayers");
  api_ui_item_rna_common(func);
  parm = RNA_def_pointer(
      func, "used_layers_data", "AnyType", "", "Data from which to take property");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_string(
      func, "used_layers_property", nullptr, 0, "", "Identifier of property in data");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "active_layer", 0, 0, INT_MAX, "Active Layer", "", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "template_color_picker", "uiTemplateColorPicker");
  RNA_def_function_ui_description(func, "Item. A color wheel widget to pick colors.");
  api_ui_item_rna_common(func);
  RNA_def_boolean(
      func, "value_slider", false, "", "Display the value slider to the right of the color wheel");
  RNA_def_boolean(func,
                  "lock",
                  false,
                  "",
                  "Lock the color wheel display to value 1.0 regardless of actual color");
  RNA_def_boolean(
      func, "lock_luminosity", false, "", "Keep the color at its original vector length");
  RNA_def_boolean(func, "cubic", false, "", "Cubic saturation for picking values close to white");

  func = RNA_def_function(srna, "template_palette", "uiTemplatePalette");
  RNA_def_function_ui_description(func, "Item. A palette used to pick colors.");
  api_ui_item_rna_common(func);
  RNA_def_boolean(func, "color", false, "", "Display the colors as colors or values");

  func = RNA_def_function(srna, "template_image_layers", "uiTemplateImageLayers");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "image", "Image", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "image_user", "ImageUser", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "template_image", "uiTemplateImage");
  RNA_def_function_ui_description(
      func, "Item(s). User interface for selecting images and their source paths.");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  api_ui_item_rna_common(func);
  parm = RNA_def_pointer(func, "image_user", "ImageUser", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_boolean(func, "compact", false, "", "Use more compact layout");
  RNA_def_boolean(func, "multiview", false, "", "Expose Multi-View options");

  func = RNA_def_function(srna, "template_image_settings", "uiTemplateImageSettings");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "User interface for setting image format options");
  parm = RNA_def_pointer(func, "image_settings", "ImageFormatSettings", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_boolean(func, "color_management", false, "", "Show color management settings");

  func = RNA_def_function(srna, "template_image_stereo_3d", "uiTemplateImageStereo3d");
  RNA_def_function_ui_description(func, "User interface for setting image stereo 3d options");
  parm = RNA_def_pointer(func, "stereo_3d_format", "Stereo3dFormat", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);

  func = RNA_def_function(srna, "template_image_views", "uiTemplateImageViews");
  RNA_def_function_ui_description(func, "User interface for setting image views output options");
  parm = RNA_def_pointer(func, "image_settings", "ImageFormatSettings", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);

  func = RNA_def_function(srna, "template_movieclip", "uiTemplateMovieClip");
  RNA_def_function_ui_description(
      func, "Item(s). User interface for selecting movie clips and their source paths.");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  api_ui_item_rna_common(func);
  RNA_def_boolean(func, "compact", false, "", "Use more compact layout");

  func = RNA_def_function(srna, "template_track", "uiTemplateTrack");
  RNA_def_function_ui_description(func, "Item. A movie-track widget to preview tracking image.");
  api_ui_item_rna_common(func);

  func = RNA_def_function(srna, "template_marker", "uiTemplateMarker");
  RNA_def_function_ui_description(func, "Item. A widget to control single marker settings.");
  api_ui_item_rna_common(func);
  parm = RNA_def_pointer(func, "clip_user", "MovieClipUser", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_pointer(func, "track", "MovieTrackingTrack", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_boolean(func, "compact", false, "", "Use more compact layout");

  func = RNA_def_function(
      srna, "template_movieclip_information", "uiTemplateMovieclipInformation");
  RNA_def_function_ui_description(func, "Item. Movie clip information data.");
  api_ui_item_rna_common(func);
  parm = RNA_def_pointer(func, "clip_user", "MovieClipUser", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);

  func = RNA_def_function(srna, "template_list", "rna_uiTemplateList");
  RNA_def_function_ui_description(func, "Item. A list widget to display data, e.g. vertexgroups.");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_string(
      func, "listtype_name", nullptr, 0, "", "Identifier of the list type to use");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(
      func,
      "list_id",
      nullptr,
      0,
      "",
      "Identifier of this list widget. Necessary to tell apart different list widgets. Mandatory "
      "when using default \"" UI_UL_DEFAULT_CLASS_NAME
      "\" class. "
      "If this not an empty string, the uilist gets a custom ID, otherwise it takes the "
      "name of the class used to define the uilist (for example, if the "
      "class name is \"OBJECT_UL_vgroups\", and list_id is not set by the "
      "script, then bl_idname = \"OBJECT_UL_vgroups\")");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "dataptr", "AnyType", "", "Data from which to take the Collection property");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_string(
      func, "propname", nullptr, 0, "", "Identifier of the Collection property in data");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func,
                         "active_dataptr",
                         "AnyType",
                         "",
                         "Data from which to take the integer property, index of the active item");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_string(
      func,
      "active_propname",
      nullptr,
      0,
      "",
      "Identifier of the integer property in active_data, index of the active item");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_string(func,
                 "item_dyntip_propname",
                 nullptr,
                 0,
                 "",
                 "Identifier of a string property in items, to use as tooltip content");
  RNA_def_int(func,
              "rows",
              5,
              0,
              INT_MAX,
              "",
              "Default and minimum number of rows to display",
              0,
              INT_MAX);
  RNA_def_int(
      func, "maxrows", 5, 0, INT_MAX, "", "Default maximum number of rows to display", 0, INT_MAX);
  RNA_def_enum(func,
               "type",
               rna_enum_uilist_layout_type_items,
               UILST_LAYOUT_DEFAULT,
               "Type",
               "Type of layout to use");
  RNA_def_int(func,
              "columns",
              9,
              0,
              INT_MAX,
              "",
              "Number of items to display per row, for GRID layout",
              0,
              INT_MAX);
  RNA_def_boolean(func, "sort_reverse", false, "", "Display items in reverse order by default");
  RNA_def_boolean(func, "sort_lock", false, "", "Lock display order to default value");

  func = RNA_def_function(srna, "template_running_jobs", "uiTemplateRunningJobs");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);

  RNA_def_function(srna, "template_operator_search", "uiTemplateOperatorSearch");
  RNA_def_function(srna, "template_menu_search", "uiTemplateMenuSearch");

  func = RNA_def_function(srna, "template_header_3D_mode", "uiTemplateHeader3D_mode");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "");

  func = RNA_def_function(srna, "template_edit_mode_selection", "uiTemplateEditModeSelection");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(
      func, "Inserts common 3DView Edit modes header UI (selector for selection mode)");

  func = RNA_def_function(srna, "template_reports_banner", "uiTemplateReportsBanner");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);

  func = RNA_def_function(srna, "template_input_status", "uiTemplateInputStatus");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);

  func = RNA_def_function(srna, "template_status_info", "uiTemplateStatusInfo");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);

  func = RNA_def_function(srna, "template_node_link", "uiTemplateNodeLink");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "ntree", "NodeTree", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "node", "Node", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "socket", "NodeSocket", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "template_node_view", "uiTemplateNodeView");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "ntree", "NodeTree", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "node", "Node", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "socket", "NodeSocket", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(
      srna, "template_node_asset_menu_items", "rna_uiLayout_template_node_asset_menu_items");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_string(func, "catalog_path", nullptr, 0, "", "");
  parm = RNA_def_enum(func,
                      "operator",
                      rna_enum_template_node_operator_type,
                      int(NodeAssetMenuOperatorType::Add),
                      "Operator",
                      "The operator the asset menu will use");

  func = RNA_def_function(srna,
                          "template_modifier_asset_menu_items",
                          "rna_uiLayout_template_modifier_asset_menu_items");
  parm = RNA_def_string(func, "catalog_path", nullptr, 0, "", "");
  parm = RNA_def_boolean(func, "skip_essentials", false, "", "");

  func = RNA_def_function(srna,
                          "template_node_operator_asset_menu_items",
                          "rna_uiLayout_template_node_operator_asset_menu_items");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_string(func, "catalog_path", nullptr, 0, "", "");

  func = RNA_def_function(srna,
                          "template_node_operator_asset_root_items",
                          "rna_uiLayout_template_node_operator_root_items");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);

  func = RNA_def_function(srna, "template_texture_user", "uiTemplateTextureUser");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);

  func = RNA_def_function(
      srna, "template_keymap_item_properties", "uiTemplateKeymapItemProperties");
  parm = RNA_def_pointer(func, "item", "KeyMapItem", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);

  func = RNA_def_function(srna, "template_component_menu", "uiTemplateComponentMenu");
  RNA_def_function_ui_description(func, "Item. Display expanded property in a popup menu");
  parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_string(func, "property", nullptr, 0, "", "Identifier of property in data");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_string(func, "name", nullptr, 0, "", "");

  /* color management templates */
  func = RNA_def_function(srna, "template_colorspace_settings", "uiTemplateColorspaceSettings");
  RNA_def_function_ui_description(func, "Item. A widget to control input color space settings.");
  api_ui_item_rna_common(func);

  func = RNA_def_function(
      srna, "template_colormanaged_view_settings", "uiTemplateColormanagedViewSettings");
  RNA_def_function_ui_description(func, "Item. A widget to control color managed view settings.");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  api_ui_item_rna_common(func);
#  if 0
  RNA_def_boolean(func,
                  "show_global_settings",
                  false,
                  "",
                  "Show widgets to control global color management settings");
#  endif

  /* node socket icon */
  func = RNA_def_function(srna, "template_node_socket", "uiTemplateNodeSocket");
  RNA_def_function_ui_description(func, "Node Socket Icon");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_float_array(
      func, "color", 4, node_socket_color_default, 0.0f, 1.0f, "Color", "", 0.0f, 1.0f);

  func = RNA_def_function(srna, "template_cache_file", "rna_uiTemplateCacheFile");
  RNA_def_function_ui_description(
      func, "Item(s). User interface for selecting cache files and their source paths");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  api_ui_item_rna_common(func);

  func = RNA_def_function(srna, "template_cache_file_velocity", "rna_uiTemplateCacheFileVelocity");
  RNA_def_function_ui_description(func, "Show cache files velocity properties");
  api_ui_item_rna_common(func);

  func = RNA_def_function(
      srna, "template_cache_file_time_settings", "rna_uiTemplateCacheFileTimeSettings");
  RNA_def_function_ui_description(func, "Show cache files time settings");
  api_ui_item_rna_common(func);

  func = RNA_def_function(srna, "template_cache_file_layers", "rna_uiTemplateCacheFileLayers");
  RNA_def_function_ui_description(func, "Show cache files override layers properties");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  api_ui_item_rna_common(func);

  func = RNA_def_function(srna, "template_recent_files", "uiTemplateRecentFiles");
  RNA_def_function_ui_description(func, "Show list of recently saved .blend files");
  RNA_def_int(func, "rows", 6, 1, INT_MAX, "", "Maximum number of items to show", 1, INT_MAX);
  parm = RNA_def_int(func, "found", 0, 0, INT_MAX, "", "Number of items drawn", 0, INT_MAX);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "template_file_select_path", "uiTemplateFileSelectPath");
  RNA_def_function_ui_description(func,
                                  "Item. A text button to set the active file browser path.");
  parm = RNA_def_pointer(func, "params", "FileSelectParams", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);

  func = RNA_def_function(
      srna, "template_event_from_keymap_item", "rna_uiTemplateEventFromKeymapItem");
  RNA_def_function_ui_description(func, "Display keymap item as icons/text");
  parm = RNA_def_property(func, "item", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(parm, "KeyMapItem");
  RNA_def_property_ui_text(parm, "Item", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  api_ui_item_common_text(func);

  func = RNA_def_function(
      srna, "template_light_linking_collection", "uiTemplateLightLinkingCollection");
  RNA_def_function_ui_description(func,
                                  "Visualization of a content of a light linking collection");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func,
                         "context_layout",
                         "UILayout",
                         "",
                         "Layout to set active list element as context properties");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  api_ui_item_rna_common(func);

  func = RNA_def_function(srna, "template_bone_collection_tree", "uiTemplateBoneCollectionTree");
  RNA_def_function_ui_description(func, "Show bone collections tree");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);

  func = RNA_def_function(
      srna, "template_grease_pencil_layer_tree", "uiTemplateGreasePencilLayerTree");
  RNA_def_function_ui_description(func, "View of the active Grease Pencil layer tree");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);

  func = RNA_def_function(srna, "template_node_tree_interface", "uiTemplateNodeTreeInterface");
  RNA_def_function_ui_description(func, "Show a node tree interface");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func,
                         "interface",
                         "NodeTreeInterface",
                         "Node Tree Interface",
                         "Interface of a node tree to display");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);

  func = RNA_def_function(srna, "template_node_inputs", "uiTemplateNodeInputs");
  RNA_def_function_ui_description(func, "Show a node settings and input socket values");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "node", "Node", "Node", "Display inputs of this node");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);

  func = RNA_def_function(srna, "template_asset_shelf_popover", "rna_uiTemplateAssetShelfPopover");
  RNA_def_function_ui_description(func, "Create a button to open an asset shelf in a popover");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_string(func,
                        "asset_shelf",
                        nullptr,
                        0,
                        "",
                        "Identifier of the asset shelf to display (``bl_idname``)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(
      func, "name", nullptr, 0, "", "Optional name to indicate the active asset");
  RNA_def_property_clear_flag(parm, PROP_NEVER_NULL);
  parm = RNA_def_property(func, "icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(parm, rna_enum_icon_items);
  RNA_def_property_ui_text(parm, "Icon", "Override automatic icon of the item");
  parm = RNA_def_property(func, "icon_value", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(parm, "Icon Value", "Override automatic icon of the item");

  /* A version of `operator` that defines a [Cancel, Confirm] pair of buttons. */
  func = RNA_def_function(srna, "template_popup_confirm", "rna_uiTemplatePopupConfirm");
  api_ui_item_op_common(func);
  parm = RNA_def_string(func,
                        "cancel_text",
                        nullptr,
                        0,
                        "",
                        "Optional text to use for the cancel, not shown when an empty string");
  RNA_def_boolean(func, "cancel_default", false, "", "Cancel button by default");
  RNA_def_function_ui_description(
      func, "Add confirm & cancel buttons into a popup which will close the popup when pressed");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);

  parm = RNA_def_pointer(
      func, "properties", "OperatorProperties", "", "Operator properties to fill in");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(
      srna, "template_shape_key_tree", "blender::ed::object::shapekey::template_tree");
  RNA_def_function_ui_description(func, "Shape Key tree view");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
}

#endif
