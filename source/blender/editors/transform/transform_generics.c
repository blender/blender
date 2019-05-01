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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edtransform
 */

#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_sys_types.h" /* for intptr_t support */

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_lattice_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_view3d_types.h"
#include "DNA_modifier_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_mask_types.h"
#include "DNA_meta_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "BLT_translation.h"

#include "RNA_access.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "BIK_api.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_editmesh.h"
#include "BKE_fcurve.h"
#include "BKE_gpencil.h"
#include "BKE_lattice.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_mask.h"
#include "BKE_nla.h"
#include "BKE_paint.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_tracking.h"
#include "BKE_workspace.h"

#include "DEG_depsgraph.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_image.h"
#include "ED_keyframing.h"
#include "ED_markers.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_particle.h"
#include "ED_screen_types.h"
#include "ED_space_api.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"
#include "ED_curve.h" /* for curve_editnurbs */
#include "ED_clip.h"
#include "ED_screen.h"
#include "ED_gpencil.h"

#include "WM_types.h"
#include "WM_api.h"

#include "RE_engine.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "transform.h"

/* ************************** Functions *************************** */

void getViewVector(const TransInfo *t, const float coord[3], float vec[3])
{
  if (t->persp != RV3D_ORTHO) {
    sub_v3_v3v3(vec, coord, t->viewinv[3]);
  }
  else {
    copy_v3_v3(vec, t->viewinv[2]);
  }
  normalize_v3(vec);
}

/* ************************** GENERICS **************************** */

static void clipMirrorModifier(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    Object *ob = tc->obedit;
    ModifierData *md = ob->modifiers.first;
    float tolerance[3] = {0.0f, 0.0f, 0.0f};
    int axis = 0;

    for (; md; md = md->next) {
      if ((md->type == eModifierType_Mirror) && (md->mode & eModifierMode_Realtime)) {
        MirrorModifierData *mmd = (MirrorModifierData *)md;

        if (mmd->flag & MOD_MIR_CLIPPING) {
          axis = 0;
          if (mmd->flag & MOD_MIR_AXIS_X) {
            axis |= 1;
            tolerance[0] = mmd->tolerance;
          }
          if (mmd->flag & MOD_MIR_AXIS_Y) {
            axis |= 2;
            tolerance[1] = mmd->tolerance;
          }
          if (mmd->flag & MOD_MIR_AXIS_Z) {
            axis |= 4;
            tolerance[2] = mmd->tolerance;
          }
          if (axis) {
            float mtx[4][4], imtx[4][4];
            int i;

            if (mmd->mirror_ob) {
              float obinv[4][4];

              invert_m4_m4(obinv, mmd->mirror_ob->obmat);
              mul_m4_m4m4(mtx, obinv, ob->obmat);
              invert_m4_m4(imtx, mtx);
            }

            TransData *td = tc->data;
            for (i = 0; i < tc->data_len; i++, td++) {
              int clip;
              float loc[3], iloc[3];

              if (td->flag & TD_NOACTION) {
                break;
              }
              if (td->loc == NULL) {
                break;
              }

              if (td->flag & TD_SKIP) {
                continue;
              }

              copy_v3_v3(loc, td->loc);
              copy_v3_v3(iloc, td->iloc);

              if (mmd->mirror_ob) {
                mul_m4_v3(mtx, loc);
                mul_m4_v3(mtx, iloc);
              }

              clip = 0;
              if (axis & 1) {
                if (fabsf(iloc[0]) <= tolerance[0] || loc[0] * iloc[0] < 0.0f) {
                  loc[0] = 0.0f;
                  clip = 1;
                }
              }

              if (axis & 2) {
                if (fabsf(iloc[1]) <= tolerance[1] || loc[1] * iloc[1] < 0.0f) {
                  loc[1] = 0.0f;
                  clip = 1;
                }
              }
              if (axis & 4) {
                if (fabsf(iloc[2]) <= tolerance[2] || loc[2] * iloc[2] < 0.0f) {
                  loc[2] = 0.0f;
                  clip = 1;
                }
              }
              if (clip) {
                if (mmd->mirror_ob) {
                  mul_m4_v3(imtx, loc);
                }
                copy_v3_v3(td->loc, loc);
              }
            }
          }
        }
      }
    }
  }
}

/* assumes obedit set to mesh object */
static void editbmesh_apply_to_mirror(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->mirror.axis_flag) {
      TransData *td = tc->data;
      BMVert *eve;
      int i;

      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_NOACTION) {
          break;
        }
        if (td->loc == NULL) {
          break;
        }
        if (td->flag & TD_SKIP) {
          continue;
        }

        eve = td->extra;
        if (eve) {
          eve->co[0] = -td->loc[0];
          eve->co[1] = td->loc[1];
          eve->co[2] = td->loc[2];
        }

        if (td->flag & TD_MIRROR_EDGE) {
          td->loc[0] = 0;
        }
      }
    }
  }
}

/* for the realtime animation recording feature, handle overlapping data */
static void animrecord_check_state(Scene *scene, ID *id, wmTimer *animtimer)
{
  ScreenAnimData *sad = (animtimer) ? animtimer->customdata : NULL;

  /* sanity checks */
  if (ELEM(NULL, scene, id, sad)) {
    return;
  }

  /* check if we need a new strip if:
   * - if animtimer is running
   * - we're not only keying for available channels
   * - the option to add new actions for each round is not enabled
   */
  if (IS_AUTOKEY_FLAG(scene, INSERTAVAIL) == 0 &&
      (scene->toolsettings->autokey_flag & ANIMRECORD_FLAG_WITHNLA)) {
    /* if playback has just looped around,
     * we need to add a new NLA track+strip to allow a clean pass to occur */
    if ((sad) && (sad->flag & ANIMPLAY_FLAG_JUMPED)) {
      AnimData *adt = BKE_animdata_from_id(id);
      const bool is_first = (adt) && (adt->nla_tracks.first == NULL);

      /* perform push-down manually with some differences
       * NOTE: BKE_nla_action_pushdown() sync warning...
       */
      if ((adt->action) && !(adt->flag & ADT_NLA_EDIT_ON)) {
        float astart, aend;

        /* only push down if action is more than 1-2 frames long */
        calc_action_range(adt->action, &astart, &aend, 1);
        if (aend > astart + 2.0f) {
          NlaStrip *strip = BKE_nlastack_add_strip(adt, adt->action);

          /* clear reference to action now that we've pushed it onto the stack */
          id_us_min(&adt->action->id);
          adt->action = NULL;

          /* adjust blending + extend so that they will behave correctly */
          strip->extendmode = NLASTRIP_EXTEND_NOTHING;
          strip->flag &= ~(NLASTRIP_FLAG_AUTO_BLENDS | NLASTRIP_FLAG_SELECT |
                           NLASTRIP_FLAG_ACTIVE);

          /* copy current "action blending" settings from adt to the strip,
           * as it was keyframed with these settings, so omitting them will
           * change the effect  [T54766]
           */
          if (is_first == false) {
            strip->blendmode = adt->act_blendmode;
            strip->influence = adt->act_influence;

            if (adt->act_influence < 1.0f) {
              /* enable "user-controlled" influence (which will insert a default keyframe)
               * so that the influence doesn't get lost on the new update
               *
               * NOTE: An alternative way would have been to instead hack the influence
               * to not get always get reset to full strength if NLASTRIP_FLAG_USR_INFLUENCE
               * is disabled but auto-blending isn't being used. However, that approach
               * is a bit hacky/hard to discover, and may cause backwards compatibility issues,
               * so it's better to just do it this way.
               */
              strip->flag |= NLASTRIP_FLAG_USR_INFLUENCE;
              BKE_nlastrip_validate_fcurves(strip);
            }
          }

          /* also, adjust the AnimData's action extend mode to be on
           * 'nothing' so that previous result still play
           */
          adt->act_extendmode = NLASTRIP_EXTEND_NOTHING;
        }
      }
    }
  }
}

static bool fcu_test_selected(FCurve *fcu)
{
  BezTriple *bezt = fcu->bezt;
  unsigned int i;

  if (bezt == NULL) { /* ignore baked */
    return 0;
  }

  for (i = 0; i < fcu->totvert; i++, bezt++) {
    if (BEZT_ISSEL_ANY(bezt)) {
      return 1;
    }
  }

  return 0;
}

