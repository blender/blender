/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#pragma warning (disable : 4786)

// defines USE_ODE to choose physics engine
#include "KX_ConvertPhysicsObject.h"
#include "KX_GameObject.h"
#include "RAS_MeshObject.h"
#include "KX_Scene.h"
#include "SYS_System.h"

#include "PHY_Pro.h" //todo cleanup
#include "KX_ClientObjectInfo.h"

#include "GEN_Map.h"
#include "GEN_HashedPtr.h"

#include "KX_PhysicsEngineEnums.h"
#include "PHY_Pro.h"

#include "KX_MotionState.h" // bridge between motionstate and scenegraph node
#ifdef USE_ODE

#include "KX_OdePhysicsController.h"
#include "OdePhysicsEnvironment.h"
#endif //USE_ODE


// USE_SUMO_SOLID is defined in headerfile KX_ConvertPhysicsObject.h
#ifdef USE_SUMO_SOLID


#include "SumoPhysicsEnvironment.h"
#include "KX_SumoPhysicsController.h"


// sumo physics specific
#include "SM_Object.h"
#include "SM_FhObject.h"
#include "SM_Scene.h"
#include "SM_ClientObjectInfo.h"

#include "KX_SumoPhysicsController.h"

GEN_Map<GEN_HashedPtr,DT_ShapeHandle> map_gamemesh_to_sumoshape;

// forward declarations
void	BL_RegisterSumoObject(KX_GameObject* gameobj,class SM_Scene* sumoScene,DT_SceneHandle solidscene,class SM_Object* sumoObj,const char* matname,bool isDynamic,bool isActor);
DT_ShapeHandle CreateShapeFromMesh(RAS_MeshObject* meshobj);


void	KX_ConvertSumoObject(	class	KX_GameObject* gameobj,
							class	RAS_MeshObject* meshobj,
							class	KX_Scene* kxscene,
							PHY_ShapeProps* kxshapeprops,
							PHY_MaterialProps*	kxmaterial,
							struct	KX_ObjectProperties*	objprop)


{

	SM_ShapeProps* smprop = new SM_ShapeProps;

	smprop->m_ang_drag = kxshapeprops->m_ang_drag;
	smprop->m_do_anisotropic = kxshapeprops->m_do_anisotropic;
	smprop->m_do_fh = kxshapeprops->m_do_fh;
	smprop->m_do_rot_fh = kxshapeprops->m_do_rot_fh ;
	smprop->m_friction_scaling[0]  = kxshapeprops->m_friction_scaling[0];
	smprop->m_friction_scaling[1]  = kxshapeprops->m_friction_scaling[1];
	smprop->m_friction_scaling[2]  = kxshapeprops->m_friction_scaling[2];
	smprop->m_inertia = kxshapeprops->m_inertia;
	smprop->m_lin_drag = kxshapeprops->m_lin_drag;
	smprop->m_mass = kxshapeprops->m_mass;


	SM_MaterialProps* smmaterial = new SM_MaterialProps;

	smmaterial->m_fh_damping = kxmaterial->m_fh_damping;
	smmaterial->m_fh_distance = kxmaterial->m_fh_distance;
	smmaterial->m_fh_normal = kxmaterial->m_fh_normal;
	smmaterial->m_fh_spring = kxmaterial->m_fh_spring;
	smmaterial->m_friction = kxmaterial->m_friction;
	smmaterial->m_restitution = kxmaterial->m_restitution;

	class SumoPhysicsEnvironment* sumoEnv =
		(SumoPhysicsEnvironment*)kxscene->GetPhysicsEnvironment();

	SM_Scene*	sceneptr = sumoEnv->GetSumoScene();



	SM_Object*	sumoObj=NULL;

