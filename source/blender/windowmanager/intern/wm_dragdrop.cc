/* SPDX-FileCopyrightText: 2010 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Our own drag-and-drop, drag state and drop boxes.
 */

#include <cstring>

#include "AS_asset_representation.hh"

#include "DNA_asset_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLT_translation.hh"

#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BIF_glutil.hh"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_preview_image.hh"
#include "BKE_screen.hh"

#include "BLO_readfile.hh"

#include "ED_fileselect.hh"
#include "ED_screen.hh"

#include "GPU_shader_builtin.hh"
#include "GPU_state.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "GHOST_Types.h"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"
#include "wm_event_system.hh"
#include "wm_window.hh"

#include <fmt/format.h>
/* ****************************************************** */

static ListBase dropboxes = {nullptr, nullptr};

static void wm_drag_free_asset_data(wmDragAsset **asset_data);
static void wm_drag_free_path_data(wmDragPath **path_data);

static void wm_drop_item_free_data(wmDropBox *drop);
static void wm_drop_item_clear_runtime(wmDropBox *drop);

wmDragActiveDropState::wmDragActiveDropState() = default;
wmDragActiveDropState::~wmDragActiveDropState() = default;

/* Drop box maps are stored global for now. */
/* These are part of blender's UI/space specs, and not like keymaps. */
/* When editors become configurable, they can add their own dropbox definitions. */

struct wmDropBoxMap {
  wmDropBoxMap *next, *prev;

  ListBase dropboxes;
  short spaceid, regionid;
  char idname[KMAP_MAX_NAME];
};

ListBase *WM_dropboxmap_find(const char *idname, int spaceid, int regionid)
{
  LISTBASE_FOREACH (wmDropBoxMap *, dm, &dropboxes) {
    if (dm->spaceid == spaceid && dm->regionid == regionid) {
      if (STREQLEN(idname, dm->idname, KMAP_MAX_NAME)) {
        return &dm->dropboxes;
      }
    }
  }

  wmDropBoxMap *dm = MEM_callocN<wmDropBoxMap>(__func__);
  STRNCPY_UTF8(dm->idname, idname);
  dm->spaceid = spaceid;
  dm->regionid = regionid;
  BLI_addtail(&dropboxes, dm);

  return &dm->dropboxes;
}

wmDropBox *WM_dropbox_add(ListBase *lb,
                          const char *idname,
                          bool (*poll)(bContext *C, wmDrag *drag, const wmEvent *event),
                          void (*copy)(bContext *C, wmDrag *drag, wmDropBox *drop),
                          void (*cancel)(Main *bmain, wmDrag *drag, wmDropBox *drop),
                          WMDropboxTooltipFunc tooltip)
{
  wmOperatorType *ot = WM_operatortype_find(idname, true);
  if (ot == nullptr) {
    printf("Error: dropbox with unknown operator: %s\n", idname);
    return nullptr;
  }

  wmDropBox *drop = MEM_callocN<wmDropBox>(__func__);
  drop->poll = poll;
  drop->copy = copy;
  drop->cancel = cancel;
  drop->tooltip = tooltip;
  drop->ot = ot;
  STRNCPY(drop->opname, idname);

  WM_operator_properties_alloc(&(drop->ptr), &(drop->properties), idname);
  WM_operator_properties_sanitize(drop->ptr, true);

  /* Signal for no context, see #STRUCT_NO_CONTEXT_WITHOUT_OWNER_ID. */
  drop->ptr->owner_id = nullptr;

  BLI_addtail(lb, drop);

  return drop;
}

static void wm_dropbox_item_update_ot(wmDropBox *drop)
{
  /* NOTE(@ideasman42): this closely follows #wm_keymap_item_properties_update_ot.
   * `keep_properties` is implied because drop boxes aren't dynamically added & removed.
   * It's possible in the future drop-boxes can be (un)registered by scripts.
   * In this case we might want to remove drop-boxes that point to missing operators. */
  wmOperatorType *ot = WM_operatortype_find(drop->opname, false);
  if (ot == nullptr) {
    /* Allow for the operator to be added back and re-validated, keep it's properties. */
    wm_drop_item_clear_runtime(drop);
    drop->ot = nullptr;
    return;
  }

  if (drop->ptr == nullptr) {
    WM_operator_properties_alloc(&(drop->ptr), &(drop->properties), drop->opname);
    WM_operator_properties_sanitize(drop->ptr, true);
  }
  else {
    if (ot->srna != drop->ptr->type) {
      WM_operator_properties_create_ptr(drop->ptr, ot);
      if (drop->properties) {
        drop->ptr->data = drop->properties;
      }
      WM_operator_properties_sanitize(drop->ptr, true);
    }
  }

  if (drop->ptr) {
    drop->ptr->owner_id = nullptr;
  }
  drop->ot = ot;
}

void WM_dropbox_update_ot()
{
  LISTBASE_FOREACH (wmDropBoxMap *, dm, &dropboxes) {
    LISTBASE_FOREACH (wmDropBox *, drop, &dm->dropboxes) {
      wm_dropbox_item_update_ot(drop);
    }
  }
}

static void wm_drop_item_free_data(wmDropBox *drop)
{
  if (drop->ptr) {
    WM_operator_properties_free(drop->ptr);
    MEM_delete(drop->ptr);
    drop->ptr = nullptr;
    drop->properties = nullptr;
  }
  else if (drop->properties) {
    IDP_FreeProperty(drop->properties);
    drop->properties = nullptr;
  }
}