/* helper for recalcData() - for Action Editor transforms */
static void recalcData_actedit(TransInfo *t)
{
  ViewLayer *view_layer = t->view_layer;
  SpaceAction *saction = (SpaceAction *)t->sa->spacedata.first;

  bAnimContext ac = {NULL};
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* initialize relevant anim-context 'context' data from TransInfo data */
  /* NOTE: sync this with the code in ANIM_animdata_get_context() */
  ac.bmain = CTX_data_main(t->context);
  ac.scene = t->scene;
  ac.view_layer = t->view_layer;
  ac.obact = OBACT(view_layer);
  ac.sa = t->sa;
  ac.ar = t->ar;
  ac.sl = (t->sa) ? t->sa->spacedata.first : NULL;
  ac.spacetype = (t->sa) ? t->sa->spacetype : 0;
  ac.regiontype = (t->ar) ? t->ar->regiontype : 0;

  ANIM_animdata_context_getdata(&ac);

  /* perform flush */
  if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
    /* flush transform values back to actual coordinates */
    flushTransIntFrameActionData(t);
  }

  if (ac.datatype != ANIMCONT_MASK) {
    /* Get animdata blocks visible in editor,
     * assuming that these will be the ones where things changed. */
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_ANIMDATA);
    ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

    /* just tag these animdata-blocks to recalc, assuming that some data there changed
     * BUT only do this if realtime updates are enabled
     */
    if ((saction->flag & SACTION_NOREALTIMEUPDATES) == 0) {
      for (ale = anim_data.first; ale; ale = ale->next) {
        /* set refresh tags for objects using this animation */
        ANIM_list_elem_update(CTX_data_main(t->context), t->scene, ale);
      }
    }

    /* now free temp channels */
    ANIM_animdata_freelist(&anim_data);
  }
}
/* helper for recalcData() - for Graph Editor transforms */
static void recalcData_graphedit(TransInfo *t)
{
  SpaceGraph *sipo = (SpaceGraph *)t->sa->spacedata.first;
  ViewLayer *view_layer = t->view_layer;

  ListBase anim_data = {NULL, NULL};
  bAnimContext ac = {NULL};
  int filter;

  bAnimListElem *ale;
  int dosort = 0;

  /* initialize relevant anim-context 'context' data from TransInfo data */
  /* NOTE: sync this with the code in ANIM_animdata_get_context() */
  ac.bmain = CTX_data_main(t->context);
  ac.scene = t->scene;
  ac.view_layer = t->view_layer;
  ac.obact = OBACT(view_layer);
  ac.sa = t->sa;
  ac.ar = t->ar;
  ac.sl = (t->sa) ? t->sa->spacedata.first : NULL;
  ac.spacetype = (t->sa) ? t->sa->spacetype : 0;
  ac.regiontype = (t->ar) ? t->ar->regiontype : 0;

  ANIM_animdata_context_getdata(&ac);

  /* do the flush first */
  flushTransGraphData(t);

  /* get curves to check if a re-sort is needed */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVE_VISIBLE);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* now test if there is a need to re-sort */
  for (ale = anim_data.first; ale; ale = ale->next) {
    FCurve *fcu = (FCurve *)ale->key_data;

    /* ignore FC-Curves without any selected verts */
    if (!fcu_test_selected(fcu)) {
      continue;
    }

    /* watch it: if the time is wrong: do not correct handles yet */
    if (test_time_fcurve(fcu)) {
      dosort++;
    }
    else {
      calchandles_fcurve(fcu);
    }

    /* set refresh tags for objects using this animation,
     * BUT only if realtime updates are enabled
     */
    if ((sipo->flag & SIPO_NOREALTIMEUPDATES) == 0) {
      ANIM_list_elem_update(CTX_data_main(t->context), t->scene, ale);
    }
  }

  /* do resort and other updates? */
  if (dosort) {
    remake_graph_transdata(t, &anim_data);
  }

  /* now free temp channels */
  ANIM_animdata_freelist(&anim_data);
}

/* helper for recalcData() - for NLA Editor transforms */
static void recalcData_nla(TransInfo *t)
{
  SpaceNla *snla = (SpaceNla *)t->sa->spacedata.first;
  Scene *scene = t->scene;
  double secf = FPS;
  int i;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  TransDataNla *tdn = tc->custom.type.data;

  /* For each strip we've got, perform some additional validation of the values
   * that got set before using RNA to set the value (which does some special
   * operations when setting these values to make sure that everything works ok).
   */
  for (i = 0; i < tc->data_len; i++, tdn++) {
    NlaStrip *strip = tdn->strip;
    PointerRNA strip_ptr;
    short pExceeded, nExceeded, iter;
    int delta_y1, delta_y2;

    /* if this tdn has no handles, that means it is just a dummy that should be skipped */
    if (tdn->handle == 0) {
      continue;
    }

    /* set refresh tags for objects using this animation,
     * BUT only if realtime updates are enabled
     */
    if ((snla->flag & SNLA_NOREALTIMEUPDATES) == 0) {
      ANIM_id_update(CTX_data_main(t->context), tdn->id);
    }

    /* if canceling transform, just write the values without validating, then move on */
    if (t->state == TRANS_CANCEL) {
      /* clear the values by directly overwriting the originals, but also need to restore
       * endpoints of neighboring transition-strips
       */

      /* start */
      strip->start = tdn->h1[0];

      if ((strip->prev) && (strip->prev->type == NLASTRIP_TYPE_TRANSITION)) {
        strip->prev->end = tdn->h1[0];
      }

      /* end */
      strip->end = tdn->h2[0];

      if ((strip->next) && (strip->next->type == NLASTRIP_TYPE_TRANSITION)) {
        strip->next->start = tdn->h2[0];
      }

      /* flush transforms to child strips (since this should be a meta) */
      BKE_nlameta_flush_transforms(strip);

      /* restore to original track (if needed) */
      if (tdn->oldTrack != tdn->nlt) {
        /* Just append to end of list for now,
         * since strips get sorted in special_aftertrans_update(). */
        BLI_remlink(&tdn->nlt->strips, strip);
        BLI_addtail(&tdn->oldTrack->strips, strip);
      }

      continue;
    }

    /* firstly, check if the proposed transform locations would overlap with any neighboring strips
     * (barring transitions) which are absolute barriers since they are not being moved
     *
     * this is done as a iterative procedure (done 5 times max for now)
     */
    for (iter = 0; iter < 5; iter++) {
      pExceeded = ((strip->prev) && (strip->prev->type != NLASTRIP_TYPE_TRANSITION) &&
                   (tdn->h1[0] < strip->prev->end));
      nExceeded = ((strip->next) && (strip->next->type != NLASTRIP_TYPE_TRANSITION) &&
                   (tdn->h2[0] > strip->next->start));

      if ((pExceeded && nExceeded) || (iter == 4)) {
        /* both endpoints exceeded (or iteration ping-pong'd meaning that we need a compromise)
         * - Simply crop strip to fit within the bounds of the strips bounding it
         * - If there were no neighbors, clear the transforms
         *   (make it default to the strip's current values).
         */
        if (strip->prev && strip->next) {
          tdn->h1[0] = strip->prev->end;
          tdn->h2[0] = strip->next->start;
        }
        else {
          tdn->h1[0] = strip->start;
          tdn->h2[0] = strip->end;
        }
      }
      else if (nExceeded) {
        /* move backwards */
        float offset = tdn->h2[0] - strip->next->start;

        tdn->h1[0] -= offset;
        tdn->h2[0] -= offset;
      }
      else if (pExceeded) {
        /* more forwards */
        float offset = strip->prev->end - tdn->h1[0];

        tdn->h1[0] += offset;
        tdn->h2[0] += offset;
      }
      else { /* all is fine and well */
        break;
      }
    }

    /* handle auto-snapping
     * NOTE: only do this when transform is still running, or we can't restore
     */
    if (t->state != TRANS_CANCEL) {
      switch (snla->autosnap) {
        case SACTSNAP_FRAME: /* snap to nearest frame */
        case SACTSNAP_STEP:  /* frame step - this is basically the same,
                              * since we don't have any remapping going on */
        {
          tdn->h1[0] = floorf(tdn->h1[0] + 0.5f);
          tdn->h2[0] = floorf(tdn->h2[0] + 0.5f);
          break;
        }

        case SACTSNAP_SECOND: /* snap to nearest second */
        case SACTSNAP_TSTEP:  /* second step - this is basically the same,
                               * since we don't have any remapping going on */
        {
          /* This case behaves differently from the rest, since lengths of strips
           * may not be multiples of a second. If we just naively resize adjust
           * the handles, things may not work correctly. Instead, we only snap
           * the first handle, and move the other to fit.
           *
           * FIXME: we do run into problems here when user attempts to negatively
           *        scale the strip, as it then just compresses down and refuses
           *        to expand out the other end.
           */
          float h1_new = (float)(floor(((double)tdn->h1[0] / secf) + 0.5) * secf);
          float delta = h1_new - tdn->h1[0];

          tdn->h1[0] = h1_new;
          tdn->h2[0] += delta;
          break;
        }

        case SACTSNAP_MARKER: /* snap to nearest marker */
        {
          tdn->h1[0] = (float)ED_markers_find_nearest_marker_time(&t->scene->markers, tdn->h1[0]);
          tdn->h2[0] = (float)ED_markers_find_nearest_marker_time(&t->scene->markers, tdn->h2[0]);
          break;
        }
      }
    }

    /* Use RNA to write the values to ensure that constraints on these are obeyed
     * (e.g. for transition strips, the values are taken from the neighbors)
     *
     * NOTE: we write these twice to avoid truncation errors which can arise when
     * moving the strips a large distance using numeric input [#33852]
     */
    RNA_pointer_create(NULL, &RNA_NlaStrip, strip, &strip_ptr);

    RNA_float_set(&strip_ptr, "frame_start", tdn->h1[0]);
    RNA_float_set(&strip_ptr, "frame_end", tdn->h2[0]);

    RNA_float_set(&strip_ptr, "frame_start", tdn->h1[0]);
    RNA_float_set(&strip_ptr, "frame_end", tdn->h2[0]);

    /* flush transforms to child strips (since this should be a meta) */
    BKE_nlameta_flush_transforms(strip);

    /* Now, check if we need to try and move track:
     * - we need to calculate both,
     *   as only one may have been altered by transform if only 1 handle moved.
     */
    delta_y1 = ((int)tdn->h1[1] / NLACHANNEL_STEP(snla) - tdn->trackIndex);
    delta_y2 = ((int)tdn->h2[1] / NLACHANNEL_STEP(snla) - tdn->trackIndex);

    if (delta_y1 || delta_y2) {
      NlaTrack *track;
      int delta = (delta_y2) ? delta_y2 : delta_y1;
      int n;

      /* Move in the requested direction,
       * checking at each layer if there's space for strip to pass through,
       * stopping on the last track available or that we're able to fit in.
       */
      if (delta > 0) {
        for (track = tdn->nlt->next, n = 0; (track) && (n < delta); track = track->next, n++) {
          /* check if space in this track for the strip */
          if (BKE_nlatrack_has_space(track, strip->start, strip->end)) {
            /* move strip to this track */
            BLI_remlink(&tdn->nlt->strips, strip);
            BKE_nlatrack_add_strip(track, strip);

            tdn->nlt = track;
            tdn->trackIndex++;
          }
          else { /* can't move any further */
            break;
          }
        }
      }
      else {
        /* make delta 'positive' before using it, since we now know to go backwards */
        delta = -delta;

        for (track = tdn->nlt->prev, n = 0; (track) && (n < delta); track = track->prev, n++) {
          /* check if space in this track for the strip */
          if (BKE_nlatrack_has_space(track, strip->start, strip->end)) {
            /* move strip to this track */
            BLI_remlink(&tdn->nlt->strips, strip);
            BKE_nlatrack_add_strip(track, strip);

            tdn->nlt = track;
            tdn->trackIndex--;
          }
          else { /* can't move any further */
            break;
          }
        }
      }
    }
  }
}

