/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <optional>

#include "AS_asset_catalog.hh"
#include "AS_asset_library.hh"

#include "BLI_listbase.h"
#include "BLI_string_search.h"

#include "DNA_space_types.h"

#include "BKE_asset.h"
#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_node_tree_update.h"
#include "BKE_screen.h"

#include "DEG_depsgraph_build.h"

#include "BLT_translation.h"

#include "RNA_access.h"

#include "WM_api.h"

#include "ED_asset.h"
#include "ED_node.h"

#include "node_intern.hh"

struct bContext;

namespace blender::ed::space_node {

struct AddNodeItem {
  std::string ui_name;
  std::string identifier;
  std::string description;
  std::optional<AssetHandle> asset;
  std::function<void(const bContext &, bNodeTree &, bNode &)> after_add_fn;
  int weight = 0;
};

struct AddNodeSearchStorage {
  float2 cursor;
  bool use_transform;
  Vector<AddNodeItem> search_add_items;
  char search[256];
  bool update_items_tag = true;
};

static void add_node_search_listen_fn(const wmRegionListenerParams *params, void *arg)
{
  AddNodeSearchStorage &storage = *static_cast<AddNodeSearchStorage *>(arg);
  const wmNotifier *wmn = params->notifier;

  switch (wmn->category) {
    case NC_ASSET:
      if (wmn->data == ND_ASSET_LIST_READING) {
        storage.update_items_tag = true;
      }
      break;
  }
}

static void search_items_for_asset_metadata(const bNodeTree &node_tree,
                                            const AssetHandle asset,
                                            Vector<AddNodeItem> &search_items)
{
  const AssetMetaData &asset_data = *ED_asset_handle_get_metadata(&asset);
  const IDProperty *tree_type = BKE_asset_metadata_idprop_find(&asset_data, "type");
  if (tree_type == nullptr || IDP_Int(tree_type) != node_tree.type) {
    return;
  }

  AddNodeItem item{};
  item.ui_name = ED_asset_handle_get_name(&asset);
  item.identifier = node_tree.typeinfo->group_idname;
  item.description = asset_data.description == nullptr ? "" : asset_data.description;
  item.asset = asset;
  item.after_add_fn = [asset](const bContext &C, bNodeTree &node_tree, bNode &node) {
    Main &bmain = *CTX_data_main(&C);
    node.flag &= ~NODE_OPTIONS;
    node.id = asset::get_local_id_from_asset_or_append_and_reuse(bmain, asset);
    id_us_plus(node.id);
    BKE_ntree_update_tag_node_property(&node_tree, &node);
    DEG_relations_tag_update(&bmain);
  };

  search_items.append(std::move(item));
}

static void gather_search_items_for_asset_library(const bContext &C,
                                                  const bNodeTree &node_tree,
                                                  const AssetLibraryReference &library_ref,
                                                  Set<std::string> &r_added_assets,
                                                  Vector<AddNodeItem> &search_items)
{
  AssetFilterSettings filter_settings{};
  filter_settings.id_types = FILTER_ID_NT;

  ED_assetlist_storage_fetch(&library_ref, &C);
  ED_assetlist_ensure_previews_job(&library_ref, &C);
  ED_assetlist_iterate(library_ref, [&](AssetHandle asset) {
    if (!ED_asset_filter_matches_asset(&filter_settings, &asset)) {
      return true;
    }
    if (!r_added_assets.add(ED_asset_handle_get_name(&asset))) {
      /* If an asset with the same name has already been added, skip this. */
      return true;
    }
    search_items_for_asset_metadata(node_tree, asset, search_items);
    return true;
  });
}

static void gather_search_items_for_all_assets(const bContext &C,
                                               const bNodeTree &node_tree,
                                               Set<std::string> &r_added_assets,
                                               Vector<AddNodeItem> &search_items)
{
  int i;
  LISTBASE_FOREACH_INDEX (const bUserAssetLibrary *, asset_library, &U.asset_libraries, i) {
    AssetLibraryReference library_ref{};
    library_ref.custom_library_index = i;
    library_ref.type = ASSET_LIBRARY_CUSTOM;
    /* Skip local assets to avoid duplicates when the asset is part of the local file library. */
    gather_search_items_for_asset_library(C, node_tree, library_ref, r_added_assets, search_items);
  }

  AssetLibraryReference library_ref{};
  library_ref.custom_library_index = -1;
  library_ref.type = ASSET_LIBRARY_LOCAL;
  gather_search_items_for_asset_library(C, node_tree, library_ref, r_added_assets, search_items);
}

static void gather_search_items_for_node_groups(const bContext &C,
                                                const bNodeTree &node_tree,
                                                const Set<std::string> &local_assets,
                                                Vector<AddNodeItem> &search_items)
{
  const StringRef group_node_id = node_tree.typeinfo->group_idname;

  Main &bmain = *CTX_data_main(&C);
  LISTBASE_FOREACH (bNodeTree *, node_group, &bmain.nodetrees) {
    if (node_group->typeinfo->group_idname != group_node_id) {
      continue;
    }
    if (local_assets.contains(node_group->id.name)) {
      continue;
    }
    if (!nodeGroupPoll(&node_tree, node_group, nullptr)) {
      continue;
    }
    AddNodeItem item{};
    item.ui_name = node_group->id.name + 2;
    item.identifier = node_tree.typeinfo->group_idname;
    item.after_add_fn = [node_group](const bContext &C, bNodeTree &node_tree, bNode &node) {
      Main &bmain = *CTX_data_main(&C);
      node.id = &node_group->id;
      id_us_plus(node.id);
      BKE_ntree_update_tag_node_property(&node_tree, &node);
      DEG_relations_tag_update(&bmain);
    };
    search_items.append(std::move(item));
  }
}

static void gather_add_node_operations(const bContext &C,
                                       bNodeTree &node_tree,
                                       Vector<AddNodeItem> &r_search_items)
{
  NODE_TYPES_BEGIN (node_type) {
    const char *disabled_hint;
    if (!(node_type->poll && node_type->poll(node_type, &node_tree, &disabled_hint))) {
      continue;
    }
    if (StringRefNull(node_tree.typeinfo->group_idname) == node_type->idname) {
      /* Skip the empty group type. */
      continue;
    }
    if (StringRefNull(node_type->ui_name).endswith("(Legacy)")) {
      continue;
    }

    AddNodeItem item{};
    item.ui_name = IFACE_(node_type->ui_name);
    item.identifier = node_type->idname;
    item.description = TIP_(node_type->ui_description);
    r_search_items.append(std::move(item));
  }
  NODE_TYPES_END;

  /* Use a set to avoid adding items for node groups that are also assets. Using data-block
   * names is a crutch, since different assets may have the same name. However, an alternative
   * using #ED_asset_handle_get_local_id didn't work in this case. */
  Set<std::string> added_assets;
  gather_search_items_for_all_assets(C, node_tree, added_assets, r_search_items);
  gather_search_items_for_node_groups(C, node_tree, added_assets, r_search_items);
}

static void add_node_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, const bool is_first)
{
  AddNodeSearchStorage &storage = *static_cast<AddNodeSearchStorage *>(arg);
  if (storage.update_items_tag) {
    bNodeTree *node_tree = CTX_wm_space_node(C)->edittree;
    storage.search_add_items.clear();
    gather_add_node_operations(*C, *node_tree, storage.search_add_items);
    storage.update_items_tag = false;
  }

  StringSearch *search = BLI_string_search_new();

  for (AddNodeItem &item : storage.search_add_items) {
    BLI_string_search_add(search, item.ui_name.c_str(), &item, item.weight);
  }

  /* Don't filter when the menu is first opened, but still run the search
   * so the items are in the same order they will appear in while searching. */
  const char *string = is_first ? "" : str;
  AddNodeItem **filtered_items;
  const int filtered_amount = BLI_string_search_query(search, string, (void ***)&filtered_items);

  for (const int i : IndexRange(filtered_amount)) {
    AddNodeItem &item = *filtered_items[i];
    if (!UI_search_item_add(items, item.ui_name.c_str(), &item, ICON_NONE, 0, 0)) {
      break;
    }
  }

  MEM_freeN(filtered_items);
  BLI_string_search_free(search);
}

