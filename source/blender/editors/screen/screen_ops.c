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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edscr
 */

#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_dlrbTree.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mask_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_workspace_types.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mask.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_sound.h"
#include "BKE_workspace.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_clip.h"
#include "ED_image.h"
#include "ED_keyframes_draw.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_screen_types.h"
#include "ED_sequencer.h"
#include "ED_undo.h"
#include "ED_util.h"
#include "ED_view3d.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "screen_intern.h" /* own module include */

#define KM_MODAL_CANCEL 1
#define KM_MODAL_APPLY 2
#define KM_MODAL_SNAP_ON 3
#define KM_MODAL_SNAP_OFF 4

/* -------------------------------------------------------------------- */
/** \name Public Poll API
 * \{ */

bool ED_operator_regionactive(bContext *C)
{
  if (CTX_wm_window(C) == NULL) {
    return 0;
  }
  if (CTX_wm_screen(C) == NULL) {
    return 0;
  }
  if (CTX_wm_region(C) == NULL) {
    return 0;
  }
  return 1;
}

bool ED_operator_areaactive(bContext *C)
{
  if (CTX_wm_window(C) == NULL) {
    return 0;
  }
  if (CTX_wm_screen(C) == NULL) {
    return 0;
  }
  if (CTX_wm_area(C) == NULL) {
    return 0;
  }
  return 1;
}

bool ED_operator_screenactive(bContext *C)
{
  if (CTX_wm_window(C) == NULL) {
    return 0;
  }
  if (CTX_wm_screen(C) == NULL) {
    return 0;
  }
  return 1;
}

/* XXX added this to prevent anim state to change during renders */
static bool ED_operator_screenactive_norender(bContext *C)
{
  if (G.is_rendering) {
    return 0;
  }
  if (CTX_wm_window(C) == NULL) {
    return 0;
  }
  if (CTX_wm_screen(C) == NULL) {
    return 0;
  }
  return 1;
}

/* when mouse is over area-edge */
bool ED_operator_screen_mainwinactive(bContext *C)
{
  bScreen *screen;
  if (CTX_wm_window(C) == NULL) {
    return 0;
  }
  screen = CTX_wm_screen(C);
  if (screen == NULL) {
    return 0;
  }
  if (screen->active_region != NULL) {
    return 0;
  }
  return 1;
}

bool ED_operator_scene(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  if (scene) {
    return 1;
  }
  return 0;
}

bool ED_operator_scene_editable(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  if (scene && !ID_IS_LINKED(scene)) {
    return true;
  }
  return false;
}

bool ED_operator_objectmode(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  Object *obact = CTX_data_active_object(C);

  if (scene == NULL || ID_IS_LINKED(scene)) {
    return 0;
  }
  if (CTX_data_edit_object(C)) {
    return 0;
  }

  /* add a check for ob->mode too? */
  if (obact && (obact->mode != OB_MODE_OBJECT)) {
    return 0;
  }

  return 1;
}

static bool ed_spacetype_test(bContext *C, int type)
{
  if (ED_operator_areaactive(C)) {
    SpaceLink *sl = (SpaceLink *)CTX_wm_space_data(C);
    return sl && (sl->spacetype == type);
  }
  return 0;
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

/* generic for any view2d which uses anim_ops */
bool ED_operator_animview_active(bContext *C)
{
  if (ED_operator_areaactive(C)) {
    SpaceLink *sl = (SpaceLink *)CTX_wm_space_data(C);
    if (sl && (ELEM(sl->spacetype, SPACE_SEQ, SPACE_ACTION, SPACE_NLA, SPACE_GRAPH))) {
      return true;
    }
  }

  CTX_wm_operator_poll_msg_set(C, "expected a timeline/animation area to be active");
  return 0;
}

bool ED_operator_outliner_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_OUTLINER);
}

bool ED_operator_outliner_active_no_editobject(bContext *C)
{
  if (ed_spacetype_test(C, SPACE_OUTLINER)) {
    Object *ob = ED_object_active_context(C);
    Object *obedit = CTX_data_edit_object(C);
    if (ob && ob == obedit) {
      return 0;
    }
    else {
      return 1;
    }
  }
  return 0;
}

bool ED_operator_file_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_FILE);
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
    return 1;
  }

  return 0;
}

bool ED_operator_node_editable(bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  if (snode && snode->edittree && !ID_IS_LINKED(snode->edittree)) {
    return 1;
  }

  return 0;
}

bool ED_operator_graphedit_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_GRAPH);
}

bool ED_operator_sequencer_active(bContext *C)
{
  return ed_spacetype_test(C, SPACE_SEQ);
}

bool ED_operator_sequencer_active_editable(bContext *C)
{
  return ed_spacetype_test(C, SPACE_SEQ) && ED_operator_scene_editable(C);
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
  return ((ob->restrictflag & OB_RESTRICT_VIEWPORT) && !(ob->mode & OB_MODE_EDIT));
}

bool ED_operator_object_active(bContext *C)
{
  Object *ob = ED_object_active_context(C);
  return ((ob != NULL) && !ed_object_hidden(ob));
}

bool ED_operator_object_active_editable_ex(bContext *UNUSED(C), const Object *ob)
{
  return ((ob != NULL) && !ID_IS_LINKED(ob) && !ed_object_hidden(ob));
}

bool ED_operator_object_active_editable(bContext *C)
{
  Object *ob = ED_object_active_context(C);
  return ED_operator_object_active_editable_ex(C, ob);
}

bool ED_operator_object_active_editable_mesh(bContext *C)
{
  Object *ob = ED_object_active_context(C);
  return ((ob != NULL) && !ID_IS_LINKED(ob) && !ed_object_hidden(ob) && (ob->type == OB_MESH) &&
          !ID_IS_LINKED(ob->data));
}

bool ED_operator_object_active_editable_font(bContext *C)
{
  Object *ob = ED_object_active_context(C);
  return ((ob != NULL) && !ID_IS_LINKED(ob) && !ed_object_hidden(ob) && (ob->type == OB_FONT));
}

bool ED_operator_editable_mesh(bContext *C)
{
  Mesh *mesh = ED_mesh_context(C);
  return (mesh != NULL) && !ID_IS_LINKED(mesh);
}

bool ED_operator_editmesh(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_MESH) {
    return NULL != BKE_editmesh_from_object(obedit);
  }
  return 0;
}

bool ED_operator_editmesh_view3d(bContext *C)
{
  return ED_operator_editmesh(C) && ED_operator_view3d_active(C);
}

bool ED_operator_editmesh_region_view3d(bContext *C)
{
  if (ED_operator_editmesh(C) && CTX_wm_region_view3d(C)) {
    return 1;
  }

  CTX_wm_operator_poll_msg_set(C, "expected a view3d region & editmesh");
  return 0;
}

bool ED_operator_editmesh_auto_smooth(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_MESH && (((Mesh *)(obedit->data))->flag & ME_AUTOSMOOTH)) {
    return NULL != BKE_editmesh_from_object(obedit);
  }
  return 0;
}

bool ED_operator_editarmature(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_ARMATURE) {
    return NULL != ((bArmature *)obedit->data)->edbo;
  }
  return 0;
}

/**
 * \brief check for pose mode (no mixed modes)
 *
 * We want to enable most pose operations in weight paint mode,
 * when it comes to transforming bones, but managing bones layers/groups
 * can be left for pose mode only. (not weight paint mode)
 */
bool ED_operator_posemode_exclusive(bContext *C)
{
  Object *obact = CTX_data_active_object(C);

  if (obact && !(obact->mode & OB_MODE_EDIT)) {
    Object *obpose;
    if ((obpose = BKE_object_pose_armature_get(obact))) {
      if (obact == obpose) {
        return 1;
      }
    }
  }

  return 0;
}

/* allows for pinned pose objects to be used in the object buttons
 * and the non-active pose object to be used in the 3D view */
bool ED_operator_posemode_context(bContext *C)
{
  Object *obpose = ED_pose_object_from_context(C);

  if (obpose && !(obpose->mode & OB_MODE_EDIT)) {
    if (BKE_object_pose_context_check(obpose)) {
      return 1;
    }
  }

  return 0;
}

bool ED_operator_posemode(bContext *C)
{
  Object *obact = CTX_data_active_object(C);

  if (obact && !(obact->mode & OB_MODE_EDIT)) {
    Object *obpose;
    if ((obpose = BKE_object_pose_armature_get(obact))) {
      if ((obact == obpose) || (obact->mode & OB_MODE_WEIGHT_PAINT)) {
        return 1;
      }
    }
  }

  return 0;
}

bool ED_operator_posemode_local(bContext *C)
{
  if (ED_operator_posemode(C)) {
    Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
    bArmature *arm = ob->data;
    return !(ID_IS_LINKED(&ob->id) || ID_IS_LINKED(&arm->id));
  }
  return false;
}

/* wrapper for ED_space_image_show_uvedit */
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
  BMEditMesh *em = NULL;

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
  if (obedit && ELEM(obedit->type, OB_CURVE, OB_SURF)) {
    return NULL != ((Curve *)obedit->data)->editnurb;
  }
  return 0;
}

bool ED_operator_editsurfcurve_region_view3d(bContext *C)
{
  if (ED_operator_editsurfcurve(C) && CTX_wm_region_view3d(C)) {
    return 1;
  }

  CTX_wm_operator_poll_msg_set(C, "expected a view3d region & editcurve");
  return 0;
}

bool ED_operator_editcurve(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_CURVE) {
    return NULL != ((Curve *)obedit->data)->editnurb;
  }
  return 0;
}

bool ED_operator_editcurve_3d(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_CURVE) {
    Curve *cu = (Curve *)obedit->data;

    return (cu->flag & CU_3D) && (NULL != cu->editnurb);
  }
  return 0;
}

bool ED_operator_editsurf(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_SURF) {
    return NULL != ((Curve *)obedit->data)->editnurb;
  }
  return 0;
}

bool ED_operator_editfont(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_FONT) {
    return NULL != ((Curve *)obedit->data)->editfont;
  }
  return 0;
}

bool ED_operator_editlattice(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_LATTICE) {
    return NULL != ((Lattice *)obedit->data)->editlatt;
  }
  return 0;
}

bool ED_operator_editmball(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_MBALL) {
    return NULL != ((MetaBall *)obedit->data)->editelems;
  }
  return 0;
}

bool ED_operator_mask(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacedata.first) {
    switch (area->spacetype) {
      case SPACE_CLIP: {
        SpaceClip *screen = area->spacedata.first;
        return ED_space_clip_check_show_maskedit(screen);
      }
      case SPACE_SEQ: {
        SpaceSeq *sseq = area->spacedata.first;
        Scene *scene = CTX_data_scene(C);
        return ED_space_sequencer_check_show_maskedit(sseq, scene);
      }
      case SPACE_IMAGE: {
        SpaceImage *sima = area->spacedata.first;
        ViewLayer *view_layer = CTX_data_view_layer(C);
        return ED_space_image_check_show_maskedit(sima, view_layer);
      }
    }
  }

  return false;
}

bool ED_operator_camera(bContext *C)
{
  struct Camera *cam = CTX_data_pointer_get_type(C, "camera", &RNA_Camera).data;
  return (cam != NULL);
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
      return 0;
    }
    return 1;
  }
  return 0;
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

typedef struct sActionzoneData {
  ScrArea *sa1, *sa2;
  AZone *az;
  int x, y, gesture_dir, modifier;
} sActionzoneData;

/* quick poll to save operators to be created and handled */
static bool actionzone_area_poll(bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = WM_window_get_active_screen(win);

  if (screen && win && win->eventstate) {
    const int *xy = &win->eventstate->x;
    AZone *az;

    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      for (az = area->actionzones.first; az; az = az->next) {
        if (BLI_rcti_isect_pt_v(&az->rect, xy)) {
          return 1;
        }
      }
    }
  }
  return 0;
}