static void wm_drop_item_clear_runtime(wmDropBox *drop)
{
  IDProperty *properties = drop->properties;
  drop->properties = nullptr;
  if (drop->ptr) {
    drop->ptr->data = nullptr;
  }
  wm_drop_item_free_data(drop);
  drop->properties = properties;
}

void wm_dropbox_free()
{

  LISTBASE_FOREACH (wmDropBoxMap *, dm, &dropboxes) {
    LISTBASE_FOREACH (wmDropBox *, drop, &dm->dropboxes) {
      wm_drop_item_free_data(drop);
    }
    BLI_freelistN(&dm->dropboxes);
  }

  BLI_freelistN(&dropboxes);
}

/* *********************************** */

static void wm_dropbox_invoke(bContext *C, wmDrag *drag)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  /* Create a bitmap flag matrix of all currently visible region and area types.
   * Everything that isn't visible in the current window should not prefetch any data. */
  bool area_region_tag[SPACE_TYPE_NUM][RGN_TYPE_NUM] = {{false}};

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    bScreen *screen = WM_window_get_active_screen(win);
    ED_screen_areas_iter (win, screen, area) {
      LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
        if (region->runtime->visible) {
          BLI_assert(area->spacetype < SPACE_TYPE_NUM);
          BLI_assert(region->regiontype < RGN_TYPE_NUM);
          area_region_tag[area->spacetype][region->regiontype] = true;
        }
      }
    }
  }

  LISTBASE_FOREACH (wmDropBoxMap *, dm, &dropboxes) {
    if (!area_region_tag[dm->spaceid][dm->regionid]) {
      continue;
    }
    LISTBASE_FOREACH (wmDropBox *, drop, &dm->dropboxes) {
      if (drag->drop_state.ui_context) {
        CTX_store_set(C, drag->drop_state.ui_context.get());
      }

      if (drop->on_drag_start) {
        drop->on_drag_start(C, drag);
      }
      CTX_store_set(C, nullptr);
    }
  }
}

wmDrag *WM_drag_data_create(bContext *C, int icon, eWM_DragDataType type, void *poin, uint flags)
{
  wmDrag *drag = MEM_new<wmDrag>(__func__);

  /* Keep track of future multi-touch drag too, add a mouse-pointer id or so. */
  /* If multiple drags are added, they're drawn as list. */

  drag->flags = static_cast<eWM_DragFlags>(flags);
  drag->icon = icon;
  drag->type = type;
  switch (type) {
    case WM_DRAG_PATH:
      drag->poin = poin;
      drag->flags |= WM_DRAG_FREE_DATA;
      break;
    case WM_DRAG_ID:
      if (poin) {
        WM_drag_add_local_ID(drag, static_cast<ID *>(poin), nullptr);
      }
      break;
    case WM_DRAG_GREASE_PENCIL_LAYER:
    case WM_DRAG_ASSET:
    case WM_DRAG_ASSET_CATALOG:
      /* Move ownership of poin to wmDrag. */
      drag->poin = poin;
      drag->flags |= WM_DRAG_FREE_DATA;
      break;
      /* The asset-list case is special: We get multiple assets from context and attach them to the
       * drag item. */
    case WM_DRAG_ASSET_LIST: {
      blender::Vector<PointerRNA> asset_links = CTX_data_collection_get(C, "selected_assets");
      for (const PointerRNA &ptr : asset_links) {
        const AssetRepresentationHandle *asset = static_cast<const AssetRepresentationHandle *>(
            ptr.data);
        WM_drag_add_asset_list_item(drag, asset);
      }
      break;
    }
    default:
      drag->poin = poin;
      break;
  }

  return drag;
}

void WM_event_start_prepared_drag(bContext *C, wmDrag *drag)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  BLI_addtail(&wm->runtime->drags, drag);
  wm_dropbox_invoke(C, drag);
}

void WM_event_start_drag(bContext *C, int icon, eWM_DragDataType type, void *poin, uint flags)
{
  wmDrag *drag = WM_drag_data_create(C, icon, type, poin, flags);
  WM_event_start_prepared_drag(C, drag);
}

void wm_drags_exit(wmWindowManager *wm, wmWindow *win)
{
  /* Turn off modal cursor for all windows. */
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    WM_cursor_modal_restore(win);
  }

  /* Active area should always redraw, even if canceled. */
  int event_xy_target[2];
  wmWindow *target_win = WM_window_find_under_cursor(win, win->eventstate->xy, event_xy_target);
  if (target_win) {
    const bScreen *screen = WM_window_get_active_screen(target_win);
    ED_region_tag_redraw_no_rebuild(screen->active_region);

    /* Ensure the correct area cursor is restored. */
    target_win->tag_cursor_refresh = true;
    WM_event_add_mousemove(target_win);
  }
}

static std::unique_ptr<bContextStore> wm_drop_ui_context_create(const bContext *C)
{
  uiBut *active_but = UI_region_active_but_get(CTX_wm_region(C));
  if (!active_but) {
    return nullptr;
  }

  const bContextStore *but_context = UI_but_context_get(active_but);
  if (!but_context) {
    return nullptr;
  }

  return std::make_unique<bContextStore>(*but_context);
}

void WM_event_drag_image(wmDrag *drag, const ImBuf *imb, float scale)
{
  drag->imb = imb;
  drag->imbuf_scale = scale;
}

void WM_event_drag_path_override_poin_data_with_space_file_paths(const bContext *C, wmDrag *drag)
{
  BLI_assert(drag->type == WM_DRAG_PATH);
  const SpaceFile *sfile = CTX_wm_space_file(C);
  if (!sfile) {
    return;
  }
  const blender::Vector<std::string> selected_paths = ED_fileselect_selected_files_full_paths(
      sfile);
  blender::Vector<const char *> paths;
  for (const std::string &path : selected_paths) {
    paths.append(path.c_str());
  }
  if (paths.is_empty()) {
    return;
  }
  WM_drag_data_free(drag->type, drag->poin);
  drag->poin = WM_drag_create_path_data(paths);
}

