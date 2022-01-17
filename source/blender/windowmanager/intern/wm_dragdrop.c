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
 *
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 *
 * Our own drag-and-drop, drag state and drop boxes.
 */

#include <string.h>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLT_translation.h"

#include "BLI_blenlib.h"
#include "BLI_math_color.h"

#include "BIF_glutil.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_screen.h"

#include "GHOST_C-api.h"

#include "BLO_readfile.h"

#include "ED_asset.h"
#include "ED_screen.h"

#include "GPU_shader.h"
#include "GPU_state.h"
#include "GPU_viewport.h"

#include "IMB_imbuf_types.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_event_system.h"
#include "wm_window.h"

/* ****************************************************** */

static ListBase dropboxes = {NULL, NULL};

static void wm_drag_free_asset_data(wmDragAsset **asset_data);

/* drop box maps are stored global for now */
/* these are part of blender's UI/space specs, and not like keymaps */
/* when editors become configurable, they can add own dropbox definitions */

typedef struct wmDropBoxMap {
  struct wmDropBoxMap *next, *prev;

  ListBase dropboxes;
  short spaceid, regionid;
  char idname[KMAP_MAX_NAME];

} wmDropBoxMap;

ListBase *WM_dropboxmap_find(const char *idname, int spaceid, int regionid)
{
  LISTBASE_FOREACH (wmDropBoxMap *, dm, &dropboxes) {
    if (dm->spaceid == spaceid && dm->regionid == regionid) {
      if (STREQLEN(idname, dm->idname, KMAP_MAX_NAME)) {
        return &dm->dropboxes;
      }
    }
  }

  wmDropBoxMap *dm = MEM_callocN(sizeof(struct wmDropBoxMap), "dropmap list");
  BLI_strncpy(dm->idname, idname, KMAP_MAX_NAME);
  dm->spaceid = spaceid;
  dm->regionid = regionid;
  BLI_addtail(&dropboxes, dm);

  return &dm->dropboxes;
}

wmDropBox *WM_dropbox_add(ListBase *lb,
                          const char *idname,
                          bool (*poll)(bContext *, wmDrag *, const wmEvent *),
                          void (*copy)(wmDrag *, wmDropBox *),
                          void (*cancel)(struct Main *, wmDrag *, wmDropBox *),
                          WMDropboxTooltipFunc tooltip)
{
  wmDropBox *drop = MEM_callocN(sizeof(wmDropBox), "wmDropBox");
  drop->poll = poll;
  drop->copy = copy;
  drop->cancel = cancel;
  drop->tooltip = tooltip;
  drop->ot = WM_operatortype_find(idname, 0);

  if (drop->ot == NULL) {
    MEM_freeN(drop);
    printf("Error: dropbox with unknown operator: %s\n", idname);
    return NULL;
  }
  WM_operator_properties_alloc(&(drop->ptr), &(drop->properties), idname);

  BLI_addtail(lb, drop);

  return drop;
}

void wm_dropbox_free(void)
{

  LISTBASE_FOREACH (wmDropBoxMap *, dm, &dropboxes) {
    LISTBASE_FOREACH (wmDropBox *, drop, &dm->dropboxes) {
      if (drop->ptr) {
        WM_operator_properties_free(drop->ptr);
        MEM_freeN(drop->ptr);
      }
    }
    BLI_freelistN(&dm->dropboxes);
  }

  BLI_freelistN(&dropboxes);
}

/* *********************************** */

