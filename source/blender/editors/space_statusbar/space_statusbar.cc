/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spstatusbar
 */

#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_screen.hh"
#include "ED_space_api.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"

#include "BLO_read_write.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_types.hh"

/* ******************** default callbacks for statusbar space ******************** */

static SpaceLink *statusbar_create(const ScrArea * /*area*/, const Scene * /*scene*/)
{
  ARegion *region;
  SpaceStatusBar *sstatusbar;

  sstatusbar = static_cast<SpaceStatusBar *>(MEM_callocN(sizeof(*sstatusbar), "init statusbar"));
  sstatusbar->spacetype = SPACE_STATUSBAR;

  /* header region */
  region = static_cast<ARegion *>(MEM_callocN(sizeof(*region), "header for statusbar"));
  BLI_addtail(&sstatusbar->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = RGN_ALIGN_NONE;

  return (SpaceLink *)sstatusbar;
}

/* Doesn't free the space-link itself. */
static void statusbar_free(SpaceLink * /*sl*/) {}

/* spacetype; init callback */
static void statusbar_init(wmWindowManager * /*wm*/, ScrArea * /*area*/) {}

static SpaceLink *statusbar_duplicate(SpaceLink *sl)
{
  SpaceStatusBar *sstatusbarn = static_cast<SpaceStatusBar *>(MEM_dupallocN(sl));

  /* clear or remove stuff from old */

  return (SpaceLink *)sstatusbarn;
}

/* add handlers, stuff you only do once or on area/region changes */
static void statusbar_header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  if (ELEM(RGN_ALIGN_ENUM_FROM_MASK(region->alignment), RGN_ALIGN_RIGHT)) {
    region->flag |= RGN_FLAG_DYNAMIC_SIZE;
  }
  ED_region_header_init(region);
}

static void statusbar_operatortypes() {}

static void statusbar_keymap(wmKeyConfig * /*keyconf*/) {}

static void statusbar_header_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_SCREEN:
      if (ELEM(wmn->data, ND_LAYER, ND_ANIMPLAY)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_WM:
      if (wmn->data == ND_JOB) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCENE:
      if (wmn->data == ND_RENDER_RESULT) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_INFO) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

static void statusbar_header_region_message_subscribe(const wmRegionMessageSubscribeParams *params)
{
  wmMsgBus *mbus = params->message_bus;
  ARegion *region = params->region;

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw{};
  msg_sub_value_region_tag_redraw.owner = region;
  msg_sub_value_region_tag_redraw.user_data = region;
  msg_sub_value_region_tag_redraw.notify = ED_region_do_msg_notify_tag_redraw;

  WM_msg_subscribe_rna_anon_prop(mbus, Window, view_layer, &msg_sub_value_region_tag_redraw);
  WM_msg_subscribe_rna_anon_prop(mbus, ViewLayer, name, &msg_sub_value_region_tag_redraw);
}

static void statusbar_space_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  BLO_write_struct(writer, SpaceStatusBar, sl);
}

void ED_spacetype_statusbar()
{
  SpaceType *st = static_cast<SpaceType *>(MEM_callocN(sizeof(*st), "spacetype statusbar"));
  ARegionType *art;

  st->spaceid = SPACE_STATUSBAR;
  STRNCPY(st->name, "Status Bar");

  st->create = statusbar_create;
  st->free = statusbar_free;
  st->init = statusbar_init;
  st->duplicate = statusbar_duplicate;
  st->operatortypes = statusbar_operatortypes;
  st->keymap = statusbar_keymap;
  st->blend_write = statusbar_space_blend_write;

  /* regions: header window */
  art = static_cast<ARegionType *>(MEM_callocN(sizeof(*art), "spacetype statusbar header region"));
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = 0.8f * HEADERY;
  art->prefsizex = UI_UNIT_X * 5; /* Mainly to avoid glitches */
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
  art->init = statusbar_header_region_init;
  art->layout = ED_region_header_layout;
  art->draw = ED_region_header_draw;
  art->listener = statusbar_header_region_listener;
  art->message_subscribe = statusbar_header_region_message_subscribe;
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(st);
}
