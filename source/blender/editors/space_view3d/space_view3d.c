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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>

#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_global.h"
#include "BKE_screen.h"

#include "ED_area.h"

#include "BIF_gl.h"

#include "view3d_intern.h"	// own include

/* ******************** default callbacks for view3d space ***************** */

static SpaceLink *view3d_new(void)
{
	View3D *vd;
	
	vd= MEM_callocN(sizeof(View3D), "initview3d");
	
	vd->spacetype= SPACE_VIEW3D;
	vd->blockscale= 0.7f;
	vd->viewquat[0]= 1.0f;
	vd->viewquat[1]= vd->viewquat[2]= vd->viewquat[3]= 0.0f;
	vd->persp= 1;
	vd->drawtype= OB_WIRE;
	vd->view= 7;
	vd->dist= 10.0;
	vd->lens= 35.0f;
	vd->near= 0.01f;
	vd->far= 500.0f;
	vd->grid= 1.0f;
	vd->gridlines= 16;
	vd->gridsubdiv = 10;
	
	vd->lay= vd->layact= 1;
	if(G.scene) {
		vd->lay= vd->layact= G.scene->lay;
		vd->camera= G.scene->camera;
	}
	vd->scenelock= 1;
	vd->gridflag |= V3D_SHOW_X;
	vd->gridflag |= V3D_SHOW_Y;
	vd->gridflag |= V3D_SHOW_FLOOR;
	vd->gridflag &= ~V3D_SHOW_Z;
	
	vd->depths= NULL;
	
	return (SpaceLink *)vd;
}

/* not spacelink itself */
static void view3d_free(SpaceLink *sl)
{
	View3D *vd= (View3D *) sl;
	
	if(vd->bgpic) {
		if(vd->bgpic->ima) vd->bgpic->ima->id.us--;
		MEM_freeN(vd->bgpic);
	}
	
	if(vd->localvd) MEM_freeN(vd->localvd);
	if(vd->clipbb) MEM_freeN(vd->clipbb);
	if(vd->depths) {
		if(vd->depths->depths) MEM_freeN(vd->depths->depths);
		MEM_freeN(vd->depths);
		vd->depths= NULL;
	}
	
// XXX	retopo_free_view_data(vd);
	
	if(vd->properties_storage) MEM_freeN(vd->properties_storage);
	if(vd->ri) { 
// XXX		BIF_view3d_previewrender_free(vd);
	}
	
}


/* spacetype; init callback */
static void view3d_init(ScrArea *sa)
{
	ARegion *ar;
	
	/* link area to SpaceXXX struct */
	
	/* define how many regions, the order and types */
	
	/* add types to regions */
	for(ar= sa->regionbase.first; ar; ar= ar->next) {
		static ARegionType art={NULL, NULL, NULL, NULL};
		
		/* for time being; register 1 type */
		ar->type= &art;
		
	}
}

/* spacetype; context changed */
static void view3d_refresh(bContext *C, ScrArea *sa)
{
	
}

static SpaceLink *view3d_duplicate(SpaceLink *sl)
{
	View3D *v3do= (View3D *)sl;
	View3D *v3dn= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	
// XXX	BIF_view3d_previewrender_free(v3do);
	v3do->depths= NULL;
	v3do->retopo_view_data= NULL;
	
	if(v3do->localvd) {
// XXX		restore_localviewdata(v3do);
		v3do->localvd= NULL;
		v3do->properties_storage= NULL;
		v3do->localview= 0;
		v3do->lay &= 0xFFFFFF;
	}
	
	/* copy or clear inside new stuff */

	if(v3dn->bgpic) {
		v3dn->bgpic= MEM_dupallocN(v3dn->bgpic);
		if(v3dn->bgpic->ima) v3dn->bgpic->ima->id.us++;
	}
	v3dn->clipbb= MEM_dupallocN(v3dn->clipbb);
	v3dn->ri= NULL;
	v3dn->properties_storage= NULL;
	
	return (SpaceLink *)v3dn;
}

/* only called once, from screen/spacetypes.c */
void ED_spacetype_view3d(void)
{
	static SpaceType st;
	
	st.spaceid= SPACE_VIEW3D;
	
	st.new= view3d_new;
	st.free= view3d_free;
	st.init= view3d_init;
	st.refresh= view3d_refresh;
	st.duplicate= view3d_duplicate;
	
	BKE_spacetype_register(&st);
	
	
}