static void recalcData_mask_common(TransInfo *t)
{
  Mask *mask = CTX_data_edit_mask(t->context);

  flushTransMasking(t);

  DEG_id_tag_update(&mask->id, 0);
}

/* helper for recalcData() - for Image Editor transforms */
static void recalcData_image(TransInfo *t)
{
  if (t->options & CTX_MASK) {
    recalcData_mask_common(t);
  }
  else if (t->options & CTX_PAINT_CURVE) {
    flushTransPaintCurve(t);
  }
  else if ((t->flag & T_EDIT) && t->obedit_type == OB_MESH) {
    SpaceImage *sima = t->sa->spacedata.first;

    flushTransUVs(t);
    if (sima->flag & SI_LIVE_UNWRAP) {
      ED_uvedit_live_unwrap_re_solve();
    }

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      if (tc->data_len) {
        DEG_id_tag_update(tc->obedit->data, 0);
      }
    }
  }
}

/* helper for recalcData() - for Movie Clip transforms */
static void recalcData_spaceclip(TransInfo *t)
{
  SpaceClip *sc = t->sa->spacedata.first;

  if (ED_space_clip_check_show_trackedit(sc)) {
    MovieClip *clip = ED_space_clip_get_clip(sc);
    ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
    MovieTrackingTrack *track;
    int framenr = ED_space_clip_get_clip_frame_number(sc);

    flushTransTracking(t);

    track = tracksbase->first;
    while (track) {
      if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
        MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

        if (t->mode == TFM_TRANSLATION) {
          if (TRACK_AREA_SELECTED(track, TRACK_AREA_PAT)) {
            BKE_tracking_marker_clamp(marker, CLAMP_PAT_POS);
          }
          if (TRACK_AREA_SELECTED(track, TRACK_AREA_SEARCH)) {
            BKE_tracking_marker_clamp(marker, CLAMP_SEARCH_POS);
          }
        }
        else if (t->mode == TFM_RESIZE) {
          if (TRACK_AREA_SELECTED(track, TRACK_AREA_PAT)) {
            BKE_tracking_marker_clamp(marker, CLAMP_PAT_DIM);
          }
          if (TRACK_AREA_SELECTED(track, TRACK_AREA_SEARCH)) {
            BKE_tracking_marker_clamp(marker, CLAMP_SEARCH_DIM);
          }
        }
        else if (t->mode == TFM_ROTATION) {
          if (TRACK_AREA_SELECTED(track, TRACK_AREA_PAT)) {
            BKE_tracking_marker_clamp(marker, CLAMP_PAT_POS);
          }
        }
      }

      track = track->next;
    }

    DEG_id_tag_update(&clip->id, 0);
  }
  else if (t->options & CTX_MASK) {
    recalcData_mask_common(t);
  }
}

/* helper for recalcData() - for object transforms, typically in the 3D view */
static void recalcData_objects(TransInfo *t)
{
  Base *base = t->view_layer->basact;

  if (t->obedit_type != -1) {
    if (ELEM(t->obedit_type, OB_CURVE, OB_SURF)) {

      if (t->state != TRANS_CANCEL) {
        clipMirrorModifier(t);
        applyProject(t);
      }

      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        Curve *cu = tc->obedit->data;
        ListBase *nurbs = BKE_curve_editNurbs_get(cu);
        Nurb *nu = nurbs->first;

        DEG_id_tag_update(tc->obedit->data, 0); /* sets recalc flags */

        if (t->state == TRANS_CANCEL) {
          while (nu) {
            /* Cant do testhandlesNurb here, it messes up the h1 and h2 flags */
            BKE_nurb_handles_calc(nu);
            nu = nu->next;
          }
        }
        else {
          /* Normal updating */
          while (nu) {
            BKE_nurb_test_2d(nu);
            BKE_nurb_handles_calc(nu);
            nu = nu->next;
          }
        }
      }
    }
    else if (t->obedit_type == OB_LATTICE) {

      if (t->state != TRANS_CANCEL) {
        applyProject(t);
      }

      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        Lattice *la = tc->obedit->data;
        DEG_id_tag_update(tc->obedit->data, 0); /* sets recalc flags */
        if (la->editlatt->latt->flag & LT_OUTSIDE) {
          outside_lattice(la->editlatt->latt);
        }
      }
    }
    else if (t->obedit_type == OB_MESH) {
      /* mirror modifier clipping? */
      if (t->state != TRANS_CANCEL) {
        /* apply clipping after so we never project past the clip plane [#25423] */
        applyProject(t);
        clipMirrorModifier(t);
      }
      if ((t->flag & T_NO_MIRROR) == 0 && (t->options & CTX_NO_MIRROR) == 0) {
        editbmesh_apply_to_mirror(t);
      }

      if (t->mode == TFM_EDGE_SLIDE) {
        projectEdgeSlideData(t, false);
      }
      else if (t->mode == TFM_VERT_SLIDE) {
        projectVertSlideData(t, false);
      }

      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        DEG_id_tag_update(tc->obedit->data, 0); /* sets recalc flags */
        BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
        EDBM_mesh_normals_update(em);
        BKE_editmesh_tessface_calc(em);
      }
    }
    else if (t->obedit_type == OB_ARMATURE) { /* no recalc flag, does pose */

      if (t->state != TRANS_CANCEL) {
        applyProject(t);
      }

      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        bArmature *arm = tc->obedit->data;
        ListBase *edbo = arm->edbo;
        EditBone *ebo, *ebo_parent;
        TransData *td = tc->data;
        int i;

        /* Ensure all bones are correctly adjusted */
        for (ebo = edbo->first; ebo; ebo = ebo->next) {
          ebo_parent = (ebo->flag & BONE_CONNECTED) ? ebo->parent : NULL;

          if (ebo_parent) {
            /* If this bone has a parent tip that has been moved */
            if (ebo_parent->flag & BONE_TIPSEL) {
              copy_v3_v3(ebo->head, ebo_parent->tail);
              if (t->mode == TFM_BONE_ENVELOPE) {
                ebo->rad_head = ebo_parent->rad_tail;
              }
            }
            /* If this bone has a parent tip that has NOT been moved */
            else {
              copy_v3_v3(ebo_parent->tail, ebo->head);
              if (t->mode == TFM_BONE_ENVELOPE) {
                ebo_parent->rad_tail = ebo->rad_head;
              }
            }
          }

          /* on extrude bones, oldlength==0.0f, so we scale radius of points */
          ebo->length = len_v3v3(ebo->head, ebo->tail);
          if (ebo->oldlength == 0.0f) {
            ebo->rad_head = 0.25f * ebo->length;
            ebo->rad_tail = 0.10f * ebo->length;
            ebo->dist = 0.25f * ebo->length;
            if (ebo->parent) {
              if (ebo->rad_head > ebo->parent->rad_tail) {
                ebo->rad_head = ebo->parent->rad_tail;
              }
            }
          }
          else if (t->mode != TFM_BONE_ENVELOPE) {
            /* if bones change length, lets do that for the deform distance as well */
            ebo->dist *= ebo->length / ebo->oldlength;
            ebo->rad_head *= ebo->length / ebo->oldlength;
            ebo->rad_tail *= ebo->length / ebo->oldlength;
            ebo->oldlength = ebo->length;

            if (ebo_parent) {
              ebo_parent->rad_tail = ebo->rad_head;
            }
          }
        }

        if (!ELEM(
                t->mode, TFM_BONE_ROLL, TFM_BONE_ENVELOPE, TFM_BONE_ENVELOPE_DIST, TFM_BONESIZE)) {
          /* fix roll */
          for (i = 0; i < tc->data_len; i++, td++) {
            if (td->extra) {
              float vec[3], up_axis[3];
              float qrot[4];
              float roll;

              ebo = td->extra;

              if (t->state == TRANS_CANCEL) {
                /* restore roll */
                ebo->roll = td->ival;
              }
              else {
                copy_v3_v3(up_axis, td->axismtx[2]);

                sub_v3_v3v3(vec, ebo->tail, ebo->head);
                normalize_v3(vec);
                rotation_between_vecs_to_quat(qrot, td->axismtx[1], vec);
                mul_qt_v3(qrot, up_axis);

                /* roll has a tendency to flip in certain orientations - [#34283], [#33974] */
                roll = ED_armature_ebone_roll_to_vector(ebo, up_axis, false);
                ebo->roll = angle_compat_rad(roll, td->ival);
              }
            }
          }
        }

        if (arm->flag & ARM_MIRROR_EDIT) {
          if (t->state != TRANS_CANCEL) {
            ED_armature_edit_transform_mirror_update(tc->obedit);
          }
          else {
            restoreBones(tc);
          }
        }
      }
    }
    else {
      if (t->state != TRANS_CANCEL) {
        applyProject(t);
      }
      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        if (tc->data_len) {
          DEG_id_tag_update(tc->obedit->data, 0); /* sets recalc flags */
        }
      }
    }
  }
  else if (t->flag & T_POSE && (t->mode == TFM_BONESIZE)) {
    /* Handle the exception where for TFM_BONESIZE in edit mode we pretend to be
     * in pose mode (to use bone orientation matrix),
     * in that case we have to do mirroring as well. */
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      Object *ob = tc->poseobj;
      bArmature *arm = ob->data;
      if (arm->flag & ARM_MIRROR_EDIT) {
        if (t->state != TRANS_CANCEL) {
          ED_armature_edit_transform_mirror_update(ob);
        }
        else {
          restoreBones(tc);
        }
      }
    }
  }
  else if (t->flag & T_POSE) {
    GSet *motionpath_updates = BLI_gset_ptr_new("motionpath updates");

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      Object *ob = tc->poseobj;
      bArmature *arm = ob->data;

      /* if animtimer is running, and the object already has animation data,
       * check if the auto-record feature means that we should record 'samples'
       * (i.e. un-editable animation values)
       *
       * context is needed for keying set poll() functions.
       */

      /* TODO: autokeyframe calls need some setting to specify to add samples
       * (FPoints) instead of keyframes? */
      if ((t->animtimer) && (t->context) && IS_AUTOKEY_ON(t->scene)) {
        int targetless_ik =
            (t->flag & T_AUTOIK);  // XXX this currently doesn't work, since flags aren't set yet!

        animrecord_check_state(t->scene, &ob->id, t->animtimer);
        autokeyframe_pose(t->context, t->scene, ob, t->mode, targetless_ik);
      }

      if (motionpath_need_update_pose(t->scene, ob)) {
        BLI_gset_insert(motionpath_updates, ob);
      }

      /* old optimize trick... this enforces to bypass the depgraph */
      if (!(arm->flag & ARM_DELAYDEFORM)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY); /* sets recalc flags */
        /* transformation of pose may affect IK tree, make sure it is rebuilt */
        BIK_clear_data(ob->pose);
      }
      else {
        BKE_pose_where_is(t->depsgraph, t->scene, ob);
      }
    }

    /* Update motion paths once for all transformed bones in an object. */
    GSetIterator gs_iter;
    GSET_ITER (gs_iter, motionpath_updates) {
      Object *ob = BLI_gsetIterator_getKey(&gs_iter);
      ED_pose_recalculate_paths(t->context, t->scene, ob, true);
    }
    BLI_gset_free(motionpath_updates, NULL);
  }
  else if (base && (base->object->mode & OB_MODE_PARTICLE_EDIT) &&
           PE_get_current(t->scene, base->object)) {
    if (t->state != TRANS_CANCEL) {
      applyProject(t);
    }
    flushTransParticles(t);
  }
  else {
    bool motionpath_update = false;

    if (t->state != TRANS_CANCEL) {
      applyProject(t);
    }

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;

      for (int i = 0; i < tc->data_len; i++, td++) {
        Object *ob = td->ob;

        if (td->flag & TD_NOACTION) {
          break;
        }

        if (td->flag & TD_SKIP) {
          continue;
        }

        /* if animtimer is running, and the object already has animation data,
         * check if the auto-record feature means that we should record 'samples'
         * (i.e. uneditable animation values)
         */
        /* TODO: autokeyframe calls need some setting to specify to add samples
         * (FPoints) instead of keyframes? */
        if ((t->animtimer) && IS_AUTOKEY_ON(t->scene)) {
          animrecord_check_state(t->scene, &ob->id, t->animtimer);
          autokeyframe_object(t->context, t->scene, t->view_layer, ob, t->mode);
        }

        motionpath_update |= motionpath_need_update_object(t->scene, ob);

        /* sets recalc flags fully, instead of flushing existing ones
         * otherwise proxies don't function correctly
         */
        DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);

        if (t->flag & T_TEXTURE) {
          DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
        }
      }
    }

    if (motionpath_update) {
      /* Update motion paths once for all transformed objects. */
      ED_objects_recalculate_paths(t->context, t->scene, true);
    }
  }
}

