/*
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
 * Ketsji scene. Holds references to all scene data.
 */

#ifdef WIN32
#pragma warning (disable : 4786)
#endif //WIN32

#include "KX_Scene.h"
#include "KX_PythonInit.h"
#include "MT_assert.h"
#include "KX_KetsjiEngine.h"
#include "KX_BlenderMaterial.h"
#include "RAS_IPolygonMaterial.h"
#include "ListValue.h"
#include "SCA_LogicManager.h"
#include "SCA_TimeEventManager.h"
//#include "SCA_AlwaysEventManager.h"
//#include "SCA_RandomEventManager.h"
//#include "KX_RayEventManager.h"
#include "KX_TouchEventManager.h"
#include "SCA_KeyboardManager.h"
#include "SCA_MouseManager.h"
//#include "SCA_PropertyEventManager.h"
#include "SCA_ActuatorEventManager.h"
#include "SCA_BasicEventManager.h"
#include "KX_Camera.h"
#include "SCA_JoystickManager.h"

#include "RAS_MeshObject.h"

#include "RAS_IRasterizer.h"
#include "RAS_BucketManager.h"

#include "FloatValue.h"
#include "SCA_IController.h"
#include "SCA_IActuator.h"
#include "SG_Node.h"
#include "SYS_System.h"
#include "SG_Controller.h"
#include "SG_IObject.h"
#include "SG_Tree.h"
#include "DNA_group_types.h"
#include "DNA_scene_types.h"
#include "BKE_anim.h"

#include "KX_SG_NodeRelationships.h"

#include "KX_NetworkEventManager.h"
#include "NG_NetworkScene.h"
#include "PHY_IPhysicsEnvironment.h"
#include "KX_IPhysicsController.h"
#include "PHY_IGraphicController.h"
#include "KX_BlenderSceneConverter.h"
#include "KX_MotionState.h"

#include "BL_ModifierDeformer.h"
#include "BL_ShapeDeformer.h"
#include "BL_DeformableGameObject.h"
#include "KX_SoftBodyDeformer.h"

// to get USE_BULLET!
#include "KX_ConvertPhysicsObject.h"

#ifdef USE_BULLET
#include "CcdPhysicsEnvironment.h"
#include "CcdPhysicsController.h"
#endif

#include "KX_Light.h"

#include <stdio.h>

void* KX_SceneReplicationFunc(SG_IObject* node,void* gameobj,void* scene)
{
	KX_GameObject* replica = ((KX_Scene*)scene)->AddNodeReplicaObject(node,(KX_GameObject*)gameobj);

	if(replica)
		replica->Release();

	return (void*)replica;
}

void* KX_SceneDestructionFunc(SG_IObject* node,void* gameobj,void* scene)
{
	((KX_Scene*)scene)->RemoveNodeDestructObject(node,(KX_GameObject*)gameobj);

	return NULL;
};

bool KX_Scene::KX_ScenegraphUpdateFunc(SG_IObject* node,void* gameobj,void* scene)
{
	return ((SG_Node*)node)->Schedule(((KX_Scene*)scene)->m_sghead);
}

bool KX_Scene::KX_ScenegraphRescheduleFunc(SG_IObject* node,void* gameobj,void* scene)
{
	return ((SG_Node*)node)->Reschedule(((KX_Scene*)scene)->m_sghead);
}

SG_Callbacks KX_Scene::m_callbacks = SG_Callbacks(
	KX_SceneReplicationFunc,
	KX_SceneDestructionFunc,
	KX_GameObject::UpdateTransformFunc,
	KX_Scene::KX_ScenegraphUpdateFunc,
	KX_Scene::KX_ScenegraphRescheduleFunc);

// temporarily var until there is a button in the userinterface
// (defined in KX_PythonInit.cpp)
extern bool gUseVisibilityTemp;

KX_Scene::KX_Scene(class SCA_IInputDevice* keyboarddevice,
				   class SCA_IInputDevice* mousedevice,
				   class NG_NetworkDeviceInterface *ndi,
				   const STR_String& sceneName,
				   Scene *scene,
				   class RAS_ICanvas* canvas): 
	PyObjectPlus(),
	m_keyboardmgr(NULL),
	m_mousemgr(NULL),
	m_sceneConverter(NULL),
	m_physicsEnvironment(0),
	m_sceneName(sceneName),
	m_networkDeviceInterface(ndi),
	m_active_camera(NULL),
	m_ueberExecutionPriority(0),
	m_blenderScene(scene)
{
	m_suspendedtime = 0.0;
	m_suspendeddelta = 0.0;

	m_dbvt_culling = false;
	m_dbvt_occlusion_res = 0;
	m_activity_culling = false;
	m_suspend = false;
	m_isclearingZbuffer = true;
	m_tempObjectList = new CListValue();
	m_objectlist = new CListValue();
	m_parentlist = new CListValue();
	m_lightlist= new CListValue();
	m_inactivelist = new CListValue();
	m_euthanasyobjects = new CListValue();

	m_logicmgr = new SCA_LogicManager();
	
	m_timemgr = new SCA_TimeEventManager(m_logicmgr);
	m_keyboardmgr = new SCA_KeyboardManager(m_logicmgr,keyboarddevice);
	m_mousemgr = new SCA_MouseManager(m_logicmgr,mousedevice, canvas);
	
	//SCA_AlwaysEventManager* alwaysmgr = new SCA_AlwaysEventManager(m_logicmgr);
	//SCA_PropertyEventManager* propmgr = new SCA_PropertyEventManager(m_logicmgr);
	SCA_ActuatorEventManager* actmgr = new SCA_ActuatorEventManager(m_logicmgr);
	//SCA_RandomEventManager* rndmgr = new SCA_RandomEventManager(m_logicmgr);
	SCA_BasicEventManager* basicmgr = new SCA_BasicEventManager(m_logicmgr);
	//KX_RayEventManager* raymgr = new KX_RayEventManager(m_logicmgr);

	KX_NetworkEventManager* netmgr = new KX_NetworkEventManager(m_logicmgr, ndi);
	
	

	//m_logicmgr->RegisterEventManager(alwaysmgr);
	//m_logicmgr->RegisterEventManager(propmgr);
	m_logicmgr->RegisterEventManager(actmgr);
	m_logicmgr->RegisterEventManager(m_keyboardmgr);
	m_logicmgr->RegisterEventManager(m_mousemgr);
	m_logicmgr->RegisterEventManager(m_timemgr);
	//m_logicmgr->RegisterEventManager(rndmgr);
	//m_logicmgr->RegisterEventManager(raymgr);
	m_logicmgr->RegisterEventManager(netmgr);
	m_logicmgr->RegisterEventManager(basicmgr);


	SYS_SystemHandle hSystem = SYS_GetSystem();
	bool nojoystick= SYS_GetCommandLineInt(hSystem,"nojoystick",0);
	if (!nojoystick)
	{
		SCA_JoystickManager *joymgr	= new SCA_JoystickManager(m_logicmgr);
		m_logicmgr->RegisterEventManager(joymgr);
	}

	MT_assert (m_networkDeviceInterface != NULL);
	m_networkScene = new NG_NetworkScene(m_networkDeviceInterface);
	
	m_rootnode = NULL;

	m_bucketmanager=new RAS_BucketManager();
	
#ifndef DISABLE_PYTHON
	m_attr_dict = PyDict_New(); /* new ref */
	m_draw_call_pre = NULL;
	m_draw_call_post = NULL;
#endif
}



KX_Scene::~KX_Scene()
{
	// The release of debug properties used to be in SCA_IScene::~SCA_IScene
	// It's still there but we remove all properties here otherwise some
	// reference might be hanging and causing late release of objects
	RemoveAllDebugProperties();

	while (GetRootParentList()->GetCount() > 0) 
	{
		KX_GameObject* parentobj = (KX_GameObject*) GetRootParentList()->GetValue(0);
		this->RemoveObject(parentobj);
	}

	if(m_objectlist)
		m_objectlist->Release();

	if (m_parentlist)
		m_parentlist->Release();
	
	if (m_inactivelist)
		m_inactivelist->Release();

	if (m_lightlist)
		m_lightlist->Release();
	
	if (m_tempObjectList)
		m_tempObjectList->Release();

	if (m_euthanasyobjects)
		m_euthanasyobjects->Release();

	if (m_logicmgr)
		delete m_logicmgr;

	if (m_physicsEnvironment)
		delete m_physicsEnvironment;

	if (m_networkScene)
		delete m_networkScene;
	
	if (m_bucketmanager)
	{
		delete m_bucketmanager;
	}

#ifndef DISABLE_PYTHON
	PyDict_Clear(m_attr_dict);
	Py_DECREF(m_attr_dict);

	Py_XDECREF(m_draw_call_pre);
	Py_XDECREF(m_draw_call_post);
#endif
}

RAS_BucketManager* KX_Scene::GetBucketManager()
{
	return m_bucketmanager;
}


CListValue* KX_Scene::GetTempObjectList()
{
	return m_tempObjectList;
}

CListValue* KX_Scene::GetObjectList()
{
	return m_objectlist;
}


CListValue* KX_Scene::GetRootParentList()
{
	return m_parentlist;
}

CListValue* KX_Scene::GetInactiveList()
{
	return m_inactivelist;
}



