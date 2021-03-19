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
 * The Original Code is Copyright (C) Blender Foundation, 2008
 */

/** \file
 * \ingroup spapi
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "GPU_state.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_asset.h"
#include "ED_clip.h"
#include "ED_curve.h"
#include "ED_fileselect.h"
#include "ED_geometry.h"
#include "ED_gizmo_library.h"
#include "ED_gpencil.h"
#include "ED_lattice.h"
#include "ED_markers.h"
#include "ED_mask.h"
#include "ED_mball.h"
#include "ED_mesh.h"
#include "ED_node.h"
#include "ED_object.h"
#include "ED_paint.h"
#include "ED_physics.h"
#include "ED_render.h"
#include "ED_scene.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_sequencer.h"
#include "ED_sound.h"
#include "ED_space_api.h"
#include "ED_transform.h"
#include "ED_userpref.h"
#include "ED_util.h"
#include "ED_uvedit.h"

#include "io_ops.h"

/* Only called once on startup. storage is global in BKE kernel listbase. */
void ED_spacetypes_init(void)
{
  /* UI unit is a variable, may be used in some space type inits. */
  U.widget_unit = 20;

  /* Create space types. */
  ED_spacetype_outliner();
  ED_spacetype_view3d();
  ED_spacetype_ipo();
  ED_spacetype_image();
  ED_spacetype_node();
  ED_spacetype_buttons();
  ED_spacetype_info();
  ED_spacetype_file();
  ED_spacetype_action();
  ED_spacetype_nla();
  ED_spacetype_script();
  ED_spacetype_text();
  ED_spacetype_sequencer();
  ED_spacetype_console();
  ED_spacetype_userpref();
  ED_spacetype_clip();
  ED_spacetype_statusbar();
  ED_spacetype_topbar();
  ED_spacetype_spreadsheet();

  /* Register operator types for screen and all spaces. */
  ED_operatortypes_userpref();
  ED_operatortypes_workspace();
  ED_operatortypes_scene();
  ED_operatortypes_screen();
  ED_operatortypes_anim();
  ED_operatortypes_animchannels();
  ED_operatortypes_asset();
  ED_operatortypes_gpencil();
  ED_operatortypes_object();
  ED_operatortypes_lattice();
  ED_operatortypes_mesh();
  ED_operatortypes_geometry();
  ED_operatortypes_sculpt();
  ED_operatortypes_uvedit();
  ED_operatortypes_paint();
  ED_operatortypes_physics();
  ED_operatortypes_curve();
  ED_operatortypes_armature();
  ED_operatortypes_marker();
  ED_operatortypes_metaball();
  ED_operatortypes_sound();
  ED_operatortypes_render();
  ED_operatortypes_mask();
  ED_operatortypes_io();
  ED_operatortypes_edutils();

  ED_operatortypes_view2d();
  ED_operatortypes_ui();

  ED_screen_user_menu_register();

  /* Gizmo types. */
  ED_gizmotypes_button_2d();
  ED_gizmotypes_dial_3d();
  ED_gizmotypes_move_3d();
  ED_gizmotypes_arrow_3d();
  ED_gizmotypes_preselect_3d();
  ED_gizmotypes_primitive_3d();
  ED_gizmotypes_blank_3d();
  ED_gizmotypes_cage_2d();
  ED_gizmotypes_cage_3d();
  ED_gizmotypes_snap_3d();

  /* Register types for operators and gizmos. */
  const ListBase *spacetypes = BKE_spacetypes_list();
  LISTBASE_FOREACH (const SpaceType *, type, spacetypes) {
    /* Initialize gizmo types first, operator types need them. */
    if (type->gizmos) {
      type->gizmos();
    }
    if (type->operatortypes) {
      type->operatortypes();
    }
  }
}

void ED_spacemacros_init(void)
{
  /* Macros must go last since they reference other operators.
   * They need to be registered after python operators too. */
  ED_operatormacros_armature();
  ED_operatormacros_mesh();
  ED_operatormacros_uvedit();
  ED_operatormacros_metaball();
  ED_operatormacros_node();
  ED_operatormacros_object();
  ED_operatormacros_file();
  ED_operatormacros_graph();
  ED_operatormacros_action();
  ED_operatormacros_clip();
  ED_operatormacros_curve();
  ED_operatormacros_mask();
  ED_operatormacros_sequencer();
  ED_operatormacros_paint();
  ED_operatormacros_gpencil();

  /* Register dropboxes (can use macros). */
  const ListBase *spacetypes = BKE_spacetypes_list();
  LISTBASE_FOREACH (const SpaceType *, type, spacetypes) {
    if (type->dropboxes) {
      type->dropboxes();
    }
  }
}

