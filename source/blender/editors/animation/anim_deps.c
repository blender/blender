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
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_scene.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"


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
void ED_update_for_newframe(bContext *C, int mute)
{
	bScreen *screen= CTX_wm_screen(C);
	Scene *scene= screen->scene;
	
	//extern void audiostream_scrub(unsigned int frame);	/* seqaudio.c */
	
	/* this function applies the changes too */
	/* XXX future: do all windows */
	scene_update_for_newframe(scene, screen_view3d_layers(screen)); /* BKE_scene.h */
	
	//if ( (CFRA>1) && (!mute) && (G.scene->audio.flag & AUDIO_SCRUB)) 
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