/* the debug drawing of the click_rect is in area_draw_azone_fullscreen, keep both in sync */
static void fullscreen_click_rcti_init(
    rcti *rect, const short UNUSED(x1), const short UNUSED(y1), const short x2, const short y2)
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
        ((az->region->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL)) == 0)) {
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

static AZone *area_actionzone_refresh_xy(ScrArea *area, const int xy[2], const bool test_only)
{
  AZone *az = NULL;

  for (az = area->actionzones.first; az; az = az->next) {
    rcti az_rect_clip;
    if (BLI_rcti_isect_pt_v(&az->rect, xy) &&
        /* Check clipping if this is clipped */
        (!azone_clipped_rect_calc(az, &az_rect_clip) || BLI_rcti_isect_pt_v(&az_rect_clip, xy))) {

      if (az->type == AZONE_AREA) {
        break;
      }
      else if (az->type == AZONE_REGION) {
        break;
      }
      else if (az->type == AZONE_FULLSCREEN) {
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
            const int spot_sq = square_i(AZONESPOTW);
            const int fadein_sq = square_i(AZONEFADEIN);
            const int fadeout_sq = square_i(AZONEFADEOUT);

            if (mouse_sq < spot_sq) {
              az->alpha = 1.0f;
            }
            else if (mouse_sq < fadein_sq) {
              az->alpha = 1.0f;
            }
            else if (mouse_sq < fadeout_sq) {
              az->alpha = 1.0f -
                          ((float)(mouse_sq - fadein_sq)) / ((float)(fadeout_sq - fadein_sq));
            }
            else {
              az->alpha = 0.0f;
            }

            /* fade in/out but no click */
            az = NULL;
          }

          /* XXX force redraw to show/hide the action zone */
          ED_area_tag_redraw(area);
          break;
        }
      }
      else if (az->type == AZONE_REGION_SCROLL) {
        ARegion *region = az->region;
        View2D *v2d = &region->v2d;
        int scroll_flag = 0;
        const int isect_value = UI_view2d_mouse_in_scrollers_ex(
            region, v2d, xy[0], xy[1], &scroll_flag);

        /* Check if we even have scroll bars. */
        if (((az->direction == AZ_SCROLL_HOR) && !(scroll_flag & V2D_SCROLL_HORIZONTAL)) ||
            ((az->direction == AZ_SCROLL_VERT) && !(scroll_flag & V2D_SCROLL_VERTICAL))) {
          /* no scrollbars, do nothing. */
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
              dist_fac = BLI_rcti_length_y(&v2d->hor, local_xy[1]) / AZONEFADEIN;
              CLAMP(dist_fac, 0.0f, 1.0f);
              alpha = 1.0f - dist_fac;

              v2d->alpha_hor = alpha * 255;
            }
            else if (az->direction == AZ_SCROLL_VERT) {
              dist_fac = BLI_rcti_length_x(&v2d->vert, local_xy[0]) / AZONEFADEIN;
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
      else if (az->type == AZONE_REGION_SCROLL) {
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
          BLI_assert(0);
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
    if (az != NULL) {
      return az;
    }
  }
  return NULL;
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
  return NULL;
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
  if (op->customdata) {
    MEM_freeN(op->customdata);
  }
  op->customdata = NULL;

  G.moving &= ~G_TRANSFORM_WM;
}

/* send EVT_ACTIONZONE event */
static void actionzone_apply(bContext *C, wmOperator *op, int type)
{
  wmEvent event;
  wmWindow *win = CTX_wm_window(C);
  sActionzoneData *sad = op->customdata;

  sad->modifier = RNA_int_get(op->ptr, "modifier");

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
  event.customdata = op->customdata;
  event.customdatafree = true;
  op->customdata = NULL;

  wm_event_add(win, &event);
}

static int actionzone_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bScreen *screen = CTX_wm_screen(C);
  AZone *az = screen_actionzone_find_xy(screen, &event->x);
  sActionzoneData *sad;

  /* Quick escape - Scroll azones only hide/unhide the scroll-bars,
   * they have their own handling. */
  if (az == NULL || ELEM(az->type, AZONE_REGION_SCROLL)) {
    return OPERATOR_PASS_THROUGH;
  }

  /* ok we do the action-zone */
  sad = op->customdata = MEM_callocN(sizeof(sActionzoneData), "sActionzoneData");
  sad->sa1 = screen_actionzone_area(screen, az);
  sad->az = az;
  sad->x = event->x;
  sad->y = event->y;

  /* region azone directly reacts on mouse clicks */
  if (ELEM(sad->az->type, AZONE_REGION, AZONE_FULLSCREEN)) {
    actionzone_apply(C, op, sad->az->type);
    actionzone_exit(op);
    return OPERATOR_FINISHED;
  }
  else {
    BLI_assert(ELEM(sad->az->type, AZONE_AREA, AZONE_REGION_SCROLL));

    /* add modal handler */
    G.moving |= G_TRANSFORM_WM;
    WM_event_add_modal_handler(C, op);
    return OPERATOR_RUNNING_MODAL;
  }
}

static int actionzone_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  bScreen *screen = CTX_wm_screen(C);
  sActionzoneData *sad = op->customdata;

  switch (event->type) {
    case MOUSEMOVE: {
      bool is_gesture;

      const int delta_x = (event->x - sad->x);
      const int delta_y = (event->y - sad->y);

      /* Movement in dominant direction. */
      const int delta_max = max_ii(abs(delta_x), abs(delta_y));

      /* Movement in dominant direction before action taken. */
      const int join_threshold = (0.6 * U.widget_unit);
      const int split_threshold = (1.2 * U.widget_unit);
      const int area_threshold = (0.1 * U.widget_unit);

      /* Calculate gesture cardinal direction. */
      if (delta_y > abs(delta_x)) {
        sad->gesture_dir = 'n';
      }
      else if (delta_x >= abs(delta_y)) {
        sad->gesture_dir = 'e';
      }
      else if (delta_y < -abs(delta_x)) {
        sad->gesture_dir = 's';
      }
      else {
        sad->gesture_dir = 'w';
      }

      if (sad->az->type == AZONE_AREA) {
        wmWindow *win = CTX_wm_window(C);
        rcti screen_rect;

        WM_window_screen_rect_calc(win, &screen_rect);

        /* Have we dragged off the zone and are not on an edge? */
        if ((ED_area_actionzone_find_xy(sad->sa1, &event->x) != sad->az) &&
            (screen_geom_area_map_find_active_scredge(
                 AREAMAP_FROM_SCREEN(screen), &screen_rect, event->x, event->y) == NULL)) {
          /* Are we still in same area? */
          if (BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, event->x, event->y) == sad->sa1) {
            /* Same area, so possible split. */
            WM_cursor_set(
                win, (ELEM(sad->gesture_dir, 'n', 's')) ? WM_CURSOR_H_SPLIT : WM_CURSOR_V_SPLIT);
            is_gesture = (delta_max > split_threshold);
          }
          else {
            /* Different area, so possible join. */
            if (sad->gesture_dir == 'n') {
              WM_cursor_set(win, WM_CURSOR_N_ARROW);
            }
            else if (sad->gesture_dir == 's') {
              WM_cursor_set(win, WM_CURSOR_S_ARROW);
            }
            else if (sad->gesture_dir == 'e') {
              WM_cursor_set(win, WM_CURSOR_E_ARROW);
            }
            else {
              WM_cursor_set(win, WM_CURSOR_W_ARROW);
            }
            is_gesture = (delta_max > join_threshold);
          }
        }
        else {
          WM_cursor_set(CTX_wm_window(C), WM_CURSOR_CROSS);
          is_gesture = false;
        }
      }
      else {
        is_gesture = (delta_max > area_threshold);
      }

      /* gesture is large enough? */
      if (is_gesture) {
        /* second area, for join when (sa1 != sa2) */
        sad->sa2 = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, event->x, event->y);
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
  }

  return OPERATOR_RUNNING_MODAL;
}

static void actionzone_cancel(bContext *UNUSED(C), wmOperator *op)
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
  ScrEdge *actedge;
  rcti window_rect;
  WM_window_rect_calc(win, &window_rect);
  actedge = screen_geom_area_map_find_active_scredge(
      AREAMAP_FROM_SCREEN(screen), &window_rect, cursor[0], cursor[1]);
  *r_sa1 = NULL;
  *r_sa2 = NULL;
  if (actedge == NULL) {
    return NULL;
  }
  int borderwidth = (4 * UI_DPI_FAC);
  ScrArea *sa1, *sa2;
  if (screen_geom_edge_is_horizontal(actedge)) {
    sa1 = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, cursor[0], cursor[1] + borderwidth);
    sa2 = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, cursor[0], cursor[1] - borderwidth);
  }
  else {
    sa1 = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, cursor[0] + borderwidth, cursor[1]);
    sa2 = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, cursor[0] - borderwidth, cursor[1]);
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
 * invoke() gets called on shift+lmb drag in action-zone
 * exec()   execute without any user interaction, based on properties
 * call init(), add handler
 *
 * modal()  accept modal events while doing it
 */

typedef struct sAreaSwapData {
  ScrArea *sa1, *sa2;
} sAreaSwapData;

static int area_swap_init(wmOperator *op, const wmEvent *event)
{
  sAreaSwapData *sd = NULL;
  sActionzoneData *sad = event->customdata;

  if (sad == NULL || sad->sa1 == NULL) {
    return 0;
  }

  sd = MEM_callocN(sizeof(sAreaSwapData), "sAreaSwapData");
  sd->sa1 = sad->sa1;
  sd->sa2 = sad->sa2;
  op->customdata = sd;

  return 1;
}

static void area_swap_exit(bContext *C, wmOperator *op)
{
  WM_cursor_modal_restore(CTX_wm_window(C));
  if (op->customdata) {
    MEM_freeN(op->customdata);
  }
  op->customdata = NULL;
}

static void area_swap_cancel(bContext *C, wmOperator *op)
{
  area_swap_exit(C, op);
}

static int area_swap_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{

  if (!area_swap_init(op, event)) {
    return OPERATOR_PASS_THROUGH;
  }

  /* add modal handler */
  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_SWAP_AREA);
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int area_swap_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  sActionzoneData *sad = op->customdata;

  switch (event->type) {
    case MOUSEMOVE:
      /* second area, for join */
      sad->sa2 = BKE_screen_find_area_xy(CTX_wm_screen(C), SPACE_TYPE_ANY, event->x, event->y);
      break;
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

        WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);

        return OPERATOR_FINISHED;
      }
      break;

    case EVT_ESCKEY:
      area_swap_cancel(C, op);
      return OPERATOR_CANCELLED;
  }
  return OPERATOR_RUNNING_MODAL;
}