/**
 * \note Keymap definitions are registered only once per WM initialize,
 * usually on file read, using the keymap the actual areas/regions add the handlers.
 * \note Called in wm.c. */
void ED_spacetypes_keymap(wmKeyConfig *keyconf)
{
  ED_keymap_screen(keyconf);
  ED_keymap_anim(keyconf);
  ED_keymap_animchannels(keyconf);
  ED_keymap_gpencil(keyconf);
  ED_keymap_object(keyconf);
  ED_keymap_lattice(keyconf);
  ED_keymap_mesh(keyconf);
  ED_keymap_uvedit(keyconf);
  ED_keymap_curve(keyconf);
  ED_keymap_armature(keyconf);
  ED_keymap_physics(keyconf);
  ED_keymap_metaball(keyconf);
  ED_keymap_paint(keyconf);
  ED_keymap_mask(keyconf);
  ED_keymap_marker(keyconf);

  ED_keymap_view2d(keyconf);
  ED_keymap_ui(keyconf);

  ED_keymap_transform(keyconf);

  const ListBase *spacetypes = BKE_spacetypes_list();
  LISTBASE_FOREACH (const SpaceType *, type, spacetypes) {
    if (type->keymap) {
      type->keymap(keyconf);
    }
    LISTBASE_FOREACH (ARegionType *, region_type, &type->regiontypes) {
      if (region_type->keymap) {
        region_type->keymap(keyconf);
      }
    }
  }
}

/* ********************** Custom Draw Call API ***************** */

typedef struct RegionDrawCB {
  struct RegionDrawCB *next, *prev;

  void (*draw)(const struct bContext *, struct ARegion *, void *);
  void *customdata;

  int type;

} RegionDrawCB;

void *ED_region_draw_cb_activate(ARegionType *art,
                                 void (*draw)(const struct bContext *, struct ARegion *, void *),
                                 void *customdata,
                                 int type)
{
  RegionDrawCB *rdc = MEM_callocN(sizeof(RegionDrawCB), "RegionDrawCB");

  BLI_addtail(&art->drawcalls, rdc);
  rdc->draw = draw;
  rdc->customdata = customdata;
  rdc->type = type;

  return rdc;
}

void ED_region_draw_cb_exit(ARegionType *art, void *handle)
{
  LISTBASE_FOREACH (RegionDrawCB *, rdc, &art->drawcalls) {
    if (rdc == (RegionDrawCB *)handle) {
      BLI_remlink(&art->drawcalls, rdc);
      MEM_freeN(rdc);
      return;
    }
  }
}

void ED_region_draw_cb_draw(const bContext *C, ARegion *region, int type)
{
  LISTBASE_FOREACH_MUTABLE (RegionDrawCB *, rdc, &region->type->drawcalls) {
    if (rdc->type == type) {
      rdc->draw(C, region, rdc->customdata);

      /* This is needed until we get rid of BGL which can change the states we are tracking. */
      GPU_bgl_end();
    }
  }
}

void ED_region_draw_cb_remove_by_type(ARegionType *art, void *draw_fn, void (*free)(void *))
{
  LISTBASE_FOREACH_MUTABLE (RegionDrawCB *, rdc, &art->drawcalls) {
    if (rdc->draw == draw_fn) {
      if (free) {
        free(rdc->customdata);
      }
      BLI_remlink(&art->drawcalls, rdc);
      MEM_freeN(rdc);
    }
  }
}

/* ********************* space template *********************** */
/* forward declare */
void ED_spacetype_xxx(void);

/* allocate and init some vars */
static SpaceLink *xxx_create(const ScrArea *UNUSED(area), const Scene *UNUSED(scene))
{
  return NULL;
}

/* not spacelink itself */
static void xxx_free(SpaceLink *UNUSED(sl))
{
}

/* spacetype; init callback for usage, should be re-doable. */
static void xxx_init(wmWindowManager *UNUSED(wm), ScrArea *UNUSED(area))
{

  /* link area to SpaceXXX struct */

  /* define how many regions, the order and types */

  /* add types to regions */
}

static SpaceLink *xxx_duplicate(SpaceLink *UNUSED(sl))
{

  return NULL;
}

static void xxx_operatortypes(void)
{
  /* register operator types for this space */
}

static void xxx_keymap(wmKeyConfig *UNUSED(keyconf))
{
  /* add default items to keymap */
}

/* only called once, from screen/spacetypes.c */
void ED_spacetype_xxx(void)
{
  static SpaceType st;

  st.spaceid = SPACE_VIEW3D;

  st.create = xxx_create;
  st.free = xxx_free;
  st.init = xxx_init;
  st.duplicate = xxx_duplicate;
  st.operatortypes = xxx_operatortypes;
  st.keymap = xxx_keymap;

  BKE_spacetype_register(&st);
}

/* ****************************** end template *********************** */