CListValue* KX_Scene::GetLightList()
{
	return m_lightlist;
}

SCA_LogicManager* KX_Scene::GetLogicManager()
{
	return m_logicmgr;
}

SCA_TimeEventManager* KX_Scene::GetTimeEventManager()
{
	return m_timemgr;
}



 
list<class KX_Camera*>* KX_Scene::GetCameras()
{
	return &m_cameras;
}



void KX_Scene::SetFramingType(RAS_FrameSettings & frame_settings)
{
	m_frame_settings = frame_settings;
};

/**
 * Return a const reference to the framing 
 * type set by the above call.
 * The contents are not guarenteed to be sensible
 * if you don't call the above function.
 */
const RAS_FrameSettings& KX_Scene::GetFramingType() const 
{
	return m_frame_settings;
};	



/**
 * Store the current scene's viewport on the 
 * game engine canvas.
 */
void KX_Scene::SetSceneViewport(const RAS_Rect &viewport)
{
	m_viewport = viewport;
}



const RAS_Rect& KX_Scene::GetSceneViewport() const 
{
	return m_viewport;
}



void KX_Scene::SetWorldInfo(class KX_WorldInfo* worldinfo)
{
	m_worldinfo = worldinfo;
}



class KX_WorldInfo* KX_Scene::GetWorldInfo()
{
	return m_worldinfo;
}


const STR_String& KX_Scene::GetName()
{
	return m_sceneName;
}


void KX_Scene::Suspend()
{
	m_suspend = true;
}

void KX_Scene::Resume()
{
	m_suspend = false;
}

void KX_Scene::SetActivityCulling(bool b)
{
	m_activity_culling = b;
}

bool KX_Scene::IsSuspended()
{
	return m_suspend;
}

bool KX_Scene::IsClearingZBuffer()
{
	return m_isclearingZbuffer;
}

void KX_Scene::EnableZBufferClearing(bool isclearingZbuffer)
{
	m_isclearingZbuffer = isclearingZbuffer;
}

void KX_Scene::RemoveNodeDestructObject(class SG_IObject* node,class CValue* gameobj)
{
	KX_GameObject* orgobj = (KX_GameObject*)gameobj;	
	if (NewRemoveObject(orgobj) != 0)
	{
		// object is not yet deleted because a reference is hanging somewhere.
		// This should not happen anymore since we use proxy object for Python
		// confident enough to put an assert?
		//assert(false);
		printf("Zombie object! name=%s\n", orgobj->GetName().ReadPtr());
		orgobj->SetSGNode(NULL);
		PHY_IGraphicController* ctrl = orgobj->GetGraphicController();
		if (ctrl)
		{
			// a graphic controller is set, we must delete it as the node will be deleted
			delete ctrl;
			orgobj->SetGraphicController(NULL);
		}
	}
	if (node)
		delete node;
}

KX_GameObject* KX_Scene::AddNodeReplicaObject(class SG_IObject* node, class CValue* gameobj)
{
	// for group duplication, limit the duplication of the hierarchy to the
	// objects that are part of the group. 
	if (!IsObjectInGroup(gameobj))
		return NULL;
	
	KX_GameObject* orgobj = (KX_GameObject*)gameobj;
	KX_GameObject* newobj = (KX_GameObject*)orgobj->GetReplica();
	m_map_gameobject_to_replica.insert(orgobj, newobj);

	// also register 'timers' (time properties) of the replica
	int numprops = newobj->GetPropertyCount();

	for (int i = 0; i < numprops; i++)
	{
		CValue* prop = newobj->GetProperty(i);

		if (prop->GetProperty("timer"))
			this->m_timemgr->AddTimeProperty(prop);
	}

	if (node)
	{
		newobj->SetSGNode((SG_Node*)node);
	}
	else
	{
		m_rootnode = new SG_Node(newobj,this,KX_Scene::m_callbacks);
	
		// this fixes part of the scaling-added object bug
		SG_Node* orgnode = orgobj->GetSGNode();
		m_rootnode->SetLocalScale(orgnode->GetLocalScale());
		m_rootnode->SetLocalPosition(orgnode->GetLocalPosition());
		m_rootnode->SetLocalOrientation(orgnode->GetLocalOrientation());

		// define the relationship between this node and it's parent.
		KX_NormalParentRelation * parent_relation = 
			KX_NormalParentRelation::New();
		m_rootnode->SetParentRelation(parent_relation);

		newobj->SetSGNode(m_rootnode);
	}
	
	SG_IObject* replicanode = newobj->GetSGNode();
//	SG_Node* rootnode = (replicanode == m_rootnode ? NULL : m_rootnode);

	replicanode->SetSGClientObject(newobj);

	// this is the list of object that are send to the graphics pipeline
	m_objectlist->Add(newobj->AddRef());
	if (newobj->GetGameObjectType()==SCA_IObject::OBJ_LIGHT)
		m_lightlist->Add(newobj->AddRef());
	newobj->AddMeshUser();

	// logic cannot be replicated, until the whole hierarchy is replicated.
	m_logicHierarchicalGameObjects.push_back(newobj);
	//replicate controllers of this node
	SGControllerList	scenegraphcontrollers = orgobj->GetSGNode()->GetSGControllerList();
	replicanode->RemoveAllControllers();
	SGControllerList::iterator cit;
	//int numcont = scenegraphcontrollers.size();
	
	for (cit = scenegraphcontrollers.begin();!(cit==scenegraphcontrollers.end());++cit)
	{
		// controller replication is quite complicated
		// only replicate ipo and physics controller for now

		SG_Controller* replicacontroller = (*cit)->GetReplica((SG_Node*) replicanode);
		if (replicacontroller)
		{
			replicacontroller->SetObject(replicanode);
			replicanode->AddSGController(replicacontroller);
		}
	}
	// replicate graphic controller
	if (orgobj->GetGraphicController())
	{
		PHY_IMotionState* motionstate = new KX_MotionState(newobj->GetSGNode());
		PHY_IGraphicController* newctrl = orgobj->GetGraphicController()->GetReplica(motionstate);
		newctrl->setNewClientInfo(newobj->getClientInfo());
		newobj->SetGraphicController(newctrl);
	}
	return newobj;
}



// before calling this method KX_Scene::ReplicateLogic(), make sure to
// have called 'GameObject::ReParentLogic' for each object this
// hierarchy that's because first ALL bricks must exist in the new
// replica of the hierarchy in order to make cross-links work properly
// !
// It is VERY important that the order of sensors and actuators in
// the replicated object is preserved: it is used to reconnect the logic.
// This method is more robust then using the bricks name in case of complex 
// group replication. The replication of logic bricks is done in 
// SCA_IObject::ReParentLogic(), make sure it preserves the order of the bricks.
void KX_Scene::ReplicateLogic(KX_GameObject* newobj)
{
	// also relink the controller to sensors/actuators
	SCA_ControllerList& controllers = newobj->GetControllers();
	//SCA_SensorList&     sensors     = newobj->GetSensors();
	//SCA_ActuatorList&   actuators   = newobj->GetActuators();

	for (SCA_ControllerList::iterator itc = controllers.begin(); !(itc==controllers.end());itc++)
	{
		SCA_IController* cont = (*itc);
		cont->SetUeberExecutePriority(m_ueberExecutionPriority);
		vector<SCA_ISensor*> linkedsensors = cont->GetLinkedSensors();
		vector<SCA_IActuator*> linkedactuators = cont->GetLinkedActuators();

		// disconnect the sensors and actuators
		// do it directly on the list at this controller is not connected to anything at this stage
		cont->GetLinkedSensors().clear();
		cont->GetLinkedActuators().clear();
		
		// now relink each sensor
		for (vector<SCA_ISensor*>::iterator its = linkedsensors.begin();!(its==linkedsensors.end());its++)
		{
			SCA_ISensor* oldsensor = (*its);
			SCA_IObject* oldsensorobj = oldsensor->GetParent();
			SCA_IObject* newsensorobj = NULL;
		
			// the original owner of the sensor has been replicated?
			void **h_obj = m_map_gameobject_to_replica[oldsensorobj];
			if (h_obj)
				newsensorobj = (SCA_IObject*)(*h_obj);
			if (!newsensorobj)
			{
				// no, then the sensor points outside the hierachy, keep it the same
				if (m_objectlist->SearchValue(oldsensorobj))
					// only replicate links that points to active objects
					m_logicmgr->RegisterToSensor(cont,oldsensor);
			}
			else
			{
				// yes, then the new sensor has the same position
				SCA_SensorList& sensorlist = oldsensorobj->GetSensors();
				SCA_SensorList::iterator sit;
				SCA_ISensor* newsensor = NULL;
				int sensorpos;

				for (sensorpos=0, sit=sensorlist.begin(); sit!=sensorlist.end(); sit++, sensorpos++)
				{
					if ((*sit) == oldsensor) 
					{
						newsensor = newsensorobj->GetSensors().at(sensorpos);
						break;
					}
				}
				assert(newsensor != NULL);
				m_logicmgr->RegisterToSensor(cont,newsensor);
			}
		}
		
		// now relink each actuator
		for (vector<SCA_IActuator*>::iterator ita = linkedactuators.begin();!(ita==linkedactuators.end());ita++)
		{
			SCA_IActuator* oldactuator = (*ita);
			SCA_IObject* oldactuatorobj = oldactuator->GetParent();
			SCA_IObject* newactuatorobj = NULL;

			// the original owner of the sensor has been replicated?
			void **h_obj = m_map_gameobject_to_replica[oldactuatorobj];
			if (h_obj)
				newactuatorobj = (SCA_IObject*)(*h_obj);

			if (!newactuatorobj)
			{
				// no, then the sensor points outside the hierachy, keep it the same
				if (m_objectlist->SearchValue(oldactuatorobj))
					// only replicate links that points to active objects
					m_logicmgr->RegisterToActuator(cont,oldactuator);
			}
			else
			{
				// yes, then the new sensor has the same position
				SCA_ActuatorList& actuatorlist = oldactuatorobj->GetActuators();
				SCA_ActuatorList::iterator ait;
				SCA_IActuator* newactuator = NULL;
				int actuatorpos;

				for (actuatorpos=0, ait=actuatorlist.begin(); ait!=actuatorlist.end(); ait++, actuatorpos++)
				{
					if ((*ait) == oldactuator) 
					{
						newactuator = newactuatorobj->GetActuators().at(actuatorpos);
						break;
					}
				}
				assert(newactuator != NULL);
				m_logicmgr->RegisterToActuator(cont,newactuator);
				newactuator->SetUeberExecutePriority(m_ueberExecutionPriority);
			}
		}
	}
	// ready to set initial state
	newobj->ResetState();
}