static int area_swap_exec(bContext *C, wmOperator *op)
{
  ScrArea *sa1, *sa2;
  int cursor[2];
  RNA_int_get_array(op->ptr, "cursor", cursor);
  screen_area_edge_from_cursor(C, cursor, &sa1, &sa2);
  if (sa1 == NULL || sa2 == NULL) {
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
      ot->srna, "cursor", 2, NULL, INT_MIN, INT_MAX, "Cursor", "", INT_MIN, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Area Duplicate Operator
 *
 * Create new window from area.
 * \{ */

/* operator callback */
static int area_dupli_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  wmWindow *newwin, *win = CTX_wm_window(C);
  Scene *scene;
  WorkSpace *workspace = WM_window_get_active_workspace(win);
  WorkSpaceLayout *layout_old = WM_window_get_active_layout(win);
  WorkSpaceLayout *layout_new;
  bScreen *newsc;
  ScrArea *area;
  rcti rect;

  win = CTX_wm_window(C);
  scene = CTX_data_scene(C);
  area = CTX_wm_area(C);

  /* XXX hrmf! */
  if (event->type == EVT_ACTIONZONE_AREA) {
    sActionzoneData *sad = event->customdata;

    if (sad == NULL) {
      return OPERATOR_PASS_THROUGH;
    }

    area = sad->sa1;
  }

  /* adds window to WM */
  rect = area->totrct;
  BLI_rcti_translate(&rect, win->posx, win->posy);
  rect.xmax = rect.xmin + BLI_rcti_size_x(&rect) / U.pixelsize;
  rect.ymax = rect.ymin + BLI_rcti_size_y(&rect) / U.pixelsize;

  newwin = WM_window_open(C, &rect);
  if (newwin == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Failed to open window!");
    goto finally;
  }

  *newwin->stereo3d_format = *win->stereo3d_format;

  newwin->scene = scene;

  STRNCPY(newwin->view_layer_name, win->view_layer_name);

  BKE_workspace_active_set(newwin->workspace_hook, workspace);
  /* allocs new screen and adds to newly created window, using window size */
  layout_new = ED_workspace_layout_add(
      bmain, workspace, newwin, BKE_workspace_layout_name_get(layout_old));
  newsc = BKE_workspace_layout_screen_get(layout_new);
  WM_window_set_active_layout(newwin, workspace, layout_new);

  /* copy area to new screen */
  ED_area_data_copy((ScrArea *)newsc->areabase.first, area, true);

  ED_area_tag_redraw((ScrArea *)newsc->areabase.first);

  /* screen, areas init */
  WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);

finally:
  if (event->type == EVT_ACTIONZONE_AREA) {
    actionzone_exit(op);
  }

  if (newwin) {
    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
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

typedef struct sAreaMoveData {
  int bigger, smaller, origval, step;
  char dir;
  enum AreaMoveSnapType {
    /* Snapping disabled */
    SNAP_NONE = 0,
    /* Snap to an invisible grid with a unit defined in AREAGRID */
    SNAP_AREAGRID,
    /* Snap to fraction (half, third.. etc) and adjacent edges. */
    SNAP_FRACTION_AND_ADJACENT,
    /* Snap to either bigger or smaller, nothing in-between (used for
     * global areas). This has priority over other snap types, if it is
     * used, toggling SNAP_FRACTION_AND_ADJACENT doesn't work. */
    SNAP_BIGGER_SMALLER_ONLY,
  } snap_type;
} sAreaMoveData;

/* helper call to move area-edge, sets limits
 * need window bounds in order to get correct limits */
static void area_move_set_limits(wmWindow *win,
                                 bScreen *screen,
                                 int dir,
                                 int *bigger,
                                 int *smaller,
                                 bool *use_bigger_smaller_snap)
{
  rcti window_rect;
  int areaminy = ED_area_headersize();
  int areamin;

  /* we check all areas and test for free space with MINSIZE */
  *bigger = *smaller = 100000;

  if (use_bigger_smaller_snap != NULL) {
    *use_bigger_smaller_snap = false;
    LISTBASE_FOREACH (ScrArea *, area, &win->global_areas.areabase) {
      int size_min = ED_area_global_min_size_y(area) - 1;
      int size_max = ED_area_global_max_size_y(area) - 1;

      size_min = max_ii(size_min, 0);
      BLI_assert(size_min <= size_max);

      /* logic here is only tested for lower edge :) */
      /* left edge */
      if ((area->v1->editflag && area->v2->editflag)) {
        *smaller = area->v4->vec.x - size_max;
        *bigger = area->v4->vec.x - size_min;
        *use_bigger_smaller_snap = true;
        return;
      }
      /* top edge */
      else if ((area->v2->editflag && area->v3->editflag)) {
        *smaller = area->v1->vec.y + size_min;
        *bigger = area->v1->vec.y + size_max;
        *use_bigger_smaller_snap = true;
        return;
      }
      /* right edge */
      else if ((area->v3->editflag && area->v4->editflag)) {
        *smaller = area->v1->vec.x + size_min;
        *bigger = area->v1->vec.x + size_max;
        *use_bigger_smaller_snap = true;
        return;
      }
      /* lower edge */
      else if ((area->v4->editflag && area->v1->editflag)) {
        *smaller = area->v2->vec.y - size_max;
        *bigger = area->v2->vec.y - size_min;
        *use_bigger_smaller_snap = true;
        return;
      }
    }
  }

  WM_window_rect_calc(win, &window_rect);

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    if (dir == 'h') {
      int y1;
      areamin = areaminy;

      if (area->v1->vec.y > window_rect.ymin) {
        areamin += U.pixelsize;
      }
      if (area->v2->vec.y < (window_rect.ymax - 1)) {
        areamin += U.pixelsize;
      }

      y1 = screen_geom_area_height(area) - areamin;

      /* if top or down edge selected, test height */
      if (area->v1->editflag && area->v4->editflag) {
        *bigger = min_ii(*bigger, y1);
      }
      else if (area->v2->editflag && area->v3->editflag) {
        *smaller = min_ii(*smaller, y1);
      }
    }
    else {
      int x1;
      areamin = AREAMINX;

      if (area->v1->vec.x > window_rect.xmin) {
        areamin += U.pixelsize;
      }
      if (area->v4->vec.x < (window_rect.xmax - 1)) {
        areamin += U.pixelsize;
      }

      x1 = screen_geom_area_width(area) - areamin;

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

/* validate selection inside screen, set variables OK */
/* return 0: init failed */
static int area_move_init(bContext *C, wmOperator *op)
{
  bScreen *screen = CTX_wm_screen(C);
  wmWindow *win = CTX_wm_window(C);
  ScrEdge *actedge;
  sAreaMoveData *md;
  int x, y;

  /* required properties */
  x = RNA_int_get(op->ptr, "x");
  y = RNA_int_get(op->ptr, "y");

  /* setup */
  actedge = screen_geom_find_active_scredge(win, screen, x, y);
  if (actedge == NULL) {
    return 0;
  }

  md = MEM_callocN(sizeof(sAreaMoveData), "sAreaMoveData");
  op->customdata = md;

  md->dir = screen_geom_edge_is_horizontal(actedge) ? 'h' : 'v';
  if (md->dir == 'h') {
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
  area_move_set_limits(win, screen, md->dir, &md->bigger, &md->smaller, &use_bigger_smaller_snap);

  md->snap_type = use_bigger_smaller_snap ? SNAP_BIGGER_SMALLER_ONLY : SNAP_AREAGRID;

  return 1;
}

static int area_snap_calc_location(const bScreen *screen,
                                   const enum AreaMoveSnapType snap_type,
                                   const int delta,
                                   const int origval,
                                   const int dir,
                                   const int bigger,
                                   const int smaller)
{
  BLI_assert(snap_type != SNAP_NONE);
  int m_cursor_final = -1;
  const int m_cursor = origval + delta;
  const int m_span = (float)(bigger + smaller);
  const int m_min = origval - smaller;
  // const int axis_max = axis_min + m_span;

  switch (snap_type) {
    case SNAP_AREAGRID:
      m_cursor_final = m_cursor;
      if (delta != bigger && delta != -smaller) {
        m_cursor_final -= (m_cursor % AREAGRID);
        CLAMP(m_cursor_final, origval - smaller, origval + bigger);
      }
      break;

    case SNAP_BIGGER_SMALLER_ONLY:
      m_cursor_final = (m_cursor >= bigger) ? bigger : smaller;
      break;

    case SNAP_FRACTION_AND_ADJACENT: {
      const int axis = (dir == 'v') ? 0 : 1;
      int snap_dist_best = INT_MAX;
      {
        const float div_array[] = {
            /* Middle. */
            1.0f / 2.0f,
            /* Thirds. */
            1.0f / 3.0f,
            2.0f / 3.0f,
            /* Quaters. */
            1.0f / 4.0f,
            3.0f / 4.0f,
            /* Eighth. */
            1.0f / 8.0f,
            3.0f / 8.0f,
            5.0f / 8.0f,
            7.0f / 8.0f,
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
static void area_move_apply_do(const bContext *C,
                               int delta,
                               const int origval,
                               const int dir,
                               const int bigger,
                               const int smaller,
                               const enum AreaMoveSnapType snap_type)
{
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
    final_loc = area_snap_calc_location(screen, snap_type, delta, origval, dir, bigger, smaller);
  }

  BLI_assert(final_loc != -1);
  short axis = (dir == 'v') ? 0 : 1;

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
          int height = round_fl_to_int(screen_geom_area_height(area) / UI_DPI_FAC);
          if (abs(height - area->global->size_min) < abs(height - area->global->size_max)) {
            area->global->cur_fixed_height = area->global->size_min;
          }
          else {
            area->global->cur_fixed_height = area->global->size_max;
          }

          screen->do_refresh = true;
          redraw_all = true;
        }
        ED_area_tag_redraw(area);
      }
    }
    if (redraw_all) {
      ED_screen_areas_iter (win, screen, area) {
        ED_area_tag_redraw(area);
      }
    }

    ED_screen_global_areas_sync(win);

    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL); /* redraw everything */
    /* Update preview thumbnail */
    BKE_icon_changed(screen->id.icon_id);
  }
}

static void area_move_apply(bContext *C, wmOperator *op)
{
  sAreaMoveData *md = op->customdata;
  int delta = RNA_int_get(op->ptr, "delta");

  area_move_apply_do(C, delta, md->origval, md->dir, md->bigger, md->smaller, md->snap_type);
}

static void area_move_exit(bContext *C, wmOperator *op)
{
  if (op->customdata) {
    MEM_freeN(op->customdata);
  }
  op->customdata = NULL;

  /* this makes sure aligned edges will result in aligned grabbing */
  BKE_screen_remove_double_scrverts(CTX_wm_screen(C));
  BKE_screen_remove_double_scredges(CTX_wm_screen(C));

  G.moving &= ~G_TRANSFORM_WM;
}

static int area_move_exec(bContext *C, wmOperator *op)
{
  if (!area_move_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  area_move_apply(C, op);
  area_move_exit(C, op);

  return OPERATOR_FINISHED;
}

/* interaction callback */
static int area_move_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RNA_int_set(op->ptr, "x", event->x);
  RNA_int_set(op->ptr, "y", event->y);

  if (!area_move_init(C, op)) {
    return OPERATOR_PASS_THROUGH;
  }

  /* add temp handler */
  G.moving |= G_TRANSFORM_WM;
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
static int area_move_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  sAreaMoveData *md = op->customdata;
  int delta, x, y;

  /* execute the events */
  switch (event->type) {
    case MOUSEMOVE: {
      x = RNA_int_get(op->ptr, "x");
      y = RNA_int_get(op->ptr, "y");

      delta = (md->dir == 'v') ? event->x - x : event->y - y;
      RNA_int_set(op->ptr, "delta", delta);

      area_move_apply(C, op);
      break;
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
      break;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static void SCREEN_OT_area_move(wmOperatorType *ot)
{
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
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Split Area Operator
 * \{ */

/*
 * operator state vars:
 * fac              spit point
 * dir              direction 'v' or 'h'
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

typedef struct sAreaSplitData {
  int origval;           /* for move areas */
  int bigger, smaller;   /* constraints for moving new edge */
  int delta;             /* delta move edge */
  int origmin, origsize; /* to calculate fac, for property storage */
  int previewmode;       /* draw previewline, then split */
  void *draw_callback;   /* call `ED_screen_draw_split_preview` */
  bool do_snap;

  ScrEdge *nedge; /* new edge */
  ScrArea *sarea; /* start area */
  ScrArea *narea; /* new area */

} sAreaSplitData;

static void area_split_draw_cb(const struct wmWindow *UNUSED(win), void *userdata)
{
  const wmOperator *op = userdata;

  sAreaSplitData *sd = op->customdata;
  if (sd->sarea) {
    int dir = RNA_enum_get(op->ptr, "direction");
    float fac = RNA_float_get(op->ptr, "factor");

    ED_screen_draw_split_preview(sd->sarea, dir, fac);
  }
}

/* generic init, menu case, doesn't need active area */
static int area_split_menu_init(bContext *C, wmOperator *op)
{
  sAreaSplitData *sd;

  /* custom data */
  sd = (sAreaSplitData *)MEM_callocN(sizeof(sAreaSplitData), "op_area_split");
  op->customdata = sd;

  sd->sarea = CTX_wm_area(C);

  return 1;
}

/* generic init, no UI stuff here, assumes active area */
static int area_split_init(bContext *C, wmOperator *op)
{
  ScrArea *area = CTX_wm_area(C);
  sAreaSplitData *sd;
  int areaminy = ED_area_headersize();
  int dir;

  /* required context */
  if (area == NULL) {
    return 0;
  }

  /* required properties */
  dir = RNA_enum_get(op->ptr, "direction");

  /* minimal size */
  if (dir == 'v' && area->winx < 2 * AREAMINX) {
    return 0;
  }
  if (dir == 'h' && area->winy < 2 * areaminy) {
    return 0;
  }

  /* custom data */
  sd = (sAreaSplitData *)MEM_callocN(sizeof(sAreaSplitData), "op_area_split");
  op->customdata = sd;

  sd->sarea = area;
  if (dir == 'v') {
    sd->origmin = area->v1->vec.x;
    sd->origsize = area->v4->vec.x - sd->origmin;
  }
  else {
    sd->origmin = area->v1->vec.y;
    sd->origsize = area->v2->vec.y - sd->origmin;
  }

  return 1;
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

  if (sav1 == sbv4 && sav2 == sbv3) { /* area to right of sb = W */
    return BKE_screen_find_edge(screen, sav1, sav2);
  }
  else if (sav2 == sbv1 && sav3 == sbv4) { /* area to bottom of sb = N */
    return BKE_screen_find_edge(screen, sav2, sav3);
  }
  else if (sav3 == sbv2 && sav4 == sbv1) { /* area to left of sb = E */
    return BKE_screen_find_edge(screen, sav3, sav4);
  }
  else if (sav1 == sbv2 && sav4 == sbv3) { /* area on top of sb = S*/
    return BKE_screen_find_edge(screen, sav1, sav4);
  }

  return NULL;
}

/* do the split, return success */
static int area_split_apply(bContext *C, wmOperator *op)
{
  const wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);
  sAreaSplitData *sd = (sAreaSplitData *)op->customdata;
  float fac;
  int dir;

  fac = RNA_float_get(op->ptr, "factor");
  dir = RNA_enum_get(op->ptr, "direction");

  sd->narea = area_split(win, screen, sd->sarea, dir, fac, 0); /* 0 = no merge */

  if (sd->narea) {
    sd->nedge = area_findsharededge(screen, sd->sarea, sd->narea);

    /* select newly created edge, prepare for moving edge */
    ED_screen_verts_iter(win, screen, sv)
    {
      sv->editflag = 0;
    }

    sd->nedge->v1->editflag = 1;
    sd->nedge->v2->editflag = 1;

    if (dir == 'h') {
      sd->origval = sd->nedge->v1->vec.y;
    }
    else {
      sd->origval = sd->nedge->v1->vec.x;
    }

    ED_area_tag_redraw(sd->sarea);
    ED_area_tag_redraw(sd->narea);

    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);
    /* Update preview thumbnail */
    BKE_icon_changed(screen->id.icon_id);

    return 1;
  }

  return 0;
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

    MEM_freeN(op->customdata);
    op->customdata = NULL;
  }

  WM_cursor_modal_restore(CTX_wm_window(C));
  WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);

  /* this makes sure aligned edges will result in aligned grabbing */
  BKE_screen_remove_double_scrverts(CTX_wm_screen(C));
  BKE_screen_remove_double_scredges(CTX_wm_screen(C));

  G.moving &= ~G_TRANSFORM_WM;
}

static void area_split_preview_update_cursor(bContext *C, wmOperator *op)
{
  wmWindow *win = CTX_wm_window(C);
  int dir = RNA_enum_get(op->ptr, "direction");
  WM_cursor_set(win, dir == 'h' ? WM_CURSOR_H_SPLIT : WM_CURSOR_V_SPLIT);
}

/* UI callback, adds new handler */
static int area_split_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);
  sAreaSplitData *sd;
  int dir;

  /* no full window splitting allowed */
  BLI_assert(screen->state == SCREENNORMAL);

  PropertyRNA *prop_dir = RNA_struct_find_property(op->ptr, "direction");
  PropertyRNA *prop_factor = RNA_struct_find_property(op->ptr, "factor");
  PropertyRNA *prop_cursor = RNA_struct_find_property(op->ptr, "cursor");

  if (event->type == EVT_ACTIONZONE_AREA) {
    sActionzoneData *sad = event->customdata;

    if (sad == NULL || sad->modifier > 0) {
      return OPERATOR_PASS_THROUGH;
    }

    /* verify *sad itself */
    if (sad->sa1 == NULL || sad->az == NULL) {
      return OPERATOR_PASS_THROUGH;
    }

    /* is this our *sad? if areas not equal it should be passed on */
    if (CTX_wm_area(C) != sad->sa1 || sad->sa1 != sad->sa2) {
      return OPERATOR_PASS_THROUGH;
    }

    /* The factor will be close to 1.0f when near the top-left and the bottom-right corners. */
    const float factor_v = ((float)(event->y - sad->sa1->v1->vec.y)) / (float)sad->sa1->winy;
    const float factor_h = ((float)(event->x - sad->sa1->v1->vec.x)) / (float)sad->sa1->winx;
    const bool is_left = factor_v < 0.5f;
    const bool is_bottom = factor_h < 0.5f;
    const bool is_right = !is_left;
    const bool is_top = !is_bottom;
    float factor;

    /* Prepare operator state vars. */
    if (ELEM(sad->gesture_dir, 'n', 's')) {
      dir = 'h';
      factor = factor_h;
    }
    else {
      dir = 'v';
      factor = factor_v;
    }

    if ((is_top && is_left) || (is_bottom && is_right)) {
      factor = 1.0f - factor;
    }

    RNA_property_float_set(op->ptr, prop_factor, factor);

    RNA_property_enum_set(op->ptr, prop_dir, dir);

    /* general init, also non-UI case, adds customdata, sets area and defaults */
    if (!area_split_init(C, op)) {
      return OPERATOR_PASS_THROUGH;
    }
  }
  else if (RNA_property_is_set(op->ptr, prop_dir)) {
    ScrArea *area = CTX_wm_area(C);
    if (area == NULL) {
      return OPERATOR_CANCELLED;
    }
    dir = RNA_property_enum_get(op->ptr, prop_dir);
    if (dir == 'h') {
      RNA_property_float_set(
          op->ptr, prop_factor, ((float)(event->x - area->v1->vec.x)) / (float)area->winx);
    }
    else {
      RNA_property_float_set(
          op->ptr, prop_factor, ((float)(event->y - area->v1->vec.y)) / (float)area->winy);
    }

    if (!area_split_init(C, op)) {
      return OPERATOR_CANCELLED;
    }
  }
  else {
    ScrEdge *actedge;
    rcti window_rect;
    int event_co[2];

    /* retrieve initial mouse coord, so we can find the active edge */
    if (RNA_property_is_set(op->ptr, prop_cursor)) {
      RNA_property_int_get_array(op->ptr, prop_cursor, event_co);
    }
    else {
      copy_v2_v2_int(event_co, &event->x);
    }

    WM_window_rect_calc(win, &window_rect);

    actedge = screen_geom_area_map_find_active_scredge(
        AREAMAP_FROM_SCREEN(screen), &window_rect, event_co[0], event_co[1]);
    if (actedge == NULL) {
      return OPERATOR_CANCELLED;
    }

    dir = screen_geom_edge_is_horizontal(actedge) ? 'v' : 'h';

    RNA_property_enum_set(op->ptr, prop_dir, dir);

    /* special case, adds customdata, sets defaults */
    if (!area_split_menu_init(C, op)) {
      return OPERATOR_CANCELLED;
    }
  }

  sd = (sAreaSplitData *)op->customdata;

  if (event->type == EVT_ACTIONZONE_AREA) {

    /* do the split */
    if (area_split_apply(C, op)) {
      area_move_set_limits(win, screen, dir, &sd->bigger, &sd->smaller, NULL);

      /* add temp handler for edge move or cancel */
      G.moving |= G_TRANSFORM_WM;
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
static int area_split_exec(bContext *C, wmOperator *op)
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
    if (screen_area_join(C, CTX_wm_screen(C), sd->sarea, sd->narea)) {
      if (CTX_wm_area(C) == sd->narea) {
        CTX_wm_area_set(C, NULL);
        CTX_wm_region_set(C, NULL);
      }
      sd->narea = NULL;
    }
  }
  area_split_exit(C, op);
}

static int area_split_modal(bContext *C, wmOperator *op, const wmEvent *event)
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
            int dir = RNA_property_enum_get(op->ptr, prop_dir);
            RNA_property_enum_set(op->ptr, prop_dir, (dir == 'v') ? 'h' : 'v');
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
      sd->do_snap = event->val == KM_PRESS;
      update_factor = true;
      break;
  }

  if (update_factor) {
    const int dir = RNA_property_enum_get(op->ptr, prop_dir);

    sd->delta = (dir == 'v') ? event->x - sd->origval : event->y - sd->origval;

    if (sd->previewmode == 0) {
      if (sd->do_snap) {
        const int snap_loc = area_snap_calc_location(CTX_wm_screen(C),
                                                     SNAP_FRACTION_AND_ADJACENT,
                                                     sd->delta,
                                                     sd->origval,
                                                     dir,
                                                     sd->bigger,
                                                     sd->smaller);
        sd->delta = snap_loc - sd->origval;
      }
      area_move_apply_do(C, sd->delta, sd->origval, dir, sd->bigger, sd->smaller, SNAP_NONE);
    }
    else {
      if (sd->sarea) {
        ED_area_tag_redraw(sd->sarea);
      }
      /* area context not set */
      sd->sarea = BKE_screen_find_area_xy(CTX_wm_screen(C), SPACE_TYPE_ANY, event->x, event->y);

      if (sd->sarea) {
        ScrArea *area = sd->sarea;
        if (dir == 'v') {
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
                                                       dir,
                                                       sd->origmin + sd->origsize,
                                                       -sd->origmin);

          area->v1->editflag = area->v2->editflag = area->v3->editflag = area->v4->editflag = 0;
          sd->delta = snap_loc - sd->origval;
        }

        ED_area_tag_redraw(sd->sarea);
      }

      CTX_wm_screen(C)->do_draw = true;
    }

    float fac = (float)(sd->delta + sd->origval - sd->origmin) / sd->origsize;
    RNA_float_set(op->ptr, "factor", fac);
  }

  return OPERATOR_RUNNING_MODAL;
}

static const EnumPropertyItem prop_direction_items[] = {
    {'h', "HORIZONTAL", 0, "Horizontal", ""},
    {'v', "VERTICAL", 0, "Vertical", ""},
    {0, NULL, 0, NULL, NULL},
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
  RNA_def_enum(ot->srna, "direction", prop_direction_items, 'h', "Direction", "");
  RNA_def_float(ot->srna, "factor", 0.5f, 0.0, 1.0, "Factor", "", 0.0, 1.0);
  RNA_def_int_vector(
      ot->srna, "cursor", 2, NULL, INT_MIN, INT_MAX, "Cursor", "", INT_MIN, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scale Region Edge Operator
 * \{ */

typedef struct RegionMoveData {
  AZone *az;
  ARegion *region;
  ScrArea *area;
  int bigger, smaller, origval;
  int origx, origy;
  int maxsize;
  AZEdge edge;

} RegionMoveData;

static int area_max_regionsize(ScrArea *area, ARegion *scalear, AZEdge edge)
{
  int dist;

  /* regions in regions. */
  if (scalear->alignment & RGN_SPLIT_PREV) {
    const int align = RGN_ALIGN_ENUM_FROM_MASK(scalear->alignment);

    if (ELEM(align, RGN_ALIGN_TOP, RGN_ALIGN_BOTTOM)) {
      ARegion *region = scalear->prev;
      dist = region->winy + scalear->winy - U.pixelsize;
    }
    else /* if (ELEM(align, RGN_ALIGN_LEFT, RGN_ALIGN_RIGHT)) */ {
      ARegion *region = scalear->prev;
      dist = region->winx + scalear->winx - U.pixelsize;
    }
  }
  else {
    if (edge == AE_RIGHT_TO_TOPLEFT || edge == AE_LEFT_TO_TOPRIGHT) {
      dist = BLI_rcti_size_x(&area->totrct);
    }
    else { /* AE_BOTTOM_TO_TOPLEFT, AE_TOP_TO_BOTTOMRIGHT */
      dist = BLI_rcti_size_y(&area->totrct);
    }

    /* subtractwidth of regions on opposite side
     * prevents dragging regions into other opposite regions */
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      if (region == scalear) {
        continue;
      }

      if (scalear->alignment == RGN_ALIGN_LEFT && region->alignment == RGN_ALIGN_RIGHT) {
        dist -= region->winx;
      }
      else if (scalear->alignment == RGN_ALIGN_RIGHT && region->alignment == RGN_ALIGN_LEFT) {
        dist -= region->winx;
      }
      else if (scalear->alignment == RGN_ALIGN_TOP &&
               (region->alignment == RGN_ALIGN_BOTTOM ||
                ELEM(
                    region->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER, RGN_TYPE_FOOTER))) {
        dist -= region->winy;
      }
      else if (scalear->alignment == RGN_ALIGN_BOTTOM &&
               (region->alignment == RGN_ALIGN_TOP ||
                ELEM(
                    region->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER, RGN_TYPE_FOOTER))) {
        dist -= region->winy;
      }
    }
  }

  dist /= UI_DPI_FAC;
  return dist;
}

static bool is_split_edge(const int alignment, const AZEdge edge)
{
  return ((alignment == RGN_ALIGN_BOTTOM) && (edge == AE_TOP_TO_BOTTOMRIGHT)) ||
         ((alignment == RGN_ALIGN_TOP) && (edge == AE_BOTTOM_TO_TOPLEFT)) ||
         ((alignment == RGN_ALIGN_LEFT) && (edge == AE_RIGHT_TO_TOPLEFT)) ||
         ((alignment == RGN_ALIGN_RIGHT) && (edge == AE_LEFT_TO_TOPRIGHT));
}

static void region_scale_exit(wmOperator *op)
{
  MEM_freeN(op->customdata);
  op->customdata = NULL;

  G.moving &= ~G_TRANSFORM_WM;
}

static int region_scale_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  sActionzoneData *sad = event->customdata;
  AZone *az;

  if (event->type != EVT_ACTIONZONE_REGION) {
    BKE_report(op->reports, RPT_ERROR, "Can only scale region size from an action zone");
    return OPERATOR_CANCELLED;
  }

  az = sad->az;

  if (az->region) {
    RegionMoveData *rmd = MEM_callocN(sizeof(RegionMoveData), "RegionMoveData");

    op->customdata = rmd;

    rmd->az = az;
    /* special case for region within region - this allows the scale of
     * the parent region if the azone edge is not the edge splitting
     * both regions */
    if ((az->region->alignment & RGN_SPLIT_PREV) && az->region->prev &&
        !is_split_edge(RGN_ALIGN_ENUM_FROM_MASK(az->region->alignment), az->edge)) {
      rmd->region = az->region->prev;
    }
    else {
      rmd->region = az->region;
    }
    rmd->area = sad->sa1;
    rmd->edge = az->edge;
    rmd->origx = event->x;
    rmd->origy = event->y;
    rmd->maxsize = area_max_regionsize(rmd->area, rmd->region, rmd->edge);

    /* if not set we do now, otherwise it uses type */
    if (rmd->region->sizex == 0) {
      rmd->region->sizex = rmd->region->winx;
    }
    if (rmd->region->sizey == 0) {
      rmd->region->sizey = rmd->region->winy;
    }

    /* now copy to regionmovedata */
    if (rmd->edge == AE_LEFT_TO_TOPRIGHT || rmd->edge == AE_RIGHT_TO_TOPLEFT) {
      rmd->origval = rmd->region->sizex;
    }
    else {
      rmd->origval = rmd->region->sizey;
    }

    CLAMP(rmd->maxsize, 0, 1000);

    /* add temp handler */
    G.moving |= G_TRANSFORM_WM;
    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }

  return OPERATOR_FINISHED;
}

static void region_scale_validate_size(RegionMoveData *rmd)
{
  if ((rmd->region->flag & RGN_FLAG_HIDDEN) == 0) {
    short *size, maxsize = -1;

    if (rmd->edge == AE_LEFT_TO_TOPRIGHT || rmd->edge == AE_RIGHT_TO_TOPLEFT) {
      size = &rmd->region->sizex;
    }
    else {
      size = &rmd->region->sizey;
    }

    maxsize = rmd->maxsize - (UI_UNIT_Y / UI_DPI_FAC);

    if (*size > maxsize && maxsize > 0) {
      *size = maxsize;
    }
  }
}

static void region_scale_toggle_hidden(bContext *C, RegionMoveData *rmd)
{
  /* hidden areas may have bad 'View2D.cur' value,
   * correct before displaying. see T45156 */
  if (rmd->region->flag & RGN_FLAG_HIDDEN) {
    UI_view2d_curRect_validate(&rmd->region->v2d);
  }

  region_toggle_hidden(C, rmd->region, 0);
  region_scale_validate_size(rmd);

  if ((rmd->region->flag & RGN_FLAG_HIDDEN) == 0) {
    if (rmd->region->regiontype == RGN_TYPE_HEADER) {
      ARegion *region_tool_header = BKE_area_find_region_type(rmd->area, RGN_TYPE_TOOL_HEADER);
      if (region_tool_header != NULL) {
        if ((region_tool_header->flag & RGN_FLAG_HIDDEN_BY_USER) == 0 &&
            (region_tool_header->flag & RGN_FLAG_HIDDEN) != 0) {
          region_toggle_hidden(C, region_tool_header, 0);
        }
      }
    }
  }
}

static int region_scale_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  RegionMoveData *rmd = op->customdata;
  int delta;

  /* execute the events */
  switch (event->type) {
    case MOUSEMOVE: {
      const float aspect = BLI_rctf_size_x(&rmd->region->v2d.cur) /
                           (BLI_rcti_size_x(&rmd->region->v2d.mask) + 1);
      const int snap_size_threshold = (U.widget_unit * 2) / aspect;
      if (rmd->edge == AE_LEFT_TO_TOPRIGHT || rmd->edge == AE_RIGHT_TO_TOPLEFT) {
        delta = event->x - rmd->origx;
        if (rmd->edge == AE_LEFT_TO_TOPRIGHT) {
          delta = -delta;
        }

        /* region sizes now get multiplied */
        delta /= UI_DPI_FAC;

        const int size_no_snap = rmd->origval + delta;
        rmd->region->sizex = size_no_snap;

        if (rmd->region->type->snap_size) {
          short sizex_test = rmd->region->type->snap_size(rmd->region, rmd->region->sizex, 0);
          if (abs(rmd->region->sizex - sizex_test) < snap_size_threshold) {
            rmd->region->sizex = sizex_test;
          }
        }
        CLAMP(rmd->region->sizex, 0, rmd->maxsize);

        if (size_no_snap < UI_UNIT_X / aspect) {
          rmd->region->sizex = rmd->origval;
          if (!(rmd->region->flag & RGN_FLAG_HIDDEN)) {
            region_scale_toggle_hidden(C, rmd);
          }
        }
        else if (rmd->region->flag & RGN_FLAG_HIDDEN) {
          region_scale_toggle_hidden(C, rmd);
        }
        else if (rmd->region->flag & RGN_FLAG_DYNAMIC_SIZE) {
          rmd->region->sizex = rmd->origval;
        }
      }
      else {
        delta = event->y - rmd->origy;
        if (rmd->edge == AE_BOTTOM_TO_TOPLEFT) {
          delta = -delta;
        }

        /* region sizes now get multiplied */
        delta /= UI_DPI_FAC;

        const int size_no_snap = rmd->origval + delta;
        rmd->region->sizey = size_no_snap;

        if (rmd->region->type->snap_size) {
          short sizey_test = rmd->region->type->snap_size(rmd->region, rmd->region->sizey, 1);
          if (abs(rmd->region->sizey - sizey_test) < snap_size_threshold) {
            rmd->region->sizey = sizey_test;
          }
        }
        CLAMP(rmd->region->sizey, 0, rmd->maxsize);

        /* note, 'UI_UNIT_Y/4' means you need to drag the footer and execute region
         * almost all the way down for it to become hidden, this is done
         * otherwise its too easy to do this by accident */
        if (size_no_snap < (UI_UNIT_Y / 4) / aspect) {
          rmd->region->sizey = rmd->origval;
          if (!(rmd->region->flag & RGN_FLAG_HIDDEN)) {
            region_scale_toggle_hidden(C, rmd);
          }
        }
        else if (rmd->region->flag & RGN_FLAG_HIDDEN) {
          region_scale_toggle_hidden(C, rmd);
        }
        else if (rmd->region->flag & RGN_FLAG_DYNAMIC_SIZE) {
          rmd->region->sizey = rmd->origval;
        }
      }
      ED_area_tag_redraw(rmd->area);
      WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);

      break;
    }
    case LEFTMOUSE:
      if (event->val == KM_RELEASE) {
        if (len_manhattan_v2v2_int(&event->x, &rmd->origx) <= WM_EVENT_CURSOR_MOTION_THRESHOLD) {
          if (rmd->region->flag & RGN_FLAG_HIDDEN) {
            region_scale_toggle_hidden(C, rmd);
          }
          else if (rmd->region->flag & RGN_FLAG_TOO_SMALL) {
            region_scale_validate_size(rmd);
          }

          ED_area_tag_redraw(rmd->area);
          WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);
        }

        region_scale_exit(op);

        return OPERATOR_FINISHED;
      }
      break;

    case EVT_ESCKEY:
      break;
  }

  return OPERATOR_RUNNING_MODAL;
}

