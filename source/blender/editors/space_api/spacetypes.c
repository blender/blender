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

#include "DNA_windowmanager_types.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "BIF_gl.h"

#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_anim_api.h"


ARegionType *ED_regiontype_from_id(SpaceType *st, int regionid)
{
	ARegionType *art;
	
	for(art= st->regiontypes.first; art; art= art->next)
		if(art->regionid==regionid)
			return art;
	
	printf("Error, region type missing in %s\n", st->name);
	return st->regiontypes.first;
}


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
//	...
	
	/* register operator types for screen and all spaces */
	ED_operatortypes_screen();
	ui_view2d_operatortypes();
	ED_operatortypes_anim();
	
	spacetypes = BKE_spacetypes_list();
	for(type=spacetypes->first; type; type=type->next)
		type->operatortypes();
}

/* called in wm.c */
/* keymap definitions are registered only once per WM initialize, usually on file read,
   using the keymap the actual areas/regions add the handlers */
void ED_spacetypes_keymap(wmWindowManager *wm)
{
	const ListBase *spacetypes;
	SpaceType *stype;
	ARegionType *atype;

	ED_keymap_screen(wm);
	UI_view2d_keymap(wm);
	ED_keymap_anim(wm);

	spacetypes = BKE_spacetypes_list();
	for(stype=spacetypes->first; stype; stype=stype->next) {
		if(stype->keymap)
			stype->keymap(wm);
		for(atype=stype->regiontypes.first; atype; atype=atype->next) {
			if(atype->keymap)
				atype->keymap(wm);
		}
	}
}

/* ****************************** space template *********************** */

/* allocate and init some vars */
static SpaceLink *xxx_new(void)
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

static void xxx_keymap(wmWindowManager *wm)
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