static void add_node_search_exec_fn(bContext *C, void *arg1, void *arg2)
{
  Main &bmain = *CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &node_tree = *snode.edittree;
  AddNodeSearchStorage &storage = *static_cast<AddNodeSearchStorage *>(arg1);
  AddNodeItem *item = static_cast<AddNodeItem *>(arg2);
  if (item == nullptr) {
    return;
  }

  node_deselect_all(node_tree);
  bNode *new_node = nodeAddNode(C, &node_tree, item->identifier.c_str());
  BLI_assert(new_node != nullptr);

  if (item->after_add_fn) {
    item->after_add_fn(*C, node_tree, *new_node);
  }

  new_node->locx = storage.cursor.x / UI_DPI_FAC;
  new_node->locy = storage.cursor.y / UI_DPI_FAC + 20;

  nodeSetSelected(new_node, true);
  nodeSetActive(&node_tree, new_node);

  /* Ideally it would be possible to tag the node tree in some way so it updates only after the
   * translate operation is finished, but normally moving nodes around doesn't cause updates. */
  ED_node_tree_propagate_change(C, &bmain, &node_tree);

  if (storage.use_transform) {
    wmOperatorType *ot = WM_operatortype_find("NODE_OT_translate_attach_remove_on_cancel", true);
    BLI_assert(ot);
    PointerRNA ptr;
    WM_operator_properties_create_ptr(&ptr, ot);
    WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &ptr, nullptr);
    WM_operator_properties_free(&ptr);
  }
}

