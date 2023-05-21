/* SPDX-License-Identifier: GPL-2.0-or-later */

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
#include "RNA_prototypes.h"

#include "UI_interface.h"
#include "UI_interface.hh"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.hh"

struct AssetViewListData {
  AssetLibraryReference asset_library_ref;
  AssetFilterSettings filter_settings;
  bScreen *screen;
  bool show_names;
};

static void asset_view_item_but_drag_set(uiBut *but, AssetHandle *asset_handle)
{
  ID *id = ED_asset_handle_get_local_id(asset_handle);
  if (id != nullptr) {
    UI_but_drag_set_id(but, id);
    return;
  }

  char blend_path[FILE_MAX_LIBEXTRA];
  ED_asset_handle_get_full_library_path(asset_handle, blend_path);

  const eAssetImportMethod import_method =
      ED_asset_handle_get_import_method(asset_handle).value_or(ASSET_IMPORT_APPEND_REUSE);

  if (blend_path[0]) {
    ImBuf *imbuf = ED_assetlist_asset_image_get(asset_handle);
    UI_but_drag_set_asset(but,
                          asset_handle,
                          BLI_strdup(blend_path),
                          import_method,
                          ED_asset_handle_get_preview_icon_id(asset_handle),
                          imbuf,
                          1.0f);
  }
}

static void asset_view_draw_item(uiList *ui_list,
                                 const bContext * /*C*/,
                                 uiLayout *layout,
                                 PointerRNA * /*dataptr*/,
                                 PointerRNA * /*itemptr*/,
                                 int /*icon*/,
                                 PointerRNA * /*active_dataptr*/,
                                 const char * /*active_propname*/,
                                 int index,
                                 int /*flt_flag*/)
{
  AssetViewListData *list_data = (AssetViewListData *)ui_list->dyn_data->customdata;

  AssetHandle asset_handle = ED_assetlist_asset_get_by_index(&list_data->asset_library_ref, index);

  PointerRNA file_ptr;
  RNA_pointer_create(&list_data->screen->id,
                     &RNA_FileSelectEntry,
                     const_cast<FileDirEntry *>(asset_handle.file_data),
                     &file_ptr);
  uiLayoutSetContextPointer(layout, "active_file", &file_ptr);

  uiBlock *block = uiLayoutGetBlock(layout);
  const bool show_names = list_data->show_names;
  const float size_x = UI_preview_tile_size_x();
  const float size_y = show_names ? UI_preview_tile_size_y() : UI_preview_tile_size_y_no_label();
  uiBut *but = uiDefIconTextBut(block,
                                UI_BTYPE_PREVIEW_TILE,
                                0,
                                ED_asset_handle_get_preview_icon_id(&asset_handle),
                                show_names ? ED_asset_handle_get_name(&asset_handle) : "",
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
                  ED_asset_handle_get_preview_icon_id(&asset_handle),
                  /* NOLINTNEXTLINE: bugprone-suspicious-enum-usage */
                  UI_HAS_ICON | UI_BUT_ICON_PREVIEW);
  but->emboss = UI_EMBOSS_NONE;
  if (!ui_list->dyn_data->custom_drag_optype) {
    asset_view_item_but_drag_set(but, &asset_handle);
  }
}

static void asset_view_filter_items(uiList *ui_list,
                                    const bContext *C,
                                    PointerRNA *dataptr,
                                    const char *propname)
{
  AssetViewListData *list_data = (AssetViewListData *)ui_list->dyn_data->customdata;
  AssetFilterSettings &filter_settings = list_data->filter_settings;

  uiListNameFilter name_filter(*ui_list);

  UI_list_filter_and_sort_items(
      ui_list,
      C,
      [&name_filter, list_data, &filter_settings](
          const PointerRNA &itemptr, blender::StringRefNull name, int index) {
        AssetHandle asset = ED_assetlist_asset_get_by_index(&list_data->asset_library_ref, index);
        if (!ED_asset_filter_matches_asset(&filter_settings, &asset)) {
          return UI_LIST_ITEM_NEVER_SHOW;
        }
        return name_filter(itemptr, name, index);
      },
      dataptr,
      propname,
      [list_data](const PointerRNA & /*itemptr*/, int index) -> std::string {
        AssetHandle asset = ED_assetlist_asset_get_by_index(&list_data->asset_library_ref, index);
        return ED_asset_handle_get_name(&asset);
      });
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

  STRNCPY(list_type->idname, "UI_UL_asset_view");
  list_type->draw_item = asset_view_draw_item;
  list_type->filter_items = asset_view_filter_items;
  list_type->listener = asset_view_listener;

  return list_type;
}

static void populate_asset_collection(const AssetLibraryReference &asset_library_ref,
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
  if (!RNA_struct_is_a(RNA_property_pointer_type(&assets_dataptr, assets_prop), &RNA_AssetHandle))
  {
    RNA_warning("Expected a collection property for AssetHandle items");
    return;
  }

  RNA_property_collection_clear(&assets_dataptr, assets_prop);

  ED_assetlist_iterate(asset_library_ref, [&](AssetHandle /*asset*/) {
    /* XXX creating a dummy #RNA_AssetHandle collection item. It's #file_data will be null. This is
     * because the #FileDirEntry may be freed while iterating, there's a cache for them with a
     * maximum size. Further code will query as needed it using the collection index. */

    PointerRNA itemptr, fileptr;
    RNA_property_collection_add(&assets_dataptr, assets_prop, &itemptr);

    RNA_pointer_create(nullptr, &RNA_FileSelectEntry, nullptr, &fileptr);
    RNA_pointer_set(&itemptr, "file_data", fileptr);

    return true;
  });
}

void uiTemplateAssetView(uiLayout *layout,
                         const bContext *C,
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
      uiItemO(row, "", ICON_FILE_REFRESH, "ASSET_OT_library_refresh");
    }
  }

  ED_assetlist_storage_fetch(&asset_library_ref, C);
  ED_assetlist_ensure_previews_job(&asset_library_ref, C);
  const int tot_items = ED_assetlist_size(&asset_library_ref);

  populate_asset_collection(asset_library_ref, *assets_dataptr, assets_propname);

  AssetViewListData *list_data = (AssetViewListData *)MEM_mallocN(sizeof(*list_data),
                                                                  "AssetViewListData");
  list_data->asset_library_ref = asset_library_ref;
  list_data->filter_settings = *filter_settings;
  list_data->screen = CTX_wm_screen(C);
  list_data->show_names = (display_flags & UI_TEMPLATE_ASSET_DRAW_NO_NAMES) == 0;

  uiTemplateListFlags template_list_flags = UI_TEMPLATE_LIST_NO_GRIP;
  if ((display_flags & UI_TEMPLATE_ASSET_DRAW_NO_NAMES) != 0) {
    template_list_flags |= UI_TEMPLATE_LIST_NO_NAMES;
  }
  if ((display_flags & UI_TEMPLATE_ASSET_DRAW_NO_FILTER) != 0) {
    template_list_flags |= UI_TEMPLATE_LIST_NO_FILTER_OPTIONS;
  }

  uiLayout *subcol = uiLayoutColumn(col, false);

  uiLayoutSetScaleX(subcol, 0.8f);
  uiLayoutSetScaleY(subcol, 0.8f);

  /* TODO can we have some kind of model-view API to handle referencing, filtering and lazy loading
   * (of previews) of the items? */
  uiList *list = uiTemplateList_ex(subcol,
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
