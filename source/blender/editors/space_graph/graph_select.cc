/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spgraph
 */

#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_lasso_2d.hh"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "BKE_fcurve.hh"
#include "BKE_nla.h"

#include "UI_interface_c.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "ED_anim_api.hh"
#include "ED_keyframes_edit.hh"
#include "ED_markers.hh"
#include "ED_select_utils.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "graph_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Internal Keyframe Utilities
 * \{ */

/* temp info for caching handle vertices close */
struct tNearestVertInfo {
  tNearestVertInfo *next, *prev;

  FCurve *fcu; /* F-Curve that keyframe comes from */

  BezTriple *bezt; /* keyframe to consider */
  FPoint *fpt;     /* sample point to consider */

  short hpoint; /* the handle index that we hit (eHandleIndex) */
  short sel;    /* whether the handle is selected or not */
  int dist;     /* distance from mouse to vert */

  eAnim_ChannelType ctype; /* type of animation channel this FCurve comes from */

  float frame; /* frame that point was on when it matched (global time) */
};

/* Tags for the type of graph vert that we have */
enum eGraphVertIndex {
  NEAREST_HANDLE_LEFT = -1,
  NEAREST_HANDLE_KEY,
  NEAREST_HANDLE_RIGHT,
};

/* Tolerance for absolute radius (in pixels) of the vert from the cursor to use */
/* TODO: perhaps this should depend a bit on the size that the user set the vertices to be? */
#define GVERTSEL_TOL (10 * U.pixelsize)

/* ....... */

/* check if its ok to select a handle */
/* XXX also need to check for int-values only? */
static bool fcurve_handle_sel_check(SpaceGraph *sipo, BezTriple *bezt)
{
  if (sipo->flag & SIPO_NOHANDLES) {
    return false;
  }
  if ((sipo->flag & SIPO_SELVHANDLESONLY) && BEZT_ISSEL_ANY(bezt) == 0) {
    return false;
  }
  return true;
}

/* check if the given vertex is within bounds or not */
/* TODO: should we return if we hit something? */
static void nearest_fcurve_vert_store(ListBase *matches,
                                      View2D *v2d,
                                      FCurve *fcu,
                                      eAnim_ChannelType ctype,
                                      BezTriple *bezt,
                                      FPoint *fpt,
                                      short hpoint,
                                      const int mval[2],
                                      float unit_scale,
                                      float offset)
{
  /* Keyframes or Samples? */
  if (bezt) {
    int screen_co[2], dist;

    /* convert from data-space to screen coordinates
     * NOTE: `hpoint +1` gives us 0,1,2 respectively for each handle,
     * needed to access the relevant vertex coordinates in the 3x3 'vec' matrix */
    if (UI_view2d_view_to_region_clip(v2d,
                                      bezt->vec[hpoint + 1][0],
                                      (bezt->vec[hpoint + 1][1] + offset) * unit_scale,
                                      &screen_co[0],
                                      &screen_co[1]) &&
        /* check if distance from mouse cursor to vert in screen space is within tolerance */
        ((dist = len_v2v2_int(mval, screen_co)) <= GVERTSEL_TOL))
    {
      tNearestVertInfo *nvi = (tNearestVertInfo *)matches->last;
      bool replace = false;

      /* If there is already a point for the F-Curve,
       * check if this point is closer than that was. */
      if ((nvi) && (nvi->fcu == fcu)) {
        /* replace if we are closer, or if equal and that one wasn't selected but we are... */
        if ((nvi->dist > dist) || ((nvi->sel == 0) && BEZT_ISSEL_ANY(bezt))) {
          replace = true;
        }
      }
      /* add new if not replacing... */
      if (replace == 0) {
        nvi = static_cast<tNearestVertInfo *>(
            MEM_callocN(sizeof(tNearestVertInfo), "Nearest Graph Vert Info - Bezt"));
      }

      /* store values */
      nvi->fcu = fcu;
      nvi->ctype = ctype;

      nvi->bezt = bezt;
      nvi->hpoint = hpoint;
      nvi->dist = dist;

      nvi->frame = bezt->vec[1][0]; /* currently in global time... */

      /* NOTE: `hpoint` is -1,0,1, but `BEZT_ISSEL_IDX` expects 0,1,2. */
      nvi->sel = BEZT_ISSEL_IDX(bezt, hpoint + 1);

      /* add to list of matches if appropriate... */
      if (replace == 0) {
        BLI_addtail(matches, nvi);
      }
    }
  }
  else if (fpt) {
    /* TODO: support #FPoint. */
  }
}

/* helper for find_nearest_fcurve_vert() - build the list of nearest matches */
static void get_nearest_fcurve_verts_list(bAnimContext *ac, const int mval[2], ListBase *matches)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  SpaceGraph *sipo = (SpaceGraph *)ac->sl;
  View2D *v2d = &ac->region->v2d;
  short mapping_flag = 0;

  /* get curves to search through
   * - if the option to only show keyframes that belong to selected F-Curves is enabled,
   *   include the 'only selected' flag...
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_NODUPLIS);
  /* FIXME: this should really be check for by the filtering code. */
  if (U.animation_flag & USER_ANIM_ONLY_SHOW_SELECTED_CURVE_KEYS) {
    filter |= ANIMFILTER_SEL;
  }
  mapping_flag |= ANIM_get_normalization_flags(ac->sl);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;
    AnimData *adt = ANIM_nla_mapping_get(ac, ale);
    float offset;
    float unit_scale = ANIM_unit_mapping_get_factor(
        ac->scene, ale->id, fcu, mapping_flag, &offset);

    /* apply NLA mapping to all the keyframes */
    if (adt) {
      ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), false, false);
    }

    if (fcu->bezt) {
      BezTriple *bezt1 = fcu->bezt, *prevbezt = nullptr;
      int i;

      for (i = 0; i < fcu->totvert; i++, prevbezt = bezt1, bezt1++) {
        /* keyframe */
        nearest_fcurve_vert_store(matches,
                                  v2d,
                                  fcu,
                                  eAnim_ChannelType(ale->type),
                                  bezt1,
                                  nullptr,
                                  NEAREST_HANDLE_KEY,
                                  mval,
                                  unit_scale,
                                  offset);

        /* Handles. */
        if (fcurve_handle_sel_check(sipo, bezt1)) {
          /* first handle only visible if previous segment had handles */
          if ((!prevbezt && (bezt1->ipo == BEZT_IPO_BEZ)) ||
              (prevbezt && (prevbezt->ipo == BEZT_IPO_BEZ)))
          {
            nearest_fcurve_vert_store(matches,
                                      v2d,
                                      fcu,
                                      eAnim_ChannelType(ale->type),
                                      bezt1,
                                      nullptr,
                                      NEAREST_HANDLE_LEFT,
                                      mval,
                                      unit_scale,
                                      offset);
          }

          /* second handle only visible if this segment is bezier */
          if (bezt1->ipo == BEZT_IPO_BEZ) {
            nearest_fcurve_vert_store(matches,
                                      v2d,
                                      fcu,
                                      eAnim_ChannelType(ale->type),
                                      bezt1,
                                      nullptr,
                                      NEAREST_HANDLE_RIGHT,
                                      mval,
                                      unit_scale,
                                      offset);
          }
        }
      }
    }
    else if (fcu->fpt) {
      /* TODO: do this for samples too. */
    }

    /* un-apply NLA mapping from all the keyframes */
    if (adt) {
      ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), true, false);
    }
  }

  /* free channels */
  ANIM_animdata_freelist(&anim_data);
}

