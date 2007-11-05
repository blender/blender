/*  implicit.c      
* 
*
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
* The Original Code is Copyright (C) Blender Foundation
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): none yet.
*
* ***** END GPL/BL DUAL LICENSE BLOCK *****
*/
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "MEM_guardedalloc.h"
/* types */
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"	
#include "DNA_cloth_types.h"	
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_lattice_types.h"
#include "DNA_scene_types.h"
#include "DNA_modifier_types.h"
#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"
#include "BLI_threads.h"
#include "BKE_collisions.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_object.h"
#include "BKE_cloth.h"
#include "BKE_modifier.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"
#include  "BIF_editdeform.h"

struct Cloth;

int verlet_init ( Object *ob, ClothModifierData *clmd )
{
	return 1;
}

int verlet_free ( ClothModifierData *clmd )
{
	return 1;
}

void integrate ( ClothModifierData *clmd, float dt )
{
	Cloth *cloth = clmd->clothObject;
	unsigned int i = 0;

	// temporary vectors
	float temp[3], velocity[3], force[3];
	
	mul_fvector_S( force, clmd->sim_parms.gravity, dt*dt );
	
	// iterate through all control points
	for(i = 0; i < cloth->numverts; i++)
	{
		if(((clmd->sim_parms.flags & CLOTH_SIMSETTINGS_FLAG_GOAL) && (cloth->verts [i].goal < SOFTGOALSNAP)) || !(clmd->sim_parms.flags & CLOTH_SIMSETTINGS_FLAG_GOAL))
		{
			// save current control point location
			VECCOPY ( temp, cloth->x[i] );

			// update control point by the formula
			//  x += (x - old_x)*dampingFactor + force*timeStep^2
			VECSUB ( velocity, cloth->x[i], cloth->xold[i] );
			VECSUBMUL( force, velocity, -clmd->sim_parms.Cvi * 0.01f* dt * dt);
			VecMulf(velocity, 0.99);
			VECADD ( cloth->x[i], cloth->x[i], velocity );
			VECADD ( cloth->x[i], cloth->x[i], force );

			// store old control point location
			VECCOPY ( cloth->xold[i], temp );
		}
	}
}

void satisfyconstraints(ClothModifierData *clmd)
{
	float delta[3];
	Cloth *cloth = clmd->clothObject;
	unsigned int i = 0;

	for(i = 0; i < 5; i++)
	{
		// calculate spring forces
		LinkNode *search = cloth->springs;
		while(search)
		{
			ClothSpring *spring = search->link;
			float temp = 0;
			float restlen2 = spring->restlen * spring->restlen;
			float len2 = 0, len = 0;
			
			VECSUB(delta, cloth->x[spring->kl], cloth->x[spring->ij]);
			len = sqrt(INPR(delta, delta));
			
			if(spring->type != CLOTH_SPRING_TYPE_BENDING)
			{
				temp = (len - spring->restlen)/len;
				
				mul_fvector_S(delta, delta, temp*0.5);
				
				// check if vertex is pinned
				if(((clmd->sim_parms.flags & CLOTH_SIMSETTINGS_FLAG_GOAL) && (cloth->verts [spring->ij].goal < SOFTGOALSNAP)) || !(clmd->sim_parms.flags & CLOTH_SIMSETTINGS_FLAG_GOAL))
					VECADD(cloth->x[spring->ij], cloth->x[spring->ij], delta);
				
				// check if vertex is pinned
				if(((clmd->sim_parms.flags & CLOTH_SIMSETTINGS_FLAG_GOAL) && (cloth->verts [spring->kl].goal < SOFTGOALSNAP)) || !(clmd->sim_parms.flags & CLOTH_SIMSETTINGS_FLAG_GOAL))
					VECSUB(cloth->x[spring->kl], cloth->x[spring->kl], delta);
			}
			
			search = search->next;
		}
	}
}

int verlet_solver ( Object *ob, float frame, ClothModifierData *clmd, ListBase *effectors )
{
	float dt = 0.01;
	float step = 0;
	
	while(step < 1.0f)
	{
		integrate(clmd, dt);
		satisfyconstraints(clmd);
		
		step+= dt;
	}
	
	return 1;
}






