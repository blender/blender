/**
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel Genrich.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef BKE_SPH_H
#define BKE_SPH_H


#include "BKE_DerivedMesh.h"
#include "BKE_utildefines.h"

#include "BLI_linklist.h"

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

void sph_init(SphModifierData *sphmd);
void sph_free_modifier (SphModifierData *sphmd);
DerivedMesh *sphModifier_do(SphModifierData *sphmd,Object *ob, DerivedMesh *dm, int useRenderParams, int isFinalCalc);
int sph_init_all (SphModifierData *sphmd, DerivedMesh *dm, Object *ob);


/* SIMULATION FLAGS: goal flags,.. */
/* These are the bits used in SimSettings.flags. */
// first 16 (short) flags are used for fluid type identification
typedef enum
{
	SPH_SIMSETTINGS_FLAG_FLUID = ( 1 << 0 ), // Fluid object?
	SPH_SIMSETTINGS_FLAG_OBSTACLE = ( 1 << 1 ), // Obstacle?
	SPH_SIMSETTINGS_FLAG_DOMAIN = ( 1 << 2 ), // Fluid domain
	
	SPH_SIMSETTINGS_FLAG_GHOSTS = ( 1 << 16 ), // use ghost particles?
	SPH_SIMSETTINGS_FLAG_OFFLINE = ( 1 << 17 ), // do offline simulation?
	SPH_SIMSETTINGS_FLAG_MULTIRES = ( 1 << 18 ), // use multires?
	SPH_SIMSETTINGS_FLAG_VORTICITY = ( 1 << 19 ), // use vorticity enhancement?
	SPH_SIMSETTINGS_FLAG_BAKING = ( 1 << 20 ), // is domain baking?
	SPH_SIMSETTINGS_FLAG_INIT = ( 1 << 21 ), // inited?
} SPH_SIMSETTINGS_FLAGS;


#endif //BKE_SPH_H



