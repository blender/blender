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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef KX_CONVERTPHYSICSOBJECTS
#define KX_CONVERTPHYSICSOBJECTS

/* These are defined by the build system... */
//but the build system is broken, because it doesn't allow for 2 or more defines at once.
//Please leave Sumo _AND_ Bullet enabled
#define USE_SUMO_SOLID
#define USE_BULLET

//#define USE_ODE

//on visual studio 7/8, always enable BULLET for now 
//you can have multiple physics engines running anyway, and 
//the scons build system doesn't really support this at the moment.
//if you got troubles, just comment out USE_BULLET
#if 1300 <= _MSC_VER
#define USE_BULLET
#endif

class RAS_MeshObject;
class KX_Scene;

typedef enum {
	KX_BOUNDBOX,
	KX_BOUNDSPHERE,
	KX_BOUNDCYLINDER,
	KX_BOUNDCONE,
	KX_BOUNDMESH,
	KX_BOUNDPOLYTOPE,
	KX_BOUND_DYN_MESH
} KX_BoundBoxClass;

struct KX_BoxBounds
{
	float m_center[3];
	float m_extends[3];
};

/* Cone/Cylinder */
struct KX_CBounds
{
	float m_radius;
	float m_height;
};


struct KX_ObjectProperties
{
	bool	m_dyna;
	bool	m_softbody;
	double m_radius;
	bool	m_angular_rigidbody;
	bool	m_in_active_layer;
	bool	m_ghost;
	class KX_GameObject*	m_dynamic_parent;
	bool	m_isactor;
	bool	m_concave;
	bool	m_isdeformable;
	bool	m_disableSleeping;
	bool	m_hasCompoundChildren;
	bool	m_isCompoundChild;

	float	m_linearStiffness;
	float m_angularStiffness;
	float	m_volumePreservation;
	int		m_gamesoftFlag;
	
	double  m_margin;
	KX_BoundBoxClass	m_boundclass;
	union {
		KX_BoxBounds	box;
		KX_CBounds	c;
	} m_boundobject;
};

#ifdef USE_ODE


void	KX_ConvertODEEngineObject(KX_GameObject* gameobj,
	RAS_MeshObject* meshobj,
	KX_Scene* kxscene,
	struct	PHY_ShapeProps* shapeprops,
	struct	PHY_MaterialProps*	smmaterial,
	struct	KX_ObjectProperties*	objprop);


#endif //USE_ODE


void	KX_ConvertDynamoObject(KX_GameObject* gameobj,
	RAS_MeshObject* meshobj,
	KX_Scene* kxscene,
	struct	PHY_ShapeProps* shapeprops,
	struct	PHY_MaterialProps*	smmaterial,
	struct	KX_ObjectProperties*	objprop);

#ifdef USE_SUMO_SOLID

void	KX_ConvertSumoObject(	class	KX_GameObject* gameobj,
	class	RAS_MeshObject* meshobj,
	class	KX_Scene* kxscene,
	struct	PHY_ShapeProps* shapeprops,
	struct	PHY_MaterialProps*	smmaterial,
	struct	KX_ObjectProperties*	objprop);
	
void	KX_ClearSumoSharedShapes();
bool KX_ReInstanceShapeFromMesh(RAS_MeshObject* meshobj);

#endif

#ifdef USE_BULLET

void	KX_ConvertBulletObject(	class	KX_GameObject* gameobj,
	class	RAS_MeshObject* meshobj,
	class	KX_Scene* kxscene,
	struct	PHY_ShapeProps* shapeprops,
	struct	PHY_MaterialProps*	smmaterial,
	struct	KX_ObjectProperties*	objprop);
	
void	KX_ClearBulletSharedShapes();
//bool KX_ReInstanceShapeFromMesh(RAS_MeshObject* meshobj);

#endif
#endif //KX_CONVERTPHYSICSOBJECTS