static void region_scale_cancel(bContext *UNUSED(C), wmOperator *op)
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

static void areas_do_frame_follow(bContext *C, bool middle)
{
  bScreen *screen_ctx = CTX_wm_screen(C);
  Scene *scene = CTX_data_scene(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  LISTBASE_FOREACH (wmWindow *, window, &wm->windows) {
    const bScreen *screen = WM_window_get_active_screen(window);

    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
        /* do follow here if editor type supports it */
        if ((screen_ctx->redraws_flag & TIME_FOLLOW)) {
          if ((region->regiontype == RGN_TYPE_WINDOW &&
               ELEM(area->spacetype, SPACE_SEQ, SPACE_GRAPH, SPACE_ACTION, SPACE_NLA)) ||
              (area->spacetype == SPACE_CLIP && region->regiontype == RGN_TYPE_PREVIEW)) {
            float w = BLI_rctf_size_x(&region->v2d.cur);

            if (middle) {
              if ((scene->r.cfra < region->v2d.cur.xmin) ||
                  (scene->r.cfra > region->v2d.cur.xmax)) {
                region->v2d.cur.xmax = scene->r.cfra + (w / 2);
                region->v2d.cur.xmin = scene->r.cfra - (w / 2);
              }
            }
            else {
              if (scene->r.cfra < region->v2d.cur.xmin) {
                region->v2d.cur.xmax = scene->r.cfra;
                region->v2d.cur.xmin = region->v2d.cur.xmax - w;
              }
              else if (scene->r.cfra > region->v2d.cur.xmax) {
                region->v2d.cur.xmin = scene->r.cfra;
                region->v2d.cur.xmax = region->v2d.cur.xmin + w;
              }
            }
          }
        }
      }
    }
  }
}

