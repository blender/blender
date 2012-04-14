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
#ifndef __KX_NAVMESHOBJECT_H__
#define __KX_NAVMESHOBJECT_H__
#include "DetourStatNavMesh.h"
#include "KX_GameObject.h"
#include "PyObjectPlus.h"
#include <vector>

class RAS_MeshObject;
class MT_Transform;

class KX_NavMeshObject: public KX_GameObject
{
	Py_Header

protected:
	dtStatNavMesh* m_navMesh;
	
	bool BuildVertIndArrays(float *&vertices, int& nverts,
							unsigned short* &polys, int& npolys, unsigned short *&dmeshes, 
							float *&dvertices, int &ndvertsuniq, unsigned short* &dtris, 
							int& ndtris, int &vertsPerPoly);
	
public:
	KX_NavMeshObject(void* sgReplicationInfo, SG_Callbacks callbacks);
	~KX_NavMeshObject();

	virtual	CValue* GetReplica();
	virtual	void ProcessReplica();


	bool BuildNavMesh();
	dtStatNavMesh* GetNavMesh();
	int FindPath(const MT_Point3& from, const MT_Point3& to, float* path, int maxPathLen);
	float Raycast(const MT_Point3& from, const MT_Point3& to);

	enum NavMeshRenderMode {RM_WALLS, RM_POLYS, RM_TRIS, RM_MAX};
	void DrawNavMesh(NavMeshRenderMode mode);
	void DrawPath(const float *path, int pathLen, const MT_Vector3& color);

	MT_Point3 TransformToLocalCoords(const MT_Point3& wpos);
	MT_Point3 TransformToWorldCoords(const MT_Point3& lpos);
#ifdef WITH_PYTHON
	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

	KX_PYMETHOD_DOC(KX_NavMeshObject, findPath);
	KX_PYMETHOD_DOC(KX_NavMeshObject, raycast);
	KX_PYMETHOD_DOC(KX_NavMeshObject, draw);
	KX_PYMETHOD_DOC_NOARGS(KX_NavMeshObject, rebuild);
#endif // WITH_PYTHON
};

#endif //__KX_NAVMESHOBJECT_H__

