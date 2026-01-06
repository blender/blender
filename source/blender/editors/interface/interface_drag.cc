/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "AS_asset_representation.hh"

#include "ED_asset.hh"

#include "WM_api.hh"

#include "interface_intern.hh"

namespace blender::ui {

void button_drag_set_id(Button *but, ID *id)
{
  but->dragtype = WM_DRAG_ID;
  if (but->dragflag & BUT_DRAGPOIN_FREE) {
    WM_drag_data_free(but->dragtype, but->dragpoin);
    but->dragflag &= ~BUT_DRAGPOIN_FREE;
  }
  but->dragpoin = static_cast<void *>(id);
}

void button_drag_attach_image(Button *but, const ImBuf *imb, const float scale)
{
  but->imb = imb;
  but->imb_scale = scale;
  button_dragflag_enable(but, BUT_DRAG_FULL_BUT);
}

void button_drag_set_asset(Button *but,
                           const asset_system::AssetRepresentation *asset,
                           const AssetImportSettings &import_settings,
                           BIFIconID icon,
                           BIFIconID preview_icon)
{
  wmDragAsset *asset_drag = WM_drag_create_asset_data(asset, import_settings);

  but->dragtype = WM_DRAG_ASSET;
  def_but_icon(but, icon, 0); /* no flag UI_HAS_ICON, so icon doesn't draw in button */
  if (but->dragflag & BUT_DRAGPOIN_FREE) {
    WM_drag_data_free(but->dragtype, but->dragpoin);
  }
  but->dragpoin = asset_drag;
  but->dragflag |= BUT_DRAGPOIN_FREE;
  but->drag_preview_icon_id = preview_icon;
}

void button_drag_set_rna(Button *but, PointerRNA *ptr)
{
  but->dragtype = WM_DRAG_RNA;
  if (but->dragflag & BUT_DRAGPOIN_FREE) {
    WM_drag_data_free(but->dragtype, but->dragpoin);
    but->dragflag &= ~BUT_DRAGPOIN_FREE;
  }
  but->dragpoin = static_cast<void *>(ptr);
}

void button_drag_set_path(Button *but, const char *path)
{
  but->dragtype = WM_DRAG_PATH;
  if (but->dragflag & BUT_DRAGPOIN_FREE) {
    WM_drag_data_free(but->dragtype, but->dragpoin);
  }
  but->dragpoin = WM_drag_create_path_data(Span(&path, 1));
  but->dragflag |= BUT_DRAGPOIN_FREE;
}

void button_drag_set_name(Button *but, const char *name)
{
  but->dragtype = WM_DRAG_NAME;
  if (but->dragflag & BUT_DRAGPOIN_FREE) {
    WM_drag_data_free(but->dragtype, but->dragpoin);
    but->dragflag &= ~BUT_DRAGPOIN_FREE;
  }
  but->dragpoin = (void *)name;
}

void button_drag_set_image(Button *but, const char *path, int icon, const ImBuf *imb, float scale)
{
  def_but_icon(but, icon, 0); /* no flag UI_HAS_ICON, so icon doesn't draw in button */
  button_drag_set_path(but, path);
  button_drag_attach_image(but, imb, scale);
}

void button_drag_free(Button *but)
{
  if (but->dragpoin && (but->dragflag & BUT_DRAGPOIN_FREE)) {
    WM_drag_data_free(but->dragtype, but->dragpoin);
  }
}

bool button_drag_is_draggable(const Button *but)
{
  return but->dragpoin != nullptr;
}

void button_drag_start(bContext *C, Button *but)
{
  wmDrag *drag = WM_drag_data_create(C,
                                     but->icon,
                                     but->dragtype,
                                     but->dragpoin,
                                     (but->dragflag & BUT_DRAGPOIN_FREE) ? WM_DRAG_FREE_DATA :
                                                                           WM_DRAG_NOP);
  /* wmDrag has ownership over dragpoin now, stop messing with it. */
  but->dragpoin = nullptr;

  if (but->imb) {
    WM_event_drag_image(drag, but->imb, but->imb_scale);
  }
  else if (but->drag_preview_icon_id) {
    WM_event_drag_preview_icon(drag, but->drag_preview_icon_id);
  }

  WM_event_start_prepared_drag(C, drag);

  /* Special feature for assets: We add another drag item that supports multiple assets. It
   * gets the assets from context. */
  if (ELEM(but->dragtype, WM_DRAG_ASSET, WM_DRAG_ID)) {
    WM_event_start_drag(C, ICON_NONE, WM_DRAG_ASSET_LIST, nullptr, WM_DRAG_NOP);
  }

  if (but->dragtype == WM_DRAG_PATH) {
    WM_event_drag_path_override_poin_data_with_space_file_paths(C, drag);
  }
}

}  // namespace blender::ui
