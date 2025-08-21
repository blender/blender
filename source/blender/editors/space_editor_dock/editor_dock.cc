/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup speditordock
 */

#include "BKE_screen.hh"

#include "BLI_listbase.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "ED_screen.hh"

#include "WM_api.hh"

#include "ED_editor_dock.hh"

namespace blender::ed::editor_dock {

/* TODO this isn't editor dock specific. Move somewhere else? */
SpaceLink *add_docked_space(ScrArea *area, const eSpace_Type type, const Scene *scene)
{
  SpaceType *st = BKE_spacetype_from_id(type);
  if (!st) {
    return nullptr;
  }

  SpaceLink *sl_old = static_cast<SpaceLink *>(area->spacedata.first);

  area->spacetype = type;
  area->type = st;

  SpaceLink *sl = st->create(area, scene);
  BLI_addhead(&area->spacedata, sl);
  BLI_addhead(&area->docked_spaces_ordered, BLI_genericNodeN(sl));

  if (sl_old) {
    sl_old->regionbase = area->regionbase;
  }
  area->regionbase = sl->regionbase;
  BLI_listbase_clear(&sl->regionbase);

  return sl;
}

void activate_docked_space(bContext *C, ScrArea *docked_area, SpaceLink *space)
{
  BLI_assert(docked_area->flag & AREA_FLAG_DOCKED);

  SpaceType *st = BKE_spacetype_from_id(space->spacetype);
  if (!st) {
    return;
  }

  ED_area_exit(C, docked_area);
  docked_area->spacetype = space->spacetype;
  docked_area->type = st;

  SpaceLink *sl_old = static_cast<SpaceLink *>(docked_area->spacedata.first);
  if (sl_old) {
    sl_old->regionbase = docked_area->regionbase;
  }
  docked_area->regionbase = space->regionbase;
  BLI_listbase_clear(&space->regionbase);
  BLI_remlink(&docked_area->spacedata, space);
  BLI_addhead(&docked_area->spacedata, space);

  ED_area_init(C, CTX_wm_window(C), docked_area);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_CHANGED, docked_area);
  ED_area_tag_refresh(docked_area);
  ED_area_tag_redraw(docked_area);

  WM_event_add_mousemove(CTX_wm_window(C));
}

}  // namespace blender::ed::editor_dock
