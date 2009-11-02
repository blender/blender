/**
 * $Id: spacetypes.c
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
 * The Original Code is Copyright (C) Blender Foundation, 2008
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "BIF_gl.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_gpencil.h"
#include "ED_markers.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_physics.h"
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_space_api.h"
#include "ED_sound.h"
#include "ED_uvedit.h"
#include "ED_mball.h"

/* only call once on startup, storage is global in BKE kernel listbase */
void ED_spacetypes_init(void)
{
	const ListBase *spacetypes;
	SpaceType *type;

	/* create space types */
	ED_spacetype_outliner();
	ED_spacetype_time();
	ED_spacetype_view3d();
	ED_spacetype_ipo();
	ED_spacetype_image();
	ED_spacetype_node();
	ED_spacetype_buttons();
	ED_spacetype_info();
	ED_spacetype_file();
	ED_spacetype_sound();
	ED_spacetype_action();
	ED_spacetype_nla();
	ED_spacetype_script();
	ED_spacetype_text();
	ED_spacetype_sequencer();
	ED_spacetype_logic();
	ED_spacetype_console();
	ED_spacetype_userpref();
//	...
	
	/* register operator types for screen and all spaces */
	ED_operatortypes_screen();
	ED_operatortypes_anim();
	ED_operatortypes_animchannels();
	ED_operatortypes_gpencil();
	ED_operatortypes_object();
	ED_operatortypes_mesh();
	ED_operatortypes_sculpt();
	ED_operatortypes_uvedit();
	ED_operatortypes_paint();
	ED_operatortypes_physics();
	ED_operatortypes_curve();
	ED_operatortypes_armature();
	ED_operatortypes_marker();
	ED_operatortypes_metaball();
	ED_operatortypes_sound();
	ED_operatortypes_render();
	
	ui_view2d_operatortypes();
	
	spacetypes = BKE_spacetypes_list();
	for(type=spacetypes->first; type; type=type->next)
		type->operatortypes();


	/* Macros's must go last since they reference other operators
	 * maybe we'll need to have them go after python operators too? */
	ED_operatormacros_armature();
	ED_operatormacros_mesh();
	ED_operatormacros_object();
}

/* called in wm.c */
/* keymap definitions are registered only once per WM initialize, usually on file read,
   using the keymap the actual areas/regions add the handlers */
void ED_spacetypes_keymap(wmKeyConfig *keyconf)
{
	const ListBase *spacetypes;
	SpaceType *stype;
	ARegionType *atype;

	ED_keymap_screen(keyconf);
	ED_keymap_anim(keyconf);
	ED_keymap_animchannels(keyconf);
	ED_keymap_gpencil(keyconf);
	ED_keymap_object(keyconf); /* defines lattice also */
	ED_keymap_mesh(keyconf);
	ED_keymap_uvedit(keyconf);
	ED_keymap_curve(keyconf);
	ED_keymap_armature(keyconf);
	ED_keymap_physics(keyconf);
	ED_keymap_metaball(keyconf);
	ED_keymap_paint(keyconf);
	ED_marker_keymap(keyconf);

	UI_view2d_keymap(keyconf);

	spacetypes = BKE_spacetypes_list();
	for(stype=spacetypes->first; stype; stype=stype->next) {
		if(stype->keymap)
			stype->keymap(keyconf);
		for(atype=stype->regiontypes.first; atype; atype=atype->next) {
			if(atype->keymap)
				atype->keymap(keyconf);
		}
	}
}

/* ********************** custom drawcall api ***************** */

typedef struct RegionDrawCB {
	struct RegionDrawCB *next, *prev;
	
	void (*draw)(const struct bContext *, struct ARegion *, void *);	
	void *customdata;
	
	int type;
	
} RegionDrawCB;

void *ED_region_draw_cb_activate(ARegionType *art, 
								 void	(*draw)(const struct bContext *, struct ARegion *, void *),
								 void *customdata, int type)
{
	RegionDrawCB *rdc= MEM_callocN(sizeof(RegionDrawCB), "RegionDrawCB");
	
	BLI_addtail(&art->drawcalls, rdc);
	rdc->draw= draw;
	rdc->customdata= customdata;
	rdc->type= type;
	
	return rdc;
}

void ED_region_draw_cb_exit(ARegionType *art, void *handle)
{
	RegionDrawCB *rdc;
	
	for(rdc= art->drawcalls.first; rdc; rdc= rdc->next) {
		if(rdc==(RegionDrawCB *)handle) {
			BLI_remlink(&art->drawcalls, rdc);
			MEM_freeN(rdc);
			return;
		}
	}
}

void ED_region_draw_cb_draw(const bContext *C, ARegion *ar, int type)
{
	RegionDrawCB *rdc;
	
	for(rdc= ar->type->drawcalls.first; rdc; rdc= rdc->next) {
		if(rdc->type==type)
			rdc->draw(C, ar, rdc->customdata);
	}		
}



/* ********************* space template *********************** */

/* allocate and init some vars */
static SpaceLink *xxx_new(const bContext *C)
{
	return NULL;
}

/* not spacelink itself */
static void xxx_free(SpaceLink *sl)
{

}

/* spacetype; init callback for usage, should be redoable */
static void xxx_init(wmWindowManager *wm, ScrArea *sa)
{
	
	/* link area to SpaceXXX struct */
	
	/* define how many regions, the order and types */
	
	/* add types to regions */
}

static SpaceLink *xxx_duplicate(SpaceLink *sl)
{
	
	return NULL;
}

static void xxx_operatortypes(void)
{
	/* register operator types for this space */
}

static void xxx_keymap(wmKeyConfig *keyconf)
{
	/* add default items to keymap */
}

/* only called once, from screen/spacetypes.c */
void ED_spacetype_xxx(void)
{
	static SpaceType st;
	
	st.spaceid= SPACE_VIEW3D;
	
	st.new= xxx_new;
	st.free= xxx_free;
	st.init= xxx_init;
	st.duplicate= xxx_duplicate;
	st.operatortypes= xxx_operatortypes;
	st.keymap= xxx_keymap;
	
	BKE_spacetype_register(&st);
}

/* ****************************** end template *********************** */




