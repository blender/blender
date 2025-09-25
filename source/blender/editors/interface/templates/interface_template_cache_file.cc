/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BLI_listbase.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"

#include "BKE_context.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_cachefile_types.h"
#include "DNA_space_types.h"

#include "DEG_depsgraph_query.hh"

#include "RE_engine.h"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "interface_intern.hh"

using blender::StringRefNull;

void uiTemplateCacheFileVelocity(uiLayout *layout, PointerRNA *fileptr)
{
  if (RNA_pointer_is_null(fileptr)) {
    return;
  }

  /* Ensure that the context has a CacheFile as this may not be set inside of modifiers panels. */
  layout->context_ptr_set("edit_cachefile", fileptr);

  layout->prop(fileptr, "velocity_name", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(fileptr, "velocity_unit", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

void uiTemplateCacheFileTimeSettings(uiLayout *layout, PointerRNA *fileptr)
{
  if (RNA_pointer_is_null(fileptr)) {
    return;
  }

  /* Ensure that the context has a CacheFile as this may not be set inside of modifiers panels. */
  layout->context_ptr_set("edit_cachefile", fileptr);

  uiLayout *row, *sub, *subsub;

  row = &layout->row(false);
  row->prop(fileptr, "is_sequence", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  row = &layout->row(true, IFACE_("Override Frame"));
  sub = &row->row(true);
  sub->use_property_decorate_set(false);
  sub->prop(fileptr, "override_frame", UI_ITEM_NONE, "", ICON_NONE);
  subsub = &sub->row(true);
  subsub->active_set(RNA_boolean_get(fileptr, "override_frame"));
  subsub->prop(fileptr, "frame", UI_ITEM_NONE, "", ICON_NONE);
  row->decorator(fileptr, "frame", 0);

  row = &layout->row(false);
  row->prop(fileptr, "frame_offset", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  row->active_set(!RNA_boolean_get(fileptr, "is_sequence"));
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
  uiLayout *row = &layout->row(true);
  row->prop(itemptr, "hide_layer", UI_ITEM_R_NO_BG, "", ICON_NONE);
  row->prop(itemptr, "filepath", UI_ITEM_R_NO_BG, "", ICON_NONE);
}

uiListType *UI_UL_cache_file_layers()
{
  uiListType *list_type = (uiListType *)MEM_callocN(sizeof(*list_type), __func__);

  STRNCPY_UTF8(list_type->idname, "UI_UL_cache_file_layers");
  list_type->draw_item = cache_file_layer_item;

  return list_type;
}

void uiTemplateCacheFileLayers(uiLayout *layout, const bContext *C, PointerRNA *fileptr)
{
  if (RNA_pointer_is_null(fileptr)) {
    return;
  }

  /* Ensure that the context has a CacheFile as this may not be set inside of modifiers panels. */
  layout->context_ptr_set("edit_cachefile", fileptr);

  uiLayout *row = &layout->row(false);
  uiLayout *col = &row->column(true);

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

  col = &row->column(true);
  col->op("cachefile.layer_add", "", ICON_ADD);
  col->op("cachefile.layer_remove", "", ICON_REMOVE);

  CacheFile *file = static_cast<CacheFile *>(fileptr->data);
  if (BLI_listbase_count(&file->layers) > 1) {
    col->separator(1.0f);
    col->op("cachefile.layer_move", "", ICON_TRIA_UP);
    col->op("cachefile.layer_move", "", ICON_TRIA_DOWN);
  }
}

bool uiTemplateCacheFilePointer(PointerRNA *ptr,
                                const StringRefNull propname,
                                PointerRNA *r_file_ptr)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());

  if (!prop) {
    printf("%s: property not found: %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname.c_str());
    return false;
  }

  if (RNA_property_type(prop) != PROP_POINTER) {
    printf("%s: expected pointer property for %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname.c_str());
    return false;
  }

  *r_file_ptr = RNA_property_pointer_get(ptr, prop);
  return true;
}

void uiTemplateCacheFile(uiLayout *layout,
                         const bContext *C,
                         PointerRNA *ptr,
                         const StringRefNull propname)
{
  if (!ptr->data) {
    return;
  }

  PointerRNA fileptr;
  if (!uiTemplateCacheFilePointer(ptr, propname, &fileptr)) {
    return;
  }

  CacheFile *file = static_cast<CacheFile *>(fileptr.data);

  layout->context_ptr_set("edit_cachefile", &fileptr);

  uiTemplateID(layout, C, ptr, propname, nullptr, "CACHEFILE_OT_open", nullptr);

  if (!file) {
    return;
  }

  SpaceProperties *sbuts = CTX_wm_space_properties(C);

  uiLayout *row, *sub;

  layout->use_property_split_set(true);

  row = &layout->row(true);
  row->prop(&fileptr, "filepath", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  sub = &row->row(true);
  sub->op("cachefile.reload", "", ICON_FILE_REFRESH);

  if (sbuts->mainb == BCONTEXT_CONSTRAINT) {
    row = &layout->row(false);
    row->prop(&fileptr, "scale", UI_ITEM_NONE, IFACE_("Manual Scale"), ICON_NONE);
  }

  /* TODO: unused for now, so no need to expose. */
#if 0
  row = &layout->row(false);
  row->prop(&fileptr, "forward_axis", UI_ITEM_NONE, IFACE_("Forward Axis"), ICON_NONE);

  row = &layout->row(false);
  row->prop(&fileptr, "up_axis", UI_ITEM_NONE, IFACE_("Up Axis"), ICON_NONE);
#endif
}
