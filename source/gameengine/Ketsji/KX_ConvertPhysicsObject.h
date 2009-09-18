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
#define USE_BULLET

//on visual studio 7/8, always enable BULLET for now 
//you can have multiple physics engines running anyway, and 
//the scons build system doesn't really support this at the moment.
//if you got troubles, just comment out USE_BULLET
#if 1300 <= _MSC_VER
#define USE_BULLET
#endif

class RAS_MeshObject;
class KX_Scene;
struct DerivedMesh;

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
	bool	m_sensor;
	bool	m_concave;
	bool	m_isdeformable;
	bool	m_disableSleeping;
	bool	m_hasCompoundChildren;
	bool	m_isCompoundChild;

	/////////////////////////

	int		m_gamesoftFlag;
	float	m_soft_linStiff;			/* linear stiffness 0..1 */
	float	m_soft_angStiff;		/* angular stiffness 0..1 */
	float	m_soft_volume;			/* volume preservation 0..1 */

	int		m_soft_viterations;		/* Velocities solver iterations */
	int		m_soft_piterations;		/* Positions solver iterations */
	int		m_soft_diterations;		/* Drift solver iterations */
	int		m_soft_citerations;		/* Cluster solver iterations */

	float	m_soft_kSRHR_CL;		/* Soft vs rigid hardness [0,1] (cluster only) */
	float	m_soft_kSKHR_CL;		/* Soft vs kinetic hardness [0,1] (cluster only) */
	float	m_soft_kSSHR_CL;		/* Soft vs soft hardness [0,1] (cluster only) */
	float	m_soft_kSR_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */

	float	m_soft_kSK_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */
	float	m_soft_kSS_SPLT_CL;	/* Soft vs rigid impulse split [0,1] (cluster only) */
	float	m_soft_kVCF;			/* Velocities correction factor (Baumgarte) */
	float	m_soft_kDP;			/* Damping coefficient [0,1] */

	float	m_soft_kDG;			/* Drag coefficient [0,+inf] */
	float	m_soft_kLF;			/* Lift coefficient [0,+inf] */
	float	m_soft_kPR;			/* Pressure coefficient [-inf,+inf] */
	float	m_soft_kVC;			/* Volume conversation coefficient [0,+inf] */

	float	m_soft_kDF;			/* Dynamic friction coefficient [0,1] */
	float	m_soft_kMT;			/* Pose matching coefficient [0,1] */
	float	m_soft_kCHR;			/* Rigid contacts hardness [0,1] */
	float	m_soft_kKHR;			/* Kinetic contacts hardness [0,1] */

	float	m_soft_kSHR;			/* Soft contacts hardness [0,1] */
	float	m_soft_kAHR;			/* Anchors hardness [0,1] */
	int		m_soft_collisionflags;	/* Vertex/Face or Signed Distance Field(SDF) or Clusters, Soft versus Soft or Rigid */
	int		m_soft_numclusteriterations;	/* number of iterations to refine collision clusters*/
	float   m_soft_welding;			/*   threshold to remove duplicate/nearby vertices */

	/////////////////////////
	
	bool	m_lockXaxis;
	bool	m_lockYaxis;
	bool	m_lockZaxis;
	bool	m_lockXRotaxis;
	bool	m_lockYRotaxis;
	bool	m_lockZRotaxis;

	/////////////////////////
	double  m_margin;
	float	m_contactProcessingThreshold;

	KX_BoundBoxClass	m_boundclass;
	union {
		KX_BoxBounds	box;
		KX_CBounds	c;
	} m_boundobject;
};

void	KX_ConvertDynamoObject(KX_GameObject* gameobj,
	RAS_MeshObject* meshobj,
	KX_Scene* kxscene,
	struct	PHY_ShapeProps* shapeprops,
	struct	PHY_MaterialProps*	smmaterial,
	struct	KX_ObjectProperties*	objprop);


#ifdef USE_BULLET

void	KX_ConvertBulletObject(	class	KX_GameObject* gameobj,
	class	RAS_MeshObject* meshobj,
	struct  DerivedMesh* dm,
	class	KX_Scene* kxscene,
	struct	PHY_ShapeProps* shapeprops,
	struct	PHY_MaterialProps*	smmaterial,
	struct	KX_ObjectProperties*	objprop);
	
void	KX_ClearBulletSharedShapes();
bool KX_ReInstanceBulletShapeFromMesh(KX_GameObject *gameobj, KX_GameObject *from_gameobj, RAS_MeshObject* from_meshobj);

#endif
#endif //KX_CONVERTPHYSICSOBJECTS