void KX_Scene::DupliGroupRecurse(CValue* obj, int level)
{
	KX_GameObject* groupobj = (KX_GameObject*) obj;
	KX_GameObject* replica;
	KX_GameObject* gameobj;
	Object* blgroupobj = groupobj->GetBlenderObject();
	Group* group;
	GroupObject *go;
	vector<KX_GameObject*> duplilist;

	if (!groupobj->GetSGNode() ||
		!groupobj->IsDupliGroup() ||
		level>MAX_DUPLI_RECUR)
		return;

	// we will add one group at a time
	m_logicHierarchicalGameObjects.clear();
	m_map_gameobject_to_replica.clear();
	m_ueberExecutionPriority++;
	// for groups will do something special: 
	// we will force the creation of objects to those in the group only
	// Again, this is match what Blender is doing (it doesn't care of parent relationship)
	m_groupGameObjects.clear();

	group = blgroupobj->dup_group;
	for(go=(GroupObject*)group->gobject.first; go; go=(GroupObject*)go->next) 
	{
		Object* blenderobj = go->ob;
		if (blgroupobj == blenderobj)
			// this check is also in group_duplilist()
			continue;

		gameobj = (KX_GameObject*)m_logicmgr->FindGameObjByBlendObj(blenderobj);
		if (gameobj == NULL) 
		{
			// this object has not been converted!!!
			// Should not happen as dupli group are created automatically 
			continue;
		}

		gameobj->SetBlenderGroupObject(blgroupobj);

		if ((blenderobj->lay & group->layer)==0)
		{
			// object is not visible in the 3D view, will not be instantiated
			continue;
		}
		m_groupGameObjects.insert(gameobj);
	}

	set<CValue*>::iterator oit;
	for (oit=m_groupGameObjects.begin(); oit != m_groupGameObjects.end(); oit++)
	{
		gameobj = (KX_GameObject*)(*oit);

		KX_GameObject *parent = gameobj->GetParent();
		if (parent != NULL)
		{
			parent->Release(); // GetParent() increased the refcount

			// this object is not a top parent. Either it is the child of another
			// object in the group and it will be added automatically when the parent
			// is added. Or it is the child of an object outside the group and the group
			// is inconsistent, skip it anyway
			continue;
		}
		replica = (KX_GameObject*) AddNodeReplicaObject(NULL,gameobj);
		// add to 'rootparent' list (this is the list of top hierarchy objects, updated each frame)
		m_parentlist->Add(replica->AddRef());

		// recurse replication into children nodes
		NodeList& children = gameobj->GetSGNode()->GetSGChildren();

		replica->GetSGNode()->ClearSGChildren();
		for (NodeList::iterator childit = children.begin();!(childit==children.end());++childit)
		{
			SG_Node* orgnode = (*childit);
			SG_Node* childreplicanode = orgnode->GetSGReplica();
			if (childreplicanode)
				replica->GetSGNode()->AddChild(childreplicanode);
		}
		// don't replicate logic now: we assume that the objects in the group can have
		// logic relationship, even outside parent relationship
		// In order to match 3D view, the position of groupobj is used as a 
		// transformation matrix instead of the new position. This means that 
		// the group reference point is 0,0,0

		// get the rootnode's scale
		MT_Vector3 newscale = groupobj->NodeGetWorldScaling();
		// set the replica's relative scale with the rootnode's scale
		replica->NodeSetRelativeScale(newscale);

		MT_Point3 offset(group->dupli_ofs);
		MT_Point3 newpos = groupobj->NodeGetWorldPosition() + 
			newscale*(groupobj->NodeGetWorldOrientation() * (gameobj->NodeGetWorldPosition()-offset));
		replica->NodeSetLocalPosition(newpos);
		// set the orientation after position for softbody!
		MT_Matrix3x3 newori = groupobj->NodeGetWorldOrientation() * gameobj->NodeGetWorldOrientation();
		replica->NodeSetLocalOrientation(newori);
		// update scenegraph for entire tree of children
		replica->GetSGNode()->UpdateWorldData(0);
		replica->GetSGNode()->SetBBox(gameobj->GetSGNode()->BBox());
		replica->GetSGNode()->SetRadius(gameobj->GetSGNode()->Radius());
		// we can now add the graphic controller to the physic engine
		replica->ActivateGraphicController(true);

		// done with replica
		replica->Release();
	}

	// the logic must be replicated first because we need
	// the new logic bricks before relinking
	vector<KX_GameObject*>::iterator git;
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		(*git)->ReParentLogic();
	}
	
	//	relink any pointers as necessary, sort of a temporary solution
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		// this will also relink the actuator to objects within the hierarchy
		(*git)->Relink(&m_map_gameobject_to_replica);
		// add the object in the layer of the parent
		(*git)->SetLayer(groupobj->GetLayer());
		// If the object was a light, we need to update it's RAS_LightObject as well
		if ((*git)->GetGameObjectType()==SCA_IObject::OBJ_LIGHT)
		{
			KX_LightObject* lightobj = static_cast<KX_LightObject*>(*git);
			lightobj->GetLightData()->m_layer = groupobj->GetLayer();
		}
	}

	// replicate crosslinks etc. between logic bricks
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		ReplicateLogic((*git));
	}
	
	// now look if object in the hierarchy have dupli group and recurse
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		if ((*git) != groupobj && (*git)->IsDupliGroup())
			// can't instantiate group immediately as it destroys m_logicHierarchicalGameObjects
			duplilist.push_back((*git));
	}

	for (git = duplilist.begin(); !(git == duplilist.end()); ++git)
	{
		DupliGroupRecurse((*git), level+1);
	}
}


SCA_IObject* KX_Scene::AddReplicaObject(class CValue* originalobject,
										class CValue* parentobject,
										int lifespan)
{

	m_logicHierarchicalGameObjects.clear();
	m_map_gameobject_to_replica.clear();
	m_groupGameObjects.clear();

	// todo: place a timebomb in the object, for temporarily objects :)
	// lifespan of zero means 'this object lives forever'
	KX_GameObject* originalobj = (KX_GameObject*) originalobject;
	KX_GameObject* parentobj = (KX_GameObject*) parentobject;

	m_ueberExecutionPriority++;

	// lets create a replica
	KX_GameObject* replica = (KX_GameObject*) AddNodeReplicaObject(NULL,originalobj);

	if (lifespan > 0)
	{
		// add a timebomb to this object
		// for now, convert between so called frames and realtime
		m_tempObjectList->Add(replica->AddRef());
		CValue *fval = new CFloatValue(lifespan*0.02);
		replica->SetProperty("::timebomb",fval);
		fval->Release();
	}

	// add to 'rootparent' list (this is the list of top hierarchy objects, updated each frame)
	m_parentlist->Add(replica->AddRef());

	// recurse replication into children nodes

	NodeList& children = originalobj->GetSGNode()->GetSGChildren();

	replica->GetSGNode()->ClearSGChildren();
	for (NodeList::iterator childit = children.begin();!(childit==children.end());++childit)
	{
		SG_Node* orgnode = (*childit);
		SG_Node* childreplicanode = orgnode->GetSGReplica();
		if (childreplicanode)
			replica->GetSGNode()->AddChild(childreplicanode);
	}

	// At this stage all the objects in the hierarchy have been duplicated,
	// we can update the scenegraph, we need it for the duplication of logic
	MT_Point3 newpos = ((KX_GameObject*) parentobject)->NodeGetWorldPosition();
	replica->NodeSetLocalPosition(newpos);

	MT_Matrix3x3 newori = ((KX_GameObject*) parentobject)->NodeGetWorldOrientation();
	replica->NodeSetLocalOrientation(newori);
	
	// get the rootnode's scale
	MT_Vector3 newscale = parentobj->GetSGNode()->GetRootSGParent()->GetLocalScale();

	// set the replica's relative scale with the rootnode's scale
	replica->NodeSetRelativeScale(newscale);

	replica->GetSGNode()->UpdateWorldData(0);
	replica->GetSGNode()->SetBBox(originalobj->GetSGNode()->BBox());
	replica->GetSGNode()->SetRadius(originalobj->GetSGNode()->Radius());
	// the size is correct, we can add the graphic controller to the physic engine
	replica->ActivateGraphicController(true);

	// now replicate logic
	vector<KX_GameObject*>::iterator git;
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		(*git)->ReParentLogic();
	}
	
	//	relink any pointers as necessary, sort of a temporary solution
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		// this will also relink the actuators in the hierarchy
		(*git)->Relink(&m_map_gameobject_to_replica);
		// add the object in the layer of the parent
		(*git)->SetLayer(parentobj->GetLayer());
		// If the object was a light, we need to update it's RAS_LightObject as well
		if ((*git)->GetGameObjectType()==SCA_IObject::OBJ_LIGHT)
		{
			KX_LightObject* lightobj = static_cast<KX_LightObject*>(*git);
			lightobj->GetLightData()->m_layer = parentobj->GetLayer();
		}
	}

	// replicate crosslinks etc. between logic bricks
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		ReplicateLogic((*git));
	}
	
	// check if there are objects with dupligroup in the hierarchy
	vector<KX_GameObject*> duplilist;
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		if ((*git)->IsDupliGroup())
		{
			// separate list as m_logicHierarchicalGameObjects is also used by DupliGroupRecurse()
			duplilist.push_back(*git);
		}
	}
	for (git = duplilist.begin();!(git==duplilist.end());++git)
	{
		DupliGroupRecurse(*git, 0);
	}
	//	don't release replica here because we are returning it, not done with it...
	return replica;
}



