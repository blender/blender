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
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"


/* ***************** depsgraph calls and anim updates ************* */

/* generic update flush, reads from context Screen (layers) and scene */
/* this is for compliancy, later it can do all windows etc */
void ED_anim_dag_flush_update(bContext *C)
{
	Scene *scene= CTX_data_scene(C);
	bScreen *screen= CTX_wm_screen(C);
	int layer= scene->lay;	/* as minimum this */
	
	if(screen) {
		ScrArea *sa;

		/* get all used view3d layers */
		for(sa= screen->areabase.first; sa; sa= sa->next) {
			if(sa->spacetype==SPACE_VIEW3D)
				layer |= ((View3D *)sa->spacedata.first)->lay;
		}
	}
	
	DAG_scene_flush_update(scene, layer, 0);
}