/* function to be called outside UI context, or for redo */
static int frame_offset_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  int delta;

  delta = RNA_int_get(op->ptr, "delta");

  CFRA += delta;
  FRAMENUMBER_MIN_CLAMP(CFRA);
  SUBFRA = 0.f;

  areas_do_frame_follow(C, false);

  DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO_SEEK);

  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_frame_offset(wmOperatorType *ot)
{
  ot->name = "Frame Offset";
  ot->idname = "SCREEN_OT_frame_offset";
  ot->description = "Move current frame forward/backward by a given number";

  ot->exec = frame_offset_exec;

  ot->poll = ED_operator_screenactive_norender;
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
static int frame_jump_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  wmTimer *animtimer = CTX_wm_screen(C)->animtimer;

  /* Don't change CFRA directly if animtimer is running as this can cause
   * first/last frame not to be actually shown (bad since for example physics
   * simulations aren't reset properly).
   */
  if (animtimer) {
    ScreenAnimData *sad = animtimer->customdata;

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
      CFRA = PEFRA;
    }
    else {
      CFRA = PSFRA;
    }

    areas_do_frame_follow(C, true);

    DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO_SEEK);

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

  ot->poll = ED_operator_screenactive_norender;
  ot->flag = OPTYPE_UNDO_GROUPED;
  ot->undo_group = "Frame Change";

  /* rna */
  RNA_def_boolean(ot->srna, "end", 0, "Last Frame", "Jump to the last frame of the frame range");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Jump to Key-Frame Operator
 * \{ */

/* function to be called outside UI context, or for redo */
static int keyframe_jump_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  bDopeSheet ads = {NULL};
  DLRBT_Tree keys;
  ActKeyColumn *ak;
  float cfra;
  const bool next = RNA_boolean_get(op->ptr, "next");
  bool done = false;

  /* sanity checks */
  if (scene == NULL) {
    return OPERATOR_CANCELLED;
  }

  cfra = (float)(CFRA);

  /* init binarytree-list for getting keyframes */
  BLI_dlrbTree_init(&keys);

  /* seed up dummy dopesheet context with flags to perform necessary filtering */
  if ((scene->flag & SCE_KEYS_NO_SELONLY) == 0) {
    /* only selected channels are included */
    ads.filterflag |= ADS_FILTER_ONLYSEL;
  }

  /* populate tree with keyframe nodes */
  scene_to_keylist(&ads, scene, &keys, 0);

  if (ob) {
    ob_to_keylist(&ads, ob, &keys, 0);

    if (ob->type == OB_GPENCIL) {
      const bool active = !(scene->flag & SCE_KEYS_NO_SELONLY);
      gpencil_to_keylist(&ads, ob->data, &keys, active);
    }
  }

  {
    Mask *mask = CTX_data_edit_mask(C);
    if (mask) {
      MaskLayer *masklay = BKE_mask_layer_active(mask);
      mask_to_keylist(&ads, masklay, &keys);
    }
  }

  /* find matching keyframe in the right direction */
  if (next) {
    ak = (ActKeyColumn *)BLI_dlrbTree_search_next(&keys, compare_ak_cfraPtr, &cfra);
  }
  else {
    ak = (ActKeyColumn *)BLI_dlrbTree_search_prev(&keys, compare_ak_cfraPtr, &cfra);
  }

  while ((ak != NULL) && (done == false)) {
    if (CFRA != (int)ak->cfra) {
      /* this changes the frame, so set the frame and we're done */
      CFRA = (int)ak->cfra;
      done = true;
    }
    else {
      /* take another step... */
      if (next) {
        ak = ak->next;
      }
      else {
        ak = ak->prev;
      }
    }
  }

  /* free temp stuff */
  BLI_dlrbTree_free(&keys);

  /* any success? */
  if (done == false) {
    BKE_report(op->reports, RPT_INFO, "No more keyframes to jump to in this direction");

    return OPERATOR_CANCELLED;
  }
  else {
    areas_do_frame_follow(C, true);

    DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO_SEEK);

    WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

    return OPERATOR_FINISHED;
  }
}