void WM_event_drag_preview_icon(wmDrag *drag, int icon_id)
{
  BLI_assert_msg(!drag->imb, "Drag image and preview are mutually exclusive");
  drag->preview_icon_id = icon_id;
}

void WM_drag_data_free(eWM_DragDataType dragtype, void *poin)
{
  /* Don't require all the callers to have a nullptr-check, just allow passing nullptr. */
  if (!poin) {
    return;
  }

  /* Not too nice, could become a callback. */
  switch (dragtype) {
    case WM_DRAG_ASSET: {
      wmDragAsset *asset_data = static_cast<wmDragAsset *>(poin);
      wm_drag_free_asset_data(&asset_data);
      break;
    }
    case WM_DRAG_PATH: {
      wmDragPath *path_data = static_cast<wmDragPath *>(poin);
      wm_drag_free_path_data(&path_data);
      break;
    }
    case WM_DRAG_STRING: {
      std::string *str = static_cast<std::string *>(poin);
      MEM_delete(str);
      break;
    }
    default:
      MEM_freeN(poin);
      break;
  }
}

void WM_drag_free(wmDrag *drag)
{
  if (drag->drop_state.active_dropbox && drag->drop_state.active_dropbox->on_exit) {
    drag->drop_state.active_dropbox->on_exit(drag->drop_state.active_dropbox, drag);
  }
  if (drag->flags & WM_DRAG_FREE_DATA) {
    WM_drag_data_free(drag->type, drag->poin);
  }
  drag->drop_state.ui_context.reset();
  if (drag->drop_state.free_disabled_info) {
    MEM_SAFE_FREE(drag->drop_state.disabled_info);
  }
  BLI_freelistN(&drag->ids);
  LISTBASE_FOREACH_MUTABLE (wmDragAssetListItem *, asset_item, &drag->asset_items) {
    if (asset_item->is_external) {
      wm_drag_free_asset_data(&asset_item->asset_data.external_info);
    }
    BLI_freelinkN(&drag->asset_items, asset_item);
  }
  MEM_delete(drag);
}

void WM_drag_free_list(ListBase *lb)
{
  while (wmDrag *drag = static_cast<wmDrag *>(BLI_pophead(lb))) {
    WM_drag_free(drag);
  }
}

static std::string dropbox_tooltip(bContext *C, wmDrag *drag, const int xy[2], wmDropBox *drop)
{
  if (drop->tooltip) {
    return drop->tooltip(C, drag, xy, drop);
  }
  if (drop->ot) {
    return WM_operatortype_name(drop->ot, drop->ptr);
  }
  return {};
}

static wmDropBox *dropbox_active(bContext *C,
                                 ListBase *handlers,
                                 wmDrag *drag,
                                 const wmEvent *event)
{
  if (drag->drop_state.free_disabled_info) {
    MEM_SAFE_FREE(drag->drop_state.disabled_info);
  }
  drag->drop_state.disabled_info = nullptr;

  LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers) {
    if (handler_base->type == WM_HANDLER_TYPE_DROPBOX) {
      wmEventHandler_Dropbox *handler = (wmEventHandler_Dropbox *)handler_base;
      if (handler->dropboxes) {
        LISTBASE_FOREACH (wmDropBox *, drop, handler->dropboxes) {
          if (drag->drop_state.ui_context) {
            CTX_store_set(C, drag->drop_state.ui_context.get());
          }

          if (!drop->poll(C, drag, event)) {
            /* If the drop's poll fails, don't set the disabled-info. This would be too aggressive.
             * Instead show it only if the drop box could be used in principle, but the operator
             * can't be executed. */
            continue;
          }

          const blender::wm::OpCallContext opcontext = wm_drop_operator_context_get(drop);
          if (drop->ot && WM_operator_poll_context(C, drop->ot, opcontext)) {
            /* Get dropbox tooltip now, #wm_drag_draw_tooltip can use a different draw context. */
            drag->drop_state.tooltip = dropbox_tooltip(C, drag, event->xy, drop);
            CTX_store_set(C, nullptr);
            return drop;
          }

          /* Attempt to set the disabled hint when the poll fails. Will always be the last hint set
           * when there are multiple failing polls (could allow multiple disabled-hints too). */
          bool free_disabled_info = false;
          const char *disabled_hint = CTX_wm_operator_poll_msg_get(C, &free_disabled_info);
          if (disabled_hint) {
            drag->drop_state.disabled_info = disabled_hint;
            drag->drop_state.free_disabled_info = free_disabled_info;
          }
        }
      }
    }
  }
  CTX_store_set(C, nullptr);
  return nullptr;
}

/* Return active operator tooltip/name when mouse is in box. */
static wmDropBox *wm_dropbox_active(bContext *C, wmDrag *drag, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = WM_window_get_active_screen(win);
  ScrArea *area = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, event->xy);
  wmDropBox *drop = nullptr;

  if (area) {
    ARegion *region = BKE_area_find_region_xy(area, RGN_TYPE_ANY, event->xy);
    if (region) {
      drop = dropbox_active(C, &region->runtime->handlers, drag, event);
    }

    if (!drop) {
      drop = dropbox_active(C, &area->handlers, drag, event);
    }
  }
  if (!drop) {
    drop = dropbox_active(C, &win->handlers, drag, event);
  }
  return drop;
}

