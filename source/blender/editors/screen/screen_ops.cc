/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscr
 */

#include <cmath>
#include <cstring>
#include <fmt/format.h>

#include "MEM_guardedalloc.h"

#include "BLI_build_config.h"
#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_workspace_types.h"

#include "BKE_callbacks.hh"
#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_fcurve.hh"
#include "BKE_global.hh"
#include "BKE_icons.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_mask.h"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_sound.hh"
#include "BKE_workspace.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "ED_anim_api.hh"
#include "ED_armature.hh"
#include "ED_buttons.hh"
#include "ED_fileselect.hh"
#include "ED_image.hh"
#include "ED_keyframes_keylist.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_scene.hh"
#include "ED_screen.hh"
#include "ED_screen_types.hh"
#include "ED_sequencer.hh"
#include "ED_space_graph.hh"
#include "ED_view3d.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "GPU_capabilities.hh"

#include "wm_window.hh"

#include "screen_intern.hh" /* own module include */

using blender::Vector;

#define KM_MODAL_CANCEL 1
#define KM_MODAL_APPLY 2
#define KM_MODAL_SNAP_ON 3
#define KM_MODAL_SNAP_OFF 4

/* -------------------------------------------------------------------- */
/** \name Public Poll API
 * \{ */

bool ED_operator_regionactive(bContext *C)
{
  if (CTX_wm_window(C) == nullptr) {
    return false;
  }
  if (CTX_wm_screen(C) == nullptr) {
    return false;
  }
  if (CTX_wm_region(C) == nullptr) {
    return false;
  }
  return true;
}

bool ED_operator_areaactive(bContext *C)
{
  if (CTX_wm_window(C) == nullptr) {
    return false;
  }
  if (CTX_wm_screen(C) == nullptr) {
    return false;
  }
  if (CTX_wm_area(C) == nullptr) {
    return false;
  }
  return true;
}

bool ED_operator_screenactive(bContext *C)
{
  if (CTX_wm_window(C) == nullptr) {
    return false;
  }
  if (CTX_wm_screen(C) == nullptr) {
    return false;
  }
  return true;
}

bool ED_operator_screenactive_nobackground(bContext *C)
{
  if (G.background) {
    return false;
  }
  return ED_operator_screenactive(C);
}

/* XXX added this to prevent anim state to change during renders */
static bool operator_screenactive_norender(bContext *C)
{
  if (G.is_rendering) {
    return false;
  }
  if (CTX_wm_window(C) == nullptr) {
    return false;
  }
  if (CTX_wm_screen(C) == nullptr) {
    return false;
  }
  return true;
}

bool ED_operator_screen_mainwinactive(bContext *C)
{
  if (CTX_wm_window(C) == nullptr) {
    return false;
  }
  bScreen *screen = CTX_wm_screen(C);
  if (screen == nullptr) {
    return false;
  }
  if (screen->active_region != nullptr) {
    return false;
  }
  return true;
}

bool ED_operator_scene(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  if (scene) {
    return true;
  }
  return false;
}

bool ED_operator_sequencer_scene(bContext *C)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (scene == nullptr || !BKE_id_is_editable(CTX_data_main(C), &scene->id)) {
    return false;
  }
  return true;
}

bool ED_operator_scene_editable(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr || !BKE_id_is_editable(CTX_data_main(C), &scene->id)) {
    return false;
  }
  return true;
}

bool ED_operator_sequencer_scene_editable(bContext *C)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (scene == nullptr || !BKE_id_is_editable(CTX_data_main(C), &scene->id)) {
    return false;
  }
  return true;
}

bool ED_operator_objectmode(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  Object *obact = CTX_data_active_object(C);

  if (scene == nullptr || !ID_IS_EDITABLE(scene)) {
    return false;
  }
  if (CTX_data_edit_object(C)) {
    return false;
  }

  /* add a check for ob->mode too? */
  if (obact && (obact->mode != OB_MODE_OBJECT)) {
    return false;
  }

  return true;
}

bool ED_operator_objectmode_poll_msg(bContext *C)
{
  if (!ED_operator_objectmode(C)) {
    CTX_wm_operator_poll_msg_set(C, "Only supported in object mode");
    return false;
  }

  return true;
}

bool ED_operator_objectmode_with_view3d_poll_msg(bContext *C)
{
  if (!ED_operator_objectmode_poll_msg(C)) {
    return false;
  }
  if (!ED_operator_region_view3d_active(C)) {
    return false;
  }
  return true;
}

static bool ed_spacetype_test(bContext *C, int type)
{
  if (ED_operator_areaactive(C)) {
    SpaceLink *sl = (SpaceLink *)CTX_wm_space_data(C);
    return sl && (sl->spacetype == type);
  }
  return false;
}

bool ED_operator_view3d_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_VIEW3D);
}

bool ED_operator_region_view3d_active(bContext *C)
{
  if (CTX_wm_region_view3d(C)) {
    return true;
  }

  CTX_wm_operator_poll_msg_set(C, "expected a view3d region");
  return false;
}

bool ED_operator_region_gizmo_active(bContext *C)
{
  ARegion *region = CTX_wm_region(C);
  if (region == nullptr) {
    return false;
  }
  wmGizmoMap *gzmap = region->runtime->gizmo_map;
  if (gzmap == nullptr) {
    return false;
  }
  return true;
}

bool ED_operator_animview_active(bContext *C)
{
  if (ED_operator_areaactive(C)) {
    SpaceLink *sl = (SpaceLink *)CTX_wm_space_data(C);
    if (sl && ELEM(sl->spacetype, SPACE_SEQ, SPACE_ACTION, SPACE_NLA, SPACE_GRAPH)) {
      return true;
    }
  }

  CTX_wm_operator_poll_msg_set(C, "expected a timeline/animation area to be active");
  return false;
}

bool ED_operator_outliner_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_OUTLINER);
}

bool ED_operator_region_outliner_active(bContext *C)
{
  if (!ED_operator_outliner_active(C)) {
    CTX_wm_operator_poll_msg_set(C, "Expected an active Outliner");
    return false;
  }
  const ARegion *region = CTX_wm_region(C);
  if (!(region && region->regiontype == RGN_TYPE_WINDOW)) {
    CTX_wm_operator_poll_msg_set(C, "Expected an Outliner region");
    return false;
  }
  return true;
}

bool ED_operator_outliner_active_no_editobject(bContext *C)
{
  if (ed_spacetype_test(C, SPACE_OUTLINER)) {
    Object *ob = blender::ed::object::context_active_object(C);
    Object *obedit = CTX_data_edit_object(C);
    if (ob && ob == obedit) {
      return false;
    }
    return true;
  }
  return false;
}

bool ED_operator_file_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_FILE);
}

bool ED_operator_file_browsing_active(bContext *C)
{
  if (ed_spacetype_test(C, SPACE_FILE)) {
    return ED_fileselect_is_file_browser(CTX_wm_space_file(C));
  }
  return false;
}

bool ED_operator_asset_browsing_active(bContext *C)
{
  if (ed_spacetype_test(C, SPACE_FILE)) {
    return ED_fileselect_is_asset_browser(CTX_wm_space_file(C));
  }
  return false;
}

bool ED_operator_spreadsheet_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_SPREADSHEET);
}

bool ED_operator_action_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_ACTION);
}

bool ED_operator_buttons_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_PROPERTIES);
}

bool ED_operator_node_active(bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  if (snode && snode->edittree) {
    return true;
  }

  return false;
}

bool ED_operator_node_editable(bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  if (snode && snode->edittree && BKE_id_is_editable(CTX_data_main(C), &snode->edittree->id)) {
    return true;
  }

  return false;
}

bool ED_operator_graphedit_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_GRAPH);
}

bool ED_operator_sequencer_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_SEQ) && CTX_data_sequencer_scene(C) != nullptr;
}

bool ED_operator_sequencer_active_editable(bContext *C)
{
  return ed_spacetype_test(C, SPACE_SEQ) && ED_operator_sequencer_scene_editable(C);
}

bool ED_operator_image_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_IMAGE);
}

bool ED_operator_nla_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_NLA);
}

bool ED_operator_info_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_INFO);
}

bool ED_operator_console_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_CONSOLE);
}

static bool ed_object_hidden(const Object *ob)
{
  /* if hidden but in edit mode, we still display, can happen with animation */
  return ((ob->visibility_flag & OB_HIDE_VIEWPORT) && !(ob->mode & OB_MODE_EDIT));
}

bool ED_operator_object_active_only(bContext *C)
{
  Object *ob = blender::ed::object::context_active_object(C);
  return (ob != nullptr);
}

bool ED_operator_object_active(bContext *C)
{
  Object *ob = blender::ed::object::context_active_object(C);
  return ((ob != nullptr) && !ed_object_hidden(ob));
}

bool ED_operator_object_active_editable_ex(bContext *C, const Object *ob)
{
  if (ob == nullptr) {
    CTX_wm_operator_poll_msg_set(C, "Context missing active object");
    return false;
  }

  if (!BKE_id_is_editable(CTX_data_main(C), (ID *)ob)) {
    CTX_wm_operator_poll_msg_set(C, "Cannot edit library linked or non-editable override object");
    return false;
  }

  if (ed_object_hidden(ob)) {
    CTX_wm_operator_poll_msg_set(C, "Cannot edit hidden object");
    return false;
  }

  return true;
}

bool ED_operator_object_active_editable(bContext *C)
{
  Object *ob = blender::ed::object::context_active_object(C);
  return ED_operator_object_active_editable_ex(C, ob);
}

bool ED_operator_object_active_local_editable_ex(bContext *C, const Object *ob)
{
  return ED_operator_object_active_editable_ex(C, ob) && !ID_IS_OVERRIDE_LIBRARY(ob);
}

bool ED_operator_object_active_local_editable(bContext *C)
{
  Object *ob = blender::ed::object::context_active_object(C);
  return ED_operator_object_active_editable_ex(C, ob) && !ID_IS_OVERRIDE_LIBRARY(ob);
}

bool ED_operator_object_active_editable_mesh(bContext *C)
{
  Object *ob = blender::ed::object::context_active_object(C);
  return ((ob != nullptr) && ID_IS_EDITABLE(ob) && !ed_object_hidden(ob) &&
          (ob->type == OB_MESH) && ID_IS_EDITABLE(ob->data) && !ID_IS_OVERRIDE_LIBRARY(ob->data));
}

bool ED_operator_object_active_editable_font(bContext *C)
{
  Object *ob = blender::ed::object::context_active_object(C);
  return ((ob != nullptr) && ID_IS_EDITABLE(ob) && !ed_object_hidden(ob) &&
          (ob->type == OB_FONT) && ID_IS_EDITABLE(ob->data) && !ID_IS_OVERRIDE_LIBRARY(ob->data));
}

bool ED_operator_editable_mesh(bContext *C)
{
  Mesh *mesh = ED_mesh_context(C);
  return (mesh != nullptr) && ID_IS_EDITABLE(mesh) && !ID_IS_OVERRIDE_LIBRARY(mesh);
}

bool ED_operator_editmesh(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_MESH) {
    return nullptr != BKE_editmesh_from_object(obedit);
  }
  return false;
}

bool ED_operator_editmesh_view3d(bContext *C)
{
  return ED_operator_editmesh(C) && ED_operator_view3d_active(C);
}

bool ED_operator_editmesh_region_view3d(bContext *C)
{
  if (ED_operator_editmesh(C) && CTX_wm_region_view3d(C)) {
    return true;
  }

  CTX_wm_operator_poll_msg_set(C, "expected a view3d region & editmesh");
  return false;
}

bool ED_operator_editarmature(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_ARMATURE) {
    return nullptr != ((bArmature *)obedit->data)->edbo;
  }
  return false;
}

/**
 * Check for pose mode (no mixed modes).
 *
 * We want to enable most pose operations in weight paint mode, when it comes to transforming
 * bones, but managing bones layers/groups and their constraints can be left for pose mode only
 * (not weight paint mode).
 */
static bool ed_operator_posemode_exclusive_ex(bContext *C, Object *obact)
{
  if (obact != nullptr && !(obact->mode & OB_MODE_EDIT)) {
    if (obact == BKE_object_pose_armature_get(obact)) {
      return true;
    }
  }

  CTX_wm_operator_poll_msg_set(C, "No object, or not exclusively in pose mode");
  return false;
}

bool ED_operator_posemode_exclusive(bContext *C)
{
  Object *obact = blender::ed::object::context_active_object(C);

  return ed_operator_posemode_exclusive_ex(C, obact);
}

bool ED_operator_object_active_local_editable_posemode_exclusive(bContext *C)
{
  Object *obact = blender::ed::object::context_active_object(C);

  if (!ed_operator_posemode_exclusive_ex(C, obact)) {
    return false;
  }

  if (ID_IS_OVERRIDE_LIBRARY(obact)) {
    CTX_wm_operator_poll_msg_set(C, "Object is a local library override");
    return false;
  }

  return true;
}

bool ED_operator_posemode_context(bContext *C)
{
  Object *obpose = ED_pose_object_from_context(C);

  if (obpose && !(obpose->mode & OB_MODE_EDIT)) {
    if (BKE_object_pose_context_check(obpose)) {
      return true;
    }
  }

  return false;
}

bool ED_operator_posemode(bContext *C)
{
  Object *obact = CTX_data_active_object(C);

  if (obact && !(obact->mode & OB_MODE_EDIT)) {
    Object *obpose = BKE_object_pose_armature_get(obact);
    if (obpose != nullptr) {
      if ((obact == obpose) || (obact->mode & OB_MODE_ALL_WEIGHT_PAINT)) {
        return true;
      }
    }
  }

  return false;
}

bool ED_operator_posemode_local(bContext *C)
{
  if (ED_operator_posemode(C)) {
    Main *bmain = CTX_data_main(C);
    Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
    bArmature *arm = static_cast<bArmature *>(ob->data);
    return (BKE_id_is_editable(bmain, &ob->id) && BKE_id_is_editable(bmain, &arm->id));
  }
  return false;
}

bool ED_operator_uvedit(bContext *C)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Object *obedit = CTX_data_edit_object(C);
  return ED_space_image_show_uvedit(sima, obedit);
}

bool ED_operator_uvedit_space_image(bContext *C)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Object *obedit = CTX_data_edit_object(C);
  return sima && ED_space_image_show_uvedit(sima, obedit);
}

bool ED_operator_uvmap(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  BMEditMesh *em = nullptr;

  if (obedit && obedit->type == OB_MESH) {
    em = BKE_editmesh_from_object(obedit);
  }

  if (em && (em->bm->totface)) {
    return true;
  }

  return false;
}

bool ED_operator_editsurfcurve(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && ELEM(obedit->type, OB_CURVES_LEGACY, OB_SURF)) {
    return nullptr != ((Curve *)obedit->data)->editnurb;
  }
  return false;
}

bool ED_operator_editsurfcurve_region_view3d(bContext *C)
{
  if (ED_operator_editsurfcurve(C) && CTX_wm_region_view3d(C)) {
    return true;
  }

  CTX_wm_operator_poll_msg_set(C, "expected a view3d region & editcurve");
  return false;
}

bool ED_operator_editcurve(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_CURVES_LEGACY) {
    return nullptr != ((Curve *)obedit->data)->editnurb;
  }
  return false;
}

bool ED_operator_editcurve_3d(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_CURVES_LEGACY) {
    Curve *cu = (Curve *)obedit->data;

    return (cu->flag & CU_3D) && (nullptr != cu->editnurb);
  }
  return false;
}

bool ED_operator_editsurf(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_SURF) {
    return nullptr != ((Curve *)obedit->data)->editnurb;
  }
  return false;
}

bool ED_operator_editfont(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_FONT) {
    return nullptr != ((Curve *)obedit->data)->editfont;
  }
  return false;
}

bool ED_operator_editlattice(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_LATTICE) {
    return nullptr != ((Lattice *)obedit->data)->editlatt;
  }
  return false;
}

bool ED_operator_editmball(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_MBALL) {
    return nullptr != ((MetaBall *)obedit->data)->editelems;
  }
  return false;
}