/* helper for find_nearest_fcurve_vert() - get the best match to use */
static tNearestVertInfo *get_best_nearest_fcurve_vert(ListBase *matches)
{
  /* abort if list is empty */
  if (BLI_listbase_is_empty(matches)) {
    return nullptr;
  }

  /* if list only has 1 item, remove it from the list and return */
  if (BLI_listbase_is_single(matches)) {
    /* need to remove from the list, otherwise it gets freed and then we can't return it */
    return static_cast<tNearestVertInfo *>(BLI_pophead(matches));
  }

  /* The goal of the remaining code below is to prioritize selecting verts on
   * selected fcurves, but to still cycle through the vertices in `matches` if
   * a selected-fcurve vertex is already selected. */

  /* Try to find the first selected vert in `matches`.  Additionally, if
   * one exists, rotate `matches` to put it last in the list and the vert
   * following it first, since that's the order we'll want to scan in. */
  tNearestVertInfo *nvi_first_selected = nullptr;
  LISTBASE_FOREACH (tNearestVertInfo *, nvi, matches) {
    if (nvi->sel) {
      nvi_first_selected = nvi;
      BLI_listbase_rotate_last(matches, nvi_first_selected);
      break;
    }
  }

  /* Try to find the next vert that's on the active fcurve, falling back
   * to the next vert on any selected fcurve if that's not found. */
  tNearestVertInfo *nvi_to_select = nullptr;
  LISTBASE_FOREACH (tNearestVertInfo *, nvi, matches) {
    if (nvi == nvi_first_selected) {
      continue;
    }

    if (nvi->fcu->flag & FCURVE_ACTIVE) {
      nvi_to_select = nvi;
      break;
    }

    if (nvi->fcu->flag & FCURVE_SELECTED && !nvi_to_select) {
      nvi_to_select = nvi;
    }
  }

  /* If we found a vert on a selected fcurve, return it. */
  if (nvi_to_select) {
    BLI_remlink(matches, nvi_to_select);
    return nvi_to_select;
  }

  /* If we're still here, that means we didn't find any verts on selected
   * fcurves.  So return the head (which is also the item following
   * `nvi_first_selected` if that was found). */
  return static_cast<tNearestVertInfo *>(BLI_pophead(matches));
}

/**
 * Find the nearest vertices (either a handle or the keyframe)
 * that are nearest to the mouse cursor (in area coordinates)
 *
 * \note the match info found must still be freed.
 */
static tNearestVertInfo *find_nearest_fcurve_vert(bAnimContext *ac, const int mval[2])
{
  ListBase matches = {nullptr, nullptr};
  tNearestVertInfo *nvi;

  /* step 1: get the nearest verts */
  get_nearest_fcurve_verts_list(ac, mval, &matches);

  /* step 2: find the best vert */
  nvi = get_best_nearest_fcurve_vert(&matches);

  BLI_freelistN(&matches);

  /* return the best vert found */
  return nvi;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deselect All Operator
 *
 * This operator works in one of three ways:
 * 1) (de)select all (AKEY) - test if select all or deselect all
 * 2) invert all (CTRL-IKEY) - invert selection of all keyframes
 * 3) (de)select all - no testing is done; only for use internal tools as normal function...
 * \{ */

void deselect_graph_keys(bAnimContext *ac, bool test, short sel, bool do_channels)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  KeyframeEditData ked = {{nullptr}};
  KeyframeEditFunc test_cb, sel_cb;

  /* determine type-based settings */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_NODUPLIS);

  /* filter data */
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* init BezTriple looping data */
  test_cb = ANIM_editkeyframes_ok(BEZT_OK_SELECTED);

  /* See if we should be selecting or deselecting */
  if (test) {
    LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
      if (ANIM_fcurve_keyframes_loop(
              &ked, static_cast<FCurve *>(ale->key_data), nullptr, test_cb, nullptr))
      {
        sel = SELECT_SUBTRACT;
        break;
      }
    }
  }

  /* convert sel to selectmode, and use that to get editor */
  sel_cb = ANIM_editkeyframes_select(sel);

  /* Now set the flags */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;

    /* Keyframes First */
    ANIM_fcurve_keyframes_loop(
        &ked, static_cast<FCurve *>(ale->key_data), nullptr, sel_cb, nullptr);

    /* affect channel selection status? */
    if (do_channels) {
      /* Only change selection of channel when the visibility of keyframes
       * doesn't depend on this. */
      if ((U.animation_flag & USER_ANIM_ONLY_SHOW_SELECTED_CURVE_KEYS) == 0) {
        /* deactivate the F-Curve, and deselect if deselecting keyframes.
         * otherwise select the F-Curve too since we've selected all the keyframes
         */
        if (sel == SELECT_SUBTRACT) {
          fcu->flag &= ~FCURVE_SELECTED;
        }
        else {
          fcu->flag |= FCURVE_SELECTED;
        }
      }

      /* always deactivate all F-Curves if we perform batch ops for selection */
      fcu->flag &= ~FCURVE_ACTIVE;
    }
  }

  /* Cleanup */
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_deselectall_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  bAnimListElem *ale_active = nullptr;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* find active F-Curve, and preserve this for later
   * or else it becomes annoying with the current active
   * curve keeps fading out even while you're editing it
   */
  ale_active = get_active_fcurve_channel(&ac);

  /* 'standard' behavior - check if selected, then apply relevant selection */
  const int action = RNA_enum_get(op->ptr, "action");
  switch (action) {
    case SEL_TOGGLE:
      deselect_graph_keys(&ac, true, SELECT_ADD, true);
      break;
    case SEL_SELECT:
      deselect_graph_keys(&ac, false, SELECT_ADD, true);
      break;
    case SEL_DESELECT:
      deselect_graph_keys(&ac, false, SELECT_SUBTRACT, true);
      break;
    case SEL_INVERT:
      deselect_graph_keys(&ac, false, SELECT_INVERT, true);
      break;
    default:
      BLI_assert(0);
      break;
  }

  /* restore active F-Curve... */
  if (ale_active) {
    FCurve *fcu = (FCurve *)ale_active->data;

    /* all others should not be disabled, so we should be able to just set this directly...
     * - selection needs to be set too, or else this won't work...
     */
    fcu->flag |= (FCURVE_SELECTED | FCURVE_ACTIVE);

    MEM_freeN(ale_active);
    ale_active = nullptr;
  }

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select All";
  ot->idname = "GRAPH_OT_select_all";
  ot->description = "Toggle selection of all keyframes";

  /* api callbacks */
  ot->exec = graphkeys_deselectall_exec;
  ot->poll = graphop_visible_keyframes_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Operator
 *
 * This operator currently works in one of three ways:
 * -> BKEY     - 1) all keyframes within region are selected (validation with BEZT_OK_REGION)
 * -> ALT-BKEY - depending on which axis of the region was larger...
 *    -> 2) x-axis, so select all frames within frame range (validation with BEZT_OK_FRAMERANGE)
 *    -> 3) y-axis, so select all frames within channels that region included
 *          (validation with BEZT_OK_VALUERANGE).
 *
 * The selection backend is also reused for the Lasso and Circle select operators.
 * \{ */

static rctf initialize_box_select_coords(const bAnimContext *ac, const rctf *rectf_view)
{
  const View2D *v2d = &ac->region->v2d;
  rctf rectf;

  /* Convert mouse coordinates to frame ranges and
   * channel coordinates corrected for view pan/zoom. */
  UI_view2d_region_to_view_rctf(v2d, rectf_view, &rectf);
  return rectf;
}

static int initialize_animdata_selection_filter()
{
  int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
                ANIMFILTER_NODUPLIS);
  if (U.animation_flag & USER_ANIM_ONLY_SHOW_SELECTED_CURVE_KEYS) {
    filter |= ANIMFILTER_FOREDIT | ANIMFILTER_SELEDIT;
  }
  return filter;
}

static ListBase initialize_box_select_anim_data(const int filter, bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));
  return anim_data;
}

static void initialize_box_select_key_editing_data(const bool incl_handles,
                                                   const short mode,
                                                   bAnimContext *ac,
                                                   void *data,
                                                   rctf *scaled_rectf,
                                                   KeyframeEditData *r_ked,
                                                   int *r_mapping_flag)
{
  memset(r_ked, 0, sizeof(KeyframeEditData));
  switch (mode) {
    case BEZT_OK_REGION_LASSO: {
      KeyframeEdit_LassoData *data_lasso = static_cast<KeyframeEdit_LassoData *>(data);
      data_lasso->rectf_scaled = scaled_rectf;
      r_ked->data = data_lasso;
      break;
    }
    case BEZT_OK_REGION_CIRCLE: {
      KeyframeEdit_CircleData *data_circle = static_cast<KeyframeEdit_CircleData *>(data);
      data_circle->rectf_scaled = scaled_rectf;
      r_ked->data = data_circle;
      break;
    }
    default:
      r_ked->data = scaled_rectf;
      break;
  }
  SpaceGraph *sipo = (SpaceGraph *)ac->sl;
  if (sipo->flag & SIPO_SELVHANDLESONLY) {
    r_ked->iterflags |= KEYFRAME_ITER_HANDLES_DEFAULT_INVISIBLE;
  }

  /* Enable handles selection. (used in keyframes_edit.cc > keyframe_ok_checks function) */
  if (incl_handles) {
    r_ked->iterflags |= KEYFRAME_ITER_INCL_HANDLES;
    *r_mapping_flag = 0;
  }
  else {
    *r_mapping_flag = ANIM_UNITCONV_ONLYKEYS;
  }

  *r_mapping_flag |= ANIM_get_normalization_flags(ac->sl);
}

