/**
 * $Id:
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_scene.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_anim_api.h"

/* ***************** depsgraph calls and anim updates ************* */

static unsigned int screen_view3d_layers(bScreen *screen)
{
	if(screen) {
		unsigned int layer= screen->scene->lay;	/* as minimum this */
		ScrArea *sa;
		
		/* get all used view3d layers */
		for(sa= screen->areabase.first; sa; sa= sa->next) {
			if(sa->spacetype==SPACE_VIEW3D)
				layer |= ((View3D *)sa->spacedata.first)->lay;
		}
		return layer;
	}
	return 0;
}

/* generic update flush, reads from context Screen (layers) and scene */
/* this is for compliancy, later it can do all windows etc */
void ED_anim_dag_flush_update(const bContext *C)
{
	Scene *scene= CTX_data_scene(C);
	bScreen *screen= CTX_wm_screen(C);
	
	DAG_scene_flush_update(scene, screen_view3d_layers(screen), 0);
}


/* results in fully updated anim system */
/* in future sound should be on WM level, only 1 sound can play! */
void ED_update_for_newframe(const bContext *C, int mute)
{
	bScreen *screen= CTX_wm_screen(C);
	Scene *scene= screen->scene;
	
	//extern void audiostream_scrub(unsigned int frame);	/* seqaudio.c */
	
	/* this function applies the changes too */
	/* XXX future: do all windows */
	scene_update_for_newframe(scene, screen_view3d_layers(screen)); /* BKE_scene.h */
	
	//if ( (CFRA>1) && (!mute) && (scene->audio.flag & AUDIO_SCRUB)) 
	//	audiostream_scrub( CFRA );
	
	/* 3d window, preview */
	//BIF_view3d_previewrender_signal(curarea, PR_DBASE|PR_DISPRECT);
	
	/* all movie/sequence images */
	//BIF_image_update_frame();
	
	/* composite */
	if(scene->use_nodes && scene->nodetree)
		ntreeCompositTagAnimated(scene->nodetree);
	
	/* update animated texture nodes */
	{
		Tex *tex;
		for(tex= G.main->tex.first; tex; tex= tex->id.next)
			if( tex->use_nodes && tex->nodetree ) {
				ntreeTexTagAnimated( tex->nodetree );
			}
	}
}

/* **************************** pose <-> action syncing ******************************** */
/* Summary of what needs to be synced between poses and actions:
 *	1) Flags
 *		a) Visibility (only for pose to action) 
 *		b) Selection status (both ways)
 *	2) Group settings  (only for pose to action) - do we also need to make sure same groups exist?
 *	3) Grouping (only for pose to action for now)
 */

/* Notifier from Action/Dopesheet (this may be extended to include other things such as Python...)
 * Channels in action changed, so update pose channels/groups to reflect changes.
 *
 * An object (usually 'active' Object) needs to be supplied, so that its Pose-Channels can be synced with
 * the channels in its active Action.
 */
void ANIM_action_to_pose_sync (Object *ob)
{
	bAction *act= (bAction *)ob->action;
	bActionChannel *achan;
	bPoseChannel *pchan;
	
	/* error checking */
	if ((ob == NULL) || (ob->type != OB_ARMATURE) || ELEM(NULL, act, ob->pose))
		return;
	
	/* 1b) loop through all Action-Channels (there should be fewer channels to search through here in general) */
	for (achan= act->chanbase.first; achan; achan= achan->next) {
		/* find matching pose-channel */
		pchan= get_pose_channel(ob->pose, achan->name);
		
		/* sync active and selected flags */
		if (pchan && pchan->bone) {
			/* selection */
			if (achan->flag & ACHAN_SELECTED)
				pchan->bone->flag |= BONE_SELECTED;
			else
				pchan->bone->flag &= ~BONE_SELECTED;
			
			/* active */
			if (achan->flag & ACHAN_HILIGHTED)
				pchan->bone->flag |= BONE_ACTIVE;
			else
				pchan->bone->flag &= ~BONE_ACTIVE;
		}
	}
	
	// TODO: add grouping changes too? For now, these tools aren't exposed to users in animation editors yet...
} 
 
/* Notifier from 3D-View/Outliner (this is likely to include other sources too...)
 * Pose channels/groups changed, so update action channels
 *
 * An object (usually 'active' Object) needs to be supplied, so that its Pose-Channels can be synced with
 * the channels in its active Action.
 */
void ANIM_pose_to_action_sync (Object *ob)
{
	bArmature *arm= (bArmature *)ob->data;
	bAction *act= (bAction *)ob->action;
	bActionChannel *achan;
	//bActionGroup *agrp, *bgrp;
	bPoseChannel *pchan;
	
	/* error checking */
	if ((ob == NULL) || (ob->type != OB_ARMATURE) || ELEM3(NULL, arm, act, ob->pose))
		return;
		
	/* 1) loop through all Action-Channels (there should be fewer channels to search through here in general) */
	for (achan= act->chanbase.first; achan; achan= achan->next) {
		/* find matching pose-channel */
		pchan= get_pose_channel(ob->pose, achan->name);
		
		/* sync selection and visibility settings */
		if (pchan && pchan->bone) {
			/* visibility - if layer is hidden, or if bone itself is hidden */
			// XXX we may not want this happening though! (maybe we need some extra flags from context or so)
			// only if SACTION_NOHIDE==0, and saction->pin == 0, when in Action Editor mode
			if (!(pchan->bone->layer & arm->layer) || (pchan->bone->flag & BONE_HIDDEN_P))
				achan->flag |= ACHAN_HIDDEN;
			else
				achan->flag &= ~ACHAN_HIDDEN;
				
			/* selection */
			if (pchan->bone->flag & BONE_SELECTED)
				achan->flag |= ACHAN_SELECTED;
			else
				achan->flag &= ~ACHAN_SELECTED;
		}
	}
	
	// XXX step 2 needs to be coded still... currently missing action/bone group API to do any more work here...	
	// XXX step 3 needs to be coded still... it's a messy case to deal with (we'll use the temp indices for this?)
}
