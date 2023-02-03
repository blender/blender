/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "BLO_read_write.h"

#include "BLT_translation.h"

#include "DNA_screen_types.h"

#include "ED_asset_list.h"
#include "ED_asset_list.hh"
#include "ED_screen.h"

#include "RNA_prototypes.h"

#include "UI_interface.h"
#include "UI_interface.hh"
#include "UI_resources.h"
#include "UI_tree_view.hh"

#include "WM_api.h"

#include "ED_asset_shelf.h"

using namespace blender;

static void asset_shelf_send_redraw_notifier(bContext &C)
{
  WM_event_add_notifier(&C, NC_SPACE | ND_SPACE_ASSET_SHELF, nullptr);
}

/* -------------------------------------------------------------------- */
/** \name Asset Shelf Regions
 * \{ */

static void asset_shelf_region_listen(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  switch (wmn->category) {
    case NC_SPACE:
      if (wmn->data == ND_SPACE_ASSET_SHELF) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

void ED_asset_shelf_region_listen(const wmRegionListenerParams *params)
{
  if (ED_assetlist_listen(params->notifier)) {
    ED_region_tag_redraw_no_rebuild(params->region);
  }
  /* If the asset list didn't catch the notifier, let the region itself listen. */
  else {
    asset_shelf_region_listen(params);
  }
}

void ED_asset_shelf_footer_region_listen(const wmRegionListenerParams *params)
{
  asset_shelf_region_listen(params);
}

void ED_asset_shelf_footer_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

void ED_asset_shelf_footer_region(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Asset Shelf Settings
 * \{ */

AssetShelfSettings *ED_asset_shelf_settings_duplicate(const AssetShelfSettings *shelf_settings)
{
  if (!shelf_settings) {
    return nullptr;
  }

  static_assert(
      std::is_trivial_v<AssetShelfSettings>,
      "AssetShelfSettings needs to be trivial to allow freeing with MEM_freeN() (API promise)");
  AssetShelfSettings *new_settings = MEM_new<AssetShelfSettings>(__func__, *shelf_settings);

  LISTBASE_FOREACH (LinkData *, catalog_path_item, &shelf_settings->enabled_catalog_paths) {
    LinkData *new_path_item = static_cast<LinkData *>(MEM_dupallocN(catalog_path_item));
    new_path_item->data = BLI_strdup((char *)catalog_path_item->data);
    BLI_addtail(&new_settings->enabled_catalog_paths, new_path_item);
  }

  return new_settings;
}

static void asset_shelf_settings_clear_enabled_catalogs(AssetShelfSettings &shelf_settings)
{
  LISTBASE_FOREACH_MUTABLE (LinkData *, catalog_path_item, &shelf_settings.enabled_catalog_paths) {
    MEM_freeN(catalog_path_item->data);
    BLI_freelinkN(&shelf_settings.enabled_catalog_paths, catalog_path_item);
  }
  BLI_assert(BLI_listbase_is_empty(&shelf_settings.enabled_catalog_paths));
}

static void asset_shelf_settings_set_active_catalog(AssetShelfSettings &shelf_settings,
                                                    const asset_system::AssetCatalogPath &path)
{
  MEM_delete(shelf_settings.active_catalog_path);
  shelf_settings.active_catalog_path = BLI_strdupn(path.c_str(), path.length());
}

static bool asset_shelf_settings_is_active_catalog(const AssetShelfSettings &shelf_settings,
                                                   const asset_system::AssetCatalogPath &path)
{
  return shelf_settings.active_catalog_path && shelf_settings.active_catalog_path == path.str();
}

void ED_asset_shelf_settings_free(AssetShelfSettings *shelf_settings)
{
  asset_shelf_settings_clear_enabled_catalogs(*shelf_settings);
  MEM_delete(shelf_settings->active_catalog_path);
}

void ED_asset_shelf_settings_blend_write(BlendWriter *writer,
                                         const AssetShelfSettings *shelf_settings)
{
  if (!shelf_settings) {
    return;
  }

  BLO_write_struct(writer, AssetShelfSettings, shelf_settings);

  LISTBASE_FOREACH (LinkData *, catalog_path_item, &shelf_settings->enabled_catalog_paths) {
    BLO_write_struct(writer, LinkData, catalog_path_item);
    BLO_write_string(writer, (const char *)catalog_path_item->data);
  }
}

void ED_asset_shelf_settings_blend_read_data(BlendDataReader *reader,
                                             AssetShelfSettings **shelf_settings)
{
  if (!*shelf_settings) {
    return;
  }

  BLO_read_data_address(reader, shelf_settings);

  BLO_read_list(reader, &(*shelf_settings)->enabled_catalog_paths);
  LISTBASE_FOREACH (LinkData *, catalog_path_item, &(*shelf_settings)->enabled_catalog_paths) {
    BLO_read_data_address(reader, &catalog_path_item->data);
  }
}

static bool asset_shelf_settings_is_catalog_path_enabled(
    const AssetShelfSettings &shelf_settings, const asset_system::AssetCatalogPath &path)
{
  LISTBASE_FOREACH (LinkData *, catalog_path_item, &shelf_settings.enabled_catalog_paths) {
    if (StringRef((const char *)catalog_path_item->data) == path.str()) {
      return true;
    }
  }
  return false;
}

static void asset_shelf_settings_set_catalog_path_enabled(
    AssetShelfSettings &shelf_settings, const asset_system::AssetCatalogPath &path)
{
  char *path_copy = BLI_strdupn(path.c_str(), path.length());
  BLI_addtail(&shelf_settings.enabled_catalog_paths, BLI_genericNodeN(path_copy));
}

static void asset_shelf_settings_foreach_enabled_catalog_path(
    const AssetShelfSettings &shelf_settings,
    FunctionRef<void(const asset_system::AssetCatalogPath &catalog_path)> fn)
{
  LISTBASE_FOREACH (LinkData *, catalog_path_item, &shelf_settings.enabled_catalog_paths) {
    fn(asset_system::AssetCatalogPath((char *)catalog_path_item->data));
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Asset Shelf Context
 * \{ */

int ED_asset_shelf_context(const bContext *C,
                           const char *member,
                           bContextDataResult *result,
                           AssetShelfSettings *shelf_settings)
{
  static const char *context_dir[] = {
      "asset_shelf_settings",
      "active_file", /* XXX yuk... */
      nullptr,
  };

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, context_dir);
    return CTX_RESULT_OK;
  }

  bScreen *screen = CTX_wm_screen(C);

  if (CTX_data_equals(member, "asset_shelf_settings")) {
    CTX_data_pointer_set(result, &screen->id, &RNA_AssetShelfSettings, shelf_settings);

    return CTX_RESULT_OK;
  }

  /* XXX hack. Get the asset from the hovered button, but needs to be the file... */
  if (CTX_data_equals(member, "active_file")) {
    const uiBut *but = UI_context_active_but_get(C);
    if (!but) {
      return CTX_RESULT_NO_DATA;
    }

    const bContextStore *but_context = UI_but_context_get(but);
    if (!but_context) {
      return CTX_RESULT_NO_DATA;
    }

    const PointerRNA *file_ptr = CTX_store_ptr_lookup(
        but_context, "active_file", &RNA_FileSelectEntry);
    if (!file_ptr) {
      return CTX_RESULT_NO_DATA;
    }

    CTX_data_pointer_set_ptr(result, file_ptr);
    return CTX_RESULT_OK;
  }

  return CTX_RESULT_MEMBER_NOT_FOUND;
}

static AssetShelfSettings *get_asset_shelf_settings_from_context(const bContext *C)
{
  PointerRNA shelf_settings_ptr = CTX_data_pointer_get_type(
      C, "asset_shelf_settings", &RNA_AssetShelfSettings);
  return static_cast<AssetShelfSettings *>(shelf_settings_ptr.data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Asset Catalog Selector UI
 *
 *  Popup containing a tree-view to select which catalogs to display in the asset shelf footer.
 * \{ */

class AssetCatalogSelectorTree : public ui::AbstractTreeView {
  asset_system::AssetLibrary &library_;
  asset_system::AssetCatalogTree *catalog_tree_;
  AssetShelfSettings &shelf_settings_;

 public:
  class Item;

  AssetCatalogSelectorTree(asset_system::AssetLibrary &library, AssetShelfSettings &shelf_settings)
      : library_(library), shelf_settings_(shelf_settings)
  {
    asset_system::AssetCatalogService *catalog_service = library_.catalog_service.get();
    catalog_tree_ = catalog_service->get_catalog_tree();
  }

  void build_tree() override
  {
    if (!catalog_tree_) {
      return;
    }

    catalog_tree_->foreach_root_item([this](asset_system::AssetCatalogTreeItem &catalog_item) {
      build_catalog_items_recursive(*this, catalog_item);
    });
  }

  Item &build_catalog_items_recursive(ui::TreeViewOrItem &parent_view_item,
                                      asset_system::AssetCatalogTreeItem &catalog_item) const
  {
    Item &view_item = parent_view_item.add_tree_item<Item>(catalog_item, shelf_settings_);

    catalog_item.foreach_child([&view_item, this](asset_system::AssetCatalogTreeItem &child) {
      build_catalog_items_recursive(view_item, child);
    });

    return view_item;
  }

  void update_shelf_settings_from_enabled_catalogs();

  class Item : public ui::BasicTreeViewItem {
    asset_system::AssetCatalogTreeItem catalog_item_;
    /* Is the catalog path enabled in this redraw? Set on construction, updated by the UI (which
     * gets a pointer to it). The UI needs it as char. */
    char catalog_path_enabled_ = false;

   public:
    Item(asset_system::AssetCatalogTreeItem &catalog_item, AssetShelfSettings &shelf_settings)
        : ui::BasicTreeViewItem(catalog_item.get_name()),
          catalog_item_(catalog_item),
          catalog_path_enabled_(asset_shelf_settings_is_catalog_path_enabled(
              shelf_settings, catalog_item.catalog_path()))
    {
    }

    bool is_catalog_path_enabled() const
    {
      return catalog_path_enabled_ != 0;
    }

    asset_system::AssetCatalogPath catalog_path() const
    {
      return catalog_item_.catalog_path();
    }

    void build_row(uiLayout &row) override
    {
      AssetCatalogSelectorTree &tree = dynamic_cast<AssetCatalogSelectorTree &>(get_tree_view());
      uiBlock *block = uiLayoutGetBlock(&row);

      uiLayoutSetEmboss(&row, UI_EMBOSS);

      if (!is_collapsible()) {
        uiItemL(&row, nullptr, ICON_BLANK1);
      }

      uiBut *but = uiDefButC(block,
                             UI_BTYPE_CHECKBOX,
                             0,
                             catalog_item_.get_name().c_str(),
                             0,
                             0,
                             UI_UNIT_X * 10,
                             UI_UNIT_Y,
                             (char *)&catalog_path_enabled_,
                             0,
                             0,
                             0,
                             0,
                             TIP_("Toggle catalog visibility in the asset shelf"));
      UI_but_func_set(but, [&tree](bContext &C) {
        tree.update_shelf_settings_from_enabled_catalogs();
        asset_shelf_send_redraw_notifier(C);
      });
      UI_but_flag_disable(but, UI_BUT_UNDO);
    }
  };
};

void AssetCatalogSelectorTree::update_shelf_settings_from_enabled_catalogs()
{
  asset_shelf_settings_clear_enabled_catalogs(shelf_settings_);
  foreach_item([this](ui::AbstractTreeViewItem &view_item) {
    const auto &selector_tree_item = dynamic_cast<AssetCatalogSelectorTree::Item &>(view_item);
    if (selector_tree_item.is_catalog_path_enabled()) {
      asset_shelf_settings_set_catalog_path_enabled(shelf_settings_,
                                                    selector_tree_item.catalog_path());
    }
  });
}

static uiBlock *asset_shelf_catalog_selector_block_draw(bContext *C,
                                                        ARegion *region,
                                                        void * /*arg1*/)
{
  const AssetLibraryReference *library_ref = CTX_wm_asset_library_ref(C);
  asset_system::AssetLibrary *library = ED_assetlist_library_get_once_available(*library_ref);
  AssetShelfSettings *shelf_settings = get_asset_shelf_settings_from_context(C);

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  UI_block_flag_enable(block,
                       UI_BLOCK_KEEP_OPEN | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_POPUP_CAN_REFRESH);

  uiLayout *layout = UI_block_layout(block,
                                     UI_LAYOUT_VERTICAL,
                                     UI_LAYOUT_PANEL,
                                     0,
                                     0,
                                     UI_UNIT_X * 12,
                                     UI_UNIT_Y,
                                     0,
                                     UI_style_get());

  uiItemL(layout, "Enable Catalogs", ICON_NONE);
  uiItemS(layout);

  uiLayoutSetEmboss(layout, UI_EMBOSS_NONE);
  if (library && shelf_settings) {
    ui::AbstractTreeView *tree_view = UI_block_add_view(
        *block,
        "asset catalog tree view",
        std::make_unique<AssetCatalogSelectorTree>(*library, *shelf_settings));

    ui::TreeViewBuilder builder(*block);
    builder.build_tree_view(*tree_view);
  }

  UI_block_bounds_set_normal(block, 0.3f * U.widget_unit);
  UI_block_direction_set(block, UI_DIR_UP);

  return block;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Catalog toggle buttons
 * \{ */

static void add_catalog_toggle_buttons(AssetShelfSettings &shelf_settings, uiLayout &layout)
{
  uiBlock *block = uiLayoutGetBlock(&layout);
  const uiStyle *style = UI_style_get_dpi();

  asset_shelf_settings_foreach_enabled_catalog_path(
      shelf_settings, [&shelf_settings, block, style](const asset_system::AssetCatalogPath &path) {
        const char *name = path.name().c_str();
        const int string_width = UI_fontstyle_string_width(&style->widget, name);
        const int pad_x = UI_UNIT_X * 0.3f;
        const int but_width = std::min(string_width + 2 * pad_x, UI_UNIT_X * 8);

        uiBut *but = uiDefBut(
            block,
            UI_BTYPE_TAB,
            0,
            name,
            0,
            0,
            but_width,
            UI_UNIT_Y,
            nullptr,
            0,
            0,
            0,
            0,
            "Enable catalog, making contained assets visible in the asset shelf");

        UI_but_drawflag_enable(but, UI_BUT_ALIGN_TOP);
        UI_but_func_set(but, [&shelf_settings, path](bContext &C) {
          asset_shelf_settings_set_active_catalog(shelf_settings, path);
          asset_shelf_send_redraw_notifier(C);
        });
        UI_but_func_pushed_state_set(but, [&shelf_settings, path](const uiBut &) -> bool {
          return asset_shelf_settings_is_active_catalog(shelf_settings, path);
        });
      });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Asset Shelf Footer
 *
 * Implemented as HeaderType for #RGN_TYPE_ASSET_SHELF_FOOTER.
 * \{ */

static void asset_shelf_footer_draw(const bContext *C, Header *header)
{
  uiLayout *layout = header->layout;
  uiBlock *block = uiLayoutGetBlock(layout);
  const AssetLibraryReference *library_ref = CTX_wm_asset_library_ref(C);

  ED_assetlist_storage_fetch(library_ref, C);
  uiDefIconBlockBut(block,
                    asset_shelf_catalog_selector_block_draw,
                    nullptr,
                    0,
                    ICON_RIGHTARROW,
                    0,
                    0,
                    UI_UNIT_X * 1.5f,
                    UI_UNIT_Y,
                    TIP_("Select catalogs to display"));

  uiItemS(layout);

  AssetShelfSettings *shelf_settings = get_asset_shelf_settings_from_context(C);
  if (shelf_settings) {
    add_catalog_toggle_buttons(*shelf_settings, *layout);
  }
}

void ED_asset_shelf_footer_register(ARegionType *region_type,
                                    const char *idname,
                                    const int space_type)
{
  HeaderType *ht = MEM_cnew<HeaderType>(__func__);
  strcpy(ht->idname, idname);
  ht->space_type = space_type;
  ht->region_type = RGN_TYPE_ASSET_SHELF_FOOTER;
  ht->draw = asset_shelf_footer_draw;
  BLI_addtail(&region_type->headertypes, ht);
}

/** \} */
