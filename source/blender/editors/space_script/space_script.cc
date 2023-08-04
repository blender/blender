/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spscript
 */

#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_screen.hh"
#include "ED_space_api.h"

#include "WM_api.hh"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "BLO_read_write.h"

#ifdef WITH_PYTHON
#endif

#include "script_intern.h" /* own include */

// static script_run_python(char *funcname, )

/* ******************** default callbacks for script space ***************** */

static SpaceLink *script_create(const ScrArea * /*area*/, const Scene * /*scene*/)
{
  ARegion *region;
  SpaceScript *sscript;

  sscript = static_cast<SpaceScript *>(MEM_callocN(sizeof(SpaceScript), "initscript"));
  sscript->spacetype = SPACE_SCRIPT;

  /* header */
  region = static_cast<ARegion *>(MEM_callocN(sizeof(ARegion), "header for script"));

  BLI_addtail(&sscript->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* main region */
  region = static_cast<ARegion *>(MEM_callocN(sizeof(ARegion), "main region for script"));

  BLI_addtail(&sscript->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  /* channel list region XXX */

  return (SpaceLink *)sscript;
}

/* Doesn't free the space-link itself. */
static void script_free(SpaceLink *sl)
{
  SpaceScript *sscript = (SpaceScript *)sl;

#ifdef WITH_PYTHON
  /* Free buttons references. */
  if (sscript->but_refs) {
    sscript->but_refs = nullptr;
  }
#endif
  sscript->script = nullptr;
}

/* spacetype; init callback */
static void script_init(wmWindowManager * /*wm*/, ScrArea * /*area*/) {}

static SpaceLink *script_duplicate(SpaceLink *sl)
{
  SpaceScript *sscriptn = static_cast<SpaceScript *>(MEM_dupallocN(sl));

  /* clear or remove stuff from old */

  return (SpaceLink *)sscriptn;
}

/* add handlers, stuff you only do once or on area/region changes */
static void script_main_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_STANDARD, region->winx, region->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "Script", SPACE_SCRIPT, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);
}

static void script_main_region_draw(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  SpaceScript *sscript = (SpaceScript *)CTX_wm_space_data(C);
  View2D *v2d = &region->v2d;

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);

  UI_view2d_view_ortho(v2d);

  /* data... */
  // BPY_script_exec(C, "/root/blender-svn/blender25/test.py", nullptr);

#ifdef WITH_PYTHON
  if (sscript->script) {
    // BPY_run_script_space_draw(C, sscript);
  }
#else
  (void)sscript;
#endif

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* scrollers? */
}

/* add handlers, stuff you only do once or on area/region changes */
static void script_header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

static void script_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

static void script_main_region_listener(const wmRegionListenerParams * /*params*/)
{
/* XXX: Todo, need the ScriptSpace accessible to get the python script to run. */
#if 0
  BPY_run_script_space_listener()
#endif
}

static void script_space_blend_read_lib(BlendLibReader *reader, ID *parent_id, SpaceLink *sl)
{
  SpaceScript *scpt = (SpaceScript *)sl;
  /*scpt->script = nullptr; - 2.45 set to null, better re-run the script */
  if (scpt->script) {
    BLO_read_id_address(reader, parent_id, &scpt->script);
    if (scpt->script) {
      SCRIPT_SET_NULL(scpt->script);
    }
  }
}

static void script_space_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  SpaceScript *scr = (SpaceScript *)sl;
  scr->but_refs = nullptr;
  BLO_write_struct(writer, SpaceScript, sl);
}

void ED_spacetype_script()
{
  SpaceType *st = static_cast<SpaceType *>(MEM_callocN(sizeof(SpaceType), "spacetype script"));
  ARegionType *art;

  st->spaceid = SPACE_SCRIPT;
  STRNCPY(st->name, "Script");

  st->create = script_create;
  st->free = script_free;
  st->init = script_init;
  st->duplicate = script_duplicate;
  st->operatortypes = script_operatortypes;
  st->keymap = script_keymap;
  st->blend_read_lib = script_space_blend_read_lib;
  st->blend_write = script_space_blend_write;

  /* regions: main window */
  art = static_cast<ARegionType *>(MEM_callocN(sizeof(ARegionType), "spacetype script region"));
  art->regionid = RGN_TYPE_WINDOW;
  art->init = script_main_region_init;
  art->draw = script_main_region_draw;
  art->listener = script_main_region_listener;
  /* XXX: Need to further test this ED_KEYMAP_UI is needed for button interaction. */
  art->keymapflag = ED_KEYMAP_VIEW2D | ED_KEYMAP_UI | ED_KEYMAP_FRAMES;

  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = static_cast<ARegionType *>(MEM_callocN(sizeof(ARegionType), "spacetype script region"));
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;

  art->init = script_header_region_init;
  art->draw = script_header_region_draw;

  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(st);
}
