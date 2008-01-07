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

#include "BKE_global.h"
#include "BKE_screen.h"

#include "BIF_gl.h"

#include "WM_api.h"

#include "ED_screen.h"
#include "ED_area.h"

#include "screen_intern.h"	/* own module include */


/* only call once on startup, storage is static data (no malloc!) in kernel listbase */
void ED_spacetypes_init(void)
{
	ED_spacetype_view3d();
//	ED_spacetype_ipo();
//	...
	
	
	ED_operatortypes_screen();
//	ED_operatortypes_view3d();
//	...
	
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
static void xxx_init(ScrArea *sa)
{
	
	/* link area to SpaceXXX struct */
	
	/* define how many regions, the order and types */
	
	/* add types to regions */
}

/* spacetype; external context changed */
static void xxx_refresh(bContext *C, ScrArea *sa)
{
	
}

static SpaceLink *xxx_duplicate(SpaceLink *sl)
{
	
	return NULL;
}

/* only called once, from screen/spacetypes.c */
void ED_spacetype_xxx(void)
{
	static SpaceType st;
	
	st.spaceid= SPACE_VIEW3D;
	
	st.new= xxx_new;
	st.free= xxx_free;
	st.init= xxx_init;
	st.refresh= xxx_refresh;
	st.duplicate= xxx_duplicate;
	
	BKE_spacetype_register(&st);
}

/* ****************************** end template *********************** */




