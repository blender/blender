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

#include "MT_assert.h"

#include "KX_SoftBodyDeformer.h"
#include "KX_ConvertPhysicsObject.h"
#include "BL_DeformableGameObject.h"
#include "RAS_MeshObject.h"
#include "KX_Scene.h"
#include "BL_System.h"

#include "PHY_Pro.h" //todo cleanup
#include "KX_ClientObjectInfo.h"

#include "CTR_Map.h"
#include "CTR_HashedPtr.h"

#include "MT_MinMax.h"

#include "KX_PhysicsEngineEnums.h"

#include "KX_MotionState.h" // bridge between motionstate and scenegraph node

extern "C"{
	#include "BLI_utildefines.h"
	#include "BLI_math.h"
	#include "BKE_DerivedMesh.h"
	#include "BKE_object.h"
}

#include "DNA_object_force.h"

#ifdef WITH_BULLET
#include "BulletSoftBody/btSoftBody.h"

#include "CcdPhysicsEnvironment.h"
#include "CcdPhysicsController.h"
#include "BulletCollision/BroadphaseCollision/btBroadphaseInterface.h"

#include "btBulletDynamicsCommon.h"

#ifdef WIN32
#if defined(_MSC_VER) && (_MSC_VER >= 1310)
//only use SIMD Hull code under Win32
//#define TEST_HULL 1
#ifdef TEST_HULL
#define USE_HULL 1
//#define TEST_SIMD_HULL 1

#include "NarrowPhaseCollision/Hull.h"
#endif //#ifdef TEST_HULL

#endif //_MSC_VER 
#endif //WIN32

// my_tex_space_mesh and my_get_local_bounds were moved from BL_BlenderDataConversion.cpp (my_boundbox_mesh is just copied)
// there has to be a better way to do this...
static float my_boundbox_mesh(Mesh *me, float *loc, float *size)
{
	MVert *mvert;
	BoundBox *bb;
	float min[3], max[3];
	float mloc[3], msize[3];
	float radius_sq=0.0f, vert_radius_sq, *co;
	int a;

	if (me->bb==0) {
		me->bb = BKE_boundbox_alloc_unit();
	}
	bb= me->bb;

	INIT_MINMAX(min, max);

	if (!loc) loc= mloc;
	if (!size) size= msize;

	mvert= me->mvert;
	for (a = 0; a<me->totvert; a++, mvert++) {
		co = mvert->co;

		/* bounds */
		minmax_v3v3_v3(min, max, co);

		/* radius */

		vert_radius_sq = len_squared_v3(co);
		if (vert_radius_sq > radius_sq)
			radius_sq = vert_radius_sq;
	}

	if (me->totvert) {
		loc[0] = (min[0] + max[0]) / 2.0f;
		loc[1] = (min[1] + max[1]) / 2.0f;
		loc[2] = (min[2] + max[2]) / 2.0f;

		size[0] = (max[0] - min[0]) / 2.0f;
		size[1] = (max[1] - min[1]) / 2.0f;
		size[2] = (max[2] - min[2]) / 2.0f;
	}
	else {
		loc[0] = loc[1] = loc[2] = 0.0f;
		size[0] = size[1] = size[2] = 0.0f;
	}

	bb->vec[0][0] = bb->vec[1][0] = bb->vec[2][0] = bb->vec[3][0] = loc[0]-size[0];
	bb->vec[4][0] = bb->vec[5][0] = bb->vec[6][0] = bb->vec[7][0] = loc[0]+size[0];

	bb->vec[0][1] = bb->vec[1][1] = bb->vec[4][1] = bb->vec[5][1] = loc[1]-size[1];
	bb->vec[2][1] = bb->vec[3][1] = bb->vec[6][1] = bb->vec[7][1] = loc[1]+size[1];

	bb->vec[0][2] = bb->vec[3][2] = bb->vec[4][2] = bb->vec[7][2] = loc[2]-size[2];
	bb->vec[1][2] = bb->vec[2][2] = bb->vec[5][2] = bb->vec[6][2] = loc[2]+size[2];

	return sqrtf_signed(radius_sq);
}

