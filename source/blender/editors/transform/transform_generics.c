/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/transform/transform_generics.c
 *  \ingroup edtransform
 */

#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_sys_types.h" /* for intptr_t support */

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
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

#include "BLF_translation.h"

#include "RNA_access.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BIK_api.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_lattice.h"
#include "BKE_nla.h"
#include "BKE_context.h"
#include "BKE_paint.h"
#include "BKE_sequencer.h"
#include "BKE_editmesh.h"
#include "BKE_tracking.h"
#include "BKE_mask.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_image.h"
#include "ED_keyframing.h"
#include "ED_markers.h"
#include "ED_mesh.h"
#include "ED_particle.h"
#include "ED_screen_types.h"
#include "ED_space_api.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"
#include "ED_curve.h" /* for curve_editnurbs */
#include "ED_clip.h"
#include "ED_screen.h"

#include "WM_types.h"
#include "WM_api.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "transform.h"

/* ************************** Functions *************************** */

void getViewVector(TransInfo *t, float coord[3], float vec[3])
{
	if (t->persp != RV3D_ORTHO) {
		float p1[4], p2[4];
		
		copy_v3_v3(p1, coord);
		p1[3] = 1.0f;
		copy_v3_v3(p2, p1);
		p2[3] = 1.0f;
		mul_m4_v4(t->viewmat, p2);
		
		p2[0] = 2.0f * p2[0];
		p2[1] = 2.0f * p2[1];
		p2[2] = 2.0f * p2[2];
		
		mul_m4_v4(t->viewinv, p2);
		
		sub_v3_v3v3(vec, p1, p2);
	}
	else {
		copy_v3_v3(vec, t->viewinv[2]);
	}
	normalize_v3(vec);
}

/* ************************** GENERICS **************************** */


