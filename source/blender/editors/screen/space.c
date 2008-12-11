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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_global.h"
#include "BKE_screen.h"

#include "BLO_readfile.h"

#include "WM_api.h"

#include "ED_area.h"
#include "ED_screen.h"

/* */

void ED_newspace(ScrArea *sa, int type)
{
	if(sa->spacetype != type) {
		SpaceType *st= BKE_spacetype_from_id(type);
		SpaceLink *slold= sa->spacedata.first;
		SpaceLink *sl;
		
		sa->spacetype= type;
		sa->butspacetype= type;
		
		/* check previously stored space */
		for (sl= sa->spacedata.first; sl; sl= sl->next)
			if(sl->spacetype==type)
				break;
		
		/* old spacedata... happened during work on 2.50, remove */
		if(sl && sl->regionbase.first==NULL) {
			st->free(sl);
			MEM_freeN(sl);
			sl= NULL;
		}
		
		if (sl) {
			
			/* swap regions */
			slold->regionbase= sa->regionbase;
			sa->regionbase= sl->regionbase;
			sl->regionbase.first= sl->regionbase.last= NULL;
			
			/* put in front of list */
			BLI_remlink(&sa->spacedata, sl);
			BLI_addhead(&sa->spacedata, sl);
		} 
		else {
			/* new space */
			if(st) {
				sl= st->new();
				BLI_addhead(&sa->spacedata, sl);
				
				/* swap regions */
				slold->regionbase= sa->regionbase;
				sa->regionbase= sl->regionbase;
				sl->regionbase.first= sl->regionbase.last= NULL;
			}
		}
	}
	
}



