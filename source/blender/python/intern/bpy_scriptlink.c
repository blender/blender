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
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_lamp_types.h"
#include "DNA_camera_types.h"
#include "DNA_world_types.h"
#include "DNA_scene_types.h"
#include "DNA_material_types.h"

#include "BLI_blenlib.h"

#include "BKE_blender.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"


/* only copies internal pointers, scriptlink usually is integral part of a struct */
void BPY_copy_scriptlink( struct ScriptLink *scriptlink )
{
	
	if( scriptlink->totscript ) {
		scriptlink->scripts = MEM_dupallocN(scriptlink->scripts);		
		scriptlink->flag = MEM_dupallocN(scriptlink->flag);		
	}
	
	return;
}

/* not free slink itself */
void BPY_free_scriptlink( struct ScriptLink *slink )
{
	if( slink->totscript ) {
		if( slink->flag ) {
			MEM_freeN( slink->flag );
			slink->flag= NULL;
		}
		if( slink->scripts ) {
			MEM_freeN( slink->scripts );
			slink->scripts= NULL;
		}
	}
	
	return;
}