static ARegion *add_node_search_tooltip_fn(
    bContext *C, ARegion *region, const rcti *item_rect, void * /*arg*/, void *active)
{
  const AddNodeItem *item = static_cast<const AddNodeItem *>(active);

  uiSearchItemTooltipData tooltip_data{};

  BLI_strncpy(tooltip_data.description,
              item->asset ? item->description.c_str() : TIP_(item->description.c_str()),
              sizeof(tooltip_data.description));

  return UI_tooltip_create_from_search_item_generic(C, region, item_rect, &tooltip_data);
}

static void add_node_search_free_fn(void *arg)
{
  AddNodeSearchStorage *storage = static_cast<AddNodeSearchStorage *>(arg);
  delete storage;
}

static uiBlock *create_search_popup_block(bContext *C, ARegion *region, void *arg_op)
{
  AddNodeSearchStorage &storage = *(AddNodeSearchStorage *)arg_op;

  uiBlock *block = UI_block_begin(C, region, "_popup", UI_EMBOSS);
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_SEARCH_MENU);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  uiBut *but = uiDefSearchBut(block,
                              storage.search,
                              0,
                              ICON_VIEWZOOM,
                              sizeof(storage.search),
                              10,
                              10,
                              UI_searchbox_size_x(),
                              UI_UNIT_Y,
                              0,
                              0,
                              "");
  UI_but_func_search_set_sep_string(but, UI_MENU_ARROW_SEP);
  UI_but_func_search_set(but,
                         nullptr,
                         add_node_search_update_fn,
                         &storage,
                         false,
                         add_node_search_free_fn,
                         add_node_search_exec_fn,
                         nullptr);
  UI_but_flag_enable(but, UI_BUT_ACTIVATE_ON_INIT);
  UI_but_func_search_set_tooltip(but, add_node_search_tooltip_fn);
  UI_but_func_search_set_listen(but, add_node_search_listen_fn);

  /* Fake button to hold space for the search items. */
  uiDefBut(block,
           UI_BTYPE_LABEL,
           0,
           "",
           10,
           10 - UI_searchbox_size_y(),
           UI_searchbox_size_x(),
           UI_searchbox_size_y(),
           nullptr,
           0,
           0,
           0,
           0,
           nullptr);

  const int offset[2] = {0, -UI_UNIT_Y};
  UI_block_bounds_set_popup(block, 0.3f * U.widget_unit, offset);
  return block;
}

void invoke_add_node_search_menu(bContext &C, const float2 &cursor, const bool use_transform)
{
  AddNodeSearchStorage *storage = new AddNodeSearchStorage{cursor, use_transform};
  /* Use the "_ex" variant with `can_refresh` false to avoid a double free when closing Blender. */
  UI_popup_block_invoke_ex(&C, create_search_popup_block, storage, nullptr, false);
}

}  // namespace blender::ed::space_node