/**
 * Box Select only selects keyframes, as overshooting handles often get caught too,
 * which means that they may be inadvertently moved as well. However, incl_handles overrides
 * this, and allow handles to be considered independently too.
 * Also, for convenience, handles should get same status as keyframe (if it was within bounds).
 *
 * This function returns true if there was any change in the selection of a key (selecting or
 * deselecting any key returns true, otherwise it returns false).
 */
static bool box_select_graphkeys(bAnimContext *ac,
                                 const rctf *rectf_view,
                                 short mode,
                                 short selectmode,
                                 bool incl_handles,
                                 void *data)
{
  const rctf rectf = initialize_box_select_coords(ac, rectf_view);
  const int filter = initialize_animdata_selection_filter();
  ListBase anim_data = initialize_box_select_anim_data(filter, ac);
  rctf scaled_rectf;
  KeyframeEditData ked;
  int mapping_flag;
  initialize_box_select_key_editing_data(
      incl_handles, mode, ac, data, &scaled_rectf, &ked, &mapping_flag);

  /* Get beztriple editing/validation functions. */
  const KeyframeEditFunc select_cb = ANIM_editkeyframes_select(selectmode);
  const KeyframeEditFunc ok_cb = ANIM_editkeyframes_ok(mode);

  /* This variable will be set to true if any key is selected or deselected. */
  bool any_key_selection_changed = false;

  /* First loop over data, doing box select. try selecting keys only. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    AnimData *adt = ANIM_nla_mapping_get(ac, ale);
    FCurve *fcu = (FCurve *)ale->key_data;
    float offset;
    const float unit_scale = ANIM_unit_mapping_get_factor(
        ac->scene, ale->id, fcu, mapping_flag, &offset);

    /* Apply NLA mapping to all the keyframes, since it's easier than trying to
     * guess when a callback might use something different.
     */
    if (adt) {
      ANIM_nla_mapping_apply_fcurve(
          adt, static_cast<FCurve *>(ale->key_data), false, incl_handles == 0);
    }

    scaled_rectf.xmin = rectf.xmin;
    scaled_rectf.xmax = rectf.xmax;
    scaled_rectf.ymin = rectf.ymin / unit_scale - offset;
    scaled_rectf.ymax = rectf.ymax / unit_scale - offset;

    /* Set horizontal range (if applicable).
     * NOTE: these values are only used for x-range and y-range but not region
     *      (which uses ked.data, i.e. rectf)
     */
    if (mode != BEZT_OK_VALUERANGE) {
      ked.f1 = rectf.xmin;
      ked.f2 = rectf.xmax;
    }
    else {
      ked.f1 = rectf.ymin;
      ked.f2 = rectf.ymax;
    }

    /* Firstly, check if any keyframes will be hit by this. */
    if (ANIM_fcurve_keyframes_loop(&ked, fcu, nullptr, ok_cb, nullptr)) {
      /* select keyframes that are in the appropriate places */
      ANIM_fcurve_keyframes_loop(&ked, fcu, ok_cb, select_cb, nullptr);
      any_key_selection_changed = true;
      /* Only change selection of channel when the visibility of keyframes
       * doesn't depend on this. */
      if ((U.animation_flag & USER_ANIM_ONLY_SHOW_SELECTED_CURVE_KEYS) == 0) {
        /* select the curve too now that curve will be touched */
        if (selectmode == SELECT_ADD) {
          fcu->flag |= FCURVE_SELECTED;
        }
      }
    }

    /* Un-apply NLA mapping from all the keyframes. */
    if (adt) {
      ANIM_nla_mapping_apply_fcurve(
          adt, static_cast<FCurve *>(ale->key_data), true, incl_handles == 0);
    }
  }

  /* Cleanup. */
  ANIM_animdata_freelist(&anim_data);

  return any_key_selection_changed;
}

/**
 * This function is used to set all the keyframes of a given curve as selectable
 * by the "select_cb" function inside of "box_select_graphcurves".
 */
static short ok_bezier_always_ok(KeyframeEditData * /*ked*/, BezTriple * /*bezt*/)
{
  return KEYFRAME_OK_KEY | KEYFRAME_OK_H1 | KEYFRAME_OK_H2;
}

#define ABOVE 1
#define INSIDE 0
#define BELOW -1
static int rectf_curve_zone_y(const FCurve *fcu,
                              const rctf *rectf,
                              const float offset,
                              const float unit_scale,
                              const float eval_x)
{
  const float fcurve_y = (evaluate_fcurve(fcu, eval_x) + offset) * unit_scale;
  return fcurve_y < rectf->ymin ? BELOW : fcurve_y <= rectf->ymax ? INSIDE : ABOVE;
}

/* Checks whether the given rectangle intersects the given fcurve's calculated curve (i.e. not
 * only keyframes, but also all the interpolated values). This is done by sampling the curve at
 * different points between the xmin and the xmax of the rectangle.
 */
static bool rectf_curve_intersection(
    const float offset, const float unit_scale, const rctf *rectf, AnimData *adt, FCurve *fcu)
{
  /* 30 sampling points. This worked well in tests. */
  int num_steps = 30;

  /* Remap the range at which to evaluate the fcurves. This enables us to avoid remapping
   * the keys themselves. */
  const float mapped_max = BKE_nla_tweakedit_remap(adt, rectf->xmax, NLATIME_CONVERT_UNMAP);
  const float mapped_min = BKE_nla_tweakedit_remap(adt, rectf->xmin, NLATIME_CONVERT_UNMAP);
  const float eval_step = (mapped_max - mapped_min) / num_steps;

  /* Sample points on the given fcurve in the interval defined by the
   * mapped_min and mapped_max of the selected rectangle.
   * For each point, check if it is inside of the selection box. If it is, then select
   * all the keyframes of the curve, the curve, and stop the loop.
   */
  struct {
    float eval_x;
    int zone;
  } cur, prev;

  prev.eval_x = mapped_min;
  prev.zone = rectf_curve_zone_y(fcu, rectf, offset, unit_scale, prev.eval_x);
  if (prev.zone == INSIDE) {
    return true;
  }

  while (num_steps--) {
    cur.eval_x = prev.eval_x + eval_step;
    cur.zone = rectf_curve_zone_y(fcu, rectf, offset, unit_scale, cur.eval_x);
    if (cur.zone != prev.zone) {
      return true;
    }

    prev = cur;
  }
  return false;
}
#undef ABOVE
#undef INSIDE
#undef BELOW

/**
 * Perform a box selection of the curves themselves. This means this function tries
 * to select a curve by sampling it at various points instead of trying to select the
 * keyframes directly.
 * The selection actions done to a curve are actually done on all the keyframes of the curve.
 * \note This function is only called if no keyframe is in the selection area.
 */
