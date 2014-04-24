/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/KX_ConvertPhysicsObjects.cpp
 *  \ingroup ketsji
 */

#ifdef _MSC_VER
#  pragma warning (disable:4786)
#endif

#include "KX_ConvertPhysicsObject.h"

#include "KX_GameObject.h"
#include "RAS_MeshObject.h"

#ifdef WITH_BULLET

#include "CcdPhysicsEnvironment.h"
#include "CcdPhysicsController.h"

/* Refresh the physics object from either an object or a mesh.
 * gameobj must be valid
 * from_gameobj and from_meshobj can be NULL
 * 
 * when setting the mesh, the following vars get priority
 * 1) from_meshobj - creates the phys mesh from RAS_MeshObject
 * 2) from_gameobj - creates the phys mesh from the DerivedMesh where possible, else the RAS_MeshObject
 * 3) gameobj - update the phys mesh from DerivedMesh or RAS_MeshObject
 * 
 * Most of the logic behind this is in shapeInfo->UpdateMesh(...)
 */
bool KX_ReInstanceBulletShapeFromMesh(KX_GameObject *gameobj, KX_GameObject *from_gameobj, RAS_MeshObject* from_meshobj)
{
	CcdPhysicsController	*spc= static_cast<CcdPhysicsController*>(gameobj->GetPhysicsController());
	CcdShapeConstructionInfo	*shapeInfo;

	/* if this is the child of a compound shape this can happen
	 * don't support compound shapes for now */
	if (spc==NULL)
		return false;
	
	shapeInfo = spc->GetShapeInfo();
	
	if (shapeInfo->m_shapeType != PHY_SHAPE_MESH/* || spc->GetSoftBody()*/)
		return false;
	
	spc->DeleteControllerShape();
	
	if (from_gameobj==NULL && from_meshobj==NULL)
		from_gameobj= gameobj;
	
	/* updates the arrays used for making the new bullet mesh */
	shapeInfo->UpdateMesh(from_gameobj, from_meshobj);

	/* create the new bullet mesh */
	CcdConstructionInfo& cci = spc->GetConstructionInfo();
	btCollisionShape* bm= shapeInfo->CreateBulletShape(cci.m_margin, cci.m_bGimpact, !cci.m_bSoft);

	spc->ReplaceControllerShape(bm);
	return true;
}
#endif // WITH_BULLET
