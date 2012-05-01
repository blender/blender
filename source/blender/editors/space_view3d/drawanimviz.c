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
 * The Original Code is Copyright (C) 2009 by the Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/drawanimviz.c
 *  \ingroup spview3d
 */


#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "BLO_sys_types.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_dlrbTree.h"

#include "BKE_animsys.h"
#include "BKE_action.h"

#include "BIF_gl.h"

#include "ED_armature.h"
#include "ED_keyframes_draw.h"

#include "BLF_api.h"

#include "UI_resources.h"

#include "view3d_intern.h"

/* ************************************ Motion Paths ************************************* */

// TODO: 
//	- options to draw paths with lines
//	- include support for editing the path verts

/* Set up drawing environment for drawing motion paths */
void draw_motion_paths_init(View3D *v3d, ARegion *ar) 
{
	RegionView3D *rv3d = ar->regiondata;
	
	if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
	
	glPushMatrix();
	glLoadMatrixf(rv3d->viewmat);
}

/* Draw the given motion path for an Object or a Bone 
 *  - assumes that the viewport has already been initialized properly
 *		i.e. draw_motion_paths_init() has been called
 */
void draw_motion_path_instance(Scene *scene, 
                               Object *ob, bPoseChannel *pchan, bAnimVizSettings *avs, bMotionPath *mpath)
{
	//RegionView3D *rv3d= ar->regiondata;
	bMotionPathVert *mpv, *mpv_start;
	int i, stepsize = avs->path_step;
	int sfra, efra, len;
	
	
	/* get frame ranges */
	if (avs->path_type == MOTIONPATH_TYPE_ACFRA) {
		int sind;
		
		/* With "Around Current", we only choose frames from around 
		 * the current frame to draw. However, this range is still 
		 * restricted by the limits of the original path.
		 */
		sfra = CFRA - avs->path_bc;
		efra = CFRA + avs->path_ac;
		if (sfra < mpath->start_frame) sfra = mpath->start_frame;
		if (efra > mpath->end_frame) efra = mpath->end_frame;
		
		len = efra - sfra;
		
		sind = sfra - mpath->start_frame;
		mpv_start = (mpath->points + sind);
	}
	else {
		sfra = mpath->start_frame;
		efra = sfra + mpath->length;
		len = mpath->length;
		mpv_start = mpath->points;
	}

	if (len <= 0) {
		return;
	}

	/* draw curve-line of path */
	glShadeModel(GL_SMOOTH);
	
	glBegin(GL_LINE_STRIP); 				
	for (i = 0, mpv = mpv_start; i < len; i++, mpv++) {
		short sel = (pchan) ? (pchan->bone->flag & BONE_SELECTED) : (ob->flag & SELECT);
		float intensity; /* how faint */
		
		/* set color
		 * - more intense for active/selected bones, less intense for unselected bones
		 * - black for before current frame, green for current frame, blue for after current frame
		 * - intensity decreases as distance from current frame increases
		 */
		#define SET_INTENSITY(A, B, C, min, max) (((1.0f - ((C - B) / (C - A))) * (max - min)) + min)
		if ((sfra + i) < CFRA) {
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
		else if ((sfra + i) > CFRA) {
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
				intensity = 0.5f;
			}
			else {
				intensity = 0.99f;
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
	 * NOTE: this is not really visible/noticeable
	 */
	glBegin(GL_POINTS);
	for (i = 0, mpv = mpv_start; i < len; i++, mpv++)
		glVertex3fv(mpv->co);
	glEnd();
	
	/* Draw little white dots at each framestep value */
	UI_ThemeColor(TH_TEXT_HI);
	glBegin(GL_POINTS);
	for (i = 0, mpv = mpv_start; i < len; i += stepsize, mpv += stepsize)
		glVertex3fv(mpv->co);
	glEnd();
	
	/* Draw big green dot where the current frame is 
	 * NOTE: this is only done when keyframes are shown, since this adds similar types of clutter
	 */
	if ((avs->path_viewflag & MOTIONPATH_VIEW_KFRAS) &&
	    (sfra < CFRA) && (CFRA <= efra))
	{
		UI_ThemeColor(TH_CFRAME);
		glPointSize(6.0f);
		
		glBegin(GL_POINTS);
		mpv = mpv_start + (CFRA - sfra);
		glVertex3fv(mpv->co);
		glEnd();
		
		glPointSize(1.0f);
		UI_ThemeColor(TH_TEXT_HI);
	}
	
	// XXX, this isn't up to date but probably should be kept so.
	invert_m4_m4(ob->imat, ob->obmat);
	
	/* Draw frame numbers at each framestep value */
	if (avs->path_viewflag & MOTIONPATH_VIEW_FNUMS) {
		unsigned char col[4];
		UI_GetThemeColor3ubv(TH_TEXT_HI, col);
		col[3] = 255;

		for (i = 0, mpv = mpv_start; i < len; i += stepsize, mpv += stepsize) {
			char numstr[32];
			float co[3];
			
			/* only draw framenum if several consecutive highlighted points don't occur on same point */
			if (i == 0) {
				sprintf(numstr, "%d", (i + sfra));
				mul_v3_m4v3(co, ob->imat, mpv->co);
				view3d_cached_text_draw_add(co, numstr, 0, V3D_CACHE_TEXT_WORLDSPACE | V3D_CACHE_TEXT_ASCII, col);
			}
			else if ((i > stepsize) && (i < len - stepsize)) {
				bMotionPathVert *mpvP = (mpv - stepsize);
				bMotionPathVert *mpvN = (mpv + stepsize);
				
				if ((equals_v3v3(mpv->co, mpvP->co) == 0) || (equals_v3v3(mpv->co, mpvN->co) == 0)) {
					sprintf(numstr, "%d", (sfra + i));
					mul_v3_m4v3(co, ob->imat, mpv->co);
					view3d_cached_text_draw_add(co, numstr, 0, V3D_CACHE_TEXT_WORLDSPACE | V3D_CACHE_TEXT_ASCII, col);
				}
			}
		}
	}
	
	/* Keyframes - dots and numbers */
	if (avs->path_viewflag & MOTIONPATH_VIEW_KFRAS) {
		unsigned char col[4];
		
		AnimData *adt = BKE_animdata_from_id(&ob->id);
		DLRBT_Tree keys;
		
		/* build list of all keyframes in active action for object or pchan */
		BLI_dlrbTree_init(&keys);
		
		if (adt) {
			/* it is assumed that keyframes for bones are all grouped in a single group
			 * unless an option is set to always use the whole action
			 */
			if ((pchan) && (avs->path_viewflag & MOTIONPATH_VIEW_KFACT) == 0) {
				bActionGroup *agrp = action_groups_find_named(adt->action, pchan->name);
				
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
		UI_GetThemeColor3ubv(TH_VERTEX_SELECT, col);
		col[3] = 255;
		
		glPointSize(4.0f); // XXX perhaps a bit too big
		glColor3ubv(col);
		
		glBegin(GL_POINTS);
		for (i = 0, mpv = mpv_start; i < len; i++, mpv++) {
			float mframe = (float)(sfra + i);
			
			if (BLI_dlrbTree_search_exact(&keys, compare_ak_cfraPtr, &mframe))
				glVertex3fv(mpv->co);
		}
		glEnd();
		
		glPointSize(1.0f);
		
		/* Draw frame numbers of keyframes  */
		if (avs->path_viewflag & MOTIONPATH_VIEW_KFNOS) {
			float co[3];
			for (i = 0, mpv = mpv_start; i < len; i++, mpv++) {
				float mframe = (float)(sfra + i);
				
				if (BLI_dlrbTree_search_exact(&keys, compare_ak_cfraPtr, &mframe)) {
					char numstr[32];
					
					sprintf(numstr, "%d", (sfra + i));
					mul_v3_m4v3(co, ob->imat, mpv->co);
					view3d_cached_text_draw_add(co, numstr, 0, V3D_CACHE_TEXT_WORLDSPACE | V3D_CACHE_TEXT_ASCII, col);
				}
			}
		}
		
		BLI_dlrbTree_free(&keys);
	}
}

/* Clean up drawing environment after drawing motion paths */
void draw_motion_paths_cleanup(View3D *v3d)
{
	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
	glPopMatrix();
}