/**
 * Update dropping information for the current mouse position in \a event.
 */
static void wm_drop_update_active(bContext *C, wmDrag *drag, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  const blender::int2 win_size = WM_window_native_pixel_size(win);

  /* For multi-window drags, we only do this if mouse inside. */
  if (event->xy[0] < 0 || event->xy[1] < 0 || event->xy[0] > win_size[0] ||
      event->xy[1] > win_size[1])
  {
    return;
  }

  /* Update UI context, before polling so polls can query this context. */
  drag->drop_state.ui_context.reset();
  drag->drop_state.ui_context = wm_drop_ui_context_create(C);
  drag->drop_state.tooltip = "";

  wmDropBox *drop_prev = drag->drop_state.active_dropbox;
  wmDropBox *drop = wm_dropbox_active(C, drag, event);
  if (drop != drop_prev) {
    if (drop_prev && drop_prev->on_exit) {
      drop_prev->on_exit(drop_prev, drag);
      BLI_assert(drop_prev->draw_data == nullptr);
    }
    if (drop && drop->on_enter) {
      drop->on_enter(drop, drag);
    }
    drag->drop_state.active_dropbox = drop;
    drag->drop_state.area_from = drop ? CTX_wm_area(C) : nullptr;
    drag->drop_state.region_from = drop ? CTX_wm_region(C) : nullptr;
  }

  if (!drag->drop_state.active_dropbox) {
    drag->drop_state.ui_context.reset();
  }
}

void wm_drop_prepare(bContext *C, wmDrag *drag, wmDropBox *drop)
{
  const blender::wm::OpCallContext opcontext = wm_drop_operator_context_get(drop);

  if (drag->drop_state.ui_context) {
    CTX_store_set(C, drag->drop_state.ui_context.get());
  }

  /* Optionally copy drag information to operator properties. Don't call it if the
   * operator fails anyway, it might do more than just set properties (e.g.
   * typically import an asset). */
  if (drop->copy && WM_operator_poll_context(C, drop->ot, opcontext)) {
    drop->copy(C, drag, drop);
  }

  wm_drags_exit(CTX_wm_manager(C), CTX_wm_window(C));
}

void wm_drop_end(bContext *C, wmDrag * /*drag*/, wmDropBox * /*drop*/)
{
  CTX_store_set(C, nullptr);
}

void wm_drags_check_ops(bContext *C, const wmEvent *event)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  bool any_active = false;
  LISTBASE_FOREACH (wmDrag *, drag, &wm->runtime->drags) {
    wm_drop_update_active(C, drag, event);

    if (drag->drop_state.active_dropbox) {
      any_active = true;
    }
  }

  /* Change the cursor to display that dropping isn't possible here. But only if there is something
   * being dragged actually. Cursor will be restored in #wm_drags_exit(). */
  if (!BLI_listbase_is_empty(&wm->runtime->drags)) {
    WM_cursor_modal_set(CTX_wm_window(C), any_active ? WM_CURSOR_DEFAULT : WM_CURSOR_STOP);
  }
}

blender::wm::OpCallContext wm_drop_operator_context_get(const wmDropBox * /*drop*/)
{
  return blender::wm::OpCallContext::InvokeDefault;
}

/* ************** IDs ***************** */

void WM_drag_add_local_ID(wmDrag *drag, ID *id, ID *from_parent)
{
  /* Don't drag the same ID twice. */
  LISTBASE_FOREACH (wmDragID *, drag_id, &drag->ids) {
    if (drag_id->id == id) {
      if (drag_id->from_parent == nullptr) {
        drag_id->from_parent = from_parent;
      }
      return;
    }
    if (GS(drag_id->id->name) != GS(id->name)) {
      BLI_assert_msg(0, "All dragged IDs must have the same type");
      return;
    }
  }

  /* Add to list. */
  wmDragID *drag_id = MEM_callocN<wmDragID>(__func__);
  drag_id->id = id;
  drag_id->from_parent = from_parent;
  BLI_addtail(&drag->ids, drag_id);
}

ID *WM_drag_get_local_ID(const wmDrag *drag, short idcode)
{
  if (drag->type != WM_DRAG_ID) {
    return nullptr;
  }

  wmDragID *drag_id = static_cast<wmDragID *>(drag->ids.first);
  if (!drag_id) {
    return nullptr;
  }

  ID *id = drag_id->id;
  return (idcode == 0 || GS(id->name) == idcode) ? id : nullptr;
}

ID *WM_drag_get_local_ID_from_event(const wmEvent *event, short idcode)
{
  if (event->custom != EVT_DATA_DRAGDROP) {
    return nullptr;
  }

  ListBase *lb = static_cast<ListBase *>(event->customdata);
  return WM_drag_get_local_ID(static_cast<const wmDrag *>(lb->first), idcode);
}

bool WM_drag_is_ID_type(const wmDrag *drag, int idcode)
{
  return WM_drag_get_local_ID(drag, idcode) || WM_drag_get_asset_data(drag, idcode);
}

wmDragAsset *WM_drag_create_asset_data(const blender::asset_system::AssetRepresentation *asset,
                                       const AssetImportSettings &import_settings)
{
  wmDragAsset *asset_drag = MEM_new<wmDragAsset>(__func__);

  asset_drag->asset = asset;
  asset_drag->import_settings = import_settings;

  return asset_drag;
}

static void wm_drag_free_asset_data(wmDragAsset **asset_data)
{
  if (*asset_data) {
    MEM_delete(*asset_data);
    *asset_data = nullptr;
  }
}