static void box_select_graphcurves(bAnimContext *ac,
                                   const rctf *rectf_view,
                                   const short mode,
                                   const short selectmode,
                                   const bool incl_handles,
                                   void *data)
{
  const int filter = initialize_animdata_selection_filter();
  ListBase anim_data = initialize_box_select_anim_data(filter, ac);
  rctf scaled_rectf;
  KeyframeEditData ked;
  int mapping_flag;
  initialize_box_select_key_editing_data(
      incl_handles, mode, ac, data, &scaled_rectf, &ked, &mapping_flag);

  FCurve *last_selected_curve = nullptr;

  /* Go through all the curves and try selecting them. This function is only called
   * if no keyframe is in the selection area, so we only have to check if the curve
   * intersects the area in order to check if the selection/deselection must happen.
   */

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    AnimData *adt = ANIM_nla_mapping_get(ac, ale);
    FCurve *fcu = (FCurve *)ale->key_data;
    float offset;
    const float unit_scale = ANIM_unit_mapping_get_factor(
        ac->scene, ale->id, fcu, mapping_flag, &offset);

    const rctf rectf = initialize_box_select_coords(ac, rectf_view);

    /* scaled_rectf is declared at the top of the block because it is required by the
     * initialize_box_select_key_editing_data function (which does
     * data_xxx->rectf_scaled = scaled_rectf). The below assignment therefore modifies the
     * data we use to iterate over the curves (ked).
     */
    scaled_rectf.xmin = rectf.xmin;
    scaled_rectf.xmax = rectf.xmax;
    scaled_rectf.ymin = rectf.ymin / unit_scale - offset;
    scaled_rectf.ymax = rectf.ymax / unit_scale - offset;

    const KeyframeEditFunc select_cb = ANIM_editkeyframes_select(selectmode);
    if (rectf_curve_intersection(offset, unit_scale, &rectf, adt, fcu)) {
      if ((selectmode & SELECT_ADD) || (selectmode & SELECT_REPLACE)) {
        fcu->flag |= FCURVE_SELECTED;
        last_selected_curve = fcu;
      }
      else {
        fcu->flag &= ~FCURVE_SELECTED;
      }
      ANIM_fcurve_keyframes_loop(&ked, fcu, ok_bezier_always_ok, select_cb, nullptr);
    }
  }

  /* Make sure that one of the selected curves is active in the end. */
  if (last_selected_curve != nullptr) {
    ANIM_set_active_channel(ac,
                            ac->data,
                            eAnimCont_Types(ac->datatype),
                            eAnimFilter_Flags(filter),
                            last_selected_curve,
                            ANIMTYPE_FCURVE);
  }

  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_box_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bAnimContext ac;
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "tweak")) {
    int mval[2];
    WM_event_drag_start_mval(event, ac.region, mval);
    tNearestVertInfo *under_mouse = find_nearest_fcurve_vert(&ac, mval);
    bool mouse_is_over_element = under_mouse != nullptr;
    if (under_mouse) {
      MEM_freeN(under_mouse);
    }

    if (mouse_is_over_element) {
      return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
    }
  }

  return WM_gesture_box_invoke(C, op, event);
}

static int graphkeys_box_select_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  rcti rect;
  rctf rect_fl;
  short mode = 0;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  const int selectmode = (sel_op != SEL_OP_SUB) ? SELECT_ADD : SELECT_SUBTRACT;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    deselect_graph_keys(&ac, true, SELECT_SUBTRACT, true);
  }

  /* 'include_handles' from the operator specifies whether to include handles in the selection. */
  const bool incl_handles = RNA_boolean_get(op->ptr, "include_handles");

  /* Get settings from operator. */
  WM_operator_properties_border_to_rcti(op, &rect);

  /* Selection 'mode' depends on whether box_select region only matters on one axis. */
  if (RNA_boolean_get(op->ptr, "axis_range")) {
    /* Mode depends on which axis of the range is larger to determine which axis to use
     * - Checking this in region-space is fine, as it's fundamentally still going to be a
     *   different rect size.
     * - The frame-range select option is favored over the channel one (x over y),
     *   as frame-range one is often used for tweaking timing when "blocking",
     *   while channels is not that useful.
     */
    if (BLI_rcti_size_x(&rect) >= BLI_rcti_size_y(&rect)) {
      mode = BEZT_OK_FRAMERANGE;
    }
    else {
      mode = BEZT_OK_VALUERANGE;
    }
  }
  else {
    mode = BEZT_OK_REGION;
  }

  BLI_rctf_rcti_copy(&rect_fl, &rect);

  /* Apply box_select action. */
  const bool any_key_selection_changed = box_select_graphkeys(
      &ac, &rect_fl, mode, selectmode, incl_handles, nullptr);
  const bool use_curve_selection = RNA_boolean_get(op->ptr, "use_curve_selection");
  if (use_curve_selection && !any_key_selection_changed) {
    box_select_graphcurves(&ac, &rect_fl, mode, selectmode, incl_handles, nullptr);
  }
  /* Send notifier that keyframe selection has changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_select_box(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Box Select";
  ot->idname = "GRAPH_OT_select_box";
  ot->description = "Select all keyframes within the specified region";

  /* API callbacks. */
  ot->invoke = graphkeys_box_select_invoke;
  ot->exec = graphkeys_box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = graphop_visible_keyframes_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  ot->prop = RNA_def_boolean(ot->srna, "axis_range", false, "Axis Range", "");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);

  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna,
                         "include_handles",
                         true,
                         "Include Handles",
                         "Are handles tested individually against the selection criteria");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna, "tweak", false, "Tweak", "Operator has been activated using a click-drag event");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna,
      "use_curve_selection",
      true,
      "Select Curves",
      "Allow selecting all the keyframes of a curve by selecting the calculated F-curve");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/* ------------------- */

static int graphkeys_lassoselect_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  KeyframeEdit_LassoData data_lasso{};
  rcti rect;
  rctf rect_fl;

  bool incl_handles;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  data_lasso.rectf_view = &rect_fl;
  data_lasso.mcoords = WM_gesture_lasso_path_to_array(C, op);
  if (data_lasso.mcoords.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  const short selectmode = (sel_op != SEL_OP_SUB) ? SELECT_ADD : SELECT_SUBTRACT;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    deselect_graph_keys(&ac, false, SELECT_SUBTRACT, true);
  }

  {
    SpaceGraph *sipo = (SpaceGraph *)ac.sl;
    if (selectmode == SELECT_ADD) {
      incl_handles = ((sipo->flag & SIPO_SELVHANDLESONLY) || (sipo->flag & SIPO_NOHANDLES)) == 0;
    }
    else {
      incl_handles = (sipo->flag & SIPO_NOHANDLES) == 0;
    }
  }

  /* Get settings from operator. */
  BLI_lasso_boundbox(&rect, data_lasso.mcoords);
  BLI_rctf_rcti_copy(&rect_fl, &rect);

  /* Apply box_select action. */
  const bool any_key_selection_changed = box_select_graphkeys(
      &ac, &rect_fl, BEZT_OK_REGION_LASSO, selectmode, incl_handles, &data_lasso);
  const bool use_curve_selection = RNA_boolean_get(op->ptr, "use_curve_selection");
  if (use_curve_selection && !any_key_selection_changed) {
    box_select_graphcurves(
        &ac, &rect_fl, BEZT_OK_REGION_LASSO, selectmode, incl_handles, &data_lasso);
  }

  /* Send notifier that keyframe selection has changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_select_lasso(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Lasso Select";
  ot->description = "Select keyframe points using lasso selection";
  ot->idname = "GRAPH_OT_select_lasso";

  /* API callbacks. */
  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = graphkeys_lassoselect_exec;
  ot->poll = graphop_visible_keyframes_poll;
  ot->cancel = WM_gesture_lasso_cancel;

  /* Flags. */
  ot->flag = OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  /* Properties. */
  WM_operator_properties_gesture_lasso(ot);
  WM_operator_properties_select_operation_simple(ot);
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna,
      "use_curve_selection",
      true,
      "Select Curves",
      "Allow selecting all the keyframes of a curve by selecting the curve itself");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ------------------- */