static void my_tex_space_mesh(Mesh *me)
{
	KeyBlock *kb;
	float *fp, loc[3], size[3], min[3], max[3];
	int a;

	my_boundbox_mesh(me, loc, size);

	if (me->texflag & ME_AUTOSPACE) {
		if (me->key) {
			kb= me->key->refkey;
			if (kb) {

				INIT_MINMAX(min, max);

				fp= (float *)kb->data;
				for (a=0; a<kb->totelem; a++, fp += 3) {
					minmax_v3v3_v3(min, max, fp);
				}
				if (kb->totelem) {
					loc[0] = (min[0]+max[0])/2.0f; loc[1] = (min[1]+max[1])/2.0f; loc[2] = (min[2]+max[2])/2.0f;
					size[0] = (max[0]-min[0])/2.0f; size[1] = (max[1]-min[1])/2.0f; size[2] = (max[2]-min[2])/2.0f;
				}
				else {
					loc[0] = loc[1] = loc[2] = 0.0;
					size[0] = size[1] = size[2] = 0.0;
				}

			}
		}

		copy_v3_v3(me->loc, loc);
		copy_v3_v3(me->size, size);
		me->rot[0] = me->rot[1] = me->rot[2] = 0.0f;

		if (me->size[0] == 0.0f) me->size[0] = 1.0f;
		else if (me->size[0] > 0.0f && me->size[0]< 0.00001f) me->size[0] = 0.00001f;
		else if (me->size[0] < 0.0f && me->size[0]> -0.00001f) me->size[0] = -0.00001f;

		if (me->size[1] == 0.0f) me->size[1] = 1.0f;
		else if (me->size[1] > 0.0f && me->size[1]< 0.00001f) me->size[1] = 0.00001f;
		else if (me->size[1] < 0.0f && me->size[1]> -0.00001f) me->size[1] = -0.00001f;

		if (me->size[2] == 0.0f) me->size[2] = 1.0f;
		else if (me->size[2] > 0.0f && me->size[2]< 0.00001f) me->size[2] = 0.00001f;
		else if (me->size[2] < 0.0f && me->size[2]> -0.00001f) me->size[2] = -0.00001f;
	}

}

static void my_get_local_bounds(Object *ob, DerivedMesh *dm, float *center, float *size)
{
	BoundBox *bb= NULL;
	/* uses boundbox, function used by Ketsji */
	switch (ob->type)
	{
		case OB_MESH:
			if (dm)
			{
				float min_r[3], max_r[3];
				INIT_MINMAX(min_r, max_r);
				dm->getMinMax(dm, min_r, max_r);
				size[0] = 0.5f * fabsf(max_r[0] - min_r[0]);
				size[1] = 0.5f * fabsf(max_r[1] - min_r[1]);
				size[2] = 0.5f * fabsf(max_r[2] - min_r[2]);

				center[0] = 0.5f * (max_r[0] + min_r[0]);
				center[1] = 0.5f * (max_r[1] + min_r[1]);
				center[2] = 0.5f * (max_r[2] + min_r[2]);
				return;
			} else
			{
				bb= ( (Mesh *)ob->data )->bb;
				if (bb==0)
				{
					my_tex_space_mesh((struct Mesh *)ob->data);
					bb= ( (Mesh *)ob->data )->bb;
				}
			}
			break;
		case OB_CURVE:
		case OB_SURF:
			center[0] = center[1] = center[2] = 0.0;
			size[0]  = size[1]=size[2]=0.0;
			break;
		case OB_FONT:
			center[0] = center[1] = center[2] = 0.0;
			size[0]  = size[1]=size[2]=1.0;
			break;
		case OB_MBALL:
			bb= ob->bb;
			break;
	}

	if (bb==NULL)
	{
		center[0] = center[1] = center[2] = 0.0;
		size[0] = size[1] = size[2] = 1.0;
	}
	else
	{
		size[0] = 0.5f * fabsf(bb->vec[0][0] - bb->vec[4][0]);
		size[1] = 0.5f * fabsf(bb->vec[0][1] - bb->vec[2][1]);
		size[2] = 0.5f * fabsf(bb->vec[0][2] - bb->vec[1][2]);

		center[0] = 0.5f * (bb->vec[0][0] + bb->vec[4][0]);
		center[1] = 0.5f * (bb->vec[0][1] + bb->vec[2][1]);
		center[2] = 0.5f * (bb->vec[0][2] + bb->vec[1][2]);
	}
}