static void SCREEN_OT_keyframe_jump(wmOperatorType *ot)
{
  ot->name = "Jump to Keyframe";
  ot->description = "Jump to previous/next keyframe";
  ot->idname = "SCREEN_OT_keyframe_jump";

  ot->exec = keyframe_jump_exec;

  ot->poll = ED_operator_screenactive_norender;
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
static int marker_jump_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  TimeMarker *marker;
  int closest = CFRA;
  const bool next = RNA_boolean_get(op->ptr, "next");
  bool found = false;

  /* find matching marker in the right direction */
  for (marker = scene->markers.first; marker; marker = marker->next) {
    if (next) {
      if ((marker->frame > CFRA) && (!found || closest > marker->frame)) {
        closest = marker->frame;
        found = true;
      }
    }
    else {
      if ((marker->frame < CFRA) && (!found || closest < marker->frame)) {
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
  else {
    CFRA = closest;

    areas_do_frame_follow(C, true);

    DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO_SEEK);

    WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

    return OPERATOR_FINISHED;
  }
}

static void SCREEN_OT_marker_jump(wmOperatorType *ot)
{
  ot->name = "Jump to Marker";
  ot->description = "Jump to previous/next marker";
  ot->idname = "SCREEN_OT_marker_jump";

  ot->exec = marker_jump_exec;

  ot->poll = ED_operator_screenactive_norender;
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
static int screen_set_exec(bContext *C, wmOperator *op)
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
static int screen_maximize_area_exec(bContext *C, wmOperator *op)
{
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = NULL;
  const bool hide_panels = RNA_boolean_get(op->ptr, "use_hide_panels");

  /* search current screen for 'fullscreen' areas */
  /* prevents restoring info header, when mouse is over it */
  for (area = screen->areabase.first; area; area = area->next) {
    if (area->full) {
      break;
    }
  }

  if (area == NULL) {
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
    ED_screen_state_toggle(C, CTX_wm_window(C), area, SCREENMAXIMIZED);
  }

  return OPERATOR_FINISHED;
}

static bool screen_maximize_area_poll(bContext *C)
{
  const bScreen *screen = CTX_wm_screen(C);
  const ScrArea *area = CTX_wm_area(C);
  return ED_operator_areaactive(C) &&
         /* Don't allow maximizing global areas but allow minimizing from them. */
         ((screen->state != SCREENNORMAL) || !ED_area_is_global(area));
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

  prop = RNA_def_boolean(ot->srna, "use_hide_panels", false, "Hide Panels", "Hide all the panels");
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

typedef struct sAreaJoinData {
  ScrArea *sa1;        /* first area to be considered */
  ScrArea *sa2;        /* second area to be considered */
  void *draw_callback; /* call `ED_screen_draw_join_shape` */

} sAreaJoinData;

static void area_join_draw_cb(const struct wmWindow *UNUSED(win), void *userdata)
{
  const wmOperator *op = userdata;

  sAreaJoinData *sd = op->customdata;
  if (sd->sa1 && sd->sa2) {
    ED_screen_draw_join_shape(sd->sa1, sd->sa2);
  }
}

/* validate selection inside screen, set variables OK */
/* return 0: init failed */
static int area_join_init(bContext *C, wmOperator *op, ScrArea *sa1, ScrArea *sa2)
{
  if (sa1 == NULL || sa2 == NULL) {
    /* Get areas from cursor location if not specified. */
    int cursor[2];
    RNA_int_get_array(op->ptr, "cursor", cursor);
    screen_area_edge_from_cursor(C, cursor, &sa1, &sa2);
  }
  if (sa1 == NULL || sa2 == NULL) {
    return 0;
  }

  sAreaJoinData *jd = MEM_callocN(sizeof(sAreaJoinData), "op_area_join");

  jd->sa1 = sa1;
  jd->sa2 = sa2;

  op->customdata = jd;

  jd->draw_callback = WM_draw_cb_activate(CTX_wm_window(C), area_join_draw_cb, op);

  return 1;
}

/* apply the join of the areas (space types) */
static int area_join_apply(bContext *C, wmOperator *op)
{
  sAreaJoinData *jd = (sAreaJoinData *)op->customdata;
  if (!jd) {
    return 0;
  }

  if (!screen_area_join(C, CTX_wm_screen(C), jd->sa1, jd->sa2)) {
    return 0;
  }
  if (CTX_wm_area(C) == jd->sa2) {
    CTX_wm_area_set(C, NULL);
    CTX_wm_region_set(C, NULL);
  }

  return 1;
}

/* finish operation */
static void area_join_exit(bContext *C, wmOperator *op)
{
  sAreaJoinData *jd = (sAreaJoinData *)op->customdata;

  if (jd) {
    if (jd->draw_callback) {
      WM_draw_cb_exit(CTX_wm_window(C), jd->draw_callback);
    }

    MEM_freeN(jd);
    op->customdata = NULL;
  }

  /* this makes sure aligned edges will result in aligned grabbing */
  BKE_screen_remove_double_scredges(CTX_wm_screen(C));
  BKE_screen_remove_unused_scredges(CTX_wm_screen(C));
  BKE_screen_remove_unused_scrverts(CTX_wm_screen(C));
}

static int area_join_exec(bContext *C, wmOperator *op)
{
  if (!area_join_init(C, op, NULL, NULL)) {
    return OPERATOR_CANCELLED;
  }

  area_join_apply(C, op);
  area_join_exit(C, op);

  return OPERATOR_FINISHED;
}

/* interaction callback */
static int area_join_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{

  if (event->type == EVT_ACTIONZONE_AREA) {
    sActionzoneData *sad = event->customdata;

    if (sad == NULL || sad->modifier > 0) {
      return OPERATOR_PASS_THROUGH;
    }

    /* verify *sad itself */
    if (sad->sa1 == NULL || sad->sa2 == NULL) {
      return OPERATOR_PASS_THROUGH;
    }

    /* is this our *sad? if areas equal it should be passed on */
    if (sad->sa1 == sad->sa2) {
      return OPERATOR_PASS_THROUGH;
    }
    else {
      if (!area_join_init(C, op, sad->sa1, sad->sa2)) {
        return OPERATOR_CANCELLED;
      }
    }
  }

  /* add temp handler */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void area_join_cancel(bContext *C, wmOperator *op)
{
  WM_event_add_notifier(C, NC_WINDOW, NULL);

  area_join_exit(C, op);
}

/* modal callback while selecting area (space) that will be removed */
static int area_join_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  bScreen *screen = CTX_wm_screen(C);
  wmWindow *win = CTX_wm_window(C);
  sAreaJoinData *jd;

  if (op->customdata == NULL) {
    if (!area_join_init(C, op, NULL, NULL)) {
      return OPERATOR_CANCELLED;
    }
  }
  jd = (sAreaJoinData *)op->customdata;

  /* execute the events */
  switch (event->type) {

    case MOUSEMOVE: {
      ScrArea *area = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, event->x, event->y);
      int dir = -1;

      if (area) {
        if (jd->sa1 != area) {
          dir = area_getorientation(jd->sa1, area);
          if (dir != -1) {
            jd->sa2 = area;
          }
          else {
            /* we are not bordering on the previously selected area
             * we check if area has common border with the one marked for removal
             * in this case we can swap areas.
             */
            dir = area_getorientation(area, jd->sa2);
            if (dir != -1) {
              jd->sa1 = jd->sa2;
              jd->sa2 = area;
            }
            else {
              jd->sa2 = NULL;
            }
          }
          WM_event_add_notifier(C, NC_WINDOW, NULL);
        }
        else {
          /* we are back in the area previously selected for keeping
           * we swap the areas if possible to allow user to choose */
          if (jd->sa2 != NULL) {
            jd->sa1 = jd->sa2;
            jd->sa2 = area;
            dir = area_getorientation(jd->sa1, jd->sa2);
            if (dir == -1) {
              printf("oops, didn't expect that!\n");
            }
          }
          else {
            dir = area_getorientation(jd->sa1, area);
            if (dir != -1) {
              jd->sa2 = area;
            }
          }
          WM_event_add_notifier(C, NC_WINDOW, NULL);
        }
      }

      if (dir == 1) {
        WM_cursor_set(win, WM_CURSOR_N_ARROW);
      }
      else if (dir == 3) {
        WM_cursor_set(win, WM_CURSOR_S_ARROW);
      }
      else if (dir == 2) {
        WM_cursor_set(win, WM_CURSOR_E_ARROW);
      }
      else if (dir == 0) {
        WM_cursor_set(win, WM_CURSOR_W_ARROW);
      }
      else {
        WM_cursor_set(win, WM_CURSOR_STOP);
      }

      break;
    }
    case LEFTMOUSE:
      if (event->val == KM_RELEASE) {
        ED_area_tag_redraw(jd->sa1);
        ED_area_tag_redraw(jd->sa2);

        area_join_apply(C, op);
        WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);
        area_join_exit(C, op);
        return OPERATOR_FINISHED;
      }
      break;

    case RIGHTMOUSE:
    case EVT_ESCKEY:
      area_join_cancel(C, op);
      return OPERATOR_CANCELLED;
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

  /* api callbacks */
  ot->exec = area_join_exec;
  ot->invoke = area_join_invoke;
  ot->modal = area_join_modal;
  ot->poll = screen_active_editable;
  ot->cancel = area_join_cancel;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_INTERNAL;

  /* rna */
  RNA_def_int_vector(
      ot->srna, "cursor", 2, NULL, INT_MIN, INT_MAX, "Cursor", "", INT_MIN, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Screen Area Options Operator
 * \{ */

static int screen_area_options_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  uiPopupMenu *pup;
  uiLayout *layout;
  PointerRNA ptr;

  ScrArea *sa1, *sa2;

  if (screen_area_edge_from_cursor(C, &event->x, &sa1, &sa2) == NULL) {
    return OPERATOR_CANCELLED;
  }

  pup = UI_popup_menu_begin(C, WM_operatortype_name(op->type, op->ptr), ICON_NONE);
  layout = UI_popup_menu_layout(pup);

  /* Vertical Split */
  uiItemFullO(layout,
              "SCREEN_OT_area_split",
              IFACE_("Vertical Split"),
              ICON_NONE,
              NULL,
              WM_OP_INVOKE_DEFAULT,
              0,
              &ptr);
  /* store initial mouse cursor position. */
  RNA_int_set_array(&ptr, "cursor", &event->x);
  RNA_enum_set(&ptr, "direction", 'v');

  /* Horizontal Split */
  uiItemFullO(layout,
              "SCREEN_OT_area_split",
              IFACE_("Horizontal Split"),
              ICON_NONE,
              NULL,
              WM_OP_INVOKE_DEFAULT,
              0,
              &ptr);
  /* store initial mouse cursor position. */
  RNA_int_set_array(&ptr, "cursor", &event->x);
  RNA_enum_set(&ptr, "direction", 'h');

  if (sa1 && sa2) {
    uiItemS(layout);
  }

  /* Join needs two very similar areas. */
  if (sa1 && sa2 && (area_getorientation(sa1, sa2) != -1)) {
    uiItemFullO(layout,
                "SCREEN_OT_area_join",
                IFACE_("Join Areas"),
                ICON_NONE,
                NULL,
                WM_OP_INVOKE_DEFAULT,
                0,
                &ptr);
    RNA_int_set_array(&ptr, "cursor", &event->x);
  }

  /* Swap just needs two areas. */
  if (sa1 && sa2) {
    uiItemFullO(layout,
                "SCREEN_OT_area_swap",
                IFACE_("Swap Areas"),
                ICON_NONE,
                NULL,
                WM_OP_EXEC_DEFAULT,
                0,
                &ptr);
    RNA_int_set_array(&ptr, "cursor", &event->x);
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

  /* api callbacks */
  ot->invoke = screen_area_options_invoke;

  ot->poll = ED_operator_screen_mainwinactive;

  /* flags */
  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space Data Cleanup Operator
 * \{ */

static int spacedata_cleanup_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  bScreen *screen;
  ScrArea *area;
  int tot = 0;

  for (screen = bmain->screens.first; screen; screen = screen->id.next) {
    for (area = screen->areabase.first; area; area = area->next) {
      if (area->spacedata.first != area->spacedata.last) {
        SpaceLink *sl = area->spacedata.first;

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
  ot->name = "Clean-up Space-data";
  ot->description = "Remove unused settings for invisible editors";
  ot->idname = "SCREEN_OT_spacedata_cleanup";

  /* api callbacks */
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
  return !BLI_listbase_is_empty(&wm->operators);
}

static int repeat_last_exec(bContext *C, wmOperator *UNUSED(op))
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmOperator *lastop = wm->operators.last;

  /* Seek last registered operator */
  while (lastop) {
    if (lastop->type->flag & OPTYPE_REGISTER) {
      break;
    }
    else {
      lastop = lastop->prev;
    }
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

  /* api callbacks */
  ot->exec = repeat_last_exec;

  ot->poll = repeat_history_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Repeat History Operator
 * \{ */

static int repeat_history_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmOperator *lastop;
  uiPopupMenu *pup;
  uiLayout *layout;
  int items, i;

  items = BLI_listbase_count(&wm->operators);
  if (items == 0) {
    return OPERATOR_CANCELLED;
  }

  pup = UI_popup_menu_begin(C, WM_operatortype_name(op->type, op->ptr), ICON_NONE);
  layout = UI_popup_menu_layout(pup);

  for (i = items - 1, lastop = wm->operators.last; lastop; lastop = lastop->prev, i--) {
    if ((lastop->type->flag & OPTYPE_REGISTER) && WM_operator_repeat_check(C, lastop)) {
      uiItemIntO(layout,
                 WM_operatortype_name(lastop->type, lastop->ptr),
                 ICON_NONE,
                 op->type->idname,
                 "index",
                 i);
    }
  }

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static int repeat_history_exec(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  op = BLI_findlink(&wm->operators, RNA_int_get(op->ptr, "index"));
  if (op) {
    /* let's put it as last operator in list */
    BLI_remlink(&wm->operators, op);
    BLI_addtail(&wm->operators, op);

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

  /* api callbacks */
  ot->invoke = repeat_history_invoke;
  ot->exec = repeat_history_exec;
  ot->poll = repeat_history_poll;

  RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, 1000);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Redo Operator
 * \{ */

static int redo_last_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *UNUSED(event))
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

  /* api callbacks */
  ot->invoke = redo_last_invoke;
  ot->poll = repeat_history_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region Quad-View Operator
 * \{ */

static void view3d_localview_update_rv3d(struct RegionView3D *rv3d)
{
  if (rv3d->localvd) {
    rv3d->localvd->view = rv3d->view;
    rv3d->localvd->persp = rv3d->persp;
    copy_qt_qt(rv3d->localvd->viewquat, rv3d->viewquat);
  }
}

static void region_quadview_init_rv3d(
    ScrArea *area, ARegion *region, const char viewlock, const char view, const char persp)
{
  RegionView3D *rv3d = region->regiondata;

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
static int region_quadview_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);

  /* some rules... */
  if (region->regiontype != RGN_TYPE_WINDOW) {
    BKE_report(op->reports, RPT_ERROR, "Only window region can be 4-split");
  }
  else if (region->alignment == RGN_ALIGN_QSPLIT) {
    /* Exit quad-view */
    ScrArea *area = CTX_wm_area(C);
    ARegion *arn;

    /* keep current region */
    region->alignment = 0;

    if (area->spacetype == SPACE_VIEW3D) {
      ARegion *region_iter;
      RegionView3D *rv3d = region->regiondata;

      /* if this is a locked view, use settings from 'User' view */
      if (rv3d->viewlock) {
        View3D *v3d_user;
        ARegion *region_user;

        if (ED_view3d_context_user_region(C, &v3d_user, &region_user)) {
          if (region != region_user) {
            SWAP(void *, region->regiondata, region_user->regiondata);
            rv3d = region->regiondata;
          }
        }
      }

      rv3d->viewlock_quad = RV3D_VIEWLOCK_INIT;
      rv3d->viewlock = 0;

      /* FIXME: This fixes missing update to workbench TAA. (see T76216)
       * However, it would be nice if the tagging should be done in a more conventional way. */
      rv3d->rflag |= RV3D_GPULIGHT_UPDATE;

      /* Accumulate locks, in case they're mixed. */
      for (region_iter = area->regionbase.first; region_iter; region_iter = region_iter->next) {
        if (region_iter->regiontype == RGN_TYPE_WINDOW) {
          RegionView3D *rv3d_iter = region_iter->regiondata;
          rv3d->viewlock_quad |= rv3d_iter->viewlock;
        }
      }
    }

    for (region = area->regionbase.first; region; region = arn) {
      arn = region->next;
      if (region->alignment == RGN_ALIGN_QSPLIT) {
        ED_region_remove(C, area, region);
      }
    }
    ED_area_tag_redraw(area);
    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);
  }
  else if (region->next) {
    BKE_report(op->reports, RPT_ERROR, "Only last region can be 4-split");
  }
  else {
    /* Enter quad-view */
    ScrArea *area = CTX_wm_area(C);
    ARegion *newar;
    int count;

    region->alignment = RGN_ALIGN_QSPLIT;

    for (count = 0; count < 3; count++) {
      newar = BKE_area_region_copy(area->type, region);
      BLI_addtail(&area->regionbase, newar);
    }

    /* lock views and set them */
    if (area->spacetype == SPACE_VIEW3D) {
      View3D *v3d = area->spacedata.first;
      int index_qsplit = 0;

      /* run ED_view3d_lock() so the correct 'rv3d->viewquat' is set,
       * otherwise when restoring rv3d->localvd the 'viewquat' won't
       * match the 'view', set on entering localview See: [#26315],
       *
       * We could avoid manipulating rv3d->localvd here if exiting
       * localview with a 4-split would assign these view locks */
      RegionView3D *rv3d = region->regiondata;
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
    WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);
  }

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_region_quadview(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Quad View";
  ot->description = "Split selected area into camera, front, right & top views";
  ot->idname = "SCREEN_OT_region_quadview";

  /* api callbacks */
  ot->exec = region_quadview_exec;
  ot->poll = ED_operator_region_view3d_active;
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region Toggle Operator
 * \{ */

static int region_toggle_exec(bContext *C, wmOperator *op)
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
    return 0;
  }

  return ED_operator_areaactive(C);
}

static void SCREEN_OT_region_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Region";
  ot->idname = "SCREEN_OT_region_toggle";
  ot->description = "Hide or unhide the region";

  /* api callbacks */
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
static int region_flip_exec(bContext *C, wmOperator *UNUSED(op))
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
  WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

static bool region_flip_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);

  /* Don't flip anything around in top-bar. */
  if (area && area->spacetype == SPACE_TOPBAR) {
    CTX_wm_operator_poll_msg_set(C, "Flipping regions in the Top-bar is not allowed");
    return 0;
  }

  return ED_operator_areaactive(C);
}

static void SCREEN_OT_region_flip(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Flip Region";
  ot->idname = "SCREEN_OT_region_flip";
  ot->description = "Toggle the region's alignment (left/right or top/bottom)";

  /* api callbacks */
  ot->exec = region_flip_exec;
  ot->poll = region_flip_poll;
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Header Toggle Menu Operator
 * \{ */

/* show/hide header text menus */
static int header_toggle_menus_exec(bContext *C, wmOperator *UNUSED(op))
{
  ScrArea *area = CTX_wm_area(C);

  area->flag = area->flag ^ HEADER_NO_PULLDOWN;

  ED_area_tag_redraw(area);
  WM_event_add_notifier(C, NC_SCREEN | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_header_toggle_menus(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Expand/Collapse Header Menus";
  ot->idname = "SCREEN_OT_header_toggle_menus";
  ot->description = "Expand or collapse the header pulldown menus";

  /* api callbacks */
  ot->exec = header_toggle_menus_exec;
  ot->poll = ED_operator_areaactive;
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region Context Menu Operator (Header/Footer/Navbar)
 * \{ */

static bool screen_region_context_menu_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  return (area && area->spacetype != SPACE_STATUSBAR);
}

void ED_screens_header_tools_menu_create(bContext *C, uiLayout *layout, void *UNUSED(arg))
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  const char *but_flip_str = (RGN_ALIGN_ENUM_FROM_MASK(region->alignment) == RGN_ALIGN_TOP) ?
                                 IFACE_("Flip to Bottom") :
                                 IFACE_("Flip to Top");
  {
    PointerRNA ptr;
    RNA_pointer_create((ID *)CTX_wm_screen(C), &RNA_Space, area->spacedata.first, &ptr);
    if (!ELEM(area->spacetype, SPACE_TOPBAR)) {
      uiItemR(layout, &ptr, "show_region_header", 0, IFACE_("Show Header"), ICON_NONE);
    }

    ARegion *region_header = BKE_area_find_region_type(area, RGN_TYPE_HEADER);
    uiLayout *col = uiLayoutColumn(layout, 0);
    uiLayoutSetActive(col, (region_header->flag & RGN_FLAG_HIDDEN) == 0);

    if (BKE_area_find_region_type(area, RGN_TYPE_TOOL_HEADER)) {
      uiItemR(col, &ptr, "show_region_tool_header", 0, IFACE_("Show Tool Settings"), ICON_NONE);
    }

    uiItemO(col,
            IFACE_("Show Menus"),
            (area->flag & HEADER_NO_PULLDOWN) ? ICON_CHECKBOX_DEHLT : ICON_CHECKBOX_HLT,
            "SCREEN_OT_header_toggle_menus");
  }

  /* default is WM_OP_INVOKE_REGION_WIN, which we don't want here. */
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  if (!ELEM(area->spacetype, SPACE_TOPBAR)) {
    uiItemS(layout);

    uiItemO(layout, but_flip_str, ICON_NONE, "SCREEN_OT_region_flip");
  }

  /* File browser should be fullscreen all the time, top-bar should
   * never be. But other regions can be maximized/restored. */
  if (!ELEM(area->spacetype, SPACE_FILE, SPACE_TOPBAR)) {
    uiItemS(layout);

    const char *but_str = area->full ? IFACE_("Tile Area") : IFACE_("Maximize Area");
    uiItemO(layout, but_str, ICON_NONE, "SCREEN_OT_screen_full_area");
  }
}

void ED_screens_footer_tools_menu_create(bContext *C, uiLayout *layout, void *UNUSED(arg))
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  const char *but_flip_str = (RGN_ALIGN_ENUM_FROM_MASK(region->alignment) == RGN_ALIGN_TOP) ?
                                 IFACE_("Flip to Bottom") :
                                 IFACE_("Flip to Top");
  {
    PointerRNA ptr;
    RNA_pointer_create((ID *)CTX_wm_screen(C), &RNA_Space, area->spacedata.first, &ptr);
    uiItemR(layout, &ptr, "show_region_footer", 0, IFACE_("Show Footer"), ICON_NONE);
  }

  /* default is WM_OP_INVOKE_REGION_WIN, which we don't want here. */
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  uiItemO(layout, but_flip_str, ICON_NONE, "SCREEN_OT_region_flip");

  /* File browser should be fullscreen all the time, top-bar should
   * never be. But other regions can be maximized/restored... */
  if (!ELEM(area->spacetype, SPACE_FILE, SPACE_TOPBAR)) {
    uiItemS(layout);

    const char *but_str = area->full ? IFACE_("Tile Area") : IFACE_("Maximize Area");
    uiItemO(layout, but_str, ICON_NONE, "SCREEN_OT_screen_full_area");
  }
}

void ED_screens_navigation_bar_tools_menu_create(bContext *C, uiLayout *layout, void *UNUSED(arg))
{
  const ARegion *region = CTX_wm_region(C);
  const char *but_flip_str = (RGN_ALIGN_ENUM_FROM_MASK(region->alignment) == RGN_ALIGN_LEFT) ?
                                 IFACE_("Flip to Right") :
                                 IFACE_("Flip to Left");

  /* default is WM_OP_INVOKE_REGION_WIN, which we don't want here. */
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  uiItemO(layout, but_flip_str, ICON_NONE, "SCREEN_OT_region_flip");
}

static int screen_context_menu_invoke(bContext *C,
                                      wmOperator *UNUSED(op),
                                      const wmEvent *UNUSED(event))
{
  uiPopupMenu *pup;
  uiLayout *layout;
  const ARegion *region = CTX_wm_region(C);

  if (ELEM(region->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
    pup = UI_popup_menu_begin(C, IFACE_("Header"), ICON_NONE);
    layout = UI_popup_menu_layout(pup);
    ED_screens_header_tools_menu_create(C, layout, NULL);
    UI_popup_menu_end(C, pup);
  }
  else if (region->regiontype == RGN_TYPE_FOOTER) {
    pup = UI_popup_menu_begin(C, IFACE_("Footer"), ICON_NONE);
    layout = UI_popup_menu_layout(pup);
    ED_screens_footer_tools_menu_create(C, layout, NULL);
    UI_popup_menu_end(C, pup);
  }
  else if (region->regiontype == RGN_TYPE_NAV_BAR) {
    pup = UI_popup_menu_begin(C, IFACE_("Navigation Bar"), ICON_NONE);
    layout = UI_popup_menu_layout(pup);
    ED_screens_navigation_bar_tools_menu_create(C, layout, NULL);
    UI_popup_menu_end(C, pup);
  }

  return OPERATOR_INTERFACE;
}

static void SCREEN_OT_region_context_menu(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Region Context Menu";
  ot->description = "Display region context menu";
  ot->idname = "SCREEN_OT_region_context_menu";

  /* api callbacks */
  ot->poll = screen_region_context_menu_poll;
  ot->invoke = screen_context_menu_invoke;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation Step Operator
 *
 * Animation Step.
 * \{ */

static int match_region_with_redraws(int spacetype,
                                     int regiontype,
                                     int redraws,
                                     bool from_anim_edit)
{
  if (regiontype == RGN_TYPE_WINDOW) {

    switch (spacetype) {
      case SPACE_VIEW3D:
        if ((redraws & TIME_ALL_3D_WIN) || from_anim_edit) {
          return 1;
        }
        break;
      case SPACE_GRAPH:
      case SPACE_NLA:
        if ((redraws & TIME_ALL_ANIM_WIN) || from_anim_edit) {
          return 1;
        }
        break;
      case SPACE_ACTION:
        /* if only 1 window or 3d windows, we do timeline too
         * NOTE: Now we do action editor in all these cases, since timeline is here. */
        if ((redraws & (TIME_ALL_ANIM_WIN | TIME_REGION | TIME_ALL_3D_WIN)) || from_anim_edit) {
          return 1;
        }
        break;
      case SPACE_PROPERTIES:
        if (redraws & TIME_ALL_BUTS_WIN) {
          return 1;
        }
        break;
      case SPACE_SEQ:
        if ((redraws & (TIME_SEQ | TIME_ALL_ANIM_WIN)) || from_anim_edit) {
          return 1;
        }
        break;
      case SPACE_NODE:
        if (redraws & TIME_NODES) {
          return 1;
        }
        break;
      case SPACE_IMAGE:
        if ((redraws & TIME_ALL_IMAGE_WIN) || from_anim_edit) {
          return 1;
        }
        break;
      case SPACE_CLIP:
        if ((redraws & TIME_CLIPS) || from_anim_edit) {
          return 1;
        }
        break;
    }
  }
  else if (regiontype == RGN_TYPE_CHANNELS) {
    switch (spacetype) {
      case SPACE_GRAPH:
      case SPACE_ACTION:
      case SPACE_NLA:
        if (redraws & TIME_ALL_ANIM_WIN) {
          return 1;
        }
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
      return 1;
    }

    if (redraws & TIME_ALL_BUTS_WIN) {
      return 1;
    }
  }
  else if (ELEM(regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
    if (spacetype == SPACE_ACTION) {
      return 1;
    }
  }
  else if (regiontype == RGN_TYPE_PREVIEW) {
    switch (spacetype) {
      case SPACE_SEQ:
        if (redraws & (TIME_SEQ | TIME_ALL_ANIM_WIN)) {
          return 1;
        }
        break;
      case SPACE_CLIP:
        return 1;
    }
  }
  return 0;
}

//#define PROFILE_AUDIO_SYNCH

static int screen_animation_step(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  bScreen *screen = CTX_wm_screen(C);
  wmWindow *win = CTX_wm_window(C);

#ifdef PROFILE_AUDIO_SYNCH
  static int old_frame = 0;
  int newfra_int;
#endif

  if (screen->animtimer && screen->animtimer == event->customdata) {
    Main *bmain = CTX_data_main(C);
    Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = WM_window_get_active_view_layer(win);
    Depsgraph *depsgraph = BKE_scene_get_depsgraph(bmain, scene, view_layer, false);
    Scene *scene_eval = (depsgraph != NULL) ? DEG_get_evaluated_scene(depsgraph) : NULL;
    wmTimer *wt = screen->animtimer;
    ScreenAnimData *sad = wt->customdata;
    wmWindowManager *wm = CTX_wm_manager(C);
    wmWindow *window;
    ScrArea *area;
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

    if (scene_eval == NULL) {
      /* Happens when undo/redo system is used during playback, nothing meaningful we can do here.
       */
    }
    else if (scene_eval->id.recalc & ID_RECALC_AUDIO_SEEK) {
      /* Ignore seek here, the audio will be updated to the scene frame after jump during next
       * dependency graph update. */
    }
    else if ((scene->audio.flag & AUDIO_SYNC) && (sad->flag & ANIMPLAY_FLAG_REVERSE) == false &&
             isfinite(time = BKE_sound_sync_scene(scene_eval))) {
      double newfra = time * FPS;

      /* give some space here to avoid jumps */
      if (newfra + 0.5 > scene->r.cfra && newfra - 0.5 < scene->r.cfra) {
        scene->r.cfra++;
      }
      else {
        scene->r.cfra = max_ii(scene->r.cfra, newfra + 0.5);
      }

#ifdef PROFILE_AUDIO_SYNCH
      newfra_int = scene->r.cfra;
      if (newfra_int < old_frame) {
        printf("back jump detected, frame %d!\n", newfra_int);
      }
      else if (newfra_int > old_frame + 1) {
        printf("forward jump detected, frame %d!\n", newfra_int);
      }
      fflush(stdout);
      old_frame = newfra_int;
#endif
    }
    else {
      if (sync) {
        /* Try to keep the playback in realtime by dropping frames. */

        /* How much time (in frames) has passed since the last frame was drawn? */
        double delta_frames = wt->delta * FPS;

        /* Add the remaining fraction from the last time step. */
        delta_frames += sad->lagging_frame_count;

        if (delta_frames < 1.0) {
          /* We can render faster than the scene frame rate. However skipping or delaying frames
           * here seems to in practice lead to jittery playback so just step forward a minimum of
           * one frame. (Even though this can lead to too fast playback, the jitteryness is more
           * annoying)
           */
          delta_frames = 1.0f;
          sad->lagging_frame_count = 0;
        }
        else {
          /* Extract the delta frame fractions that will be skipped when converting to int. */
          sad->lagging_frame_count = delta_frames - (int)delta_frames;
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
      DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO_SEEK);
#ifdef PROFILE_AUDIO_SYNCH
      old_frame = CFRA;
#endif
    }

    /* since we follow drawflags, we can't send notifier but tag regions ourselves */
    if (depsgraph != NULL) {
      ED_update_for_newframe(bmain, depsgraph);
    }

    for (window = wm->windows.first; window; window = window->next) {
      const bScreen *win_screen = WM_window_get_active_screen(window);

      for (area = win_screen->areabase.first; area; area = area->next) {
        ARegion *region;
        for (region = area->regionbase.first; region; region = region->next) {
          bool redraw = false;
          if (region == sad->region) {
            redraw = true;
          }
          else if (match_region_with_redraws(
                       area->spacetype, region->regiontype, sad->redraws, sad->from_anim_edit)) {
            redraw = true;
          }

          if (redraw) {
            ED_region_tag_redraw(region);
            /* do follow here if editor type supports it */
            if ((sad->redraws & TIME_FOLLOW)) {
              if ((region->regiontype == RGN_TYPE_WINDOW &&
                   ELEM(area->spacetype, SPACE_SEQ, SPACE_GRAPH, SPACE_ACTION, SPACE_NLA)) ||
                  (area->spacetype == SPACE_CLIP && region->regiontype == RGN_TYPE_PREVIEW)) {
                float w = BLI_rctf_size_x(&region->v2d.cur);
                if (scene->r.cfra < region->v2d.cur.xmin) {
                  region->v2d.cur.xmax = scene->r.cfra;
                  region->v2d.cur.xmin = region->v2d.cur.xmax - w;
                }
                else if (scene->r.cfra > region->v2d.cur.xmax) {
                  region->v2d.cur.xmin = scene->r.cfra;
                  region->v2d.cur.xmax = region->v2d.cur.xmin + w;
                }
              }
            }
          }
        }
      }
    }

    /* update frame rate info too
     * NOTE: this may not be accurate enough, since we might need this after modifiers/etc.
     * have been calculated instead of just before updates have been done?
     */
    ED_refresh_viewport_fps(C);

    /* Recalculate the time-step for the timer now that we've finished calculating this,
     * since the frames-per-second value may have been changed.
     */
    /* TODO: this may make evaluation a bit slower if the value doesn't change...
     * any way to avoid this? */
    wt->timestep = (1.0 / FPS);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_PASS_THROUGH;
}

static void SCREEN_OT_animation_step(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Animation Step";
  ot->description = "Step through animation by position";
  ot->idname = "SCREEN_OT_animation_step";

  /* api callbacks */
  ot->invoke = screen_animation_step;

  ot->poll = ED_operator_screenactive_norender;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation Playback Operator
 *
 * Animation Playback with Timer.
 * \{ */

/* find window that owns the animation timer */
bScreen *ED_screen_animation_playing(const wmWindowManager *wm)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    bScreen *screen = WM_window_get_active_screen(win);

    if (screen->animtimer || screen->scrubbing) {
      return screen;
    }
  }

  return NULL;
}

bScreen *ED_screen_animation_no_scrub(const wmWindowManager *wm)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    bScreen *screen = WM_window_get_active_screen(win);

    if (screen->animtimer) {
      return screen;
    }
  }

  return NULL;
}

/* toggle operator */
int ED_screen_animation_play(bContext *C, int sync, int mode)
{
  bScreen *screen = CTX_wm_screen(C);
  Scene *scene = CTX_data_scene(C);
  Scene *scene_eval = DEG_get_evaluated_scene(CTX_data_ensure_evaluated_depsgraph(C));

  if (ED_screen_animation_playing(CTX_wm_manager(C))) {
    /* stop playback now */
    ED_screen_animation_timer(C, 0, 0, 0);
    BKE_sound_stop_scene(scene_eval);

    WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
  }
  else {
    /* these settings are currently only available from a menu in the TimeLine */
    if (mode == 1) { /* XXX only play audio forwards!? */
      BKE_sound_play_scene(scene_eval);
    }

    ED_screen_animation_timer(C, screen->redraws_flag, sync, mode);

    if (screen->animtimer) {
      wmTimer *wt = screen->animtimer;
      ScreenAnimData *sad = wt->customdata;

      sad->region = CTX_wm_region(C);
    }
  }

  return OPERATOR_FINISHED;
}

static int screen_animation_play_exec(bContext *C, wmOperator *op)
{
  int mode = (RNA_boolean_get(op->ptr, "reverse")) ? -1 : 1;
  int sync = -1;

  if (RNA_struct_property_is_set(op->ptr, "sync")) {
    sync = (RNA_boolean_get(op->ptr, "sync"));
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

  /* api callbacks */
  ot->exec = screen_animation_play_exec;

  ot->poll = ED_operator_screenactive_norender;

  prop = RNA_def_boolean(
      ot->srna, "reverse", 0, "Play in Reverse", "Animation is played backwards");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "sync", 0, "Sync", "Drop frames to maintain framerate");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation Cancel Operator
 * \{ */

static int screen_animation_cancel_exec(bContext *C, wmOperator *op)
{
  bScreen *screen = ED_screen_animation_playing(CTX_wm_manager(C));

  if (screen) {
    if (RNA_boolean_get(op->ptr, "restore_frame") && screen->animtimer) {
      ScreenAnimData *sad = screen->animtimer->customdata;
      Scene *scene = CTX_data_scene(C);

      /* reset current frame before stopping, and just send a notifier to deal with the rest
       * (since playback still needs to be stopped)
       */
      scene->r.cfra = sad->sfra;

      WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
    }

    /* call the other "toggling" operator to clean up now */
    ED_screen_animation_play(C, 0, 0);
  }

  return OPERATOR_PASS_THROUGH;
}

static void SCREEN_OT_animation_cancel(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cancel Animation";
  ot->description = "Cancel animation, returning to the original frame";
  ot->idname = "SCREEN_OT_animation_cancel";

  /* api callbacks */
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
static int box_select_exec(bContext *C, wmOperator *op)
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

  /* api callbacks */
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

static int fullscreen_back_exec(bContext *C, wmOperator *op)
{
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = NULL;

  /* search current screen for 'fullscreen' areas */
  for (area = screen->areabase.first; area; area = area->next) {
    if (area->full) {
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

static void SCREEN_OT_back_to_previous(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Back to Previous Screen";
  ot->description = "Revert back to the original screen layout, before fullscreen area overlay";
  ot->idname = "SCREEN_OT_back_to_previous";

  /* api callbacks */
  ot->exec = fullscreen_back_exec;
  ot->poll = ED_operator_screenactive;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Show User Preferences Operator
 * \{ */

static int userpref_show_exec(bContext *C, wmOperator *op)
{
  wmWindow *win_cur = CTX_wm_window(C);
  /* Use eventstate, not event from _invoke, so this can be called through exec(). */
  const wmEvent *event = win_cur->eventstate;
  int sizex = (500 + UI_NAVIGATION_REGION_WIDTH) * UI_DPI_FAC;
  int sizey = 520 * UI_DPI_FAC;

  /* changes context! */
  if (WM_window_open_temp(C,
                          IFACE_("Blender Preferences"),
                          event->x,
                          event->y,
                          sizex,
                          sizey,
                          SPACE_USERPREF,
                          false) != NULL) {
    /* The header only contains the editor switcher and looks empty.
     * So hiding in the temp window makes sense. */
    ScrArea *area = CTX_wm_area(C);
    ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_HEADER);

    region->flag |= RGN_FLAG_HIDDEN;
    ED_region_visibility_change_update(C, area, region);

    return OPERATOR_FINISHED;
  }
  else {
    BKE_report(op->reports, RPT_ERROR, "Failed to open window!");
    return OPERATOR_CANCELLED;
  }
}

static void SCREEN_OT_userpref_show(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show Preferences";
  ot->description = "Edit user preferences and system settings";
  ot->idname = "SCREEN_OT_userpref_show";

  /* api callbacks */
  ot->exec = userpref_show_exec;
  ot->poll = ED_operator_screenactive;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Show Drivers Editor Operator
 * \{ */

static int drivers_editor_show_exec(bContext *C, wmOperator *op)
{
  wmWindow *win_cur = CTX_wm_window(C);
  /* Use eventstate, not event from _invoke, so this can be called through exec(). */
  const wmEvent *event = win_cur->eventstate;
  PointerRNA ptr = {NULL};
  PropertyRNA *prop = NULL;
  int index = -1;
  uiBut *but = NULL;

  int sizex = 900 * UI_DPI_FAC;
  int sizey = 580 * UI_DPI_FAC;

  /* Get active property to show driver for
   * - Need to grab it first, or else this info disappears
   *   after we've created the window
   */
  but = UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  /* changes context! */
  if (WM_window_open_temp(C,
                          IFACE_("Blender Drivers Editor"),
                          event->x,
                          event->y,
                          sizex,
                          sizey,
                          SPACE_GRAPH,
                          false) != NULL) {
    ED_drivers_editor_init(C, CTX_wm_area(C));

    /* activate driver F-Curve for the property under the cursor */
    if (but) {
      FCurve *fcu;
      bool driven, special;

      fcu = BKE_fcurve_find_by_rna_context_ui(C, &ptr, prop, index, NULL, NULL, &driven, &special);
      if (fcu) {
        /* Isolate this F-Curve... */
        bAnimContext ac;
        if (ANIM_animdata_get_context(C, &ac)) {
          int filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_NODUPLIS;
          ANIM_deselect_anim_channels(&ac, ac.data, ac.datatype, 0, ACHANNEL_SETFLAG_CLEAR);
          ANIM_set_active_channel(&ac, ac.data, ac.datatype, filter, fcu, ANIMTYPE_FCURVE);
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
  else {
    BKE_report(op->reports, RPT_ERROR, "Failed to open window!");
    return OPERATOR_CANCELLED;
  }
}

static void SCREEN_OT_drivers_editor_show(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show Drivers Editor";
  ot->description = "Show drivers editor in a separate window";
  ot->idname = "SCREEN_OT_drivers_editor_show";

  /* api callbacks */
  ot->exec = drivers_editor_show_exec;
  ot->poll = ED_operator_screenactive;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Show Info Log Operator
 * \{ */

static int info_log_show_exec(bContext *C, wmOperator *op)
{
  wmWindow *win_cur = CTX_wm_window(C);
  /* Use eventstate, not event from _invoke, so this can be called through exec(). */
  const wmEvent *event = win_cur->eventstate;
  int sizex = 900 * UI_DPI_FAC;
  int sizey = 580 * UI_DPI_FAC;
  int shift_y = 480;

  /* changes context! */
  if (WM_window_open_temp(C,
                          IFACE_("Blender Info Log"),
                          event->x,
                          event->y + shift_y,
                          sizex,
                          sizey,
                          SPACE_INFO,
                          false) != NULL) {
    return OPERATOR_FINISHED;
  }
  else {
    BKE_report(op->reports, RPT_ERROR, "Failed to open window!");
    return OPERATOR_CANCELLED;
  }
}

static void SCREEN_OT_info_log_show(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show Info Log";
  ot->description = "Show info log in a separate window";
  ot->idname = "SCREEN_OT_info_log_show";

  /* api callbacks */
  ot->exec = info_log_show_exec;
  ot->poll = ED_operator_screenactive;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name New Screen Operator
 * \{ */

static int screen_new_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  wmWindow *win = CTX_wm_window(C);
  WorkSpace *workspace = BKE_workspace_active_get(win->workspace_hook);
  WorkSpaceLayout *layout_old = BKE_workspace_active_layout_get(win->workspace_hook);
  WorkSpaceLayout *layout_new;

  layout_new = ED_workspace_layout_duplicate(bmain, workspace, layout_old, win);
  WM_event_add_notifier(C, NC_SCREEN | ND_LAYOUTBROWSE, layout_new);

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_new(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "New Screen";
  ot->description = "Add a new screen";
  ot->idname = "SCREEN_OT_new";

  /* api callbacks */
  ot->exec = screen_new_exec;
  ot->poll = WM_operator_winactive;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Screen Operator
 * \{ */

static int screen_delete_exec(bContext *C, wmOperator *UNUSED(op))
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

  /* api callbacks */
  ot->exec = screen_delete_exec;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region Alpha Blending Operator
 *
 * Implementation note: a disappearing region needs at least 1 last draw with
 * 100% backbuffer texture over it - then triple buffer will clear it entirely.
 * This because flag #RGN_FLAG_HIDDEN is set in end - region doesn't draw at all then.
 *
 * \{ */

typedef struct RegionAlphaInfo {
  ScrArea *area;
  ARegion *region, *child_region; /* other region */
  int hidden;
} RegionAlphaInfo;

#define TIMEOUT 0.1f
#define TIMESTEP (1.0f / 60.0f)

float ED_region_blend_alpha(ARegion *region)
{
  /* check parent too */
  if (region->regiontimer == NULL && (region->alignment & RGN_SPLIT_PREV) && region->prev) {
    region = region->prev;
  }

  if (region->regiontimer) {
    RegionAlphaInfo *rgi = region->regiontimer->customdata;
    float alpha;

    alpha = (float)region->regiontimer->duration / TIMEOUT;
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
  RegionAlphaInfo *rgi = region->regiontimer->customdata;

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
      ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), rgi->area);
    }
    /* area decoration needs redraw in end */
    ED_area_tag_redraw(rgi->area);
  }
  WM_event_remove_timer(CTX_wm_manager(C), NULL, region->regiontimer); /* frees rgi */
  region->regiontimer = NULL;
}
/**
 * \note Assumes that \a region itself is not a split version from previous region.
 */
void ED_region_visibility_change_update_animated(bContext *C, ScrArea *area, ARegion *region)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  RegionAlphaInfo *rgi;

  /* end running timer */
  if (region->regiontimer) {

    region_blend_end(C, region, true);
  }
  rgi = MEM_callocN(sizeof(RegionAlphaInfo), "RegionAlphaInfo");

  rgi->hidden = region->flag & RGN_FLAG_HIDDEN;
  rgi->area = area;
  rgi->region = region;
  region->flag &= ~RGN_FLAG_HIDDEN;

  /* blend in, reinitialize regions because it got unhidden */
  if (rgi->hidden == 0) {
    ED_area_initialize(wm, win, area);
  }
  else {
    WM_event_remove_handlers(C, &region->handlers);
  }

  if (region->next) {
    if (region->next->alignment & RGN_SPLIT_PREV) {
      rgi->child_region = region->next;
    }
  }

  /* new timer */
  region->regiontimer = WM_event_add_timer(wm, win, TIMERREGION, TIMESTEP);
  region->regiontimer->customdata = rgi;
}

/* timer runs in win->handlers, so it cannot use context to find area/region */
static int region_blend_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  RegionAlphaInfo *rgi;
  wmTimer *timer = event->customdata;

  /* event type is TIMERREGION, but we better check */
  if (event->type != TIMERREGION || timer == NULL) {
    return OPERATOR_PASS_THROUGH;
  }

  rgi = timer->customdata;

  /* always send redraws */
  ED_region_tag_redraw(rgi->region);
  if (rgi->child_region) {
    ED_region_tag_redraw(rgi->child_region);
  }

  /* end timer? */
  if (rgi->region->regiontimer->duration > (double)TIMEOUT) {
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

  /* api callbacks */
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

static int space_type_set_or_cycle_exec(bContext *C, wmOperator *op)
{
  const int space_type = RNA_enum_get(op->ptr, "space_type");

  PointerRNA ptr;
  ScrArea *area = CTX_wm_area(C);
  RNA_pointer_create((ID *)CTX_wm_screen(C), &RNA_Area, area, &ptr);
  PropertyRNA *prop_type = RNA_struct_find_property(&ptr, "type");
  PropertyRNA *prop_ui_type = RNA_struct_find_property(&ptr, "ui_type");

  if (area->spacetype != space_type) {
    /* Set the type. */
    RNA_property_enum_set(&ptr, prop_type, space_type);
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
      MEM_freeN((void *)item);
    }
  }

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_space_type_set_or_cycle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cycle Space Type Set";
  ot->description = "Set the space type or cycle sub-type";
  ot->idname = "SCREEN_OT_space_type_set_or_cycle";

  /* api callbacks */
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
    {0, NULL, 0, NULL, NULL},
};

static bool space_context_cycle_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  /* area might be NULL if called out of window bounds */
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
      RNA_pointer_create(&screen->id, &RNA_SpaceProperties, area->spacedata.first, r_ptr);
      propname = "context";
      break;
    case SPACE_USERPREF:
      RNA_pointer_create(NULL, &RNA_Preferences, &U, r_ptr);
      propname = "active_section";
      break;
    default:
      BLI_assert(0);
      propname = "";
  }

  *r_prop = RNA_struct_find_property(r_ptr, propname);
}

static int space_context_cycle_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  const int direction = RNA_enum_get(op->ptr, "direction");

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

  /* api callbacks */
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

static int space_workspace_cycle_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  wmWindow *win = CTX_wm_window(C);
  if (WM_window_is_temp_screen(win)) {
    return OPERATOR_CANCELLED;
  }

  Main *bmain = CTX_data_main(C);
  const int direction = RNA_enum_get(op->ptr, "direction");
  WorkSpace *workspace_src = WM_window_get_active_workspace(win);
  WorkSpace *workspace_dst = NULL;

  ListBase ordered;
  BKE_id_ordered_list(&ordered, &bmain->workspaces);

  LISTBASE_FOREACH (LinkData *, link, &ordered) {
    if (link->data == workspace_src) {
      if (direction == SPACE_CONTEXT_CYCLE_PREV) {
        workspace_dst = (link->prev) ? link->prev->data : NULL;
      }
      else {
        workspace_dst = (link->next) ? link->next->data : NULL;
      }
    }
  }

  if (workspace_dst == NULL) {
    LinkData *link = (direction == SPACE_CONTEXT_CYCLE_PREV) ? ordered.last : ordered.first;
    workspace_dst = link->data;
  }

  BLI_freelistN(&ordered);

  if (workspace_src == workspace_dst) {
    return OPERATOR_CANCELLED;
  }

  win->workspace_hook->temp_workspace_store = workspace_dst;
  WM_event_add_notifier(C, NC_SCREEN | ND_WORKSPACE_SET, workspace_dst);
  win->workspace_hook->temp_workspace_store = NULL;

  return OPERATOR_FINISHED;
}

static void SCREEN_OT_workspace_cycle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cycle Workspace";
  ot->description = "Cycle through workspaces";
  ot->idname = "SCREEN_OT_workspace_cycle";

  /* api callbacks */
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

/* called in spacetypes.c */
void ED_operatortypes_screen(void)
{
  /* generic UI stuff */
  WM_operatortype_append(SCREEN_OT_actionzone);
  WM_operatortype_append(SCREEN_OT_repeat_last);
  WM_operatortype_append(SCREEN_OT_repeat_history);
  WM_operatortype_append(SCREEN_OT_redo_last);

  /* screen tools */
  WM_operatortype_append(SCREEN_OT_area_move);
  WM_operatortype_append(SCREEN_OT_area_split);
  WM_operatortype_append(SCREEN_OT_area_join);
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
  WM_operatortype_append(SCREEN_OT_userpref_show);
  WM_operatortype_append(SCREEN_OT_drivers_editor_show);
  WM_operatortype_append(SCREEN_OT_info_log_show);
  WM_operatortype_append(SCREEN_OT_region_blend);
  WM_operatortype_append(SCREEN_OT_space_type_set_or_cycle);
  WM_operatortype_append(SCREEN_OT_space_context_cycle);
  WM_operatortype_append(SCREEN_OT_workspace_cycle);

  /*frame changes*/
  WM_operatortype_append(SCREEN_OT_frame_offset);
  WM_operatortype_append(SCREEN_OT_frame_jump);
  WM_operatortype_append(SCREEN_OT_keyframe_jump);
  WM_operatortype_append(SCREEN_OT_marker_jump);

  WM_operatortype_append(SCREEN_OT_animation_step);
  WM_operatortype_append(SCREEN_OT_animation_play);
  WM_operatortype_append(SCREEN_OT_animation_cancel);

  /* new/delete */
  WM_operatortype_append(SCREEN_OT_new);
  WM_operatortype_append(SCREEN_OT_delete);

  /* tools shared by more space types */
  WM_operatortype_append(ED_OT_undo);
  WM_operatortype_append(ED_OT_undo_push);
  WM_operatortype_append(ED_OT_redo);
  WM_operatortype_append(ED_OT_undo_redo);
  WM_operatortype_append(ED_OT_undo_history);

  WM_operatortype_append(ED_OT_flush_edits);
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
      {KM_MODAL_SNAP_ON, "SNAP", 0, "Snap on", ""},
      {KM_MODAL_SNAP_OFF, "SNAP_OFF", 0, "Snap off", ""},
      {0, NULL, 0, NULL, NULL},
  };
  wmKeyMap *keymap;

  /* Standard Modal keymap ------------------------------------------------ */
  keymap = WM_modalkeymap_ensure(keyconf, "Standard Modal Map", modal_items);

  WM_modalkeymap_assign(keymap, "SCREEN_OT_area_move");
}

static bool blend_file_drop_poll(bContext *UNUSED(C),
                                 wmDrag *drag,
                                 const wmEvent *UNUSED(event),
                                 const char **UNUSED(r_tooltip))
{
  if (drag->type == WM_DRAG_PATH) {
    if (drag->icon == ICON_FILE_BLEND) {
      return 1;
    }
  }
  return 0;
}

static void blend_file_drop_copy(wmDrag *drag, wmDropBox *drop)
{
  /* copy drag path to properties */
  RNA_string_set(drop->ptr, "filepath", drag->path);
}

/* called in spacetypes.c */
void ED_keymap_screen(wmKeyConfig *keyconf)
{
  ListBase *lb;

  /* Screen Editing ------------------------------------------------ */
  WM_keymap_ensure(keyconf, "Screen Editing", 0, 0);

  /* Header Editing ------------------------------------------------ */
  /* note: this is only used when the cursor is inside the header */
  WM_keymap_ensure(keyconf, "Header", 0, 0);

  /* Screen General ------------------------------------------------ */
  WM_keymap_ensure(keyconf, "Screen", 0, 0);

  /* Anim Playback ------------------------------------------------ */
  WM_keymap_ensure(keyconf, "Frames", 0, 0);

  /* dropbox for entire window */
  lb = WM_dropboxmap_find("Window", 0, 0);
  WM_dropbox_add(lb, "WM_OT_drop_blend_file", blend_file_drop_poll, blend_file_drop_copy);
  WM_dropbox_add(lb, "UI_OT_drop_color", UI_drop_color_poll, UI_drop_color_copy);

  keymap_modal_set(keyconf);
}

/** \} */
