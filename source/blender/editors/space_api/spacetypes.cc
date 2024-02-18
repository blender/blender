/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spapi
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "GPU_state.h"

#include "UI_interface.hh"
#include "UI_view2d.hh"

#include "ED_anim_api.hh"
#include "ED_armature.hh"
#include "ED_asset.hh"
#include "ED_clip.hh"
#include "ED_curve.hh"
#include "ED_curves.hh"
#include "ED_curves_sculpt.hh"
#include "ED_fileselect.hh"
#include "ED_geometry.hh"
#include "ED_gizmo_library.hh"
#include "ED_gpencil_legacy.hh"
#include "ED_grease_pencil.hh"
#include "ED_lattice.hh"
#include "ED_markers.hh"
#include "ED_mask.hh"
#include "ED_mball.hh"
#include "ED_mesh.hh"
#include "ED_node.hh"
#include "ED_object.hh"
#include "ED_paint.hh"
#include "ED_physics.hh"
#include "ED_render.hh"
#include "ED_scene.hh"
#include "ED_screen.hh"
#include "ED_sculpt.hh"
#include "ED_sequencer.hh"
#include "ED_sound.hh"
#include "ED_space_api.hh"
#include "ED_transform.hh"
#include "ED_userpref.hh"
#include "ED_util.hh"
#include "ED_uvedit.hh"

#include "io_ops.hh"

void ED_spacetypes_init()
{
  using namespace blender::ed;
  /* UI unit is a variable, may be used in some space type initialization. */
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
  spreadsheet::register_spacetype();

  /* Register operator types for screen and all spaces. */
  ED_operatortypes_userpref();
  ED_operatortypes_workspace();
  ED_operatortypes_scene();
  ED_operatortypes_screen();
  ED_operatortypes_anim();
  ED_operatortypes_animchannels();
  asset::operatortypes_asset();
  ED_operatortypes_gpencil_legacy();
  ED_operatortypes_grease_pencil();
  ED_operatortypes_object();
  ED_operatortypes_lattice();
  ED_operatortypes_mesh();
  ED_operatortypes_geometry();
  ED_operatortypes_sculpt();
  ED_operatortypes_sculpt_curves();
  ED_operatortypes_uvedit();
  ED_operatortypes_paint();
  ED_operatortypes_physics();
  ED_operatortypes_curve();
  ED_operatortypes_curves();
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

  ED_uilisttypes_ui();

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
  for (const std::unique_ptr<SpaceType> &type : BKE_spacetypes_list()) {
    /* Initialize gizmo types first, operator types need them. */
    if (type->gizmos) {
      type->gizmos();
    }
    if (type->operatortypes) {
      type->operatortypes();
    }
  }
}

void ED_spacemacros_init()
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
  ED_operatormacros_curves();
  ED_operatormacros_mask();
  ED_operatormacros_sequencer();
  ED_operatormacros_paint();
  ED_operatormacros_gpencil();
  ED_operatormacros_grease_pencil();
  ED_operatormacros_nla();

  /* Register dropboxes (can use macros). */
  ED_dropboxes_ui();
  for (const std::unique_ptr<SpaceType> &type : BKE_spacetypes_list()) {
    if (type->dropboxes) {
      type->dropboxes();
    }
  }
}

void ED_spacetypes_keymap(wmKeyConfig *keyconf)
{
  ED_keymap_screen(keyconf);
  ED_keymap_anim(keyconf);
  ED_keymap_animchannels(keyconf);
  ED_keymap_gpencil_legacy(keyconf);
  ED_keymap_grease_pencil(keyconf);
  ED_keymap_object(keyconf);
  ED_keymap_lattice(keyconf);
  ED_keymap_mesh(keyconf);
  ED_keymap_uvedit(keyconf);
  ED_keymap_curve(keyconf);
  ED_keymap_curves(keyconf);
  ED_keymap_armature(keyconf);
  ED_keymap_physics(keyconf);
  ED_keymap_metaball(keyconf);
  ED_keymap_paint(keyconf);
  ED_keymap_mask(keyconf);
  ED_keymap_marker(keyconf);
  ED_keymap_sculpt(keyconf);

  ED_keymap_view2d(keyconf);
  ED_keymap_ui(keyconf);

  ED_keymap_transform(keyconf);

  for (const std::unique_ptr<SpaceType> &type : BKE_spacetypes_list()) {
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

struct RegionDrawCB {
  RegionDrawCB *next, *prev;

  void (*draw)(const bContext *, ARegion *, void *);
  void *customdata;

  int type;
};

void *ED_region_draw_cb_activate(ARegionType *art,
                                 void (*draw)(const bContext *, ARegion *, void *),
                                 void *customdata,
                                 int type)
{
  RegionDrawCB *rdc = MEM_cnew<RegionDrawCB>(__func__);

  BLI_addtail(&art->drawcalls, rdc);
  rdc->draw = draw;
  rdc->customdata = customdata;
  rdc->type = type;

  return rdc;
}

bool ED_region_draw_cb_exit(ARegionType *art, void *handle)
{
  LISTBASE_FOREACH (RegionDrawCB *, rdc, &art->drawcalls) {
    if (rdc == (RegionDrawCB *)handle) {
      BLI_remlink(&art->drawcalls, rdc);
      MEM_freeN(rdc);
      return true;
    }
  }
  return false;
}

static void ed_region_draw_cb_draw(const bContext *C, ARegion *region, ARegionType *art, int type)
{
  LISTBASE_FOREACH_MUTABLE (RegionDrawCB *, rdc, &art->drawcalls) {
    if (rdc->type == type) {
      rdc->draw(C, region, rdc->customdata);

      /* This is needed until we get rid of BGL which can change the states we are tracking. */
      GPU_bgl_end();
    }
  }
}

void ED_region_draw_cb_draw(const bContext *C, ARegion *region, int type)
{
  ed_region_draw_cb_draw(C, region, region->type, type);
}

void ED_region_surface_draw_cb_draw(ARegionType *art, int type)
{
  ed_region_draw_cb_draw(nullptr, nullptr, art, type);
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
