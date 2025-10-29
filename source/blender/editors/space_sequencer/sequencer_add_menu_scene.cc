/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "BLI_listbase.h"
#include "BLI_multi_value_map.hh"
#include "BLI_string.h"

#include "DNA_space_types.h"
#include "DNA_workspace_types.h"

#include "BKE_idprop.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"

#include "ED_asset.hh"
#include "ED_asset_menu_utils.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "sequencer_intern.hh"

namespace blender::ed::vse {

static bool sequencer_add_menu_poll(const bContext *C, MenuType * /*mt*/)
{
  /* Add menu is not accessible from the VSE preview. */
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  return sseq && ELEM(sseq->view, SEQ_VIEW_SEQUENCE, SEQ_VIEW_SEQUENCE_PREVIEW);
}

static bool all_loading_finished()
{
  AssetLibraryReference all_library_ref = asset_system::all_library_reference();
  return asset::list::is_loaded(&all_library_ref);
}

static asset::AssetItemTree build_catalog_tree(const bContext &C)
{
  asset::AssetFilterSettings type_filter{};
  type_filter.id_types = FILTER_ID_SCE;
  const AssetLibraryReference library = asset_system::all_library_reference();
  asset_system::all_library_reload_catalogs_if_dirty();
  return asset::build_filtered_all_catalog_tree(library, C, type_filter, {});
}

static void sequencer_add_catalog_assets_draw(const bContext *C, Menu *menu)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  if (!sseq || !ELEM(sseq->view, SEQ_VIEW_SEQUENCE, SEQ_VIEW_SEQUENCE_PREVIEW)) {
    return;
  }
  if (!sseq->runtime->assets_for_menu) {
    sseq->runtime->assets_for_menu = std::make_shared<asset::AssetItemTree>(
        build_catalog_tree(*C));
    return;
  }
  asset::AssetItemTree &tree = *sseq->runtime->assets_for_menu;

  const std::optional<StringRefNull> menu_path = CTX_data_string_get(C, "asset_catalog_path");
  if (!menu_path) {
    return;
  }
  const Span<asset_system::AssetRepresentation *> assets = tree.assets_per_path.lookup(
      menu_path->c_str());
  const asset_system::AssetCatalogTreeItem *catalog_item = tree.catalogs.find_item(
      menu_path->c_str());
  BLI_assert(catalog_item != nullptr);

  if (assets.is_empty() && !catalog_item->has_children()) {
    return;
  }

  uiLayout *layout = menu->layout;
  bool add_separator = true;

  for (const asset_system::AssetRepresentation *asset : assets) {
    if (add_separator) {
      layout->separator();
      add_separator = false;
    }
    PointerRNA op_ptr = layout->op("SEQUENCER_OT_add_scene_strip_from_scene_asset",
                                   IFACE_(asset->get_name()),
                                   ICON_NONE,
                                   wm::OpCallContext::InvokeRegionWin,
                                   UI_ITEM_NONE);
    asset::operator_asset_reference_props_set(*asset, op_ptr);
  }

  catalog_item->foreach_child([&](const asset_system::AssetCatalogTreeItem &item) {
    if (add_separator) {
      layout->separator();
      add_separator = false;
    }
    asset::draw_menu_for_catalog(item, "SEQUENCER_MT_scene_add_catalog_assets", *layout);
  });
}

static void sequencer_add_unassigned_assets_draw(const bContext *C, Menu *menu)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  if (!sseq || !ELEM(sseq->view, SEQ_VIEW_SEQUENCE, SEQ_VIEW_SEQUENCE_PREVIEW)) {
    return;
  }
  if (!sseq->runtime->assets_for_menu) {
    sseq->runtime->assets_for_menu = std::make_shared<asset::AssetItemTree>(
        build_catalog_tree(*C));
    return;
  }
  asset::AssetItemTree &tree = *sseq->runtime->assets_for_menu;
  for (const asset_system::AssetRepresentation *asset : tree.unassigned_assets) {
    PointerRNA op_ptr = menu->layout->op("SEQUENCER_OT_add_scene_strip_from_scene_asset",
                                         IFACE_(asset->get_name()),
                                         ICON_NONE,
                                         wm::OpCallContext::InvokeRegionWin,
                                         UI_ITEM_NONE);
    BLI_assert(op_ptr.data != nullptr);
    asset::operator_asset_reference_props_set(*asset, op_ptr);
  }
}