bool ED_operator_camera_poll(bContext *C)
{
  Camera *cam = static_cast<Camera *>(CTX_data_pointer_get_type(C, "camera", &RNA_Camera).data);
  return (cam != nullptr && ID_IS_EDITABLE(cam));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Screen Utilities
 * \{ */

static bool screen_active_editable(bContext *C)
{
  if (ED_operator_screenactive(C)) {
    /* no full window splitting allowed */
    if (CTX_wm_screen(C)->state != SCREENNORMAL) {
      return false;
    }
    return true;
  }
  return false;
}

/**
 * Begin a modal operation,
 * the caller is responsible for calling #screen_modal_action_end when it's ended.
 */
static void screen_modal_action_begin()
{
  G.moving |= G_TRANSFORM_WM;
}

/** Call once the modal action has finished. */
static void screen_modal_action_end()
{
  G.moving &= ~G_TRANSFORM_WM;

  /* Full refresh after `G.moving` is cleared otherwise tool gizmos
   * won't be refreshed with the modified flag, see: #143629. */
  WM_main_add_notifier(NC_SCREEN | NA_EDITED, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Action Zone Operator
 * \{ */

/* operator state vars used:
 * none
 *
 * functions:
 *
 * apply() set action-zone event
 *
 * exit()   free customdata
 *
 * callbacks:
 *
 * exec()   never used
 *
 * invoke() check if in zone
 * add customdata, put mouseco and area in it
 * add modal handler
 *
 * modal()  accept modal events while doing it
 * call apply() with gesture info, active window, nonactive window
 * call exit() and remove handler when LMB confirm
 */

struct sActionzoneData {
  ScrArea *sa1, *sa2;
  AZone *az;
  int x, y;
  eScreenDir gesture_dir;
  int modifier;
};

/* quick poll to save operators to be created and handled */
static bool actionzone_area_poll(bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  if (win && win->eventstate) {
    bScreen *screen = WM_window_get_active_screen(win);
    if (screen) {
      const int *xy = &win->eventstate->xy[0];

      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (AZone *, az, &area->actionzones) {
          if (BLI_rcti_isect_pt_v(&az->rect, xy)) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

/* the debug drawing of the click_rect is in area_draw_azone_fullscreen, keep both in sync */
static void fullscreen_click_rcti_init(
    rcti *rect, const short /*x1*/, const short /*y1*/, const short x2, const short y2)
{
  BLI_rcti_init(rect, x2 - U.widget_unit, x2, y2 - U.widget_unit, y2);
}

static bool azone_clipped_rect_calc(const AZone *az, rcti *r_rect_clip)
{
  const ARegion *region = az->region;
  *r_rect_clip = az->rect;
  if (az->type == AZONE_REGION) {
    if (region->overlap && (region->v2d.keeptot != V2D_KEEPTOT_STRICT) &&
        /* Only when this isn't hidden (where it's displayed as an button that expands). */
        region->runtime->visible)
    {
      /* A floating region to be resized, clip by the visible region. */
      switch (az->edge) {
        case AE_TOP_TO_BOTTOMRIGHT:
        case AE_BOTTOM_TO_TOPLEFT: {
          r_rect_clip->xmin = max_ii(
              r_rect_clip->xmin,
              (region->winrct.xmin +
               UI_view2d_view_to_region_x(&region->v2d, region->v2d.tot.xmin)) -
                  UI_REGION_OVERLAP_MARGIN);
          r_rect_clip->xmax = min_ii(
              r_rect_clip->xmax,
              (region->winrct.xmin +
               UI_view2d_view_to_region_x(&region->v2d, region->v2d.tot.xmax)) +
                  UI_REGION_OVERLAP_MARGIN);
          return true;
        }
        case AE_LEFT_TO_TOPRIGHT:
        case AE_RIGHT_TO_TOPLEFT: {
          r_rect_clip->ymin = max_ii(
              r_rect_clip->ymin,
              (region->winrct.ymin +
               UI_view2d_view_to_region_y(&region->v2d, region->v2d.tot.ymin)) -
                  UI_REGION_OVERLAP_MARGIN);
          r_rect_clip->ymax = min_ii(
              r_rect_clip->ymax,
              (region->winrct.ymin +
               UI_view2d_view_to_region_y(&region->v2d, region->v2d.tot.ymax)) +
                  UI_REGION_OVERLAP_MARGIN);
          return true;
        }
      }
    }
  }
  return false;
}

/* Return the azone's calculated rect. */
static void area_actionzone_get_rect(AZone *az, rcti *r_rect)
{
  if (az->type == AZONE_REGION_SCROLL) {
    const bool is_horizontal = az->direction == AZ_SCROLL_HOR;
    const bool is_vertical = az->direction == AZ_SCROLL_VERT;
    const bool is_right = is_vertical && bool(az->region->v2d.scroll & V2D_SCROLL_RIGHT);
    const bool is_left = is_vertical && bool(az->region->v2d.scroll & V2D_SCROLL_LEFT);
    const bool is_top = is_horizontal && bool(az->region->v2d.scroll & V2D_SCROLL_TOP);
    const bool is_bottom = is_horizontal && bool(az->region->v2d.scroll & V2D_SCROLL_BOTTOM);
    /* For scroll azones use the area around the region's scroll-bar location. */
    rcti scroller_vert = is_horizontal ? az->region->v2d.hor : az->region->v2d.vert;
    BLI_rcti_translate(&scroller_vert, az->region->winrct.xmin, az->region->winrct.ymin);

    /* Pull the zone in from edge and match the visible hit zone. */
    const int edge_padding = int(-3.0f * UI_SCALE_FAC);
    r_rect->xmin = scroller_vert.xmin - (is_right ? V2D_SCROLL_HIDE_HEIGHT : edge_padding);
    r_rect->ymin = scroller_vert.ymin - (is_top ? V2D_SCROLL_HIDE_WIDTH : edge_padding);
    r_rect->xmax = scroller_vert.xmax + (is_left ? V2D_SCROLL_HIDE_HEIGHT : edge_padding);
    r_rect->ymax = scroller_vert.ymax + (is_bottom ? V2D_SCROLL_HIDE_WIDTH : edge_padding);
  }
  else {
    azone_clipped_rect_calc(az, r_rect);
  }
}

static AZone *area_actionzone_refresh_xy(ScrArea *area, const int xy[2], const bool test_only)
{
  AZone *az = nullptr;

  for (az = static_cast<AZone *>(area->actionzones.first); az; az = az->next) {
    rcti az_rect;
    area_actionzone_get_rect(az, &az_rect);
    if (BLI_rcti_isect_pt_v(&az_rect, xy)) {

      if (az->type == AZONE_AREA) {
        break;
      }
      if (az->type == AZONE_REGION) {
        const ARegion *region = az->region;
        const int local_xy[2] = {xy[0] - region->winrct.xmin, xy[1] - region->winrct.ymin};

        /* Respect button sections: Clusters of buttons (separated using separator-spacers) are
         * drawn with a background, in-between them the region is fully transparent (if "Region
         * Overlap" is enabled). Only allow dragging visible edges, so at the button sections. */
        if (region->runtime->visible && region->overlap &&
            (region->flag & RGN_FLAG_RESIZE_RESPECT_BUTTON_SECTIONS) &&
            !UI_region_button_sections_is_inside_x(az->region, local_xy[0]))
        {
          az = nullptr;
          break;
        }

        break;
      }
      if (az->type == AZONE_FULLSCREEN) {
        rcti click_rect;
        fullscreen_click_rcti_init(&click_rect, az->x1, az->y1, az->x2, az->y2);
        const bool click_isect = BLI_rcti_isect_pt_v(&click_rect, xy);

        if (test_only) {
          if (click_isect) {
            break;
          }
        }
        else {
          if (click_isect) {
            az->alpha = 1.0f;
          }
          else {
            const int mouse_sq = square_i(xy[0] - az->x2) + square_i(xy[1] - az->y2);
            const int spot_sq = square_i(UI_AZONESPOTW_RIGHT);
            const int fadein_sq = square_i(AZONEFADEIN);
            const int fadeout_sq = square_i(AZONEFADEOUT);

            if (mouse_sq < spot_sq) {
              az->alpha = 1.0f;
            }
            else if (mouse_sq < fadein_sq) {
              az->alpha = 1.0f;
            }
            else if (mouse_sq < fadeout_sq) {
              az->alpha = 1.0f - float(mouse_sq - fadein_sq) / float(fadeout_sq - fadein_sq);
            }
            else {
              az->alpha = 0.0f;
            }

            /* fade in/out but no click */
            az = nullptr;
          }

          /* XXX force redraw to show/hide the action zone */
          ED_area_tag_redraw(area);
          break;
        }
      }
      else if (az->type == AZONE_REGION_SCROLL && az->region->runtime->visible) {
        /* If the region is not visible we can ignore this scroll-bar zone. */
        ARegion *region = az->region;
        View2D *v2d = &region->v2d;
        int scroll_flag = 0;
        const int isect_value = UI_view2d_mouse_in_scrollers_ex(region, v2d, xy, &scroll_flag);

        /* Check if we even have scroll bars. */
        if (((az->direction == AZ_SCROLL_HOR) && !(scroll_flag & V2D_SCROLL_HORIZONTAL)) ||
            ((az->direction == AZ_SCROLL_VERT) && !(scroll_flag & V2D_SCROLL_VERTICAL)))
        {
          /* No scroll-bars, do nothing. */
        }
        else if (test_only) {
          if (isect_value != 0) {
            break;
          }
        }
        else {
          bool redraw = false;

          if (isect_value == 'h') {
            if (az->direction == AZ_SCROLL_HOR) {
              az->alpha = 1.0f;
              v2d->alpha_hor = 255;
              redraw = true;
            }
          }
          else if (isect_value == 'v') {
            if (az->direction == AZ_SCROLL_VERT) {
              az->alpha = 1.0f;
              v2d->alpha_vert = 255;
              redraw = true;
            }
          }
          else {
            const int local_xy[2] = {xy[0] - region->winrct.xmin, xy[1] - region->winrct.ymin};
            float dist_fac = 0.0f, alpha = 0.0f;

            if (az->direction == AZ_SCROLL_HOR) {
              dist_fac = BLI_rcti_length_y(&v2d->hor, local_xy[1]) / V2D_SCROLL_HIDE_WIDTH;
              CLAMP(dist_fac, 0.0f, 1.0f);
              alpha = 1.0f - dist_fac;

              v2d->alpha_hor = alpha * 255;
            }
            else if (az->direction == AZ_SCROLL_VERT) {
              dist_fac = BLI_rcti_length_x(&v2d->vert, local_xy[0]) / V2D_SCROLL_HIDE_HEIGHT;
              CLAMP(dist_fac, 0.0f, 1.0f);
              alpha = 1.0f - dist_fac;

              v2d->alpha_vert = alpha * 255;
            }
            az->alpha = alpha;
            redraw = true;
          }

          if (redraw) {
            ED_region_tag_redraw_no_rebuild(region);
          }
          /* Don't return! */
        }
      }
    }
    else if (!test_only && !IS_EQF(az->alpha, 0.0f)) {
      if (az->type == AZONE_FULLSCREEN) {
        az->alpha = 0.0f;
        area->flag &= ~AREA_FLAG_ACTIONZONES_UPDATE;
        ED_area_tag_redraw_no_rebuild(area);
      }
      else if (az->type == AZONE_REGION_SCROLL && az->region->runtime->visible) {
        /* If the region is not visible we can ignore this scroll-bar zone. */
        if (az->direction == AZ_SCROLL_VERT) {
          az->alpha = az->region->v2d.alpha_vert = 0;
          area->flag &= ~AREA_FLAG_ACTIONZONES_UPDATE;
          ED_region_tag_redraw_no_rebuild(az->region);
        }
        else if (az->direction == AZ_SCROLL_HOR) {
          az->alpha = az->region->v2d.alpha_hor = 0;
          area->flag &= ~AREA_FLAG_ACTIONZONES_UPDATE;
          ED_region_tag_redraw_no_rebuild(az->region);
        }
        else {
          BLI_assert(false);
        }
      }
    }
  }

  return az;
}

/* Finds an action-zone by position in entire screen so azones can overlap. */
static AZone *screen_actionzone_find_xy(bScreen *screen, const int xy[2])
{
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    AZone *az = area_actionzone_refresh_xy(area, xy, true);
    if (az != nullptr) {
      return az;
    }
  }
  return nullptr;
}

/* Returns the area that the azone belongs to */
static ScrArea *screen_actionzone_area(bScreen *screen, const AZone *az)
{
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (AZone *, zone, &area->actionzones) {
      if (zone == az) {
        return area;
      }
    }
  }
  return nullptr;
}

AZone *ED_area_actionzone_find_xy(ScrArea *area, const int xy[2])
{
  return area_actionzone_refresh_xy(area, xy, true);
}

AZone *ED_area_azones_update(ScrArea *area, const int xy[2])
{
  return area_actionzone_refresh_xy(area, xy, false);
}

static void actionzone_exit(wmOperator *op)
{
  sActionzoneData *sad = static_cast<sActionzoneData *>(op->customdata);
  if (sad) {
    MEM_freeN(sad);
  }
  op->customdata = nullptr;

  screen_modal_action_end();
}

/* send EVT_ACTIONZONE event */
static void actionzone_apply(bContext *C, wmOperator *op, int type)
{
  wmWindow *win = CTX_wm_window(C);

  wmEvent event;
  wm_event_init_from_window(win, &event);

  if (type == AZONE_AREA) {
    event.type = EVT_ACTIONZONE_AREA;
  }
  else if (type == AZONE_FULLSCREEN) {
    event.type = EVT_ACTIONZONE_FULLSCREEN;
  }
  else {
    event.type = EVT_ACTIONZONE_REGION;
  }

  event.val = KM_NOTHING;
  event.flag = eWM_EventFlag(0);
  event.customdata = op->customdata;
  event.customdata_free = true;
  op->customdata = nullptr;

  WM_event_add(win, &event);
}

static wmOperatorStatus actionzone_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bScreen *screen = CTX_wm_screen(C);
  AZone *az = screen_actionzone_find_xy(screen, event->xy);

  /* Quick escape - Scroll azones only hide/unhide the scroll-bars,
   * they have their own handling. */
  if (az == nullptr || ELEM(az->type, AZONE_REGION_SCROLL)) {
    return OPERATOR_PASS_THROUGH;
  }

  /* ok we do the action-zone */
  sActionzoneData *sad = static_cast<sActionzoneData *>(
      op->customdata = MEM_callocN(sizeof(sActionzoneData), "sActionzoneData"));
  sad->sa1 = screen_actionzone_area(screen, az);
  sad->az = az;
  sad->x = event->xy[0];
  sad->y = event->xy[1];
  sad->modifier = RNA_int_get(op->ptr, "modifier");

  /* region azone directly reacts on mouse clicks */
  if (ELEM(sad->az->type, AZONE_REGION, AZONE_FULLSCREEN)) {
    actionzone_apply(C, op, sad->az->type);
    actionzone_exit(op);
    return OPERATOR_FINISHED;
  }

  if (sad->az->type == AZONE_AREA && sad->modifier == 0) {
    actionzone_apply(C, op, sad->az->type);
    actionzone_exit(op);
    return OPERATOR_FINISHED;
  }

  /* add modal handler */
  screen_modal_action_begin();
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus actionzone_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  bScreen *screen = CTX_wm_screen(C);
  sActionzoneData *sad = static_cast<sActionzoneData *>(op->customdata);

  switch (event->type) {
    case MOUSEMOVE: {
      const int delta_x = (event->xy[0] - sad->x);
      const int delta_y = (event->xy[1] - sad->y);

      /* Movement in dominant direction. */
      const int delta_max = max_ii(abs(delta_x), abs(delta_y));

      /* Movement in dominant direction before action taken. */
      const int join_threshold = (0.6 * U.widget_unit);
      const int split_threshold = (1.2 * U.widget_unit);
      const int area_threshold = (0.1 * U.widget_unit);

      /* Calculate gesture cardinal direction. */
      if (delta_y > abs(delta_x)) {
        sad->gesture_dir = SCREEN_DIR_N;
      }
      else if (delta_x >= abs(delta_y)) {
        sad->gesture_dir = SCREEN_DIR_E;
      }
      else if (delta_y < -abs(delta_x)) {
        sad->gesture_dir = SCREEN_DIR_S;
      }
      else {
        sad->gesture_dir = SCREEN_DIR_W;
      }

      bool is_gesture;
      if (sad->az->type == AZONE_AREA) {
        wmWindow *win = CTX_wm_window(C);

        rcti screen_rect;
        WM_window_screen_rect_calc(win, &screen_rect);

        /* Have we dragged off the zone and are not on an edge? */
        if ((ED_area_actionzone_find_xy(sad->sa1, event->xy) != sad->az) &&
            (screen_geom_area_map_find_active_scredge(
                 AREAMAP_FROM_SCREEN(screen), &screen_rect, event->xy[0], event->xy[1]) ==
             nullptr))
        {

          /* What area are we now in? */
          ScrArea *area = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, event->xy);

          if (sad->modifier == 1) {
            /* Duplicate area into new window. */
            WM_cursor_set(win, WM_CURSOR_EDIT);
            is_gesture = (delta_max > area_threshold);
          }
          else if (sad->modifier == 2) {
            /* Swap areas. */
            WM_cursor_set(win, WM_CURSOR_SWAP_AREA);
            is_gesture = true;
          }
          else if (area == sad->sa1) {
            /* Same area, so possible split. */
            WM_cursor_set(win,
                          SCREEN_DIR_IS_VERTICAL(sad->gesture_dir) ? WM_CURSOR_H_SPLIT :
                                                                     WM_CURSOR_V_SPLIT);
            is_gesture = (delta_max > split_threshold);
          }
          else if (!area || area->global) {
            /* No area or Top bar or Status bar. */
            WM_cursor_set(win, WM_CURSOR_STOP);
            is_gesture = false;
          }
          else {
            /* Different area, so possible join. */
            if (sad->gesture_dir == SCREEN_DIR_N) {
              WM_cursor_set(win, WM_CURSOR_N_ARROW);
            }
            else if (sad->gesture_dir == SCREEN_DIR_S) {
              WM_cursor_set(win, WM_CURSOR_S_ARROW);
            }
            else if (sad->gesture_dir == SCREEN_DIR_E) {
              WM_cursor_set(win, WM_CURSOR_E_ARROW);
            }
            else {
              BLI_assert(sad->gesture_dir == SCREEN_DIR_W);
              WM_cursor_set(win, WM_CURSOR_W_ARROW);
            }
            is_gesture = (delta_max > join_threshold);
          }
        }
        else {
#if defined(__APPLE__)
          const int cursor = WM_CURSOR_HAND_CLOSED;
#else
          const int cursor = WM_CURSOR_MOVE;
#endif
          WM_cursor_set(win, cursor);
          is_gesture = false;
        }
      }
      else {
        is_gesture = (delta_max > area_threshold);
      }

      /* gesture is large enough? */
      if (is_gesture) {
        /* second area, for join when (sa1 != sa2) */
        sad->sa2 = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, event->xy);
        /* apply sends event */
        actionzone_apply(C, op, sad->az->type);
        actionzone_exit(op);

        return OPERATOR_FINISHED;
      }
      break;
    }
    case EVT_ESCKEY:
      actionzone_exit(op);
      return OPERATOR_CANCELLED;
    case LEFTMOUSE:
      actionzone_exit(op);
      return OPERATOR_CANCELLED;
    default: {
      break;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static void actionzone_cancel(bContext * /*C*/, wmOperator *op)
{
  actionzone_exit(op);
}

static void SCREEN_OT_actionzone(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Handle Area Action Zones";
  ot->description = "Handle area action zones for mouse actions/gestures";
  ot->idname = "SCREEN_OT_actionzone";

  ot->invoke = actionzone_invoke;
  ot->modal = actionzone_modal;
  ot->poll = actionzone_area_poll;
  ot->cancel = actionzone_cancel;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_INTERNAL;

  RNA_def_int(ot->srna, "modifier", 0, 0, 2, "Modifier", "Modifier state", 0, 2);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Area edge detection utility
 * \{ */

static ScrEdge *screen_area_edge_from_cursor(const bContext *C,
                                             const int cursor[2],
                                             ScrArea **r_sa1,
                                             ScrArea **r_sa2)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);
  rcti window_rect;
  WM_window_rect_calc(win, &window_rect);
  ScrEdge *actedge = screen_geom_area_map_find_active_scredge(
      AREAMAP_FROM_SCREEN(screen), &window_rect, cursor[0], cursor[1]);
  *r_sa1 = nullptr;
  *r_sa2 = nullptr;
  if (actedge == nullptr) {
    return nullptr;
  }
  int borderwidth = (4 * UI_SCALE_FAC);
  ScrArea *sa1, *sa2;
  if (screen_geom_edge_is_horizontal(actedge)) {
    sa1 = BKE_screen_find_area_xy(
        screen, SPACE_TYPE_ANY, blender::int2{cursor[0], cursor[1] + borderwidth});
    sa2 = BKE_screen_find_area_xy(
        screen, SPACE_TYPE_ANY, blender::int2{cursor[0], cursor[1] - borderwidth});
  }
  else {
    sa1 = BKE_screen_find_area_xy(
        screen, SPACE_TYPE_ANY, blender::int2{cursor[0] + borderwidth, cursor[1]});
    sa2 = BKE_screen_find_area_xy(
        screen, SPACE_TYPE_ANY, blender::int2{cursor[0] - borderwidth, cursor[1]});
  }
  bool isGlobal = ((sa1 && ED_area_is_global(sa1)) || (sa2 && ED_area_is_global(sa2)));
  if (!isGlobal) {
    *r_sa1 = sa1;
    *r_sa2 = sa2;
  }
  return actedge;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Swap Area Operator
 * \{ */

/* operator state vars used:
 * sa1      start area
 * sa2      area to swap with
 *
 * functions:
 *
 * init()   set custom data for operator, based on action-zone event custom data
 *
 * cancel() cancel the operator
 *
 * exit()   cleanup, send notifier
 *
 * callbacks:
 *
 * invoke() gets called on Shift-LMB drag in action-zone
 * exec()   execute without any user interaction, based on properties
 * call init(), add handler
 *
 * modal()  accept modal events while doing it
 */

struct sAreaSwapData {
  ScrArea *sa1, *sa2;
};

static bool area_swap_init(wmOperator *op, const wmEvent *event)
{
  sActionzoneData *sad = static_cast<sActionzoneData *>(event->customdata);

  if (sad == nullptr || sad->sa1 == nullptr) {
    return false;
  }

  sAreaSwapData *sd = MEM_callocN<sAreaSwapData>("sAreaSwapData");
  sd->sa1 = sad->sa1;
  sd->sa2 = sad->sa2;
  op->customdata = sd;

  return true;
}

static void area_swap_exit(bContext *C, wmOperator *op)
{
  sAreaSwapData *sd = static_cast<sAreaSwapData *>(op->customdata);
  MEM_freeN(sd);
  op->customdata = nullptr;

  WM_cursor_modal_restore(CTX_wm_window(C));
  ED_workspace_status_text(C, nullptr);
}

static void area_swap_cancel(bContext *C, wmOperator *op)
{
  area_swap_exit(C, op);
}

static wmOperatorStatus area_swap_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!area_swap_init(op, event)) {
    return OPERATOR_PASS_THROUGH;
  }

  /* add modal handler */
  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_SWAP_AREA);
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus area_swap_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  sActionzoneData *sad = static_cast<sActionzoneData *>(op->customdata);

  switch (event->type) {
    case MOUSEMOVE: {
      /* Second area to swap with. */
      sad->sa2 = ED_area_find_under_cursor(C, SPACE_TYPE_ANY, event->xy);
      WM_cursor_set(CTX_wm_window(C), (sad->sa2) ? WM_CURSOR_SWAP_AREA : WM_CURSOR_STOP);
      WorkspaceStatus status(C);
      status.item(IFACE_("Select Area"), ICON_MOUSE_LMB);
      status.item(IFACE_("Cancel"), ICON_EVENT_ESC);
      break;
    }
    case LEFTMOUSE: /* release LMB */
      if (event->val == KM_RELEASE) {
        if (!sad->sa2 || sad->sa1 == sad->sa2) {
          area_swap_cancel(C, op);
          return OPERATOR_CANCELLED;
        }

        ED_area_tag_redraw(sad->sa1);
        ED_area_tag_redraw(sad->sa2);

        ED_area_swapspace(C, sad->sa1, sad->sa2);

        area_swap_exit(C, op);

        WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);

        return OPERATOR_FINISHED;
      }
      break;

    case EVT_ESCKEY:
      area_swap_cancel(C, op);
      return OPERATOR_CANCELLED;
    default: {
      break;
    }
  }
  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus area_swap_exec(bContext *C, wmOperator *op)
{
  ScrArea *sa1, *sa2;
  int cursor[2];
  RNA_int_get_array(op->ptr, "cursor", cursor);
  screen_area_edge_from_cursor(C, cursor, &sa1, &sa2);
  if (sa1 == nullptr || sa2 == nullptr) {
    return OPERATOR_CANCELLED;
  }
  ED_area_swapspace(C, sa1, sa2);
  return OPERATOR_FINISHED;
}

static void SCREEN_OT_area_swap(wmOperatorType *ot)
{
  ot->name = "Swap Areas";
  ot->description = "Swap selected areas screen positions";
  ot->idname = "SCREEN_OT_area_swap";

  ot->invoke = area_swap_invoke;
  ot->modal = area_swap_modal;
  ot->exec = area_swap_exec;
  ot->poll = screen_active_editable;
  ot->cancel = area_swap_cancel;

  ot->flag = OPTYPE_BLOCKING;

  /* rna */
  RNA_def_int_vector(
      ot->srna, "cursor", 2, nullptr, INT_MIN, INT_MAX, "Cursor", "", INT_MIN, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Area Duplicate Operator
 *
 * Create new window from area.
 * \{ */

/** Callback for #WM_window_open to setup the area's data. */
static void area_dupli_fn(bScreen * /*screen*/, ScrArea *area, void *user_data)
{
  ScrArea *area_src = static_cast<ScrArea *>(user_data);
  ED_area_data_copy(area, area_src, true);
  ED_area_tag_redraw(area);
}

/* operator callback */
static bool area_dupli_open(bContext *C, ScrArea *area, const blender::int2 position)
{
  const wmWindow *win = CTX_wm_window(C);
  const rcti window_rect = {win->posx + position.x,
                            win->posx + position.x + area->winx,
                            win->posy + position.y,
                            win->posy + position.y + area->winy};

  /* Create new window. No need to set space_type since it will be copied over. */
  wmWindow *newwin = WM_window_open(C,
                                    nullptr,
                                    &window_rect,
                                    SPACE_EMPTY,
                                    false,
                                    false,
                                    false,
                                    WIN_ALIGN_ABSOLUTE,
                                    /* Initialize area from callback. */
                                    area_dupli_fn,
                                    (void *)area);
  return (newwin != nullptr);
}

static wmOperatorStatus area_dupli_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ScrArea *area = CTX_wm_area(C);
  if (event && event->customdata) {
    sActionzoneData *sad = static_cast<sActionzoneData *>(event->customdata);
    if (sad == nullptr) {
      return OPERATOR_PASS_THROUGH;
    }
    area = sad->sa1;
  }

  bool newwin = area_dupli_open(C, area, blender::int2(area->totrct.xmin, area->totrct.ymin));

  if (newwin) {
    /* screen, areas init */
    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);
  }
  else {
    BKE_report(op->reports, RPT_ERROR, "Failed to open window!");
  }

  if (event && event->customdata) {
    actionzone_exit(op);
  }

  return newwin ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void SCREEN_OT_area_dupli(wmOperatorType *ot)
{
  ot->name = "Duplicate Area into New Window";
  ot->description = "Duplicate selected area into new window";
  ot->idname = "SCREEN_OT_area_dupli";

  ot->invoke = area_dupli_invoke;
  ot->poll = ED_operator_areaactive;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Area Close Operator
 *
 * Close selected area, replace by expanding a neighbor
 * \{ */

/**
 * \note This can be used interactively or from Python.
 *
 * \note Most of the window management operators don't support execution from Python.
 * An exception is made for closing areas since it allows application templates
 * to customize the layout.
 */
static wmOperatorStatus area_close_exec(bContext *C, wmOperator *op)
{
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);

  /* This operator is script-able, so the area passed could be invalid. */
  if (BLI_findindex(&screen->areabase, area) == -1) {
    BKE_report(op->reports, RPT_ERROR, "Area not found in the active screen");
    return OPERATOR_CANCELLED;
  }

  float inner[4] = {0.0f, 0.0f, 0.0f, 0.7f};
  screen_animate_area_highlight(
      CTX_wm_window(C), CTX_wm_screen(C), &area->totrct, inner, nullptr, AREA_CLOSE_FADEOUT);

  if (!screen_area_close(C, op->reports, screen, area)) {
    BKE_report(op->reports, RPT_ERROR, "Unable to close area");
    return OPERATOR_CANCELLED;
  }

  /* Ensure the event loop doesn't attempt to continue handling events.
   *
   * This causes execution from the Python console fail to return to the prompt as it should.
   * This glitch could be solved in the event loop handling as other operators may also
   * destructively manipulate windowing data. */
  CTX_wm_window_set(C, nullptr);

  WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static bool area_close_poll(bContext *C)
{
  if (!ED_operator_areaactive(C)) {
    return false;
  }

  ScrArea *area = CTX_wm_area(C);

  if (ED_area_is_global(area)) {
    return false;
  }

  bScreen *screen = CTX_wm_screen(C);

  /* Can this area join with ANY other area? */
  LISTBASE_FOREACH (ScrArea *, ar, &screen->areabase) {
    if (area_getorientation(ar, area) != -1) {
      return true;
    }
  }

  return false;
}

static void SCREEN_OT_area_close(wmOperatorType *ot)
{
  ot->name = "Close Area";
  ot->description = "Close selected area";
  ot->idname = "SCREEN_OT_area_close";
  ot->exec = area_close_exec;
  ot->poll = area_close_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Move Area Edge Operator
 * \{ */

/* operator state vars used:
 * x, y             mouse coord near edge
 * delta            movement of edge
 *
 * functions:
 *
 * init()   set default property values, find edge based on mouse coords, test
 * if the edge can be moved, select edges, calculate min and max movement
 *
 * apply()  apply delta on selection
 *
 * exit()   cleanup, send notifier
 *
 * cancel() cancel moving
 *
 * callbacks:
 *
 * exec()   execute without any user interaction, based on properties
 * call init(), apply(), exit()
 *
 * invoke() gets called on mouse click near edge
 * call init(), add handler
 *
 * modal()  accept modal events while doing it
 * call apply() with delta motion
 * call exit() and remove handler
 */

enum AreaMoveSnapType {
  /* Snapping disabled */
  SNAP_NONE = 0,              /* Snap to an invisible grid with a unit defined in AREAGRID */
  SNAP_AREAGRID,              /* Snap to fraction (half, third.. etc) and adjacent edges. */
  SNAP_FRACTION_AND_ADJACENT, /* Snap to either bigger or smaller, nothing in-between (used for
                               * global areas). This has priority over other snap types, if it is
                               * used, toggling SNAP_FRACTION_AND_ADJACENT doesn't work. */
  SNAP_BIGGER_SMALLER_ONLY,
};

struct sAreaMoveData {
  int bigger, smaller, origval, step;
  eScreenAxis dir_axis;
  AreaMoveSnapType snap_type;
  bScreen *screen;
  double start_time;
  double end_time;
  wmWindow *win;
  void *draw_callback; /* Call #screen_draw_move_highlight */
};

/* helper call to move area-edge, sets limits
 * need window bounds in order to get correct limits */
static void area_move_set_limits(wmWindow *win,
                                 bScreen *screen,
                                 const eScreenAxis dir_axis,
                                 int *bigger,
                                 int *smaller,
                                 bool *use_bigger_smaller_snap)
{
  /* we check all areas and test for free space with MINSIZE */
  *bigger = *smaller = 100000;

  if (use_bigger_smaller_snap != nullptr) {
    *use_bigger_smaller_snap = false;
    LISTBASE_FOREACH (ScrArea *, area, &win->global_areas.areabase) {
      int size_min = ED_area_global_min_size_y(area) - 1;
      int size_max = ED_area_global_max_size_y(area) - 1;

      size_min = max_ii(size_min, 0);
      BLI_assert(size_min <= size_max);

      /* logic here is only tested for lower edge :) */
      /* left edge */
      if (area->v1->editflag && area->v2->editflag) {
        *smaller = area->v4->vec.x - size_max;
        *bigger = area->v4->vec.x - size_min;
        *use_bigger_smaller_snap = true;
        return;
      }
      /* top edge */
      if (area->v2->editflag && area->v3->editflag) {
        *smaller = area->v1->vec.y + size_min;
        *bigger = area->v1->vec.y + size_max;
        *use_bigger_smaller_snap = true;
        return;
      }
      /* right edge */
      if (area->v3->editflag && area->v4->editflag) {
        *smaller = area->v1->vec.x + size_min;
        *bigger = area->v1->vec.x + size_max;
        *use_bigger_smaller_snap = true;
        return;
      }
      /* lower edge */
      if (area->v4->editflag && area->v1->editflag) {
        *smaller = area->v2->vec.y - size_max;
        *bigger = area->v2->vec.y - size_min;
        *use_bigger_smaller_snap = true;
        return;
      }
    }
  }

  rcti window_rect;
  WM_window_rect_calc(win, &window_rect);

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    if (dir_axis == SCREEN_AXIS_H) {
      const int y1 = area->winy - ED_area_headersize();
      /* if top or down edge selected, test height */
      if (area->v1->editflag && area->v4->editflag) {
        *bigger = min_ii(*bigger, y1);
      }
      else if (area->v2->editflag && area->v3->editflag) {
        *smaller = min_ii(*smaller, y1);
      }
    }
    else {
      const int x1 = area->winx - int(AREAMINX * UI_SCALE_FAC) - 1;
      /* if left or right edge selected, test width */
      if (area->v1->editflag && area->v2->editflag) {
        *bigger = min_ii(*bigger, x1);
      }
      else if (area->v3->editflag && area->v4->editflag) {
        *smaller = min_ii(*smaller, x1);
      }
    }
  }
}

static void area_move_draw_cb(const wmWindow *win, void *userdata)
{
  const wmOperator *op = static_cast<const wmOperator *>(userdata);
  const sAreaMoveData *md = static_cast<sAreaMoveData *>(op->customdata);
  const double now = BLI_time_now_seconds();
  float factor = 1.0f;
  if (now < md->end_time) {
    factor = pow((now - md->start_time) / (md->end_time - md->start_time), 2);
    md->screen->do_refresh = true;
  }
  screen_draw_move_highlight(win, md->screen, md->dir_axis, factor);
}

static void area_move_out_draw_cb(const wmWindow *win, void *userdata)
{
  const sAreaMoveData *md = static_cast<sAreaMoveData *>(userdata);
  const double now = BLI_time_now_seconds();
  float factor = 1.0f;
  if (now > md->end_time) {
    WM_draw_cb_exit(md->win, md->draw_callback);
    MEM_freeN(md);
    return;
  }
  if (now < md->end_time) {
    factor = 1.0f - pow((now - md->start_time) / (md->end_time - md->start_time), 2);
    md->screen->do_refresh = true;
  }
  screen_draw_move_highlight(win, md->screen, md->dir_axis, factor);
}

/* validate selection inside screen, set variables OK */
/* return false: init failed */
static bool area_move_init(bContext *C, wmOperator *op)
{
  bScreen *screen = CTX_wm_screen(C);
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);

  /* required properties */
  int x = RNA_int_get(op->ptr, "x");
  int y = RNA_int_get(op->ptr, "y");
  bool snap_prop = RNA_boolean_get(op->ptr, "snap");

  /* setup */
  ScrEdge *actedge = screen_geom_find_active_scredge(win, screen, x, y);

  if (area) {
    /* Favor scroll bars and action zones over expanded edge zone. */
    const int xy[2] = {x, y};
    if (ED_area_actionzone_find_xy(area, xy)) {
      return false;
    }
  }

  if (actedge == nullptr) {
    return false;
  }

  sAreaMoveData *md = MEM_callocN<sAreaMoveData>("sAreaMoveData");
  op->customdata = md;

  md->dir_axis = screen_geom_edge_is_horizontal(actedge) ? SCREEN_AXIS_H : SCREEN_AXIS_V;
  if (md->dir_axis == SCREEN_AXIS_H) {
    md->origval = actedge->v1->vec.y;
  }
  else {
    md->origval = actedge->v1->vec.x;
  }

  screen_geom_select_connected_edge(win, actedge);
  /* now all vertices with 'flag == 1' are the ones that can be moved. Move this to editflag */
  ED_screen_verts_iter(win, screen, v1)
  {
    v1->editflag = v1->flag;
  }

  bool use_bigger_smaller_snap = false;
  area_move_set_limits(
      win, screen, md->dir_axis, &md->bigger, &md->smaller, &use_bigger_smaller_snap);

  if (snap_prop) {
    md->snap_type = SNAP_FRACTION_AND_ADJACENT;
  }
  else {
    md->snap_type = use_bigger_smaller_snap ? SNAP_BIGGER_SMALLER_ONLY : SNAP_AREAGRID;
  }

  md->screen = screen;
  md->start_time = BLI_time_now_seconds();
  md->end_time = md->start_time + AREA_MOVE_LINE_FADEIN;
  md->draw_callback = WM_draw_cb_activate(CTX_wm_window(C), area_move_draw_cb, op);

  return true;
}

static int area_snap_calc_location(const bScreen *screen,
                                   const enum AreaMoveSnapType snap_type,
                                   const int delta,
                                   const int origval,
                                   const eScreenAxis dir_axis,
                                   const int bigger,
                                   const int smaller)
{
  BLI_assert(snap_type != SNAP_NONE);
  int m_cursor_final = -1;
  const int m_cursor = origval + delta;
  const int m_span = float(bigger + smaller);
  const int m_min = origval - smaller;
  // const int axis_max = axis_min + m_span;

  switch (snap_type) {
    case SNAP_AREAGRID: {
      m_cursor_final = m_cursor;
      if (!ELEM(delta, bigger, -smaller)) {
        m_cursor_final -= (m_cursor % AREAGRID);
        CLAMP(m_cursor_final, origval - smaller, origval + bigger);
      }

      /* Slight snap to vertical minimum and maximum. */
      const int snap_threshold = int(float(ED_area_headersize()) * 0.6f);
      if (m_cursor_final < (m_min + snap_threshold)) {
        m_cursor_final = m_min;
      }
      else if (m_cursor_final > (origval + bigger - snap_threshold)) {
        m_cursor_final = origval + bigger;
      }
    } break;

    case SNAP_BIGGER_SMALLER_ONLY:
      m_cursor_final = (m_cursor >= bigger) ? bigger : smaller;
      break;

    case SNAP_FRACTION_AND_ADJACENT: {
      const int axis = (dir_axis == SCREEN_AXIS_V) ? 0 : 1;
      int snap_dist_best = INT_MAX;
      {
        const float div_array[] = {
            0.0f,
            1.0f / 12.0f,
            2.0f / 12.0f,
            3.0f / 12.0f,
            4.0f / 12.0f,
            5.0f / 12.0f,
            6.0f / 12.0f,
            7.0f / 12.0f,
            8.0f / 12.0f,
            9.0f / 12.0f,
            10.0f / 12.0f,
            11.0f / 12.0f,
            1.0f,
        };
        /* Test the snap to the best division. */
        for (int i = 0; i < ARRAY_SIZE(div_array); i++) {
          const int m_cursor_test = m_min + round_fl_to_int(m_span * div_array[i]);
          const int snap_dist_test = abs(m_cursor - m_cursor_test);
          if (snap_dist_best >= snap_dist_test) {
            snap_dist_best = snap_dist_test;
            m_cursor_final = m_cursor_test;
          }
        }
      }

      LISTBASE_FOREACH (const ScrVert *, v1, &screen->vertbase) {
        if (!v1->editflag) {
          continue;
        }
        const int v_loc = (&v1->vec.x)[!axis];

        LISTBASE_FOREACH (const ScrVert *, v2, &screen->vertbase) {
          if (v2->editflag) {
            continue;
          }
          if (v_loc == (&v2->vec.x)[!axis]) {
            const int v_loc2 = (&v2->vec.x)[axis];
            /* Do not snap to the vertices at the ends. */
            if ((origval - smaller) < v_loc2 && v_loc2 < (origval + bigger)) {
              const int snap_dist_test = abs(m_cursor - v_loc2);
              if (snap_dist_best >= snap_dist_test) {
                snap_dist_best = snap_dist_test;
                m_cursor_final = v_loc2;
              }
            }
          }
        }
      }
      break;
    }
    case SNAP_NONE:
      break;
  }

  BLI_assert(ELEM(snap_type, SNAP_BIGGER_SMALLER_ONLY) ||
             IN_RANGE_INCL(m_cursor_final, origval - smaller, origval + bigger));

  return m_cursor_final;
}

/* moves selected screen edge amount of delta, used by split & move */
static void area_move_apply_do(bContext *C,
                               int delta,
                               const int origval,
                               const eScreenAxis dir_axis,
                               const int bigger,
                               const int smaller,
                               const enum AreaMoveSnapType snap_type)
{
  WorkspaceStatus status(C);
  status.item(IFACE_("Confirm"), ICON_MOUSE_LMB);
  status.item(IFACE_("Cancel"), ICON_EVENT_ESC);
  status.item_bool(IFACE_("Snap"), snap_type == SNAP_FRACTION_AND_ADJACENT, ICON_EVENT_CTRL);

  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);
  short final_loc = -1;
  bool doredraw = false;

  if (snap_type != SNAP_BIGGER_SMALLER_ONLY) {
    CLAMP(delta, -smaller, bigger);
  }

  if (snap_type == SNAP_NONE) {
    final_loc = origval + delta;
  }
  else {
    final_loc = area_snap_calc_location(
        screen, snap_type, delta, origval, dir_axis, bigger, smaller);
  }

  BLI_assert(final_loc != -1);
  short axis = (dir_axis == SCREEN_AXIS_V) ? 0 : 1;

  ED_screen_verts_iter(win, screen, v1)
  {
    if (v1->editflag) {
      short oldval = (&v1->vec.x)[axis];
      (&v1->vec.x)[axis] = final_loc;

      if (oldval == final_loc) {
        /* nothing will change to the other vertices either. */
        break;
      }
      doredraw = true;
    }
  }

  /* only redraw if we actually moved a screen vert, for AREAGRID */
  if (doredraw) {
    bool redraw_all = false;
    ED_screen_areas_iter (win, screen, area) {
      if (area->v1->editflag || area->v2->editflag || area->v3->editflag || area->v4->editflag) {
        if (ED_area_is_global(area)) {
          /* Snap to minimum or maximum for global areas. */
          int height = round_fl_to_int(screen_geom_area_height(area) / UI_SCALE_FAC);
          if (abs(height - area->global->size_min) < abs(height - area->global->size_max)) {
            area->global->cur_fixed_height = area->global->size_min;
          }
          else {
            area->global->cur_fixed_height = area->global->size_max;
          }

          screen->do_refresh = true;
          redraw_all = true;
        }
        ED_area_tag_redraw_no_rebuild(area);
      }
    }
    if (redraw_all) {
      ED_screen_areas_iter (win, screen, area) {
        ED_area_tag_redraw(area);
      }
    }

    ED_screen_global_areas_sync(win);

    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr); /* redraw everything */
    /* Update preview thumbnail */
    BKE_icon_changed(screen->id.icon_id);
  }
}

static void area_move_apply(bContext *C, wmOperator *op)
{
  sAreaMoveData *md = static_cast<sAreaMoveData *>(op->customdata);
  int delta = RNA_int_get(op->ptr, "delta");

  area_move_apply_do(C, delta, md->origval, md->dir_axis, md->bigger, md->smaller, md->snap_type);
}

static void area_move_exit(bContext *C, wmOperator *op)
{
  sAreaMoveData *md = static_cast<sAreaMoveData *>(op->customdata);
  if (md->draw_callback) {
    WM_draw_cb_exit(CTX_wm_window(C), md->draw_callback);
  }

  op->customdata = nullptr;

  md->start_time = BLI_time_now_seconds();
  md->end_time = md->start_time + AREA_MOVE_LINE_FADEOUT;
  md->win = CTX_wm_window(C);
  md->draw_callback = WM_draw_cb_activate(md->win, area_move_out_draw_cb, md);

  /* this makes sure aligned edges will result in aligned grabbing */
  BKE_screen_remove_double_scrverts(CTX_wm_screen(C));
  BKE_screen_remove_double_scredges(CTX_wm_screen(C));
  ED_workspace_status_text(C, nullptr);

  screen_modal_action_end();
}

static wmOperatorStatus area_move_exec(bContext *C, wmOperator *op)
{
  if (!area_move_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  area_move_apply(C, op);
  area_move_exit(C, op);

  return OPERATOR_FINISHED;
}

/* interaction callback */
static wmOperatorStatus area_move_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RNA_int_set(op->ptr, "x", event->xy[0]);
  RNA_int_set(op->ptr, "y", event->xy[1]);

  if (!area_move_init(C, op)) {
    return OPERATOR_PASS_THROUGH;
  }

  sAreaMoveData *md = static_cast<sAreaMoveData *>(op->customdata);

  WorkspaceStatus status(C);
  status.item(IFACE_("Confirm"), ICON_MOUSE_LMB);
  status.item(IFACE_("Cancel"), ICON_EVENT_ESC);
  status.item_bool(IFACE_("Snap"), md->snap_type == SNAP_FRACTION_AND_ADJACENT, ICON_EVENT_CTRL);

  /* add temp handler */
  screen_modal_action_begin();
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void area_move_cancel(bContext *C, wmOperator *op)
{

  RNA_int_set(op->ptr, "delta", 0);
  area_move_apply(C, op);
  area_move_exit(C, op);
}

/* modal callback for while moving edges */
static wmOperatorStatus area_move_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  sAreaMoveData *md = static_cast<sAreaMoveData *>(op->customdata);

  /* execute the events */
  switch (event->type) {
    case MOUSEMOVE: {
      int x = RNA_int_get(op->ptr, "x");
      int y = RNA_int_get(op->ptr, "y");

      const int delta = (md->dir_axis == SCREEN_AXIS_V) ? event->xy[0] - x : event->xy[1] - y;
      RNA_int_set(op->ptr, "delta", delta);

      area_move_apply(C, op);
      break;
    }
    case RIGHTMOUSE: {
      area_move_cancel(C, op);
      return OPERATOR_CANCELLED;
    }
    case EVT_MODAL_MAP: {
      switch (event->val) {
        case KM_MODAL_APPLY:
          area_move_exit(C, op);
          return OPERATOR_FINISHED;

        case KM_MODAL_CANCEL:
          area_move_cancel(C, op);
          return OPERATOR_CANCELLED;

        case KM_MODAL_SNAP_ON:
          if (md->snap_type != SNAP_BIGGER_SMALLER_ONLY) {
            md->snap_type = SNAP_FRACTION_AND_ADJACENT;
          }
          break;

        case KM_MODAL_SNAP_OFF:
          if (md->snap_type != SNAP_BIGGER_SMALLER_ONLY) {
            md->snap_type = SNAP_AREAGRID;
          }
          break;
      }
      WorkspaceStatus status(C);
      status.item(IFACE_("Confirm"), ICON_MOUSE_LMB);
      status.item(IFACE_("Cancel"), ICON_EVENT_ESC);
      status.item_bool(
          IFACE_("Snap"), md->snap_type == SNAP_FRACTION_AND_ADJACENT, ICON_EVENT_CTRL);
      break;
    }
    default: {
      break;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static void SCREEN_OT_area_move(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Move Area Edges";
  ot->description = "Move selected area edges";
  ot->idname = "SCREEN_OT_area_move";

  ot->exec = area_move_exec;
  ot->invoke = area_move_invoke;
  ot->cancel = area_move_cancel;
  ot->modal = area_move_modal;
  ot->poll = ED_operator_screen_mainwinactive; /* when mouse is over area-edge */

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_INTERNAL;

  /* rna */
  RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "delta", 0, INT_MIN, INT_MAX, "Delta", "", INT_MIN, INT_MAX);

  prop = RNA_def_boolean(ot->srna, "snap", false, "Snapping", "Enable snapping");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Split Area Operator
 * \{ */

/*
 * operator state vars:
 * fac              spit point
 * dir              direction #SCREEN_AXIS_V or #SCREEN_AXIS_H
 *
 * operator customdata:
 * area             pointer to (active) area
 * x, y             last used mouse pos
 * (more, see below)
 *
 * functions:
 *
 * init()   set default property values, find area based on context
 *
 * apply()  split area based on state vars
 *
 * exit()   cleanup, send notifier
 *
 * cancel() remove duplicated area
 *
 * callbacks:
 *
 * exec()   execute without any user interaction, based on state vars
 * call init(), apply(), exit()
 *
 * invoke() gets called on mouse click in action-widget
 * call init(), add modal handler
 * call apply() with initial motion
 *
 * modal()  accept modal events while doing it
 * call move-areas code with delta motion
 * call exit() or cancel() and remove handler
 */

struct sAreaSplitData {
  int origval;           /* for move areas */
  int bigger, smaller;   /* constraints for moving new edge */
  int delta;             /* delta move edge */
  int origmin, origsize; /* to calculate fac, for property storage */
  int previewmode;       /* draw preview-line, then split. */
  void *draw_callback;   /* call `screen_draw_split_preview` */
  bool do_snap;

  ScrEdge *nedge; /* new edge */
  ScrArea *sarea; /* start area */
  ScrArea *narea; /* new area */
};

static bool area_split_allowed(const ScrArea *area, const eScreenAxis dir_axis)
{
  if (!area || area->global) {
    /* Must be a non-global area. */
    return false;
  }

  if ((dir_axis == SCREEN_AXIS_V && area->winx <= 2 * AREAMINX * UI_SCALE_FAC) ||
      (dir_axis == SCREEN_AXIS_H && area->winy <= 2 * ED_area_headersize()))
  {
    /* Must be at least double minimum sizes to split into two. */
    return false;
  }

  return true;
}

static void area_split_draw_cb(const wmWindow * /*win*/, void *userdata)
{
  const wmOperator *op = static_cast<const wmOperator *>(userdata);

  sAreaSplitData *sd = static_cast<sAreaSplitData *>(op->customdata);
  const eScreenAxis dir_axis = eScreenAxis(RNA_enum_get(op->ptr, "direction"));

  if (area_split_allowed(sd->sarea, dir_axis)) {
    float fac = RNA_float_get(op->ptr, "factor");
    screen_draw_split_preview(sd->sarea, dir_axis, fac);
  }
}

/* generic init, menu case, doesn't need active area */
static bool area_split_menu_init(bContext *C, wmOperator *op)
{
  /* custom data */
  sAreaSplitData *sd = MEM_callocN<sAreaSplitData>("op_area_split");
  op->customdata = sd;

  sd->sarea = CTX_wm_area(C);

  return true;
}

/* generic init, no UI stuff here, assumes active area */
static bool area_split_init(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);

  /* required context */
  if (area == nullptr) {
    return false;
  }

  /* required properties */
  const eScreenAxis dir_axis = eScreenAxis(RNA_enum_get(op->ptr, "direction"));

  /* custom data */
  sAreaSplitData *sd = MEM_callocN<sAreaSplitData>("op_area_split");
  op->customdata = sd;

  sd->sarea = area;
  if (dir_axis == SCREEN_AXIS_V) {
    sd->origmin = area->v1->vec.x;
    sd->origsize = area->v4->vec.x - sd->origmin;
  }
  else {
    sd->origmin = area->v1->vec.y;
    sd->origsize = area->v2->vec.y - sd->origmin;
  }

  return true;
}

/* with area as center, sb is located at: 0=W, 1=N, 2=E, 3=S */
/* used with split operator */
static ScrEdge *area_findsharededge(bScreen *screen, ScrArea *area, ScrArea *sb)
{
  ScrVert *sav1 = area->v1;
  ScrVert *sav2 = area->v2;
  ScrVert *sav3 = area->v3;
  ScrVert *sav4 = area->v4;
  ScrVert *sbv1 = sb->v1;
  ScrVert *sbv2 = sb->v2;
  ScrVert *sbv3 = sb->v3;
  ScrVert *sbv4 = sb->v4;

  if (sav1 == sbv4 && sav2 == sbv3) { /* Area to right of sb = W. */
    return BKE_screen_find_edge(screen, sav1, sav2);
  }
  if (sav2 == sbv1 && sav3 == sbv4) { /* Area to bottom of sb = N. */
    return BKE_screen_find_edge(screen, sav2, sav3);
  }
  if (sav3 == sbv2 && sav4 == sbv1) { /* Area to left of sb = E. */
    return BKE_screen_find_edge(screen, sav3, sav4);
  }
  if (sav1 == sbv2 && sav4 == sbv3) { /* Area on top of sb = S. */
    return BKE_screen_find_edge(screen, sav1, sav4);
  }

  return nullptr;
}

/* do the split, return success */
static bool area_split_apply(bContext *C, wmOperator *op)
{
  const wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);
  sAreaSplitData *sd = (sAreaSplitData *)op->customdata;

  float fac = RNA_float_get(op->ptr, "factor");
  const eScreenAxis dir_axis = eScreenAxis(RNA_enum_get(op->ptr, "direction"));

  if (!area_split_allowed(sd->sarea, dir_axis)) {
    return false;
  }

  sd->narea = area_split(win, screen, sd->sarea, dir_axis, fac, false); /* false = no merge */

  if (sd->narea == nullptr) {
    return false;
  }

  sd->nedge = area_findsharededge(screen, sd->sarea, sd->narea);

  /* select newly created edge, prepare for moving edge */
  ED_screen_verts_iter(win, screen, sv)
  {
    sv->editflag = 0;
  }

  sd->nedge->v1->editflag = 1;
  sd->nedge->v2->editflag = 1;

  if (dir_axis == SCREEN_AXIS_H) {
    sd->origval = sd->nedge->v1->vec.y;
  }
  else {
    sd->origval = sd->nedge->v1->vec.x;
  }

  ED_area_tag_redraw(sd->sarea);
  ED_area_tag_redraw(sd->narea);

  WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);
  /* Update preview thumbnail */
  BKE_icon_changed(screen->id.icon_id);

  /* We have more than one area now, so reset window title. */
  WM_window_title_refresh(CTX_wm_manager(C), CTX_wm_window(C));

  return true;
}

static void area_split_exit(bContext *C, wmOperator *op)
{
  if (op->customdata) {
    sAreaSplitData *sd = (sAreaSplitData *)op->customdata;
    if (sd->sarea) {
      ED_area_tag_redraw(sd->sarea);
    }
    if (sd->narea) {
      ED_area_tag_redraw(sd->narea);
    }

    if (sd->draw_callback) {
      WM_draw_cb_exit(CTX_wm_window(C), sd->draw_callback);
    }

    MEM_freeN(sd);
    op->customdata = nullptr;
  }

  WM_cursor_modal_restore(CTX_wm_window(C));
  WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);
  ED_workspace_status_text(C, nullptr);

  /* this makes sure aligned edges will result in aligned grabbing */
  BKE_screen_remove_double_scrverts(CTX_wm_screen(C));
  BKE_screen_remove_double_scredges(CTX_wm_screen(C));

  screen_modal_action_end();
}

static void area_split_preview_update_cursor(bContext *C, wmOperator *op)
{
  sAreaSplitData *sd = (sAreaSplitData *)op->customdata;
  const eScreenAxis dir_axis = eScreenAxis(RNA_enum_get(op->ptr, "direction"));
  if (area_split_allowed(sd->sarea, dir_axis)) {
    WM_cursor_set(CTX_wm_window(C),
                  (dir_axis == SCREEN_AXIS_H) ? WM_CURSOR_H_SPLIT : WM_CURSOR_V_SPLIT);
  }
  else {
    WM_cursor_set(CTX_wm_window(C), WM_CURSOR_STOP);
  }
}

/* UI callback, adds new handler */
static wmOperatorStatus area_split_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);

  /* no full window splitting allowed */
  BLI_assert(screen->state == SCREENNORMAL);

  PropertyRNA *prop_dir = RNA_struct_find_property(op->ptr, "direction");
  PropertyRNA *prop_factor = RNA_struct_find_property(op->ptr, "factor");
  PropertyRNA *prop_cursor = RNA_struct_find_property(op->ptr, "cursor");

  eScreenAxis dir_axis;
  if (event->type == EVT_ACTIONZONE_AREA) {
    sActionzoneData *sad = static_cast<sActionzoneData *>(event->customdata);

    if (sad == nullptr || sad->modifier > 0) {
      return OPERATOR_PASS_THROUGH;
    }

    /* verify *sad itself */
    if (sad->sa1 == nullptr || sad->az == nullptr) {
      return OPERATOR_PASS_THROUGH;
    }

    /* is this our *sad? if areas not equal it should be passed on */
    if (CTX_wm_area(C) != sad->sa1 || sad->sa1 != sad->sa2) {
      return OPERATOR_PASS_THROUGH;
    }

    /* The factor will be close to 1.0f when near the top-left and the bottom-right corners. */
    const float factor_v = float(event->xy[1] - sad->sa1->v1->vec.y) / float(sad->sa1->winy);
    const float factor_h = float(event->xy[0] - sad->sa1->v1->vec.x) / float(sad->sa1->winx);
    const bool is_left = factor_v < 0.5f;
    const bool is_bottom = factor_h < 0.5f;
    const bool is_right = !is_left;
    const bool is_top = !is_bottom;
    float factor;

    /* Prepare operator state vars. */
    if (SCREEN_DIR_IS_VERTICAL(sad->gesture_dir)) {
      dir_axis = SCREEN_AXIS_H;
      factor = factor_h;
    }
    else {
      dir_axis = SCREEN_AXIS_V;
      factor = factor_v;
    }

    if ((is_top && is_left) || (is_bottom && is_right)) {
      factor = 1.0f - factor;
    }

    RNA_property_float_set(op->ptr, prop_factor, factor);

    RNA_property_enum_set(op->ptr, prop_dir, dir_axis);

    /* general init, also non-UI case, adds customdata, sets area and defaults */
    if (!area_split_init(C, op)) {
      return OPERATOR_PASS_THROUGH;
    }
  }
  else if (RNA_property_is_set(op->ptr, prop_dir)) {
    ScrArea *area = CTX_wm_area(C);
    if (area == nullptr) {
      return OPERATOR_CANCELLED;
    }
    dir_axis = eScreenAxis(RNA_property_enum_get(op->ptr, prop_dir));
    if (dir_axis == SCREEN_AXIS_H) {
      RNA_property_float_set(
          op->ptr, prop_factor, float(event->xy[0] - area->v1->vec.x) / float(area->winx));
    }
    else {
      RNA_property_float_set(
          op->ptr, prop_factor, float(event->xy[1] - area->v1->vec.y) / float(area->winy));
    }

    if (!area_split_init(C, op)) {
      return OPERATOR_CANCELLED;
    }
  }
  else {
    int event_co[2];

    /* retrieve initial mouse coord, so we can find the active edge */
    if (RNA_property_is_set(op->ptr, prop_cursor)) {
      RNA_property_int_get_array(op->ptr, prop_cursor, event_co);
    }
    else {
      copy_v2_v2_int(event_co, event->xy);
    }

    rcti window_rect;
    WM_window_rect_calc(win, &window_rect);

    ScrEdge *actedge = screen_geom_area_map_find_active_scredge(
        AREAMAP_FROM_SCREEN(screen), &window_rect, event_co[0], event_co[1]);
    if (actedge == nullptr) {
      return OPERATOR_CANCELLED;
    }

    dir_axis = screen_geom_edge_is_horizontal(actedge) ? SCREEN_AXIS_V : SCREEN_AXIS_H;

    RNA_property_enum_set(op->ptr, prop_dir, dir_axis);

    /* special case, adds customdata, sets defaults */
    if (!area_split_menu_init(C, op)) {
      return OPERATOR_CANCELLED;
    }
  }

  sAreaSplitData *sd = (sAreaSplitData *)op->customdata;

  if (event->type == EVT_ACTIONZONE_AREA) {
    /* do the split */
    if (area_split_apply(C, op)) {
      area_move_set_limits(win, screen, dir_axis, &sd->bigger, &sd->smaller, nullptr);

      /* add temp handler for edge move or cancel */
      screen_modal_action_begin();
      WM_event_add_modal_handler(C, op);

      return OPERATOR_RUNNING_MODAL;
    }
  }
  else {
    sd->previewmode = 1;
    sd->draw_callback = WM_draw_cb_activate(win, area_split_draw_cb, op);
    /* add temp handler for edge move or cancel */
    WM_event_add_modal_handler(C, op);
    area_split_preview_update_cursor(C, op);

    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_PASS_THROUGH;
}

/* function to be called outside UI context, or for redo */
static wmOperatorStatus area_split_exec(bContext *C, wmOperator *op)
{
  if (!area_split_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  area_split_apply(C, op);
  area_split_exit(C, op);

  return OPERATOR_FINISHED;
}

static void area_split_cancel(bContext *C, wmOperator *op)
{
  sAreaSplitData *sd = (sAreaSplitData *)op->customdata;

  if (sd->previewmode) {
    /* pass */
  }
  else {
    if (screen_area_join(C, op->reports, CTX_wm_screen(C), sd->sarea, sd->narea)) {
      if (CTX_wm_area(C) == sd->narea) {
        CTX_wm_area_set(C, nullptr);
        CTX_wm_region_set(C, nullptr);
      }
      sd->narea = nullptr;
    }
  }
  area_split_exit(C, op);
}

static wmOperatorStatus area_split_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  sAreaSplitData *sd = (sAreaSplitData *)op->customdata;
  PropertyRNA *prop_dir = RNA_struct_find_property(op->ptr, "direction");
  bool update_factor = false;

  /* execute the events */
  switch (event->type) {
    case MOUSEMOVE:
      update_factor = true;
      break;

    case LEFTMOUSE:
      if (sd->previewmode) {
        float inner[4] = {1.0f, 1.0f, 1.0f, 0.1f};
        float outline[4] = {1.0f, 1.0f, 1.0f, 0.3f};
        screen_animate_area_highlight(CTX_wm_window(C),
                                      CTX_wm_screen(C),
                                      &sd->sarea->totrct,
                                      inner,
                                      outline,
                                      AREA_SPLIT_FADEOUT);
        area_split_apply(C, op);
        area_split_exit(C, op);
        return OPERATOR_FINISHED;
      }
      else {
        if (event->val == KM_RELEASE) { /* mouse up */
          area_split_exit(C, op);
          return OPERATOR_FINISHED;
        }
      }
      break;

    case MIDDLEMOUSE:
    case EVT_TABKEY:
      if (sd->previewmode == 0) {
        /* pass */
      }
      else {
        if (event->val == KM_PRESS) {
          if (sd->sarea) {
            const eScreenAxis dir_axis = eScreenAxis(RNA_property_enum_get(op->ptr, prop_dir));
            RNA_property_enum_set(
                op->ptr, prop_dir, (dir_axis == SCREEN_AXIS_V) ? SCREEN_AXIS_H : SCREEN_AXIS_V);
            area_split_preview_update_cursor(C, op);
            update_factor = true;
          }
        }
      }

      break;

    case RIGHTMOUSE: /* cancel operation */
    case EVT_ESCKEY:
      area_split_cancel(C, op);
      return OPERATOR_CANCELLED;

    case EVT_LEFTCTRLKEY:
    case EVT_RIGHTCTRLKEY:
      sd->do_snap = event->val == KM_PRESS;
      update_factor = true;
      break;
    default: {
      break;
    }
  }

  if (update_factor) {
    const eScreenAxis dir_axis = eScreenAxis(RNA_property_enum_get(op->ptr, prop_dir));

    sd->delta = (dir_axis == SCREEN_AXIS_V) ? event->xy[0] - sd->origval :
                                              event->xy[1] - sd->origval;

    if (sd->previewmode == 0) {
      if (sd->do_snap) {
        const int snap_loc = area_snap_calc_location(CTX_wm_screen(C),
                                                     SNAP_FRACTION_AND_ADJACENT,
                                                     sd->delta,
                                                     sd->origval,
                                                     dir_axis,
                                                     sd->bigger,
                                                     sd->smaller);
        sd->delta = snap_loc - sd->origval;
        area_move_apply_do(C,
                           sd->delta,
                           sd->origval,
                           dir_axis,
                           sd->bigger,
                           sd->smaller,
                           SNAP_FRACTION_AND_ADJACENT);
      }
      else {
        area_move_apply_do(
            C, sd->delta, sd->origval, dir_axis, sd->bigger, sd->smaller, SNAP_NONE);
      }
    }
    else {
      if (sd->sarea) {
        ED_area_tag_redraw(sd->sarea);
      }

      area_split_preview_update_cursor(C, op);

      /* area context not set */
      sd->sarea = BKE_screen_find_area_xy(CTX_wm_screen(C), SPACE_TYPE_ANY, event->xy);

      if (sd->sarea) {
        ScrArea *area = sd->sarea;
        if (dir_axis == SCREEN_AXIS_V) {
          sd->origmin = area->v1->vec.x;
          sd->origsize = area->v4->vec.x - sd->origmin;
        }
        else {
          sd->origmin = area->v1->vec.y;
          sd->origsize = area->v2->vec.y - sd->origmin;
        }

        if (sd->do_snap) {
          area->v1->editflag = area->v2->editflag = area->v3->editflag = area->v4->editflag = 1;

          const int snap_loc = area_snap_calc_location(CTX_wm_screen(C),
                                                       SNAP_FRACTION_AND_ADJACENT,
                                                       sd->delta,
                                                       sd->origval,
                                                       dir_axis,
                                                       sd->origmin + sd->origsize,
                                                       -sd->origmin);

          area->v1->editflag = area->v2->editflag = area->v3->editflag = area->v4->editflag = 0;
          sd->delta = snap_loc - sd->origval;
        }

        ED_area_tag_redraw(sd->sarea);
      }

      CTX_wm_screen(C)->do_draw = true;
    }

    float fac = float(sd->delta + sd->origval - sd->origmin) / sd->origsize;
    RNA_float_set(op->ptr, "factor", fac);
  }

  return OPERATOR_RUNNING_MODAL;
}

static const EnumPropertyItem prop_direction_items[] = {
    {SCREEN_AXIS_H, "HORIZONTAL", 0, "Horizontal", ""},
    {SCREEN_AXIS_V, "VERTICAL", 0, "Vertical", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void SCREEN_OT_area_split(wmOperatorType *ot)
{
  ot->name = "Split Area";
  ot->description = "Split selected area into new windows";
  ot->idname = "SCREEN_OT_area_split";

  ot->exec = area_split_exec;
  ot->invoke = area_split_invoke;
  ot->modal = area_split_modal;
  ot->cancel = area_split_cancel;

  ot->poll = screen_active_editable;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_INTERNAL;

  /* rna */
  RNA_def_enum(ot->srna, "direction", prop_direction_items, SCREEN_AXIS_H, "Direction", "");
  RNA_def_float(ot->srna, "factor", 0.5f, 0.0, 1.0, "Factor", "", 0.0, 1.0);
  RNA_def_int_vector(
      ot->srna, "cursor", 2, nullptr, INT_MIN, INT_MAX, "Cursor", "", INT_MIN, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scale Region Edge Operator
 * \{ */

struct RegionMoveData {
  AZone *az;
  ARegion *region;
  ScrArea *area;
  wmWindow *win;
  void *draw_callback;
  int bigger, smaller, origval;
  int orig_xy[2];
  int maxsize;
  AZEdge edge;
};

static bool is_split_edge(const int alignment, const AZEdge edge)
{
  return ((alignment == RGN_ALIGN_BOTTOM) && (edge == AE_TOP_TO_BOTTOMRIGHT)) ||
         ((alignment == RGN_ALIGN_TOP) && (edge == AE_BOTTOM_TO_TOPLEFT)) ||
         ((alignment == RGN_ALIGN_LEFT) && (edge == AE_RIGHT_TO_TOPLEFT)) ||
         ((alignment == RGN_ALIGN_RIGHT) && (edge == AE_LEFT_TO_TOPRIGHT));
}

static void region_scale_draw_cb(const wmWindow * /*win*/, void *userdata)
{
  const wmOperator *op = static_cast<const wmOperator *>(userdata);
  RegionMoveData *rmd = static_cast<RegionMoveData *>(op->customdata);
  screen_draw_region_scale_highlight(rmd->region);
}

static void region_scale_exit(wmOperator *op)
{
  RegionMoveData *rmd = static_cast<RegionMoveData *>(op->customdata);
  WM_draw_cb_exit(rmd->win, rmd->draw_callback);

  MEM_freeN(rmd);
  op->customdata = nullptr;

  screen_modal_action_end();
}

static wmOperatorStatus region_scale_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  sActionzoneData *sad = static_cast<sActionzoneData *>(event->customdata);

  if (event->type != EVT_ACTIONZONE_REGION) {
    BKE_report(op->reports, RPT_ERROR, "Can only scale region size from an action zone");
    return OPERATOR_CANCELLED;
  }

  AZone *az = sad->az;

  if (az->region) {
    RegionMoveData *rmd = MEM_callocN<RegionMoveData>("RegionMoveData");

    op->customdata = rmd;

    rmd->az = az;
    /* special case for region within region - this allows the scale of
     * the parent region if the azone edge is not the edge splitting
     * both regions */
    if ((az->region->alignment & RGN_SPLIT_PREV) && az->region->prev &&
        !is_split_edge(RGN_ALIGN_ENUM_FROM_MASK(az->region->alignment), az->edge))
    {
      rmd->region = az->region->prev;
    }
    /* Flag to always forward scaling to the previous region. */
    else if (az->region->prev && (az->region->alignment & RGN_SPLIT_SCALE_PREV)) {
      rmd->region = az->region->prev;
    }
    else {
      rmd->region = az->region;
    }
    rmd->area = sad->sa1;
    rmd->edge = az->edge;
    copy_v2_v2_int(rmd->orig_xy, event->xy);
    rmd->maxsize = ED_area_max_regionsize(rmd->area, rmd->region, rmd->edge);

    /* if not set we do now, otherwise it uses type */
    if (rmd->region->sizex == 0) {
      rmd->region->sizex = rmd->region->winx;
    }
    if (rmd->region->sizey == 0) {
      rmd->region->sizey = rmd->region->winy;
    }

    /* Reset our saved widths if the region is hidden.
     * Otherwise you can't drag it out a second time. */
    if (rmd->region->flag & RGN_FLAG_HIDDEN) {
      if (ELEM(rmd->edge, AE_LEFT_TO_TOPRIGHT, AE_RIGHT_TO_TOPLEFT)) {
        rmd->region->winx = rmd->region->sizex = 0;
      }
      else {
        rmd->region->winy = rmd->region->sizey = 0;
      }
    }

    /* Now copy to region-move-data. */
    if (ELEM(rmd->edge, AE_LEFT_TO_TOPRIGHT, AE_RIGHT_TO_TOPLEFT)) {
      rmd->origval = rmd->region->sizex;
    }
    else {
      rmd->origval = rmd->region->sizey;
    }

    CLAMP(rmd->maxsize, 0, 1000);

    rmd->win = CTX_wm_window(C);
    rmd->draw_callback = WM_draw_cb_activate(CTX_wm_window(C), region_scale_draw_cb, op);
    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);

    /* add temp handler */
    screen_modal_action_begin();
    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_FINISHED;
}

static void region_scale_validate_size(RegionMoveData *rmd)
{
  if ((rmd->region->flag & RGN_FLAG_HIDDEN) == 0) {
    short *size, maxsize = -1;

    if (ELEM(rmd->edge, AE_LEFT_TO_TOPRIGHT, AE_RIGHT_TO_TOPLEFT)) {
      size = &rmd->region->sizex;
    }
    else {
      size = &rmd->region->sizey;
    }

    maxsize = rmd->maxsize - (UI_UNIT_Y / UI_SCALE_FAC);

    if (*size > maxsize && maxsize > 0) {
      *size = maxsize;
    }
  }
}

static void region_scale_toggle_hidden(bContext *C, RegionMoveData *rmd)
{
  /* hidden areas may have bad 'View2D.cur' value,
   * correct before displaying. see #45156 */
  if (rmd->region->flag & RGN_FLAG_HIDDEN) {
    UI_view2d_curRect_validate(&rmd->region->v2d);
  }

  region_toggle_hidden(C, rmd->region, false);
  region_scale_validate_size(rmd);

  if ((rmd->region->flag & RGN_FLAG_HIDDEN) == 0) {
    if (rmd->region->regiontype == RGN_TYPE_HEADER) {
      ARegion *region_tool_header = BKE_area_find_region_type(rmd->area, RGN_TYPE_TOOL_HEADER);
      if (region_tool_header != nullptr) {
        if ((region_tool_header->flag & RGN_FLAG_HIDDEN_BY_USER) == 0 &&
            (region_tool_header->flag & RGN_FLAG_HIDDEN) != 0)
        {
          region_toggle_hidden(C, region_tool_header, false);
        }
      }
    }
  }
}

static wmOperatorStatus region_scale_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  RegionMoveData *rmd = static_cast<RegionMoveData *>(op->customdata);
  int delta;

  /* execute the events */
  switch (event->type) {
    case MOUSEMOVE: {
      const float aspect = (rmd->region->v2d.flag & V2D_IS_INIT) ?
                               (BLI_rctf_size_x(&rmd->region->v2d.cur) /
                                (BLI_rcti_size_x(&rmd->region->v2d.mask) + 1)) :
                               1.0f;
      const int snap_size_threshold = (U.widget_unit * 2) / aspect;
      bool size_changed = false;

      if (ELEM(rmd->edge, AE_LEFT_TO_TOPRIGHT, AE_RIGHT_TO_TOPLEFT)) {
        delta = event->xy[0] - rmd->orig_xy[0];
        if (rmd->edge == AE_LEFT_TO_TOPRIGHT) {
          delta = -delta;
        }

        /* region sizes now get multiplied */
        delta /= UI_SCALE_FAC;

        const int size_no_snap = rmd->origval + delta;
        rmd->region->sizex = size_no_snap;
        /* Clamp before snapping, so the snapping doesn't use a size that's invalid anyway. It will
         * check for and respect the max-width too. */
        CLAMP(rmd->region->sizex, 0, rmd->maxsize);

        if (rmd->region->runtime->type->snap_size) {
          short sizex_test = rmd->region->runtime->type->snap_size(
              rmd->region, rmd->region->sizex, 0);
          if ((abs(rmd->region->sizex - sizex_test) < snap_size_threshold) &&
              /* Don't snap to a new size if that would exceed the maximum width. */
              sizex_test <= rmd->maxsize)
          {
            rmd->region->sizex = sizex_test;
          }
        }
        BLI_assert(rmd->region->sizex <= rmd->maxsize);

        if (size_no_snap < UI_UNIT_X / aspect) {
          rmd->region->sizex = rmd->origval;
          if (!(rmd->region->flag & RGN_FLAG_HIDDEN)) {
            region_scale_toggle_hidden(C, rmd);
          }
        }
        else if (rmd->region->flag & RGN_FLAG_HIDDEN) {
          region_scale_toggle_hidden(C, rmd);
        }

        /* Hiding/unhiding is handled above, but still fix the size as requested. */
        if (rmd->region->flag & RGN_FLAG_NO_USER_RESIZE) {
          rmd->region->sizex = rmd->origval;
        }

        if (rmd->region->sizex != rmd->origval) {
          size_changed = true;
        }
      }
      else {
        delta = event->xy[1] - rmd->orig_xy[1];
        if (rmd->edge == AE_BOTTOM_TO_TOPLEFT) {
          delta = -delta;
        }

        /* region sizes now get multiplied */
        delta /= UI_SCALE_FAC;

        const int size_no_snap = rmd->origval + delta;
        rmd->region->sizey = size_no_snap;
        /* Clamp before snapping, so the snapping doesn't use a size that's invalid anyway. It will
         * check for and respect the max-height too. */
        CLAMP(rmd->region->sizey, 0, rmd->maxsize);

        if (rmd->region->runtime->type->snap_size) {
          short sizey_test = rmd->region->runtime->type->snap_size(
              rmd->region, rmd->region->sizey, 1);
          if ((abs(rmd->region->sizey - sizey_test) < snap_size_threshold) &&
              /* Don't snap to a new size if that would exceed the maximum height. */
              (sizey_test <= rmd->maxsize))
          {
            rmd->region->sizey = sizey_test;
          }
        }
        BLI_assert(rmd->region->sizey <= rmd->maxsize);

        /* NOTE: `UI_UNIT_Y / 4` means you need to drag the footer and execute region
         * almost all the way down for it to become hidden, this is done
         * otherwise its too easy to do this by accident. */
        if (size_no_snap < (UI_UNIT_Y / 4) / aspect) {
          rmd->region->sizey = rmd->origval;
          if (!(rmd->region->flag & RGN_FLAG_HIDDEN)) {
            region_scale_toggle_hidden(C, rmd);
          }
        }
        else if (rmd->region->flag & RGN_FLAG_HIDDEN) {
          region_scale_toggle_hidden(C, rmd);
        }

        /* Hiding/unhiding is handled above, but still fix the size as requested. */
        if (rmd->region->flag & RGN_FLAG_NO_USER_RESIZE) {
          rmd->region->sizey = rmd->origval;
        }

        if (rmd->region->sizey != rmd->origval) {
          size_changed = true;
        }
      }
      if (size_changed && rmd->region->runtime->type->on_user_resize) {
        rmd->region->runtime->type->on_user_resize(rmd->region);
      }
      if (size_changed) {
        if (ELEM(rmd->edge, AE_LEFT_TO_TOPRIGHT, AE_RIGHT_TO_TOPLEFT)) {
          WM_cursor_set(CTX_wm_window(C), WM_CURSOR_X_MOVE);
        }
        else {
          WM_cursor_set(CTX_wm_window(C), WM_CURSOR_Y_MOVE);
        }
      }
      ED_area_tag_redraw(rmd->area);
      WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);

      break;
    }
    case LEFTMOUSE:
      if (event->val == KM_RELEASE) {
        if (len_manhattan_v2v2_int(event->xy, rmd->orig_xy) <= WM_event_drag_threshold(event)) {
          if (rmd->region->flag & RGN_FLAG_HIDDEN) {
            region_scale_toggle_hidden(C, rmd);
          }
          else if (rmd->region->flag & RGN_FLAG_TOO_SMALL) {
            region_scale_validate_size(rmd);
          }

          ED_area_tag_redraw(rmd->area);
          WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);
        }

        region_scale_exit(op);

        return OPERATOR_FINISHED;
      }
      break;

    case EVT_ESCKEY:
      region_scale_exit(op);
      WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);
      return OPERATOR_CANCELLED;
    default: {
      break;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static void region_scale_cancel(bContext * /*C*/, wmOperator *op)
{
  region_scale_exit(op);
}

static void SCREEN_OT_region_scale(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Scale Region Size";
  ot->description = "Scale selected area";
  ot->idname = "SCREEN_OT_region_scale";

  ot->invoke = region_scale_invoke;
  ot->modal = region_scale_modal;
  ot->cancel = region_scale_cancel;

  ot->poll = ED_operator_areaactive;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame Change Operator
 * \{ */

static bool screen_animation_region_supports_time_follow(eSpace_Type spacetype,
                                                         eRegion_Type regiontype)
{
  return (regiontype == RGN_TYPE_WINDOW &&
          ELEM(spacetype, SPACE_SEQ, SPACE_GRAPH, SPACE_ACTION, SPACE_NLA)) ||
         (spacetype == SPACE_CLIP && regiontype == RGN_TYPE_PREVIEW);
}

void ED_areas_do_frame_follow(bContext *C, bool center_view)
{
  bScreen *screen_ctx = CTX_wm_screen(C);
  if (!(screen_ctx->redraws_flag & TIME_FOLLOW)) {
    return;
  }

  const int current_frame = CTX_data_scene(C)->r.cfra;
  wmWindowManager *wm = CTX_wm_manager(C);
  LISTBASE_FOREACH (wmWindow *, window, &wm->windows) {
    const bScreen *screen = WM_window_get_active_screen(window);

    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
        /* Only frame/center the current-frame indicator here if editor type supports it */
        if (!screen_animation_region_supports_time_follow(eSpace_Type(area->spacetype),
                                                          eRegion_Type(region->regiontype)))
        {
          continue;
        }

        if ((current_frame >= region->v2d.cur.xmin) && (current_frame <= region->v2d.cur.xmax)) {
          /* The current-frame indicator is already in view, do nothing. */
          continue;
        }

        const float w = BLI_rctf_size_x(&region->v2d.cur);

        if (center_view) {
          region->v2d.cur.xmax = current_frame + (w / 2);
          region->v2d.cur.xmin = current_frame - (w / 2);
          continue;
        }
        if (current_frame < region->v2d.cur.xmin) {
          region->v2d.cur.xmax = current_frame;
          region->v2d.cur.xmin = region->v2d.cur.xmax - w;
        }
        else {
          region->v2d.cur.xmin = current_frame;
          region->v2d.cur.xmax = region->v2d.cur.xmin + w;
        }
      }
    }
  }
}

/* function to be called outside UI context, or for redo */
static wmOperatorStatus frame_offset_exec(bContext *C, wmOperator *op)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  if (!scene) {
    return OPERATOR_CANCELLED;
  }

  int delta = RNA_int_get(op->ptr, "delta");

  /* In order to jump from e.g. 1.5 to 1 the delta needs to be incremented by 1 since the sub-frame
   * is always zeroed. Otherwise it would jump to 0. */
  if (delta < 0 && scene->r.subframe > 0) {
    delta += 1;
  }
  scene->r.cfra += delta;
  FRAMENUMBER_MIN_CLAMP(scene->r.cfra);
  scene->r.subframe = 0.0f;

  ED_areas_do_frame_follow(C, false);

  blender::ed::vse::sync_active_scene_and_time_with_scene_strip(*C);

  DEG_id_tag_update(&scene->id, ID_RECALC_FRAME_CHANGE);

  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_frame_offset(wmOperatorType *ot)
{
  ot->name = "Frame Offset";
  ot->idname = "SCREEN_OT_frame_offset";
  ot->description = "Move current frame forward/backward by a given number";

  ot->exec = frame_offset_exec;

  ot->poll = operator_screenactive_norender;
  ot->flag = OPTYPE_UNDO_GROUPED;
  ot->undo_group = "Frame Change";

  /* rna */
  RNA_def_int(ot->srna, "delta", 0, INT_MIN, INT_MAX, "Delta", "", INT_MIN, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame Jump Operator
 * \{ */

/* function to be called outside UI context, or for redo */
static wmOperatorStatus frame_jump_exec(bContext *C, wmOperator *op)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  if (!scene) {
    return OPERATOR_CANCELLED;
  }
  wmTimer *animtimer = CTX_wm_screen(C)->animtimer;

  /* Don't change scene->r.cfra directly if animtimer is running as this can cause
   * first/last frame not to be actually shown (bad since for example physics
   * simulations aren't reset properly).
   */
  if (animtimer) {
    ScreenAnimData *sad = static_cast<ScreenAnimData *>(animtimer->customdata);

    sad->flag |= ANIMPLAY_FLAG_USE_NEXT_FRAME;

    if (RNA_boolean_get(op->ptr, "end")) {
      sad->nextfra = PEFRA;
    }
    else {
      sad->nextfra = PSFRA;
    }
  }
  else {
    if (RNA_boolean_get(op->ptr, "end")) {
      scene->r.cfra = PEFRA;
    }
    else {
      scene->r.cfra = PSFRA;
    }

    ED_areas_do_frame_follow(C, true);

    blender::ed::vse::sync_active_scene_and_time_with_scene_strip(*C);

    DEG_id_tag_update(&scene->id, ID_RECALC_FRAME_CHANGE);

    WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
  }

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_frame_jump(wmOperatorType *ot)
{
  ot->name = "Jump to Endpoint";
  ot->description = "Jump to first/last frame in frame range";
  ot->idname = "SCREEN_OT_frame_jump";

  ot->exec = frame_jump_exec;

  ot->poll = operator_screenactive_norender;
  ot->flag = OPTYPE_UNDO_GROUPED;
  ot->undo_group = "Frame Change";

  /* rna */
  RNA_def_boolean(
      ot->srna, "end", false, "Last Frame", "Jump to the last frame of the frame range");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Time Jump Operator
 * \{ */

/* function to be called outside UI context, or for redo */
static wmOperatorStatus frame_jump_delta_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_wm_space_seq(C) != nullptr ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  const bool backward = RNA_boolean_get(op->ptr, "backward");

  if (scene == nullptr) {
    return OPERATOR_CANCELLED;
  }

  float delta = scene->r.time_jump_delta;

  if (scene->r.time_jump_unit == SCE_TIME_JUMP_SECOND) {
    delta *= scene->r.frs_sec / scene->r.frs_sec_base;
  }

  int step = (int)delta;
  float fraction = delta - step;
  if (backward) {
    scene->r.cfra -= step;
    scene->r.subframe -= fraction;
  }
  else {
    scene->r.cfra += step;
    scene->r.subframe += fraction;
  }

  /* Check if subframe has a non-fractional component, and roll that into cfra. */
  if (scene->r.subframe < 0.0f || scene->r.subframe >= 1.0f) {
    const float subframe_offset = floorf(scene->r.subframe);
    const int frame_offset = (int)subframe_offset;
    scene->r.cfra += frame_offset;
    scene->r.subframe -= subframe_offset;
  }

  ED_areas_do_frame_follow(C, true);
  blender::ed::vse::sync_active_scene_and_time_with_scene_strip(*C);

  DEG_id_tag_update(&scene->id, ID_RECALC_FRAME_CHANGE);

  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_time_jump(wmOperatorType *ot)
{
  ot->name = "Jump Time by Delta";
  ot->description = "Jump forward/backward by a given number of frames or seconds";
  ot->idname = "SCREEN_OT_time_jump";

  ot->exec = frame_jump_delta_exec;

  ot->poll = operator_screenactive_norender;
  ot->flag = OPTYPE_UNDO_GROUPED;
  ot->undo_group = "Frame Change";

  /* rna */
  RNA_def_boolean(ot->srna, "backward", false, "Backwards", "Jump backwards in time");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Jump to Key-Frame Operator
 * \{ */

static void keylist_from_dopesheet(bContext &C, AnimKeylist &keylist)
{
  bAnimContext ac;

  if (ANIM_animdata_get_context(&C, &ac) == 0) {
    return;
  }
  BLI_assert(ac.area->spacetype == SPACE_ACTION);
  summary_to_keylist(&ac, &keylist, 0, {-FLT_MAX, FLT_MAX});
}

static void keylist_from_graph_editor(bContext &C, AnimKeylist &keylist)
{
  bAnimContext ac;

  if (ANIM_animdata_get_context(&C, &ac) == 0) {
    return;
  }

  ListBase anim_data = blender::ed::graph::get_editable_fcurves(ac);

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = static_cast<FCurve *>(ale->key_data);
    if (!fcu->bezt) {
      continue;
    }

    const bool use_nla_mapping = ANIM_nla_mapping_allowed(ale);
    fcurve_to_keylist(ale->adt, fcu, &keylist, 0, {-FLT_MAX, FLT_MAX}, use_nla_mapping);
  }

  ANIM_animdata_freelist(&anim_data);
}

/* This is used for all editors where a more specific function isn't implemented. */
static void keylist_fallback_for_keyframe_jump(bContext &C, Scene *scene, AnimKeylist &keylist)
{
  bDopeSheet ads = {nullptr};

  /* Speed up dummy dope-sheet context with flags to perform necessary filtering. */
  if ((scene->flag & SCE_KEYS_NO_SELONLY) == 0) {
    /* Only selected channels are included. */
    ads.filterflag |= ADS_FILTER_ONLYSEL;
  }

  /* populate tree with keyframe nodes */
  scene_to_keylist(&ads, scene, &keylist, 0, {-FLT_MAX, FLT_MAX});

  /* Return early when invoked from sequencer with sequencer scene. Objects may belong to different
   * scenes and are irrelevant. */
  if (CTX_wm_space_seq(&C) != nullptr && scene == CTX_data_sequencer_scene(&C)) {
    return;
  }

  Object *ob = CTX_data_active_object(&C);
  if (ob) {
    ob_to_keylist(&ads, ob, &keylist, 0, {-FLT_MAX, FLT_MAX});

    if (ob->type == OB_GREASE_PENCIL) {
      const bool active_layer_only = !(scene->flag & SCE_KEYS_NO_SELONLY);
      grease_pencil_data_block_to_keylist(
          nullptr, static_cast<const GreasePencil *>(ob->data), &keylist, 0, active_layer_only);
    }
  }

  {
    Mask *mask = CTX_data_edit_mask(&C);
    if (mask) {
      MaskLayer *masklay = BKE_mask_layer_active(mask);
      mask_to_keylist(&ads, masklay, &keylist);
    }
  }
}

/* function to be called outside UI context, or for redo */
static wmOperatorStatus keyframe_jump_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_wm_space_seq(C) != nullptr ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  const bool next = RNA_boolean_get(op->ptr, "next");
  bool done = false;

  /* sanity checks */
  if (scene == nullptr) {
    return OPERATOR_CANCELLED;
  }

  AnimKeylist *keylist = ED_keylist_create();

  ScrArea *area = CTX_wm_area(C);
  switch (area ? eSpace_Type(area->spacetype) : SPACE_EMPTY) {
    case SPACE_ACTION: {
      keylist_from_dopesheet(*C, *keylist);
      break;
    }

    case SPACE_GRAPH:
      keylist_from_graph_editor(*C, *keylist);
      break;

    default:
      keylist_fallback_for_keyframe_jump(*C, scene, *keylist);
      break;
  }

  /* Initialize binary-tree-list for getting keyframes. */
  ED_keylist_prepare_for_direct_access(keylist);

  const float cfra = BKE_scene_frame_get(scene);
  /* find matching keyframe in the right direction */
  const ActKeyColumn *ak;

  if (next) {
    ak = ED_keylist_find_next(keylist, cfra);
    while ((ak != nullptr) && (done == false)) {
      if (cfra < ak->cfra) {
        BKE_scene_frame_set(scene, ak->cfra);
        done = true;
      }
      else {
        ak = ak->next;
      }
    }
  }

  else {
    ak = ED_keylist_find_prev(keylist, cfra);
    while ((ak != nullptr) && (done == false)) {
      if (cfra > ak->cfra) {
        BKE_scene_frame_set(scene, ak->cfra);
        done = true;
      }
      else {
        ak = ak->prev;
      }
    }
  }

  /* free temp stuff */
  ED_keylist_free(keylist);

  /* any success? */
  if (done == false) {
    BKE_report(op->reports, RPT_INFO, "No more keyframes to jump to in this direction");

    return OPERATOR_CANCELLED;
  }

  ED_areas_do_frame_follow(C, true);
  blender::ed::vse::sync_active_scene_and_time_with_scene_strip(*C);

  DEG_id_tag_update(&scene->id, ID_RECALC_FRAME_CHANGE);

  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

  return OPERATOR_FINISHED;
}

static bool keyframe_jump_poll(bContext *C)
{
  return operator_screenactive_norender(C);
}

static void SCREEN_OT_keyframe_jump(wmOperatorType *ot)
{
  ot->name = "Jump to Keyframe";
  ot->description = "Jump to previous/next keyframe";
  ot->idname = "SCREEN_OT_keyframe_jump";

  ot->exec = keyframe_jump_exec;

  ot->poll = keyframe_jump_poll;
  ot->flag = OPTYPE_UNDO_GROUPED;
  ot->undo_group = "Frame Change";

  /* properties */
  RNA_def_boolean(ot->srna, "next", true, "Next Keyframe", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Jump to Marker Operator
 * \{ */

/* function to be called outside UI context, or for redo */
static wmOperatorStatus marker_jump_exec(bContext *C, wmOperator *op)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  if (!scene) {
    return OPERATOR_CANCELLED;
  }
  int closest = scene->r.cfra;
  const bool next = RNA_boolean_get(op->ptr, "next");
  bool found = false;

  /* find matching marker in the right direction */
  LISTBASE_FOREACH (TimeMarker *, marker, &scene->markers) {
    if (next) {
      if ((marker->frame > scene->r.cfra) && (!found || closest > marker->frame)) {
        closest = marker->frame;
        found = true;
      }
    }
    else {
      if ((marker->frame < scene->r.cfra) && (!found || closest < marker->frame)) {
        closest = marker->frame;
        found = true;
      }
    }
  }

  /* any success? */
  if (!found) {
    BKE_report(op->reports, RPT_INFO, "No more markers to jump to in this direction");

    return OPERATOR_CANCELLED;
  }

  scene->r.cfra = closest;

  ED_areas_do_frame_follow(C, true);

  blender::ed::vse::sync_active_scene_and_time_with_scene_strip(*C);

  DEG_id_tag_update(&scene->id, ID_RECALC_FRAME_CHANGE);

  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_marker_jump(wmOperatorType *ot)
{
  ot->name = "Jump to Marker";
  ot->description = "Jump to previous/next marker";
  ot->idname = "SCREEN_OT_marker_jump";

  ot->exec = marker_jump_exec;

  ot->poll = operator_screenactive_norender;
  ot->flag = OPTYPE_UNDO_GROUPED;
  ot->undo_group = "Frame Change";

  /* properties */
  RNA_def_boolean(ot->srna, "next", true, "Next Marker", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Screen Operator
 * \{ */

/* function to be called outside UI context, or for redo */
static wmOperatorStatus screen_set_exec(bContext *C, wmOperator *op)
{
  WorkSpace *workspace = CTX_wm_workspace(C);
  int delta = RNA_int_get(op->ptr, "delta");

  if (ED_workspace_layout_cycle(workspace, delta, C)) {
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

static void SCREEN_OT_screen_set(wmOperatorType *ot)
{
  ot->name = "Set Screen";
  ot->description = "Cycle through available screens";
  ot->idname = "SCREEN_OT_screen_set";

  ot->exec = screen_set_exec;
  ot->poll = ED_operator_screenactive;

  /* rna */
  RNA_def_int(ot->srna, "delta", 1, -1, 1, "Delta", "", -1, 1);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Screen Full-Area Operator
 * \{ */

/* function to be called outside UI context, or for redo */
static wmOperatorStatus screen_maximize_area_exec(bContext *C, wmOperator *op)
{
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = nullptr;
  const bool hide_panels = RNA_boolean_get(op->ptr, "use_hide_panels");

  BLI_assert(!screen->temp);

  /* search current screen for 'full-screen' areas */
  /* prevents restoring info header, when mouse is over it */
  LISTBASE_FOREACH (ScrArea *, area_iter, &screen->areabase) {
    if (area_iter->full) {
      area = area_iter;
      break;
    }
  }

  if (area == nullptr) {
    area = CTX_wm_area(C);
  }

  if (hide_panels) {
    if (!ELEM(screen->state, SCREENNORMAL, SCREENFULL)) {
      return OPERATOR_CANCELLED;
    }
    ED_screen_state_toggle(C, CTX_wm_window(C), area, SCREENFULL);
  }
  else {
    if (!ELEM(screen->state, SCREENNORMAL, SCREENMAXIMIZED)) {
      return OPERATOR_CANCELLED;
    }
    if (BLI_listbase_is_single(&screen->areabase) && screen->state == SCREENNORMAL) {
      /* SCREENMAXIMIZED is not useful when a singleton. #144740. */
      return OPERATOR_CANCELLED;
    }
    ED_screen_state_toggle(C, CTX_wm_window(C), area, SCREENMAXIMIZED);
  }

  return OPERATOR_FINISHED;
}

static bool screen_maximize_area_poll(bContext *C)
{
  const wmWindow *win = CTX_wm_window(C);
  const bScreen *screen = CTX_wm_screen(C);
  const ScrArea *area = CTX_wm_area(C);
  const wmWindowManager *wm = CTX_wm_manager(C);
  return ED_operator_areaactive(C) &&
         /* Don't allow maximizing global areas but allow minimizing from them. */
         ((screen->state != SCREENNORMAL) || !ED_area_is_global(area)) &&
         /* Don't change temporary screens. */
         !WM_window_is_temp_screen(win) &&
         /* Don't maximize when dragging. */
         BLI_listbase_is_empty(&wm->runtime->drags);
}

static void SCREEN_OT_screen_full_area(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Toggle Maximize Area";
  ot->description = "Toggle display selected area as fullscreen/maximized";
  ot->idname = "SCREEN_OT_screen_full_area";

  ot->exec = screen_maximize_area_exec;
  ot->poll = screen_maximize_area_poll;
  ot->flag = 0;

  prop = RNA_def_boolean(
      ot->srna, "use_hide_panels", false, "Hide Panels", "Hide all the panels (Focus Mode)");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Screen Join-Area Operator
 * \{ */

/* operator state vars used:
 * x1, y1     mouse coord in first area, which will disappear
 * x2, y2     mouse coord in 2nd area, which will become joined
 *
 * functions:
 *
 * init()   find edge based on state vars
 * test if the edge divides two areas,
 * store active and nonactive area,
 *
 * apply()  do the actual join
 *
 * exit()   cleanup, send notifier
 *
 * callbacks:
 *
 * exec()   calls init, apply, exit
 *
 * invoke() sets mouse coords in x,y
 * call init()
 * add modal handler
 *
 * modal()  accept modal events while doing it
 * call apply() with active window and nonactive window
 * call exit() and remove handler when LMB confirm
 */

struct sAreaJoinData {
  ScrArea *sa1;               /* Potential source area (kept). */
  ScrArea *sa2;               /* Potential target area (removed or reduced). */
  eScreenDir dir;             /* Direction of potential join. */
  eScreenAxis split_dir;      /* Direction of split within the source area. */
  AreaDockTarget dock_target; /* Position within target we are pointing to. */
  float factor;               /* dock target size can vary. */
  int start_x, start_y;       /* Starting mouse position. */
  int current_x, current_y;   /* Current mouse position. */
  float split_fac;            /* Split factor in split_dir direction. */
  wmWindow *win1;             /* Window of source area. */
  wmWindow *win2;             /* Window of the target area. */
  bScreen *screen;            /* Screen of the source area. */
  double start_time;          /* Start time of animation. */
  double end_time;            /* End time of animation. */
  wmWindow *draw_dock_win;    /* Window getting docking highlight. */
  bool close_win;             /* Close the source window when done. */
  void *draw_callback;        /* call #screen_draw_join_highlight */
  void *draw_dock_callback;   /* call #screen_draw_dock_highlight, overlay on draw_dock_win. */
};

static void area_join_draw_cb(const wmWindow *win, void *userdata)
{
  const wmOperator *op = static_cast<const wmOperator *>(userdata);
  sAreaJoinData *sd = static_cast<sAreaJoinData *>(op->customdata);
  if (!sd || !sd->sa1) {
    return;
  }

  float factor = 1.0f;
  const double now = BLI_time_now_seconds();
  if (now < sd->end_time) {
    factor = pow((now - sd->start_time) / (sd->end_time - sd->start_time), 2);
    sd->screen->do_refresh = true;
  }

  if (sd->sa1 == sd->sa2 && sd->split_fac > 0.0f) {
    screen_draw_split_preview(sd->sa1, sd->split_dir, sd->split_fac);
  }
  else if (sd->sa2 && sd->dir != SCREEN_DIR_NONE) {
    screen_draw_join_highlight(win, sd->sa1, sd->sa2, sd->dir, factor);
  }
}

static void area_join_dock_cb(const wmWindow *win, void *userdata)
{
  const wmOperator *op = static_cast<wmOperator *>(userdata);
  sAreaJoinData *jd = static_cast<sAreaJoinData *>(op->customdata);
  if (!jd || !jd->sa2 || jd->dir != SCREEN_DIR_NONE || jd->sa1 == jd->sa2) {
    return;
  }

  float factor = 1.0f;
  const double now = BLI_time_now_seconds();
  if (now < jd->end_time) {
    factor = pow((now - jd->start_time) / (jd->end_time - jd->start_time), 2);
    jd->screen->do_refresh = true;
  }

  screen_draw_dock_preview(
      win, jd->sa1, jd->sa2, jd->dock_target, jd->factor, jd->current_x, jd->current_y, factor);
}

static void area_join_dock_cb_window(sAreaJoinData *jd, wmOperator *op)
{
  if (jd->sa2 && jd->win2 && jd->win2 != jd->draw_dock_win) {
    /* Change of highlight window. */
    if (jd->draw_dock_callback) {
      WM_draw_cb_exit(jd->draw_dock_win, jd->draw_dock_callback);
      /* Refresh the entire window. */
      ED_screen_areas_iter (
          jd->draw_dock_win, WM_window_get_active_screen(jd->draw_dock_win), area)
      {
        ED_area_tag_redraw(area);
      }
    }
    if (jd->win2) {
      jd->draw_dock_win = jd->win2;
      jd->draw_dock_callback = WM_draw_cb_activate(jd->draw_dock_win, area_join_dock_cb, op);
    }
  }
}

/* validate selection inside screen, set variables OK */
/* return false: init failed */
static bool area_join_init(bContext *C, wmOperator *op, ScrArea *sa1, ScrArea *sa2)
{
  if (sa1 == nullptr && sa2 == nullptr) {
    /* Get areas from cursor location if not specified. */
    PropertyRNA *prop;
    int cursor[2];

    prop = RNA_struct_find_property(op->ptr, "source_xy");
    if (RNA_property_is_set(op->ptr, prop)) {
      RNA_property_int_get_array(op->ptr, prop, cursor);
      sa1 = BKE_screen_find_area_xy(CTX_wm_screen(C), SPACE_TYPE_ANY, cursor);
    }

    prop = RNA_struct_find_property(op->ptr, "target_xy");
    if (RNA_property_is_set(op->ptr, prop)) {
      RNA_property_int_get_array(op->ptr, prop, cursor);
      sa2 = BKE_screen_find_area_xy(CTX_wm_screen(C), SPACE_TYPE_ANY, cursor);
    }
  }
  if (sa1 == nullptr) {
    return false;
  }

  sAreaJoinData *jd = MEM_callocN<sAreaJoinData>("op_area_join");
  jd->sa1 = sa1;
  jd->sa2 = sa2;
  jd->dir = area_getorientation(sa1, sa2);
  jd->win1 = WM_window_find_by_area(CTX_wm_manager(C), sa1);
  jd->win2 = WM_window_find_by_area(CTX_wm_manager(C), sa2);
  jd->screen = CTX_wm_screen(C);
  jd->start_time = BLI_time_now_seconds();
  jd->end_time = jd->start_time + AREA_DOCK_FADEIN;

  op->customdata = jd;
  return true;
}

/* apply the join of the areas (space types) */
static bool area_join_apply(bContext *C, wmOperator *op)
{
  sAreaJoinData *jd = (sAreaJoinData *)op->customdata;
  if (!jd || (jd->dir == SCREEN_DIR_NONE)) {
    return false;
  }

  bScreen *screen = CTX_wm_screen(C);

  /* Rect of the combined areas. */
  const bool vertical = SCREEN_DIR_IS_VERTICAL(jd->dir);
  rcti combined{};
  combined.xmin = vertical ? std::max(jd->sa1->totrct.xmin, jd->sa2->totrct.xmin) :
                             std::min(jd->sa1->totrct.xmin, jd->sa2->totrct.xmin);
  combined.xmax = vertical ? std::min(jd->sa1->totrct.xmax, jd->sa2->totrct.xmax) :
                             std::max(jd->sa1->totrct.xmax, jd->sa2->totrct.xmax);
  combined.ymin = vertical ? std::min(jd->sa1->totrct.ymin, jd->sa2->totrct.ymin) :
                             std::max(jd->sa1->totrct.ymin, jd->sa2->totrct.ymin);
  combined.ymax = vertical ? std::max(jd->sa1->totrct.ymax, jd->sa2->totrct.ymax) :
                             std::min(jd->sa1->totrct.ymax, jd->sa2->totrct.ymax);
  float inner[4] = {1.0f, 1.0f, 1.0f, 0.1f};
  float outline[4] = {1.0f, 1.0f, 1.0f, 0.3f};
  screen_animate_area_highlight(
      CTX_wm_window(C), screen, &combined, inner, outline, AREA_JOIN_FADEOUT);

  if (!screen_area_join(C, op->reports, screen, jd->sa1, jd->sa2)) {
    return false;
  }
  if (CTX_wm_area(C) == jd->sa2) {
    CTX_wm_area_set(C, nullptr);
    CTX_wm_region_set(C, nullptr);
  }

  if (BLI_listbase_is_single(&screen->areabase)) {
    /* Areas reduced to just one, so show nicer title. */
    WM_window_title_refresh(CTX_wm_manager(C), CTX_wm_window(C));
  }

  return true;
}

/* finish operation */
static void area_join_exit(bContext *C, wmOperator *op)
{
  sAreaJoinData *jd = (sAreaJoinData *)op->customdata;

  if (jd) {
    if (jd->draw_callback) {
      WM_draw_cb_exit(jd->win1, jd->draw_callback);
    }
    if (jd->draw_dock_callback) {
      WM_draw_cb_exit(jd->draw_dock_win, jd->draw_dock_callback);
    }

    MEM_freeN(jd);
    op->customdata = nullptr;
  }

  /* this makes sure aligned edges will result in aligned grabbing */
  BKE_screen_remove_double_scredges(CTX_wm_screen(C));
  BKE_screen_remove_unused_scredges(CTX_wm_screen(C));
  BKE_screen_remove_unused_scrverts(CTX_wm_screen(C));

  ED_workspace_status_text(C, nullptr);

  screen_modal_action_end();
}

static wmOperatorStatus area_join_exec(bContext *C, wmOperator *op)
{
  if (!area_join_init(C, op, nullptr, nullptr)) {
    return OPERATOR_CANCELLED;
  }

  sAreaJoinData *jd = (sAreaJoinData *)op->customdata;

  if (jd->sa2 == nullptr || area_getorientation(jd->sa1, jd->sa2) == SCREEN_DIR_NONE) {
    return OPERATOR_CANCELLED;
  }

  ED_area_tag_redraw(jd->sa1);

  area_join_apply(C, op);
  area_join_exit(C, op);
  WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static void area_join_update_data(bContext *C, sAreaJoinData *jd, const wmEvent *event);
static int area_join_cursor(sAreaJoinData *jd, const wmEvent *event);

/* interaction callback */
static wmOperatorStatus area_join_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type == EVT_ACTIONZONE_AREA) {
    sActionzoneData *sad = static_cast<sActionzoneData *>(event->customdata);

    if (sad == nullptr || sad->modifier > 0 || sad->sa1 == nullptr) {
      return OPERATOR_PASS_THROUGH;
    }

    if (!area_join_init(C, op, sad->sa1, sad->sa2)) {
      return OPERATOR_CANCELLED;
    }

    sAreaJoinData *jd = (sAreaJoinData *)op->customdata;
    jd->start_x = sad->x;
    jd->start_y = sad->y;
    jd->draw_callback = WM_draw_cb_activate(CTX_wm_window(C), area_join_draw_cb, op);

    WM_event_add_modal_handler(C, op);
    return OPERATOR_RUNNING_MODAL;
  }

  /* Launched from menu item or keyboard shortcut. */
  if (!area_join_init(C, op, nullptr, nullptr)) {
    ScrArea *sa1 = CTX_wm_area(C);
    if (!sa1 || ED_area_is_global(sa1) || !area_join_init(C, op, sa1, nullptr)) {
      return OPERATOR_CANCELLED;
    }
  }
  sAreaJoinData *jd = (sAreaJoinData *)op->customdata;
  jd->sa2 = jd->sa1;
  jd->start_x = jd->sa1->totrct.xmin;
  jd->start_y = jd->sa1->totrct.ymax;
  jd->current_x = event->xy[0];
  jd->current_y = event->xy[1];
  jd->draw_callback = WM_draw_cb_activate(CTX_wm_window(C), area_join_draw_cb, op);
  WM_cursor_set(jd->win1, area_join_cursor(jd, event));
  area_join_update_data(C, jd, event);
  area_join_dock_cb_window(jd, op);
  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

/* Apply the docking of the area. */
void static area_docking_apply(bContext *C, wmOperator *op)
{
  sAreaJoinData *jd = (sAreaJoinData *)op->customdata;

  int offset1;
  int offset2;
  area_getoffsets(jd->sa1, jd->sa2, area_getorientation(jd->sa1, jd->sa2), &offset1, &offset2);

  /* Check before making changes. */
  bool aligned_neighbors = (offset1 == 0 && offset2 == 0);
  bool same_area = (jd->sa1 == jd->sa2);

  if (!(jd->dock_target == AreaDockTarget::Center)) {
    eScreenAxis dir = ELEM(jd->dock_target, AreaDockTarget::Left, AreaDockTarget::Right) ?
                          SCREEN_AXIS_V :
                          SCREEN_AXIS_H;

    float fac = jd->factor;
    if (ELEM(jd->dock_target, AreaDockTarget::Right, AreaDockTarget::Top)) {
      fac = 1.0f - fac;
    }

    ScrArea *newa = area_split(
        jd->win2, WM_window_get_active_screen(jd->win2), jd->sa2, dir, fac, true);

    if (jd->factor <= 0.5f) {
      jd->sa2 = newa;
    }
    else {
      /* Force full rebuild. #130732 */
      ED_area_tag_redraw(newa);
    }
  }

  if (same_area) {
    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);
    return;
  }

  float inner[4] = {1.0f, 1.0f, 1.0f, 0.15f};
  float outline[4] = {1.0f, 1.0f, 1.0f, 0.4f};
  jd->sa2->flag |= AREA_FLAG_REGION_SIZE_UPDATE;
  ED_area_update_region_sizes(CTX_wm_manager(C), jd->win2, jd->sa2);
  screen_animate_area_highlight(
      jd->win2, CTX_wm_screen(C), &jd->sa2->totrct, inner, outline, AREA_DOCK_FADEOUT);

  if (!aligned_neighbors || !screen_area_join(C, op->reports, CTX_wm_screen(C), jd->sa1, jd->sa2))
  {
    ED_area_swapspace(C, jd->sa2, jd->sa1);
    if (BLI_listbase_is_single(&WM_window_get_active_screen(jd->win1)->areabase) &&
        BLI_listbase_is_empty(&jd->win1->global_areas.areabase))
    {
      jd->close_win = true;
      /* Clear the active region in each screen, otherwise they are pointing
       * at incorrect regions and will cause errors in uiTemplateInputStatus. */
      WM_window_get_active_screen(jd->win1)->active_region = nullptr;
      WM_window_get_active_screen(jd->win2)->active_region = nullptr;
    }
    else {
      float inner[4] = {0.0f, 0.0f, 0.0f, 0.7f};
      screen_animate_area_highlight(
          jd->win1, CTX_wm_screen(C), &jd->sa1->totrct, inner, nullptr, AREA_CLOSE_FADEOUT);
      screen_area_close(C, op->reports, CTX_wm_screen(C), jd->sa1);
    }
  }

  if (jd && jd->sa2 == CTX_wm_area(C)) {
    CTX_wm_area_set(C, nullptr);
    CTX_wm_region_set(C, nullptr);
  }
}

static int area_join_cursor(sAreaJoinData *jd, const wmEvent *event)
{
  if (!jd->sa2 && jd->dock_target == AreaDockTarget::None) {
    /* Mouse outside window, so can open new window. */
    if (event->xy[0] < 0 || event->xy[0] > jd->win1->sizex || event->xy[1] < 1 ||
        event->xy[1] > jd->win1->sizey)
    {
      return WM_CURSOR_PICK_AREA;
    }
    return WM_CURSOR_STOP;
  }

  if (jd->win2 && jd->win2->workspace_hook) {
    bScreen *screen = BKE_workspace_active_screen_get(jd->win2->workspace_hook);
    if (screen && screen->temp) {
      return WM_CURSOR_STOP;
    }
  }

  if (jd->sa1 && jd->sa1 == jd->sa2) {
    if (jd->split_fac >= 0.0001f) {
      /* Mouse inside source area, so allow splitting. */
      return (jd->split_dir == SCREEN_AXIS_V) ? WM_CURSOR_V_SPLIT : WM_CURSOR_H_SPLIT;
    }
    return WM_CURSOR_EDIT;
  }

  if (jd->dir != SCREEN_DIR_NONE) {
    /* Joining */
    switch (jd->dir) {
      case SCREEN_DIR_N:
        return WM_CURSOR_N_ARROW;
        break;
      case SCREEN_DIR_S:
        return WM_CURSOR_S_ARROW;
        break;
      case SCREEN_DIR_W:
        return WM_CURSOR_W_ARROW;
        break;
      default:
        return WM_CURSOR_E_ARROW;
    }
  }

  if (jd->dir != SCREEN_DIR_NONE || jd->dock_target != AreaDockTarget::None) {
#if defined(__APPLE__)
    return WM_CURSOR_HAND_CLOSED;
#else
    return WM_CURSOR_MOVE;
#endif
  }

  return WM_CURSOR_PICK_AREA;
}

static float area_docking_snap(const float pos, const wmEvent *event)
{
  const bool alt = event->modifier & KM_ALT;
  const bool ctrl = event->modifier & KM_CTRL;
  const float accel = (alt || ctrl) ? 2.5f : 2.0;

  float factor = pos * accel;

  if (!alt) {
    if (factor >= 0.4375f && factor < 0.5f) {
      factor = 0.499999f;
    }
    else if (factor >= 0.5f && factor < 0.5625f) {
      factor = 0.500001f;
    }
  }

  if (ctrl) {
    if (factor < 0.1875f) {
      factor = 0.125f;
    }
    else if (factor >= 0.1875f && factor < 0.3125f) {
      factor = 0.25f;
    }
    else if (factor >= 0.3125f && factor < 0.4375f) {
      factor = 0.375f;
    }
    else if (factor >= 0.5625f && factor < 0.6875f) {
      factor = 0.625f;
    }
    else if (factor >= 0.6875f && factor < 0.8125f) {
      factor = 0.75f;
    }
    else if (factor > 0.8125f) {
      factor = 0.875f;
    }
  }

  return factor;
}

static AreaDockTarget area_docking_target(sAreaJoinData *jd, const wmEvent *event)
{
  if (!jd->sa2 || !jd->win2) {
    return AreaDockTarget::None;
  }

  if (jd->sa1 == jd->sa2) {
    return AreaDockTarget::None;
  }

  if (jd->win2 && jd->win2->workspace_hook) {
    bScreen *screen = BKE_workspace_active_screen_get(jd->win2->workspace_hook);
    if (screen && screen->temp) {
      return AreaDockTarget::None;
    }
  }

  /* Convert to local coordinates in sa2. */
  int win1_posx = jd->win1->posx;
  int win1_posy = jd->win1->posy;
  int win2_posx = jd->win2->posx;
  int win2_posy = jd->win2->posy;
  WM_window_native_pixel_coords(jd->win1, &win1_posx, &win1_posy);
  WM_window_native_pixel_coords(jd->win2, &win2_posx, &win2_posy);

  const int x = event->xy[0] + win1_posx - win2_posx - jd->sa2->totrct.xmin;
  const int y = event->xy[1] + win1_posy - win2_posy - jd->sa2->totrct.ymin;

  jd->current_x = x + jd->sa2->totrct.xmin;
  jd->current_y = y + jd->sa2->totrct.ymin;

  const float fac_x = float(x) / float(jd->sa2->winx);
  const float fac_y = float(y) / float(jd->sa2->winy);
  const int min_x = 2 * AREAMINX * UI_SCALE_FAC;
  const int min_y = 2 * HEADERY * UI_SCALE_FAC;

  if (ELEM(jd->dir, SCREEN_DIR_N, SCREEN_DIR_S)) {
    /* Up or Down to immediate neighbor. */
    if (event->xy[0] <= jd->sa1->totrct.xmax && event->xy[0] >= jd->sa1->totrct.xmin) {
      const int join_y = std::min(jd->sa2->winy * 0.25f, 5 * HEADERY * UI_SCALE_FAC);
      if (jd->sa2->winy < min_y || (jd->dir == SCREEN_DIR_N && y < join_y) ||
          (jd->dir == SCREEN_DIR_S && (jd->sa2->winy - y) < join_y))
      {
        return AreaDockTarget::None;
      }
    }
  }

  if (ELEM(jd->dir, SCREEN_DIR_W, SCREEN_DIR_E)) {
    /* Left or Right to immediate neighbor. */
    if (event->xy[1] <= jd->sa1->totrct.ymax && event->xy[1] >= jd->sa1->totrct.ymin) {
      const int join_x = std::min(jd->sa2->winx * 0.25f, 5 * AREAMINX * UI_SCALE_FAC);
      if (jd->sa2->winx < min_x || (jd->dir == SCREEN_DIR_W && (jd->sa2->winx - x) < join_x) ||
          (jd->dir == SCREEN_DIR_E && x < join_x))
      {
        return AreaDockTarget::None;
      }
    }
  }

  /* If we've made it here, then there can be no joining possible. */
  jd->dir = SCREEN_DIR_NONE;
  jd->factor = 0.5f;

  const float min_fac_x = 1.5f * AREAMINX * UI_SCALE_FAC / float(jd->sa2->winx);
  const float min_fac_y = 1.5f * HEADERY * UI_SCALE_FAC / float(jd->sa2->winy);

  /* if the area is narrow then there are only two docking targets. */
  if (jd->sa2->winx < (min_x * 3)) {
    if (fac_y > 0.4f && fac_y < 0.6f) {
      return AreaDockTarget::Center;
    }
    if (float(y) > float(jd->sa2->winy) / 2.0f) {
      jd->factor = area_docking_snap(std::max(1.0f - fac_y, min_fac_y), event);
      return AreaDockTarget::Top;
    }
    jd->factor = area_docking_snap(std::max(fac_y, min_fac_y), event);
    return AreaDockTarget::Bottom;
  }
  if (jd->sa2->winy < (min_y * 3)) {
    if (fac_x > 0.4f && fac_x < 0.6f) {
      return AreaDockTarget::Center;
    }
    if (float(x) > float(jd->sa2->winx) / 2.0f) {
      jd->factor = area_docking_snap(std::max(1.0f - fac_x, min_fac_x), event);
      return AreaDockTarget::Right;
    }
    jd->factor = area_docking_snap(std::max(fac_x, min_fac_x), event);
    return AreaDockTarget::Left;
  }

  /* Are we in the center? But not in same area! */
  if (fac_x > 0.4f && fac_x < 0.6f && fac_y > 0.4f && fac_y < 0.6f) {
    return AreaDockTarget::Center;
  }

  /* Area is large enough for four docking targets. */
  const float area_ratio = float(jd->sa2->winx) / float(jd->sa2->winy);
  /* Split the area diagonally from top-right to bottom-left. */
  const bool upper_left = float(x) / float(y + 1) < area_ratio;
  /* Split the area diagonally from top-left to bottom-right. */
  const bool lower_left = float(x) / float(jd->sa2->winy - y + 1) < area_ratio;

  if (upper_left && !lower_left) {
    jd->factor = area_docking_snap(std::max(1.0f - fac_y, min_fac_y), event);
    return AreaDockTarget::Top;
  }
  if (!upper_left && lower_left) {
    jd->factor = area_docking_snap(std::max(fac_y, min_fac_y), event);
    return AreaDockTarget::Bottom;
  }
  if (upper_left && lower_left) {
    jd->factor = area_docking_snap(std::max(fac_x, min_fac_x), event);
    return AreaDockTarget::Left;
  }
  if (!upper_left && !lower_left) {
    jd->factor = area_docking_snap(std::max(1.0f - fac_x, min_fac_x), event);
    return AreaDockTarget::Right;
  }
  return AreaDockTarget::None;
}

static float area_split_factor(bContext *C, sAreaJoinData *jd, const wmEvent *event)
{
  float fac = (jd->split_dir == SCREEN_AXIS_V) ?
                  float(event->xy[0] - jd->sa1->totrct.xmin) / float(jd->sa1->winx + 1) :
                  float(event->xy[1] - jd->sa1->totrct.ymin) / float(jd->sa1->winy + 1);

  if (event->modifier & KM_CTRL) {
    /* Snapping on. */

    /* Find nearest neighboring vertex. */
    const int axis = (jd->split_dir == SCREEN_AXIS_V) ? 0 : 1;
    int dist = INT_MAX;
    int loc = 0;
    LISTBASE_FOREACH (const ScrVert *, v1, &CTX_wm_screen(C)->vertbase) {
      const int v_loc = (&v1->vec.x)[axis];
      const int v_dist = abs(v_loc - event->xy[axis]);
      if (v_dist < dist) {
        loc = v_loc;
        dist = v_dist;
      }
    }
    float near_fac = (axis) ? float(loc - jd->sa1->totrct.ymin) / float(jd->sa1->winy + 1) :
                              float(loc - jd->sa1->totrct.xmin) / float(jd->sa1->winx + 1);

    /* Rounded to nearest 12th. */
    float frac_fac = round(fac * 12.0f) / 12.0f;

    /* Use nearest neighbor or fractional, whichever is closest. */
    fac = (fabs(near_fac - fac) < fabs(frac_fac - fac)) ? near_fac : frac_fac;
  }
  else {
    /* Slight snap to center when no modifiers are held. */
    if (fac >= 0.48f && fac < 0.5f) {
      fac = 0.499999f;
    }
    else if (fac >= 0.5f && fac < 0.52f) {
      fac = 0.500001f;
    }
  }

  /* Don't allow a new area to be created that is very small. */
  const float min_size = float(2.0f * ED_area_headersize());
  const float min_fac = min_size / ((jd->split_dir == SCREEN_AXIS_V) ? float(jd->sa1->winx + 1) :
                                                                       float(jd->sa1->winy + 1));
  if (min_fac < 0.5f) {
    return std::clamp(fac, min_fac, 1.0f - min_fac);
  }
  return 0.5f;
}

static void area_join_update_data(bContext *C, sAreaJoinData *jd, const wmEvent *event)
{
  ScrArea *area = nullptr;

  /* TODO: The following is needed until we have linux-specific implementations of
   * getWindowUnderCursor. See #130242. Use active window if there are overlapping. */

#if (OS_WINDOWS || OS_MAC)
  area = ED_area_find_under_cursor(C, SPACE_TYPE_ANY, event->xy);
#else
  int win_count = 0;
  LISTBASE_FOREACH (wmWindow *, win, &CTX_wm_manager(C)->windows) {
    int cursor[2];
    if (wm_cursor_position_get(win, &cursor[0], &cursor[1])) {
      rcti rect;
      WM_window_rect_calc(win, &rect);
      if (BLI_rcti_isect_pt_v(&rect, cursor)) {
        win_count++;
      }
    }
  }

  if (win_count > 1) {
    area = BKE_screen_find_area_xy(CTX_wm_screen(C), SPACE_TYPE_ANY, event->xy);
  }
  else {
    area = ED_area_find_under_cursor(C, SPACE_TYPE_ANY, event->xy);
  }
#endif

  jd->win2 = WM_window_find_by_area(CTX_wm_manager(C), jd->sa2);
  jd->dir = SCREEN_DIR_NONE;
  jd->dock_target = AreaDockTarget::None;
  jd->dir = area_getorientation(jd->sa1, area);
  jd->dock_target = area_docking_target(jd, event);

  if (jd->sa2 != area) {
    jd->start_time = BLI_time_now_seconds();
    jd->end_time = jd->start_time + AREA_DOCK_FADEIN;
  }

  if (jd->sa1 == area) {
    const int drag_threshold = 20 * UI_SCALE_FAC;
    jd->sa2 = area;
    if (!(abs(jd->start_x - event->xy[0]) > drag_threshold ||
          abs(jd->start_y - event->xy[1]) > drag_threshold))
    {
      /* We haven't moved enough to start a split. */
      jd->dir = SCREEN_DIR_NONE;
      jd->split_fac = 0.0f;
      jd->dock_target = AreaDockTarget::None;
      return;
    }

    eScreenAxis dir = (abs(event->xy[0] - jd->start_x) > abs(event->xy[1] - jd->start_y)) ?
                          SCREEN_AXIS_V :
                          SCREEN_AXIS_H;
    jd->split_dir = dir;
    jd->split_fac = area_split_factor(C, jd, event);
    return;
  }

  jd->sa2 = area;
  jd->win2 = WM_window_find_by_area(CTX_wm_manager(C), jd->sa2);
  jd->dir = area_getorientation(jd->sa1, jd->sa2);
  jd->dock_target = area_docking_target(jd, event);
}

static void area_join_cancel(bContext *C, wmOperator *op)
{
  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  WM_cursor_set(CTX_wm_window(C), WM_CURSOR_DEFAULT);
  area_join_exit(C, op);
}

static void screen_area_touch_menu_create(bContext *C, ScrArea *area)
{
  uiPopupMenu *pup = UI_popup_menu_begin(C, "Area Options", ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);
  layout->operator_context_set(blender::wm::OpCallContext::InvokeDefault);

  PointerRNA ptr = layout->op("SCREEN_OT_area_split",
                              IFACE_("Horizontal Split"),
                              ICON_SPLIT_HORIZONTAL,
                              blender::wm::OpCallContext::ExecDefault,
                              UI_ITEM_NONE);
  RNA_enum_set(&ptr, "direction", SCREEN_AXIS_H);
  RNA_float_set(&ptr, "factor", 0.49999f);
  blender::int2 pos = {area->totrct.xmin + area->winx / 2, area->totrct.ymin + area->winy / 2};
  RNA_int_set_array(&ptr, "cursor", pos);

  ptr = layout->op("SCREEN_OT_area_split",
                   IFACE_("Vertical Split"),
                   ICON_SPLIT_VERTICAL,
                   blender::wm::OpCallContext::ExecDefault,
                   UI_ITEM_NONE);
  RNA_enum_set(&ptr, "direction", SCREEN_AXIS_V);
  RNA_float_set(&ptr, "factor", 0.49999f);
  RNA_int_set_array(&ptr, "cursor", pos);

  layout->separator();

  layout->op("SCREEN_OT_area_join", IFACE_("Move/Join/Dock Area"), ICON_AREA_DOCK);

  layout->separator();

  layout->op("SCREEN_OT_screen_full_area",
             area->full ? IFACE_("Restore Areas") : IFACE_("Maximize Area"),
             ICON_NONE);

  ptr = layout->op("SCREEN_OT_screen_full_area", IFACE_("Focus Mode"), ICON_NONE);
  RNA_boolean_set(&ptr, "use_hide_panels", true);

  layout->op("SCREEN_OT_area_dupli", std::nullopt, ICON_NONE);
  layout->separator();
  layout->op("SCREEN_OT_area_close", IFACE_("Close Area"), ICON_X);

  UI_popup_menu_end(C, pup);
}

static bool is_header_azone_location(ScrArea *area, const wmEvent *event)
{
  if (event->xy[0] > (area->totrct.xmin + UI_HEADER_OFFSET)) {
    return false;
  }

  ARegion *header = BKE_area_find_region_type(area, RGN_TYPE_HEADER);
  if (!header || header->flag & RGN_FLAG_HIDDEN) {
    return false;
  }

  const int height = ED_area_headersize();
  if (header->alignment == RGN_ALIGN_TOP && event->xy[1] > (area->totrct.ymax - height)) {
    return true;
  }
  if (header->alignment == RGN_ALIGN_BOTTOM && event->xy[1] < (area->totrct.ymin + height)) {
    return true;
  }

  return false;
}

/* modal callback while selecting area (space) that will be removed */
static wmOperatorStatus area_join_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type == WINDEACTIVATE) {
    /* This operator can close windows, which can cause it to be re-run. */
    area_join_exit(C, op);
    return OPERATOR_FINISHED;
  }

  if (op->customdata == nullptr) {
    if (!area_join_init(C, op, nullptr, nullptr)) {
      return OPERATOR_CANCELLED;
    }
  }
  sAreaJoinData *jd = (sAreaJoinData *)op->customdata;
  if (jd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* execute the events */
  switch (event->type) {

    case MOUSEMOVE: {
      area_join_update_data(C, jd, event);
      area_join_dock_cb_window(jd, op);
      WM_cursor_set(jd->win1, area_join_cursor(jd, event));
      WM_event_add_notifier(C, NC_WINDOW, nullptr);

      WorkspaceStatus status(C);
      if (jd->sa1 && jd->sa1 == jd->sa2) {
        if (jd->split_fac == 0.0f) {
          status.item(IFACE_("Split/Dock"), ICON_MOUSE_LMB_DRAG);
          status.item(IFACE_("Cancel"), ICON_EVENT_ESC);
        }
        else {
          status.item(IFACE_("Select Split"), ICON_MOUSE_LMB_DRAG);
          status.item(IFACE_("Cancel"), ICON_EVENT_ESC);
          status.item_bool(IFACE_("Snap"), event->modifier & KM_CTRL, ICON_EVENT_CTRL);
        }
      }
      else {
        if (jd->dock_target == AreaDockTarget::None) {
          status.item(IFACE_("Select Area"), ICON_MOUSE_LMB_DRAG);
          status.item(IFACE_("Cancel"), ICON_EVENT_ESC);
        }
        else {
          status.item(IFACE_("Select Location"), ICON_MOUSE_LMB_DRAG);
          status.item(IFACE_("Cancel"), ICON_EVENT_ESC);
          status.item_bool(CTX_IFACE_(BLT_I18NCONTEXT_ID_SCREEN, "Precision"),
                           event->modifier & KM_ALT,
                           ICON_EVENT_ALT);
          status.item_bool(IFACE_("Snap"), event->modifier & KM_CTRL, ICON_EVENT_CTRL);
        }
      }
      break;
    }
    case LEFTMOUSE:
      if (event->val == KM_RELEASE) {
        area_join_update_data(C, jd, event);
        area_join_dock_cb_window(jd, op);
        ED_area_tag_redraw(jd->sa1);
        ED_area_tag_redraw(jd->sa2);
        if (jd->dir == SCREEN_DIR_NONE && jd->dock_target == AreaDockTarget::None &&
            jd->split_fac == 0.0f && is_header_azone_location(jd->sa1, event))
        {
          screen_area_touch_menu_create(C, jd->sa1);
          area_join_cancel(C, op);
          return OPERATOR_CANCELLED;
        }
        if (jd->sa1 && !jd->sa2) {
          /* Break out into new window if we are really outside the source window bounds. */
          if (event->xy[0] < 0 || event->xy[0] > jd->win1->sizex || event->xy[1] < 1 ||
              event->xy[1] > jd->win1->sizey)
          {
            /* We have to clear handlers or we get an error in wm_gizmomap_modal_get. */
            WM_event_modal_handler_region_replace(jd->win1, CTX_wm_region(C), nullptr);
            area_dupli_open(C, jd->sa1, blender::int2(event->xy[0], event->xy[1] - jd->sa1->winy));
            if (!screen_area_close(C, op->reports, WM_window_get_active_screen(jd->win1), jd->sa1))
            {
              if (BLI_listbase_is_single(&WM_window_get_active_screen(jd->win1)->areabase) &&
                  BLI_listbase_is_empty(&jd->win1->global_areas.areabase))
              {
                /* We've pulled a single editor out of the window into empty space.
                 * Close the source window so we don't end up with a duplicate. */
                jd->close_win = true;
              }
            }
          }
        }
        else if (jd->sa1 && jd->sa1 == jd->sa2) {
          /* Same area so split. */
          if (area_split_allowed(jd->sa1, jd->split_dir) && jd->split_fac > 0.0001) {
            float inner[4] = {1.0f, 1.0f, 1.0f, 0.1f};
            float outline[4] = {1.0f, 1.0f, 1.0f, 0.3f};
            screen_animate_area_highlight(
                jd->win1, CTX_wm_screen(C), &jd->sa1->totrct, inner, outline, AREA_SPLIT_FADEOUT);
            jd->sa2 = area_split(jd->win2,
                                 WM_window_get_active_screen(jd->win1),
                                 jd->sa1,
                                 jd->split_dir,
                                 jd->split_fac,
                                 true);

            const bool large_v = jd->split_dir == SCREEN_AXIS_V &&
                                 ((jd->start_x < event->xy[0] && jd->split_fac > 0.5f) ||
                                  (jd->start_x > event->xy[0] && jd->split_fac < 0.5f));

            const bool large_h = jd->split_dir == SCREEN_AXIS_H &&
                                 ((jd->start_y < event->xy[1] && jd->split_fac > 0.5f) ||
                                  (jd->start_y > event->xy[1] && jd->split_fac < 0.5f));

            if (large_v || large_h) {
              /* Swap areas to follow old behavior of new area added based on starting location.
               * When from above the new area is above, when from below the new area is below, etc.
               * Note that this preserves runtime data, unlike #ED_area_swapspace. */
              std::swap(jd->sa1->v1, jd->sa2->v1);
              std::swap(jd->sa1->v2, jd->sa2->v2);
              std::swap(jd->sa1->v3, jd->sa2->v3);
              std::swap(jd->sa1->v4, jd->sa2->v4);
              std::swap(jd->sa1->totrct, jd->sa2->totrct);
              std::swap(jd->sa1->winx, jd->sa2->winx);
              std::swap(jd->sa1->winy, jd->sa2->winy);
            }

            ED_area_tag_redraw(jd->sa1);
            ED_area_tag_redraw(jd->sa2);
          }
        }
        else if (jd->sa1 && jd->sa2 && jd->dock_target != AreaDockTarget::None) {
          /* Dock this to the new location. */
          area_docking_apply(C, op);
        }
        else if (jd->sa1 && jd->sa2 && jd->dir != SCREEN_DIR_NONE) {
          /* Join to neighbor. */
          area_join_apply(C, op);
        }
        else {
          area_join_cancel(C, op);
          return OPERATOR_CANCELLED;
        }

        /* Areas changed, update window titles. */
        if (jd->win2 && jd->win2 != jd->win1) {
          WM_window_title_refresh(CTX_wm_manager(C), jd->win2);
        }
        if (jd->win1 && !jd->close_win) {
          WM_window_title_refresh(CTX_wm_manager(C), jd->win1);
        }

        const bool do_close_win = jd->close_win;
        wmWindow *close_win = jd->win1;
        area_join_exit(C, op);
        if (do_close_win) {
          wm_window_close(C, CTX_wm_manager(C), close_win);
        }

        WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);
        return OPERATOR_FINISHED;
      }
      break;

    case RIGHTMOUSE:
    case EVT_ESCKEY:
      area_join_cancel(C, op);
      return OPERATOR_CANCELLED;
    default: {
      break;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

/* Operator for joining two areas (space types) */
static void SCREEN_OT_area_join(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Join Area";
  ot->description = "Join selected areas into new window";
  ot->idname = "SCREEN_OT_area_join";

  /* API callbacks. */
  ot->exec = area_join_exec;
  ot->invoke = area_join_invoke;
  ot->modal = area_join_modal;
  ot->poll = screen_active_editable;
  ot->cancel = area_join_cancel;

  /* flags */
  ot->flag = OPTYPE_BLOCKING;

  /* rna */
  RNA_def_int_vector(ot->srna,
                     "source_xy",
                     2,
                     nullptr,
                     INT_MIN,
                     INT_MAX,
                     "Source location",
                     "",
                     INT_MIN,
                     INT_MAX);
  RNA_def_int_vector(ot->srna,
                     "target_xy",
                     2,
                     nullptr,
                     INT_MIN,
                     INT_MAX,
                     "Target location",
                     "",
                     INT_MIN,
                     INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Screen Area Options Operator
 * \{ */

static wmOperatorStatus screen_area_options_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  ScrArea *sa1, *sa2;
  if (screen_area_edge_from_cursor(C, event->xy, &sa1, &sa2) == nullptr) {
    return OPERATOR_CANCELLED;
  }

  uiPopupMenu *pup = UI_popup_menu_begin(
      C, WM_operatortype_name(op->type, op->ptr).c_str(), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  /* Vertical Split */
  PointerRNA ptr;
  ptr = layout->op("SCREEN_OT_area_split",
                   IFACE_("Vertical Split"),
                   ICON_SPLIT_VERTICAL,
                   blender::wm::OpCallContext::InvokeDefault,
                   UI_ITEM_NONE);
  /* store initial mouse cursor position. */
  RNA_int_set_array(&ptr, "cursor", event->xy);
  RNA_enum_set(&ptr, "direction", SCREEN_AXIS_V);

  /* Horizontal Split */
  ptr = layout->op("SCREEN_OT_area_split",
                   IFACE_("Horizontal Split"),
                   ICON_SPLIT_HORIZONTAL,
                   blender::wm::OpCallContext::InvokeDefault,
                   UI_ITEM_NONE);
  /* store initial mouse cursor position. */
  RNA_int_set_array(&ptr, "cursor", event->xy);
  RNA_enum_set(&ptr, "direction", SCREEN_AXIS_H);

  if (sa1 && sa2) {
    layout->separator();
  }

  /* Join needs two very similar areas. */
  if (sa1 && sa2) {
    eScreenDir dir = area_getorientation(sa1, sa2);
    if (dir != SCREEN_DIR_NONE) {
      ptr = layout->op("SCREEN_OT_area_join",
                       ELEM(dir, SCREEN_DIR_N, SCREEN_DIR_S) ? IFACE_("Join Up") :
                                                               IFACE_("Join Right"),
                       ELEM(dir, SCREEN_DIR_N, SCREEN_DIR_S) ? ICON_AREA_JOIN_UP : ICON_AREA_JOIN,
                       blender::wm::OpCallContext::ExecDefault,
                       UI_ITEM_NONE);
      RNA_int_set_array(&ptr, "source_xy", blender::int2{sa2->totrct.xmin, sa2->totrct.ymin});
      RNA_int_set_array(&ptr, "target_xy", blender::int2{sa1->totrct.xmin, sa1->totrct.ymin});

      ptr = layout->op(
          "SCREEN_OT_area_join",
          ELEM(dir, SCREEN_DIR_N, SCREEN_DIR_S) ? IFACE_("Join Down") : IFACE_("Join Left"),
          ELEM(dir, SCREEN_DIR_N, SCREEN_DIR_S) ? ICON_AREA_JOIN_DOWN : ICON_AREA_JOIN_LEFT,
          blender::wm::OpCallContext::ExecDefault,
          UI_ITEM_NONE);
      RNA_int_set_array(&ptr, "source_xy", blender::int2{sa1->totrct.xmin, sa1->totrct.ymin});
      RNA_int_set_array(&ptr, "target_xy", blender::int2{sa2->totrct.xmin, sa2->totrct.ymin});

      layout->separator();
    }
  }

  /* Swap just needs two areas. */
  if (sa1 && sa2) {
    ptr = layout->op("SCREEN_OT_area_swap",
                     IFACE_("Swap Areas"),
                     ICON_AREA_SWAP,
                     blender::wm::OpCallContext::ExecDefault,
                     UI_ITEM_NONE);
    RNA_int_set_array(&ptr, "cursor", event->xy);
  }

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static void SCREEN_OT_area_options(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Area Options";
  ot->description = "Operations for splitting and merging";
  ot->idname = "SCREEN_OT_area_options";

  /* API callbacks. */
  ot->invoke = screen_area_options_invoke;

  ot->poll = ED_operator_screen_mainwinactive;

  /* flags */
  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space Data Cleanup Operator
 * \{ */

static wmOperatorStatus spacedata_cleanup_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  int tot = 0;

  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area->spacedata.first != area->spacedata.last) {
        SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);

        BLI_remlink(&area->spacedata, sl);
        tot += BLI_listbase_count(&area->spacedata);
        BKE_spacedata_freelist(&area->spacedata);
        BLI_addtail(&area->spacedata, sl);
      }
    }
  }
  BKE_reportf(op->reports, RPT_INFO, "Removed amount of editors: %d", tot);

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_spacedata_cleanup(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clean Up Space Data";
  ot->description = "Remove unused settings for invisible editors";
  ot->idname = "SCREEN_OT_spacedata_cleanup";

  /* API callbacks. */
  ot->exec = spacedata_cleanup_exec;
  ot->poll = WM_operator_winactive;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Repeat Last Operator
 * \{ */

static bool repeat_history_poll(bContext *C)
{
  if (!ED_operator_screenactive(C)) {
    return false;
  }
  wmWindowManager *wm = CTX_wm_manager(C);
  return !BLI_listbase_is_empty(&wm->runtime->operators);
}

static wmOperatorStatus repeat_last_exec(bContext *C, wmOperator * /*op*/)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmOperator *lastop = static_cast<wmOperator *>(wm->runtime->operators.last);

  /* Seek last registered operator */
  while (lastop) {
    if (lastop->type->flag & OPTYPE_REGISTER) {
      break;
    }
    lastop = lastop->prev;
  }

  if (lastop) {
    WM_operator_free_all_after(wm, lastop);
    WM_operator_repeat_last(C, lastop);
  }

  return OPERATOR_CANCELLED;
}

static void SCREEN_OT_repeat_last(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Repeat Last";
  ot->description = "Repeat last action";
  ot->idname = "SCREEN_OT_repeat_last";

  /* API callbacks. */
  ot->exec = repeat_last_exec;

  ot->poll = repeat_history_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Repeat History Operator
 * \{ */

static wmOperatorStatus repeat_history_invoke(bContext *C,
                                              wmOperator *op,
                                              const wmEvent * /*event*/)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  int items = BLI_listbase_count(&wm->runtime->operators);
  if (items == 0) {
    return OPERATOR_CANCELLED;
  }

  uiPopupMenu *pup = UI_popup_menu_begin(
      C, WM_operatortype_name(op->type, op->ptr).c_str(), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  wmOperator *lastop;
  int i;
  for (i = items - 1, lastop = static_cast<wmOperator *>(wm->runtime->operators.last); lastop;
       lastop = lastop->prev, i--)
  {
    if ((lastop->type->flag & OPTYPE_REGISTER) && WM_operator_repeat_check(C, lastop)) {
      PointerRNA op_ptr = layout->op(
          op->type, WM_operatortype_name(lastop->type, lastop->ptr), ICON_NONE);
      RNA_int_set(&op_ptr, "index", i);
    }
  }

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static wmOperatorStatus repeat_history_exec(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  op = static_cast<wmOperator *>(
      BLI_findlink(&wm->runtime->operators, RNA_int_get(op->ptr, "index")));
  if (op) {
    /* let's put it as last operator in list */
    BLI_remlink(&wm->runtime->operators, op);
    BLI_addtail(&wm->runtime->operators, op);

    WM_operator_repeat(C, op);
  }

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_repeat_history(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Repeat History";
  ot->description = "Display menu for previous actions performed";
  ot->idname = "SCREEN_OT_repeat_history";

  /* API callbacks. */
  ot->invoke = repeat_history_invoke;
  ot->exec = repeat_history_exec;
  ot->poll = repeat_history_poll;

  RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, 1000);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Redo Operator
 * \{ */

static wmOperatorStatus redo_last_invoke(bContext *C,
                                         wmOperator * /*op*/,
                                         const wmEvent * /*event*/)
{
  wmOperator *lastop = WM_operator_last_redo(C);

  if (lastop) {
    WM_operator_redo_popup(C, lastop);
  }

  return OPERATOR_CANCELLED;
}

static void SCREEN_OT_redo_last(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Redo Last";
  ot->description = "Display parameters for last action performed";
  ot->idname = "SCREEN_OT_redo_last";

  /* API callbacks. */
  ot->invoke = redo_last_invoke;
  ot->poll = repeat_history_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region Quad-View Operator
 * \{ */

static void view3d_localview_update_rv3d(RegionView3D *rv3d)
{
  if (rv3d->localvd) {
    rv3d->localvd->view = rv3d->view;
    rv3d->localvd->view_axis_roll = rv3d->view_axis_roll;
    rv3d->localvd->persp = rv3d->persp;
    copy_qt_qt(rv3d->localvd->viewquat, rv3d->viewquat);
  }
}

static void region_quadview_init_rv3d(
    ScrArea *area, ARegion *region, const char viewlock, const char view, const char persp)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  rv3d->rflag &= ~RV3D_WAS_CAMOB;

  if (persp == RV3D_CAMOB) {
    ED_view3d_lastview_store(rv3d);
  }

  rv3d->viewlock = viewlock;
  rv3d->runtime_viewlock = 0;
  rv3d->view = view;
  rv3d->view_axis_roll = RV3D_VIEW_AXIS_ROLL_0;
  rv3d->persp = persp;

  ED_view3d_lock(rv3d);
  view3d_localview_update_rv3d(rv3d);
  if ((viewlock & RV3D_BOXCLIP) && (persp == RV3D_ORTHO)) {
    ED_view3d_quadview_update(area, region, true);
  }
}

/* insert a region in the area region list */
static wmOperatorStatus region_quadview_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);

  /* some rules... */
  if (region->regiontype != RGN_TYPE_WINDOW) {
    BKE_report(op->reports, RPT_ERROR, "Only window region can be 4-split");
  }
  else if (region->alignment == RGN_ALIGN_QSPLIT) {
    /* Exit quad-view */
    bScreen *screen = CTX_wm_screen(C);
    ScrArea *area = CTX_wm_area(C);

    /* keep current region */
    region->alignment = 0;

    if (area->spacetype == SPACE_VIEW3D) {
      RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

      /* if this is a locked view, use settings from 'User' view */
      if (rv3d->viewlock) {
        View3D *v3d_user;
        ARegion *region_user;

        if (ED_view3d_context_user_region(C, &v3d_user, &region_user)) {
          if (region != region_user) {
            std::swap(region->regiondata, region_user->regiondata);
            rv3d = static_cast<RegionView3D *>(region->regiondata);
          }
        }
      }

      rv3d->viewlock_quad = RV3D_VIEWLOCK_INIT;
      rv3d->viewlock = 0;

      /* FIXME: This fixes missing update to workbench TAA. (see #76216)
       * However, it would be nice if the tagging should be done in a more conventional way. */
      rv3d->rflag |= RV3D_GPULIGHT_UPDATE;

      /* Accumulate locks, in case they're mixed. */
      LISTBASE_FOREACH (ARegion *, region_iter, &area->regionbase) {
        if (region_iter->regiontype == RGN_TYPE_WINDOW) {
          RegionView3D *rv3d_iter = static_cast<RegionView3D *>(region_iter->regiondata);
          rv3d->viewlock_quad |= rv3d_iter->viewlock;
        }
      }
    }

    LISTBASE_FOREACH_MUTABLE (ARegion *, region_iter, &area->regionbase) {
      if (region_iter->alignment == RGN_ALIGN_QSPLIT) {
        ED_region_remove(C, area, region_iter);
        if (region_iter == screen->active_region) {
          screen->active_region = nullptr;
        }
      }
    }
    ED_area_tag_redraw(area);
    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);
  }
  else if (region->next) {
    BKE_report(op->reports, RPT_ERROR, "Only last region can be 4-split");
  }
  else {
    /* Enter quad-view */
    ScrArea *area = CTX_wm_area(C);

    region->alignment = RGN_ALIGN_QSPLIT;

    for (int count = 0; count < 3; count++) {
      ARegion *new_region = BKE_area_region_copy(area->type, region);
      BLI_addtail(&area->regionbase, new_region);
    }

    /* lock views and set them */
    if (area->spacetype == SPACE_VIEW3D) {
      View3D *v3d = static_cast<View3D *>(area->spacedata.first);
      int index_qsplit = 0;

      /* run ED_view3d_lock() so the correct 'rv3d->viewquat' is set,
       * otherwise when restoring rv3d->localvd the 'viewquat' won't
       * match the 'view', set on entering localview See: #26315,
       *
       * We could avoid manipulating rv3d->localvd here if exiting
       * localview with a 4-split would assign these view locks */
      RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
      const char viewlock = (rv3d->viewlock_quad & RV3D_VIEWLOCK_INIT) ?
                                (rv3d->viewlock_quad & ~RV3D_VIEWLOCK_INIT) :
                                RV3D_LOCK_ROTATION;

      region_quadview_init_rv3d(
          area, region, viewlock, ED_view3d_lock_view_from_index(index_qsplit++), RV3D_ORTHO);
      region_quadview_init_rv3d(area,
                                (region = region->next),
                                viewlock,
                                ED_view3d_lock_view_from_index(index_qsplit++),
                                RV3D_ORTHO);
      region_quadview_init_rv3d(area,
                                (region = region->next),
                                viewlock,
                                ED_view3d_lock_view_from_index(index_qsplit++),
                                RV3D_ORTHO);
/* forcing camera is distracting */
#if 0
      if (v3d->camera) {
        region_quadview_init_rv3d(area, (region = region->next), 0, RV3D_VIEW_CAMERA, RV3D_CAMOB);
      }
      else {
        region_quadview_init_rv3d(area, (region = region->next), 0, RV3D_VIEW_USER, RV3D_PERSP);
      }
#else
      (void)v3d;
#endif
    }
    ED_area_tag_redraw(area);
    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_region_quadview(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Quad View";
  ot->description = "Split selected area into camera, front, right, and top views";
  ot->idname = "SCREEN_OT_region_quadview";

  /* API callbacks. */
  ot->exec = region_quadview_exec;
  ot->poll = ED_operator_region_view3d_active;
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region Toggle Operator
 * \{ */

static wmOperatorStatus region_toggle_exec(bContext *C, wmOperator *op)
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "region_type");

  ARegion *region;
  if (RNA_property_is_set(op->ptr, prop)) {
    region = BKE_area_find_region_type(CTX_wm_area(C), RNA_property_enum_get(op->ptr, prop));
  }
  else {
    region = CTX_wm_region(C);
  }

  if (region && (region->alignment != RGN_ALIGN_NONE)) {
    ED_region_toggle_hidden(C, region);
  }
  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

static bool region_toggle_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);

  /* Don't flip anything around in top-bar. */
  if (area && area->spacetype == SPACE_TOPBAR) {
    CTX_wm_operator_poll_msg_set(C, "Toggling regions in the Top-bar is not allowed");
    return false;
  }

  return ED_operator_areaactive(C);
}

static void SCREEN_OT_region_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Region";
  ot->idname = "SCREEN_OT_region_toggle";
  ot->description = "Hide or unhide the region";

  /* API callbacks. */
  ot->exec = region_toggle_exec;
  ot->poll = region_toggle_poll;
  ot->flag = 0;

  RNA_def_enum(ot->srna,
               "region_type",
               rna_enum_region_type_items,
               0,
               "Region Type",
               "Type of the region to toggle");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region Flip Operator
 * \{ */

/* flip a region alignment */
static wmOperatorStatus region_flip_exec(bContext *C, wmOperator * /*op*/)
{
  ARegion *region = CTX_wm_region(C);

  if (!region) {
    return OPERATOR_CANCELLED;
  }

  if (region->alignment == RGN_ALIGN_TOP) {
    region->alignment = RGN_ALIGN_BOTTOM;
  }
  else if (region->alignment == RGN_ALIGN_BOTTOM) {
    region->alignment = RGN_ALIGN_TOP;
  }
  else if (region->alignment == RGN_ALIGN_LEFT) {
    region->alignment = RGN_ALIGN_RIGHT;
  }
  else if (region->alignment == RGN_ALIGN_RIGHT) {
    region->alignment = RGN_ALIGN_LEFT;
  }

  ED_area_tag_redraw(CTX_wm_area(C));
  WM_event_add_mousemove(CTX_wm_window(C));
  WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static bool region_flip_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);

  /* Don't flip anything around in top-bar. */
  if (area && area->spacetype == SPACE_TOPBAR) {
    CTX_wm_operator_poll_msg_set(C, "Flipping regions in the Top-bar is not allowed");
    return false;
  }

  return ED_operator_areaactive(C);
}

static void SCREEN_OT_region_flip(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Flip Region";
  ot->idname = "SCREEN_OT_region_flip";
  ot->description = "Toggle the region's alignment (left/right or top/bottom)";

  /* API callbacks. */
  ot->exec = region_flip_exec;
  ot->poll = region_flip_poll;
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Header Toggle Menu Operator
 * \{ */

/* show/hide header text menus */
static wmOperatorStatus header_toggle_menus_exec(bContext *C, wmOperator * /*op*/)
{
  ScrArea *area = CTX_wm_area(C);

  area->flag = area->flag ^ HEADER_NO_PULLDOWN;

  ED_area_tag_redraw(area);
  WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_header_toggle_menus(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Expand/Collapse Header Menus";
  ot->idname = "SCREEN_OT_header_toggle_menus";
  ot->description = "Expand or collapse the header pull-down menus";

  /* API callbacks. */
  ot->exec = header_toggle_menus_exec;
  ot->poll = ED_operator_areaactive;
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region Context Menu Operator (Header/Footer/Navigation-Bar)
 * \{ */

static void screen_area_menu_items(ScrArea *area, uiLayout *layout)
{
  if (ED_area_is_global(area)) {
    return;
  }

  PointerRNA ptr;

  ptr = layout->op("SCREEN_OT_area_join",
                   IFACE_("Move/Split Area"),
                   ICON_AREA_DOCK,
                   blender::wm::OpCallContext::InvokeDefault,
                   UI_ITEM_NONE);

  layout->separator();

  layout->op("SCREEN_OT_screen_full_area",
             area->full ? IFACE_("Restore Areas") : IFACE_("Maximize Area"),
             ICON_NONE);

  if (area->spacetype != SPACE_FILE && !area->full) {
    ptr = layout->op("SCREEN_OT_screen_full_area",
                     IFACE_("Focus Mode"),
                     ICON_NONE,
                     blender::wm::OpCallContext::InvokeDefault,
                     UI_ITEM_NONE);
    RNA_boolean_set(&ptr, "use_hide_panels", true);
  }

  layout->op("SCREEN_OT_area_dupli", std::nullopt, ICON_NONE);
  layout->separator();
  layout->op("SCREEN_OT_area_close", std::nullopt, ICON_X);
}

void ED_screens_header_tools_menu_create(bContext *C, uiLayout *layout, void * /*arg*/)
{
  ScrArea *area = CTX_wm_area(C);
  {
    PointerRNA ptr = RNA_pointer_create_discrete(
        (ID *)CTX_wm_screen(C), &RNA_Space, area->spacedata.first);
    if (!ELEM(area->spacetype, SPACE_TOPBAR)) {
      layout->prop(&ptr, "show_region_header", UI_ITEM_NONE, IFACE_("Show Header"), ICON_NONE);
    }

    ARegion *region_header = BKE_area_find_region_type(area, RGN_TYPE_HEADER);
    uiLayout *col = &layout->column(false);
    col->active_set((region_header->flag & RGN_FLAG_HIDDEN) == 0);

    if (BKE_area_find_region_type(area, RGN_TYPE_TOOL_HEADER)) {
      col->prop(
          &ptr, "show_region_tool_header", UI_ITEM_NONE, IFACE_("Show Tool Settings"), ICON_NONE);
    }

    col->op("SCREEN_OT_header_toggle_menus",
            IFACE_("Show Menus"),
            (area->flag & HEADER_NO_PULLDOWN) ? ICON_CHECKBOX_DEHLT : ICON_CHECKBOX_HLT);
  }

  if (!ELEM(area->spacetype, SPACE_TOPBAR)) {
    layout->separator();
    ED_screens_region_flip_menu_create(C, layout, nullptr);
    layout->separator();
    screen_area_menu_items(area, layout);
  }
}

void ED_screens_footer_tools_menu_create(bContext *C, uiLayout *layout, void * /*arg*/)
{
  ScrArea *area = CTX_wm_area(C);

  {
    PointerRNA ptr = RNA_pointer_create_discrete(
        (ID *)CTX_wm_screen(C), &RNA_Space, area->spacedata.first);
    layout->prop(&ptr, "show_region_footer", UI_ITEM_NONE, IFACE_("Show Footer"), ICON_NONE);
  }

  ED_screens_region_flip_menu_create(C, layout, nullptr);
  layout->separator();
  screen_area_menu_items(area, layout);
}

void ED_screens_region_flip_menu_create(bContext *C, uiLayout *layout, void * /*arg*/)
{
  const ARegion *region = CTX_wm_region(C);
  const short region_alignment = RGN_ALIGN_ENUM_FROM_MASK(region->alignment);
  const char *but_flip_str = region_alignment == RGN_ALIGN_LEFT   ? IFACE_("Flip to Right") :
                             region_alignment == RGN_ALIGN_RIGHT  ? IFACE_("Flip to Left") :
                             region_alignment == RGN_ALIGN_BOTTOM ? IFACE_("Flip to Top") :
                                                                    IFACE_("Flip to Bottom");

  /* default is blender::wm::OpCallContext::InvokeRegionWin, which we don't want here. */
  layout->operator_context_set(blender::wm::OpCallContext::InvokeDefault);

  layout->op("SCREEN_OT_region_flip", but_flip_str, ICON_NONE);
}

static void ed_screens_statusbar_menu_create(uiLayout *layout, void * /*arg*/)
{
  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, &RNA_PreferencesView, &U);
  layout->prop(&ptr, "show_statusbar_stats", UI_ITEM_NONE, IFACE_("Scene Statistics"), ICON_NONE);
  layout->prop(
      &ptr, "show_statusbar_scene_duration", UI_ITEM_NONE, IFACE_("Scene Duration"), ICON_NONE);
  layout->prop(&ptr, "show_statusbar_memory", UI_ITEM_NONE, IFACE_("System Memory"), ICON_NONE);
  if (GPU_mem_stats_supported()) {
    layout->prop(&ptr, "show_statusbar_vram", UI_ITEM_NONE, IFACE_("Video Memory"), ICON_NONE);
  }
  layout->prop(
      &ptr, "show_extensions_updates", UI_ITEM_NONE, IFACE_("Extensions Updates"), ICON_NONE);
  layout->prop(&ptr, "show_statusbar_version", UI_ITEM_NONE, IFACE_("Blender Version"), ICON_NONE);
}

static wmOperatorStatus screen_context_menu_invoke(bContext *C,
                                                   wmOperator * /*op*/,
                                                   const wmEvent * /*event*/)
{
  const ScrArea *area = CTX_wm_area(C);
  const ARegion *region = CTX_wm_region(C);

  if (area && area->spacetype == SPACE_STATUSBAR) {
    uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Status Bar"), ICON_NONE);
    uiLayout *layout = UI_popup_menu_layout(pup);
    ed_screens_statusbar_menu_create(layout, nullptr);
    UI_popup_menu_end(C, pup);
  }
  else if (region) {
    if (ELEM(region->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
      uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Header"), ICON_NONE);
      uiLayout *layout = UI_popup_menu_layout(pup);
      ED_screens_header_tools_menu_create(C, layout, nullptr);
      UI_popup_menu_end(C, pup);
    }
    else if (region->regiontype == RGN_TYPE_FOOTER) {
      uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Footer"), ICON_NONE);
      uiLayout *layout = UI_popup_menu_layout(pup);
      ED_screens_footer_tools_menu_create(C, layout, nullptr);
      UI_popup_menu_end(C, pup);
    }
    else if (region->regiontype == RGN_TYPE_NAV_BAR) {
      uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Navigation Bar"), ICON_NONE);
      uiLayout *layout = UI_popup_menu_layout(pup);

      /* We need blender::wm::OpCallContext::InvokeDefault in case menu item is over another area.
       */
      layout->operator_context_set(blender::wm::OpCallContext::InvokeDefault);
      layout->op("SCREEN_OT_region_toggle", IFACE_("Hide"), ICON_NONE);

      ED_screens_region_flip_menu_create(C, layout, nullptr);
      const ScrArea *area = CTX_wm_area(C);
      if (area && area->spacetype == SPACE_PROPERTIES) {
        layout->menu_fn(IFACE_("Visible Tabs"), ICON_NONE, ED_buttons_visible_tabs_menu, nullptr);
      }
      UI_popup_menu_end(C, pup);
    }
  }

  return OPERATOR_INTERFACE;
}

static void SCREEN_OT_region_context_menu(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Region";
  ot->description = "Display region context menu";
  ot->idname = "SCREEN_OT_region_context_menu";

  /* API callbacks. */
  ot->invoke = screen_context_menu_invoke;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation Step Operator
 *
 * Animation Step.
 * \{ */

static bool match_region_with_redraws(const ScrArea *area,
                                      eRegion_Type regiontype,
                                      eScreen_Redraws_Flag redraws,
                                      bool from_anim_edit)
{
  const eSpace_Type spacetype = eSpace_Type(area->spacetype);
  if (regiontype == RGN_TYPE_WINDOW) {

    switch (spacetype) {
      case SPACE_VIEW3D:
        if ((redraws & TIME_ALL_3D_WIN) || from_anim_edit) {
          return true;
        }
        break;
      case SPACE_GRAPH:
      case SPACE_NLA:
        if ((redraws & TIME_ALL_ANIM_WIN) || from_anim_edit) {
          return true;
        }
        break;
      case SPACE_ACTION:
        /* if only 1 window or 3d windows, we do timeline too
         * NOTE: Now we do action editor in all these cases, since timeline is here. */
        if ((redraws & (TIME_ALL_ANIM_WIN | TIME_REGION | TIME_ALL_3D_WIN)) || from_anim_edit) {
          return true;
        }
        break;
      case SPACE_PROPERTIES:
        if (redraws & TIME_ALL_BUTS_WIN) {
          return true;
        }
        break;
      case SPACE_SEQ:
        if ((redraws & (TIME_SEQ | TIME_ALL_ANIM_WIN)) || from_anim_edit) {
          return true;
        }
        break;
      case SPACE_NODE:
        if (redraws & TIME_NODES) {
          return true;
        }
        break;
      case SPACE_IMAGE:
        if ((redraws & TIME_ALL_IMAGE_WIN) || from_anim_edit) {
          return true;
        }
        break;
      case SPACE_CLIP:
        if ((redraws & TIME_CLIPS) || from_anim_edit) {
          return true;
        }
        break;
      case SPACE_SPREADSHEET:
        if (redraws & TIME_SPREADSHEETS) {
          return true;
        }
        break;
      default:
        break;
    }
  }
  else if (regiontype == RGN_TYPE_UI) {
    if (spacetype == SPACE_CLIP) {
      /* Track Preview button is on Properties Editor in SpaceClip,
       * and it's very common case when users want it be refreshing
       * during playback, so asking people to enable special option
       * for this is a bit tricky, so add exception here for refreshing
       * Properties Editor for SpaceClip always */
      return true;
    }

    if (redraws & TIME_ALL_BUTS_WIN) {
      return true;
    }
  }
  else if (regiontype == RGN_TYPE_HEADER) {
    /* The Timeline mode of the Dope Sheet shows playback controls in the header. */
    if (spacetype == SPACE_ACTION) {
      SpaceAction *saction = (SpaceAction *)area->spacedata.first;
      return saction->mode == SACTCONT_TIMELINE;
    }
  }
  else if (regiontype == RGN_TYPE_FOOTER) {
    /* The footer region in animation editors shows the current frame. */
    if (ELEM(spacetype, SPACE_ACTION, SPACE_GRAPH, SPACE_SEQ, SPACE_NLA)) {
      return true;
    }
  }
  else if (regiontype == RGN_TYPE_PREVIEW) {
    switch (spacetype) {
      case SPACE_SEQ:
        if (redraws & (TIME_SEQ | TIME_ALL_ANIM_WIN)) {
          return true;
        }
        break;
      case SPACE_CLIP:
        return true;
      default:
        break;
    }
  }
  else if (regiontype == RGN_TYPE_TOOLS) {
    switch (spacetype) {
      case SPACE_SPREADSHEET:
        if (redraws & TIME_SPREADSHEETS) {
          return true;
        }
        break;
      default:
        break;
    }
  }

  return false;
}

static void screen_animation_region_tag_redraw(
    bContext *C, ScrArea *area, ARegion *region, const Scene *scene, eScreen_Redraws_Flag redraws)
{
  /* Do follow time here if editor type supports it */
  if ((redraws & TIME_FOLLOW) &&
      screen_animation_region_supports_time_follow(eSpace_Type(area->spacetype),
                                                   eRegion_Type(region->regiontype)))
  {
    float w = BLI_rctf_size_x(&region->v2d.cur);
    if (scene->r.cfra < region->v2d.cur.xmin) {
      region->v2d.cur.xmax = scene->r.cfra;
      region->v2d.cur.xmin = region->v2d.cur.xmax - w;
      ED_region_tag_redraw(region);
      return;
    }
    if (scene->r.cfra > region->v2d.cur.xmax) {
      region->v2d.cur.xmin = scene->r.cfra;
      region->v2d.cur.xmax = region->v2d.cur.xmin + w;
      ED_region_tag_redraw(region);
      return;
    }
  }

  /* No need to do a full redraw as the current frame indicator is only updated.
   * We do need to redraw when this area is in full screen as no other areas
   * will be tagged for redrawing. */
  if (region->regiontype == RGN_TYPE_WINDOW && !area->full) {
    if (ELEM(area->spacetype, SPACE_NLA, SPACE_ACTION)) {
      return;
    }

    /* Drivers Editor needs a full redraw on playback for graph_draw_driver_debug().
     * This will make it slower than regular graph editor during playback, but drawing this in
     * graph_main_region_draw_overlay() is not feasible because it requires animation filtering
     * which has significant overhead which needs to be avoided in the overlay which is redrawn on
     * every UI interaction. */
    if (area->spacetype == SPACE_GRAPH) {
      const SpaceGraph *sipo = static_cast<const SpaceGraph *>(area->spacedata.first);
      if (sipo->mode != SIPO_MODE_DRIVERS) {
        return;
      }
      bAnimContext ac;
      if (ANIM_animdata_get_context(C, &ac) == false) {
        return;
      }
      if (ac.datatype != ANIMCONT_DRIVERS) {
        return;
      }
    }

    if (area->spacetype == SPACE_SEQ) {
      if (!blender::ed::vse::has_playback_animation(scene)) {
        return;
      }
    }
  }
  ED_region_tag_redraw(region);
}

// #define PROFILE_AUDIO_SYNC

static wmOperatorStatus screen_animation_step_invoke(bContext *C,
                                                     wmOperator * /*op*/,
                                                     const wmEvent *event)
{
  bScreen *screen = CTX_wm_screen(C);
  wmTimer *wt = screen->animtimer;

  if (!(wt && wt == event->customdata)) {
    return OPERATOR_PASS_THROUGH;
  }

#ifdef PROFILE_AUDIO_SYNC
  static int old_frame = 0;
  int newfra_int;
#endif

  Main *bmain = CTX_data_main(C);
  ScreenAnimData *sad = static_cast<ScreenAnimData *>(wt->customdata);
  Scene *scene = sad->scene;
  ViewLayer *view_layer = sad->view_layer;
  Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer);
  Scene *scene_eval = (depsgraph != nullptr) ? DEG_get_evaluated_scene(depsgraph) : nullptr;
  wmWindowManager *wm = CTX_wm_manager(C);
  int sync;
  double time;

  /* sync, don't sync, or follow scene setting */
  if (sad->flag & ANIMPLAY_FLAG_SYNC) {
    sync = 1;
  }
  else if (sad->flag & ANIMPLAY_FLAG_NO_SYNC) {
    sync = 0;
  }
  else {
    sync = (scene->flag & SCE_FRAME_DROP);
  }

  if (scene_eval == nullptr) {
    /* Happens when undo/redo system is used during playback, nothing meaningful we can do here. */
  }
  else if (scene_eval->id.recalc & ID_RECALC_FRAME_CHANGE) {
    /* Ignore seek here, the audio will be updated to the scene frame after jump during next
     * dependency graph update. */
  }
  else if ((scene->audio.flag & AUDIO_SYNC) && (sad->flag & ANIMPLAY_FLAG_REVERSE) == false &&
           isfinite(time = BKE_sound_sync_scene(scene_eval)))
  {
    scene->r.cfra = round(time * scene->frames_per_second());

#ifdef PROFILE_AUDIO_SYNC
    newfra_int = scene->r.cfra;
    if (newfra_int < old_frame) {
      printf("back -%d jump detected, frame %d!\n", old_frame - newfra_int, old_frame);
    }
    else if (newfra_int > old_frame + 1) {
      printf("forward +%d jump detected, frame %d!\n", newfra_int - old_frame, old_frame);
    }
    fflush(stdout);
    old_frame = newfra_int;
#endif
  }
  else {
    if (sync) {
      /* Try to keep the playback in realtime by dropping frames. */

      /* How much time (in frames) has passed since the last frame was drawn? */
      double delta_frames = wt->time_delta * scene->frames_per_second();

      /* Add the remaining fraction from the last time step. */
      delta_frames += sad->lagging_frame_count;

      if (delta_frames < 1.0) {
        /* We can render faster than the scene frame rate. However skipping or delaying frames
         * here seems to in practice lead to jittery playback so just step forward a minimum of
         * one frame. (Even though this can lead to too fast playback, the jitteriness is more
         * annoying)
         */
        delta_frames = 1.0f;
        sad->lagging_frame_count = 0;
      }
      else {
        /* Extract the delta frame fractions that will be skipped when converting to int. */
        sad->lagging_frame_count = delta_frames - int(delta_frames);
      }

      const int step = delta_frames;

      /* skip frames */
      if (sad->flag & ANIMPLAY_FLAG_REVERSE) {
        scene->r.cfra -= step;
      }
      else {
        scene->r.cfra += step;
      }
    }
    else {
      /* one frame +/- */
      if (sad->flag & ANIMPLAY_FLAG_REVERSE) {
        scene->r.cfra--;
      }
      else {
        scene->r.cfra++;
      }
    }
  }

  /* reset 'jumped' flag before checking if we need to jump... */
  sad->flag &= ~ANIMPLAY_FLAG_JUMPED;

  if (sad->flag & ANIMPLAY_FLAG_REVERSE) {
    /* jump back to end? */
    if (PRVRANGEON) {
      if (scene->r.cfra < scene->r.psfra) {
        scene->r.cfra = scene->r.pefra;
        sad->flag |= ANIMPLAY_FLAG_JUMPED;
      }
    }
    else {
      if (scene->r.cfra < scene->r.sfra) {
        scene->r.cfra = scene->r.efra;
        sad->flag |= ANIMPLAY_FLAG_JUMPED;
      }
    }
  }
  else {
    /* jump back to start? */
    if (PRVRANGEON) {
      if (scene->r.cfra > scene->r.pefra) {
        scene->r.cfra = scene->r.psfra;
        sad->flag |= ANIMPLAY_FLAG_JUMPED;
      }
    }
    else {
      if (scene->r.cfra > scene->r.efra) {
        scene->r.cfra = scene->r.sfra;
        sad->flag |= ANIMPLAY_FLAG_JUMPED;
      }
    }
  }

  /* next frame overridden by user action (pressed jump to first/last frame) */
  if (sad->flag & ANIMPLAY_FLAG_USE_NEXT_FRAME) {
    scene->r.cfra = sad->nextfra;
    sad->flag &= ~ANIMPLAY_FLAG_USE_NEXT_FRAME;
    sad->flag |= ANIMPLAY_FLAG_JUMPED;
  }

  if (sad->flag & ANIMPLAY_FLAG_JUMPED) {
    DEG_id_tag_update(&scene->id, ID_RECALC_FRAME_CHANGE);
#ifdef PROFILE_AUDIO_SYNC
    old_frame = scene->r.cfra;
#endif
  }

  blender::ed::vse::sync_active_scene_and_time_with_scene_strip(*C);

  /* Since we follow draw-flags, we can't send notifier but tag regions ourselves. */
  if (depsgraph != nullptr) {
    ED_update_for_newframe(bmain, depsgraph);

    /* Updating the frame, and invoking the frame pre/post hooks, can result in the current timer
     * being removed. For example, calling `screen.animation_cancel` inside `frame_change_post`. */
    if (wt->flags & WM_TIMER_TAGGED_FOR_REMOVAL) {
      return OPERATOR_FINISHED;
    }
  }

  LISTBASE_FOREACH (wmWindow *, window, &wm->windows) {
    bScreen *win_screen = WM_window_get_active_screen(window);

    LISTBASE_FOREACH (ScrArea *, area, &win_screen->areabase) {
      LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
        bool redraw = false;
        if (region == sad->region) {
          redraw = true;
        }
        else if (match_region_with_redraws(area,
                                           eRegion_Type(region->regiontype),
                                           eScreen_Redraws_Flag(sad->redraws),
                                           sad->from_anim_edit))
        {
          redraw = true;
        }

        if (redraw) {
          screen_animation_region_tag_redraw(
              C, area, region, scene, eScreen_Redraws_Flag(sad->redraws));
          /* Doesn't trigger a full redraw of the screen but makes sure at least overlay drawing
           * (#ARegionType.draw_overlay()) is triggered, which is how the current-frame is drawn.
           */
          win_screen->do_draw = true;
        }
      }
    }
  }

  if (U.uiflag & USER_SHOW_FPS) {
    /* Update frame rate info too.
     * NOTE: this may not be accurate enough, since we might need this after modifiers/etc.
     * have been calculated instead of just before updates have been done? */
    ED_scene_fps_average_accumulate(scene, U.playback_fps_samples, wt->time_last);
  }

  /* Recalculate the time-step for the timer now that we've finished calculating this,
   * since the frames-per-second value may have been changed.
   */
  /* TODO: this may make evaluation a bit slower if the value doesn't change...
   * any way to avoid this? */
  wt->time_step = (1.0 / scene->frames_per_second());

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_animation_step(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Animation Step";
  ot->description = "Step through animation by position";
  ot->idname = "SCREEN_OT_animation_step";

  /* API callbacks. */
  ot->invoke = screen_animation_step_invoke;

  ot->poll = operator_screenactive_norender;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation Playback Operator
 *
 * Animation Playback with Timer.
 * \{ */

void ED_reset_audio_device(bContext *C)
{
  /* If sound was playing back when we changed any sound settings, we need to make sure that
   * we reinitialize the playback state properly. Audaspace pauses playback on re-initializing
   * the playback device, so we need to make sure we reinitialize the playback state on our
   * end as well. (Otherwise the sound device might be in a weird state and crashes Blender). */
  bScreen *screen = ED_screen_animation_playing(CTX_wm_manager(C));
  wmWindow *timer_win = nullptr;
  const bool is_playing = screen != nullptr;
  bool playback_sync = false;
  int play_direction = 0;

  if (is_playing) {
    ScreenAnimData *sad = static_cast<ScreenAnimData *>(screen->animtimer->customdata);
    timer_win = screen->animtimer->win;
    /* -1 means play backwards. */
    play_direction = (sad->flag & ANIMPLAY_FLAG_REVERSE) ? -1 : 1;
    playback_sync = sad->flag & ANIMPLAY_FLAG_SYNC;
    /* Stop playback. */
    ED_screen_animation_play(C, 0, 0);
  }
  Main *bmain = CTX_data_main(C);
  /* Re-initialize the audio device. */
  BKE_sound_init(bmain);
  if (is_playing) {
    /* We need to set the context window to the window that was playing back previously.
     * Otherwise we will attach the new playback timer to an other window.
     */
    wmWindow *win = CTX_wm_window(C);
    CTX_wm_window_set(C, timer_win);
    ED_screen_animation_play(C, playback_sync, play_direction);
    CTX_wm_window_set(C, win);
  }
}

bScreen *ED_screen_animation_playing(const wmWindowManager *wm)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    bScreen *screen = WM_window_get_active_screen(win);

    if (screen->animtimer || screen->scrubbing) {
      return screen;
    }
  }

  return nullptr;
}

bScreen *ED_screen_animation_no_scrub(const wmWindowManager *wm)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    bScreen *screen = WM_window_get_active_screen(win);

    if (screen->animtimer) {
      return screen;
    }
  }

  return nullptr;
}

static void stop_playback(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  bScreen *screen = ED_screen_animation_playing(CTX_wm_manager(C));
  wmTimer *wt = screen->animtimer;
  ScreenAnimData *sad = static_cast<ScreenAnimData *>(wt->customdata);
  Scene *scene = sad->scene;

  ViewLayer *view_layer = sad->view_layer;
  Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, scene, view_layer);
  BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);

  /* Only stop sound playback, when playing forward, since there is no sound for reverse
   * playback. */
  if ((sad->flag & ANIMPLAY_FLAG_REVERSE) == 0) {
    BKE_sound_stop_scene(scene_eval);
  }

  ED_screen_animation_timer(C, scene, view_layer, 0, 0, 0);
  ED_scene_fps_average_clear(scene);
  BKE_callback_exec_id_depsgraph(bmain, &scene->id, depsgraph, BKE_CB_EVT_ANIMATION_PLAYBACK_POST);

  /* Triggers redraw of sequencer preview so that it does not show to fps anymore after stopping
   * playback. */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_SEQUENCER, scene);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_SPREADSHEET, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_TRANSFORM, scene);
}

static wmOperatorStatus start_playback(bContext *C, int sync, int mode)
{
  Main *bmain = CTX_data_main(C);
  bScreen *screen = CTX_wm_screen(C);

  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  if (!scene) {
    return OPERATOR_CANCELLED;
  }
  ViewLayer *view_layer = is_sequencer ? BKE_view_layer_default_render(scene) :
                                         CTX_data_view_layer(C);
  Depsgraph *depsgraph = is_sequencer ? BKE_scene_ensure_depsgraph(bmain, scene, view_layer) :
                                        CTX_data_ensure_evaluated_depsgraph(C);
  if (is_sequencer) {
    BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
  }
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);

  BKE_callback_exec_id_depsgraph(bmain, &scene->id, depsgraph, BKE_CB_EVT_ANIMATION_PLAYBACK_PRE);

  /* Only play sound when playing forward. Reverse sound playback is not implemented. */
  if (mode == 1) {
    BKE_sound_play_scene(scene_eval);
  }

  ED_screen_animation_timer(C, scene, view_layer, screen->redraws_flag, sync, mode);
  ED_scene_fps_average_clear(scene);

  if (screen->animtimer) {
    wmTimer *wt = screen->animtimer;
    ScreenAnimData *sad = static_cast<ScreenAnimData *>(wt->customdata);

    sad->region = CTX_wm_region(C);
  }

  return OPERATOR_FINISHED;
}

wmOperatorStatus ED_screen_animation_play(bContext *C, int sync, int mode)
{
  if (ED_screen_animation_playing(CTX_wm_manager(C))) {
    stop_playback(C);
    return OPERATOR_FINISHED;
  }

  return start_playback(C, sync, mode);
}

static wmOperatorStatus screen_animation_play_exec(bContext *C, wmOperator *op)
{
  int mode = RNA_boolean_get(op->ptr, "reverse") ? -1 : 1;
  int sync = -1;

  if (RNA_struct_property_is_set(op->ptr, "sync")) {
    sync = RNA_boolean_get(op->ptr, "sync");
  }

  return ED_screen_animation_play(C, sync, mode);
}

static void SCREEN_OT_animation_play(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Play Animation";
  ot->description = "Play animation";
  ot->idname = "SCREEN_OT_animation_play";

  /* API callbacks. */
  ot->exec = screen_animation_play_exec;

  ot->poll = operator_screenactive_norender;

  prop = RNA_def_boolean(
      ot->srna, "reverse", false, "Play in Reverse", "Animation is played backwards");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "sync", false, "Sync", "Drop frames to maintain framerate");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation Cancel Operator
 * \{ */

static wmOperatorStatus screen_animation_cancel_exec(bContext *C, wmOperator *op)
{
  bScreen *screen = ED_screen_animation_playing(CTX_wm_manager(C));

  if (screen) {
    bool restore_start_frame = RNA_boolean_get(op->ptr, "restore_frame") && screen->animtimer;
    int frame;
    if (restore_start_frame) {
      ScreenAnimData *sad = static_cast<ScreenAnimData *>(screen->animtimer->customdata);
      frame = sad->sfra;
    }

    /* Stop playback */
    ED_screen_animation_play(C, 0, 0);
    if (restore_start_frame) {
      Scene *scene = CTX_data_scene(C);
      /* reset current frame and just send a notifier to deal with the rest */
      scene->r.cfra = frame;
      WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
    }
  }

  return OPERATOR_PASS_THROUGH;
}

static void SCREEN_OT_animation_cancel(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cancel Animation";
  ot->description = "Cancel animation, returning to the original frame";
  ot->idname = "SCREEN_OT_animation_cancel";

  /* API callbacks. */
  ot->exec = screen_animation_cancel_exec;

  ot->poll = ED_operator_screenactive;

  RNA_def_boolean(ot->srna,
                  "restore_frame",
                  true,
                  "Restore Frame",
                  "Restore the frame when animation was initialized");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Operator (Template)
 * \{ */

/* operator state vars used: (added by default WM callbacks)
 * xmin, ymin
 * xmax, ymax
 *
 * customdata: the wmGesture pointer
 *
 * callbacks:
 *
 * exec()   has to be filled in by user
 *
 * invoke() default WM function
 * adds modal handler
 *
 * modal()  default WM function
 * accept modal events while doing it, calls exec(), handles ESC and border drawing
 *
 * poll()   has to be filled in by user for context
 */
#if 0
static wmOperatorStatus box_select_exec(bContext *C, wmOperator *op)
{
  int event_type = RNA_int_get(op->ptr, "event_type");

  if (event_type == LEFTMOUSE) {
    printf("box select do select\n");
  }
  else if (event_type == RIGHTMOUSE) {
    printf("box select deselect\n");
  }
  else {
    printf("box select do something\n");
  }

  return 1;
}

static void SCREEN_OT_box_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Box Select";
  ot->idname = "SCREEN_OT_box_select";

  /* API callbacks. */
  ot->exec = box_select_exec;
  ot->invoke = WM_gesture_box_invoke;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ED_operator_areaactive;

  /* rna */
  RNA_def_int(ot->srna, "event_type", 0, INT_MIN, INT_MAX, "Event Type", "", INT_MIN, INT_MAX);
  WM_operator_properties_border(ot);
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Full Screen Back Operator
 *
 * Use for generic full-screen 'back' button.
 * \{ */

static wmOperatorStatus fullscreen_back_exec(bContext *C, wmOperator *op)
{
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = nullptr;

  /* search current screen for 'fullscreen' areas */
  LISTBASE_FOREACH (ScrArea *, area_iter, &screen->areabase) {
    if (area_iter->full) {
      area = area_iter;
      break;
    }
  }
  if (!area) {
    BKE_report(op->reports, RPT_ERROR, "No fullscreen areas were found");
    return OPERATOR_CANCELLED;
  }

  ED_screen_full_prevspace(C, area);

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_back_to_previous(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Back to Previous Screen";
  ot->description = "Revert back to the original screen layout, before fullscreen area overlay";
  ot->idname = "SCREEN_OT_back_to_previous";

  /* API callbacks. */
  ot->exec = fullscreen_back_exec;
  ot->poll = ED_operator_screenactive;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Show User Preferences Operator
 * \{ */

static wmOperatorStatus userpref_show_exec(bContext *C, wmOperator *op)
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "section");
  if (prop && RNA_property_is_set(op->ptr, prop)) {
    /* Set active section via RNA, so it can fail properly. */

    PointerRNA pref_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Preferences, &U);
    PropertyRNA *active_section_prop = RNA_struct_find_property(&pref_ptr, "active_section");

    RNA_property_enum_set(&pref_ptr, active_section_prop, RNA_property_enum_get(op->ptr, prop));
    RNA_property_update(C, &pref_ptr, active_section_prop);
  }

  /* changes context! */
  if (ScrArea *area = ED_screen_temp_space_open(
          C, nullptr, SPACE_USERPREF, U.preferences_display_type, false))
  {
    /* The header only contains the editor switcher and looks empty.
     * So hiding in the temp window makes sense. */
    ARegion *region_header = BKE_area_find_region_type(area, RGN_TYPE_HEADER);

    region_header->flag |= RGN_FLAG_HIDDEN;
    ED_region_visibility_change_update(C, area, region_header);

    return OPERATOR_FINISHED;
  }
  BKE_report(op->reports, RPT_ERROR, "Failed to open window!");
  return OPERATOR_CANCELLED;
}

static std::string userpref_show_get_description(bContext *C,
                                                 wmOperatorType * /*ot*/,
                                                 PointerRNA *ptr)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, "section");
  if (RNA_property_is_set(ptr, prop)) {
    int section = RNA_property_enum_get(ptr, prop);
    const char *section_name;
    if (RNA_property_enum_name_gettexted(C, ptr, prop, section, &section_name)) {
      return fmt::format(fmt::runtime(TIP_("Show {} preferences")), section_name);
    }
  }
  /* Fall back to default. */
  return "";
}

static void SCREEN_OT_userpref_show(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Open Preferences...";
  ot->description = "Edit user preferences and system settings";
  ot->idname = "SCREEN_OT_userpref_show";

  /* API callbacks. */
  ot->exec = userpref_show_exec;
  ot->poll = ED_operator_screenactive_nobackground; /* Not in background as this opens a window. */
  ot->get_description = userpref_show_get_description;

  prop = RNA_def_enum(ot->srna,
                      "section",
                      rna_enum_preference_section_items,
                      0,
                      "",
                      "Section to activate in the Preferences");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Show Drivers Editor Operator
 * \{ */

static wmOperatorStatus drivers_editor_show_exec(bContext *C, wmOperator *op)
{
  /* Get active property to show driver for
   * - Need to grab it first, or else this info disappears
   *   after we've created the window
   */
  int index;
  PointerRNA ptr;
  PropertyRNA *prop;
  uiBut *but = UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  /* changes context! */
  if (WM_window_open_temp(C, IFACE_("Blender Drivers Editor"), SPACE_GRAPH, false)) {
    ED_drivers_editor_init(C, CTX_wm_area(C));

    /* activate driver F-Curve for the property under the cursor */
    if (but) {
      bool driven, special;
      FCurve *fcu = BKE_fcurve_find_by_rna_context_ui(
          C, &ptr, prop, index, nullptr, nullptr, &driven, &special);

      if (fcu) {
        /* Isolate this F-Curve... */
        bAnimContext ac;
        if (ANIM_animdata_get_context(C, &ac)) {
          int filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_NODUPLIS;
          ANIM_anim_channels_select_set(&ac, ACHANNEL_SETFLAG_CLEAR);
          ANIM_set_active_channel(&ac,
                                  ac.data,
                                  eAnimCont_Types(ac.datatype),
                                  eAnimFilter_Flags(filter),
                                  fcu,
                                  ANIMTYPE_FCURVE);
        }
        else {
          /* Just blindly isolate...
           * This isn't the best, and shouldn't happen, but may be enough. */
          fcu->flag |= (FCURVE_ACTIVE | FCURVE_SELECTED);
        }
      }
    }

    return OPERATOR_FINISHED;
  }
  BKE_report(op->reports, RPT_ERROR, "Failed to open window!");
  return OPERATOR_CANCELLED;
}

static void SCREEN_OT_drivers_editor_show(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show Drivers Editor";
  ot->description = "Show drivers editor in a separate window";
  ot->idname = "SCREEN_OT_drivers_editor_show";

  /* API callbacks. */
  ot->exec = drivers_editor_show_exec;
  ot->poll = ED_operator_screenactive_nobackground; /* Not in background as this opens a window. */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Show Info Log Operator
 * \{ */

static wmOperatorStatus info_log_show_exec(bContext *C, wmOperator *op)
{
  /* changes context! */
  if (WM_window_open_temp(C, IFACE_("Blender Info Log"), SPACE_INFO, false)) {
    return OPERATOR_FINISHED;
  }
  BKE_report(op->reports, RPT_ERROR, "Failed to open window!");
  return OPERATOR_CANCELLED;
}

static void SCREEN_OT_info_log_show(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show Info Log";
  ot->description = "Show info log in a separate window";
  ot->idname = "SCREEN_OT_info_log_show";

  /* API callbacks. */
  ot->exec = info_log_show_exec;
  ot->poll = ED_operator_screenactive_nobackground;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name New Screen Operator
 * \{ */

static wmOperatorStatus screen_new_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  wmWindow *win = CTX_wm_window(C);
  WorkSpace *workspace = BKE_workspace_active_get(win->workspace_hook);
  WorkSpaceLayout *layout_old = BKE_workspace_active_layout_get(win->workspace_hook);

  WorkSpaceLayout *layout_new = ED_workspace_layout_duplicate(bmain, workspace, layout_old, win);

  WM_event_add_notifier(C, NC_SCREEN | ND_LAYOUTBROWSE, layout_new);

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_new(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "New Screen";
  ot->description = "Add a new screen";
  ot->idname = "SCREEN_OT_new";

  /* API callbacks. */
  ot->exec = screen_new_exec;
  ot->poll = WM_operator_winactive;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Screen Operator
 * \{ */

static wmOperatorStatus screen_delete_exec(bContext *C, wmOperator * /*op*/)
{
  bScreen *screen = CTX_wm_screen(C);
  WorkSpace *workspace = CTX_wm_workspace(C);
  WorkSpaceLayout *layout = BKE_workspace_layout_find(workspace, screen);

  WM_event_add_notifier(C, NC_SCREEN | ND_LAYOUTDELETE, layout);

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Screen";
  ot->description = "Delete active screen";
  ot->idname = "SCREEN_OT_delete";

  /* API callbacks. */
  ot->exec = screen_delete_exec;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region Alpha Blending Operator
 *
 * Implementation NOTE: a disappearing region needs at least 1 last draw with
 * 100% back-buffer texture over it - then triple buffer will clear it entirely.
 * This because flag #RGN_FLAG_HIDDEN is set in end - region doesn't draw at all then.
 *
 * \{ */

struct RegionAlphaInfo {
  ScrArea *area;
  ARegion *region, *child_region; /* other region */
  int hidden;
};

#define TIMEOUT 0.1f
#define TIMESTEP (1.0f / 60.0f)

float ED_region_blend_alpha(ARegion *region)
{
  /* check parent too */
  if (region->runtime->regiontimer == nullptr &&
      (region->alignment & (RGN_SPLIT_PREV | RGN_ALIGN_HIDE_WITH_PREV)) && region->prev)
  {
    region = region->prev;
  }

  if (region->runtime->regiontimer) {
    RegionAlphaInfo *rgi = static_cast<RegionAlphaInfo *>(
        region->runtime->regiontimer->customdata);
    float alpha;

    alpha = float(region->runtime->regiontimer->time_duration) / TIMEOUT;
    /* makes sure the blend out works 100% - without area redraws */
    if (rgi->hidden) {
      alpha = 0.9f - TIMESTEP - alpha;
    }

    CLAMP(alpha, 0.0f, 1.0f);
    return alpha;
  }
  return 1.0f;
}

/* assumes region has running region-blend timer */
static void region_blend_end(bContext *C, ARegion *region, const bool is_running)
{
  RegionAlphaInfo *rgi = static_cast<RegionAlphaInfo *>(region->runtime->regiontimer->customdata);

  /* always send redraw */
  ED_region_tag_redraw(region);
  if (rgi->child_region) {
    ED_region_tag_redraw(rgi->child_region);
  }

  /* if running timer was hiding, the flag toggle went wrong */
  if (is_running) {
    if (rgi->hidden) {
      rgi->region->flag &= ~RGN_FLAG_HIDDEN;
    }
  }
  else {
    if (rgi->hidden) {
      rgi->region->flag |= rgi->hidden;
      ED_area_init(C, CTX_wm_window(C), rgi->area);
    }
    /* area decoration needs redraw in end */
    ED_area_tag_redraw(rgi->area);
  }
  WM_event_timer_remove(CTX_wm_manager(C), nullptr, region->runtime->regiontimer); /* frees rgi */
  region->runtime->regiontimer = nullptr;
}
void ED_region_visibility_change_update_animated(bContext *C, ScrArea *area, ARegion *region)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);

  /* end running timer */
  if (region->runtime->regiontimer) {

    region_blend_end(C, region, true);
  }
  RegionAlphaInfo *rgi = MEM_callocN<RegionAlphaInfo>("RegionAlphaInfo");

  rgi->hidden = region->flag & RGN_FLAG_HIDDEN;
  rgi->area = area;
  rgi->region = region;
  region->flag &= ~RGN_FLAG_HIDDEN;

  /* blend in, reinitialize regions because it got unhidden */
  if (rgi->hidden == 0) {
    ED_area_init(C, win, area);
  }
  else {
    ED_region_visibility_change_update_ex(C, area, region, true, false);
  }

  if (region->next) {
    if (region->next->alignment & (RGN_SPLIT_PREV | RGN_ALIGN_HIDE_WITH_PREV)) {
      rgi->child_region = region->next;
    }
  }

  /* new timer */
  region->runtime->regiontimer = WM_event_timer_add(wm, win, TIMERREGION, TIMESTEP);
  region->runtime->regiontimer->customdata = rgi;
}

/* timer runs in win->handlers, so it cannot use context to find area/region */
static wmOperatorStatus region_blend_invoke(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  wmTimer *timer = static_cast<wmTimer *>(event->customdata);

  /* event type is TIMERREGION, but we better check */
  if (event->type != TIMERREGION || timer == nullptr) {
    return OPERATOR_PASS_THROUGH;
  }

  RegionAlphaInfo *rgi = static_cast<RegionAlphaInfo *>(timer->customdata);

  /* always send redraws */
  ED_region_tag_redraw(rgi->region);
  if (rgi->child_region) {
    ED_region_tag_redraw(rgi->child_region);
  }

  /* end timer? */
  if (rgi->region->runtime->regiontimer->time_duration > double(TIMEOUT)) {
    region_blend_end(C, rgi->region, false);
    return (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH);
  }

  return (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH);
}

static void SCREEN_OT_region_blend(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Region Alpha";
  ot->idname = "SCREEN_OT_region_blend";
  ot->description = "Blend in and out overlapping region";

  /* API callbacks. */
  ot->invoke = region_blend_invoke;

  /* flags */
  ot->flag = OPTYPE_INTERNAL;

  /* properties */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space Type Set or Cycle Operator
 * \{ */

static bool space_type_set_or_cycle_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  return (area && !ELEM(area->spacetype, SPACE_TOPBAR, SPACE_STATUSBAR));
}

static wmOperatorStatus space_type_set_or_cycle_exec(bContext *C, wmOperator *op)
{
  const int space_type = RNA_enum_get(op->ptr, "space_type");

  ScrArea *area = CTX_wm_area(C);
  PointerRNA ptr = RNA_pointer_create_discrete((ID *)CTX_wm_screen(C), &RNA_Area, area);
  PropertyRNA *prop_type = RNA_struct_find_property(&ptr, "type");
  PropertyRNA *prop_ui_type = RNA_struct_find_property(&ptr, "ui_type");

  if (area->spacetype != space_type) {
    /* Set the type. */
    RNA_property_enum_set(&ptr, prop_type, space_type);
    /* Specify that we want last-used if there are subtypes. */
    area->butspacetype_subtype = -1;
    RNA_property_update(C, &ptr, prop_type);
  }
  else {
    /* Types match, cycle the subtype. */
    const int space_type_ui = RNA_property_enum_get(&ptr, prop_ui_type);
    const EnumPropertyItem *item;
    int item_len;
    bool free;
    RNA_property_enum_items(C, &ptr, prop_ui_type, &item, &item_len, &free);
    int index = RNA_enum_from_value(item, space_type_ui);
    for (int i = 1; i < item_len; i++) {
      const EnumPropertyItem *item_test = &item[(index + i) % item_len];
      if ((item_test->value >> 16) == space_type) {
        RNA_property_enum_set(&ptr, prop_ui_type, item_test->value);
        RNA_property_update(C, &ptr, prop_ui_type);
        break;
      }
    }
    if (free) {
      MEM_freeN(item);
    }
  }

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_space_type_set_or_cycle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cycle Space Type Set";
  ot->description = "Set the space type or cycle subtype";
  ot->idname = "SCREEN_OT_space_type_set_or_cycle";

  /* API callbacks. */
  ot->exec = space_type_set_or_cycle_exec;
  ot->poll = space_type_set_or_cycle_poll;

  ot->flag = 0;

  RNA_def_enum(ot->srna, "space_type", rna_enum_space_type_items, SPACE_EMPTY, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space Context Cycle Operator
 * \{ */

static const EnumPropertyItem space_context_cycle_direction[] = {
    {SPACE_CONTEXT_CYCLE_PREV, "PREV", 0, "Previous", ""},
    {SPACE_CONTEXT_CYCLE_NEXT, "NEXT", 0, "Next", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static bool space_context_cycle_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  /* area might be nullptr if called out of window bounds */
  return (area && ELEM(area->spacetype, SPACE_PROPERTIES, SPACE_USERPREF));
}

/**
 * Helper to get the correct RNA pointer/property pair for changing
 * the display context of active space type in \a area.
 */
static void context_cycle_prop_get(bScreen *screen,
                                   const ScrArea *area,
                                   PointerRNA *r_ptr,
                                   PropertyRNA **r_prop)
{
  const char *propname;

  switch (area->spacetype) {
    case SPACE_PROPERTIES:
      *r_ptr = RNA_pointer_create_discrete(
          &screen->id, &RNA_SpaceProperties, area->spacedata.first);
      propname = "context";
      break;
    case SPACE_USERPREF:
      *r_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Preferences, &U);
      propname = "active_section";
      break;
    default:
      BLI_assert(0);
      propname = "";
  }

  *r_prop = RNA_struct_find_property(r_ptr, propname);
}

static wmOperatorStatus space_context_cycle_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent * /*event*/)
{
  const eScreenCycle direction = eScreenCycle(RNA_enum_get(op->ptr, "direction"));

  PointerRNA ptr;
  PropertyRNA *prop;
  context_cycle_prop_get(CTX_wm_screen(C), CTX_wm_area(C), &ptr, &prop);
  const int old_context = RNA_property_enum_get(&ptr, prop);
  const int new_context = RNA_property_enum_step(
      C, &ptr, prop, old_context, direction == SPACE_CONTEXT_CYCLE_PREV ? -1 : 1);
  RNA_property_enum_set(&ptr, prop, new_context);
  RNA_property_update(C, &ptr, prop);

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_space_context_cycle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cycle Space Context";
  ot->description = "Cycle through the editor context by activating the next/previous one";
  ot->idname = "SCREEN_OT_space_context_cycle";

  /* API callbacks. */
  ot->invoke = space_context_cycle_invoke;
  ot->poll = space_context_cycle_poll;

  ot->flag = 0;

  RNA_def_enum(ot->srna,
               "direction",
               space_context_cycle_direction,
               SPACE_CONTEXT_CYCLE_NEXT,
               "Direction",
               "Direction to cycle through");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Workspace Cycle Operator
 * \{ */

static wmOperatorStatus space_workspace_cycle_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent * /*event*/)
{
  wmWindow *win = CTX_wm_window(C);
  if (WM_window_is_temp_screen(win)) {
    return OPERATOR_CANCELLED;
  }

  Main *bmain = CTX_data_main(C);
  const eScreenCycle direction = eScreenCycle(RNA_enum_get(op->ptr, "direction"));
  WorkSpace *workspace_src = WM_window_get_active_workspace(win);

  Vector<ID *> ordered = BKE_id_ordered_list(&bmain->workspaces);
  if (ordered.size() == 1) {
    return OPERATOR_CANCELLED;
  }

  const int index = ordered.first_index_of(&workspace_src->id);

  WorkSpace *workspace_dst = nullptr;
  switch (direction) {
    case SPACE_CONTEXT_CYCLE_PREV:
      workspace_dst = reinterpret_cast<WorkSpace *>(index == 0 ? ordered.last() :
                                                                 ordered[index - 1]);
      break;
    case SPACE_CONTEXT_CYCLE_NEXT:
      workspace_dst = reinterpret_cast<WorkSpace *>(
          index == ordered.index_range().last() ? ordered.first() : ordered[index + 1]);
      break;
  }

  win->workspace_hook->temp_workspace_store = workspace_dst;
  WM_event_add_notifier(C, NC_SCREEN | ND_WORKSPACE_SET, workspace_dst);
  win->workspace_hook->temp_workspace_store = nullptr;

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_workspace_cycle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cycle Workspace";
  ot->description = "Cycle through workspaces";
  ot->idname = "SCREEN_OT_workspace_cycle";

  /* API callbacks. */
  ot->invoke = space_workspace_cycle_invoke;
  ot->poll = ED_operator_screenactive;

  ot->flag = 0;

  RNA_def_enum(ot->srna,
               "direction",
               space_context_cycle_direction,
               SPACE_CONTEXT_CYCLE_NEXT,
               "Direction",
               "Direction to cycle through");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Assigning Operator Types
 * \{ */

void ED_operatortypes_screen()
{
  /* Generic UI stuff. */
  WM_operatortype_append(SCREEN_OT_actionzone);
  WM_operatortype_append(SCREEN_OT_repeat_last);
  WM_operatortype_append(SCREEN_OT_repeat_history);
  WM_operatortype_append(SCREEN_OT_redo_last);

  /* Screen tools. */
  WM_operatortype_append(SCREEN_OT_area_move);
  WM_operatortype_append(SCREEN_OT_area_split);
  WM_operatortype_append(SCREEN_OT_area_join);
  WM_operatortype_append(SCREEN_OT_area_close);
  WM_operatortype_append(SCREEN_OT_area_options);
  WM_operatortype_append(SCREEN_OT_area_dupli);
  WM_operatortype_append(SCREEN_OT_area_swap);
  WM_operatortype_append(SCREEN_OT_region_quadview);
  WM_operatortype_append(SCREEN_OT_region_scale);
  WM_operatortype_append(SCREEN_OT_region_toggle);
  WM_operatortype_append(SCREEN_OT_region_flip);
  WM_operatortype_append(SCREEN_OT_header_toggle_menus);
  WM_operatortype_append(SCREEN_OT_region_context_menu);
  WM_operatortype_append(SCREEN_OT_screen_set);
  WM_operatortype_append(SCREEN_OT_screen_full_area);
  WM_operatortype_append(SCREEN_OT_back_to_previous);
  WM_operatortype_append(SCREEN_OT_spacedata_cleanup);
  WM_operatortype_append(SCREEN_OT_screenshot);
  WM_operatortype_append(SCREEN_OT_screenshot_area);
  WM_operatortype_append(SCREEN_OT_userpref_show);
  WM_operatortype_append(SCREEN_OT_drivers_editor_show);
  WM_operatortype_append(SCREEN_OT_info_log_show);
  WM_operatortype_append(SCREEN_OT_region_blend);
  WM_operatortype_append(SCREEN_OT_space_type_set_or_cycle);
  WM_operatortype_append(SCREEN_OT_space_context_cycle);
  WM_operatortype_append(SCREEN_OT_workspace_cycle);

  /* Frame changes. */
  WM_operatortype_append(SCREEN_OT_frame_offset);
  WM_operatortype_append(SCREEN_OT_frame_jump);
  WM_operatortype_append(SCREEN_OT_time_jump);
  WM_operatortype_append(SCREEN_OT_keyframe_jump);
  WM_operatortype_append(SCREEN_OT_marker_jump);

  WM_operatortype_append(SCREEN_OT_animation_step);
  WM_operatortype_append(SCREEN_OT_animation_play);
  WM_operatortype_append(SCREEN_OT_animation_cancel);

  /* New/delete. */
  WM_operatortype_append(SCREEN_OT_new);
  WM_operatortype_append(SCREEN_OT_delete);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Key Map
 * \{ */

static void keymap_modal_set(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {KM_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {KM_MODAL_APPLY, "APPLY", 0, "Apply", ""},
      {KM_MODAL_SNAP_ON, "SNAP", 0, "Snap On", ""},
      {KM_MODAL_SNAP_OFF, "SNAP_OFF", 0, "Snap Off", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* Standard Modal keymap ------------------------------------------------ */
  wmKeyMap *keymap = WM_modalkeymap_ensure(keyconf, "Standard Modal Map", modal_items);

  WM_modalkeymap_assign(keymap, "SCREEN_OT_area_move");
}

static bool blend_file_drop_poll(bContext * /*C*/, wmDrag *drag, const wmEvent * /*event*/)
{
  if (drag->type == WM_DRAG_PATH) {
    const eFileSel_File_Types file_type = eFileSel_File_Types(WM_drag_get_path_file_type(drag));
    if (ELEM(file_type, FILE_TYPE_BLENDER, FILE_TYPE_BLENDER_BACKUP)) {
      return true;
    }
  }
  return false;
}

static void blend_file_drop_copy(bContext * /*C*/, wmDrag *drag, wmDropBox *drop)
{
  /* copy drag path to properties */
  RNA_string_set(drop->ptr, "filepath", WM_drag_get_single_path(drag));
}

static bool screen_drop_scene_poll(bContext *C, wmDrag *drag, const wmEvent * /*event*/)
{
  /* Make sure we're dropping the scene outside the asset browser. */
  SpaceFile *sfile = CTX_wm_space_file(C);
  if (sfile && ED_fileselect_is_asset_browser(sfile)) {
    return false;
  }
  return WM_drag_is_ID_type(drag, ID_SCE);
}

static void screen_drop_scene_copy(bContext *C, wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_get_local_ID_or_import_from_asset(C, drag, ID_SCE);
  BLI_assert(id);
  RNA_int_set(drop->ptr, "session_uid", int(id->session_uid));
}

static std::string screen_drop_scene_tooltip(bContext * /*C*/,
                                             wmDrag *drag,
                                             const int /*xy*/
                                                 [2],
                                             wmDropBox * /*drop*/)
{
  const char *dragged_scene_name = WM_drag_get_item_name(drag);
  wmDragAsset *asset_drag = WM_drag_get_asset_data(drag, ID_SCE);
  if (asset_drag) {
    switch (asset_drag->import_settings.method) {
      case ASSET_IMPORT_LINK:
        return fmt::format(fmt::runtime(TIP_("Link {}")), dragged_scene_name);
      case ASSET_IMPORT_PACK:
        return fmt::format(fmt::runtime(TIP_("Pack {}")), dragged_scene_name);
      case ASSET_IMPORT_APPEND:
        return fmt::format(fmt::runtime(TIP_("Append {}")), dragged_scene_name);
      case ASSET_IMPORT_APPEND_REUSE:
        return fmt::format(fmt::runtime(TIP_("Append (Reuse) {}")), dragged_scene_name);
    }
  }
  return fmt::format(fmt::runtime(TIP_("Set {} as active")), dragged_scene_name);
}

void ED_keymap_screen(wmKeyConfig *keyconf)
{
  /* Screen Editing ------------------------------------------------ */
  WM_keymap_ensure(keyconf, "Screen Editing", SPACE_EMPTY, RGN_TYPE_WINDOW);

  /* Screen General ------------------------------------------------ */
  WM_keymap_ensure(keyconf, "Screen", SPACE_EMPTY, RGN_TYPE_WINDOW);

  /* Anim Playback ------------------------------------------------ */
  WM_keymap_ensure(keyconf, "Frames", SPACE_EMPTY, RGN_TYPE_WINDOW);

  /* dropbox for entire window */
  ListBase *lb = WM_dropboxmap_find("Window", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_dropbox_add(
      lb, "WM_OT_drop_blend_file", blend_file_drop_poll, blend_file_drop_copy, nullptr, nullptr);
  WM_dropbox_add(lb, "UI_OT_drop_color", UI_drop_color_poll, UI_drop_color_copy, nullptr, nullptr);
  WM_dropbox_add(lb,
                 "SCENE_OT_drop_scene_asset",
                 screen_drop_scene_poll,
                 screen_drop_scene_copy,
                 WM_drag_free_imported_drag_ID,
                 screen_drop_scene_tooltip);

  keymap_modal_set(keyconf);
}

/** \} */