wmDrag *WM_event_start_drag(
    struct bContext *C, int icon, int type, void *poin, double value, unsigned int flags)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmDrag *drag = MEM_callocN(sizeof(struct wmDrag), "new drag");

  /* Keep track of future multi-touch drag too, add a mouse-pointer id or so. */
  /* if multiple drags are added, they're drawn as list */

  BLI_addtail(&wm->drags, drag);
  drag->flags = flags;
  drag->icon = icon;
  drag->type = type;
  switch (type) {
    case WM_DRAG_PATH:
      BLI_strncpy(drag->path, poin, FILE_MAX);
      /* As the path is being copied, free it immediately as `drag` won't "own" the data. */
      if (flags & WM_DRAG_FREE_DATA) {
        MEM_freeN(poin);
      }
      break;
    case WM_DRAG_ID:
      if (poin) {
        WM_drag_add_local_ID(drag, poin, NULL);
      }
      break;
    case WM_DRAG_ASSET:
    case WM_DRAG_ASSET_CATALOG:
      /* Move ownership of poin to wmDrag. */
      drag->poin = poin;
      drag->flags |= WM_DRAG_FREE_DATA;
      break;
      /* The asset-list case is special: We get multiple assets from context and attach them to the
       * drag item. */
    case WM_DRAG_ASSET_LIST: {
      const AssetLibraryReference *asset_library = CTX_wm_asset_library_ref(C);
      ListBase asset_file_links = CTX_data_collection_get(C, "selected_asset_files");
      LISTBASE_FOREACH (const CollectionPointerLink *, link, &asset_file_links) {
        const FileDirEntry *asset_file = link->ptr.data;
        const AssetHandle asset_handle = {asset_file};
        WM_drag_add_asset_list_item(drag, C, asset_library, &asset_handle);
      }
      BLI_freelistN(&asset_file_links);
      break;
    }
    default:
      drag->poin = poin;
      break;
  }
  drag->value = value;

  return drag;
}

void wm_drags_exit(wmWindowManager *wm, wmWindow *win)
{
  bool any_active = false;
  LISTBASE_FOREACH (const wmDrag *, drag, &wm->drags) {
    if (drag->drop_state.active_dropbox) {
      any_active = true;
      break;
    }
  }

  /* If there is no active drop-box #wm_drags_check_ops() set a stop-cursor, which needs to be
   * restored. */
  if (!any_active) {
    WM_cursor_modal_restore(win);
    /* Ensure the correct area cursor is restored. */
    win->tag_cursor_refresh = true;
    WM_event_add_mousemove(win);
  }
}

void WM_event_drag_image(wmDrag *drag, ImBuf *imb, float scale, int sx, int sy)
{
  drag->imb = imb;
  drag->scale = scale;
  drag->sx = sx;
  drag->sy = sy;
}

void WM_drag_data_free(int dragtype, void *poin)
{
  /* Don't require all the callers to have a NULL-check, just allow passing NULL. */
  if (!poin) {
    return;
  }

  /* Not too nice, could become a callback. */
  if (dragtype == WM_DRAG_ASSET) {
    wmDragAsset *asset_data = poin;
    wm_drag_free_asset_data(&asset_data);
  }
  else {
    MEM_freeN(poin);
  }
}

void WM_drag_free(wmDrag *drag)
{
  if (drag->drop_state.active_dropbox && drag->drop_state.active_dropbox->draw_deactivate) {
    drag->drop_state.active_dropbox->draw_deactivate(drag->drop_state.active_dropbox, drag);
  }
  if (drag->flags & WM_DRAG_FREE_DATA) {
    WM_drag_data_free(drag->type, drag->poin);
  }
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
  MEM_freeN(drag);
}

void WM_drag_free_list(struct ListBase *lb)
{
  wmDrag *drag;
  while ((drag = BLI_pophead(lb))) {
    WM_drag_free(drag);
  }
}

static char *dropbox_tooltip(bContext *C, wmDrag *drag, const int xy[2], wmDropBox *drop)
{
  char *tooltip = NULL;
  if (drop->tooltip) {
    tooltip = drop->tooltip(C, drag, xy, drop);
  }
  if (!tooltip) {
    tooltip = BLI_strdup(WM_operatortype_name(drop->ot, drop->ptr));
  }
  /* XXX Doing translation here might not be ideal, but later we have no more
   *     access to ot (and hence op context)... */
  return tooltip;
}