static void sequencer_add_scene_draw(const bContext *C, Menu *menu)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  if (!sseq || !ELEM(sseq->view, SEQ_VIEW_SEQUENCE, SEQ_VIEW_SEQUENCE_PREVIEW)) {
    return;
  }

  uiLayout *layout = menu->layout;
  layout->operator_context_set(wm::OpCallContext::InvokeRegionWin);

  /* New scene. */
  {
    PointerRNA op_ptr = layout->op(
        "SEQUENCER_OT_scene_strip_add_new", IFACE_("Empty Scene"), ICON_ADD);
    RNA_enum_set(&op_ptr, "type", SCE_COPY_NEW);
  }

  /* Handle the assets. */
  sseq->runtime->assets_for_menu = std::make_shared<asset::AssetItemTree>(build_catalog_tree(*C));
  const bool loading_finished = all_loading_finished();

  asset::AssetItemTree &tree = *sseq->runtime->assets_for_menu;
  const bool show_assets = !(tree.catalogs.is_empty() && loading_finished &&
                             tree.unassigned_assets.is_empty());
  if (show_assets) {
    layout->separator();

    layout->label(IFACE_("Assets"), ICON_ASSET_MANAGER);

    if (!loading_finished) {
      layout->label(IFACE_("Loading Asset Libraries"), ICON_INFO);
    }

    tree.catalogs.foreach_root_item([&](const asset_system::AssetCatalogTreeItem &item) {
      asset::draw_menu_for_catalog(item, "SEQUENCER_MT_scene_add_catalog_assets", *layout);
    });

    if (!tree.unassigned_assets.is_empty()) {
      layout->menu_contents("SEQUENCER_MT_scene_add_unassigned_assets");
    }

    layout->separator();
  }

  /* Show existing scenes. */
  Main *bmain = CTX_data_main(C);
  const int scenes_len = BLI_listbase_count(&bmain->scenes);
  if (scenes_len > 10) {
    layout->op("SEQUENCER_OT_scene_strip_add",
               IFACE_("Scenes..."),
               ICON_SCENE_DATA,
               wm::OpCallContext::InvokeDefault,
               UI_ITEM_NONE);
  }
  else if (scenes_len > 1) {
    if (show_assets) {
      layout->label(IFACE_("Scenes"), ICON_SCENE_DATA);
    }
    const Scene *active_scene = CTX_data_sequencer_scene(C);
    int i = 0;
    LISTBASE_FOREACH_INDEX (Scene *, scene, &bmain->scenes, i) {
      if (scene == active_scene) {
        continue;
      }
      if (scene->id.asset_data) {
        continue;
      }
      PointerRNA op_ptr = layout->op("SEQUENCER_OT_scene_strip_add",
                                     scene->id.name + 2,
                                     ICON_NONE,
                                     wm::OpCallContext::InvokeRegionWin,
                                     UI_ITEM_NONE);
      RNA_enum_set(&op_ptr, "scene", i);
    }
  }
}

MenuType add_catalog_assets_menu_type()
{
  MenuType type{};
  STRNCPY(type.idname, "SEQUENCER_MT_scene_add_catalog_assets");
  type.poll = sequencer_add_menu_poll;
  type.draw = sequencer_add_catalog_assets_draw;
  type.listener = asset::list::asset_reading_region_listen_fn;
  type.flag = MenuTypeFlag::ContextDependent;
  return type;
}

MenuType add_unassigned_assets_menu_type()
{
  MenuType type{};
  STRNCPY(type.idname, "SEQUENCER_MT_scene_add_unassigned_assets");
  type.poll = sequencer_add_menu_poll;
  type.draw = sequencer_add_unassigned_assets_draw;
  type.listener = asset::list::asset_reading_region_listen_fn;
  type.flag = MenuTypeFlag::ContextDependent;
  type.description = N_(
      "Scene assets not assigned to a catalog.\n"
      "Catalogs can be assigned in the Asset Browser");
  return type;
}

MenuType add_scene_menu_type()
{
  MenuType type{};
  STRNCPY(type.idname, "SEQUENCER_MT_add_scene");
  type.poll = sequencer_add_menu_poll;
  type.draw = sequencer_add_scene_draw;
  type.listener = asset::list::asset_reading_region_listen_fn;
  return type;
}

}  // namespace blender::ed::vse
