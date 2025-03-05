/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BLI_string_ref.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BLT_translation.hh"

#include "UI_interface.hh"
#include "interface_intern.hh"
#include "interface_templates_intern.hh"

using blender::StringRef;
using blender::StringRefNull;

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

  PointerRNA item_ptr = RNA_pointer_create_discrete(nullptr, type, item);
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
                                                  TemplateSearch &template_search,
                                                  const bool editable,
                                                  const bool live_icon)
{
  const char *ui_description = RNA_property_ui_description(
      template_search.search_data.target_prop);

  template_add_button_search_menu(C,
                                  layout,
                                  block,
                                  &template_search.search_data.target_ptr,
                                  template_search.search_data.target_prop,
                                  template_search_menu,
                                  MEM_new<TemplateSearch>(__func__, template_search),
                                  ui_description,
                                  template_search.use_previews,
                                  editable,
                                  live_icon,
                                  but_func_argN_free<TemplateSearch>,
                                  but_func_argN_copy<TemplateSearch>);
}

static void template_search_add_button_name(uiBlock *block,
                                            PointerRNA *active_ptr,
                                            const StructRNA *type)
{
  /* Skip text button without an active item. */
  if (active_ptr->data == nullptr) {
    return;
  }

  PropertyRNA *name_prop;
  if (type == &RNA_ActionSlot) {
    name_prop = RNA_struct_find_property(active_ptr, "name_display");
  }
  else {
    name_prop = RNA_struct_name_property(type);
  }

  const int width = template_search_textbut_width(active_ptr, name_prop);
  const int height = template_search_textbut_height();
  uiDefAutoButR(block, active_ptr, name_prop, 0, "", ICON_NONE, 0, 0, width, height);
}

static void template_search_add_button_operator(
    uiBlock *block,
    const char *const operator_name,
    const wmOperatorCallContext opcontext,
    const int icon,
    const bool editable,
    const std::optional<StringRefNull> button_text = {})
{
  if (!operator_name) {
    return;
  }

  uiBut *but;
  if (button_text) {
    const int button_width = std::max(
        UI_fontstyle_string_width(UI_FSTYLE_WIDGET, button_text->c_str()) + int(UI_UNIT_X * 1.5f),
        UI_UNIT_X * 5);

    but = uiDefIconTextButO(block,
                            UI_BTYPE_BUT,
                            operator_name,
                            opcontext,
                            icon,
                            *button_text,
                            0,
                            0,
                            button_width,
                            UI_UNIT_Y,
                            nullptr);
  }
  else {
    but = uiDefIconButO(
        block, UI_BTYPE_BUT, operator_name, opcontext, icon, 0, 0, UI_UNIT_X, UI_UNIT_Y, nullptr);
  }

  if (!editable) {
    UI_but_drawflag_enable(but, UI_BUT_DISABLED);
  }
}

static void template_search_buttons(const bContext *C,
                                    uiLayout *layout,
                                    TemplateSearch &template_search,
                                    const char *newop,
                                    const char *unlinkop,
                                    const std::optional<StringRef> text)
{
  uiBlock *block = uiLayoutGetBlock(layout);
  uiRNACollectionSearch *search_data = &template_search.search_data;
  const StructRNA *type = RNA_property_pointer_type(&search_data->target_ptr,
                                                    search_data->target_prop);
  const bool editable = RNA_property_editable(&search_data->target_ptr, search_data->target_prop);
  PointerRNA active_ptr = RNA_property_pointer_get(&search_data->target_ptr,
                                                   search_data->target_prop);

  if (active_ptr.type) {
    /* can only get correct type when there is an active item */
    type = active_ptr.type;
  }

  uiLayout *row = uiLayoutRow(layout, true);
  UI_block_align_begin(block);

  uiLayout *decorator_layout = nullptr;
  if (text && !text->is_empty()) {
    /* Add label respecting the separated layout property split state. */
    decorator_layout = uiItemL_respect_property_split(row, *text, ICON_NONE);
  }

  template_search_add_button_searchmenu(C, row, block, template_search, editable, false);
  template_search_add_button_name(block, &active_ptr, type);

  /* For Blender 4.4, the "New" button is only shown on Action Slot selectors.
   * Blender 4.5 may have this enabled for all uses of this template, in which
   * case this type-specific code will be removed. */
  const bool may_show_new_button = (type == &RNA_ActionSlot);
  if (may_show_new_button && !active_ptr.data) {
    template_search_add_button_operator(
        block, newop, WM_OP_INVOKE_DEFAULT, ICON_ADD, editable, IFACE_("New"));
  }
  else {
    template_search_add_button_operator(
        block, newop, WM_OP_INVOKE_DEFAULT, ICON_DUPLICATE, editable);
    template_search_add_button_operator(
        block, unlinkop, WM_OP_INVOKE_REGION_WIN, ICON_X, editable);
  }

  UI_block_align_end(block);

  if (decorator_layout) {
    uiItemDecoratorR(decorator_layout, nullptr, "", RNA_NO_INDEX);
  }
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
           RNA_property_pointer_type(targetptr, targetprop))
  {
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

static bool template_search_setup(TemplateSearch &template_search,
                                  PointerRNA *ptr,
                                  const StringRefNull propname,
                                  PointerRNA *searchptr,
                                  const char *const searchpropname)
{
  template_search = {};
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());

  if (!prop || RNA_property_type(prop) != PROP_POINTER) {
    RNA_warning(
        "pointer property not found: %s.%s", RNA_struct_identifier(ptr->type), propname.c_str());
    return false;
  }
  PropertyRNA *searchprop = template_search_get_searchprop(ptr, prop, searchptr, searchpropname);

  template_search.search_data.target_ptr = *ptr;
  template_search.search_data.target_prop = prop;
  template_search.search_data.search_ptr = *searchptr;
  template_search.search_data.search_prop = searchprop;

  return true;
}

void uiTemplateSearch(uiLayout *layout,
                      const bContext *C,
                      PointerRNA *ptr,
                      const StringRefNull propname,
                      PointerRNA *searchptr,
                      const char *searchpropname,
                      const char *newop,
                      const char *unlinkop,
                      const std::optional<StringRef> text)
{
  TemplateSearch template_search;
  if (template_search_setup(template_search, ptr, propname, searchptr, searchpropname)) {
    template_search_buttons(C, layout, template_search, newop, unlinkop, text);
  }
}

void uiTemplateSearchPreview(uiLayout *layout,
                             bContext *C,
                             PointerRNA *ptr,
                             const StringRefNull propname,
                             PointerRNA *searchptr,
                             const char *searchpropname,
                             const char *newop,
                             const char *unlinkop,
                             const int rows,
                             const int cols,
                             const std::optional<StringRef> text)
{
  TemplateSearch template_search;
  if (template_search_setup(template_search, ptr, propname, searchptr, searchpropname)) {
    template_search.use_previews = true;
    template_search.preview_rows = rows;
    template_search.preview_cols = cols;

    template_search_buttons(C, layout, template_search, newop, unlinkop, text);
  }
}