static int graph_circle_select_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  bool incl_handles = false;

  KeyframeEdit_CircleData data = {nullptr};
  rctf rect_fl;

  float x = RNA_int_get(op->ptr, "x");
  float y = RNA_int_get(op->ptr, "y");
  float radius = RNA_int_get(op->ptr, "radius");

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  const eSelectOp sel_op = ED_select_op_modal(
      eSelectOp(RNA_enum_get(op->ptr, "mode")),
      WM_gesture_is_modal_first(static_cast<const wmGesture *>(op->customdata)));
  const short selectmode = (sel_op != SEL_OP_SUB) ? SELECT_ADD : SELECT_SUBTRACT;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    deselect_graph_keys(&ac, false, SELECT_SUBTRACT, true);
  }

  data.mval[0] = x;
  data.mval[1] = y;
  data.radius_squared = radius * radius;
  data.rectf_view = &rect_fl;

  rect_fl.xmin = x - radius;
  rect_fl.xmax = x + radius;
  rect_fl.ymin = y - radius;
  rect_fl.ymax = y + radius;

  {
    SpaceGraph *sipo = (SpaceGraph *)ac.sl;
    if (selectmode == SELECT_ADD) {
      incl_handles = ((sipo->flag & SIPO_SELVHANDLESONLY) || (sipo->flag & SIPO_NOHANDLES)) == 0;
    }
    else {
      incl_handles = (sipo->flag & SIPO_NOHANDLES) == 0;
    }
  }

  /* Apply box_select action. */
  const bool any_key_selection_changed = box_select_graphkeys(
      &ac, &rect_fl, BEZT_OK_REGION_CIRCLE, selectmode, incl_handles, &data);
  if (any_key_selection_changed) {
    /* If any key was selected at any time during this process, the entire-curve selection should
     * be disabled. Otherwise, sliding over any keyless part of the curve will immediately cause
     * the entire curve to be selected. */
    RNA_boolean_set(op->ptr, "use_curve_selection", false);
  }
  const bool use_curve_selection = RNA_boolean_get(op->ptr, "use_curve_selection");
  if (use_curve_selection && !any_key_selection_changed) {
    box_select_graphcurves(&ac, &rect_fl, BEZT_OK_REGION_CIRCLE, selectmode, incl_handles, &data);
  }

  /* Send notifier that keyframe selection has changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_select_circle(wmOperatorType *ot)
{
  ot->name = "Circle Select";
  ot->description = "Select keyframe points using circle selection";
  ot->idname = "GRAPH_OT_select_circle";

  ot->invoke = WM_gesture_circle_invoke;
  ot->modal = WM_gesture_circle_modal;
  ot->exec = graph_circle_select_exec;
  ot->poll = graphop_visible_keyframes_poll;
  ot->cancel = WM_gesture_circle_cancel;
  ot->get_name = ED_select_circle_get_name;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_circle(ot);
  WM_operator_properties_select_operation_simple(ot);
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna,
      "use_curve_selection",
      true,
      "Select Curves",
      "Allow selecting all the keyframes of a curve by selecting the curve itself");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Column Select Operator
 *
 * This operator works in one of four ways:
 * - 1) select all keyframes in the same frame as a selected one  (KKEY)
 * - 2) select all keyframes in the same frame as the current frame marker (CTRL-KKEY)
 * - 3) select all keyframes in the same frame as a selected markers (SHIFT-KKEY)
 * - 4) select all keyframes that occur between selected markers (ALT-KKEY)
 * \{ */

/* defines for column-select mode */
static const EnumPropertyItem prop_column_select_types[] = {
    {GRAPHKEYS_COLUMNSEL_KEYS, "KEYS", 0, "On Selected Keyframes", ""},
    {GRAPHKEYS_COLUMNSEL_CFRA, "CFRA", 0, "On Current Frame", ""},
    {GRAPHKEYS_COLUMNSEL_MARKERS_COLUMN, "MARKERS_COLUMN", 0, "On Selected Markers", ""},
    {GRAPHKEYS_COLUMNSEL_MARKERS_BETWEEN,
     "MARKERS_BETWEEN",
     0,
     "Between Min/Max Selected Markers",
     ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/* ------------------- */

/* Selects all visible keyframes between the specified markers */
/* TODO(@ideasman42): this is almost an _exact_ duplicate of a function of the same name in
 * `action_select.cc` should de-duplicate. */
static void markers_selectkeys_between(bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  KeyframeEditFunc ok_cb, select_cb;
  KeyframeEditData ked = {{nullptr}};
  float min, max;

  /* get extreme markers */
  ED_markers_get_minmax(ac->markers, 1, &min, &max);
  min -= 0.5f;
  max += 0.5f;

  /* Get editing functions + data. */
  ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAMERANGE);
  select_cb = ANIM_editkeyframes_select(SELECT_ADD);

  ked.f1 = min;
  ked.f2 = max;

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* select keys in-between */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    AnimData *adt = ANIM_nla_mapping_get(ac, ale);

    if (adt) {
      ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), false, true);
      ANIM_fcurve_keyframes_loop(
          &ked, static_cast<FCurve *>(ale->key_data), ok_cb, select_cb, nullptr);
      ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), true, true);
    }
    else {
      ANIM_fcurve_keyframes_loop(
          &ked, static_cast<FCurve *>(ale->key_data), ok_cb, select_cb, nullptr);
    }
  }

  /* Cleanup */
  ANIM_animdata_freelist(&anim_data);
}

/* Selects all visible keyframes in the same frames as the specified elements */
static void columnselect_graph_keys(bAnimContext *ac, short mode)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  Scene *scene = ac->scene;
  CfraElem *ce;
  KeyframeEditFunc select_cb, ok_cb;
  KeyframeEditData ked;

  /* initialize keyframe editing data */
  memset(&ked, 0, sizeof(KeyframeEditData));

  /* build list of columns */
  switch (mode) {
    case GRAPHKEYS_COLUMNSEL_KEYS: /* list of selected keys */
      filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
                ANIMFILTER_NODUPLIS);
      ANIM_animdata_filter(
          ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

      LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
        ANIM_fcurve_keyframes_loop(
            &ked, static_cast<FCurve *>(ale->key_data), nullptr, bezt_to_cfraelem, nullptr);
      }

      ANIM_animdata_freelist(&anim_data);
      break;

    case GRAPHKEYS_COLUMNSEL_CFRA: /* current frame */
      /* make a single CfraElem for storing this */
      ce = static_cast<CfraElem *>(MEM_callocN(sizeof(CfraElem), "cfraElem"));
      BLI_addtail(&ked.list, ce);

      ce->cfra = float(scene->r.cfra);
      break;

    case GRAPHKEYS_COLUMNSEL_MARKERS_COLUMN: /* list of selected markers */
      ED_markers_make_cfra_list(ac->markers, &ked.list, SELECT);
      break;

    default: /* invalid option */
      return;
  }

  /* set up BezTriple edit callbacks */
  select_cb = ANIM_editkeyframes_select(SELECT_ADD);
  ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAME);

  /* loop through all of the keys and select additional keyframes
   * based on the keys found to be selected above
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    AnimData *adt = ANIM_nla_mapping_get(ac, ale);

    /* loop over cfraelems (stored in the KeyframeEditData->list)
     * - we need to do this here, as we can apply fewer NLA-mapping conversions
     */
    LISTBASE_FOREACH (CfraElem *, ce, &ked.list) {
      /* set frame for validation callback to refer to */
      ked.f1 = BKE_nla_tweakedit_remap(adt, ce->cfra, NLATIME_CONVERT_UNMAP);

      /* select elements with frame number matching cfraelem */
      ANIM_fcurve_keyframes_loop(
          &ked, static_cast<FCurve *>(ale->key_data), ok_cb, select_cb, nullptr);
    }
  }

  /* free elements */
  BLI_freelistN(&ked.list);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_columnselect_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short mode;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* action to take depends on the mode */
  mode = RNA_enum_get(op->ptr, "mode");

  if (mode == GRAPHKEYS_COLUMNSEL_MARKERS_BETWEEN) {
    markers_selectkeys_between(&ac);
  }
  else {
    columnselect_graph_keys(&ac, mode);
  }

  /* set notifier that keyframe selection has changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_select_column(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select All";
  ot->idname = "GRAPH_OT_select_column";
  ot->description = "Select all keyframes on the specified frame(s)";

  /* api callbacks */
  ot->exec = graphkeys_columnselect_exec;
  ot->poll = graphop_visible_keyframes_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_enum(ot->srna, "mode", prop_column_select_types, 0, "Mode", "");
  RNA_def_property_flag(ot->prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked Operator
 * \{ */

static int graphkeys_select_linked_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;

  ListBase anim_data = {nullptr, nullptr};
  int filter;

  KeyframeEditFunc ok_cb = ANIM_editkeyframes_ok(BEZT_OK_SELECTED);
  KeyframeEditFunc sel_cb = ANIM_editkeyframes_select(SELECT_ADD);

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* loop through all of the keys and select additional keyframes based on these */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;

    /* check if anything selected? */
    if (ANIM_fcurve_keyframes_loop(nullptr, fcu, nullptr, ok_cb, nullptr)) {
      /* select every keyframe in this curve then */
      ANIM_fcurve_keyframes_loop(nullptr, fcu, nullptr, sel_cb, nullptr);
    }
  }

  /* Cleanup */
  ANIM_animdata_freelist(&anim_data);

  /* set notifier that keyframe selection has changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_select_linked(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked";
  ot->idname = "GRAPH_OT_select_linked";
  ot->description = "Select keyframes occurring in the same F-Curves as selected ones";

  /* api callbacks */
  ot->exec = graphkeys_select_linked_exec;
  ot->poll = graphop_visible_keyframes_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select More/Less Operators
 * \{ */

/* Common code to perform selection */
static void select_moreless_graph_keys(bAnimContext *ac, short mode)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  KeyframeEditData ked;
  KeyframeEditFunc build_cb;

  /* init selmap building data */
  build_cb = ANIM_editkeyframes_buildselmap(mode);
  memset(&ked, 0, sizeof(KeyframeEditData));

  /* loop through all of the keys and select additional keyframes based on these */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;

    /* only continue if F-Curve has keyframes */
    if (fcu->bezt == nullptr) {
      continue;
    }

    /* build up map of whether F-Curve's keyframes should be selected or not */
    ked.data = MEM_callocN(fcu->totvert, "selmap graphEdit");
    ANIM_fcurve_keyframes_loop(&ked, fcu, nullptr, build_cb, nullptr);

    /* based on this map, adjust the selection status of the keyframes */
    ANIM_fcurve_keyframes_loop(&ked, fcu, nullptr, bezt_selmap_flush, nullptr);

    /* free the selmap used here */
    MEM_freeN(ked.data);
    ked.data = nullptr;
  }

  /* Cleanup */
  ANIM_animdata_freelist(&anim_data);
}