static void recalcData_cursor(TransInfo *t)
{
  DEG_id_tag_update(&t->scene->id, ID_RECALC_COPY_ON_WRITE);
}

/* helper for recalcData() - for sequencer transforms */
static void recalcData_sequencer(TransInfo *t)
{
  TransData *td;
  int a;
  Sequence *seq_prev = NULL;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  for (a = 0, td = tc->data; a < tc->data_len; a++, td++) {
    TransDataSeq *tdsq = (TransDataSeq *)td->extra;
    Sequence *seq = tdsq->seq;

    if (seq != seq_prev) {
      if (BKE_sequence_tx_fullupdate_test(seq)) {
        BKE_sequence_invalidate_cache(t->scene, seq);
      }
      else {
        BKE_sequence_invalidate_cache(t->scene, seq);
      }
    }

    seq_prev = seq;
  }

  DEG_id_tag_update(&t->scene->id, ID_RECALC_SEQUENCER);

  flushTransSeq(t);
}

/* force recalculation of triangles during transformation */
static void recalcData_gpencil_strokes(TransInfo *t)
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  TransData *td = tc->data;
  for (int i = 0; i < tc->data_len; i++, td++) {
    bGPDstroke *gps = td->extra;
    if (gps != NULL) {
      gps->flag |= GP_STROKE_RECALC_GEOMETRY;
    }
  }
}

/* called for updating while transform acts, once per redraw */
void recalcData(TransInfo *t)
{
  /* if tests must match createTransData for correct updates */
  if (t->options & CTX_CURSOR) {
    recalcData_cursor(t);
  }
  else if (t->options & CTX_TEXTURE) {
    recalcData_objects(t);
  }
  else if (t->options & CTX_EDGE) {
    recalcData_objects(t);
  }
  else if (t->options & CTX_PAINT_CURVE) {
    flushTransPaintCurve(t);
  }
  else if (t->options & CTX_GPENCIL_STROKES) {
    /* set recalc triangle cache flag */
    recalcData_gpencil_strokes(t);
  }
  else if (t->spacetype == SPACE_IMAGE) {
    recalcData_image(t);
  }
  else if (t->spacetype == SPACE_ACTION) {
    recalcData_actedit(t);
  }
  else if (t->spacetype == SPACE_NLA) {
    recalcData_nla(t);
  }
  else if (t->spacetype == SPACE_SEQ) {
    recalcData_sequencer(t);
  }
  else if (t->spacetype == SPACE_GRAPH) {
    recalcData_graphedit(t);
  }
  else if (t->spacetype == SPACE_NODE) {
    flushTransNodes(t);
  }
  else if (t->spacetype == SPACE_CLIP) {
    recalcData_spaceclip(t);
  }
  else {
    recalcData_objects(t);
  }
}

void drawLine(TransInfo *t, const float center[3], const float dir[3], char axis, short options)
{
  float v1[3], v2[3], v3[3];
  unsigned char col[3], col2[3];

  if (t->spacetype == SPACE_VIEW3D) {
    View3D *v3d = t->view;

    GPU_matrix_push();

    copy_v3_v3(v3, dir);
    mul_v3_fl(v3, v3d->clip_end);

    sub_v3_v3v3(v2, center, v3);
    add_v3_v3v3(v1, center, v3);

    if (options & DRAWLIGHT) {
      col[0] = col[1] = col[2] = 220;
    }
    else {
      UI_GetThemeColor3ubv(TH_GRID, col);
    }
    UI_make_axis_color(col, col2, axis);

    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor3ubv(col2);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex3fv(pos, v1);
    immVertex3fv(pos, v2);
    immEnd();

    immUnbindProgram();

    GPU_matrix_pop();
  }
}

/**
 * Free data before switching to another mode.
 */
void resetTransModal(TransInfo *t)
{
  freeTransCustomDataForMode(t);
}

void resetTransRestrictions(TransInfo *t)
{
  t->flag &= ~T_ALL_RESTRICTIONS;
}

static int initTransInfo_edit_pet_to_flag(const int proportional)
{
  int flag = 0;
  if (proportional & PROP_EDIT_USE) {
    flag |= T_PROP_EDIT;
  }
  if (proportional & PROP_EDIT_CONNECTED) {
    flag |= T_PROP_CONNECTED;
  }
  if (proportional & PROP_EDIT_PROJECTED) {
    flag |= T_PROP_PROJECTED;
  }
  return flag;
}

void initTransDataContainers_FromObjectData(TransInfo *t,
                                            Object *obact,
                                            Object **objects,
                                            uint objects_len)
{
  const eObjectMode object_mode = obact ? obact->mode : OB_MODE_OBJECT;
  const short object_type = obact ? obact->type : -1;

  if ((object_mode & OB_MODE_EDIT) || (t->options & CTX_GPENCIL_STROKES) ||
      ((object_mode & OB_MODE_POSE) && (object_type == OB_ARMATURE))) {
    if (t->data_container) {
      MEM_freeN(t->data_container);
    }

    bool free_objects = false;
    if (objects == NULL) {
      objects = BKE_view_layer_array_from_objects_in_mode(
          t->view_layer,
          (t->spacetype == SPACE_VIEW3D) ? t->view : NULL,
          &objects_len,
          {
              .object_mode = object_mode,
              .no_dup_data = true,
          });
      free_objects = true;
    }

    t->data_container = MEM_callocN(sizeof(*t->data_container) * objects_len, __func__);
    t->data_container_len = objects_len;

    for (int i = 0; i < objects_len; i++) {
      TransDataContainer *tc = &t->data_container[i];
      /* TODO, multiple axes. */
      tc->mirror.axis_flag = (((t->flag & T_NO_MIRROR) == 0) &&
                              ((t->options & CTX_NO_MIRROR) == 0) &&
                              (objects[i]->type == OB_MESH) &&
                              (((Mesh *)objects[i]->data)->editflag & ME_EDIT_MIRROR_X) != 0);

      if (object_mode & OB_MODE_EDIT) {
        tc->obedit = objects[i];
        /* Check needed for UV's */
        if ((t->flag & T_2D_EDIT) == 0) {
          tc->use_local_mat = true;
        }
      }
      else if (object_mode & OB_MODE_POSE) {
        tc->poseobj = objects[i];
        tc->use_local_mat = true;
      }
      else if (t->options & CTX_GPENCIL_STROKES) {
        tc->use_local_mat = true;
      }

      if (tc->use_local_mat) {
        BLI_assert((t->flag & T_2D_EDIT) == 0);
        copy_m4_m4(tc->mat, objects[i]->obmat);
        copy_m3_m4(tc->mat3, tc->mat);
        invert_m4_m4(tc->imat, tc->mat);
        invert_m3_m3(tc->imat3, tc->mat3);
        normalize_m3_m3(tc->mat3_unit, tc->mat3);
      }
      /* Otherwise leave as zero. */
    }

    if (free_objects) {
      MEM_freeN(objects);
    }
  }
}

