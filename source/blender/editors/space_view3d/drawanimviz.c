/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 by the Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_ID.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_dlrbTree.h"

#include "BKE_anim.h"
#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_nla.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_armature.h"
#include "ED_anim_api.h"
#include "ED_keyframes_draw.h"

#include "WM_api.h"
#include "WM_types.h"
#include "BLF_api.h"

#include "UI_resources.h"

#include "view3d_intern.h"

/* ************************************ Motion Paths ************************************* */

// TODO: 
//	- options to draw paths with lines
//	- include support for editing the path verts

/* Set up drawing environment for drawing motion paths */
void draw_motion_paths_init(Scene *scene, View3D *v3d, ARegion *ar) 
{
	RegionView3D *rv3d= ar->regiondata;
	
	if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
	
	glPushMatrix();
	glLoadMatrixf(rv3d->viewmat);
}

/* Draw the given motion path for an Object or a Bone 
 * 	- assumes that the viewport has already been initialised properly
 *		i.e. draw_motion_paths_init() has been called
 */
void draw_motion_path_instance(Scene *scene, View3D *v3d, ARegion *ar, 
			Object *ob, bPoseChannel *pchan, bAnimVizSettings *avs, bMotionPath *mpath)
{
	//RegionView3D *rv3d= ar->regiondata;
	bMotionPathVert *mpv, *mpv_start;
	int i, stepsize= avs->path_step;
	int sfra, efra, len;
	
	
	/* get frame ranges */
	if (avs->path_type == MOTIONPATH_TYPE_ACFRA) {
		int sind;
		
		/* With "Around Current", we only choose frames from around 
		 * the current frame to draw. However, this range is still 
		 * restricted by the limits of the original path.
		 */
		sfra= CFRA - avs->path_bc;
		efra= CFRA + avs->path_ac;
		if (sfra < mpath->start_frame) sfra= mpath->start_frame;
		if (efra > mpath->end_frame) efra= mpath->end_frame;
		
		len= efra - sfra;
		
		sind= sfra - mpath->start_frame;
		mpv_start= (mpath->points + sind);
	}
	else {
		sfra= mpath->start_frame;
		efra = sfra + mpath->length;
		len = mpath->length;
		mpv_start= mpath->points;
	}
	
	/* draw curve-line of path */
	glShadeModel(GL_SMOOTH);
	
	glBegin(GL_LINE_STRIP); 				
	for (i=0, mpv=mpv_start; i < len; i++, mpv++) {
		short sel= (pchan) ? (pchan->bone->flag & BONE_SELECTED) : (ob->flag & SELECT);
		float intensity; /* how faint */
		
		/* set color
		 * 	- more intense for active/selected bones, less intense for unselected bones
		 * 	- black for before current frame, green for current frame, blue for after current frame
		 * 	- intensity decreases as distance from current frame increases
		 */
		#define SET_INTENSITY(A, B, C, min, max) (((1.0f - ((C - B) / (C - A))) * (max-min)) + min) 
		if ((sfra+i) < CFRA) {
			/* black - before cfra */
			if (sel) {
				// intensity= 0.5f;
				intensity = SET_INTENSITY(sfra, i, CFRA, 0.25f, 0.75f);
			}
			else {
				//intensity= 0.8f;
				intensity = SET_INTENSITY(sfra, i, CFRA, 0.68f, 0.92f);
			}
			UI_ThemeColorBlend(TH_WIRE, TH_BACK, intensity);
		}
		else if ((sfra+i) > CFRA) {
			/* blue - after cfra */
			if (sel) {
				//intensity = 0.5f;
				intensity = SET_INTENSITY(CFRA, i, efra, 0.25f, 0.75f);
			}
			else {
				//intensity = 0.8f;
				intensity = SET_INTENSITY(CFRA, i, efra, 0.68f, 0.92f);
			}
			UI_ThemeColorBlend(TH_BONE_POSE, TH_BACK, intensity);
		}
		else {
			/* green - on cfra */
			if (sel) {
				intensity= 0.5f;
			}
			else {
				intensity= 0.99f;
			}
			UI_ThemeColorBlendShade(TH_CFRAME, TH_BACK, intensity, 10);
		}	
		
		/* draw a vertex with this color */ 
		glVertex3fv(mpv->co);
	}
	
	glEnd();
	glShadeModel(GL_FLAT);
	
	glPointSize(1.0);
	
	/* draw little black point at each frame
	 * NOTE: this is not really visible/noticable
	 */
	glBegin(GL_POINTS);
	for (i=0, mpv=mpv_start; i < len; i++, mpv++) 
		glVertex3fv(mpv->co);
	glEnd();
	
	/* Draw little white dots at each framestep value */
	UI_ThemeColor(TH_TEXT_HI);
	glBegin(GL_POINTS);
	for (i=0, mpv=mpv_start; i < len; i+=stepsize, mpv+=stepsize) 
		glVertex3fv(mpv->co);
	glEnd();
	
	/* Draw frame numbers at each framestep value */
	if (avs->path_viewflag & MOTIONPATH_VIEW_FNUMS) {
		for (i=0, mpv=mpv_start; i < len; i+=stepsize, mpv+=stepsize) {
			char str[32];
			
			/* only draw framenum if several consecutive highlighted points don't occur on same point */
			if (i == 0) {
				sprintf(str, "%d", (i+sfra));
				view3d_cached_text_draw_add(mpv->co[0], mpv->co[1], mpv->co[2], str, 0);
			}
			else if ((i > stepsize) && (i < len-stepsize)) { 
				bMotionPathVert *mpvP = (mpv - stepsize);
				bMotionPathVert *mpvN = (mpv + stepsize);
				
				if ((equals_v3v3(mpv->co, mpvP->co)==0) || (equals_v3v3(mpv->co, mpvN->co)==0)) {
					sprintf(str, "%d", (sfra+i));
					view3d_cached_text_draw_add(mpv->co[0], mpv->co[1], mpv->co[2], str, 0);
				}
			}
		}
	}

	/* Keyframes - dots and numbers */
	if (avs->path_viewflag & MOTIONPATH_VIEW_KFNOS) {
		AnimData *adt= BKE_animdata_from_id(&ob->id);
		DLRBT_Tree keys;
		
		/* build list of all keyframes in active action for object or pchan */
		BLI_dlrbTree_init(&keys);
		
		if (adt) {
			/* for now, it is assumed that keyframes for bones are all grouped in a single group */
			if (pchan) {
				bActionGroup *agrp= action_groups_find_named(adt->action, pchan->name);
				
				if (agrp) {
					agroup_to_keylist(adt, agrp, &keys, NULL);
					BLI_dlrbTree_linkedlist_sync(&keys);
				}
			}
			else {
				action_to_keylist(adt, adt->action, &keys, NULL);
				BLI_dlrbTree_linkedlist_sync(&keys);
			}
		}
		
		/* Draw slightly-larger yellow dots at each keyframe */
		UI_ThemeColor(TH_VERTEX_SELECT);
		glPointSize(4.0f); // XXX perhaps a bit too big
		
		glBegin(GL_POINTS);
		for (i=0, mpv=mpv_start; i < len; i++, mpv++) {
			float mframe= (float)(sfra + i);
			
			if (BLI_dlrbTree_search_exact(&keys, compare_ak_cfraPtr, &mframe))
				glVertex3fv(mpv->co);
		}
		glEnd();
		
		glPointSize(1.0f);
		
		/* Draw frame numbers of keyframes  */
		if (avs->path_viewflag & (MOTIONPATH_VIEW_FNUMS|MOTIONPATH_VIEW_KFNOS)) {
			for (i=0, mpv=mpv_start; i < len; i++, mpv++) {
				float mframe= (float)(sfra + i);
				
				if (BLI_dlrbTree_search_exact(&keys, compare_ak_cfraPtr, &mframe)) {
					char str[32];
					
					sprintf(str, "%d", (sfra+i));
					view3d_cached_text_draw_add(mpv->co[0], mpv->co[1], mpv->co[2], str, 0);
				}
			}
		}
		
		BLI_dlrbTree_free(&keys);
	}
}