void KX_Scene::RemoveObject(class CValue* gameobj)
{
	KX_GameObject* newobj = (KX_GameObject*) gameobj;

	// disconnect child from parent
	SG_Node* node = newobj->GetSGNode();

	if (node)
	{
		node->DisconnectFromParent();

		// recursively destruct
		node->Destruct();
	}
	//no need to do that: the object is destroyed and memory released 
	//newobj->SetSGNode(0);
}

void KX_Scene::DelayedRemoveObject(class CValue* gameobj)
{
	//KX_GameObject* newobj = (KX_GameObject*) gameobj;
	if (!m_euthanasyobjects->SearchValue(gameobj))
	{
		m_euthanasyobjects->Add(gameobj->AddRef());
	} 
}



int KX_Scene::NewRemoveObject(class CValue* gameobj)
{
	int ret;
	KX_GameObject* newobj = (KX_GameObject*) gameobj;

	/* Invalidate the python reference, since the object may exist in script lists
	 * its possible that it wont be automatically invalidated, so do it manually here,
	 * 
	 * if for some reason the object is added back into the scene python can always get a new Proxy
	 */
	newobj->InvalidateProxy();

	// keep the blender->game object association up to date
	// note that all the replicas of an object will have the same
	// blender object, that's why we need to check the game object
	// as only the deletion of the original object must be recorded
	m_logicmgr->UnregisterGameObj(newobj->GetBlenderObject(), gameobj);

	//todo: look at this
	//GetPhysicsEnvironment()->RemovePhysicsController(gameobj->getPhysicsController());

	// remove all sensors/controllers/actuators from logicsystem...
	
	SCA_SensorList& sensors = newobj->GetSensors();
	for (SCA_SensorList::iterator its = sensors.begin();
		 !(its==sensors.end());its++)
	{
		m_logicmgr->RemoveSensor(*its);
	}

	SCA_ControllerList& controllers = newobj->GetControllers();
	for (SCA_ControllerList::iterator itc = controllers.begin();
		 !(itc==controllers.end());itc++)
	{
		m_logicmgr->RemoveController(*itc);
	}

	SCA_ActuatorList& actuators = newobj->GetActuators();
	for (SCA_ActuatorList::iterator ita = actuators.begin();
		 !(ita==actuators.end());ita++)
	{
		m_logicmgr->RemoveActuator(*ita);
	}
	// the sensors/controllers/actuators must also be released, this is done in ~SCA_IObject

	// now remove the timer properties from the time manager
	int numprops = newobj->GetPropertyCount();

	for (int i = 0; i < numprops; i++)
	{
		CValue* propval = newobj->GetProperty(i);
		if (propval->GetProperty("timer"))
		{
			m_timemgr->RemoveTimeProperty(propval);
		}
	}
	
	newobj->RemoveMeshes();
	ret = 1;
	if (newobj->GetGameObjectType()==SCA_IObject::OBJ_LIGHT && m_lightlist->RemoveValue(newobj))
		ret = newobj->Release();
	if (m_objectlist->RemoveValue(newobj))
		ret = newobj->Release();
	if (m_tempObjectList->RemoveValue(newobj))
		ret = newobj->Release();
	if (m_parentlist->RemoveValue(newobj))
		ret = newobj->Release();
	if (m_inactivelist->RemoveValue(newobj))
		ret = newobj->Release();
	if (m_euthanasyobjects->RemoveValue(newobj))
		ret = newobj->Release();
		
	if (newobj == m_active_camera)
	{
		//no AddRef done on m_active_camera so no Release
		//m_active_camera->Release();
		m_active_camera = NULL;
	}

	// in case this is a camera
	m_cameras.remove((KX_Camera*)newobj);

	/* currently does nothing, keep incase we need to Unregister something */
#if 0
	if (m_sceneConverter)
		m_sceneConverter->UnregisterGameObject(newobj);
#endif
	
	// return value will be 0 if the object is actually deleted (all reference gone)
	
	return ret;
}



void KX_Scene::ReplaceMesh(class CValue* obj,void* meshobj, bool use_gfx, bool use_phys)
{
	KX_GameObject* gameobj = static_cast<KX_GameObject*>(obj);
	RAS_MeshObject* mesh = static_cast<RAS_MeshObject*>(meshobj);

	if(!gameobj) {
		std::cout << "KX_Scene::ReplaceMesh Warning: invalid object, doing nothing" << std::endl;
		return;
	}

	if(use_gfx && mesh != NULL)
	{		
	gameobj->RemoveMeshes();
	gameobj->AddMesh(mesh);
	
	if (gameobj->m_isDeformable)
	{
		BL_DeformableGameObject* newobj = static_cast<BL_DeformableGameObject*>( gameobj );
		
		if (newobj->GetDeformer())
		{
			delete newobj->GetDeformer();
			newobj->SetDeformer(NULL);
		}

		if (mesh->GetMesh()) 
		{
			// we must create a new deformer but which one?
			KX_GameObject* parentobj = newobj->GetParent();
			// this always return the original game object (also for replicate)
			Object* blendobj = newobj->GetBlenderObject();
			// object that owns the new mesh
			Object* oldblendobj = static_cast<struct Object*>(m_logicmgr->FindBlendObjByGameMeshName(mesh->GetName()));
			Mesh* blendmesh = mesh->GetMesh();

			bool bHasModifier = BL_ModifierDeformer::HasCompatibleDeformer(blendobj);
			bool bHasShapeKey = blendmesh->key != NULL && blendmesh->key->type==KEY_RELATIVE;
			bool bHasDvert = blendmesh->dvert != NULL;
			bool bHasArmature = 
				BL_ModifierDeformer::HasArmatureDeformer(blendobj) &&
				parentobj &&								// current parent is armature
				parentobj->GetGameObjectType() == SCA_IObject::OBJ_ARMATURE &&
				oldblendobj &&								// needed for mesh deform
				blendobj->parent &&							// original object had armature (not sure this test is needed)
				blendobj->parent->type == OB_ARMATURE &&
				blendmesh->dvert!=NULL;						// mesh has vertex group
			bool bHasSoftBody = (!parentobj && (blendobj->gameflag & OB_SOFT_BODY));

			bool releaseParent = true;

			
			if (oldblendobj==NULL) {
				if (bHasModifier || bHasShapeKey || bHasDvert || bHasArmature) {
					std::cout << "warning: ReplaceMesh() new mesh is not used in an object from the current scene, you will get incorrect behavior" << std::endl;
					bHasShapeKey= bHasDvert= bHasArmature=bHasModifier= false;
				}
			}
			
			if (bHasModifier)
			{
				BL_ModifierDeformer* modifierDeformer;
				if (bHasShapeKey || bHasArmature)
				{
					modifierDeformer = new BL_ModifierDeformer(
						newobj,
						m_blenderScene,
						oldblendobj, blendobj,
						mesh,
						true,
						static_cast<BL_ArmatureObject*>( parentobj )
					);
					releaseParent= false;
					modifierDeformer->LoadShapeDrivers(blendobj->parent);
				}
				else
				{
					modifierDeformer = new BL_ModifierDeformer(
						newobj,
						m_blenderScene,
						oldblendobj, blendobj,
						mesh,
						false,
						NULL
					);
				}
				newobj->SetDeformer(modifierDeformer);
			} 
			else 	if (bHasShapeKey)
			{
				BL_ShapeDeformer* shapeDeformer;
				if (bHasArmature) 
				{
					shapeDeformer = new BL_ShapeDeformer(
						newobj,
						oldblendobj, blendobj,
						mesh,
						true,
						true,
						static_cast<BL_ArmatureObject*>( parentobj )
					);
					releaseParent= false;
					shapeDeformer->LoadShapeDrivers(blendobj->parent);
				}
				else
				{
					shapeDeformer = new BL_ShapeDeformer(
						newobj,
						oldblendobj, blendobj,
						mesh,
						false,
						true,
						NULL
					);
				}
				newobj->SetDeformer( shapeDeformer);
			}
			else if (bHasArmature) 
			{
				BL_SkinDeformer* skinDeformer = new BL_SkinDeformer(
					newobj,
					oldblendobj, blendobj,
					mesh,
					true,
					true,
					static_cast<BL_ArmatureObject*>( parentobj )
				);
				releaseParent= false;
				newobj->SetDeformer(skinDeformer);
			}
			else if (bHasDvert)
			{
				BL_MeshDeformer* meshdeformer = new BL_MeshDeformer(
					newobj, oldblendobj, mesh
				);
				newobj->SetDeformer(meshdeformer);
			}
			else if (bHasSoftBody)
			{
				KX_SoftBodyDeformer *softdeformer = new KX_SoftBodyDeformer(mesh, newobj);
				newobj->SetDeformer(softdeformer);
			}

			// release parent reference if its not being used 
			if( releaseParent && parentobj)
				parentobj->Release();
		}
	}

	gameobj->AddMeshUser();
	}
	
	if(use_phys) { /* update the new assigned mesh with the physics mesh */
		KX_ReInstanceBulletShapeFromMesh(gameobj, NULL, use_gfx?NULL:mesh);
	}
}

