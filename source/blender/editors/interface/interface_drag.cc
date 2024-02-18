/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "UI_interface.hh"

#include "WM_api.hh"

#include "interface_intern.hh"

void UI_but_drag_set_id(uiBut *but, ID *id)
{
  but->dragtype = WM_DRAG_ID;
  if (but->dragflag & UI_BUT_DRAGPOIN_FREE) {
    WM_drag_data_free(but->dragtype, but->dragpoin);
    but->dragflag &= ~UI_BUT_DRAGPOIN_FREE;
  }
  but->dragpoin = (void *)id;
}

void UI_but_drag_attach_image(uiBut *but, const ImBuf *imb, const float scale)
{
  but->imb = imb;
  but->imb_scale = scale;
  UI_but_dragflag_enable(but, UI_BUT_DRAG_FULL_BUT);
}

void UI_but_drag_set_asset(uiBut *but,
                           const blender::asset_system::AssetRepresentation *asset,
                           int import_method,
                           int icon,
                           const ImBuf *imb,
                           float scale)
{
  wmDragAsset *asset_drag = WM_drag_create_asset_data(asset, import_method);

  but->dragtype = WM_DRAG_ASSET;
  ui_def_but_icon(but, icon, 0); /* no flag UI_HAS_ICON, so icon doesn't draw in button */
  if (but->dragflag & UI_BUT_DRAGPOIN_FREE) {
    WM_drag_data_free(but->dragtype, but->dragpoin);
  }
  but->dragpoin = asset_drag;
  but->dragflag |= UI_BUT_DRAGPOIN_FREE;
  UI_but_drag_attach_image(but, imb, scale);
}

void UI_but_drag_set_rna(uiBut *but, PointerRNA *ptr)
{
  but->dragtype = WM_DRAG_RNA;
  if (but->dragflag & UI_BUT_DRAGPOIN_FREE) {
    WM_drag_data_free(but->dragtype, but->dragpoin);
    but->dragflag &= ~UI_BUT_DRAGPOIN_FREE;
  }
  but->dragpoin = (void *)ptr;
}

void UI_but_drag_set_path(uiBut *but, const char *path)
{
  but->dragtype = WM_DRAG_PATH;
  if (but->dragflag & UI_BUT_DRAGPOIN_FREE) {
    WM_drag_data_free(but->dragtype, but->dragpoin);
  }
  but->dragpoin = WM_drag_create_path_data(blender::Span(&path, 1));
  but->dragflag |= UI_BUT_DRAGPOIN_FREE;
}

void UI_but_drag_set_name(uiBut *but, const char *name)
{
  but->dragtype = WM_DRAG_NAME;
  if (but->dragflag & UI_BUT_DRAGPOIN_FREE) {
    WM_drag_data_free(but->dragtype, but->dragpoin);
    but->dragflag &= ~UI_BUT_DRAGPOIN_FREE;
  }
  but->dragpoin = (void *)name;
}

void UI_but_drag_set_image(uiBut *but, const char *path, int icon, const ImBuf *imb, float scale)
{
  ui_def_but_icon(but, icon, 0); /* no flag UI_HAS_ICON, so icon doesn't draw in button */
  UI_but_drag_set_path(but, path);
  UI_but_drag_attach_image(but, imb, scale);
}

void ui_but_drag_free(uiBut *but)
{
  if (but->dragpoin && (but->dragflag & UI_BUT_DRAGPOIN_FREE)) {
    WM_drag_data_free(but->dragtype, but->dragpoin);
  }
}

bool ui_but_drag_is_draggable(const uiBut *but)
{
  return but->dragpoin != nullptr;
}

void ui_but_drag_start(bContext *C, uiBut *but)
{
  wmDrag *drag = WM_drag_data_create(C,
                                     but->icon,
                                     but->dragtype,
                                     but->dragpoin,
                                     (but->dragflag & UI_BUT_DRAGPOIN_FREE) ? WM_DRAG_FREE_DATA :
                                                                              WM_DRAG_NOP);
  /* wmDrag has ownership over dragpoin now, stop messing with it. */
  but->dragpoin = nullptr;

  if (but->imb) {
    WM_event_drag_image(drag, but->imb, but->imb_scale);
  }

  WM_event_start_prepared_drag(C, drag);

  /* Special feature for assets: We add another drag item that supports multiple assets. It
   * gets the assets from context. */
  if (ELEM(but->dragtype, WM_DRAG_ASSET, WM_DRAG_ID)) {
    WM_event_start_drag(C, ICON_NONE, WM_DRAG_ASSET_LIST, nullptr, WM_DRAG_NOP);
  }
}