/**
 * Setup internal data, mouse, vectors
 *
 * \note \a op and \a event can be NULL
 *
 * \see #saveTransform does the reverse.
 */
void initTransInfo(bContext *C, TransInfo *t, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Scene *sce = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const eObjectMode object_mode = OBACT(view_layer) ? OBACT(view_layer)->mode : OB_MODE_OBJECT;
  const short object_type = OBACT(view_layer) ? OBACT(view_layer)->type : -1;
  ToolSettings *ts = CTX_data_tool_settings(C);
  ARegion *ar = CTX_wm_region(C);
  ScrArea *sa = CTX_wm_area(C);

  bGPdata *gpd = CTX_data_gpencil_data(C);
  PropertyRNA *prop;

  t->depsgraph = depsgraph;
  t->scene = sce;
  t->view_layer = view_layer;
  t->sa = sa;
  t->ar = ar;
  t->settings = ts;
  t->reports = op ? op->reports : NULL;

  t->helpline = HLP_NONE;

  t->flag = 0;

  t->obedit_type = ((object_mode == OB_MODE_EDIT) || (object_mode == OB_MODE_EDIT_GPENCIL)) ?
                       object_type :
                       -1;

  /* Many kinds of transform only use a single handle. */
  if (t->data_container == NULL) {
    t->data_container = MEM_callocN(sizeof(*t->data_container), __func__);
    t->data_container_len = 1;
  }

  t->redraw = TREDRAW_HARD; /* redraw first time */

  if (event) {
    t->mouse.imval[0] = event->mval[0];
    t->mouse.imval[1] = event->mval[1];
  }
  else {
    t->mouse.imval[0] = 0;
    t->mouse.imval[1] = 0;
  }

  t->con.imval[0] = t->mouse.imval[0];
  t->con.imval[1] = t->mouse.imval[1];

  t->mval[0] = t->mouse.imval[0];
  t->mval[1] = t->mouse.imval[1];

  t->transform = NULL;
  t->handleEvent = NULL;

  t->data_len_all = 0;

  t->val = 0.0f;

  zero_v3(t->vec);
  zero_v3(t->center_global);

  unit_m3(t->mat);

  unit_m3(t->orient_matrix);
  negate_m3(t->orient_matrix);
  /* Leave 't->orient_matrix_is_set' to false,
   * so we overwrite it when we have a useful value. */

  /* Default to rotate on the Z axis. */
  t->orient_axis = 2;
  t->orient_axis_ortho = 1;

  /* if there's an event, we're modal */
  if (event) {
    t->flag |= T_MODAL;
  }

  /* Crease needs edge flag */
  if (ELEM(t->mode, TFM_CREASE, TFM_BWEIGHT)) {
    t->options |= CTX_EDGE;
  }

  t->remove_on_cancel = false;

  if (op && (prop = RNA_struct_find_property(op->ptr, "remove_on_cancel")) &&
      RNA_property_is_set(op->ptr, prop)) {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      t->remove_on_cancel = true;
    }
  }

  /* GPencil editing context */
  if (GPENCIL_EDIT_MODE(gpd)) {
    t->options |= CTX_GPENCIL_STROKES;
  }

  /* Assign the space type, some exceptions for running in different mode */
  if (sa == NULL) {
    /* background mode */
    t->spacetype = SPACE_EMPTY;
  }
  else if ((ar == NULL) && (sa->spacetype == SPACE_VIEW3D)) {
    /* running in the text editor */
    t->spacetype = SPACE_EMPTY;
  }
  else {
    /* normal operation */
    t->spacetype = sa->spacetype;
  }

  /* handle T_ALT_TRANSFORM initialization, we may use for different operators */
  if (op) {
    const char *prop_id = NULL;
    if (t->mode == TFM_SHRINKFATTEN) {
      prop_id = "use_even_offset";
    }

    if (prop_id && (prop = RNA_struct_find_property(op->ptr, prop_id))) {
      SET_FLAG_FROM_TEST(t->flag, RNA_property_boolean_get(op->ptr, prop), T_ALT_TRANSFORM);
    }
  }

  if (t->spacetype == SPACE_VIEW3D) {
    View3D *v3d = sa->spacedata.first;
    bScreen *animscreen = ED_screen_animation_playing(CTX_wm_manager(C));

    t->view = v3d;
    t->animtimer = (animscreen) ? animscreen->animtimer : NULL;

    /* turn gizmo off during transform */
    if (t->flag & T_MODAL) {
      t->gizmo_flag = v3d->gizmo_flag;
      v3d->gizmo_flag = V3D_GIZMO_HIDE;
    }

    if (t->scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) {
      t->flag |= T_V3D_ALIGN;
    }
    t->around = t->scene->toolsettings->transform_pivot_point;

    /* bend always uses the cursor */
    if (t->mode == TFM_BEND) {
      t->around = V3D_AROUND_CURSOR;
    }

    TransformOrientationSlot *orient_slot = &t->scene->orientation_slots[SCE_ORIENT_DEFAULT];
    t->orientation.unset = V3D_ORIENT_GLOBAL;
    t->orientation.user = orient_slot->type;
    t->orientation.custom = BKE_scene_transform_orientation_find(t->scene,
                                                                 orient_slot->index_custom);

    t->orientation.index = 0;
    ARRAY_SET_ITEMS(t->orientation.types, &t->orientation.user, NULL);

    /* Make second orientation local if both are global. */
    if (t->orientation.user == V3D_ORIENT_GLOBAL) {
      t->orientation.user_alt = V3D_ORIENT_LOCAL;
      t->orientation.types[0] = &t->orientation.user_alt;
      SWAP(short *, t->orientation.types[0], t->orientation.types[1]);
    }

    /* exceptional case */
    if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
      if (ELEM(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL)) {
        const bool use_island = transdata_check_local_islands(t, t->around);

        if ((t->obedit_type != -1) && !use_island) {
          t->options |= CTX_NO_PET;
        }
      }
    }

    if (object_mode & OB_MODE_ALL_PAINT) {
      Paint *p = BKE_paint_get_active_from_context(C);
      if (p && p->brush && (p->brush->flag & BRUSH_CURVE)) {
        t->options |= CTX_PAINT_CURVE;
      }
    }

    /* initialize UV transform from */
    if (op && ((prop = RNA_struct_find_property(op->ptr, "correct_uv")))) {
      if (RNA_property_is_set(op->ptr, prop)) {
        if (RNA_property_boolean_get(op->ptr, prop)) {
          t->settings->uvcalc_flag |= UVCALC_TRANSFORM_CORRECT;
        }
        else {
          t->settings->uvcalc_flag &= ~UVCALC_TRANSFORM_CORRECT;
        }
      }
      else {
        RNA_property_boolean_set(
            op->ptr, prop, (t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT) != 0);
      }
    }
  }
  else if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = sa->spacedata.first;
    // XXX for now, get View2D from the active region
    t->view = &ar->v2d;
    t->around = sima->around;

    if (ED_space_image_show_uvedit(sima, OBACT(t->view_layer))) {
      /* UV transform */
    }
    else if (sima->mode == SI_MODE_MASK) {
      t->options |= CTX_MASK;
    }
    else if (sima->mode == SI_MODE_PAINT) {
      Paint *p = &sce->toolsettings->imapaint.paint;
      if (p->brush && (p->brush->flag & BRUSH_CURVE)) {
        t->options |= CTX_PAINT_CURVE;
      }
    }
    /* image not in uv edit, nor in mask mode, can happen for some tools */
  }
  else if (t->spacetype == SPACE_NODE) {
    // XXX for now, get View2D from the active region
    t->view = &ar->v2d;
    t->around = V3D_AROUND_CENTER_BOUNDS;
  }
  else if (t->spacetype == SPACE_GRAPH) {
    SpaceGraph *sipo = sa->spacedata.first;
    t->view = &ar->v2d;
    t->around = sipo->around;
  }
  else if (t->spacetype == SPACE_CLIP) {
    SpaceClip *sclip = sa->spacedata.first;
    t->view = &ar->v2d;
    t->around = sclip->around;

    if (ED_space_clip_check_show_trackedit(sclip)) {
      t->options |= CTX_MOVIECLIP;
    }
    else if (ED_space_clip_check_show_maskedit(sclip)) {
      t->options |= CTX_MASK;
    }
  }
  else {
    if (ar) {
      // XXX for now, get View2D  from the active region
      t->view = &ar->v2d;
      // XXX for now, the center point is the midpoint of the data
    }
    else {
      t->view = NULL;
    }
    t->around = V3D_AROUND_CENTER_BOUNDS;
  }

  if (op && (prop = RNA_struct_find_property(op->ptr, "orient_axis"))) {
    t->orient_axis = RNA_property_enum_get(op->ptr, prop);
  }
  if (op && (prop = RNA_struct_find_property(op->ptr, "orient_axis_ortho"))) {
    t->orient_axis_ortho = RNA_property_enum_get(op->ptr, prop);
  }

  if (op &&
      ((prop = RNA_struct_find_property(op->ptr, "orient_matrix")) &&
       RNA_property_is_set(op->ptr, prop)) &&
      ((t->flag & T_MODAL) ||
       /* When using redo, don't use the the custom constraint matrix
        * if the user selects a different orientation. */
       (RNA_enum_get(op->ptr, "orient_type") == RNA_enum_get(op->ptr, "orient_matrix_type")))) {
    RNA_property_float_get_array(op->ptr, prop, &t->spacemtx[0][0]);
    /* Some transform modes use this to operate on an axis. */
    t->orient_matrix_is_set = true;
    copy_m3_m3(t->orient_matrix, t->spacemtx);
    t->orient_matrix_is_set = true;
    t->orientation.user = V3D_ORIENT_CUSTOM_MATRIX;
    t->orientation.custom = 0;
    if (t->flag & T_MODAL) {
      RNA_enum_set(op->ptr, "orient_matrix_type", RNA_enum_get(op->ptr, "orient_type"));
    }
  }
  else if (op && ((prop = RNA_struct_find_property(op->ptr, "orient_type")) &&
                  RNA_property_is_set(op->ptr, prop))) {
    short orientation = RNA_property_enum_get(op->ptr, prop);
    TransformOrientation *custom_orientation = NULL;

    if (orientation >= V3D_ORIENT_CUSTOM) {
      if (orientation >= V3D_ORIENT_CUSTOM + BIF_countTransformOrientation(C)) {
        orientation = V3D_ORIENT_GLOBAL;
      }
      else {
        custom_orientation = BKE_scene_transform_orientation_find(t->scene,
                                                                  orientation - V3D_ORIENT_CUSTOM);
        orientation = V3D_ORIENT_CUSTOM;
      }
    }

    t->orientation.user = orientation;
    t->orientation.custom = custom_orientation;
  }

  if (op && ((prop = RNA_struct_find_property(op->ptr, "release_confirm")) &&
             RNA_property_is_set(op->ptr, prop))) {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      t->flag |= T_RELEASE_CONFIRM;
    }
  }
  else {
    if (U.flag & USER_RELEASECONFIRM) {
      t->flag |= T_RELEASE_CONFIRM;
    }
  }

  if (op && ((prop = RNA_struct_find_property(op->ptr, "mirror")) &&
             RNA_property_is_set(op->ptr, prop))) {
    if (!RNA_property_boolean_get(op->ptr, prop)) {
      t->flag |= T_NO_MIRROR;
    }
  }
  else if ((t->spacetype == SPACE_VIEW3D) && (t->obedit_type == OB_MESH)) {
    /* pass */
  }
  else {
    /* Avoid mirroring for unsupported contexts. */
    t->options |= CTX_NO_MIRROR;
  }

  /* setting PET flag only if property exist in operator. Otherwise, assume it's not supported */
  if (op && (prop = RNA_struct_find_property(op->ptr, "use_proportional_edit"))) {
    if (RNA_property_is_set(op->ptr, prop)) {
      int proportional = 0;
      if (RNA_property_boolean_get(op->ptr, prop)) {
        proportional |= PROP_EDIT_USE;
        if (RNA_boolean_get(op->ptr, "use_proportional_connected")) {
          proportional |= PROP_EDIT_CONNECTED;
        }
        if (RNA_boolean_get(op->ptr, "use_proportional_projected")) {
          proportional |= PROP_EDIT_PROJECTED;
        }
      }
      t->flag |= initTransInfo_edit_pet_to_flag(proportional);
    }
    else {
      /* use settings from scene only if modal */
      if (t->flag & T_MODAL) {
        if ((t->options & CTX_NO_PET) == 0) {
          if (t->spacetype == SPACE_GRAPH) {
            t->flag |= initTransInfo_edit_pet_to_flag(ts->proportional_fcurve);
          }
          else if (t->spacetype == SPACE_ACTION) {
            t->flag |= initTransInfo_edit_pet_to_flag(ts->proportional_action);
          }
          else if (t->obedit_type != -1) {
            t->flag |= initTransInfo_edit_pet_to_flag(ts->proportional_edit);
          }
          else if (t->options & CTX_GPENCIL_STROKES) {
            t->flag |= initTransInfo_edit_pet_to_flag(ts->proportional_edit);
          }
          else if (t->options & CTX_MASK) {
            if (ts->proportional_mask) {
              t->flag |= T_PROP_EDIT;

              if (ts->proportional_edit & PROP_EDIT_CONNECTED) {
                t->flag |= T_PROP_CONNECTED;
              }
            }
          }
          else if ((t->obedit_type == -1) && ts->proportional_objects) {
            t->flag |= T_PROP_EDIT;
          }
        }
      }
    }

    if (op && ((prop = RNA_struct_find_property(op->ptr, "proportional_size")) &&
               RNA_property_is_set(op->ptr, prop))) {
      t->prop_size = RNA_property_float_get(op->ptr, prop);
    }
    else {
      t->prop_size = ts->proportional_size;
    }

    /* TRANSFORM_FIX_ME rna restrictions */
    if (t->prop_size <= 0.00001f) {
      printf("Proportional size (%f) under 0.00001, resetting to 1!\n", t->prop_size);
      t->prop_size = 1.0f;
    }

    if (op && ((prop = RNA_struct_find_property(op->ptr, "proportional_edit_falloff")) &&
               RNA_property_is_set(op->ptr, prop))) {
      t->prop_mode = RNA_property_enum_get(op->ptr, prop);
    }
    else {
      t->prop_mode = ts->prop_mode;
    }
  }
  else { /* add not pet option to context when not available */
    t->options |= CTX_NO_PET;
  }

  // Mirror is not supported with PET, turn it off.
