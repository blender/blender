/* 
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation 2002-2008
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>
#include <math.h>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "BLI_blenlib.h"

#include "BKE_screen.h"

#include "BPY_extern.h"

/* keep global; this has to be accessible outside of windowmanager */
static ListBase spacetypes= {NULL, NULL};

SpaceType *BKE_spacetype_from_id(int spaceid)
{
	SpaceType *st;
	
	for(st= spacetypes.first; st; st= st->next) {
		if(st->spaceid==spaceid)
			return st;
	}
	return NULL;
}

void BKE_spacetype_register(SpaceType *st)
{
	BLI_addtail(&spacetypes, st);
}

void BKE_spacedata_freelist(ListBase *lb)
{
	SpaceLink *sl;
	
	for (sl= lb->first; sl; sl= sl->next) {
		SpaceType *st= BKE_spacetype_from_id(sl->spacetype);
		
		if(st && st->free) 
			st->free(sl);
	}
	
	BLI_freelistN(lb);
}

/* lb1 should be empty */
void BKE_spacedata_copylist(ListBase *lb1, ListBase *lb2)
{
	SpaceLink *sl;
	
	lb1->first= lb2->last= NULL;	/* to be sure */
	
	for (sl= lb2->first; sl; sl= sl->next) {
		SpaceType *st= BKE_spacetype_from_id(sl->spacetype);
		
		if(st && st->duplicate)
			BLI_addtail(lb1, st->duplicate(sl));
	}
}


/* not area itself */
void BKE_screen_area_free(ScrArea *sa)
{
	
	BKE_spacedata_freelist(&sa->spacedata);
	
	BLI_freelistN(&sa->regionbase);
	BLI_freelistN(&sa->actionzones);
	
	BLI_freelistN(&sa->panels);
	//	uiFreeBlocks(&sa->uiblocks);
	//	uiFreePanels(&sa->panels);
	
	BPY_free_scriptlink(&sa->scriptlink);

}

/* don't free screen itself */
void free_screen(bScreen *sc)
{
	ScrArea *sa;
	
	for(sa= sc->areabase.first; sa; sa= sa->next)
		BKE_screen_area_free(sa);
	
	BLI_freelistN(&sc->vertbase);
	BLI_freelistN(&sc->edgebase);
	BLI_freelistN(&sc->areabase);
}