static void clipMirrorModifier(TransInfo *t, Object *ob)
{
	ModifierData *md = ob->modifiers.first;
	float tolerance[3] = {0.0f, 0.0f, 0.0f};
	int axis = 0;
	
	for (; md; md = md->next) {
		if ((md->type == eModifierType_Mirror) && (md->mode & eModifierMode_Realtime)) {
			MirrorModifierData *mmd = (MirrorModifierData *) md;
			
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
					TransData *td = t->data;
					
					if (mmd->mirror_ob) {
						float obinv[4][4];
						
						invert_m4_m4(obinv, mmd->mirror_ob->obmat);
						mul_m4_m4m4(mtx, obinv, ob->obmat);
						invert_m4_m4(imtx, mtx);
					}
					
					for (i = 0; i < t->total; i++, td++) {
						int clip;
						float loc[3], iloc[3];
						
						if (td->flag & TD_NOACTION)
							break;
						if (td->loc == NULL)
							break;
						
						if (td->flag & TD_SKIP)
							continue;
						
						copy_v3_v3(loc,  td->loc);
						copy_v3_v3(iloc, td->iloc);
						
						if (mmd->mirror_ob) {
							mul_m4_v3(mtx, loc);
							mul_m4_v3(mtx, iloc);
						}
						
						clip = 0;
						if (axis & 1) {
							if (fabsf(iloc[0]) <= tolerance[0] ||
							    loc[0] * iloc[0] < 0.0f)
							{
								loc[0] = 0.0f;
								clip = 1;
							}
						}
						
						if (axis & 2) {
							if (fabsf(iloc[1]) <= tolerance[1] ||
							    loc[1] * iloc[1] < 0.0f)
							{
								loc[1] = 0.0f;
								clip = 1;
							}
						}
						if (axis & 4) {
							if (fabsf(iloc[2]) <= tolerance[2] ||
							    loc[2] * iloc[2] < 0.0f)
							{
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

/* assumes obedit set to mesh object */
static void editbmesh_apply_to_mirror(TransInfo *t)
{
	TransData *td = t->data;
	BMVert *eve;
	int i;
	
	for (i = 0; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		if (td->loc == NULL)
			break;
		if (td->flag & TD_SKIP)
			continue;
		
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

/* for the realtime animation recording feature, handle overlapping data */
static void animrecord_check_state(Scene *scene, ID *id, wmTimer *animtimer)
{
	ScreenAnimData *sad = (animtimer) ? animtimer->customdata : NULL;
	
	/* sanity checks */
	if (ELEM(NULL, scene, id, sad))
		return;
	
	/* check if we need a new strip if:
	 *  - if animtimer is running
	 *	- we're not only keying for available channels
	 *	- the option to add new actions for each round is not enabled
	 */
	if (IS_AUTOKEY_FLAG(scene, INSERTAVAIL) == 0 && (scene->toolsettings->autokey_flag & ANIMRECORD_FLAG_WITHNLA)) {
		/* if playback has just looped around, we need to add a new NLA track+strip to allow a clean pass to occur */
		if ((sad) && (sad->flag & ANIMPLAY_FLAG_JUMPED)) {
			AnimData *adt = BKE_animdata_from_id(id);
			
			/* perform push-down manually with some differences 
			 * NOTE: BKE_nla_action_pushdown() sync warning...
			 */
			if ((adt->action) && !(adt->flag & ADT_NLA_EDIT_ON)) {
				float astart, aend;
				
				/* only push down if action is more than 1-2 frames long */
				calc_action_range(adt->action, &astart, &aend, 1);
				if (aend > astart + 2.0f) {
					NlaStrip *strip = add_nlastrip_to_stack(adt, adt->action);
					
					/* clear reference to action now that we've pushed it onto the stack */
					adt->action->id.us--;
					adt->action = NULL;
					
					/* adjust blending + extend so that they will behave correctly */
					strip->extendmode = NLASTRIP_EXTEND_NOTHING;
					strip->flag &= ~(NLASTRIP_FLAG_AUTO_BLENDS | NLASTRIP_FLAG_SELECT | NLASTRIP_FLAG_ACTIVE);
					
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

	if (bezt == NULL) /* ignore baked */
		return 0;

	for (i = 0; i < fcu->totvert; i++, bezt++) {
		if (BEZSELECTED(bezt)) return 1;
	}

	return 0;
}

/* helper for recalcData() - for Action Editor transforms */
static void recalcData_actedit(TransInfo *t)
{
	Scene *scene = t->scene;
	SpaceAction *saction = (SpaceAction *)t->sa->spacedata.first;
	
	bAnimContext ac = {NULL};
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* initialize relevant anim-context 'context' data from TransInfo data */
	/* NOTE: sync this with the code in ANIM_animdata_get_context() */
	ac.scene = t->scene;
	ac.obact = OBACT;
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
	else {
		/* get animdata blocks visible in editor, assuming that these will be the ones where things changed */
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_ANIMDATA);
		ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
		
		/* just tag these animdata-blocks to recalc, assuming that some data there changed 
		 * BUT only do this if realtime updates are enabled
		 */
		if ((saction->flag & SACTION_NOREALTIMEUPDATES) == 0) {
			for (ale = anim_data.first; ale; ale = ale->next) {
				/* set refresh tags for objects using this animation */
				ANIM_list_elem_update(t->scene, ale);
			}
		}
		
		/* now free temp channels */
		ANIM_animdata_freelist(&anim_data);
	}
}
/* helper for recalcData() - for Graph Editor transforms */
static void recalcData_graphedit(TransInfo *t)
{
	SpaceIpo *sipo = (SpaceIpo *)t->sa->spacedata.first;
	Scene *scene;
	
	ListBase anim_data = {NULL, NULL};
	bAnimContext ac = {NULL};
	int filter;
	
	bAnimListElem *ale;
	int dosort = 0;

	/* initialize relevant anim-context 'context' data from TransInfo data */
	/* NOTE: sync this with the code in ANIM_animdata_get_context() */
	scene = ac.scene = t->scene;
	ac.obact = OBACT;
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
		if (!fcu_test_selected(fcu))
			continue;

		/* watch it: if the time is wrong: do not correct handles yet */
		if (test_time_fcurve(fcu))
			dosort++;
		else
			calchandles_fcurve(fcu);
		
		/* set refresh tags for objects using this animation,
		 * BUT only if realtime updates are enabled  
		 */
		if ((sipo->flag & SIPO_NOREALTIMEUPDATES) == 0)
			ANIM_list_elem_update(t->scene, ale);
	}
	
	/* do resort and other updates? */
	if (dosort) remake_graph_transdata(t, &anim_data);
	
	/* now free temp channels */
	ANIM_animdata_freelist(&anim_data);
}

/* helper for recalcData() - for NLA Editor transforms */
static void recalcData_nla(TransInfo *t)
{
	TransDataNla *tdn = (TransDataNla *)t->customData;
	SpaceNla *snla = (SpaceNla *)t->sa->spacedata.first;
	Scene *scene = t->scene;
	double secf = FPS;
	int i;
	
	/* for each strip we've got, perform some additional validation of the values that got set before
	 * using RNA to set the value (which does some special operations when setting these values to make
	 * sure that everything works ok)
	 */
	for (i = 0; i < t->total; i++, tdn++) {
		NlaStrip *strip = tdn->strip;
		PointerRNA strip_ptr;
		short pExceeded, nExceeded, iter;
		int delta_y1, delta_y2;
		
		/* if this tdn has no handles, that means it is just a dummy that should be skipped */
		if (tdn->handle == 0)
			continue;
		
		/* set refresh tags for objects using this animation,
		 * BUT only if realtime updates are enabled  
		 */
		if ((snla->flag & SNLA_NOREALTIMEUPDATES) == 0)
			ANIM_id_update(t->scene, tdn->id);
		
		/* if canceling transform, just write the values without validating, then move on */
		if (t->state == TRANS_CANCEL) {
			/* clear the values by directly overwriting the originals, but also need to restore
			 * endpoints of neighboring transition-strips
			 */
			
			/* start */
			strip->start = tdn->h1[0];
			
			if ((strip->prev) && (strip->prev->type == NLASTRIP_TYPE_TRANSITION))
				strip->prev->end = tdn->h1[0];
			
			/* end */
			strip->end = tdn->h2[0];
			
			if ((strip->next) && (strip->next->type == NLASTRIP_TYPE_TRANSITION))
				strip->next->start = tdn->h2[0];
			
			/* flush transforms to child strips (since this should be a meta) */
			BKE_nlameta_flush_transforms(strip);
			
			/* restore to original track (if needed) */
			if (tdn->oldTrack != tdn->nlt) {
				/* just append to end of list for now, since strips get sorted in special_aftertrans_update() */
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
			pExceeded = ((strip->prev) && (strip->prev->type != NLASTRIP_TYPE_TRANSITION) && (tdn->h1[0] < strip->prev->end));
			nExceeded = ((strip->next) && (strip->next->type != NLASTRIP_TYPE_TRANSITION) && (tdn->h2[0] > strip->next->start));
			
			if ((pExceeded && nExceeded) || (iter == 4)) {
				/* both endpoints exceeded (or iteration ping-pong'd meaning that we need a compromise)
				 *	- simply crop strip to fit within the bounds of the strips bounding it
				 *	- if there were no neighbors, clear the transforms (make it default to the strip's current values)
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
			else /* all is fine and well */
				break;
		}
		
		/* handle auto-snapping
		 * NOTE: only do this when transform is still running, or we can't restore
		 */
		if (t->state != TRANS_CANCEL) {
			switch (snla->autosnap) {
				case SACTSNAP_FRAME: /* snap to nearest frame */
				case SACTSNAP_STEP: /* frame step - this is basically the same, since we don't have any remapping going on */
				{
					tdn->h1[0] = floorf(tdn->h1[0] + 0.5f);
					tdn->h2[0] = floorf(tdn->h2[0] + 0.5f);
					break;
				}
				
				case SACTSNAP_SECOND: /* snap to nearest second */
				case SACTSNAP_TSTEP: /* second step - this is basically the same, since we don't have any remapping going on */
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
					float delta  = h1_new - tdn->h1[0];
					
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
		 * (e.g. for transition strips, the values are taken from the neighbours)
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
		
		
		/* now, check if we need to try and move track
		 *	- we need to calculate both, as only one may have been altered by transform if only 1 handle moved
		 */
		delta_y1 = ((int)tdn->h1[1] / NLACHANNEL_STEP(snla) - tdn->trackIndex);
		delta_y2 = ((int)tdn->h2[1] / NLACHANNEL_STEP(snla) - tdn->trackIndex);
		
		if (delta_y1 || delta_y2) {
			NlaTrack *track;
			int delta = (delta_y2) ? delta_y2 : delta_y1;
			int n;
			
			/* move in the requested direction, checking at each layer if there's space for strip to pass through,
			 * stopping on the last track available or that we're able to fit in
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
					else /* can't move any further */
						break;
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
					else /* can't move any further */
						break;
				}
			}
		}
	}
}

static void recalcData_mask_common(TransInfo *t)
{
	Mask *mask = CTX_data_edit_mask(t->context);

	flushTransMasking(t);

	DAG_id_tag_update(&mask->id, 0);
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
	else if (t->obedit && t->obedit->type == OB_MESH) {
		SpaceImage *sima = t->sa->spacedata.first;
		
		flushTransUVs(t);
		if (sima->flag & SI_LIVE_UNWRAP)
			ED_uvedit_live_unwrap_re_solve();
		
		DAG_id_tag_update(t->obedit->data, 0);
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
					if (TRACK_AREA_SELECTED(track, TRACK_AREA_PAT))
						BKE_tracking_marker_clamp(marker, CLAMP_PAT_POS);
					if (TRACK_AREA_SELECTED(track, TRACK_AREA_SEARCH))
						BKE_tracking_marker_clamp(marker, CLAMP_SEARCH_POS);
				}
				else if (t->mode == TFM_RESIZE) {
					if (TRACK_AREA_SELECTED(track, TRACK_AREA_PAT))
						BKE_tracking_marker_clamp(marker, CLAMP_PAT_DIM);
					if (TRACK_AREA_SELECTED(track, TRACK_AREA_SEARCH))
						BKE_tracking_marker_clamp(marker, CLAMP_SEARCH_DIM);
				}
				else if (t->mode == TFM_ROTATION) {
					if (TRACK_AREA_SELECTED(track, TRACK_AREA_PAT))
						BKE_tracking_marker_clamp(marker, CLAMP_PAT_POS);
				}
			}

			track = track->next;
		}

		DAG_id_tag_update(&clip->id, 0);
	}
	else if (t->options & CTX_MASK) {
		recalcData_mask_common(t);
	}
}

/* helper for recalcData() - for object transforms, typically in the 3D view */
static void recalcData_objects(TransInfo *t)
{
	Base *base = t->scene->basact;
	
	if (t->obedit) {
		if (ELEM(t->obedit->type, OB_CURVE, OB_SURF)) {
			Curve *cu = t->obedit->data;
			ListBase *nurbs = BKE_curve_editNurbs_get(cu);
			Nurb *nu = nurbs->first;
			
			if (t->state != TRANS_CANCEL) {
				clipMirrorModifier(t, t->obedit);
				applyProject(t);
			}
			
			DAG_id_tag_update(t->obedit->data, 0);  /* sets recalc flags */
				
			if (t->state == TRANS_CANCEL) {
				while (nu) {
					BKE_nurb_handles_calc(nu); /* Cant do testhandlesNurb here, it messes up the h1 and h2 flags */
					nu = nu->next;
				}
			}
			else {
				/* Normal updating */
				while (nu) {
					BKE_nurb_test2D(nu);
					BKE_nurb_handles_calc(nu);
					nu = nu->next;
				}
			}
		}
		else if (t->obedit->type == OB_LATTICE) {
			Lattice *la = t->obedit->data;
			
			if (t->state != TRANS_CANCEL) {
				applyProject(t);
			}
			
			DAG_id_tag_update(t->obedit->data, 0);  /* sets recalc flags */
			
			if (la->editlatt->latt->flag & LT_OUTSIDE) outside_lattice(la->editlatt->latt);
		}
		else if (t->obedit->type == OB_MESH) {
			BMEditMesh *em = BKE_editmesh_from_object(t->obedit);
			/* mirror modifier clipping? */
			if (t->state != TRANS_CANCEL) {
				/* apply clipping after so we never project past the clip plane [#25423] */
				applyProject(t);
				clipMirrorModifier(t, t->obedit);
			}
			if ((t->options & CTX_NO_MIRROR) == 0 && (t->flag & T_MIRROR))
				editbmesh_apply_to_mirror(t);
				
			DAG_id_tag_update(t->obedit->data, 0);  /* sets recalc flags */
			
			EDBM_mesh_normals_update(em);
			BKE_editmesh_tessface_calc(em);
		}
		else if (t->obedit->type == OB_ARMATURE) { /* no recalc flag, does pose */
			bArmature *arm = t->obedit->data;
			ListBase *edbo = arm->edbo;
			EditBone *ebo, *ebo_parent;
			TransData *td = t->data;
			int i;
			
			if (t->state != TRANS_CANCEL) {
				applyProject(t);
			}
			
			/* Ensure all bones are correctly adjusted */
			for (ebo = edbo->first; ebo; ebo = ebo->next) {
				ebo_parent = (ebo->flag & BONE_CONNECTED) ? ebo->parent : NULL;
				
				if (ebo_parent) {
					/* If this bone has a parent tip that has been moved */
					if (ebo_parent->flag & BONE_TIPSEL) {
						copy_v3_v3(ebo->head, ebo_parent->tail);
						if (t->mode == TFM_BONE_ENVELOPE) ebo->rad_head = ebo_parent->rad_tail;
					}
					/* If this bone has a parent tip that has NOT been moved */
					else {
						copy_v3_v3(ebo_parent->tail, ebo->head);
						if (t->mode == TFM_BONE_ENVELOPE) ebo_parent->rad_tail = ebo->rad_head;
					}
				}
				
				/* on extrude bones, oldlength==0.0f, so we scale radius of points */
				ebo->length = len_v3v3(ebo->head, ebo->tail);
				if (ebo->oldlength == 0.0f) {
					ebo->rad_head = 0.25f * ebo->length;
					ebo->rad_tail = 0.10f * ebo->length;
					ebo->dist = 0.25f * ebo->length;
					if (ebo->parent) {
						if (ebo->rad_head > ebo->parent->rad_tail)
							ebo->rad_head = ebo->parent->rad_tail;
					}
				}
				else if (t->mode != TFM_BONE_ENVELOPE) {
					/* if bones change length, lets do that for the deform distance as well */
					ebo->dist *= ebo->length / ebo->oldlength;
					ebo->rad_head *= ebo->length / ebo->oldlength;
					ebo->rad_tail *= ebo->length / ebo->oldlength;
					ebo->oldlength = ebo->length;
				}
			}
			
			if (!ELEM(t->mode, TFM_BONE_ROLL, TFM_BONE_ENVELOPE, TFM_BONESIZE)) {
				/* fix roll */
				for (i = 0; i < t->total; i++, td++) {
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
							roll = ED_rollBoneToVector(ebo, up_axis, false);
							ebo->roll = angle_compat_rad(roll, td->ival);
						}
					}
				}
			}
			
			if (arm->flag & ARM_MIRROR_EDIT) {
				if (t->state != TRANS_CANCEL)
					transform_armature_mirror_update(t->obedit);
				else
					restoreBones(t);
			}
		}
		else {
			if (t->state != TRANS_CANCEL) {
				applyProject(t);
			}
			DAG_id_tag_update(t->obedit->data, 0);  /* sets recalc flags */
		}
	}
	else if ((t->flag & T_POSE) && t->poseobj) {
		Object *ob = t->poseobj;
		bArmature *arm = ob->data;
		
		/* if animtimer is running, and the object already has animation data,
		 * check if the auto-record feature means that we should record 'samples'
		 * (i.e. uneditable animation values)
		 *
		 * context is needed for keying set poll() functions.
		 */
		// TODO: autokeyframe calls need some setting to specify to add samples (FPoints) instead of keyframes?
		if ((t->animtimer) && (t->context) && IS_AUTOKEY_ON(t->scene)) {
			int targetless_ik = (t->flag & T_AUTOIK); // XXX this currently doesn't work, since flags aren't set yet!
			
			animrecord_check_state(t->scene, &ob->id, t->animtimer);
			autokeyframe_pose_cb_func(t->context, t->scene, (View3D *)t->view, ob, t->mode, targetless_ik);
		}
		
		/* old optimize trick... this enforces to bypass the depgraph */
		if (!(arm->flag & ARM_DELAYDEFORM)) {
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);  /* sets recalc flags */
			/* transformation of pose may affect IK tree, make sure it is rebuilt */
			BIK_clear_data(ob->pose);
		}
		else
			BKE_pose_where_is(t->scene, ob);
	}
	else if (base && (base->object->mode & OB_MODE_PARTICLE_EDIT) && PE_get_current(t->scene, base->object)) {
		if (t->state != TRANS_CANCEL) {
			applyProject(t);
		}
		flushTransParticles(t);
	}
	else {
		int i;
		
		if (t->state != TRANS_CANCEL) {
			applyProject(t);
		}
		
		for (i = 0; i < t->total; i++) {
			TransData *td = t->data + i;
			Object *ob = td->ob;
			
			if (td->flag & TD_NOACTION)
				break;
			
			if (td->flag & TD_SKIP)
				continue;
			
			/* if animtimer is running, and the object already has animation data,
			 * check if the auto-record feature means that we should record 'samples'
			 * (i.e. uneditable animation values)
			 */
			// TODO: autokeyframe calls need some setting to specify to add samples (FPoints) instead of keyframes?
			if ((t->animtimer) && IS_AUTOKEY_ON(t->scene)) {
				animrecord_check_state(t->scene, &ob->id, t->animtimer);
				autokeyframe_ob_cb_func(t->context, t->scene, (View3D *)t->view, ob, t->mode);
			}
			
			/* sets recalc flags fully, instead of flushing existing ones 
			 * otherwise proxies don't function correctly
			 */
			DAG_id_tag_update(&ob->id, OB_RECALC_OB);

			if (t->flag & T_TEXTURE)
				DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		}
	}
}

/* helper for recalcData() - for sequencer transforms */
static void recalcData_sequencer(TransInfo *t)
{
	TransData *td;
	int a;
	Sequence *seq_prev = NULL;

	for (a = 0, td = t->data; a < t->total; a++, td++) {
		TransDataSeq *tdsq = (TransDataSeq *) td->extra;
		Sequence *seq = tdsq->seq;

		if (seq != seq_prev) {
			BKE_sequence_invalidate_dependent(t->scene, seq);
		}

		seq_prev = seq;
	}

	BKE_sequencer_preprocessed_cache_cleanup();

	flushTransSeq(t);
}

/* called for updating while transform acts, once per redraw */
void recalcData(TransInfo *t)
{
	/* if tests must match createTransData for correct updates */
	if (t->options & CTX_TEXTURE) {
		recalcData_objects(t);
	}
	else if (t->options & CTX_EDGE) {
		recalcData_objects(t);
	}
	else if (t->options & CTX_PAINT_CURVE) {
		flushTransPaintCurve(t);
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
	else if (t->spacetype == SPACE_IPO) {
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
		
		glPushMatrix();
		
		//if (t->obedit) glLoadMatrixf(t->obedit->obmat);	// sets opengl viewing
		
		
		copy_v3_v3(v3, dir);
		mul_v3_fl(v3, v3d->far);
		
		sub_v3_v3v3(v2, center, v3);
		add_v3_v3v3(v1, center, v3);
		
		if (options & DRAWLIGHT) {
			col[0] = col[1] = col[2] = 220;
		}
		else {
			UI_GetThemeColor3ubv(TH_GRID, col);
		}
		UI_make_axis_color(col, col2, axis);
		glColor3ubv(col2);
		
		setlinestyle(0);
		glBegin(GL_LINE_STRIP);
		glVertex3fv(v1);
		glVertex3fv(v2);
		glEnd();
		
		glPopMatrix();
	}
}

/**
 * Free data before switching to another mode.
 */
void resetTransModal(TransInfo *t)
{
	if (t->mode == TFM_EDGE_SLIDE) {
		freeEdgeSlideVerts(t);
	}
	else if (t->mode == TFM_VERT_SLIDE) {
		freeVertSlideVerts(t);
	}
}

void resetTransRestrictions(TransInfo *t)
{
	t->flag &= ~T_ALL_RESTRICTIONS;
}

static int initTransInfo_edit_pet_to_flag(const int proportional)
{
	switch (proportional) {
		case PROP_EDIT_ON:
			return T_PROP_EDIT;
		case PROP_EDIT_CONNECTED:
			return T_PROP_EDIT | T_PROP_CONNECTED;
		case PROP_EDIT_PROJECTED:
			return T_PROP_EDIT | T_PROP_PROJECTED;
		default:
			return 0;
	}
}

/**
 * Setup internal data, mouse, vectors
 *
 * \note \a op and \a event can be NULL
 */
void initTransInfo(bContext *C, TransInfo *t, wmOperator *op, const wmEvent *event)
{
	Scene *sce = CTX_data_scene(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	ARegion *ar = CTX_wm_region(C);
	ScrArea *sa = CTX_wm_area(C);
	Object *obedit = CTX_data_edit_object(C);
	Object *ob = CTX_data_active_object(C);
	PropertyRNA *prop;
	
	t->scene = sce;
	t->sa = sa;
	t->ar = ar;
	t->obedit = obedit;
	t->settings = ts;
	t->reports = op ? op->reports : NULL;

	if (obedit) {
		copy_m3_m4(t->obedit_mat, obedit->obmat);
		normalize_m3(t->obedit_mat);
	}

	t->data = NULL;
	t->ext = NULL;
	
	t->helpline = HLP_NONE;
	
	t->flag = 0;
	
	t->redraw = TREDRAW_HARD;  /* redraw first time */
	
	if (event) {
		copy_v2_v2_int(t->imval, event->mval);
		t->event_type = event->type;
	}
	else {
		t->imval[0] = 0;
		t->imval[1] = 0;
	}
	
	t->con.imval[0] = t->imval[0];
	t->con.imval[1] = t->imval[1];
	
	t->mval[0] = t->imval[0];
	t->mval[1] = t->imval[1];
	
	t->transform        = NULL;
	t->handleEvent      = NULL;
	
	t->total            = 0;
	
	t->val = 0.0f;

	zero_v3(t->vec);
	zero_v3(t->center);

	unit_m3(t->mat);
	
	/* if there's an event, we're modal */
	if (event) {
		t->flag |= T_MODAL;
	}

	/* Crease needs edge flag */
	if (ELEM(t->mode, TFM_CREASE, TFM_BWEIGHT)) {
		t->options |= CTX_EDGE;
	}

	t->remove_on_cancel = false;

	if (op && (prop = RNA_struct_find_property(op->ptr, "remove_on_cancel")) && RNA_property_is_set(op->ptr, prop)) {
		if (RNA_property_boolean_get(op->ptr, prop)) {
			t->remove_on_cancel = true;
		}
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


	if (t->spacetype == SPACE_VIEW3D) {
		View3D *v3d = sa->spacedata.first;
		bScreen *animscreen = ED_screen_animation_playing(CTX_wm_manager(C));
		
		t->view = v3d;
		t->animtimer = (animscreen) ? animscreen->animtimer : NULL;
		
		/* turn manipulator off during transform */
		// FIXME: but don't do this when USING the manipulator...
		if (t->flag & T_MODAL) {
			t->twtype = v3d->twtype;
			v3d->twtype = 0;
		}

		if (v3d->flag & V3D_ALIGN) t->flag |= T_V3D_ALIGN;
		t->around = v3d->around;
		
		/* bend always uses the cursor */
		if (t->mode == TFM_BEND) {
			t->around = V3D_CURSOR;
		}

		if (op && ((prop = RNA_struct_find_property(op->ptr, "constraint_orientation")) &&
		           RNA_property_is_set(op->ptr, prop)))
		{
			t->current_orientation = RNA_property_enum_get(op->ptr, prop);

			if (t->current_orientation >= V3D_MANIP_CUSTOM + BIF_countTransformOrientation(C)) {
				t->current_orientation = V3D_MANIP_GLOBAL;
			}
		}
		else {
			t->current_orientation = v3d->twmode;
		}

		/* exceptional case */
		if (t->around == V3D_LOCAL && (t->settings->selectmode & SCE_SELECT_FACE)) {
			if (ELEM(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL)) {
				t->options |= CTX_NO_PET;
			}
		}

		if (ob && ob->mode & OB_MODE_ALL_PAINT) {
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
				RNA_property_boolean_set(op->ptr, prop, t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT);
			}
		}

	}
	else if (t->spacetype == SPACE_IMAGE) {
		SpaceImage *sima = sa->spacedata.first;
		// XXX for now, get View2D from the active region
		t->view = &ar->v2d;
		t->around = sima->around;

		if (ED_space_image_show_uvedit(sima, t->obedit)) {
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
		t->around = V3D_CENTER;
	}
	else if (t->spacetype == SPACE_IPO) {
		SpaceIpo *sipo = sa->spacedata.first;
		t->view = &ar->v2d;
		t->around = sipo->around;
	}
	else if (t->spacetype == SPACE_CLIP) {
		SpaceClip *sclip = sa->spacedata.first;
		t->view = &ar->v2d;
		t->around = sclip->around;

		if (ED_space_clip_check_show_trackedit(sclip))
			t->options |= CTX_MOVIECLIP;
		else if (ED_space_clip_check_show_maskedit(sclip))
			t->options |= CTX_MASK;
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
		t->around = V3D_CENTER;
	}
	
	if (op && ((prop = RNA_struct_find_property(op->ptr, "release_confirm")) &&
	           RNA_property_is_set(op->ptr, prop)))
	{
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
	           RNA_property_is_set(op->ptr, prop)))
	{
		if (RNA_property_boolean_get(op->ptr, prop)) {
			t->flag |= T_MIRROR;
			t->mirror = 1;
		}
	}
	// Need stuff to take it from edit mesh or whatnot here
	else if (t->spacetype == SPACE_VIEW3D) {
		if (t->obedit && t->obedit->type == OB_MESH && (((Mesh *)t->obedit->data)->editflag & ME_EDIT_MIRROR_X)) {
			t->flag |= T_MIRROR;
			t->mirror = 1;
		}
	}
	
	/* setting PET flag only if property exist in operator. Otherwise, assume it's not supported */
	if (op && (prop = RNA_struct_find_property(op->ptr, "proportional"))) {
		if (RNA_property_is_set(op->ptr, prop)) {
			t->flag |= initTransInfo_edit_pet_to_flag(RNA_property_enum_get(op->ptr, prop));
		}
		else {
			/* use settings from scene only if modal */
			if (t->flag & T_MODAL) {
				if ((t->options & CTX_NO_PET) == 0) {
					if (t->obedit) {
						t->flag |= initTransInfo_edit_pet_to_flag(ts->proportional);
					}
					else if (t->options & CTX_MASK) {
						if (ts->proportional_mask) {
							t->flag |= T_PROP_EDIT;

							if (ts->proportional == PROP_EDIT_CONNECTED) {
								t->flag |= T_PROP_CONNECTED;
							}
						}
					}
					else if (t->obedit == NULL && ts->proportional_objects) {
						t->flag |= T_PROP_EDIT;
					}
				}
			}
		}
		
		if (op && ((prop = RNA_struct_find_property(op->ptr, "proportional_size")) &&
		           RNA_property_is_set(op->ptr, prop)))
		{
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
		           RNA_property_is_set(op->ptr, prop)))
		{
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

	setTransformViewMatrices(t);
	initNumInput(&t->num);
}

/* Here I would suggest only TransInfo related issues, like free data & reset vars. Not redraws */
void postTrans(bContext *C, TransInfo *t)
{
	TransData *td;
	
	if (t->draw_handle_view)
		ED_region_draw_cb_exit(t->ar->type, t->draw_handle_view);
	if (t->draw_handle_apply)
		ED_region_draw_cb_exit(t->ar->type, t->draw_handle_apply);
	if (t->draw_handle_pixel)
		ED_region_draw_cb_exit(t->ar->type, t->draw_handle_pixel);
	if (t->draw_handle_cursor)
		WM_paint_cursor_end(CTX_wm_manager(C), t->draw_handle_cursor);

	if (t->customFree) {
		/* Can take over freeing t->data and data2d etc... */
		t->customFree(t);
		BLI_assert(t->customData == NULL);
	}
	else if ((t->customData != NULL) && (t->flag & T_FREE_CUSTOMDATA)) {
		MEM_freeN(t->customData);
		t->customData = NULL;
	}

	/* postTrans can be called when nothing is selected, so data is NULL already */
	if (t->data) {
		
		/* free data malloced per trans-data */
		if ((t->obedit && ELEM(t->obedit->type, OB_CURVE, OB_SURF)) ||
		    (t->spacetype == SPACE_IPO))
		{
			int a;
			for (a = 0, td = t->data; a < t->total; a++, td++) {
				if (td->flag & TD_BEZTRIPLE) {
					MEM_freeN(td->hdata);
				}
			}
		}
		MEM_freeN(t->data);
	}
	
	BLI_freelistN(&t->tsnap.points);

	if (t->ext) MEM_freeN(t->ext);
	if (t->data2d) {
		MEM_freeN(t->data2d);
		t->data2d = NULL;
	}
	
	if (t->spacetype == SPACE_IMAGE) {
		if (t->options & (CTX_MASK | CTX_PAINT_CURVE)) {
			/* pass */
		}
		else {
			SpaceImage *sima = t->sa->spacedata.first;
			if (sima->flag & SI_LIVE_UNWRAP)
				ED_uvedit_live_unwrap_end(t->state == TRANS_CANCEL);
		}
	}
	else if (t->spacetype == SPACE_VIEW3D) {
		View3D *v3d = t->sa->spacedata.first;
		/* restore manipulator */
		if (t->flag & T_MODAL) {
			v3d->twtype = t->twtype;
		}
	}
	
	if (t->mouse.data) {
		MEM_freeN(t->mouse.data);
	}
}

void applyTransObjects(TransInfo *t)
{
	TransData *td;
	
	for (td = t->data; td < t->data + t->total; td++) {
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
	TransData *td;
	TransData2D *td2d;

	for (td = t->data; td < t->data + t->total; td++) {
		restoreElement(td);
	}
	
	for (td2d = t->data2d; t->data2d && td2d < t->data2d + t->total; td2d++) {
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
	
	recalcData(t);
}

void calculateCenter2D(TransInfo *t)
{
	if (t->flag & (T_EDIT | T_POSE)) {
		Object *ob = t->obedit ? t->obedit : t->poseobj;
		float vec[3];
		
		copy_v3_v3(vec, t->center);
		mul_m4_v3(ob->obmat, vec);
		projectFloatView(t, vec, t->center2d);
	}
	else {
		projectFloatView(t, t->center, t->center2d);
	}
}

void calculateCenterCursor(TransInfo *t, float r_center[3])
{
	const float *cursor;
	
	cursor = ED_view3d_cursor3d_get(t->scene, t->view);
	copy_v3_v3(r_center, cursor);
	
	/* If edit or pose mode, move cursor in local space */
	if (t->flag & (T_EDIT | T_POSE)) {
		Object *ob = t->obedit ? t->obedit : t->poseobj;
		float mat[3][3], imat[3][3];
		
		sub_v3_v3v3(r_center, r_center, ob->obmat[3]);
		copy_m3_m4(mat, ob->obmat);
		invert_m3_m3(imat, mat);
		mul_m3_v3(imat, r_center);
	}
	else if (t->options & CTX_PAINT_CURVE) {
		if (ED_view3d_project_float_global(t->ar, cursor, r_center, V3D_PROJ_TEST_NOP) != V3D_PROJ_RET_OK) {
			r_center[0] = t->ar->winx / 2.0f;
			r_center[1] = t->ar->winy / 2.0f;
		}
		r_center[2] = 0.0f;
	}
}

void calculateCenterCursor2D(TransInfo *t, float r_center[2])
{
	float aspx = 1.0, aspy = 1.0;
	const float *cursor = NULL;
	
	if (t->spacetype == SPACE_IMAGE) {
		SpaceImage *sima = (SpaceImage *)t->sa->spacedata.first;
		if (t->options & CTX_MASK) {
			ED_space_image_get_aspect(sima, &aspx, &aspy);
		}
		else {
			ED_space_image_get_uv_aspect(sima, &aspx, &aspy);
		}
		cursor = sima->cursor;
	}
	else if (t->spacetype == SPACE_CLIP) {
		SpaceClip *space_clip = (SpaceClip *) t->sa->spacedata.first;
		if (t->options & CTX_MOVIECLIP) {
			ED_space_clip_get_aspect_dimension_aware(space_clip, &aspx, &aspy);
		}
		else {
			ED_space_clip_get_aspect(space_clip, &aspx, &aspy);
		}
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
				SpaceClip *space_clip = (SpaceClip *) t->sa->spacedata.first;
				BKE_mask_coord_from_movieclip(space_clip->clip, &space_clip->user, co, cursor);
			}
			else {
				BLI_assert(!"Shall not happen");
			}

			r_center[0] = co[0] * aspx;
			r_center[1] = co[1] * aspy;
		}
		else if (t->options & CTX_PAINT_CURVE) {
			if (t->spacetype == SPACE_IMAGE) {
				r_center[0] = UI_view2d_view_to_region_x(&t->ar->v2d, cursor[0]);
				r_center[1] = UI_view2d_view_to_region_y(&t->ar->v2d, cursor[1]);
			}
		}
		else {
			r_center[0] = cursor[0] * aspx;
			r_center[1] = cursor[1] * aspy;
		}
	}
}

void calculateCenterCursorGraph2D(TransInfo *t, float r_center[2])
{
	SpaceIpo *sipo = (SpaceIpo *)t->sa->spacedata.first;
	Scene *scene = t->scene;
	
	/* cursor is combination of current frame, and graph-editor cursor value */
	r_center[0] = (float)(scene->r.cfra);
	r_center[1] = sipo->cursorVal;
}

void calculateCenterMedian(TransInfo *t, float r_center[3])
{
	float partial[3] = {0.0f, 0.0f, 0.0f};
	int total = 0;
	int i;
	
	for (i = 0; i < t->total; i++) {
		if (t->data[i].flag & TD_SELECTED) {
			if (!(t->data[i].flag & TD_NOCENTER)) {
				add_v3_v3(partial, t->data[i].center);
				total++;
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
	float max[3];
	float min[3];
	int i;
	for (i = 0; i < t->total; i++) {
		if (i) {
			if (t->data[i].flag & TD_SELECTED) {
				if (!(t->data[i].flag & TD_NOCENTER))
					minmax_v3v3_v3(min, max, t->data[i].center);
			}
		}
		else {
			copy_v3_v3(max, t->data[i].center);
			copy_v3_v3(min, t->data[i].center);
		}
	}
	mid_v3_v3v3(r_center, min, max);
}

/**
 * \param select_only only get active center from data being transformed.
 */
bool calculateCenterActive(TransInfo *t, bool select_only, float r_center[3])
{
	bool ok = false;

	if (t->obedit) {
		switch (t->obedit->type) {
			case OB_MESH:
			{
				BMEditSelection ese;
				BMEditMesh *em = BKE_editmesh_from_object(t->obedit);

				if (BM_select_history_active_get(em->bm, &ese)) {
					BM_editselection_center(&ese, r_center);
					ok = true;
				}
				break;
			}
			case OB_ARMATURE:
			{
				bArmature *arm = t->obedit->data;
				EditBone *ebo = arm->act_edbone;

				if (ebo && (!select_only || (ebo->flag & (BONE_SELECTED | BONE_ROOTSEL)))) {
					copy_v3_v3(r_center, ebo->head);
					ok = true;
				}

				break;
			}
			case OB_CURVE:
			case OB_SURF:
			{
				float center[3];
				Curve *cu = (Curve *)t->obedit->data;

				if (ED_curve_active_center(cu, center)) {
					copy_v3_v3(r_center, center);
					ok = true;
				}
				break;
			}
			case OB_MBALL:
			{
				MetaBall *mb = (MetaBall *)t->obedit->data;
				MetaElem *ml_act = mb->lastelem;

				if (ml_act && (!select_only || (ml_act->flag & SELECT))) {
					copy_v3_v3(r_center, &ml_act->x);
					ok = true;
				}
				break;
			}
			case OB_LATTICE:
			{
				BPoint *actbp = BKE_lattice_active_point_get(t->obedit->data);

				if (actbp) {
					copy_v3_v3(r_center, actbp->vec);
					ok = true;
				}
				break;
			}
		}
	}
	else if (t->flag & T_POSE) {
		Scene *scene = t->scene;
		Object *ob = OBACT;
		if (ob) {
			bPoseChannel *pchan = BKE_pose_channel_active(ob);
			if (pchan && (!select_only || (pchan->bone->flag & BONE_SELECTED))) {
				copy_v3_v3(r_center, pchan->pose_head);
				ok = true;
			}
		}
	}
	else if (t->options & CTX_PAINT_CURVE) {
		Paint *p = BKE_paint_get_active(t->scene);
		Brush *br = p->brush;
		PaintCurve *pc = br->paint_curve;
		copy_v3_v3(r_center, pc->points[pc->add_index - 1].bez.vec[1]);
		r_center[2] = 0.0f;
		ok = true;
	}
	else {
		/* object mode */
		Scene *scene = t->scene;
		Object *ob = OBACT;
		if (ob && (!select_only || (ob->flag & SELECT))) {
			copy_v3_v3(r_center, ob->obmat[3]);
			ok = true;
		}
	}

	return ok;
}


void calculateCenter(TransInfo *t)
{
	switch (t->around) {
		case V3D_CENTER:
			calculateCenterBound(t, t->center);
			break;
		case V3D_CENTROID:
			calculateCenterMedian(t, t->center);
			break;
		case V3D_CURSOR:
			if (ELEM(t->spacetype, SPACE_IMAGE, SPACE_CLIP))
				calculateCenterCursor2D(t, t->center);
			else if (t->spacetype == SPACE_IPO)
				calculateCenterCursorGraph2D(t, t->center);
			else
				calculateCenterCursor(t, t->center);
			break;
		case V3D_LOCAL:
			/* Individual element center uses median center for helpline and such */
			calculateCenterMedian(t, t->center);
			break;
		case V3D_ACTIVE:
		{
			if (calculateCenterActive(t, false, t->center)) {
				/* pass */
			}
			else {
				/* fallback */
				calculateCenterMedian(t, t->center);
			}
			break;
		}
	}

	calculateCenter2D(t);

	/* setting constraint center */
	copy_v3_v3(t->con.center, t->center);
	if (t->flag & (T_EDIT | T_POSE)) {
		Object *ob = t->obedit ? t->obedit : t->poseobj;
		mul_m4_v3(ob->obmat, t->con.center);
	}
	
	/* for panning from cameraview */
	if (t->flag & T_OBJECT) {
		if (t->spacetype == SPACE_VIEW3D && t->ar && t->ar->regiontype == RGN_TYPE_WINDOW) {
			
			if (t->flag & T_CAMERA) {
				float axis[3];
				/* persinv is nasty, use viewinv instead, always right */
				copy_v3_v3(axis, t->viewinv[2]);
				normalize_v3(axis);
				
				/* 6.0 = 6 grid units */
				axis[0] = t->center[0] - 6.0f * axis[0];
				axis[1] = t->center[1] - 6.0f * axis[1];
				axis[2] = t->center[2] - 6.0f * axis[2];
				
				projectFloatView(t, axis, t->center2d);
				
				/* rotate only needs correct 2d center, grab needs ED_view3d_calc_zfac() value */
				if (t->mode == TFM_TRANSLATION) {
					copy_v3_v3(t->center, axis);
					copy_v3_v3(t->con.center, t->center);
				}
			}
		}
	}
	
	if (t->spacetype == SPACE_VIEW3D) {
		/* ED_view3d_calc_zfac() defines a factor for perspective depth correction, used in ED_view3d_win_to_delta() */
		float vec[3];
		if (t->flag & (T_EDIT | T_POSE)) {
			Object *ob = t->obedit ? t->obedit : t->poseobj;
			mul_v3_m4v3(vec, ob->obmat, t->center);
		}
		else {
			copy_v3_v3(vec, t->center);
		}

		/* zfac is only used convertViewVec only in cases operator was invoked in RGN_TYPE_WINDOW
		 * and never used in other cases.
		 *
		 * We need special case here as well, since ED_view3d_calc_zfac will crahs when called
		 * for a region different from RGN_TYPE_WINDOW.
		 */
		if (t->ar->regiontype == RGN_TYPE_WINDOW) {
			t->zfac = ED_view3d_calc_zfac(t->ar->regiondata, vec, NULL);
		}
		else {
			t->zfac = 0.0f;
		}
	}
}

void calculatePropRatio(TransInfo *t)
{
	TransData *td = t->data;
	int i;
	float dist;
	short connected = t->flag & T_PROP_CONNECTED;

	if (t->flag & T_PROP_EDIT) {
		for (i = 0; i < t->total; i++, td++) {
			if (td->flag & TD_SELECTED) {
				td->factor = 1.0f;
			}
			else if (t->flag & T_MIRROR && td->loc[0] * t->mirror < -0.00001f) {
				td->flag |= TD_SKIP;
				td->factor = 0.0f;
				restoreElement(td);
			}
			else if ((connected && (td->flag & TD_NOTCONNECTED || td->dist > t->prop_size)) ||
			         (connected == 0 && td->rdist > t->prop_size))
			{
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
				
				if (connected)
					dist = (t->prop_size - td->dist) / t->prop_size;
				else
					dist = (t->prop_size - td->rdist) / t->prop_size;

				/*
				 * Clamp to positive numbers.
				 * Certain corner cases with connectivity and individual centers
				 * can give values of rdist larger than propsize.
				 */
				if (dist < 0.0f)
					dist = 0.0f;
				
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
						td->factor = BLI_frand() * dist;
						break;
					default:
						td->factor = 1;
						break;
				}
			}
		}
		switch (t->prop_mode) {
			case PROP_SHARP:
				strcpy(t->proptext, IFACE_("(Sharp)"));
				break;
			case PROP_SMOOTH:
				strcpy(t->proptext, IFACE_("(Smooth)"));
				break;
			case PROP_ROOT:
				strcpy(t->proptext, IFACE_("(Root)"));
				break;
			case PROP_LIN:
				strcpy(t->proptext, IFACE_("(Linear)"));
				break;
			case PROP_CONST:
				strcpy(t->proptext, IFACE_("(Constant)"));
				break;
			case PROP_SPHERE:
				strcpy(t->proptext, IFACE_("(Sphere)"));
				break;
			case PROP_RANDOM:
				strcpy(t->proptext, IFACE_("(Random)"));
				break;
			default:
				t->proptext[0] = '\0';
				break;
		}
	}
	else {
		for (i = 0; i < t->total; i++, td++) {
			td->factor = 1.0;
		}
		t->proptext[0] = '\0';
	}
}