wmDragAsset *WM_drag_get_asset_data(const wmDrag *drag, int idcode)
{
  if (drag->type != WM_DRAG_ASSET) {
    return nullptr;
  }

  wmDragAsset *asset_drag = static_cast<wmDragAsset *>(drag->poin);
  ID_Type asset_idcode = asset_drag->asset->get_id_type();
  return ELEM(idcode, 0, asset_idcode) ? asset_drag : nullptr;
}

AssetMetaData *WM_drag_get_asset_meta_data(const wmDrag *drag, int idcode)
{
  wmDragAsset *drag_asset = WM_drag_get_asset_data(drag, idcode);
  if (drag_asset) {
    return &drag_asset->asset->get_metadata();
  }

  ID *local_id = WM_drag_get_local_ID(drag, idcode);
  if (local_id) {
    return local_id->asset_data;
  }

  return nullptr;
}

ID *WM_drag_asset_id_import(const bContext *C, wmDragAsset *asset_drag, const int flag_extra)
{
  /* Only support passing in limited flags. */
  BLI_assert(flag_extra == (flag_extra & FILE_AUTOSELECT));
  /* #eFileSel_Params_Flag + #eBLOLibLinkFlags */
  int flag = flag_extra | FILE_ACTIVE_COLLECTION;

  const char *name = asset_drag->asset->get_name().c_str();
  const std::string blend_path = asset_drag->asset->full_library_path();
  const ID_Type idtype = asset_drag->asset->get_id_type();
  const bool use_relative_path = asset_drag->asset->get_use_relative_path();

  if (asset_drag->import_settings.use_instance_collections) {
    flag |= BLO_LIBLINK_COLLECTION_INSTANCE;
  }

  /* FIXME: Link/Append should happens in the operator called at the end of drop process, not from
   * here. */

  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *view3d = CTX_wm_view3d(C);

  switch (eAssetImportMethod(asset_drag->import_settings.method)) {
    case ASSET_IMPORT_LINK:
      return WM_file_link_datablock(bmain,
                                    scene,
                                    view_layer,
                                    view3d,
                                    blend_path.c_str(),
                                    idtype,
                                    name,
                                    flag | (use_relative_path ? FILE_RELPATH : 0));
    case ASSET_IMPORT_PACK:
      return WM_file_link_datablock(bmain,
                                    scene,
                                    view_layer,
                                    view3d,
                                    blend_path.c_str(),
                                    idtype,
                                    name,
                                    flag | (use_relative_path ? FILE_RELPATH : 0) |
                                        BLO_LIBLINK_PACK);
    case ASSET_IMPORT_APPEND:
      return WM_file_append_datablock(bmain,
                                      scene,
                                      view_layer,
                                      view3d,
                                      blend_path.c_str(),
                                      idtype,
                                      name,
                                      flag | BLO_LIBLINK_APPEND_RECURSIVE |
                                          BLO_LIBLINK_APPEND_ASSET_DATA_CLEAR);
    case ASSET_IMPORT_APPEND_REUSE:
      return WM_file_append_datablock(
          G_MAIN,
          scene,
          view_layer,
          view3d,
          blend_path.c_str(),
          idtype,
          name,
          flag | BLO_LIBLINK_APPEND_RECURSIVE | BLO_LIBLINK_APPEND_ASSET_DATA_CLEAR |
              BLO_LIBLINK_APPEND_LOCAL_ID_REUSE | (use_relative_path ? FILE_RELPATH : 0));
  }

  BLI_assert_unreachable();
  return nullptr;
}

bool WM_drag_asset_will_import_linked(const wmDrag *drag)
{
  if (drag->type != WM_DRAG_ASSET) {
    return false;
  }

  const wmDragAsset *asset_drag = WM_drag_get_asset_data(drag, 0);
  return ELEM(asset_drag->import_settings.method, ASSET_IMPORT_LINK, ASSET_IMPORT_PACK);
}

ID *WM_drag_get_local_ID_or_import_from_asset(const bContext *C, const wmDrag *drag, int idcode)
{
  if (!ELEM(drag->type, WM_DRAG_ASSET, WM_DRAG_ID)) {
    return nullptr;
  }

  if (drag->type == WM_DRAG_ID) {
    return WM_drag_get_local_ID(drag, idcode);
  }

  wmDragAsset *asset_drag = WM_drag_get_asset_data(drag, idcode);
  if (!asset_drag) {
    return nullptr;
  }

  /* Link/append the asset. */
  return WM_drag_asset_id_import(C, asset_drag, 0);
}

void WM_drag_free_imported_drag_ID(Main *bmain, wmDrag *drag, wmDropBox *drop)
{
  if (drag->type != WM_DRAG_ASSET) {
    return;
  }

  wmDragAsset *asset_drag = WM_drag_get_asset_data(drag, 0);
  if (!asset_drag) {
    return;
  }

  ID_Type asset_id_type = asset_drag->asset->get_id_type();
  /* Try to find the imported ID. For this to work either a "session_uid" or "name" property must
   * have been defined (see #WM_operator_properties_id_lookup()). */
  ID *id = WM_operator_properties_id_lookup_from_name_or_session_uid(
      bmain, drop->ptr, asset_id_type);
  if (id != nullptr) {
    /* Do not delete the dragged ID if it has any user, otherwise if it is a 're-used' ID it will
     * cause #95636. Note that we need first to add the user that we want to remove in
     * #BKE_id_free_us. */
    id_us_plus(id);
    BKE_id_free_us(bmain, id);
  }
}