	if (objprop->m_dyna)
	{
		
		DT_ShapeHandle shape	=	DT_Sphere(0.0);

		if (objprop->m_ghost)
		{

			sumoObj					=	new SM_Object(shape,NULL,smprop,NULL);
		} else
		{
			sumoObj					=	new SM_Object(shape,smmaterial,smprop,NULL);
		}
		
		double radius = 		objprop->m_radius;
		
		MT_Scalar margin = radius;//0.5;
		sumoObj->setMargin(margin);
		
		//if (bRigidBody) 
		//{
			if (objprop->m_in_active_layer)
			{
				DT_AddObject(sumoEnv->GetSolidScene(),
					sumoObj->getObjectHandle());
			}
		//}
		
		if (objprop->m_angular_rigidbody)
		{
			sumoObj->setRigidBody(true);
		} else
		{
			sumoObj->setRigidBody(false);
		}

		bool isDynamic = true;
		bool isActor = true;

		BL_RegisterSumoObject(gameobj,sceneptr,sumoEnv->GetSolidScene(),sumoObj,NULL,isDynamic,isActor);
		
	} 
	else {
		// non physics object
		if (meshobj)
		{
			int numpolys = meshobj->NumPolygons();

			{

				DT_ShapeHandle complexshape=0;

				if (objprop->m_implicitbox)
				{
					complexshape = DT_Box(objprop->m_boundingbox.m_extends[0],objprop->m_boundingbox.m_extends[1],objprop->m_boundingbox.m_extends[2]);
				} else
				{
					if (numpolys>0)
					{
						complexshape	= 	CreateShapeFromMesh(meshobj);
					}
				}
				
				if (complexshape)
				{
					SM_Object *dynamicParent = NULL;

					if (objprop->m_dynamic_parent)
					{
						// problem is how to find the dynamic parent
						// in the scenegraph
						KX_SumoPhysicsController* sumoctrl = 
						(KX_SumoPhysicsController*)
							objprop->m_dynamic_parent->GetPhysicsController();

						if (sumoctrl)
						{
							dynamicParent = sumoctrl->GetSumoObject();
						}

						assert(dynamicParent);
					}
				
					
					if (objprop->m_ghost)
					{
						sumoObj	= new SM_Object(complexshape,NULL,NULL, dynamicParent);	
					} else
					{
						sumoObj	= new SM_Object(complexshape,smmaterial,NULL, dynamicParent);	
					}
					
					if (objprop->m_in_active_layer)
					{
						DT_AddObject(sumoEnv->GetSolidScene(),
							sumoObj->getObjectHandle());
					}
					
					
					const STR_String& matname=meshobj->GetMaterialName(0);

					
					BL_RegisterSumoObject(gameobj,sceneptr,
						sumoEnv->GetSolidScene(),sumoObj,
						matname.ReadPtr(),
						objprop->m_dyna,
						objprop->m_isactor);

				}
			}
		}
	}

	// physics object get updated here !

	
	// lazy evaluation because we might not support scaling !gameobj->UpdateTransform();

	if (objprop->m_in_active_layer && sumoObj)
	{
		sceneptr->add(*sumoObj);
	}

}



void	BL_RegisterSumoObject(KX_GameObject* gameobj,class SM_Scene* sumoScene,DT_SceneHandle solidscene,class SM_Object* sumoObj,const char* matname,bool isDynamic,bool isActor) {



		//gameobj->SetDynamic(isDynamic);

		PHY_IMotionState* motionstate = new KX_MotionState(gameobj->GetSGNode());

		// need easy access, not via 'node' etc.
		KX_SumoPhysicsController* physicscontroller = new KX_SumoPhysicsController(sumoScene,solidscene,sumoObj,motionstate,isDynamic);
		gameobj->SetPhysicsController(physicscontroller);
		physicscontroller->setClientInfo(gameobj);

		gameobj->GetSGNode()->AddSGController(physicscontroller);

		//gameobj->GetClientInfo()->m_type = (isActor ? 1 : 0);
		//gameobj->GetClientInfo()->m_clientobject = gameobj;

		// store materialname in auxinfo, needed for touchsensors
		//gameobj->GetClientInfo()->m_auxilary_info = (matname? (void*)(matname+2) : NULL);

		physicscontroller->SetObject(gameobj->GetSGNode());
				
		//gameobj->SetDynamicsScaling(MT_Vector3(1.0, 1.0, 1.0));

};



