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
#ifdef WIN32
#pragma warning (disable : 4786)
#endif

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
void	BL_RegisterSumoObject(KX_GameObject* gameobj,class SM_Scene* sumoScene,DT_SceneHandle solidscene,class SM_Object* sumoObj,const STR_String& matname,bool isDynamic,bool isActor);
DT_ShapeHandle CreateShapeFromMesh(RAS_MeshObject* meshobj);


void	KX_ConvertSumoObject(	KX_GameObject* gameobj,
				RAS_MeshObject* meshobj,
				KX_Scene* kxscene,
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
	smprop->m_radius = objprop->m_radius;


	SM_MaterialProps* smmaterial = new SM_MaterialProps;

	smmaterial->m_fh_damping = kxmaterial->m_fh_damping;
	smmaterial->m_fh_distance = kxmaterial->m_fh_distance;
	smmaterial->m_fh_normal = kxmaterial->m_fh_normal;
	smmaterial->m_fh_spring = kxmaterial->m_fh_spring;
	smmaterial->m_friction = kxmaterial->m_friction;
	smmaterial->m_restitution = kxmaterial->m_restitution;

	SumoPhysicsEnvironment* sumoEnv =
		(SumoPhysicsEnvironment*)kxscene->GetPhysicsEnvironment();

	SM_Scene*	sceneptr = sumoEnv->GetSumoScene();

	SM_Object*	sumoObj=NULL;

	if (objprop->m_dyna)
	{
		DT_ShapeHandle shape = NULL;
		switch (objprop->m_boundclass)
		{
			case KX_BOUNDBOX:
				shape = DT_NewBox(objprop->m_boundobject.box.m_extends[0], objprop->m_boundobject.box.m_extends[1], objprop->m_boundobject.box.m_extends[2]);
				break;
			case KX_BOUNDCYLINDER:
				shape = DT_NewCylinder(objprop->m_radius, objprop->m_boundobject.c.m_height);
				break;
			case KX_BOUNDCONE:
				shape = DT_NewCone(objprop->m_radius, objprop->m_boundobject.c.m_height);
				break;
/* Enabling this allows you to use dynamic mesh objects.  It's disabled 'cause it's really slow. */
			case KX_BOUNDMESH:
				if (meshobj && meshobj->NumPolygons() > 0)
				{
					if ((shape = CreateShapeFromMesh(meshobj)))
						break;
				}
				/* If CreateShapeFromMesh fails, fall through and use sphere */
			default:
			case KX_BOUNDSPHERE:
				shape = DT_NewSphere(objprop->m_radius);
				break;
				
		}
		
		sumoObj = new SM_Object(shape, !objprop->m_ghost?smmaterial:NULL,smprop,NULL);
		
		sumoObj->setRigidBody(objprop->m_angular_rigidbody?true:false);
		
		objprop->m_isactor = objprop->m_dyna = true;
		BL_RegisterSumoObject(gameobj,sceneptr,sumoEnv->GetSolidScene(),sumoObj,"",true, true);
		
	} 
	else {
		// non physics object
		if (meshobj)
		{
			int numpolys = meshobj->NumPolygons();
			{

				DT_ShapeHandle complexshape=0;

				switch (objprop->m_boundclass)
				{
					case KX_BOUNDBOX:
						complexshape = DT_NewBox(objprop->m_boundobject.box.m_extends[0], objprop->m_boundobject.box.m_extends[1], objprop->m_boundobject.box.m_extends[2]);
						break;
					case KX_BOUNDSPHERE:
						complexshape = DT_NewSphere(objprop->m_boundobject.c.m_radius);
						break;
					case KX_BOUNDCYLINDER:
						complexshape = DT_NewCylinder(objprop->m_boundobject.c.m_radius, objprop->m_boundobject.c.m_height);
						break;
					case KX_BOUNDCONE:
						complexshape = DT_NewCone(objprop->m_boundobject.c.m_radius, objprop->m_boundobject.c.m_height);
						break;
					default:
					case KX_BOUNDMESH:
						if (numpolys>0)
						{
							complexshape = CreateShapeFromMesh(meshobj);
							//std::cout << "Convert Physics Mesh: " << meshobj->GetName() << std::endl;
/*							if (!complexshape) 
							{
								// Something has to be done here - if the object has no polygons, it will not be able to have
								//   sensors attached to it. 
								DT_Vector3 pt = {0., 0., 0.};
								complexshape = DT_NewSphere(1.0);
								objprop->m_ghost = evilObject = true;
							} */
						}
						break;
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
				
					
					sumoObj	= new SM_Object(complexshape,!objprop->m_ghost?smmaterial:NULL,NULL, dynamicParent);	
					const STR_String& matname=meshobj->GetMaterialName(0);

					
					BL_RegisterSumoObject(gameobj,sceneptr,
						sumoEnv->GetSolidScene(),sumoObj,
						matname,
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



void	BL_RegisterSumoObject(
	KX_GameObject* gameobj,
	class SM_Scene* sumoScene,
	DT_SceneHandle solidscene,
	class SM_Object* sumoObj,
	const STR_String& matname,
	bool isDynamic,
	bool isActor) 
{



		//gameobj->SetDynamic(isDynamic);

		PHY_IMotionState* motionstate = new KX_MotionState(gameobj->GetSGNode());

		// need easy access, not via 'node' etc.
		KX_SumoPhysicsController* physicscontroller = new KX_SumoPhysicsController(sumoScene,solidscene,sumoObj,motionstate,isDynamic);
		gameobj->SetPhysicsController(physicscontroller);
		physicscontroller->setClientInfo(gameobj);
		
		if (!gameobj->getClientInfo())
			std::cout << "BL_RegisterSumoObject: WARNING: Object " << gameobj->GetName() << " has no client info" << std::endl;
		sumoObj->setClientObject(gameobj->getClientInfo());

		gameobj->GetSGNode()->AddSGController(physicscontroller);

		gameobj->getClientInfo()->m_type = (isActor ? KX_ClientObjectInfo::ACTOR : KX_ClientObjectInfo::STATIC);
		//gameobj->GetClientInfo()->m_clientobject = gameobj;

		// store materialname in auxinfo, needed for touchsensors
		gameobj->getClientInfo()->m_auxilary_info = (matname.Length() ? (void*)(matname.ReadPtr()+2) : NULL);

		physicscontroller->SetObject(gameobj->GetSGNode());
		
		//gameobj->SetDynamicsScaling(MT_Vector3(1.0, 1.0, 1.0));

};

DT_ShapeHandle CreateShapeFromMesh(RAS_MeshObject* meshobj)
{

	DT_ShapeHandle *shapeptr = map_gamemesh_to_sumoshape[GEN_HashedPtr(meshobj)];
	if (shapeptr)
	{
		return *shapeptr;
	}
	
	int numpolys = meshobj->NumPolygons();
	if (!numpolys)
	{
		return NULL;
	}
	int numvalidpolys = 0;

	for (int p=0; p<numpolys; p++)
	{
		RAS_Polygon* poly = meshobj->GetPolygon(p);
	
		// only add polygons that have the collisionflag set
		if (poly->IsCollider())
		{
			numvalidpolys++;
			break;
		}
	}
	
	if (numvalidpolys < 1)
		return NULL;
	
	DT_ShapeHandle shape = DT_NewComplexShape(NULL);
	
	
	numvalidpolys = 0;

	for (int p2=0; p2<numpolys; p2++)
	{
		RAS_Polygon* poly = meshobj->GetPolygon(p2);
	
		// only add polygons that have the collisionflag set
		if (poly->IsCollider())
		{   /* We have to tesselate here because SOLID can only raycast triangles */
		    DT_Begin();
			DT_Vector3 pt;
			/* V1 */
			meshobj->GetVertex(poly->GetVertexIndexBase().m_vtxarray, 
				poly->GetVertexIndexBase().m_indexarray[0],
				poly->GetMaterial()->GetPolyMaterial())->xyz().getValue(pt);
			DT_Vertex(pt);
			/* V2 */
			meshobj->GetVertex(poly->GetVertexIndexBase().m_vtxarray, 
				poly->GetVertexIndexBase().m_indexarray[1],
				poly->GetMaterial()->GetPolyMaterial())->xyz().getValue(pt);
			DT_Vertex(pt);
			/* V3 */
			meshobj->GetVertex(poly->GetVertexIndexBase().m_vtxarray, 
				poly->GetVertexIndexBase().m_indexarray[2],
				poly->GetMaterial()->GetPolyMaterial())->xyz().getValue(pt);
			DT_Vertex(pt);
			
			numvalidpolys++;
		    DT_End();
			
			if (poly->VertexCount() == 4)
			{
			    DT_Begin();
				/* V1 */
				meshobj->GetVertex(poly->GetVertexIndexBase().m_vtxarray, 
					poly->GetVertexIndexBase().m_indexarray[0],
					poly->GetMaterial()->GetPolyMaterial())->xyz().getValue(pt);
				DT_Vertex(pt);
				/* V3 */
				meshobj->GetVertex(poly->GetVertexIndexBase().m_vtxarray, 
					poly->GetVertexIndexBase().m_indexarray[2],
					poly->GetMaterial()->GetPolyMaterial())->xyz().getValue(pt);
				DT_Vertex(pt);
				/* V4 */
				meshobj->GetVertex(poly->GetVertexIndexBase().m_vtxarray, 
					poly->GetVertexIndexBase().m_indexarray[3],
					poly->GetMaterial()->GetPolyMaterial())->xyz().getValue(pt);
				DT_Vertex(pt);
			
				numvalidpolys++;
			    DT_End();
			}
	
		}
	}
	
	DT_EndComplexShape();

	if (numvalidpolys > 0)
	{
		map_gamemesh_to_sumoshape.insert(GEN_HashedPtr(meshobj),shape);
		return shape;
	}

	delete shape;
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