/* ----------------- */

static int graphkeys_select_more_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* perform select changes */
  select_moreless_graph_keys(&ac, SELMAP_MORE);

  /* set notifier that keyframe selection has changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_select_more(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select More";
  ot->idname = "GRAPH_OT_select_more";
  ot->description = "Select keyframes beside already selected ones";

  /* api callbacks */
  ot->exec = graphkeys_select_more_exec;
  ot->poll = graphop_visible_keyframes_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ----------------- */

static int graphkeys_select_less_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* perform select changes */
  select_moreless_graph_keys(&ac, SELMAP_LESS);

  /* set notifier that keyframe selection has changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_select_less(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Less";
  ot->idname = "GRAPH_OT_select_less";
  ot->description = "Deselect keyframes on ends of selection islands";

  /* api callbacks */
  ot->exec = graphkeys_select_less_exec;
  ot->poll = graphop_visible_keyframes_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Left/Right Operator
 *
 * Select keyframes left/right of the current frame indicator.
 * \{ */

/* defines for left-right select tool */
static const EnumPropertyItem prop_graphkeys_leftright_select_types[] = {
    {GRAPHKEYS_LRSEL_TEST, "CHECK", 0, "Check if Select Left or Right", ""},
    {GRAPHKEYS_LRSEL_LEFT, "LEFT", 0, "Before Current Frame", ""},
    {GRAPHKEYS_LRSEL_RIGHT, "RIGHT", 0, "After Current Frame", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/* --------------------------------- */

static void graphkeys_select_leftright(bAnimContext *ac, short leftright, short select_mode)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  KeyframeEditFunc ok_cb, select_cb;
  KeyframeEditData ked = {{nullptr}};
  Scene *scene = ac->scene;

  /* if select mode is replace, deselect all keyframes (and channels) first */
  if (select_mode == SELECT_REPLACE) {
    select_mode = SELECT_ADD;

    /* - deselect all other keyframes, so that just the newly selected remain
     * - channels aren't deselected, since we don't re-select any as a consequence
     */
    deselect_graph_keys(ac, false, SELECT_SUBTRACT, false);
  }

  /* set callbacks and editing data */
  ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAMERANGE);
  select_cb = ANIM_editkeyframes_select(select_mode);

  if (leftright == GRAPHKEYS_LRSEL_LEFT) {
    ked.f1 = MINAFRAMEF;
    ked.f2 = float(scene->r.cfra + 0.1f);
  }
  else {
    ked.f1 = float(scene->r.cfra - 0.1f);
    ked.f2 = MAXFRAMEF;
  }

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_NODUPLIS | ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* select keys */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    AnimData *adt = ANIM_nla_mapping_get(ac, ale);

    if (adt) {
      ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), false, true);
      ANIM_fcurve_keyframes_loop(
          &ked, static_cast<FCurve *>(ale->key_data), ok_cb, select_cb, nullptr);
      ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), true, true);
    }
    else {
      ANIM_fcurve_keyframes_loop(
          &ked, static_cast<FCurve *>(ale->key_data), ok_cb, select_cb, nullptr);
    }
  }

  /* Cleanup */
  ANIM_animdata_freelist(&anim_data);
}

/* ----------------- */

static int graphkeys_select_leftright_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short leftright = RNA_enum_get(op->ptr, "mode");
  short selectmode;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* select mode is either replace (deselect all, then add) or add/extend */
  if (RNA_boolean_get(op->ptr, "extend")) {
    selectmode = SELECT_INVERT;
  }
  else {
    selectmode = SELECT_REPLACE;
  }

  /* if "test" mode is set, we don't have any info to set this with */
  if (leftright == GRAPHKEYS_LRSEL_TEST) {
    return OPERATOR_CANCELLED;
  }

  /* do the selecting now */
  graphkeys_select_leftright(&ac, leftright, selectmode);

  /* set notifier that keyframe selection (and channels too) have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

static int graphkeys_select_leftright_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bAnimContext ac;
  short leftright = RNA_enum_get(op->ptr, "mode");

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* handle mode-based testing */
  if (leftright == GRAPHKEYS_LRSEL_TEST) {
    Scene *scene = ac.scene;
    ARegion *region = ac.region;
    View2D *v2d = &region->v2d;
    float x;

    /* determine which side of the current frame mouse is on */
    x = UI_view2d_region_to_view_x(v2d, event->mval[0]);
    if (x < scene->r.cfra) {
      RNA_enum_set(op->ptr, "mode", GRAPHKEYS_LRSEL_LEFT);
    }
    else {
      RNA_enum_set(op->ptr, "mode", GRAPHKEYS_LRSEL_RIGHT);
    }
  }

  /* perform selection */
  return graphkeys_select_leftright_exec(C, op);
}