KX_Camera* KX_Scene::FindCamera(KX_Camera* cam)
{
	list<KX_Camera*>::iterator it = m_cameras.begin();

	while ( (it != m_cameras.end()) 
			&& ((*it) != cam) ) {
	  it++;
	}

	return ((it == m_cameras.end()) ? NULL : (*it));
}


KX_Camera* KX_Scene::FindCamera(STR_String& name)
{
	list<KX_Camera*>::iterator it = m_cameras.begin();

	while ( (it != m_cameras.end()) 
			&& ((*it)->GetName() != name) ) {
	  it++;
	}

	return ((it == m_cameras.end()) ? NULL : (*it));
}

void KX_Scene::AddCamera(KX_Camera* cam)
{
	if (!FindCamera(cam))
		m_cameras.push_back(cam);
}


KX_Camera* KX_Scene::GetActiveCamera()
{	
	// NULL if not defined
	return m_active_camera;
}


void KX_Scene::SetActiveCamera(KX_Camera* cam)
{
	// only set if the cam is in the active list? Or add it otherwise?
	if (!FindCamera(cam)){
		AddCamera(cam);
		if (cam) std::cout << "Added cam " << cam->GetName() << std::endl;
	} 

	m_active_camera = cam;
}

void KX_Scene::SetCameraOnTop(KX_Camera* cam)
{
	if (!FindCamera(cam)){
		// adding is always done at the back, so that's all that needs to be done
		AddCamera(cam);
		if (cam) std::cout << "Added cam " << cam->GetName() << std::endl;
	} else {
		m_cameras.remove(cam);
		m_cameras.push_back(cam);
	}
}


void KX_Scene::UpdateMeshTransformations()
{
	// do this incrementally in the future
	for (int i = 0; i < m_objectlist->GetCount(); i++)
	{
		KX_GameObject* gameobj = (KX_GameObject*)m_objectlist->GetValue(i);
		gameobj->GetOpenGLMatrix();
	}
}

void KX_Scene::MarkVisible(SG_Tree *node, RAS_IRasterizer* rasty, KX_Camera* cam, int layer)
{
	int intersect = KX_Camera::INTERSECT;
	KX_GameObject *gameobj = node->Client()?(KX_GameObject*) node->Client()->GetSGClientObject():NULL;
	bool visible = (gameobj && gameobj->GetVisible() && (!layer || (gameobj->GetLayer() & layer)));
	bool dotest = visible || node->Left() || node->Right();

	/* If the camera is inside the box, assume intersect. */
	if (dotest && !node->inside( cam->NodeGetWorldPosition()))
	{
		MT_Scalar radius = node->Radius();
		MT_Point3 center = node->Center();
		
		intersect =  cam->SphereInsideFrustum(center, radius); 
		
		if (intersect == KX_Camera::INTERSECT)
		{
			MT_Point3 box[8];
			node->get(box);
			intersect = cam->BoxInsideFrustum(box);
		}
	}

	switch (intersect)
	{
		case KX_Camera::OUTSIDE:
			MarkSubTreeVisible(node, rasty, false, cam);
			break;
		case KX_Camera::INTERSECT:
			if (gameobj)
				MarkVisible(rasty, gameobj, cam, layer);
			if (node->Left())
				MarkVisible(node->Left(), rasty, cam, layer);
			if (node->Right())
				MarkVisible(node->Right(), rasty, cam, layer);
			break;
		case KX_Camera::INSIDE:
			MarkSubTreeVisible(node, rasty, true, cam, layer);
			break;
	}
}

void KX_Scene::MarkSubTreeVisible(SG_Tree *node, RAS_IRasterizer* rasty, bool visible, KX_Camera* cam, int layer)
{
	if (node->Client())
	{
		KX_GameObject *gameobj = (KX_GameObject*) node->Client()->GetSGClientObject();
		if (gameobj->GetVisible())
		{
			if (visible)
			{
				int nummeshes = gameobj->GetMeshCount();
				
				// this adds the vertices to the display list
				for (int m=0;m<nummeshes;m++)
					(gameobj->GetMesh(m))->SchedulePolygons(rasty->GetDrawingMode());
			}

			gameobj->SetCulled(!visible);
			gameobj->UpdateBuckets(false);
		}
	}
	if (node->Left())
		MarkSubTreeVisible(node->Left(), rasty, visible, cam, layer);
	if (node->Right())
		MarkSubTreeVisible(node->Right(), rasty, visible, cam, layer);
}

void KX_Scene::MarkVisible(RAS_IRasterizer* rasty, KX_GameObject* gameobj,KX_Camera*  cam,int layer)
{
	// User (Python/Actuator) has forced object invisible...
	if (!gameobj->GetSGNode() || !gameobj->GetVisible())
		return;
	
	// Shadow lamp layers
	if(layer && !(gameobj->GetLayer() & layer)) {
		gameobj->SetCulled(true);
		gameobj->UpdateBuckets(false);
		return;
	}

	// If Frustum culling is off, the object is always visible.
	bool vis = !cam->GetFrustumCulling();
	
	// If the camera is inside this node, then the object is visible.
	if (!vis)
	{
		vis = gameobj->GetSGNode()->inside( cam->GetCameraLocation() );
	}
		
	// Test the object's bound sphere against the view frustum.
	if (!vis)
	{
		MT_Vector3 scale = gameobj->GetSGNode()->GetWorldScaling();
		MT_Scalar radius = fabs(scale[scale.closestAxis()] * gameobj->GetSGNode()->Radius());
		switch (cam->SphereInsideFrustum(gameobj->NodeGetWorldPosition(), radius))
		{
			case KX_Camera::INSIDE:
				vis = true;
				break;
			case KX_Camera::OUTSIDE:
				vis = false;
				break;
			case KX_Camera::INTERSECT:
				// Test the object's bound box against the view frustum.
				MT_Point3 box[8];
				gameobj->GetSGNode()->getBBox(box); 
				vis = cam->BoxInsideFrustum(box) != KX_Camera::OUTSIDE;
				break;
		}
	}
	
	if (vis)
	{
		int nummeshes = gameobj->GetMeshCount();
		
		for (int m=0;m<nummeshes;m++)
		{
			// this adds the vertices to the display list
			(gameobj->GetMesh(m))->SchedulePolygons(rasty->GetDrawingMode());
		}
		// Visibility/ non-visibility are marked
		// elsewhere now.
		gameobj->SetCulled(false);
		gameobj->UpdateBuckets(false);
	} else {
		gameobj->SetCulled(true);
		gameobj->UpdateBuckets(false);
	}
}

void KX_Scene::PhysicsCullingCallback(KX_ClientObjectInfo* objectInfo, void* cullingInfo)
{
	KX_GameObject* gameobj = objectInfo->m_gameobject;
	if (!gameobj->GetVisible())
		// ideally, invisible objects should be removed from the culling tree temporarily
		return;
	if(((CullingInfo*)cullingInfo)->m_layer && !(gameobj->GetLayer() & ((CullingInfo*)cullingInfo)->m_layer))
		// used for shadow: object is not in shadow layer
		return;

	// make object visible
	gameobj->SetCulled(false);
	gameobj->UpdateBuckets(false);
}

