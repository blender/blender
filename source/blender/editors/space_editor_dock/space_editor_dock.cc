/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup speditordock
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BKE_screen.hh"

#include "ED_screen.hh"
#include "ED_space_api.hh"

#include "UI_interface.hh"
#include "UI_view2d.hh"

#include "BLO_read_write.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "editor_dock_intern.hh"

/* ******************** default callbacks for editordock space ******************** */

static SpaceLink *editor_dock_create(const ScrArea * /*area*/, const Scene * /*scene*/)
{
  ARegion *region;
  SpaceEditorDock *space_editor_dock;

  space_editor_dock = MEM_callocN<SpaceEditorDock>("init editor dock");
  space_editor_dock->spacetype = SPACE_EDITOR_DOCK;

  /* main region */
  region = BKE_area_region_new();
  BLI_addtail(&space_editor_dock->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  return (SpaceLink *)space_editor_dock;
}

/* Doesn't free the space-link itself. */
static void editor_dock_free(SpaceLink * /*sl*/) {}

/* spacetype; init callback */
static void editor_dock_init(wmWindowManager * /*wm*/, ScrArea * /*area*/) {}

static SpaceLink *editor_dock_duplicate(SpaceLink *sl)
{
  SpaceEditorDock *space_editor_dock = static_cast<SpaceEditorDock *>(MEM_dupallocN(sl));

  /* clear or remove stuff from old */

  return (SpaceLink *)space_editor_dock;
}

static void editor_dock_keymap(wmKeyConfig * /*keyconf*/) {}

/* add handlers, stuff you only do once or on area/region changes */
static void editor_dock_main_region_init(wmWindowManager *wm, ARegion *region)
{
  ED_region_panels_init(wm, region);
  region->v2d.keepzoom |= V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y;
}

static void editor_dock_main_region_draw(const bContext *C, ARegion *region)
{
  LISTBASE_FOREACH (PanelType *, pt, &region->runtime->type->paneltypes) {
    pt->flag |= PANEL_TYPE_LAYOUT_VERT_BAR;
  }

  ED_region_panels_layout(C, region);
  /* #ED_region_panels_layout adds vertical scroll-bars, we don't want them. */
  region->v2d.scroll &= ~V2D_SCROLL_VERTICAL;
  ED_region_panels_draw(C, region);
}

static void editor_dock_main_region_listener(const wmRegionListenerParams *params)
{
  // ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    break;
  }
}

static void editor_dock_space_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  BLO_write_struct(writer, SpaceEditorDock, sl);
}

void ED_spacetype_editor_dock()
{
  std::unique_ptr<SpaceType> st = std::make_unique<SpaceType>();
  ARegionType *art;

  st->spaceid = SPACE_EDITOR_DOCK;
  STRNCPY_UTF8(st->name, "Editor Dock");

  st->create = editor_dock_create;
  st->free = editor_dock_free;
  st->init = editor_dock_init;
  st->duplicate = editor_dock_duplicate;
  st->operatortypes = blender::ed::editor_dock::register_operatortypes;
  st->keymap = editor_dock_keymap;
  st->blend_write = editor_dock_space_blend_write;

  /* regions: main window */
  art = MEM_callocN<ARegionType>("spacetype editor dock main region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = editor_dock_main_region_init;
  art->draw = editor_dock_main_region_draw;
  art->listener = editor_dock_main_region_listener;
  art->prefsizex = UI_UNIT_X * 5; /* Mainly to avoid glitches */
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
  BLI_addhead(&st->regiontypes, art);
  blender::ed::editor_dock::main_region_panels_register(art);

  BKE_spacetype_register(std::move(st));
}