wmDragAssetCatalog *WM_drag_get_asset_catalog_data(const wmDrag *drag)
{
  if (drag->type != WM_DRAG_ASSET_CATALOG) {
    return nullptr;
  }

  return static_cast<wmDragAssetCatalog *>(drag->poin);
}

void WM_drag_add_asset_list_item(wmDrag *drag,
                                 const blender::asset_system::AssetRepresentation *asset)
{
  BLI_assert(drag->type == WM_DRAG_ASSET_LIST);

  /* No guarantee that the same asset isn't added twice. */

  /* Add to list. */
  wmDragAssetListItem *drag_asset = MEM_callocN<wmDragAssetListItem>(__func__);
  ID *local_id = asset->local_id();
  if (local_id) {
    drag_asset->is_external = false;
    drag_asset->asset_data.local_id = local_id;
  }
  else {
    drag_asset->is_external = true;

    AssetImportSettings import_settings{};
    import_settings.method = ASSET_IMPORT_APPEND;
    import_settings.use_instance_collections = false;

    drag_asset->asset_data.external_info = WM_drag_create_asset_data(asset, import_settings);
  }
  BLI_addtail(&drag->asset_items, drag_asset);
}

const ListBase *WM_drag_asset_list_get(const wmDrag *drag)
{
  if (drag->type != WM_DRAG_ASSET_LIST) {
    return nullptr;
  }

  return &drag->asset_items;
}

wmDragPath *WM_drag_create_path_data(blender::Span<const char *> paths)
{
  BLI_assert(!paths.is_empty());
  wmDragPath *path_data = MEM_new<wmDragPath>("wmDragPath");

  for (const char *path : paths) {
    path_data->paths.append(path);
    const int type_flag = ED_path_extension_type(path);
    path_data->file_types_bit_flag |= type_flag;
    path_data->file_types.append(type_flag);
  }

  path_data->tooltip = path_data->paths[0];

  if (path_data->paths.size() > 1) {
    std::string path_count = std::to_string(path_data->paths.size());
    path_data->tooltip = fmt::format(fmt::runtime(TIP_("Dragging {} files")), path_count);
  }

  return path_data;
}

static void wm_drag_free_path_data(wmDragPath **path_data)
{
  MEM_delete(*path_data);
  *path_data = nullptr;
}

const char *WM_drag_get_single_path(const wmDrag *drag)
{
  if (drag->type != WM_DRAG_PATH) {
    return nullptr;
  }

  const wmDragPath *path_data = static_cast<const wmDragPath *>(drag->poin);
  return path_data->paths[0].c_str();
}

const char *WM_drag_get_single_path(const wmDrag *drag, int file_type)
{
  if (drag->type != WM_DRAG_PATH) {
    return nullptr;
  }
  const wmDragPath *path_data = static_cast<const wmDragPath *>(drag->poin);
  const blender::Span<int> file_types = path_data->file_types;

  const auto *itr = std::find_if(
      file_types.begin(), file_types.end(), [file_type](const int file_fype_test) {
        return file_fype_test & file_type;
      });

  if (itr == file_types.end()) {
    return nullptr;
  }
  const int index = itr - file_types.begin();
  return path_data->paths[index].c_str();
}

bool WM_drag_has_path_file_type(const wmDrag *drag, int file_type)
{
  if (drag->type != WM_DRAG_PATH) {
    return false;
  }
  const wmDragPath *path_data = static_cast<const wmDragPath *>(drag->poin);
  return bool(path_data->file_types_bit_flag & file_type);
}

blender::Span<std::string> WM_drag_get_paths(const wmDrag *drag)
{
  if (drag->type != WM_DRAG_PATH) {
    return blender::Span<std::string>();
  }

  const wmDragPath *path_data = static_cast<const wmDragPath *>(drag->poin);
  return path_data->paths.as_span();
}

int WM_drag_get_path_file_type(const wmDrag *drag)
{
  if (drag->type != WM_DRAG_PATH) {
    return 0;
  }

  const wmDragPath *path_data = static_cast<const wmDragPath *>(drag->poin);
  return path_data->file_types[0];
}

const std::string &WM_drag_get_string(const wmDrag *drag)
{
  BLI_assert(drag->type == WM_DRAG_STRING);
  const std::string *str = static_cast<const std::string *>(drag->poin);
  return *str;
}

std::string WM_drag_get_string_firstline(const wmDrag *drag)
{
  BLI_assert(drag->type == WM_DRAG_STRING);
  const std::string *str = static_cast<const std::string *>(drag->poin);
  const size_t str_eol = str->find('\n');
  if (str_eol != std::string::npos) {
    return str->substr(0, str_eol);
  }
  return *str;
}

/* ************** draw ***************** */

static void wm_drop_operator_draw(const blender::StringRef name, int x, int y)
{
  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;

  /* Use the theme settings from tooltips. */
  const bTheme *btheme = UI_GetTheme();
  const uiWidgetColors *wcol = &btheme->tui.wcol_tooltip;

  float col_fg[4], col_bg[4];
  rgba_uchar_to_float(col_fg, wcol->text);
  rgba_uchar_to_float(col_bg, wcol->inner);

  UI_fontstyle_draw_simple_backdrop(fstyle, x, y, name, col_fg, col_bg);
}

static void wm_drop_redalert_draw(const blender::StringRef redalert_str, int x, int y)
{
  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
  const bTheme *btheme = UI_GetTheme();
  const uiWidgetColors *wcol = &btheme->tui.wcol_tooltip;

  float col_fg[4], col_bg[4];
  UI_GetThemeColor4fv(TH_REDALERT, col_fg);
  rgba_uchar_to_float(col_bg, wcol->inner);

  UI_fontstyle_draw_simple_backdrop(fstyle, x, y, redalert_str, col_fg, col_bg);
}