/* Clean up drawing environment after drawing motion paths */
void draw_motion_paths_cleanup(Scene *scene, View3D *v3d, ARegion *ar)
{
	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
	glPopMatrix();
}

#if 0 // XXX temp file guards 

/* ***************************** Onion Skinning (Ghosts) ******************************** */

#if 0 // XXX only for bones
/* helper function for ghost drawing - sets/removes flags for temporarily 
 * hiding unselected bones while drawing ghosts
 */
static void ghost_poses_tag_unselected(Object *ob, short unset)
{
	bArmature *arm= ob->data;
	bPose *pose= ob->pose;
	bPoseChannel *pchan;
	
	/* don't do anything if no hiding any bones */
	if ((arm->flag & ARM_GHOST_ONLYSEL)==0)
		return;
		
	/* loop over all pchans, adding/removing tags as appropriate */
	for (pchan= pose->chanbase.first; pchan; pchan= pchan->next) {
		if ((pchan->bone) && (arm->layer & pchan->bone->layer)) {
			if (unset) {
				/* remove tags from all pchans if cleaning up */
				pchan->bone->flag &= ~BONE_HIDDEN_PG;
			}
			else {
				/* set tags on unselected pchans only */
				if ((pchan->bone->flag & BONE_SELECTED)==0)
					pchan->bone->flag |= BONE_HIDDEN_PG;
			}
		}
	}
}
#endif // XXX only for bones