void KX_Scene::CalculateVisibleMeshes(RAS_IRasterizer* rasty,KX_Camera* cam, int layer)
{
	bool dbvt_culling = false;
	if (m_dbvt_culling) 
	{
		// test culling through Bullet
		PHY__Vector4 planes[6];
		// get the clip planes
		MT_Vector4* cplanes = cam->GetNormalizedClipPlanes();
		// and convert
		planes[0].setValue(cplanes[4].getValue());	// near
		planes[1].setValue(cplanes[5].getValue());	// far
		planes[2].setValue(cplanes[0].getValue());	// left
		planes[3].setValue(cplanes[1].getValue());	// right
		planes[4].setValue(cplanes[2].getValue());	// top
		planes[5].setValue(cplanes[3].getValue());	// bottom
		CullingInfo info(layer);
		dbvt_culling = m_physicsEnvironment->cullingTest(PhysicsCullingCallback,&info,planes,5,m_dbvt_occlusion_res);
	}
	if (!dbvt_culling) {
		// the physics engine couldn't help us, do it the hard way
		for (int i = 0; i < m_objectlist->GetCount(); i++)
		{
			MarkVisible(rasty, static_cast<KX_GameObject*>(m_objectlist->GetValue(i)), cam, layer);
		}
	}
}

// logic stuff
void KX_Scene::LogicBeginFrame(double curtime)
{
	// have a look at temp objects ...
	int lastobj = m_tempObjectList->GetCount() - 1;
	
	for (int i = lastobj; i >= 0; i--)
	{
		CValue* objval = m_tempObjectList->GetValue(i);
		CFloatValue* propval = (CFloatValue*) objval->GetProperty("::timebomb");
		
		if (propval)
		{
			float timeleft = propval->GetNumber() - 1.0/KX_KetsjiEngine::GetTicRate();
			
			if (timeleft > 0)
			{
				propval->SetFloat(timeleft);
			}
			else
			{
				DelayedRemoveObject(objval);
				// remove obj
			}
		}
		else
		{
			// all object is the tempObjectList should have a clock
		}
	}
	m_logicmgr->BeginFrame(curtime, 1.0/KX_KetsjiEngine::GetTicRate());
}



void KX_Scene::LogicUpdateFrame(double curtime, bool frame)
{
	m_logicmgr->UpdateFrame(curtime, frame);
}



void KX_Scene::LogicEndFrame()
{
	m_logicmgr->EndFrame();
	int numobj = m_euthanasyobjects->GetCount();

	KX_GameObject* obj;

	while ((numobj = m_euthanasyobjects->GetCount()) > 0)
	{
		// remove the object from this list to make sure we will not hit it again
		obj = (KX_GameObject*)m_euthanasyobjects->GetValue(numobj-1);
		m_euthanasyobjects->Remove(numobj-1);
		obj->Release();
		RemoveObject(obj);
	}
}



/**
  * UpdateParents: SceneGraph transformation update.
  */
void KX_Scene::UpdateParents(double curtime)
{
	// we use the SG dynamic list
	SG_Node* node;

	while ((node = SG_Node::GetNextScheduled(m_sghead)) != NULL)
	{
		node->UpdateWorldData(curtime);
	}

	//for (int i=0; i<GetRootParentList()->GetCount(); i++)
	//{
	//	KX_GameObject* parentobj = (KX_GameObject*)GetRootParentList()->GetValue(i);
	//	parentobj->NodeUpdateGS(curtime);
	//}

	// the list must be empty here
	assert(m_sghead.Empty());
	// some nodes may be ready for reschedule, move them to schedule list for next time
	while ((node = SG_Node::GetNextRescheduled(m_sghead)) != NULL)
	{
		node->Schedule(m_sghead);
	}
}


RAS_MaterialBucket* KX_Scene::FindBucket(class RAS_IPolyMaterial* polymat, bool &bucketCreated)
{
	return m_bucketmanager->FindBucket(polymat, bucketCreated);
}



void KX_Scene::RenderBuckets(const MT_Transform & cameratransform,
							 class RAS_IRasterizer* rasty,
							 class RAS_IRenderTools* rendertools)
{
	m_bucketmanager->Renderbuckets(cameratransform,rasty,rendertools);
	KX_BlenderMaterial::EndFrame();
}

void KX_Scene::UpdateObjectActivity(void) 
{
	if (m_activity_culling) {
		/* determine the activity criterium and set objects accordingly */
		int i=0;
		
		MT_Point3 camloc = GetActiveCamera()->NodeGetWorldPosition(); //GetCameraLocation();
		
		for (i=0;i<GetObjectList()->GetCount();i++)
		{
			KX_GameObject* ob = (KX_GameObject*) GetObjectList()->GetValue(i);
			
			if (!ob->GetIgnoreActivityCulling()) {
				/* Simple test: more than 10 away from the camera, count
				 * Manhattan distance. */
				MT_Point3 obpos = ob->NodeGetWorldPosition();
				
				if ( (fabs(camloc[0] - obpos[0]) > m_activity_box_radius)
					 || (fabs(camloc[1] - obpos[1]) > m_activity_box_radius)
					 || (fabs(camloc[2] - obpos[2]) > m_activity_box_radius) )
				{			
					ob->Suspend();
				} else {
					ob->Resume();
				}
			}
		}		
	}
}

void KX_Scene::SetActivityCullingRadius(float f)
{
	if (f < 0.5)
		f = 0.5;
	m_activity_box_radius = f;
}
	
NG_NetworkDeviceInterface* KX_Scene::GetNetworkDeviceInterface()
{
	return m_networkDeviceInterface;
}

NG_NetworkScene* KX_Scene::GetNetworkScene()
{
	return m_networkScene;
}

void KX_Scene::SetNetworkDeviceInterface(NG_NetworkDeviceInterface* newInterface)
{
	m_networkDeviceInterface = newInterface;
}

void KX_Scene::SetNetworkScene(NG_NetworkScene *newScene)
{
	m_networkScene = newScene;
}


void	KX_Scene::SetGravity(const MT_Vector3& gravity)
{
	GetPhysicsEnvironment()->setGravity(gravity[0],gravity[1],gravity[2]);
}

void KX_Scene::SetSceneConverter(class KX_BlenderSceneConverter* sceneConverter)
{
	m_sceneConverter = sceneConverter;
}

void KX_Scene::SetPhysicsEnvironment(class PHY_IPhysicsEnvironment* physEnv)
{
	m_physicsEnvironment = physEnv;
	if(m_physicsEnvironment) {
		KX_TouchEventManager* touchmgr = new KX_TouchEventManager(m_logicmgr, physEnv);
		m_logicmgr->RegisterEventManager(touchmgr);
	}
}
 
void KX_Scene::setSuspendedTime(double suspendedtime)
{
	m_suspendedtime = suspendedtime;
}
double KX_Scene::getSuspendedTime()
{
	return m_suspendedtime;
}
void KX_Scene::setSuspendedDelta(double suspendeddelta)
{
	m_suspendeddelta = suspendeddelta;
}
double KX_Scene::getSuspendedDelta()
{
	return m_suspendeddelta;
}

#include "KX_BulletPhysicsController.h"

static void MergeScene_LogicBrick(SCA_ILogicBrick* brick, KX_Scene *to)
{
	SCA_LogicManager *logicmgr= to->GetLogicManager();

	brick->Replace_IScene(to);
	brick->Replace_NetworkScene(to->GetNetworkScene());

	SCA_ISensor *sensor=  dynamic_cast<class SCA_ISensor *>(brick);
	if(sensor) {
		sensor->Replace_EventManager(logicmgr);
	}

	/* near sensors have physics controllers */
	KX_TouchSensor *touch_sensor = dynamic_cast<class KX_TouchSensor *>(brick);
	if(touch_sensor) {
		touch_sensor->GetPhysicsController()->SetPhysicsEnvironment(to->GetPhysicsEnvironment());
	}
}

#include "CcdGraphicController.h" // XXX  ctrl->SetPhysicsEnvironment(to->GetPhysicsEnvironment());
#include "CcdPhysicsEnvironment.h" // XXX  ctrl->SetPhysicsEnvironment(to->GetPhysicsEnvironment());
#include "KX_BulletPhysicsController.h"


