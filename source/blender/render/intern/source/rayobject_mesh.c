/**
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): André Pinto.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <assert.h>
 
#include "rayobject.h"

#include "MEM_guardedalloc.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "BKE_utildefines.h"

typedef struct RayMesh
{
	RayObject rayobj;
	
	Mesh *mesh;
	void *ob;
	
	RayFace *faces;
	int num_faces;
	
} RayMesh;

static int  RayObject_mesh_intersect(RayObject *o, Isect *isec);
static void RayObject_mesh_add(RayObject *o, RayObject *ob);
static void RayObject_mesh_done(RayObject *o);
static void RayObject_mesh_free(RayObject *o);
static void RayObject_mesh_bb(RayObject *o, float *min, float *max);

static RayObjectAPI mesh_api =
{
	RayObject_mesh_intersect,
	RayObject_mesh_add,
	RayObject_mesh_done,
	RayObject_mesh_free,
	RayObject_mesh_bb
};


static int  RayObject_mesh_intersect(RayObject *o, Isect *isec)
{
	RayMesh *rm= (RayMesh*)o;
	int i, hit = 0;
	for(i = 0; i<rm->num_faces; i++)
		if(RE_rayobject_raycast( (RayObject*)rm->faces+i, isec ))
		{
			hit = 1;
			if(isec->mode == RE_RAY_SHADOW)
				break;
		}

	return hit;
}

static void RayObject_mesh_add(RayObject *o, RayObject *ob)
{
}

static void RayObject_mesh_done(RayObject *o)
{
}

static void RayObject_mesh_free(RayObject *o)
{
	RayMesh *rm= (RayMesh*)o;
	MEM_freeN( rm->faces );
	MEM_freeN( rm );
}

static void RayObject_mesh_bb(RayObject *o, float *min, float *max)
{
	RayMesh *rm= (RayMesh*)o;
	int i;
	for(i = 0; i<rm->mesh->totvert; i++)
		DO_MINMAX( rm->mesh->mvert[i].co, min, max);
}

RayObject* RE_rayobject_mesh_create(Mesh *mesh, void *ob)
{
	RayMesh *rm= MEM_callocN(sizeof(RayMesh), "Octree");
	int i;
	RayFace *face;
	MFace *mface;
	
	assert( RayObject_isAligned(rm) ); /* RayObject API assumes real data to be 4-byte aligned */	
	
	rm->rayobj.api = &mesh_api;
	rm->mesh = mesh;
	rm->faces = MEM_callocN(sizeof(RayFace)*mesh->totface, "octree rayobject nodes");
	rm->num_faces = mesh->totface;
	
	face = rm->faces;
	mface = mesh->mface;
	for(i=0; i<mesh->totface; i++, face++, mface++)
	{
		face->v1 = mesh->mvert[mface->v1].co;
		face->v2 = mesh->mvert[mface->v2].co;
		face->v3 = mesh->mvert[mface->v3].co;
		face->v4 = mface->v4 ? mesh->mvert[mface->v4].co : NULL;
		
		face->ob = ob;
		face->face = (void*)i;
	}
	
	return RayObject_unalignRayAPI((RayObject*) rm);
}