/* draw ghosts that occur within a frame range 
 * 	note: object should be in posemode 
 */
static void draw_ghost_poses_range(Scene *scene, View3D *v3d, ARegion *ar, Base *base)
{
	Object *ob= base->object;
	AnimData *adt= BKE_animdata_from_id(&ob->id);
	bArmature *arm= ob->data;
	bPose *posen, *poseo;
	float start, end, stepsize, range, colfac;
	int cfrao, flago, ipoflago;
	
	start = (float)arm->ghostsf;
	end = (float)arm->ghostef;
	if (end <= start)
		return;
	
	stepsize= (float)(arm->ghostsize);
	range= (float)(end - start);
	
	/* store values */
	ob->mode &= ~OB_MODE_POSE;
	cfrao= CFRA;
	flago= arm->flag;
	arm->flag &= ~(ARM_DRAWNAMES|ARM_DRAWAXES);
	ipoflago= ob->ipoflag; 
	ob->ipoflag |= OB_DISABLE_PATH;
	
	/* copy the pose */
	poseo= ob->pose;
	copy_pose(&posen, ob->pose, 1);
	ob->pose= posen;
	armature_rebuild_pose(ob, ob->data);	/* child pointers for IK */
	ghost_poses_tag_unselected(ob, 0);		/* hide unselected bones if need be */
	
	glEnable(GL_BLEND);
	if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
	
	/* draw from first frame of range to last */
	for (CFRA= (int)start; CFRA < end; CFRA += (int)stepsize) {
		colfac = (end - (float)CFRA) / range;
		UI_ThemeColorShadeAlpha(TH_WIRE, 0, -128-(int)(120.0*sqrt(colfac)));
		
		BKE_animsys_evaluate_animdata(&ob->id, adt, (float)CFRA, ADT_RECALC_ALL);
		where_is_pose(scene, ob);
		draw_pose_bones(scene, v3d, ar, base, OB_WIRE);
	}
	glDisable(GL_BLEND);
	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);

	ghost_poses_tag_unselected(ob, 1);		/* unhide unselected bones if need be */
	free_pose(posen);
	
	/* restore */
	CFRA= cfrao;
	ob->pose= poseo;
	arm->flag= flago;
	armature_rebuild_pose(ob, ob->data);
	ob->mode |= OB_MODE_POSE;
	ob->ipoflag= ipoflago; 
}

/* draw ghosts on keyframes in action within range 
 *	- object should be in posemode 
 */
static void draw_ghost_poses_keys(Scene *scene, View3D *v3d, ARegion *ar, Base *base)
{
	Object *ob= base->object;
	AnimData *adt= BKE_animdata_from_id(&ob->id);
	bAction *act= (adt) ? adt->action : NULL;
	bArmature *arm= ob->data;
	bPose *posen, *poseo;
	DLRBT_Tree keys;
	ActKeyColumn *ak, *akn;
	float start, end, range, colfac, i;
	int cfrao, flago;
	
	start = (float)arm->ghostsf;
	end = (float)arm->ghostef;
	if (end <= start)
		return;
	
	/* get keyframes - then clip to only within range */
	BLI_dlrbTree_init(&keys);
	action_to_keylist(adt, act, &keys, NULL);
	BLI_dlrbTree_linkedlist_sync(&keys);
	
	range= 0;
	for (ak= keys.first; ak; ak= akn) {
		akn= ak->next;
		
		if ((ak->cfra < start) || (ak->cfra > end))
			BLI_freelinkN((ListBase *)&keys, ak);
		else
			range++;
	}
	if (range == 0) return;
	
	/* store values */
	ob->mode &= ~OB_MODE_POSE;
	cfrao= CFRA;
	flago= arm->flag;
	arm->flag &= ~(ARM_DRAWNAMES|ARM_DRAWAXES);
	ob->ipoflag |= OB_DISABLE_PATH;
	
	/* copy the pose */
	poseo= ob->pose;
	copy_pose(&posen, ob->pose, 1);
	ob->pose= posen;
	armature_rebuild_pose(ob, ob->data);	/* child pointers for IK */
	ghost_poses_tag_unselected(ob, 0);		/* hide unselected bones if need be */
	
	glEnable(GL_BLEND);
	if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
	
	/* draw from first frame of range to last */
	for (ak=keys.first, i=0; ak; ak=ak->next, i++) {
		colfac = i/range;
		UI_ThemeColorShadeAlpha(TH_WIRE, 0, -128-(int)(120.0*sqrt(colfac)));
		
		CFRA= (int)ak->cfra;
		
		BKE_animsys_evaluate_animdata(&ob->id, adt, (float)CFRA, ADT_RECALC_ALL);
		where_is_pose(scene, ob);
		draw_pose_bones(scene, v3d, ar, base, OB_WIRE);
	}
	glDisable(GL_BLEND);
	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);

	ghost_poses_tag_unselected(ob, 1);		/* unhide unselected bones if need be */
	BLI_dlrbTree_free(&keys);
	free_pose(posen);
	
	/* restore */
	CFRA= cfrao;
	ob->pose= poseo;
	arm->flag= flago;
	armature_rebuild_pose(ob, ob->data);
	ob->mode |= OB_MODE_POSE;
}