#if 0
  if (t->flag & T_PROP_EDIT) {
    t->flag &= ~T_MIRROR;
  }
#endif

  setTransformViewAspect(t, t->aspect);

  if (op && (prop = RNA_struct_find_property(op->ptr, "center_override")) &&
      RNA_property_is_set(op->ptr, prop)) {
    RNA_property_float_get_array(op->ptr, prop, t->center_global);
    mul_v3_v3(t->center_global, t->aspect);
    t->flag |= T_OVERRIDE_CENTER;
  }

  setTransformViewMatrices(t);
  initNumInput(&t->num);
}

static void freeTransCustomData(TransInfo *t, TransDataContainer *tc, TransCustomData *custom_data)
{
  if (custom_data->free_cb) {
    /* Can take over freeing t->data and data_2d etc... */
    custom_data->free_cb(t, tc, custom_data);
    BLI_assert(custom_data->data == NULL);
  }
  else if ((custom_data->data != NULL) && custom_data->use_free) {
    MEM_freeN(custom_data->data);
    custom_data->data = NULL;
  }
  /* In case modes are switched in the same transform session. */
  custom_data->free_cb = false;
  custom_data->use_free = false;
}

static void freeTransCustomDataContainer(TransInfo *t,
                                         TransDataContainer *tc,
                                         TransCustomDataContainer *tcdc)
{
  TransCustomData *custom_data = &tcdc->first_elem;
  for (int i = 0; i < TRANS_CUSTOM_DATA_ELEM_MAX; i++, custom_data++) {
    freeTransCustomData(t, tc, custom_data);
  }
}

/**
 * Needed for mode switching.
 */
void freeTransCustomDataForMode(TransInfo *t)
{
  freeTransCustomData(t, NULL, &t->custom.mode);
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    freeTransCustomData(t, tc, &tc->custom.mode);
  }
}

/* Here I would suggest only TransInfo related issues, like free data & reset vars. Not redraws */
void postTrans(bContext *C, TransInfo *t)
{
  if (t->draw_handle_view) {
    ED_region_draw_cb_exit(t->ar->type, t->draw_handle_view);
  }
  if (t->draw_handle_apply) {
    ED_region_draw_cb_exit(t->ar->type, t->draw_handle_apply);
  }
  if (t->draw_handle_pixel) {
    ED_region_draw_cb_exit(t->ar->type, t->draw_handle_pixel);
  }
  if (t->draw_handle_cursor) {
    WM_paint_cursor_end(CTX_wm_manager(C), t->draw_handle_cursor);
  }

  if (t->flag & T_MODAL_CURSOR_SET) {
    WM_cursor_modal_restore(CTX_wm_window(C));
  }

  /* Free all custom-data */
  freeTransCustomDataContainer(t, NULL, &t->custom);
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    freeTransCustomDataContainer(t, tc, &tc->custom);
  }

  /* postTrans can be called when nothing is selected, so data is NULL already */
  if (t->data_len_all != 0) {
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      /* free data malloced per trans-data */
      if (ELEM(t->obedit_type, OB_CURVE, OB_SURF) || (t->spacetype == SPACE_GRAPH)) {
        TransData *td = tc->data;
        for (int a = 0; a < tc->data_len; a++, td++) {
          if (td->flag & TD_BEZTRIPLE) {
            MEM_freeN(td->hdata);
          }
        }
      }
      MEM_freeN(tc->data);

      MEM_SAFE_FREE(tc->data_ext);
      MEM_SAFE_FREE(tc->data_2d);
    }
  }

  MEM_SAFE_FREE(t->data_container);
  t->data_container = NULL;

  BLI_freelistN(&t->tsnap.points);

  if (t->spacetype == SPACE_IMAGE) {
    if (t->options & (CTX_MASK | CTX_PAINT_CURVE)) {
      /* pass */
    }
    else {
      SpaceImage *sima = t->sa->spacedata.first;
      if (sima->flag & SI_LIVE_UNWRAP) {
        ED_uvedit_live_unwrap_end(t->state == TRANS_CANCEL);
      }
    }
  }
  else if (t->spacetype == SPACE_VIEW3D) {
    View3D *v3d = t->sa->spacedata.first;
    /* restore gizmo */
    if (t->flag & T_MODAL) {
      v3d->gizmo_flag = t->gizmo_flag;
    }
  }

  if (t->mouse.data) {
    MEM_freeN(t->mouse.data);
  }

  if (t->rng != NULL) {
    BLI_rng_free(t->rng);
  }

  freeSnapping(t);
}