static void MergeScene_GameObject(KX_GameObject* gameobj, KX_Scene *to, KX_Scene *from)
{
	{
		SCA_ActuatorList& actuators= gameobj->GetActuators();
		SCA_ActuatorList::iterator ita;

		for (ita = actuators.begin(); !(ita==actuators.end()); ++ita)
		{
			MergeScene_LogicBrick(*ita, to);
		}
	}


	{
		SCA_SensorList& sensors= gameobj->GetSensors();
		SCA_SensorList::iterator its;

		for (its = sensors.begin(); !(its==sensors.end()); ++its)
		{
			MergeScene_LogicBrick(*its, to);
		}
	}

	{
		SCA_ControllerList& controllers= gameobj->GetControllers();
		SCA_ControllerList::iterator itc;

		for (itc = controllers.begin(); !(itc==controllers.end()); ++itc)
		{
			SCA_IController *cont= *itc;
			MergeScene_LogicBrick(cont, to);

			vector<SCA_ISensor*> linkedsensors = cont->GetLinkedSensors();
			vector<SCA_IActuator*> linkedactuators = cont->GetLinkedActuators();

			for (vector<SCA_IActuator*>::iterator ita = linkedactuators.begin();!(ita==linkedactuators.end());++ita) {
				MergeScene_LogicBrick(*ita, to);
			}

			for (vector<SCA_ISensor*>::iterator its = linkedsensors.begin();!(its==linkedsensors.end());++its) {
				MergeScene_LogicBrick(*its, to);
			}
		}
	}

	/* graphics controller */
	PHY_IGraphicController *ctrl = gameobj->GetGraphicController();
	if(ctrl) {
		/* SHOULD update the m_cullingTree */
		ctrl->SetPhysicsEnvironment(to->GetPhysicsEnvironment());
	}

	/* SG_Node can hold a scene reference */
	SG_Node *sg= gameobj->GetSGNode();
	if(sg) {
		if(sg->GetSGClientInfo() == from) {
			sg->SetSGClientInfo(to);
		}

		SGControllerList::iterator contit;
		SGControllerList& controllers = sg->GetSGControllerList();
		for (contit = controllers.begin();contit!=controllers.end();++contit)
		{
			KX_BulletPhysicsController *phys_ctrl= dynamic_cast<KX_BulletPhysicsController *>(*contit);
			if (phys_ctrl)
				phys_ctrl->SetPhysicsEnvironment(to->GetPhysicsEnvironment());
		}
	}
	/* If the object is a light, update it's scene */
	if (gameobj->GetGameObjectType() == SCA_IObject::OBJ_LIGHT)
		((KX_LightObject*)gameobj)->UpdateScene(to);

	/* Add the object to the scene's logic manager */
	to->GetLogicManager()->RegisterGameObjectName(gameobj->GetName(), gameobj);
	to->GetLogicManager()->RegisterGameObj(gameobj->GetBlenderObject(), gameobj);

	for (int i=0; i<gameobj->GetMeshCount(); ++i)
		to->GetLogicManager()->RegisterGameMeshName(gameobj->GetMesh(i)->GetName(), gameobj->GetBlenderObject());
}

bool KX_Scene::MergeScene(KX_Scene *other)
{
	CcdPhysicsEnvironment *env=			dynamic_cast<CcdPhysicsEnvironment *>(this->GetPhysicsEnvironment());
	CcdPhysicsEnvironment *env_other=	dynamic_cast<CcdPhysicsEnvironment *>(other->GetPhysicsEnvironment());

	if((env==NULL) != (env_other==NULL)) /* TODO - even when both scenes have NONE physics, the other is loaded with bullet enabled, ??? */
	{
		printf("KX_Scene::MergeScene: physics scenes type differ, aborting\n");
		printf("\tsource %d, terget %d\n", (int)(env!=NULL), (int)(env_other!=NULL));
		return false;
	}

	if(GetSceneConverter() != other->GetSceneConverter()) {
		printf("KX_Scene::MergeScene: converters differ, aborting\n");
		return false;
	}


	GetBucketManager()->MergeBucketManager(other->GetBucketManager());

	/* move materials across, assume they both use the same scene-converters */
	GetSceneConverter()->MergeScene(this, other);

	/* active + inactive == all ??? - lets hope so */
	for (int i = 0; i < other->GetObjectList()->GetCount(); i++)
	{
		KX_GameObject* gameobj = (KX_GameObject*)other->GetObjectList()->GetValue(i);
		MergeScene_GameObject(gameobj, this, other);

		gameobj->UpdateBuckets(false); /* only for active objects */
	}

	for (int i = 0; i < other->GetInactiveList()->GetCount(); i++)
	{
		KX_GameObject* gameobj = (KX_GameObject*)other->GetInactiveList()->GetValue(i);
		MergeScene_GameObject(gameobj, this, other);
	}

	GetTempObjectList()->MergeList(other->GetTempObjectList());
	other->GetTempObjectList()->ReleaseAndRemoveAll();

	GetObjectList()->MergeList(other->GetObjectList());
	other->GetObjectList()->ReleaseAndRemoveAll();

	GetInactiveList()->MergeList(other->GetInactiveList());
	other->GetInactiveList()->ReleaseAndRemoveAll();

	GetRootParentList()->MergeList(other->GetRootParentList());
	other->GetRootParentList()->ReleaseAndRemoveAll();

	GetLightList()->MergeList(other->GetLightList());
	other->GetLightList()->ReleaseAndRemoveAll();

	if(env) /* bullet scene? - dummy scenes dont need touching */
		env->MergeEnvironment(env_other);

	/* merge logic */
	{
		SCA_LogicManager *logicmgr=			GetLogicManager();
		SCA_LogicManager *logicmgr_other=	other->GetLogicManager();

		vector<class SCA_EventManager*>evtmgrs= logicmgr->GetEventManagers();
		//vector<class SCA_EventManager*>evtmgrs_others= logicmgr_other->GetEventManagers();

		//SCA_EventManager *evtmgr;
		SCA_EventManager *evtmgr_other;

		for(unsigned int i= 0; i < evtmgrs.size(); i++) {
			evtmgr_other= logicmgr_other->FindEventManager(evtmgrs[i]->GetType());

			if(evtmgr_other) /* unlikely but possible one scene has a joystick and not the other */
				evtmgr_other->Replace_LogicManager(logicmgr);

			/* when merging objects sensors are moved across into the new manager, dont need to do this here */
		}
	}
	return true;
}

void KX_Scene::Update2DFilter(vector<STR_String>& propNames, void* gameObj, RAS_2DFilterManager::RAS_2DFILTER_MODE filtermode, int pass, STR_String& text)
{
	m_filtermanager.EnableFilter(propNames, gameObj, filtermode, pass, text);
}

void KX_Scene::Render2DFilters(RAS_ICanvas* canvas)
{
	m_filtermanager.RenderFilters(canvas);
}

#ifndef DISABLE_PYTHON

void KX_Scene::RunDrawingCallbacks(PyObject* cb_list)
{
	int len;

	if (cb_list && (len=PyList_GET_SIZE(cb_list)))
	{
		PyObject* args= PyTuple_New(0); // save python creating each call
		PyObject* func;
		PyObject* ret;

		// Iterate the list and run the callbacks
		for (int pos=0; pos < len; pos++)
		{
			func= PyList_GET_ITEM(cb_list, pos);
			ret= PyObject_Call(func, args, NULL);
			if (ret==NULL) {
				PyErr_Print();
				PyErr_Clear();
			}
			else {
				Py_DECREF(ret);
			}
		}

		Py_DECREF(args);
	}
}

//----------------------------------------------------------------------------
//Python

PyTypeObject KX_Scene::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_Scene",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,
	&Sequence,
	&Mapping,
	0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&CValue::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_Scene::Methods[] = {
	KX_PYMETHODTABLE(KX_Scene, addObject),
	KX_PYMETHODTABLE(KX_Scene, end),
	KX_PYMETHODTABLE(KX_Scene, restart),
	KX_PYMETHODTABLE(KX_Scene, replace),
	KX_PYMETHODTABLE(KX_Scene, suspend),
	KX_PYMETHODTABLE(KX_Scene, resume),
	
	/* dict style access */
	KX_PYMETHODTABLE(KX_Scene, get),
	
	{NULL,NULL} //Sentinel
};
static PyObject *Map_GetItem(PyObject *self_v, PyObject *item)
{
	KX_Scene* self= static_cast<KX_Scene*>BGE_PROXY_REF(self_v);
	const char *attr_str= _PyUnicode_AsString(item);
	PyObject* pyconvert;
	
	if (self==NULL) {
		PyErr_SetString(PyExc_SystemError, "val = scene[key]: KX_Scene, "BGE_PROXY_ERROR_MSG);
		return NULL;
	}
	
	if (self->m_attr_dict && (pyconvert=PyDict_GetItem(self->m_attr_dict, item))) {
		
		if (attr_str)
			PyErr_Clear();
		Py_INCREF(pyconvert);
		return pyconvert;
	}
	else {
		if(attr_str)	PyErr_Format(PyExc_KeyError, "value = scene[key]: KX_Scene, key \"%s\" does not exist", attr_str);
		else			PyErr_SetString(PyExc_KeyError, "value = scene[key]: KX_Scene, key does not exist");
		return NULL;
	}
		
}

static int Map_SetItem(PyObject *self_v, PyObject *key, PyObject *val)
{
	KX_Scene* self= static_cast<KX_Scene*>BGE_PROXY_REF(self_v);
	const char *attr_str= _PyUnicode_AsString(key);
	if(attr_str==NULL)
		PyErr_Clear();
	
	if (self==NULL) {
		PyErr_SetString(PyExc_SystemError, "scene[key] = value: KX_Scene, "BGE_PROXY_ERROR_MSG);
		return -1;
	}
	
	if (val==NULL) { /* del ob["key"] */
		int del= 0;
		
		if(self->m_attr_dict)
			del |= (PyDict_DelItem(self->m_attr_dict, key)==0) ? 1:0;
		
		if (del==0) {
			if(attr_str)	PyErr_Format(PyExc_KeyError, "scene[key] = value: KX_Scene, key \"%s\" could not be set", attr_str);
			else			PyErr_SetString(PyExc_KeyError, "del scene[key]: KX_Scene, key could not be deleted");
			return -1;
		}
		else if (self->m_attr_dict) {
			PyErr_Clear(); /* PyDict_DelItem sets an error when it fails */
		}
	}
	else { /* ob["key"] = value */
		int set = 0;

		if (self->m_attr_dict==NULL) /* lazy init */
			self->m_attr_dict= PyDict_New();
		
		
		if(PyDict_SetItem(self->m_attr_dict, key, val)==0)
			set= 1;
		else
			PyErr_SetString(PyExc_KeyError, "scene[key] = value: KX_Scene, key not be added to internal dictionary");
	
		if(set==0)
			return -1; /* pythons error value */
		
	}
	
	return 0; /* success */
}

