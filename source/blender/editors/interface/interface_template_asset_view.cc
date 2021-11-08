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

#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BKE_screen.h"

#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "BLO_readfile.h"

#include "ED_asset.h"
#include "ED_screen.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

struct AssetViewListData {
  AssetLibraryReference asset_library_ref;
  bScreen *screen;
  bool show_names;
};

static void asset_view_item_but_drag_set(uiBut *but,
                                         AssetViewListData *list_data,
                                         AssetHandle *asset_handle)
{
  ID *id = ED_asset_handle_get_local_id(asset_handle);
  if (id != nullptr) {
    UI_but_drag_set_id(but, id);
    return;
  }

  char blend_path[FILE_MAX_LIBEXTRA];
  /* Context can be null here, it's only needed for a File Browser specific hack that should go
   * away before too long. */
  ED_asset_handle_get_full_library_path(
      nullptr, &list_data->asset_library_ref, asset_handle, blend_path);

  if (blend_path[0]) {
    ImBuf *imbuf = ED_assetlist_asset_image_get(asset_handle);
    UI_but_drag_set_asset(but,
                          asset_handle,
                          BLI_strdup(blend_path),
                          ED_asset_handle_get_metadata(asset_handle),
                          FILE_ASSET_IMPORT_APPEND,
                          ED_asset_handle_get_preview_icon_id(asset_handle),
                          imbuf,
                          1.0f);
  }
}

static void asset_view_draw_item(uiList *ui_list,
                                 bContext *UNUSED(C),
                                 uiLayout *layout,
                                 PointerRNA *UNUSED(dataptr),
                                 PointerRNA *itemptr,
                                 int UNUSED(icon),
                                 PointerRNA *UNUSED(active_dataptr),
                                 const char *UNUSED(active_propname),
                                 int UNUSED(index),
                                 int UNUSED(flt_flag))
{
  AssetViewListData *list_data = (AssetViewListData *)ui_list->dyn_data->customdata;

  BLI_assert(RNA_struct_is_a(itemptr->type, &RNA_AssetHandle));
  AssetHandle *asset_handle = (AssetHandle *)itemptr->data;

  uiLayoutSetContextPointer(layout, "asset_handle", itemptr);

  uiBlock *block = uiLayoutGetBlock(layout);
  const bool show_names = list_data->show_names;
  /* TODO ED_fileselect_init_layout(). Share somehow? */
  const float size_x = (96.0f / 20.0f) * UI_UNIT_X;
  const float size_y = (96.0f / 20.0f) * UI_UNIT_Y - (show_names ? 0 : UI_UNIT_Y);
  uiBut *but = uiDefIconTextBut(block,
                                UI_BTYPE_PREVIEW_TILE,
                                0,
                                ED_asset_handle_get_preview_icon_id(asset_handle),
                                show_names ? ED_asset_handle_get_name(asset_handle) : "",
                                0,
                                0,
                                size_x,
                                size_y,
                                nullptr,
                                0,
                                0,
                                0,
                                0,
                                "");
  ui_def_but_icon(but,
                  ED_asset_handle_get_preview_icon_id(asset_handle),
                  /* NOLINTNEXTLINE: bugprone-suspicious-enum-usage */
                  UI_HAS_ICON | UI_BUT_ICON_PREVIEW);
  if (!ui_list->dyn_data->custom_drag_optype) {
    asset_view_item_but_drag_set(but, list_data, asset_handle);
  }
}

static void asset_view_listener(uiList *ui_list, wmRegionListenerParams *params)
{
  AssetViewListData *list_data = (AssetViewListData *)ui_list->dyn_data->customdata;
  const wmNotifier *notifier = params->notifier;

  switch (notifier->category) {
    case NC_ID: {
      if (ELEM(notifier->action, NA_RENAME)) {
        ED_assetlist_storage_tag_main_data_dirty();
      }
      break;
    }
  }

  if (ED_assetlist_listen(&list_data->asset_library_ref, params->notifier)) {
    ED_region_tag_redraw(params->region);
  }
}

uiListType *UI_UL_asset_view()
{
  uiListType *list_type = (uiListType *)MEM_callocN(sizeof(*list_type), __func__);

  BLI_strncpy(list_type->idname, "UI_UL_asset_view", sizeof(list_type->idname));
  list_type->draw_item = asset_view_draw_item;
  list_type->listener = asset_view_listener;

  return list_type;
}