void applyTransObjects(TransInfo *t)
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  TransData *td;

  for (td = tc->data; td < tc->data + tc->data_len; td++) {
    copy_v3_v3(td->iloc, td->loc);
    if (td->ext->rot) {
      copy_v3_v3(td->ext->irot, td->ext->rot);
    }
    if (td->ext->size) {
      copy_v3_v3(td->ext->isize, td->ext->size);
    }
  }
  recalcData(t);
}

static void restoreElement(TransData *td)
{
  /* TransData for crease has no loc */
  if (td->loc) {
    copy_v3_v3(td->loc, td->iloc);
  }
  if (td->val) {
    *td->val = td->ival;
  }

  if (td->ext && (td->flag & TD_NO_EXT) == 0) {
    if (td->ext->rot) {
      copy_v3_v3(td->ext->rot, td->ext->irot);
    }
    if (td->ext->rotAngle) {
      *td->ext->rotAngle = td->ext->irotAngle;
    }
    if (td->ext->rotAxis) {
      copy_v3_v3(td->ext->rotAxis, td->ext->irotAxis);
    }
    /* XXX, drotAngle & drotAxis not used yet */
    if (td->ext->size) {
      copy_v3_v3(td->ext->size, td->ext->isize);
    }
    if (td->ext->quat) {
      copy_qt_qt(td->ext->quat, td->ext->iquat);
    }
  }

  if (td->flag & TD_BEZTRIPLE) {
    *(td->hdata->h1) = td->hdata->ih1;
    *(td->hdata->h2) = td->hdata->ih2;
  }
}

void restoreTransObjects(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    TransData *td;
    TransData2D *td2d;

    for (td = tc->data; td < tc->data + tc->data_len; td++) {
      restoreElement(td);
    }

    for (td2d = tc->data_2d; tc->data_2d && td2d < tc->data_2d + tc->data_len; td2d++) {
      if (td2d->h1) {
        td2d->h1[0] = td2d->ih1[0];
        td2d->h1[1] = td2d->ih1[1];
      }
      if (td2d->h2) {
        td2d->h2[0] = td2d->ih2[0];
        td2d->h2[1] = td2d->ih2[1];
      }
    }

    unit_m3(t->mat);
  }

  recalcData(t);
}

void calculateCenter2D(TransInfo *t)
{
  BLI_assert(!is_zero_v3(t->aspect));
  projectFloatView(t, t->center_global, t->center2d);
}

void calculateCenterLocal(TransInfo *t, const float center_global[3])
{
  /* setting constraint center */
  /* note, init functions may over-ride t->center */
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->use_local_mat) {
      mul_v3_m4v3(tc->center_local, tc->imat, center_global);
    }
    else {
      copy_v3_v3(tc->center_local, center_global);
    }
  }
}

void calculateCenterCursor(TransInfo *t, float r_center[3])
{
  const float *cursor = t->scene->cursor.location;
  copy_v3_v3(r_center, cursor);

  /* If edit or pose mode, move cursor in local space */
  if (t->options & CTX_PAINT_CURVE) {
    if (ED_view3d_project_float_global(t->ar, cursor, r_center, V3D_PROJ_TEST_NOP) !=
        V3D_PROJ_RET_OK) {
      r_center[0] = t->ar->winx / 2.0f;
      r_center[1] = t->ar->winy / 2.0f;
    }
    r_center[2] = 0.0f;
  }
}

void calculateCenterCursor2D(TransInfo *t, float r_center[2])
{
  const float *cursor = NULL;

  if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = (SpaceImage *)t->sa->spacedata.first;
    cursor = sima->cursor;
  }
  else if (t->spacetype == SPACE_CLIP) {
    SpaceClip *space_clip = (SpaceClip *)t->sa->spacedata.first;
    cursor = space_clip->cursor;
  }

  if (cursor) {
    if (t->options & CTX_MASK) {
      float co[2];

      if (t->spacetype == SPACE_IMAGE) {
        SpaceImage *sima = (SpaceImage *)t->sa->spacedata.first;
        BKE_mask_coord_from_image(sima->image, &sima->iuser, co, cursor);
      }
      else if (t->spacetype == SPACE_CLIP) {
        SpaceClip *space_clip = (SpaceClip *)t->sa->spacedata.first;
        BKE_mask_coord_from_movieclip(space_clip->clip, &space_clip->user, co, cursor);
      }
      else {
        BLI_assert(!"Shall not happen");
      }

      r_center[0] = co[0] * t->aspect[0];
      r_center[1] = co[1] * t->aspect[1];
    }
    else if (t->options & CTX_PAINT_CURVE) {
      if (t->spacetype == SPACE_IMAGE) {
        r_center[0] = UI_view2d_view_to_region_x(&t->ar->v2d, cursor[0]);
        r_center[1] = UI_view2d_view_to_region_y(&t->ar->v2d, cursor[1]);
      }
    }
    else {
      r_center[0] = cursor[0] * t->aspect[0];
      r_center[1] = cursor[1] * t->aspect[1];
    }
  }
}

void calculateCenterCursorGraph2D(TransInfo *t, float r_center[2])
{
  SpaceGraph *sipo = (SpaceGraph *)t->sa->spacedata.first;
  Scene *scene = t->scene;

  /* cursor is combination of current frame, and graph-editor cursor value */
  if (sipo->mode == SIPO_MODE_DRIVERS) {
    r_center[0] = sipo->cursorTime;
    r_center[1] = sipo->cursorVal;
  }
  else {
    r_center[0] = (float)(scene->r.cfra);
    r_center[1] = sipo->cursorVal;
  }
}

void calculateCenterMedian(TransInfo *t, float r_center[3])
{
  float partial[3] = {0.0f, 0.0f, 0.0f};
  int total = 0;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    for (int i = 0; i < tc->data_len; i++) {
      if (tc->data[i].flag & TD_SELECTED) {
        if (!(tc->data[i].flag & TD_NOCENTER)) {
          if (tc->use_local_mat) {
            float v[3];
            mul_v3_m4v3(v, tc->mat, tc->data[i].center);
            add_v3_v3(partial, v);
          }
          else {
            add_v3_v3(partial, tc->data[i].center);
          }
          total++;
        }
      }
    }
  }
  if (total) {
    mul_v3_fl(partial, 1.0f / (float)total);
  }
  copy_v3_v3(r_center, partial);
}

void calculateCenterBound(TransInfo *t, float r_center[3])
{
  float max[3], min[3];
  bool changed = false;
  INIT_MINMAX(min, max);
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    for (int i = 0; i < tc->data_len; i++) {
      if (tc->data[i].flag & TD_SELECTED) {
        if (!(tc->data[i].flag & TD_NOCENTER)) {
          if (tc->use_local_mat) {
            float v[3];
            mul_v3_m4v3(v, tc->mat, tc->data[i].center);
            minmax_v3v3_v3(min, max, v);
          }
          else {
            minmax_v3v3_v3(min, max, tc->data[i].center);
          }
          changed = true;
        }
      }
    }
  }
  if (changed) {
    mid_v3_v3v3(r_center, min, max);
  }
}

/**
 * \param select_only: only get active center from data being transformed.
 */
bool calculateCenterActive(TransInfo *t, bool select_only, float r_center[3])
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_OK(t);

  if (t->spacetype != SPACE_VIEW3D) {
    return false;
  }
  else if (tc->obedit) {
    if (ED_object_calc_active_center_for_editmode(tc->obedit, select_only, r_center)) {
      mul_m4_v3(tc->obedit->obmat, r_center);
      return true;
    }
  }
  else if (t->flag & T_POSE) {
    ViewLayer *view_layer = t->view_layer;
    Object *ob = OBACT(view_layer);
    if (ED_object_calc_active_center_for_posemode(ob, select_only, r_center)) {
      mul_m4_v3(ob->obmat, r_center);
      return true;
    }
  }
  else if (t->options & CTX_PAINT_CURVE) {
    Paint *p = BKE_paint_get_active(t->scene, t->view_layer);
    Brush *br = p->brush;
    PaintCurve *pc = br->paint_curve;
    copy_v3_v3(r_center, pc->points[pc->add_index - 1].bez.vec[1]);
    r_center[2] = 0.0f;
    return true;
  }
  else {
    /* object mode */
    ViewLayer *view_layer = t->view_layer;
    Object *ob = OBACT(view_layer);
    Base *base = BASACT(view_layer);
    if (ob && ((!select_only) || ((base->flag & BASE_SELECTED) != 0))) {
      copy_v3_v3(r_center, ob->obmat[3]);
      return true;
    }
  }

  return false;
}

static void calculateCenter_FromAround(TransInfo *t, int around, float r_center[3])
{
  switch (around) {
    case V3D_AROUND_CENTER_BOUNDS:
      calculateCenterBound(t, r_center);
      break;
    case V3D_AROUND_CENTER_MEDIAN:
      calculateCenterMedian(t, r_center);
      break;
    case V3D_AROUND_CURSOR:
      if (ELEM(t->spacetype, SPACE_IMAGE, SPACE_CLIP)) {
        calculateCenterCursor2D(t, r_center);
      }
      else if (t->spacetype == SPACE_GRAPH) {
        calculateCenterCursorGraph2D(t, r_center);
      }
      else {
        calculateCenterCursor(t, r_center);
      }
      break;
    case V3D_AROUND_LOCAL_ORIGINS:
      /* Individual element center uses median center for helpline and such */
      calculateCenterMedian(t, r_center);
      break;
    case V3D_AROUND_ACTIVE: {
      if (calculateCenterActive(t, false, r_center)) {
        /* pass */
      }
      else {
        /* fallback */
        calculateCenterMedian(t, r_center);
      }
      break;
    }
  }
}