static int Seq_Contains(PyObject *self_v, PyObject *value)
{
	KX_Scene* self= static_cast<KX_Scene*>BGE_PROXY_REF(self_v);
	
	if (self==NULL) {
		PyErr_SetString(PyExc_SystemError, "val in scene: KX_Scene, "BGE_PROXY_ERROR_MSG);
		return -1;
	}
	
	if (self->m_attr_dict && PyDict_GetItem(self->m_attr_dict, value))
		return 1;
	
	return 0;
}

PyMappingMethods KX_Scene::Mapping = {
	(lenfunc)NULL					, 			/*inquiry mp_length */
	(binaryfunc)Map_GetItem,		/*binaryfunc mp_subscript */
	(objobjargproc)Map_SetItem,	/*objobjargproc mp_ass_subscript */
};

PySequenceMethods KX_Scene::Sequence = {
	NULL,		/* Cant set the len otherwise it can evaluate as false */
	NULL,		/* sq_concat */
	NULL,		/* sq_repeat */
	NULL,		/* sq_item */
	NULL,		/* sq_slice */
	NULL,		/* sq_ass_item */
	NULL,		/* sq_ass_slice */
	(objobjproc)Seq_Contains,	/* sq_contains */
	(binaryfunc) NULL, /* sq_inplace_concat */
	(ssizeargfunc) NULL, /* sq_inplace_repeat */
};

PyObject* KX_Scene::pyattr_get_name(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self= static_cast<KX_Scene*>(self_v);
	return PyUnicode_FromString(self->GetName().ReadPtr());
}

PyObject* KX_Scene::pyattr_get_objects(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self= static_cast<KX_Scene*>(self_v);
	return self->GetObjectList()->GetProxy();
}

PyObject* KX_Scene::pyattr_get_objects_inactive(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self= static_cast<KX_Scene*>(self_v);
	return self->GetInactiveList()->GetProxy();
}

PyObject* KX_Scene::pyattr_get_lights(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self= static_cast<KX_Scene*>(self_v);
	return self->GetLightList()->GetProxy();
}

PyObject* KX_Scene::pyattr_get_cameras(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	/* With refcounts in this case...
	 * the new CListValue is owned by python, so its possible python holds onto it longer then the BGE
	 * however this is the same with "scene.objects + []", when you make a copy by adding lists.
	 */
	
	KX_Scene* self= static_cast<KX_Scene*>(self_v);
	CListValue* clist = new CListValue();
	
	/* return self->GetCameras()->GetProxy(); */
	
	list<KX_Camera*>::iterator it = self->GetCameras()->begin();
	while (it != self->GetCameras()->end()) {
		clist->Add((*it)->AddRef());
		it++;
	}
	
	return clist->NewProxy(true);
}

PyObject* KX_Scene::pyattr_get_active_camera(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self= static_cast<KX_Scene*>(self_v);
	return self->GetActiveCamera()->GetProxy();
}


int KX_Scene::pyattr_set_active_camera(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_Scene* self= static_cast<KX_Scene*>(self_v);
	KX_Camera *camOb;
	
	if (!ConvertPythonToCamera(value, &camOb, false, "scene.active_camera = value: KX_Scene"))
		return PY_SET_ATTR_FAIL;
	
	self->SetActiveCamera(camOb);
	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_Scene::pyattr_get_drawing_callback_pre(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);

	if(self->m_draw_call_pre==NULL)
		self->m_draw_call_pre= PyList_New(0);
	else
		Py_INCREF(self->m_draw_call_pre);
	return self->m_draw_call_pre;
}

PyObject* KX_Scene::pyattr_get_drawing_callback_post(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);

	if(self->m_draw_call_post==NULL)
		self->m_draw_call_post= PyList_New(0);
	else
		Py_INCREF(self->m_draw_call_post);
	return self->m_draw_call_post;
}

int KX_Scene::pyattr_set_drawing_callback_pre(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);

	if (!PyList_CheckExact(value))
	{
		PyErr_SetString(PyExc_ValueError, "Expected a list");
		return PY_SET_ATTR_FAIL;
	}
	Py_XDECREF(self->m_draw_call_pre);

	Py_INCREF(value);
	self->m_draw_call_pre = value;

	return PY_SET_ATTR_SUCCESS;
}

int KX_Scene::pyattr_set_drawing_callback_post(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);

	if (!PyList_CheckExact(value))
	{
		PyErr_SetString(PyExc_ValueError, "Expected a list");
		return PY_SET_ATTR_FAIL;
	}
	Py_XDECREF(self->m_draw_call_post);

	Py_INCREF(value);
	self->m_draw_call_post = value;

	return PY_SET_ATTR_SUCCESS;
}

PyAttributeDef KX_Scene::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("name",				KX_Scene, pyattr_get_name),
	KX_PYATTRIBUTE_RO_FUNCTION("objects",			KX_Scene, pyattr_get_objects),
	KX_PYATTRIBUTE_RO_FUNCTION("objectsInactive",	KX_Scene, pyattr_get_objects_inactive),
	KX_PYATTRIBUTE_RO_FUNCTION("lights",			KX_Scene, pyattr_get_lights),
	KX_PYATTRIBUTE_RO_FUNCTION("cameras",			KX_Scene, pyattr_get_cameras),
	KX_PYATTRIBUTE_RO_FUNCTION("lights",			KX_Scene, pyattr_get_lights),
	KX_PYATTRIBUTE_RW_FUNCTION("active_camera",		KX_Scene, pyattr_get_active_camera, pyattr_set_active_camera),
	KX_PYATTRIBUTE_RW_FUNCTION("pre_draw",			KX_Scene, pyattr_get_drawing_callback_pre, pyattr_set_drawing_callback_pre),
	KX_PYATTRIBUTE_RW_FUNCTION("post_draw",			KX_Scene, pyattr_get_drawing_callback_post, pyattr_set_drawing_callback_post),
	KX_PYATTRIBUTE_BOOL_RO("suspended",				KX_Scene, m_suspend),
	KX_PYATTRIBUTE_BOOL_RO("activity_culling",		KX_Scene, m_activity_culling),
	KX_PYATTRIBUTE_FLOAT_RW("activity_culling_radius", 0.5f, FLT_MAX, KX_Scene, m_activity_box_radius),
	KX_PYATTRIBUTE_BOOL_RO("dbvt_culling",			KX_Scene, m_dbvt_culling),
	{ NULL }	//Sentinel
};

KX_PYMETHODDEF_DOC(KX_Scene, addObject,
"addObject(object, other, time=0)\n"
"Returns the added object.\n")
{
	PyObject *pyob, *pyother;
	KX_GameObject *ob, *other;

	int time = 0;

	if (!PyArg_ParseTuple(args, "OO|i:addObject", &pyob, &pyother, &time))
		return NULL;

	if (	!ConvertPythonToGameObject(pyob, &ob, false, "scene.addObject(object, other, time): KX_Scene (first argument)") ||
			!ConvertPythonToGameObject(pyother, &other, false, "scene.addObject(object, other, time): KX_Scene (second argument)") )
		return NULL;


	SCA_IObject* replica = AddReplicaObject((SCA_IObject*)ob, other, time);
	
	// release here because AddReplicaObject AddRef's
	// the object is added to the scene so we dont want python to own a reference
	replica->Release();
	return replica->GetProxy();
}

KX_PYMETHODDEF_DOC(KX_Scene, end,
"end()\n"
"Removes this scene from the game.\n")
{
	
	KX_GetActiveEngine()->RemoveScene(m_sceneName);
	
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC(KX_Scene, restart,
				   "restart()\n"
				   "Restarts this scene.\n")
{
	KX_GetActiveEngine()->ReplaceScene(m_sceneName, m_sceneName);
	
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC(KX_Scene, replace,
				   "replace(newScene)\n"
				   "Replaces this scene with another one.\n")
{
	char* name;
	
	if (!PyArg_ParseTuple(args, "s:replace", &name))
		return NULL;
	
	KX_GetActiveEngine()->ReplaceScene(m_sceneName, name);
	
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC(KX_Scene, suspend,
					"suspend()\n"
					"Suspends this scene.\n")
{
	Suspend();
	
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC(KX_Scene, resume,
					"resume()\n"
					"Resumes this scene.\n")
{
	Resume();
	
	Py_RETURN_NONE;
}

/* Matches python dict.get(key, [default]) */
KX_PYMETHODDEF_DOC(KX_Scene, get, "")
{
	PyObject *key;
	PyObject* def = Py_None;
	PyObject* ret;

	if (!PyArg_ParseTuple(args, "O|O:get", &key, &def))
		return NULL;
	
	if (m_attr_dict && (ret=PyDict_GetItem(m_attr_dict, key))) {
		Py_INCREF(ret);
		return ret;
	}
	
	Py_INCREF(def);
	return def;
}

#endif // DISABLE_PYTHON