const char *WM_drag_get_item_name(wmDrag *drag)
{
  switch (drag->type) {
    case WM_DRAG_ID: {
      ID *id = WM_drag_get_local_ID(drag, 0);
      bool single = BLI_listbase_is_single(&drag->ids);

      if (single) {
        return id->name + 2;
      }
      if (id) {
        return BKE_idtype_idcode_to_name_plural(GS(id->name));
      }
      break;
    }
    case WM_DRAG_ASSET: {
      const wmDragAsset *asset_drag = WM_drag_get_asset_data(drag, 0);
      return asset_drag->asset->get_name().c_str();
    }
    case WM_DRAG_PATH: {
      const wmDragPath *path_drag_data = static_cast<const wmDragPath *>(drag->poin);
      return path_drag_data->tooltip.c_str();
    }
    case WM_DRAG_NAME:
      return static_cast<const char *>(drag->poin);
    default:
      break;
  }
  return "";
}

static int wm_drag_imbuf_icon_width_get(const wmDrag *drag)
{
  return round_fl_to_int(drag->imb->x * drag->imbuf_scale);
}

static int wm_drag_imbuf_icon_height_get(const wmDrag *drag)
{
  return round_fl_to_int(drag->imb->y * drag->imbuf_scale);
}

static int wm_drag_preview_icon_size_get()
{
  return int(PREVIEW_DRAG_DRAW_SIZE * UI_SCALE_FAC);
}

static void wm_drag_draw_icon(bContext * /*C*/, wmWindow * /*win*/, wmDrag *drag, const int xy[2])
{
  int x, y;

  if (const int64_t path_count = WM_drag_get_paths(drag).size(); path_count > 1) {
    /* Custom scale to improve path count readability. */
    const float scale = UI_SCALE_FAC * 1.15f;
    x = xy[0] - int(8.0f * scale);
    y = xy[1] - int(scale);
    const uchar text_col[] = {255, 255, 255, 255};
    IconTextOverlay text_overlay;
    UI_icon_text_overlay_init_from_count(&text_overlay, path_count);
    UI_icon_draw_ex(
        x, y, ICON_DOCUMENTS, 1.0f / scale, 1.0f, 0.0f, text_col, false, &text_overlay);
  }
  else if (drag->imb) {
    /* This could also get the preview image of an ID when dragging one. But the big preview icon
     * may actually not always be wanted, for example when dragging objects in the Outliner it gets
     * in the way). So make the drag user set an image buffer explicitly (e.g. through
     * #UI_but_drag_attach_image()). */

    x = xy[0] - (wm_drag_imbuf_icon_width_get(drag) / 2);
    y = xy[1] - (wm_drag_imbuf_icon_height_get(drag) / 2);

    const float col[4] = {1.0f, 1.0f, 1.0f, 0.65f}; /* This blends texture. */
    IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_3D_IMAGE_COLOR);
    immDrawPixelsTexTiled_scaling(&state,
                                  x,
                                  y,
                                  drag->imb->x,
                                  drag->imb->y,
                                  blender::gpu::TextureFormat::UNORM_8_8_8_8,
                                  false,
                                  drag->imb->byte_buffer.data,
                                  drag->imbuf_scale,
                                  drag->imbuf_scale,
                                  1.0f,
                                  1.0f,
                                  col);
  }
  else if (drag->preview_icon_id) {
    const int size = wm_drag_preview_icon_size_get();
    x = xy[0] - (size / 2);
    y = xy[1] - (size / 2);

    UI_icon_draw_preview(x, y, drag->preview_icon_id, 1.0, 0.8, size);
  }
  else {
    int padding = 4 * UI_SCALE_FAC;
    x = xy[0] - 2 * padding;
    y = xy[1] - 2 * UI_SCALE_FAC;

    const uchar text_col[] = {255, 255, 255, 255};
    UI_icon_draw_ex(
        x, y, drag->icon, UI_INV_SCALE_FAC, 0.8, 0.0f, text_col, false, UI_NO_ICON_OVERLAY_TEXT);
  }
}

static void wm_drag_draw_item_name(wmDrag *drag, const int x, const int y)
{
  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
  const uchar text_col[] = {255, 255, 255, 255};
  UI_fontstyle_draw_simple(fstyle, x, y, WM_drag_get_item_name(drag), text_col);
}

void WM_drag_draw_item_name_fn(bContext * /*C*/, wmWindow *win, wmDrag *drag, const int xy[2])
{
  int x = xy[0] + 10 * UI_SCALE_FAC;
  int y = xy[1] + 1 * UI_SCALE_FAC;

  /* Needs zero offset here or it looks blurry. #128112. */
  wmWindowViewport_ex(win, 0.0f);
  wm_drag_draw_item_name(drag, x, y);
}