void GRAPH_OT_select_leftright(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select Left/Right";
  ot->idname = "GRAPH_OT_select_leftright";
  ot->description = "Select keyframes to the left or the right of the current frame";

  /* api callbacks */
  ot->invoke = graphkeys_select_leftright_invoke;
  ot->exec = graphkeys_select_leftright_exec;
  ot->poll = graphop_visible_keyframes_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* id-props */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", prop_graphkeys_leftright_select_types, GRAPHKEYS_LRSEL_TEST, "Mode", "");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend Select", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mouse-Click Select Operator
 *
 * This operator works in one of three ways:
 * - 1) keyframe under mouse - no special modifiers
 * - 2) all keyframes on the same side of current frame indicator as mouse - ALT modifier
 * - 3) column select all keyframes in frame under mouse - CTRL modifier
 *
 * In addition to these basic options, the SHIFT modifier can be used to toggle the
 * selection mode between replacing the selection (without) and inverting the selection (with).
 * \{ */

/* option 1) select keyframe directly under mouse */
static int mouse_graph_keys(bAnimContext *ac,
                            const int mval[2],
                            eEditKeyframes_Select select_mode,
                            const bool deselect_all,
                            const bool curves_only,
                            bool wait_to_deselect_others)
{
  SpaceGraph *sipo = (SpaceGraph *)ac->sl;
  tNearestVertInfo *nvi;
  BezTriple *bezt = nullptr;
  bool run_modal = false;

  /* find the beztriple that we're selecting, and the handle that was clicked on */
  nvi = find_nearest_fcurve_vert(ac, mval);

  if (select_mode != SELECT_REPLACE) {
    /* The modal execution to delay deselecting other items is only needed for normal click
     * selection, i.e. for SELECT_REPLACE. */
    wait_to_deselect_others = false;
  }

  sipo->runtime.flag &= ~(SIPO_RUNTIME_FLAG_TWEAK_HANDLES_LEFT |
                          SIPO_RUNTIME_FLAG_TWEAK_HANDLES_RIGHT);

  const bool already_selected =
      (nvi != nullptr) && (((nvi->hpoint == NEAREST_HANDLE_KEY) && (nvi->bezt->f2 & SELECT)) ||
                           ((nvi->hpoint == NEAREST_HANDLE_LEFT) && (nvi->bezt->f1 & SELECT)) ||
                           ((nvi->hpoint == NEAREST_HANDLE_RIGHT) && (nvi->bezt->f3 & SELECT)));

  if (wait_to_deselect_others && nvi && already_selected) {
    run_modal = true;
  }
  /* For replacing selection, if we have something to select, we have to clear existing selection.
   * The same goes if we found nothing to select, and deselect_all is true
   * (deselect on nothing behavior). */
  else if ((nvi != nullptr && select_mode == SELECT_REPLACE) || (nvi == nullptr && deselect_all)) {
    /* reset selection mode */
    select_mode = SELECT_ADD;

    /* deselect all other keyframes (+ F-Curves too) */
    deselect_graph_keys(ac, false, SELECT_SUBTRACT, true);

    /* Deselect other channels too, but only do this if selection of channel
     * when the visibility of keyframes doesn't depend on this. */
    if ((U.animation_flag & USER_ANIM_ONLY_SHOW_SELECTED_CURVE_KEYS) == 0) {
      ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_CLEAR);
    }
  }

  if (nvi == nullptr) {
    return deselect_all ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
  }

  /* if points can be selected on this F-Curve */
  /* TODO: what about those with no keyframes? */
  bool something_was_selected = false;
  if (!curves_only && ((nvi->fcu->flag & FCURVE_PROTECTED) == 0)) {
    /* only if there's keyframe */
    if (nvi->bezt) {
      bezt = nvi->bezt; /* Used to check `bezt` selection is set. */
      if (select_mode == SELECT_INVERT) {
        if (nvi->hpoint == NEAREST_HANDLE_KEY) {
          bezt->f2 ^= SELECT;
          something_was_selected = (bezt->f2 & SELECT);
        }
        else if (nvi->hpoint == NEAREST_HANDLE_LEFT) {
          /* toggle selection */
          bezt->f1 ^= SELECT;
          something_was_selected = (bezt->f1 & SELECT);
        }
        else {
          /* toggle selection */
          bezt->f3 ^= SELECT;
          something_was_selected = (bezt->f3 & SELECT);
        }
      }
      else {
        if (nvi->hpoint == NEAREST_HANDLE_KEY) {
          bezt->f2 |= SELECT;
        }
        else if (nvi->hpoint == NEAREST_HANDLE_LEFT) {
          bezt->f1 |= SELECT;
        }
        else {
          bezt->f3 |= SELECT;
        }
        something_was_selected = true;
      }

      if (!run_modal && BEZT_ISSEL_ANY(bezt)) {
        const bool may_activate = !already_selected ||
                                  BKE_fcurve_active_keyframe_index(nvi->fcu) ==
                                      FCURVE_ACTIVE_KEYFRAME_NONE;
        if (may_activate) {
          BKE_fcurve_active_keyframe_set(nvi->fcu, bezt);
        }
      }
    }
    else if (nvi->fpt) {
      /* TODO: need to handle sample points */
    }
  }
  else {
    KeyframeEditFunc select_cb;
    KeyframeEditData ked;

    /* initialize keyframe editing data */
    memset(&ked, 0, sizeof(KeyframeEditData));

    /* set up BezTriple edit callbacks */
    select_cb = ANIM_editkeyframes_select(select_mode);

    /* select all keyframes */
    ANIM_fcurve_keyframes_loop(&ked, nvi->fcu, nullptr, select_cb, nullptr);
  }

  /* only change selection of channel when the visibility of keyframes doesn't depend on this */
  if ((U.animation_flag & USER_ANIM_ONLY_SHOW_SELECTED_CURVE_KEYS) == 0) {
    /* select or deselect curve? */
    if (bezt) {
      /* take selection status from item that got hit, to prevent flip/flop on channel
       * selection status when shift-selecting (i.e. "SELECT_INVERT") points
       */
      if (BEZT_ISSEL_ANY(bezt)) {
        nvi->fcu->flag |= FCURVE_SELECTED;
      }
      else {
        nvi->fcu->flag &= ~FCURVE_SELECTED;
      }
    }
    else {
      /* Didn't hit any channel,
       * so just apply that selection mode to the curve's selection status. */
      if (select_mode == SELECT_INVERT) {
        nvi->fcu->flag ^= FCURVE_SELECTED;
      }
      else if (select_mode == SELECT_ADD) {
        nvi->fcu->flag |= FCURVE_SELECTED;
      }
    }
  }

  /* Set active F-Curve when something was actually selected (so not on a deselect), except when
   * dragging the selected keys. Needs to be called with (sipo->flag & SIPO_SELCUVERTSONLY),
   * otherwise the active flag won't be set #26452. */
  if (!run_modal && (nvi->fcu->flag & FCURVE_SELECTED) && something_was_selected) {
    /* NOTE: Sync the filter flags with findnearest_fcurve_vert. */
    int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
                  ANIMFILTER_NODUPLIS);
    ANIM_set_active_channel(ac,
                            ac->data,
                            eAnimCont_Types(ac->datatype),
                            eAnimFilter_Flags(filter),
                            nvi->fcu,
                            nvi->ctype);
  }

  if (nvi->hpoint == NEAREST_HANDLE_LEFT) {
    sipo->runtime.flag |= SIPO_RUNTIME_FLAG_TWEAK_HANDLES_LEFT;
  }
  else if (nvi->hpoint == NEAREST_HANDLE_RIGHT) {
    sipo->runtime.flag |= SIPO_RUNTIME_FLAG_TWEAK_HANDLES_RIGHT;
  }

  /* free temp sample data for filtering */
  MEM_freeN(nvi);

  return run_modal ? OPERATOR_RUNNING_MODAL : OPERATOR_FINISHED;
}

/* Option 2) Selects all the keyframes on either side of the current frame
 * (depends on which side the mouse is on) */
/* (see graphkeys_select_leftright) */

/* Option 3) Selects all visible keyframes in the same frame as the mouse click */
static int graphkeys_mselect_column(bAnimContext *ac,
                                    const int mval[2],
                                    eEditKeyframes_Select select_mode,
                                    bool wait_to_deselect_others)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;
  bool run_modal = false;

  KeyframeEditFunc select_cb, ok_cb;
  KeyframeEditData ked;
  tNearestVertInfo *nvi;

  /* find the beztriple that we're selecting, and the handle that was clicked on */
  nvi = find_nearest_fcurve_vert(ac, mval);

  /* check if anything to select */
  if (nvi == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* get frame number on which elements should be selected */
  /* TODO: should we restrict to integer frames only? */
  const float selx = nvi->frame;

  if (select_mode != SELECT_REPLACE) {
    /* Doesn't need to deselect anything -> Pass. */
  }
  else if (wait_to_deselect_others && (nvi->bezt->f2 & SELECT)) {
    run_modal = true;
  }
  /* If select mode is replace (and we don't do delayed deselection on mouse release), deselect all
   * keyframes first. */
  else {
    /* reset selection mode to add to selection */
    select_mode = SELECT_ADD;

    /* - deselect all other keyframes, so that just the newly selected remain
     * - channels aren't deselected, since we don't re-select any as a consequence
     */
    deselect_graph_keys(ac, false, SELECT_SUBTRACT, false);
  }

  /* initialize keyframe editing data */
  memset(&ked, 0, sizeof(KeyframeEditData));

  /* set up BezTriple edit callbacks */
  select_cb = ANIM_editkeyframes_select(select_mode);
  ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAME);

  /* loop through all of the keys and select additional keyframes
   * based on the keys found to be selected above
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    AnimData *adt = ANIM_nla_mapping_get(ac, ale);

    /* set frame for validation callback to refer to */
    if (adt) {
      ked.f1 = BKE_nla_tweakedit_remap(adt, selx, NLATIME_CONVERT_UNMAP);
    }
    else {
      ked.f1 = selx;
    }

    /* select elements with frame number matching cfra */
    ANIM_fcurve_keyframes_loop(
        &ked, static_cast<FCurve *>(ale->key_data), ok_cb, select_cb, nullptr);
  }

  /* free elements */
  MEM_freeN(nvi);
  BLI_freelistN(&ked.list);
  ANIM_animdata_freelist(&anim_data);

  return run_modal ? OPERATOR_RUNNING_MODAL : OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Click Select Operator
 * \{ */

static int graphkeys_clickselect_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* select mode is either replace (deselect all, then add) or add/extend */
  const short selectmode = RNA_boolean_get(op->ptr, "extend") ? SELECT_INVERT : SELECT_REPLACE;
  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");
  /* See #WM_operator_properties_generic_select() for a detailed description of the how and why of
   * this. */
  const bool wait_to_deselect_others = RNA_boolean_get(op->ptr, "wait_to_deselect_others");
  int mval[2];
  int ret_val;

  mval[0] = RNA_int_get(op->ptr, "mouse_x");
  mval[1] = RNA_int_get(op->ptr, "mouse_y");

  /* figure out action to take */
  if (RNA_boolean_get(op->ptr, "column")) {
    /* select all keyframes in the same frame as the one that was under the mouse */
    ret_val = graphkeys_mselect_column(
        &ac, mval, eEditKeyframes_Select(selectmode), wait_to_deselect_others);
  }
  else if (RNA_boolean_get(op->ptr, "curves")) {
    /* select all keyframes in the same F-Curve as the one under the mouse */
    ret_val = mouse_graph_keys(
        &ac, mval, eEditKeyframes_Select(selectmode), deselect_all, true, wait_to_deselect_others);
  }
  else {
    /* select keyframe under mouse */
    ret_val = mouse_graph_keys(&ac,
                               mval,
                               eEditKeyframes_Select(selectmode),
                               deselect_all,
                               false,
                               wait_to_deselect_others);
  }

  /* set notifier that keyframe selection (and also channel selection in some cases) has
   * changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED, nullptr);

  /* for tweak grab to work */
  return ret_val | OPERATOR_PASS_THROUGH;
}