// forward declarations

void	KX_ConvertBulletObject(	class	KX_GameObject* gameobj,
	class	RAS_MeshObject* meshobj,
	struct  DerivedMesh* dm,
	class	KX_Scene* kxscene,
	struct	PHY_ShapeProps* shapeprops,
	struct	PHY_MaterialProps*	smmaterial,
	int activeLayerBitInfo,
	bool isCompoundChild,
	bool hasCompoundChildren)
{
	Object* blenderobject = gameobj->GetBlenderObject();
	CcdPhysicsEnvironment* env = (CcdPhysicsEnvironment*)kxscene->GetPhysicsEnvironment();
	assert(env);
	

	bool isbulletdyna = (blenderobject->gameflag & OB_DYNAMIC) != 0;;
	bool isbulletsensor = (blenderobject->gameflag & OB_SENSOR) != 0;
	bool isbulletchar = (blenderobject->gameflag & OB_CHARACTER) != 0;
	bool isbulletsoftbody = (blenderobject->gameflag & OB_SOFT_BODY) != 0;
	bool isbulletrigidbody = (blenderobject->gameflag & OB_RIGID_BODY) != 0;
	bool useGimpact = false;
	CcdConstructionInfo ci;
	class PHY_IMotionState* motionstate = new KX_MotionState(gameobj->GetSGNode());
	class CcdShapeConstructionInfo *shapeInfo = new CcdShapeConstructionInfo();

	KX_GameObject *parent = gameobj->GetParent();
	if (parent)
	{
		isbulletdyna = false;
		isbulletsoftbody = false;
		shapeprops->m_mass = 0.f;
	}

	if (!isbulletdyna)
	{
		ci.m_collisionFlags |= btCollisionObject::CF_STATIC_OBJECT;
	}
	if ((blenderobject->gameflag & (OB_GHOST | OB_SENSOR | OB_CHARACTER)) != 0)
	{
		ci.m_collisionFlags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
	}

	ci.m_MotionState = motionstate;
	ci.m_gravity = btVector3(0,0,0);
	ci.m_linearFactor = btVector3(((blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_X_AXIS) !=0)? 0 : 1,
									((blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_Y_AXIS) !=0)? 0 : 1,
									((blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_Z_AXIS) !=0)? 0 : 1);
	ci.m_angularFactor = btVector3(((blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_X_ROT_AXIS) !=0)? 0 : 1,
									((blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_Y_ROT_AXIS) !=0)? 0 : 1,
									((blenderobject->gameflag2 & OB_LOCK_RIGID_BODY_Z_ROT_AXIS) !=0)? 0 : 1);
	ci.m_localInertiaTensor =btVector3(0,0,0);
	ci.m_mass = isbulletdyna ? shapeprops->m_mass : 0.f;
	ci.m_clamp_vel_min = shapeprops->m_clamp_vel_min;
	ci.m_clamp_vel_max = shapeprops->m_clamp_vel_max;
	ci.m_stepHeight = isbulletchar ? shapeprops->m_step_height : 0.f;
	ci.m_jumpSpeed = isbulletchar ? shapeprops->m_jump_speed : 0.f;
	ci.m_fallSpeed = isbulletchar ? shapeprops->m_fall_speed : 0.f;

	//mmm, for now, take this for the size of the dynamicobject
	// Blender uses inertia for radius of dynamic object
	shapeInfo->m_radius = ci.m_radius = blenderobject->inertia;
	useGimpact = ((isbulletdyna || isbulletsensor) && !isbulletsoftbody);

	if (isbulletsoftbody)
	{
		if (blenderobject->bsoft)
		{
			ci.m_margin = blenderobject->bsoft->margin;
		}
		else
		{
			ci.m_margin = 0.f;
		}
	}
	else
	{
		ci.m_margin = blenderobject->margin;
	}

	ci.m_localInertiaTensor = btVector3(ci.m_mass/3.f,ci.m_mass/3.f,ci.m_mass/3.f);
	
	btCollisionShape* bm = 0;

	char bounds;
	if (blenderobject->gameflag & OB_BOUNDS)
	{
		bounds = blenderobject->collision_boundtype;
	}
	else
	{
		if (blenderobject->gameflag & OB_SOFT_BODY)
			bounds = OB_BOUND_TRIANGLE_MESH;
		else if (blenderobject->gameflag & OB_CHARACTER)
			bounds = OB_BOUND_SPHERE;
		else if (isbulletdyna)
			bounds = OB_BOUND_SPHERE;
		else
			bounds = OB_BOUND_TRIANGLE_MESH;
	}

	// Can't use triangle mesh or convex hull on a non-mesh object, fall-back to sphere
	if (ELEM(bounds, OB_BOUND_TRIANGLE_MESH, OB_BOUND_CONVEX_HULL) && blenderobject->type != OB_MESH)
		bounds = OB_BOUND_SPHERE;

	float bounds_center[3], bounds_extends[3];
	my_get_local_bounds(blenderobject, dm, bounds_center, bounds_extends);

	switch (bounds)
	{
	case OB_BOUND_SPHERE:
		{
			//float radius = objprop->m_radius;
			//btVector3 inertiaHalfExtents (
			//	radius,
			//	radius,
			//	radius);
			
			//blender doesn't support multisphere, but for testing:

			//bm = new MultiSphereShape(inertiaHalfExtents,,&trans.getOrigin(),&radius,1);
			shapeInfo->m_shapeType = PHY_SHAPE_SPHERE;
			// XXX We calculated the radius but didn't use it?
			// objprop.m_boundobject.c.m_radius = MT_max(bb.m_extends[0], MT_max(bb.m_extends[1], bb.m_extends[2]));
			bm = shapeInfo->CreateBulletShape(ci.m_margin);
			break;
		};
	case OB_BOUND_BOX:
		{
			shapeInfo->m_halfExtend.setValue(
					2.f * bounds_extends[0],
			        2.f * bounds_extends[1],
			        2.f * bounds_extends[2]);

			shapeInfo->m_halfExtend /= 2.0;
			shapeInfo->m_halfExtend = shapeInfo->m_halfExtend.absolute();
			shapeInfo->m_shapeType = PHY_SHAPE_BOX;
			bm = shapeInfo->CreateBulletShape(ci.m_margin);
			break;
		};
	case OB_BOUND_CYLINDER:
		{
			float radius = MT_max(bounds_extends[0], bounds_extends[1]);
			shapeInfo->m_halfExtend.setValue(
				radius,
				radius,
				bounds_extends[2]
			);
			shapeInfo->m_shapeType = PHY_SHAPE_CYLINDER;
			bm = shapeInfo->CreateBulletShape(ci.m_margin);
			break;
		}

	case OB_BOUND_CONE:
		{
			shapeInfo->m_radius = MT_max(bounds_extends[0], bounds_extends[1]);
			shapeInfo->m_height = 2.f * bounds_extends[2];
			shapeInfo->m_shapeType = PHY_SHAPE_CONE;
			bm = shapeInfo->CreateBulletShape(ci.m_margin);
			break;
		}
	case OB_BOUND_CONVEX_HULL:
		{
			shapeInfo->SetMesh(meshobj, dm,true);
			bm = shapeInfo->CreateBulletShape(ci.m_margin);
			break;
		}
	case OB_BOUND_CAPSULE:
		{
			shapeInfo->m_radius = MT_max(bounds_extends[0], bounds_extends[1]);
			shapeInfo->m_height = 2.f * (bounds_extends[2] - shapeInfo->m_radius);
			if (shapeInfo->m_height < 0.f)
				shapeInfo->m_height = 0.f;
			shapeInfo->m_shapeType = PHY_SHAPE_CAPSULE;
			bm = shapeInfo->CreateBulletShape(ci.m_margin);
			break;
		}
	case OB_BOUND_TRIANGLE_MESH:
		{
			// mesh shapes can be shared, check first if we already have a shape on that mesh
			class CcdShapeConstructionInfo *sharedShapeInfo = CcdShapeConstructionInfo::FindMesh(meshobj, dm, false);
			if (sharedShapeInfo != NULL) 
			{
				shapeInfo->Release();
				shapeInfo = sharedShapeInfo;
				shapeInfo->AddRef();
			} else
			{
				shapeInfo->SetMesh(meshobj, dm, false);
			}

			// Soft bodies can benefit from welding, don't do it on non-soft bodies
			if (isbulletsoftbody)
			{
				// disable welding: it doesn't bring any additional stability and it breaks the relation between soft body collision shape and graphic mesh
				// shapeInfo->setVertexWeldingThreshold1((blenderobject->bsoft) ? blenderobject->bsoft->welding ? 0.f);
				shapeInfo->setVertexWeldingThreshold1(0.f); //todo: expose this to the UI
			}

			bm = shapeInfo->CreateBulletShape(ci.m_margin, useGimpact, !isbulletsoftbody);
			//should we compute inertia for dynamic shape?
			//bm->calculateLocalInertia(ci.m_mass,ci.m_localInertiaTensor);

			break;
		}
	}


//	ci.m_localInertiaTensor.setValue(0.1f,0.1f,0.1f);

	if (!bm)
	{
		delete motionstate;
		shapeInfo->Release();
		return;
	}

	//bm->setMargin(ci.m_margin);


		if (isCompoundChild)
		{
			//find parent, compound shape and add to it
			//take relative transform into account!
			CcdPhysicsController* parentCtrl = (CcdPhysicsController*)parent->GetPhysicsController();
			assert(parentCtrl);
			CcdShapeConstructionInfo* parentShapeInfo = parentCtrl->GetShapeInfo();
			btRigidBody* rigidbody = parentCtrl->GetRigidBody();
			btCollisionShape* colShape = rigidbody->getCollisionShape();
			assert(colShape->isCompound());
			btCompoundShape* compoundShape = (btCompoundShape*)colShape;

			// compute the local transform from parent, this may include several node in the chain
			SG_Node* gameNode = gameobj->GetSGNode();
			SG_Node* parentNode = parent->GetSGNode();
			// relative transform
			MT_Vector3 parentScale = parentNode->GetWorldScaling();
			parentScale[0] = MT_Scalar(1.0)/parentScale[0];
			parentScale[1] = MT_Scalar(1.0)/parentScale[1];
			parentScale[2] = MT_Scalar(1.0)/parentScale[2];
			MT_Vector3 relativeScale = gameNode->GetWorldScaling() * parentScale;
			MT_Matrix3x3 parentInvRot = parentNode->GetWorldOrientation().transposed();
			MT_Vector3 relativePos = parentInvRot*((gameNode->GetWorldPosition()-parentNode->GetWorldPosition())*parentScale);
			MT_Matrix3x3 relativeRot = parentInvRot*gameNode->GetWorldOrientation();

			shapeInfo->m_childScale.setValue(relativeScale[0],relativeScale[1],relativeScale[2]);
			bm->setLocalScaling(shapeInfo->m_childScale);
			shapeInfo->m_childTrans.getOrigin().setValue(relativePos[0],relativePos[1],relativePos[2]);
			float rot[12];
			relativeRot.getValue(rot);
			shapeInfo->m_childTrans.getBasis().setFromOpenGLSubMatrix(rot);

			parentShapeInfo->AddShape(shapeInfo);
			compoundShape->addChildShape(shapeInfo->m_childTrans,bm);
			//do some recalc?
			//recalc inertia for rigidbody
			if (!rigidbody->isStaticOrKinematicObject())
			{
				btVector3 localInertia;
				float mass = 1.f/rigidbody->getInvMass();
				compoundShape->calculateLocalInertia(mass,localInertia);
				rigidbody->setMassProps(mass,localInertia);
			}
			shapeInfo->Release();
			// delete motionstate as it's not used
			delete motionstate;
			return;
		}

		if (hasCompoundChildren)
		{
			// create a compound shape info
			CcdShapeConstructionInfo *compoundShapeInfo = new CcdShapeConstructionInfo();
			compoundShapeInfo->m_shapeType = PHY_SHAPE_COMPOUND;
			compoundShapeInfo->AddShape(shapeInfo);
			// create the compound shape manually as we already have the child shape
			btCompoundShape* compoundShape = new btCompoundShape();
			compoundShape->addChildShape(shapeInfo->m_childTrans,bm);
			// now replace the shape
			bm = compoundShape;
			shapeInfo->Release();
			shapeInfo = compoundShapeInfo;
		}






#ifdef TEST_SIMD_HULL
	if (bm->IsPolyhedral())
	{
		PolyhedralConvexShape* polyhedron = static_cast<PolyhedralConvexShape*>(bm);
		if (!polyhedron->m_optionalHull)
		{
			//first convert vertices in 'Point3' format
			int numPoints = polyhedron->GetNumVertices();
			Point3* points = new Point3[numPoints+1];
			//first 4 points should not be co-planar, so add central point to satisfy MakeHull
			points[0] = Point3(0.f,0.f,0.f);
			
			btVector3 vertex;
			for (int p=0;p<numPoints;p++)
			{
				polyhedron->GetVertex(p,vertex);
				points[p+1] = Point3(vertex.getX(),vertex.getY(),vertex.getZ());
			}

			Hull* hull = Hull::MakeHull(numPoints+1,points);
			polyhedron->m_optionalHull = hull;
		}

	}
#endif //TEST_SIMD_HULL


	ci.m_collisionShape = bm;
	ci.m_shapeInfo = shapeInfo;
	ci.m_friction = smmaterial->m_friction;//tweak the friction a bit, so the default 0.5 works nice
	ci.m_restitution = smmaterial->m_restitution;
	ci.m_physicsEnv = env;
	// drag / damping is inverted
	ci.m_linearDamping = 1.f - shapeprops->m_lin_drag;
	ci.m_angularDamping = 1.f - shapeprops->m_ang_drag;
	//need a bit of damping, else system doesn't behave well
	ci.m_inertiaFactor = shapeprops->m_inertia/0.4f;//defaults to 0.4, don't want to change behavior
	
	ci.m_do_anisotropic = shapeprops->m_do_anisotropic;
	ci.m_anisotropicFriction.setValue(shapeprops->m_friction_scaling[0],shapeprops->m_friction_scaling[1],shapeprops->m_friction_scaling[2]);


//////////
	//do Fh, do Rot Fh
	ci.m_do_fh = shapeprops->m_do_fh;
	ci.m_do_rot_fh = shapeprops->m_do_rot_fh;
	ci.m_fh_damping = smmaterial->m_fh_damping;
	ci.m_fh_distance = smmaterial->m_fh_distance;
	ci.m_fh_normal = smmaterial->m_fh_normal;
	ci.m_fh_spring = smmaterial->m_fh_spring;

	ci.m_collisionFilterGroup = 
		(isbulletsensor) ? short(CcdConstructionInfo::SensorFilter) :
		(isbulletdyna) ? short(CcdConstructionInfo::DefaultFilter) :
		(isbulletchar) ? short(CcdConstructionInfo::CharacterFilter) : 
		short(CcdConstructionInfo::StaticFilter);
	ci.m_collisionFilterMask = 
		(isbulletsensor) ? short(CcdConstructionInfo::AllFilter ^ CcdConstructionInfo::SensorFilter) :
		(isbulletdyna) ? short(CcdConstructionInfo::AllFilter) : 
		(isbulletchar) ? short(CcdConstructionInfo::AllFilter) :
		short(CcdConstructionInfo::AllFilter ^ CcdConstructionInfo::StaticFilter);
	ci.m_bRigid = isbulletdyna && isbulletrigidbody;
	ci.m_bSoft = isbulletsoftbody;
	ci.m_bDyna = isbulletdyna;
	ci.m_bSensor = isbulletsensor;
	ci.m_bCharacter = isbulletchar;
	ci.m_bGimpact = useGimpact;
	MT_Vector3 scaling = gameobj->NodeGetWorldScaling();
	ci.m_scaling.setValue(scaling[0], scaling[1], scaling[2]);
	CcdPhysicsController* physicscontroller = new CcdPhysicsController(ci);
	// shapeInfo is reference counted, decrement now as we don't use it anymore
	if (shapeInfo)
		shapeInfo->Release();

	gameobj->SetPhysicsController(physicscontroller,isbulletdyna);

	// record animation for dynamic objects
	if (isbulletdyna)
		gameobj->SetRecordAnimation(true);

	// don't add automatically sensor object, they are added when a collision sensor is registered
	if (!isbulletsensor && (blenderobject->lay & activeLayerBitInfo) != 0)
	{
		env->AddCcdPhysicsController( physicscontroller);
	}
	physicscontroller->SetNewClientInfo(gameobj->getClientInfo());
	{
		btRigidBody* rbody = physicscontroller->GetRigidBody();

		if (rbody)
		{
			if (isbulletrigidbody)
			{
				rbody->setLinearFactor(ci.m_linearFactor);
				rbody->setAngularFactor(ci.m_angularFactor);
			}

			if (rbody && (blenderobject->gameflag & OB_COLLISION_RESPONSE) != 0)
			{
				rbody->setActivationState(DISABLE_DEACTIVATION);
			}
		}
	}

	CcdPhysicsController* parentCtrl = parent ? (CcdPhysicsController*)parent->GetPhysicsController() : 0;
	physicscontroller->SetParentCtrl(parentCtrl);

	
	//Now done directly in ci.m_collisionFlags so that it propagates to replica
	//if (objprop->m_ghost)
	//{
	//	rbody->setCollisionFlags(rbody->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
	//}
	
	if (isbulletdyna && !isbulletrigidbody)
	{
#if 0
		//setting the inertia could achieve similar results to constraint the up
		//but it is prone to instability, so use special 'Angular' constraint
		btVector3 inertia = physicscontroller->GetRigidBody()->getInvInertiaDiagLocal();
		inertia.setX(0.f);
		inertia.setZ(0.f);

		physicscontroller->GetRigidBody()->setInvInertiaDiagLocal(inertia);
		physicscontroller->GetRigidBody()->updateInertiaTensor();
#endif

		//env->createConstraint(physicscontroller,0,PHY_ANGULAR_CONSTRAINT,0,0,0,0,0,1);
	
		//Now done directly in ci.m_bRigid so that it propagates to replica
		//physicscontroller->GetRigidBody()->setAngularFactor(0.f);
		;
	}

	bool isActor = (blenderobject->gameflag & OB_ACTOR)!=0;
	gameobj->getClientInfo()->m_type = 
		(isbulletsensor) ? ((isActor) ? KX_ClientObjectInfo::OBACTORSENSOR : KX_ClientObjectInfo::OBSENSOR) :
		(isActor) ? KX_ClientObjectInfo::ACTOR : KX_ClientObjectInfo::STATIC;

	// should we record animation for this object?
	if ((blenderobject->gameflag & OB_RECORD_ANIMATION) != 0)
		gameobj->SetRecordAnimation(true);

	// store materialname in auxinfo, needed for touchsensors
	if (meshobj)
	{
		const STR_String& matname=meshobj->GetMaterialName(0);
		gameobj->getClientInfo()->m_auxilary_info = (matname.Length() ? (void*)(matname.ReadPtr()+2) : NULL);
	} else
	{
		gameobj->getClientInfo()->m_auxilary_info = 0;
	}



	STR_String materialname;
	if (meshobj)
		materialname = meshobj->GetMaterialName(0);


#if 0
	///test for soft bodies
	if (objprop->m_softbody && physicscontroller)
	{
		btSoftBody* softBody = physicscontroller->GetSoftBody();
		if (softBody && gameobj->GetMesh(0))//only the first mesh, if any
		{
			//should be a mesh then, so add a soft body deformer
			KX_SoftBodyDeformer* softbodyDeformer = new KX_SoftBodyDeformer( gameobj->GetMesh(0),(BL_DeformableGameObject*)gameobj);
			gameobj->SetDeformer(softbodyDeformer);
		}
	}
#endif

}


void	KX_ClearBulletSharedShapes()
{
}

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