void calculateCenter(TransInfo *t)
{
  if ((t->flag & T_OVERRIDE_CENTER) == 0) {
    calculateCenter_FromAround(t, t->around, t->center_global);
  }
  calculateCenterLocal(t, t->center_global);

  /* avoid calculating again */
  {
    TransCenterData *cd = &t->center_cache[t->around];
    copy_v3_v3(cd->global, t->center_global);
    cd->is_set = true;
  }

  calculateCenter2D(t);

  /* for panning from cameraview */
  if ((t->flag & T_OBJECT) && (t->flag & T_OVERRIDE_CENTER) == 0) {
    if (t->spacetype == SPACE_VIEW3D && t->ar && t->ar->regiontype == RGN_TYPE_WINDOW) {

      if (t->flag & T_CAMERA) {
        float axis[3];
        /* persinv is nasty, use viewinv instead, always right */
        copy_v3_v3(axis, t->viewinv[2]);
        normalize_v3(axis);

        /* 6.0 = 6 grid units */
        axis[0] = t->center_global[0] - 6.0f * axis[0];
        axis[1] = t->center_global[1] - 6.0f * axis[1];
        axis[2] = t->center_global[2] - 6.0f * axis[2];

        projectFloatView(t, axis, t->center2d);

        /* rotate only needs correct 2d center, grab needs ED_view3d_calc_zfac() value */
        if (t->mode == TFM_TRANSLATION) {
          copy_v3_v3(t->center_global, axis);
        }
      }
    }
  }

  if (t->spacetype == SPACE_VIEW3D) {
    /* ED_view3d_calc_zfac() defines a factor for perspective depth correction,
     * used in ED_view3d_win_to_delta() */

    /* zfac is only used convertViewVec only in cases operator was invoked in RGN_TYPE_WINDOW
     * and never used in other cases.
     *
     * We need special case here as well, since ED_view3d_calc_zfac will crash when called
     * for a region different from RGN_TYPE_WINDOW.
     */
    if (t->ar->regiontype == RGN_TYPE_WINDOW) {
      t->zfac = ED_view3d_calc_zfac(t->ar->regiondata, t->center_global, NULL);
    }
    else {
      t->zfac = 0.0f;
    }
  }
}

BLI_STATIC_ASSERT(ARRAY_SIZE(((TransInfo *)NULL)->center_cache) == (V3D_AROUND_ACTIVE + 1),
                  "test size");

/**
 * Lazy initialize transform center data, when we need to access center values from other types.
 */
const TransCenterData *transformCenter_from_type(TransInfo *t, int around)
{
  BLI_assert(around <= V3D_AROUND_ACTIVE);
  TransCenterData *cd = &t->center_cache[around];
  if (cd->is_set == false) {
    calculateCenter_FromAround(t, around, cd->global);
    cd->is_set = true;
  }
  return cd;
}

void calculatePropRatio(TransInfo *t)
{
  int i;
  float dist;
  const bool connected = (t->flag & T_PROP_CONNECTED) != 0;

  t->proptext[0] = '\0';

  if (t->flag & T_PROP_EDIT) {
    const char *pet_id = NULL;
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_SELECTED) {
          td->factor = 1.0f;
        }
        else if (tc->mirror.axis_flag && (td->loc[0] * tc->mirror.sign) < -0.00001f) {
          td->flag |= TD_SKIP;
          td->factor = 0.0f;
          restoreElement(td);
        }
        else if ((connected && (td->flag & TD_NOTCONNECTED || td->dist > t->prop_size)) ||
                 (connected == 0 && td->rdist > t->prop_size)) {
          /*
           * The elements are sorted according to their dist member in the array,
           * that means we can stop when it finds one element outside of the propsize.
           * do not set 'td->flag |= TD_NOACTION', the prop circle is being changed.
           */

          td->factor = 0.0f;
          restoreElement(td);
        }
        else {
          /* Use rdist for falloff calculations, it is the real distance */
          td->flag &= ~TD_NOACTION;

          if (connected) {
            dist = (t->prop_size - td->dist) / t->prop_size;
          }
          else {
            dist = (t->prop_size - td->rdist) / t->prop_size;
          }

          /*
           * Clamp to positive numbers.
           * Certain corner cases with connectivity and individual centers
           * can give values of rdist larger than propsize.
           */
          if (dist < 0.0f) {
            dist = 0.0f;
          }

          switch (t->prop_mode) {
            case PROP_SHARP:
              td->factor = dist * dist;
              break;
            case PROP_SMOOTH:
              td->factor = 3.0f * dist * dist - 2.0f * dist * dist * dist;
              break;
            case PROP_ROOT:
              td->factor = sqrtf(dist);
              break;
            case PROP_LIN:
              td->factor = dist;
              break;
            case PROP_CONST:
              td->factor = 1.0f;
              break;
            case PROP_SPHERE:
              td->factor = sqrtf(2 * dist - dist * dist);
              break;
            case PROP_RANDOM:
              if (t->rng == NULL) {
                /* Lazy initialization. */
                uint rng_seed = (uint)(PIL_check_seconds_timer_i() & UINT_MAX);
                t->rng = BLI_rng_new(rng_seed);
              }
              td->factor = BLI_rng_get_float(t->rng) * dist;
              break;
            case PROP_INVSQUARE:
              td->factor = dist * (2.0f - dist);
              break;
            default:
              td->factor = 1;
              break;
          }
        }
      }
    }

    switch (t->prop_mode) {
      case PROP_SHARP:
        pet_id = N_("(Sharp)");
        break;
      case PROP_SMOOTH:
        pet_id = N_("(Smooth)");
        break;
      case PROP_ROOT:
        pet_id = N_("(Root)");
        break;
      case PROP_LIN:
        pet_id = N_("(Linear)");
        break;
      case PROP_CONST:
        pet_id = N_("(Constant)");
        break;
      case PROP_SPHERE:
        pet_id = N_("(Sphere)");
        break;
      case PROP_RANDOM:
        pet_id = N_("(Random)");
        break;
      case PROP_INVSQUARE:
        pet_id = N_("(InvSquare)");
        break;
      default:
        break;
    }

    if (pet_id) {
      BLI_strncpy(t->proptext, IFACE_(pet_id), sizeof(t->proptext));
    }
  }
  else {
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        td->factor = 1.0;
      }
    }
  }
}

/**
 * Rotate an element, low level code, ignore protected channels.
 * (use for objects or pose-bones)
 * Similar to #ElementRotation.
 */
void transform_data_ext_rotate(TransData *td, float mat[3][3], bool use_drot)
{
  float totmat[3][3];
  float smat[3][3];
  float fmat[3][3];
  float obmat[3][3];

  float dmat[3][3]; /* delta rotation */
  float dmat_inv[3][3];

  mul_m3_m3m3(totmat, mat, td->mtx);
  mul_m3_m3m3(smat, td->smtx, mat);

  /* logic from BKE_object_rot_to_mat3 */
  if (use_drot) {
    if (td->ext->rotOrder > 0) {
      eulO_to_mat3(dmat, td->ext->drot, td->ext->rotOrder);
    }
    else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
#if 0
      axis_angle_to_mat3(dmat, td->ext->drotAxis, td->ext->drotAngle);
#else
      unit_m3(dmat);
#endif
    }
    else {
      float tquat[4];
      normalize_qt_qt(tquat, td->ext->dquat);
      quat_to_mat3(dmat, tquat);
    }

    invert_m3_m3(dmat_inv, dmat);
  }

  if (td->ext->rotOrder == ROT_MODE_QUAT) {
    float quat[4];

    /* calculate the total rotatation */
    quat_to_mat3(obmat, td->ext->iquat);
    if (use_drot) {
      mul_m3_m3m3(obmat, dmat, obmat);
    }

    /* mat = transform, obmat = object rotation */
    mul_m3_m3m3(fmat, smat, obmat);

    if (use_drot) {
      mul_m3_m3m3(fmat, dmat_inv, fmat);
    }

    mat3_to_quat(quat, fmat);

    /* apply */
    copy_qt_qt(td->ext->quat, quat);
  }
  else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
    float axis[3], angle;

    /* calculate the total rotatation */
    axis_angle_to_mat3(obmat, td->ext->irotAxis, td->ext->irotAngle);
    if (use_drot) {
      mul_m3_m3m3(obmat, dmat, obmat);
    }

    /* mat = transform, obmat = object rotation */
    mul_m3_m3m3(fmat, smat, obmat);

    if (use_drot) {
      mul_m3_m3m3(fmat, dmat_inv, fmat);
    }

    mat3_to_axis_angle(axis, &angle, fmat);

    /* apply */
    copy_v3_v3(td->ext->rotAxis, axis);
    *td->ext->rotAngle = angle;
  }
  else {
    float eul[3];

    /* calculate the total rotatation */
    eulO_to_mat3(obmat, td->ext->irot, td->ext->rotOrder);
    if (use_drot) {
      mul_m3_m3m3(obmat, dmat, obmat);
    }

    /* mat = transform, obmat = object rotation */
    mul_m3_m3m3(fmat, smat, obmat);

    if (use_drot) {
      mul_m3_m3m3(fmat, dmat_inv, fmat);
    }

    mat3_to_compatible_eulO(eul, td->ext->rot, td->ext->rotOrder, fmat);

    /* apply */
    copy_v3_v3(td->ext->rot, eul);
  }
}