static wmDropBox *dropbox_active(bContext *C,
                                 ListBase *handlers,
                                 wmDrag *drag,
                                 const wmEvent *event)
{
  if (drag->drop_state.free_disabled_info) {
    MEM_SAFE_FREE(drag->drop_state.disabled_info);
  }
  drag->drop_state.disabled_info = NULL;

  LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers) {
    if (handler_base->type == WM_HANDLER_TYPE_DROPBOX) {
      wmEventHandler_Dropbox *handler = (wmEventHandler_Dropbox *)handler_base;
      if (handler->dropboxes) {
        LISTBASE_FOREACH (wmDropBox *, drop, handler->dropboxes) {
          if (!drop->poll(C, drag, event)) {
            /* If the drop's poll fails, don't set the disabled-info. This would be too aggressive.
             * Instead show it only if the drop box could be used in principle, but the operator
             * can't be executed. */
            continue;
          }

          const wmOperatorCallContext opcontext = wm_drop_operator_context_get(drop);
          if (WM_operator_poll_context(C, drop->ot, opcontext)) {
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
  return NULL;
}

/* return active operator tooltip/name when mouse is in box */
static wmDropBox *wm_dropbox_active(bContext *C, wmDrag *drag, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  wmDropBox *drop = dropbox_active(C, &win->handlers, drag, event);
  if (!drop) {
    ScrArea *area = CTX_wm_area(C);
    drop = dropbox_active(C, &area->handlers, drag, event);
  }
  if (!drop) {
    ARegion *region = CTX_wm_region(C);
    drop = dropbox_active(C, &region->handlers, drag, event);
  }
  return drop;
}

/**
 * Update dropping information for the current mouse position in \a event.
 */
static void wm_drop_update_active(bContext *C, wmDrag *drag, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  const int winsize_x = WM_window_pixels_x(win);
  const int winsize_y = WM_window_pixels_y(win);

  /* for multiwin drags, we only do this if mouse inside */
  if (event->xy[0] < 0 || event->xy[1] < 0 || event->xy[0] > winsize_x ||
      event->xy[1] > winsize_y) {
    return;
  }

  wmDropBox *drop_prev = drag->drop_state.active_dropbox;
  wmDropBox *drop = wm_dropbox_active(C, drag, event);
  if (drop != drop_prev) {
    if (drop_prev && drop_prev->draw_deactivate) {
      drop_prev->draw_deactivate(drop_prev, drag);
      BLI_assert(drop_prev->draw_data == NULL);
    }
    if (drop && drop->draw_activate) {
      drop->draw_activate(drop, drag);
    }
    drag->drop_state.active_dropbox = drop;
    drag->drop_state.area_from = drop ? CTX_wm_area(C) : NULL;
    drag->drop_state.region_from = drop ? CTX_wm_region(C) : NULL;
  }
}

void wm_drop_prepare(bContext *C, wmDrag *drag, wmDropBox *drop)
{
  const wmOperatorCallContext opcontext = wm_drop_operator_context_get(drop);
  /* Optionally copy drag information to operator properties. Don't call it if the
   * operator fails anyway, it might do more than just set properties (e.g.
   * typically import an asset). */
  if (drop->copy && WM_operator_poll_context(C, drop->ot, opcontext)) {
    drop->copy(drag, drop);
  }

  wm_drags_exit(CTX_wm_manager(C), CTX_wm_window(C));
}

void wm_drags_check_ops(bContext *C, const wmEvent *event)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  bool any_active = false;
  LISTBASE_FOREACH (wmDrag *, drag, &wm->drags) {
    wm_drop_update_active(C, drag, event);

    if (drag->drop_state.active_dropbox) {
      any_active = true;
    }
  }

  /* Change the cursor to display that dropping isn't possible here. But only if there is something
   * being dragged actually. Cursor will be restored in #wm_drags_exit(). */
  if (!BLI_listbase_is_empty(&wm->drags)) {
    WM_cursor_modal_set(CTX_wm_window(C), any_active ? WM_CURSOR_DEFAULT : WM_CURSOR_STOP);
  }
}

wmOperatorCallContext wm_drop_operator_context_get(const wmDropBox *UNUSED(drop))
{
  return WM_OP_INVOKE_DEFAULT;
}

/* ************** IDs ***************** */

void WM_drag_add_local_ID(wmDrag *drag, ID *id, ID *from_parent)
{
  /* Don't drag the same ID twice. */
  LISTBASE_FOREACH (wmDragID *, drag_id, &drag->ids) {
    if (drag_id->id == id) {
      if (drag_id->from_parent == NULL) {
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
  wmDragID *drag_id = MEM_callocN(sizeof(wmDragID), __func__);
  drag_id->id = id;
  drag_id->from_parent = from_parent;
  BLI_addtail(&drag->ids, drag_id);
}

ID *WM_drag_get_local_ID(const wmDrag *drag, short idcode)
{
  if (drag->type != WM_DRAG_ID) {
    return NULL;
  }

  wmDragID *drag_id = drag->ids.first;
  if (!drag_id) {
    return NULL;
  }

  ID *id = drag_id->id;
  return (idcode == 0 || GS(id->name) == idcode) ? id : NULL;
}

ID *WM_drag_get_local_ID_from_event(const wmEvent *event, short idcode)
{
  if (event->custom != EVT_DATA_DRAGDROP) {
    return NULL;
  }

  ListBase *lb = event->customdata;
  return WM_drag_get_local_ID(lb->first, idcode);
}

bool WM_drag_is_ID_type(const wmDrag *drag, int idcode)
{
  return WM_drag_get_local_ID(drag, idcode) || WM_drag_get_asset_data(drag, idcode);
}

wmDragAsset *WM_drag_create_asset_data(const AssetHandle *asset,
                                       AssetMetaData *metadata,
                                       const char *path,
                                       int import_type)
{
  wmDragAsset *asset_drag = MEM_mallocN(sizeof(*asset_drag), "wmDragAsset");

  BLI_strncpy(asset_drag->name, ED_asset_handle_get_name(asset), sizeof(asset_drag->name));
  asset_drag->metadata = metadata;
  asset_drag->path = path;
  asset_drag->id_type = ED_asset_handle_get_id_type(asset);
  asset_drag->import_type = import_type;

  return asset_drag;
}

static void wm_drag_free_asset_data(wmDragAsset **asset_data)
{
  MEM_freeN((char *)(*asset_data)->path);
  MEM_SAFE_FREE(*asset_data);
}

wmDragAsset *WM_drag_get_asset_data(const wmDrag *drag, int idcode)
{
  if (drag->type != WM_DRAG_ASSET) {
    return NULL;
  }

  wmDragAsset *asset_drag = drag->poin;
  return (ELEM(idcode, 0, asset_drag->id_type)) ? asset_drag : NULL;
}

struct AssetMetaData *WM_drag_get_asset_meta_data(const wmDrag *drag, int idcode)
{
  wmDragAsset *drag_asset = WM_drag_get_asset_data(drag, idcode);
  if (drag_asset) {
    return drag_asset->metadata;
  }

  ID *local_id = WM_drag_get_local_ID(drag, idcode);
  if (local_id) {
    return local_id->asset_data;
  }

  return NULL;
}

ID *WM_drag_asset_id_import(wmDragAsset *asset_drag, const int flag_extra)
{
  /* Only support passing in limited flags. */
  BLI_assert(flag_extra == (flag_extra & FILE_AUTOSELECT));
  eFileSel_Params_Flag flag = flag_extra | FILE_ACTIVE_COLLECTION;

  const char *name = asset_drag->name;
  ID_Type idtype = asset_drag->id_type;

  /* FIXME: Link/Append should happens in the operator called at the end of drop process, not from
   * here. */

  Main *bmain = CTX_data_main(asset_drag->evil_C);
  Scene *scene = CTX_data_scene(asset_drag->evil_C);
  ViewLayer *view_layer = CTX_data_view_layer(asset_drag->evil_C);
  View3D *view3d = CTX_wm_view3d(asset_drag->evil_C);

  switch ((eFileAssetImportType)asset_drag->import_type) {
    case FILE_ASSET_IMPORT_LINK:
      return WM_file_link_datablock(
          bmain, scene, view_layer, view3d, asset_drag->path, idtype, name, flag);
    case FILE_ASSET_IMPORT_APPEND:
      return WM_file_append_datablock(bmain,
                                      scene,
                                      view_layer,
                                      view3d,
                                      asset_drag->path,
                                      idtype,
                                      name,
                                      flag | BLO_LIBLINK_APPEND_RECURSIVE |
                                          BLO_LIBLINK_APPEND_ASSET_DATA_CLEAR);
    case FILE_ASSET_IMPORT_APPEND_REUSE:
      return WM_file_append_datablock(G_MAIN,
                                      scene,
                                      view_layer,
                                      view3d,
                                      asset_drag->path,
                                      idtype,
                                      name,
                                      flag | BLO_LIBLINK_APPEND_RECURSIVE |
                                          BLO_LIBLINK_APPEND_ASSET_DATA_CLEAR |
                                          BLO_LIBLINK_APPEND_LOCAL_ID_REUSE);
  }

  BLI_assert_unreachable();
  return NULL;
}

bool WM_drag_asset_will_import_linked(const wmDrag *drag)
{
  if (drag->type != WM_DRAG_ASSET) {
    return false;
  }

  const wmDragAsset *asset_drag = WM_drag_get_asset_data(drag, 0);
  return asset_drag->import_type == FILE_ASSET_IMPORT_LINK;
}

ID *WM_drag_get_local_ID_or_import_from_asset(const wmDrag *drag, int idcode)
{
  if (!ELEM(drag->type, WM_DRAG_ASSET, WM_DRAG_ID)) {
    return NULL;
  }

  if (drag->type == WM_DRAG_ID) {
    return WM_drag_get_local_ID(drag, idcode);
  }

  wmDragAsset *asset_drag = WM_drag_get_asset_data(drag, idcode);
  if (!asset_drag) {
    return NULL;
  }

  /* Link/append the asset. */
  return WM_drag_asset_id_import(asset_drag, 0);
}

void WM_drag_free_imported_drag_ID(struct Main *bmain, wmDrag *drag, wmDropBox *drop)
{
  if (drag->type != WM_DRAG_ASSET) {
    return;
  }

  wmDragAsset *asset_drag = WM_drag_get_asset_data(drag, 0);
  if (!asset_drag) {
    return;
  }

  /* Get name from property, not asset data - it may have changed after importing to ensure
   * uniqueness (name is assumed to be set from the imported ID name). */
  char name[MAX_ID_NAME - 2];
  RNA_string_get(drop->ptr, "name", name);
  if (!name[0]) {
    return;
  }

  ID *id = BKE_libblock_find_name(bmain, asset_drag->id_type, name);
  if (id) {
    BKE_id_delete(bmain, id);
  }
}

wmDragAssetCatalog *WM_drag_get_asset_catalog_data(const wmDrag *drag)
{
  if (drag->type != WM_DRAG_ASSET_CATALOG) {
    return NULL;
  }

  return drag->poin;
}

void WM_drag_add_asset_list_item(
    wmDrag *drag,
    /* Context only needed for the hack in #ED_asset_handle_get_full_library_path(). */
    const bContext *C,
    const AssetLibraryReference *asset_library_ref,
    const AssetHandle *asset)
{
  if (drag->type != WM_DRAG_ASSET_LIST) {
    return;
  }

  /* No guarantee that the same asset isn't added twice. */

  /* Add to list. */
  wmDragAssetListItem *drag_asset = MEM_callocN(sizeof(*drag_asset), __func__);
  ID *local_id = ED_asset_handle_get_local_id(asset);
  if (local_id) {
    drag_asset->is_external = false;
    drag_asset->asset_data.local_id = local_id;
  }
  else {
    AssetMetaData *metadata = ED_asset_handle_get_metadata(asset);
    char asset_blend_path[FILE_MAX_LIBEXTRA];
    ED_asset_handle_get_full_library_path(C, asset_library_ref, asset, asset_blend_path);
    drag_asset->is_external = true;
    drag_asset->asset_data.external_info = WM_drag_create_asset_data(
        asset, metadata, BLI_strdup(asset_blend_path), FILE_ASSET_IMPORT_APPEND);
  }
  BLI_addtail(&drag->asset_items, drag_asset);
}

const ListBase *WM_drag_asset_list_get(const wmDrag *drag)
{
  if (drag->type != WM_DRAG_ASSET_LIST) {
    return NULL;
  }

  return &drag->asset_items;
}

/* ************** draw ***************** */

static void wm_drop_operator_draw(const char *name, int x, int y)
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

static void wm_drop_redalert_draw(const char *redalert_str, int x, int y)
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
      bool single = (BLI_listbase_count_at_most(&drag->ids, 2) == 1);

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
      return asset_drag->name;
    }
    case WM_DRAG_PATH:
    case WM_DRAG_NAME:
      return drag->path;
  }
  return "";
}

static void wm_drag_draw_icon(bContext *UNUSED(C),
                              wmWindow *UNUSED(win),
                              wmDrag *drag,
                              const int xy[2])
{
  int x, y;

  /* This could also get the preview image of an ID when dragging one. But the big preview icon may
   * actually not always be wanted, for example when dragging objects in the Outliner it gets in
   * the way). So make the drag user set an image buffer explicitly (e.g. through
   * #UI_but_drag_attach_image()). */

  if (drag->imb) {
    x = xy[0] - drag->sx / 2;
    y = xy[1] - drag->sy / 2;

    float col[4] = {1.0f, 1.0f, 1.0f, 0.65f}; /* this blends texture */
    IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_COLOR);
    immDrawPixelsTexTiled_scaling(&state,
                                  x,
                                  y,
                                  drag->imb->x,
                                  drag->imb->y,
                                  GPU_RGBA8,
                                  false,
                                  drag->imb->rect,
                                  drag->scale,
                                  drag->scale,
                                  1.0f,
                                  1.0f,
                                  col);
  }
  else {
    int padding = 4 * UI_DPI_FAC;
    x = xy[0] - 2 * padding;
    y = xy[1] - 2 * UI_DPI_FAC;

    const uchar text_col[] = {255, 255, 255, 255};
    UI_icon_draw_ex(x, y, drag->icon, U.inv_dpi_fac, 0.8, 0.0f, text_col, false);
  }
}

static void wm_drag_draw_item_name(wmDrag *drag, const int x, const int y)
{
  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
  const uchar text_col[] = {255, 255, 255, 255};
  UI_fontstyle_draw_simple(fstyle, x, y, WM_drag_get_item_name(drag), text_col);
}

void WM_drag_draw_item_name_fn(bContext *UNUSED(C),
                               wmWindow *UNUSED(win),
                               wmDrag *drag,
                               const int xy[2])
{
  int x = xy[0] + 10 * UI_DPI_FAC;
  int y = xy[1] + 1 * UI_DPI_FAC;

  wm_drag_draw_item_name(drag, x, y);
}

static void wm_drag_draw_tooltip(bContext *C, wmWindow *win, wmDrag *drag, const int xy[2])
{
  if (!CTX_wm_region(C)) {
    /* Some callbacks require the region. */
    return;
  }
  int iconsize = UI_DPI_ICON_SIZE;
  int padding = 4 * UI_DPI_FAC;

  char *tooltip = NULL;
  if (drag->drop_state.active_dropbox) {
    tooltip = dropbox_tooltip(C, drag, xy, drag->drop_state.active_dropbox);
  }

  const bool has_disabled_info = drag->drop_state.disabled_info &&
                                 drag->drop_state.disabled_info[0];
  if (!tooltip && !has_disabled_info) {
    return;
  }

  const int winsize_y = WM_window_pixels_y(win);
  int x, y;
  if (drag->imb) {
    x = xy[0] - drag->sx / 2;

    if (xy[1] + drag->sy / 2 + padding + iconsize < winsize_y) {
      y = xy[1] + drag->sy / 2 + padding;
    }
    else {
      y = xy[1] - drag->sy / 2 - padding - iconsize - padding - iconsize;
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

  if (tooltip) {
    wm_drop_operator_draw(tooltip, x, y);
    MEM_freeN(tooltip);
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
    int iconsize = UI_DPI_ICON_SIZE;
    xy_tmp[0] = xy[0] - (drag->sx / 2);
    xy_tmp[1] = xy[1] - (drag->sy / 2) - iconsize;
  }
  else {
    xy_tmp[0] = xy[0] + 10 * UI_DPI_FAC;
    xy_tmp[1] = xy[1] + 1 * UI_DPI_FAC;
  }
  wm_drag_draw_item_name(drag, UNPACK2(xy_tmp));

  /* Operator name with roundbox. */
  wm_drag_draw_tooltip(C, win, drag, xy);
}

void WM_drag_draw_default_fn(bContext *C, wmWindow *win, wmDrag *drag, const int xy[2])
{
  wm_drag_draw_default(C, win, drag, xy);
}

void wm_drags_draw(bContext *C, wmWindow *win)
{
  int xy[2];
  if (ELEM(win->grabcursor, GHOST_kGrabWrap, GHOST_kGrabHide)) {
    wm_cursor_position_get(win, &xy[0], &xy[1]);
  }
  else {
    xy[0] = win->eventstate->xy[0];
    xy[1] = win->eventstate->xy[1];
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
  LISTBASE_FOREACH (wmDrag *, drag, &wm->drags) {
    if (drag->drop_state.active_dropbox) {
      CTX_wm_area_set(C, drag->drop_state.area_from);
      CTX_wm_region_set(C, drag->drop_state.region_from);

      /* Drawing should be allowed to assume the context from handling and polling (that's why we
       * restore it above). */
      if (drag->drop_state.active_dropbox->draw) {
        drag->drop_state.active_dropbox->draw(C, win, drag, xy);
        continue;
      }
    }
    else if (region) {
      CTX_wm_area_set(C, area);
      CTX_wm_region_set(C, region);
    }

    wm_drag_draw_default(C, win, drag, xy);
  }
  GPU_blend(GPU_BLEND_NONE);
  CTX_wm_area_set(C, NULL);
  CTX_wm_region_set(C, NULL);
}
