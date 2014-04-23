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

/** \file KX_ConvertPhysicsObject.h
 *  \ingroup ketsji
 */

#ifndef __KX_CONVERTPHYSICSOBJECT_H__
#define __KX_CONVERTPHYSICSOBJECT_H__

class RAS_MeshObject;
class KX_Scene;
struct DerivedMesh;

#ifdef WITH_BULLET

void	KX_ConvertBulletObject(class	KX_GameObject* gameobj,
	class	RAS_MeshObject* meshobj,
	struct  DerivedMesh* dm,
	class	KX_Scene* kxscene,
	struct	PHY_ShapeProps* shapeprops,
	struct	PHY_MaterialProps*	smmaterial,
	int activeLayerBitInfo,
	bool isCompoundChild,
	bool hasCompoundChildren);
	
void	KX_ClearBulletSharedShapes();
bool KX_ReInstanceBulletShapeFromMesh(KX_GameObject *gameobj, KX_GameObject *from_gameobj, RAS_MeshObject* from_meshobj);

#endif
#endif  /* __KX_CONVERTPHYSICSOBJECT_H__ */