static void wm_drag_draw_tooltip(bContext *C, wmWindow *win, wmDrag *drag, const int xy[2])
{
  if (!CTX_wm_region(C)) {
    /* Some callbacks require the region. */
    return;
  }
  int iconsize = UI_ICON_SIZE;
  int padding = 4 * UI_SCALE_FAC;
  blender::StringRef tooltip = drag->drop_state.tooltip;
  const bool has_disabled_info = drag->drop_state.disabled_info &&
                                 drag->drop_state.disabled_info[0];
  if (tooltip.is_empty() && !has_disabled_info) {
    return;
  }

  const int winsize_y = WM_window_native_pixel_y(win);
  int x, y;
  if (drag->imb) {
    const int icon_width = wm_drag_imbuf_icon_width_get(drag);
    const int icon_height = wm_drag_imbuf_icon_height_get(drag);

    x = xy[0] - (icon_width / 2);

    if (xy[1] + (icon_height / 2) + padding + iconsize < winsize_y) {
      y = xy[1] + (icon_height / 2) + padding;
    }
    else {
      y = xy[1] - (icon_height / 2) - padding - iconsize - padding - iconsize;
    }
  }
  if (WM_drag_get_paths(drag).size() > 1) {
    x = xy[0] - 2 * padding;

    if (xy[1] + 2 * 1.15 * iconsize < winsize_y) {
      y = xy[1] + 1.15f * (iconsize + 6 * UI_SCALE_FAC);
    }
    else {
      y = xy[1] - 1.15f * (iconsize + padding);
    }
  }
  else if (drag->preview_icon_id) {
    const int size = wm_drag_preview_icon_size_get();

    x = xy[0] - (size / 2);

    if (xy[1] + (size / 2) + padding + iconsize < winsize_y) {
      y = xy[1] + (size / 2) + padding;
    }
    else {
      y = xy[1] - (size / 2) - padding - iconsize - padding - iconsize;
    }
  }
  else {
    x = xy[0] - 2 * padding;

    if (xy[1] + iconsize + iconsize < winsize_y) {
      y = (xy[1] + iconsize) + padding;
    }
    else {
      y = (xy[1] - iconsize) - padding;
    }
  }

  if (!tooltip.is_empty()) {
    wm_drop_operator_draw(tooltip, x, y);
  }
  else if (has_disabled_info) {
    wm_drop_redalert_draw(drag->drop_state.disabled_info, x, y);
  }
}

static void wm_drag_draw_default(bContext *C, wmWindow *win, wmDrag *drag, const int xy[2])
{
  int xy_tmp[2] = {UNPACK2(xy)};

  /* Image or icon. */
  wm_drag_draw_icon(C, win, drag, xy_tmp);

  /* Item name. */
  if (drag->imb) {
    int iconsize = UI_ICON_SIZE;
    xy_tmp[0] = xy[0] - (wm_drag_imbuf_icon_width_get(drag) / 2);
    xy_tmp[1] = xy[1] - (wm_drag_imbuf_icon_height_get(drag) / 2) - iconsize;
  }
  else if (drag->preview_icon_id) {
    const int icon_size = UI_ICON_SIZE;
    const int preview_size = wm_drag_preview_icon_size_get();
    xy_tmp[0] = xy[0] - (preview_size / 2);
    xy_tmp[1] = xy[1] - (preview_size / 2) - icon_size;
  }
  else {
    xy_tmp[0] = xy[0] + 10 * UI_SCALE_FAC;
    xy_tmp[1] = xy[1] + 1 * UI_SCALE_FAC;
  }
  if (WM_drag_get_paths(drag).size() < 2) {
    wm_drag_draw_item_name(drag, UNPACK2(xy_tmp));
  }

  /* Operator name with round-box. */
  wm_drag_draw_tooltip(C, win, drag, xy);
}

void WM_drag_draw_default_fn(bContext *C, wmWindow *win, wmDrag *drag, const int xy[2])
{
  wm_drag_draw_default(C, win, drag, xy);
}

void wm_drags_draw(bContext *C, wmWindow *win)
{
  const int *xy = win->eventstate->xy;

  int xy_buf[2];
  if (ELEM(win->grabcursor, GHOST_kGrabWrap, GHOST_kGrabHide) &&
      wm_cursor_position_get(win, &xy_buf[0], &xy_buf[1]))
  {
    xy = xy_buf;
  }

  bScreen *screen = CTX_wm_screen(C);
  /* To start with, use the area and region under the mouse cursor, just like event handling. The
   * operator context may still override it. */
  ScrArea *area = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, xy);
  ARegion *region = ED_area_find_region_xy_visual(area, RGN_TYPE_ANY, xy);
  /* Will be overridden and unset eventually. */
  BLI_assert(!CTX_wm_area(C) && !CTX_wm_region(C));

  wmWindowManager *wm = CTX_wm_manager(C);

  /* Should we support multi-line drag draws? Maybe not, more types mixed won't work well. */
  GPU_blend(GPU_BLEND_ALPHA);
  LISTBASE_FOREACH (wmDrag *, drag, &wm->runtime->drags) {
    if (drag->drop_state.active_dropbox) {
      CTX_wm_area_set(C, drag->drop_state.area_from);
      CTX_wm_region_set(C, drag->drop_state.region_from);
      CTX_store_set(C, drag->drop_state.ui_context.get());

      if (region && drag->drop_state.active_dropbox->draw_in_view) {
        wmViewport(&region->winrct);
        drag->drop_state.active_dropbox->draw_in_view(C, win, drag, xy);
        wmWindowViewport(win);
      }

      /* Drawing should be allowed to assume the context from handling and polling (that's why we
       * restore it above). */
      if (drag->drop_state.active_dropbox->draw_droptip) {
        drag->drop_state.active_dropbox->draw_droptip(C, win, drag, xy);
        continue;
      }
    }
    else if (region) {
      CTX_wm_area_set(C, area);
      CTX_wm_region_set(C, region);
    }

    /* Needs zero offset here or it looks blurry. #128112. */
    wmWindowViewport_ex(win, 0.0f);
    wm_drag_draw_default(C, win, drag, xy);
  }
  GPU_blend(GPU_BLEND_NONE);
  CTX_wm_area_set(C, nullptr);
  CTX_wm_region_set(C, nullptr);
  CTX_store_set(C, nullptr);
}