DT_ShapeHandle CreateShapeFromMesh(RAS_MeshObject* meshobj)
{

	DT_ShapeHandle* shapeptr = map_gamemesh_to_sumoshape[GEN_HashedPtr(meshobj)];
	if (shapeptr)
	{
		return *shapeptr;
	}
	
	// todo: shared meshes
	DT_ShapeHandle shape = DT_NewComplexShape();
	int p=0;
	int numpolys = meshobj->NumPolygons();
	if (!numpolys)
	{
		return NULL;
	}
	int numvalidpolys = 0;



	for (p=0;p<meshobj->m_triangle_indices.size();p++)
	{
		RAS_TriangleIndex& idx = meshobj->m_triangle_indices[p];
		
		// only add polygons that have the collisionflag set
		if (idx.m_collider)
		{
			DT_Begin();
			for (int v=0;v<3;v++)
			{
				int num = meshobj->m_xyz_index_to_vertex_index_mapping[idx.m_index[v]].size();
				if (num != 1)
				{
					int i=0;
				}
				RAS_MatArrayIndex& vertindex = meshobj->m_xyz_index_to_vertex_index_mapping[idx.m_index[v]][0];

				numvalidpolys++;
	
				{
					const MT_Point3& pt = meshobj->GetVertex(vertindex.m_array, 
													  vertindex.m_index,
													  (RAS_IPolyMaterial*)vertindex.m_matid)->xyz();
					DT_Vertex(pt[0],pt[1],pt[2]);
				}
			}
			DT_End();
		}
	}

	DT_EndComplexShape();

	if (numvalidpolys > 0)
	{
		map_gamemesh_to_sumoshape.insert(GEN_HashedPtr(meshobj),shape);
		return shape;
	}

	// memleak... todo: delete shape
	return NULL;
}


void	KX_ClearSumoSharedShapes()
{
	int numshapes = map_gamemesh_to_sumoshape.size();
	for (int i=0;i<numshapes ;i++)
	{
		DT_ShapeHandle shape = *map_gamemesh_to_sumoshape.at(i);
		DT_DeleteShape(shape);
	}
	
	map_gamemesh_to_sumoshape.clear();
}





#endif //USE_SUMO_SOLID


#ifdef USE_ODE

void	KX_ConvertODEEngineObject(KX_GameObject* gameobj,
							 RAS_MeshObject* meshobj,
							 KX_Scene* kxscene,
							struct	PHY_ShapeProps* shapeprops,
							struct	PHY_MaterialProps*	smmaterial,
							struct	KX_ObjectProperties*	objprop)
{
	// not yet, future extension :)
	bool dyna=objprop->m_dyna;
	bool fullRigidBody= ( objprop->m_dyna && objprop->m_angular_rigidbody) != 0;
	bool phantom = objprop->m_ghost;
	class PHY_IMotionState* motionstate = new KX_MotionState(gameobj->GetSGNode());

	class ODEPhysicsEnvironment* odeEnv =
		(ODEPhysicsEnvironment*)kxscene->GetPhysicsEnvironment();

	dxSpace* space = odeEnv->GetOdeSpace();
	dxWorld* world = odeEnv->GetOdeWorld();

	if (!objprop->m_implicitsphere &&
		MT_fuzzyZero(objprop->m_boundingbox.m_extends[0]) ||
		MT_fuzzyZero(objprop->m_boundingbox.m_extends[1]) ||
		MT_fuzzyZero(objprop->m_boundingbox.m_extends[2])
		)
	{

	} else
	{

		KX_OdePhysicsController* physicscontroller = 
			new KX_OdePhysicsController(
			dyna,
			fullRigidBody,
			phantom,
			motionstate,
			space,
			world,
			shapeprops->m_mass,
			smmaterial->m_friction,
			smmaterial->m_restitution,
			objprop->m_implicitsphere,
			objprop->m_boundingbox.m_center,
			objprop->m_boundingbox.m_extends,
			objprop->m_radius
			);

		gameobj->SetPhysicsController(physicscontroller);
		physicscontroller->setClientInfo(gameobj);						
		gameobj->GetSGNode()->AddSGController(physicscontroller);

		bool isActor = objprop->m_isactor;
		STR_String materialname;
		if (meshobj)
			materialname = meshobj->GetMaterialName(0);

		const char* matname = materialname.ReadPtr();


		physicscontroller->SetObject(gameobj->GetSGNode());
				
	}
}


#endif // USE_ODE