void GRAPH_OT_clickselect(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select Keyframes";
  ot->idname = "GRAPH_OT_clickselect";
  ot->description = "Select keyframes by clicking on them";

  /* callbacks */
  ot->poll = graphop_visible_keyframes_poll;
  ot->exec = graphkeys_clickselect_exec;
  ot->invoke = WM_generic_select_invoke;
  ot->modal = WM_generic_select_modal;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_generic_select(ot);

  /* Key-map: Enable with `Shift`. */
  prop = RNA_def_boolean(ot->srna,
                         "extend",
                         false,
                         "Extend Select",
                         "Toggle keyframe selection instead of leaving newly selected "
                         "keyframes only");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "deselect_all",
                         false,
                         "Deselect On Nothing",
                         "Deselect all when nothing under the cursor");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* Key-map: Enable with `Alt`. */
  prop = RNA_def_boolean(ot->srna,
                         "column",
                         false,
                         "Column Select",
                         "Select all keyframes that occur on the same frame as the one under "
                         "the mouse");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* Key-map: Enable with `Ctrl-Atl`. */
  prop = RNA_def_boolean(
      ot->srna, "curves", false, "Only Curves", "Select all the keyframes in the curve");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Key/handles Selection Operator
 * \{ */

/* Defines for key/handles select tool. */
static const EnumPropertyItem prop_graphkeys_select_key_handles_actions[] = {
    {GRAPHKEYS_KEYHANDLESSEL_SELECT, "SELECT", 0, "Select", ""},
    {GRAPHKEYS_KEYHANDLESSEL_DESELECT, "DESELECT", 0, "Deselect", ""},
    {GRAPHKEYS_KEYHANDLESSEL_KEEP, "KEEP", 0, "Keep", "Leave as is"},
    {0, nullptr, 0, nullptr, nullptr},
};

/**
 * Select/deselect different parts (e.g. left/right handles) of already-selected keys.
 *
 * The *_action parameters determine what action to take on each part of
 * a key: select, deselect, or keep (do nothing).
 *
 * \param left_handle_action: selection action to perform on left handles.
 * \param key_action: selection action to perform on the keys themselves.
 * \param right_handle_action: selection action to perform on right handles.
 */
static void graphkeys_select_key_handles(
    bAnimContext *ac,
    const enum eGraphKey_SelectKeyHandles_Action left_handle_action,
    const enum eGraphKey_SelectKeyHandles_Action key_action,
    const enum eGraphKey_SelectKeyHandles_Action right_handle_action)
{
  ListBase anim_data = {nullptr, nullptr};

  const eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE |
                                    ANIMFILTER_FCURVESONLY | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, filter, ac->data, static_cast<eAnimCont_Types>(ac->datatype));
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    BLI_assert(ale->type & ANIMTYPE_FCURVE);
    FCurve *fcu = (FCurve *)ale->key_data;

    /* Only continue if F-Curve has keyframes. */
    if (fcu->bezt == nullptr) {
      continue;
    }

    for (int i = 0; i < fcu->totvert; i++) {
      BezTriple *bezt = &fcu->bezt[i];

      if (!BEZT_ISSEL_ANY(bezt)) {
        continue;
      }

      switch (left_handle_action) {
        case GRAPHKEYS_KEYHANDLESSEL_SELECT:
          BEZT_SEL_IDX(bezt, 0);
          break;
        case GRAPHKEYS_KEYHANDLESSEL_DESELECT:
          BEZT_DESEL_IDX(bezt, 0);
          break;
        case GRAPHKEYS_KEYHANDLESSEL_KEEP:
          /* Do nothing. */
          break;
      }

      switch (key_action) {
        case GRAPHKEYS_KEYHANDLESSEL_SELECT:
          BEZT_SEL_IDX(bezt, 1);
          break;
        case GRAPHKEYS_KEYHANDLESSEL_DESELECT:
          BEZT_DESEL_IDX(bezt, 1);
          break;
        case GRAPHKEYS_KEYHANDLESSEL_KEEP:
          /* Do nothing. */
          break;
      }

      switch (right_handle_action) {
        case GRAPHKEYS_KEYHANDLESSEL_SELECT:
          BEZT_SEL_IDX(bezt, 2);
          break;
        case GRAPHKEYS_KEYHANDLESSEL_DESELECT:
          BEZT_DESEL_IDX(bezt, 2);
          break;
        case GRAPHKEYS_KEYHANDLESSEL_KEEP:
          /* Do nothing. */
          break;
      }
    }
  }

  /* Cleanup */
  ANIM_animdata_freelist(&anim_data);
}

static int graphkeys_select_key_handles_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  const eGraphKey_SelectKeyHandles_Action left_handle_action =
      static_cast<eGraphKey_SelectKeyHandles_Action>(RNA_enum_get(op->ptr, "left_handle_action"));
  const eGraphKey_SelectKeyHandles_Action key_action =
      static_cast<eGraphKey_SelectKeyHandles_Action>(RNA_enum_get(op->ptr, "key_action"));
  const eGraphKey_SelectKeyHandles_Action right_handle_action =
      static_cast<eGraphKey_SelectKeyHandles_Action>(RNA_enum_get(op->ptr, "right_handle_action"));

  graphkeys_select_key_handles(&ac, left_handle_action, key_action, right_handle_action);

  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_select_key_handles(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Key / Handles";
  ot->idname = "GRAPH_OT_select_key_handles";
  ot->description =
      "For selected keyframes, select/deselect any combination of the key itself and its handles";

  /* callbacks */
  ot->poll = graphop_visible_keyframes_poll;
  ot->exec = graphkeys_select_key_handles_exec;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  RNA_def_enum(ot->srna,
               "left_handle_action",
               prop_graphkeys_select_key_handles_actions,
               GRAPHKEYS_KEYHANDLESSEL_SELECT,
               "Left Handle",
               "Effect on the left handle");
  RNA_def_enum(ot->srna,
               "right_handle_action",
               prop_graphkeys_select_key_handles_actions,
               GRAPHKEYS_KEYHANDLESSEL_SELECT,
               "Right Handle",
               "Effect on the right handle");
  RNA_def_enum(ot->srna,
               "key_action",
               prop_graphkeys_select_key_handles_actions,
               GRAPHKEYS_KEYHANDLESSEL_KEEP,
               "Key",
               "Effect on the key itself");
}

/** \} */