static void asset_view_template_refresh_asset_collection(
    const AssetLibraryReference &asset_library_ref,
    const AssetFilterSettings &filter_settings,
    PointerRNA &assets_dataptr,
    const char *assets_propname)
{
  PropertyRNA *assets_prop = RNA_struct_find_property(&assets_dataptr, assets_propname);
  if (!assets_prop) {
    RNA_warning("Asset collection not found");
    return;
  }
  if (RNA_property_type(assets_prop) != PROP_COLLECTION) {
    RNA_warning("Expected a collection property");
    return;
  }
  if (!RNA_struct_is_a(RNA_property_pointer_type(&assets_dataptr, assets_prop),
                       &RNA_AssetHandle)) {
    RNA_warning("Expected a collection property for AssetHandle items");
    return;
  }

  RNA_property_collection_clear(&assets_dataptr, assets_prop);

  ED_assetlist_iterate(asset_library_ref, [&](AssetHandle asset) {
    if (!ED_asset_filter_matches_asset(&filter_settings, &asset)) {
      /* Don't do anything else, but return true to continue iterating. */
      return true;
    }

    PointerRNA itemptr, fileptr;
    RNA_property_collection_add(&assets_dataptr, assets_prop, &itemptr);

    RNA_pointer_create(
        nullptr, &RNA_FileSelectEntry, const_cast<FileDirEntry *>(asset.file_data), &fileptr);
    RNA_pointer_set(&itemptr, "file_data", fileptr);

    return true;
  });
}

void uiTemplateAssetView(uiLayout *layout,
                         bContext *C,
                         const char *list_id,
                         PointerRNA *asset_library_dataptr,
                         const char *asset_library_propname,
                         PointerRNA *assets_dataptr,
                         const char *assets_propname,
                         PointerRNA *active_dataptr,
                         const char *active_propname,
                         const AssetFilterSettings *filter_settings,
                         const int display_flags,
                         const char *activate_opname,
                         PointerRNA *r_activate_op_properties,
                         const char *drag_opname,
                         PointerRNA *r_drag_op_properties)
{
  if (!list_id || !list_id[0]) {
    RNA_warning("Asset view needs a valid identifier");
    return;
  }

  uiLayout *col = uiLayoutColumn(layout, false);

  PropertyRNA *asset_library_prop = RNA_struct_find_property(asset_library_dataptr,
                                                             asset_library_propname);
  AssetLibraryReference asset_library_ref = ED_asset_library_reference_from_enum_value(
      RNA_property_enum_get(asset_library_dataptr, asset_library_prop));

  uiLayout *row = uiLayoutRow(col, true);
  if ((display_flags & UI_TEMPLATE_ASSET_DRAW_NO_LIBRARY) == 0) {
    uiItemFullR(row, asset_library_dataptr, asset_library_prop, RNA_NO_INDEX, 0, 0, "", 0);
    if (asset_library_ref.type != ASSET_LIBRARY_LOCAL) {
      uiItemO(row, "", ICON_FILE_REFRESH, "ASSET_OT_list_refresh");
    }
  }

  ED_assetlist_storage_fetch(&asset_library_ref, C);
  ED_assetlist_ensure_previews_job(&asset_library_ref, C);
  const int tot_items = ED_assetlist_size(&asset_library_ref);

  asset_view_template_refresh_asset_collection(
      asset_library_ref, *filter_settings, *assets_dataptr, assets_propname);

  AssetViewListData *list_data = (AssetViewListData *)MEM_mallocN(sizeof(*list_data),
                                                                  "AssetViewListData");
  list_data->asset_library_ref = asset_library_ref;
  list_data->screen = CTX_wm_screen(C);
  list_data->show_names = (display_flags & UI_TEMPLATE_ASSET_DRAW_NO_NAMES) == 0;

  uiTemplateListFlags template_list_flags = UI_TEMPLATE_LIST_NO_GRIP;
  if ((display_flags & UI_TEMPLATE_ASSET_DRAW_NO_NAMES) != 0) {
    template_list_flags |= UI_TEMPLATE_LIST_NO_NAMES;
  }
  if ((display_flags & UI_TEMPLATE_ASSET_DRAW_NO_FILTER) != 0) {
    template_list_flags |= UI_TEMPLATE_LIST_NO_FILTER_OPTIONS;
  }

  /* TODO can we have some kind of model-view API to handle referencing, filtering and lazy loading
   * (of previews) of the items? */
  uiList *list = uiTemplateList_ex(col,
                                   C,
                                   "UI_UL_asset_view",
                                   list_id,
                                   assets_dataptr,
                                   assets_propname,
                                   active_dataptr,
                                   active_propname,
                                   nullptr,
                                   tot_items,
                                   0,
                                   UILST_LAYOUT_BIG_PREVIEW_GRID,
                                   0,
                                   template_list_flags,
                                   list_data);
  if (!list) {
    /* List creation failed. */
    MEM_freeN(list_data);
    return;
  }

  if (activate_opname) {
    PointerRNA *ptr = UI_list_custom_activate_operator_set(
        list, activate_opname, r_activate_op_properties != nullptr);
    if (r_activate_op_properties && ptr) {
      *r_activate_op_properties = *ptr;
    }
  }
  if (drag_opname) {
    PointerRNA *ptr = UI_list_custom_drag_operator_set(
        list, drag_opname, r_drag_op_properties != nullptr);
    if (r_drag_op_properties && ptr) {
      *r_drag_op_properties = *ptr;
    }
  }
}