/* draw ghosts around current frame
 * 	- object is supposed to be armature in posemode 
 */
static void draw_ghost_poses(Scene *scene, View3D *v3d, ARegion *ar, Base *base)
{
	Object *ob= base->object;
	AnimData *adt= BKE_animdata_from_id(&ob->id);
	bArmature *arm= ob->data;
	bPose *posen, *poseo;
	float cur, start, end, stepsize, range, colfac, actframe, ctime;
	int cfrao, flago;
	
	/* pre conditions, get an action with sufficient frames */
	if ELEM(NULL, adt, adt->action)
		return;

	calc_action_range(adt->action, &start, &end, 0);
	if (start == end)
		return;

	stepsize= (float)(arm->ghostsize);
	range= (float)(arm->ghostep)*stepsize + 0.5f;	/* plus half to make the for loop end correct */
	
	/* store values */
	ob->mode &= ~OB_MODE_POSE;
	cfrao= CFRA;
	actframe= BKE_nla_tweakedit_remap(adt, (float)CFRA, 0);
	flago= arm->flag;
	arm->flag &= ~(ARM_DRAWNAMES|ARM_DRAWAXES);
	
	/* copy the pose */
	poseo= ob->pose;
	copy_pose(&posen, ob->pose, 1);
	ob->pose= posen;
	armature_rebuild_pose(ob, ob->data);	/* child pointers for IK */
	ghost_poses_tag_unselected(ob, 0);		/* hide unselected bones if need be */
	
	glEnable(GL_BLEND);
	if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
	
	/* draw from darkest blend to lowest */
	for(cur= stepsize; cur<range; cur+=stepsize) {
		ctime= cur - (float)fmod(cfrao, stepsize);	/* ensures consistant stepping */
		colfac= ctime/range;
		UI_ThemeColorShadeAlpha(TH_WIRE, 0, -128-(int)(120.0*sqrt(colfac)));
		
		/* only within action range */
		if (actframe+ctime >= start && actframe+ctime <= end) {
			CFRA= (int)BKE_nla_tweakedit_remap(adt, actframe+ctime, NLATIME_CONVERT_MAP);
			
			if (CFRA != cfrao) {
				BKE_animsys_evaluate_animdata(&ob->id, adt, (float)CFRA, ADT_RECALC_ALL);
				where_is_pose(scene, ob);
				draw_pose_bones(scene, v3d, ar, base, OB_WIRE);
			}
		}
		
		ctime= cur + (float)fmod((float)cfrao, stepsize) - stepsize+1.0f;	/* ensures consistant stepping */
		colfac= ctime/range;
		UI_ThemeColorShadeAlpha(TH_WIRE, 0, -128-(int)(120.0*sqrt(colfac)));
		
		/* only within action range */
		if ((actframe-ctime >= start) && (actframe-ctime <= end)) {
			CFRA= (int)BKE_nla_tweakedit_remap(adt, actframe-ctime, NLATIME_CONVERT_MAP);
			
			if (CFRA != cfrao) {
				BKE_animsys_evaluate_animdata(&ob->id, adt, (float)CFRA, ADT_RECALC_ALL);
				where_is_pose(scene, ob);
				draw_pose_bones(scene, v3d, ar, base, OB_WIRE);
			}
		}
	}
	glDisable(GL_BLEND);
	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);

	ghost_poses_tag_unselected(ob, 1);		/* unhide unselected bones if need be */
	free_pose(posen);
	
	/* restore */
	CFRA= cfrao;
	ob->pose= poseo;
	arm->flag= flago;
	armature_rebuild_pose(ob, ob->data);
	ob->mode |= OB_MODE_POSE;
}



#endif // XXX temp file guards
