/*
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
 * Ketsji scene. Holds references to all scene data.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#pragma warning (disable : 4786)
#endif //WIN32

#include "KX_KetsjiEngine.h"
#include "RAS_IPolygonMaterial.h"
#include "KX_Scene.h"
#include "ListValue.h"
#include "SCA_LogicManager.h"
#include "SCA_TimeEventManager.h"
#include "SCA_AlwaysEventManager.h"
#include "SCA_RandomEventManager.h"
#include "KX_RayEventManager.h"
#include "KX_TouchEventManager.h"
#include "SCA_KeyboardManager.h"
#include "SCA_MouseManager.h"
#include "SCA_PropertyEventManager.h"
#include "KX_Camera.h"

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

#include "KX_SG_NodeRelationships.h"

#include "KX_NetworkEventManager.h"
#include "NG_NetworkScene.h"
#include "PHY_IPhysicsEnvironment.h"
#include "KX_IPhysicsController.h"


void* KX_SceneReplicationFunc(SG_IObject* node,void* gameobj,void* scene)
{
	KX_GameObject* replica = ((KX_Scene*)scene)->AddNodeReplicaObject(node,(KX_GameObject*)gameobj);

	return (void*)replica;
}

void* KX_SceneDestructionFunc(SG_IObject* node,void* gameobj,void* scene)
{
	((KX_Scene*)scene)->RemoveNodeDestructObject(node,(KX_GameObject*)gameobj);

	return NULL;
};


SG_Callbacks KX_Scene::m_callbacks = SG_Callbacks(KX_SceneReplicationFunc,KX_SceneDestructionFunc);

// temporarily var until there is a button in the userinterface
// (defined in KX_PythonInit.cpp)
extern bool gUseVisibilityTemp;




KX_Scene::KX_Scene(class SCA_IInputDevice* keyboarddevice,
				   class SCA_IInputDevice* mousedevice,
				   class NG_NetworkDeviceInterface *ndi,
				   class SND_IAudioDevice* adi,
				   const STR_String& sceneName): 

	m_mousemgr(NULL),
	m_keyboardmgr(NULL),
	m_active_camera(NULL),
	m_ueberExecutionPriority(0),
	m_adi(adi),
	m_sceneName(sceneName),
	m_networkDeviceInterface(ndi),
	m_physicsEnvironment(0)
{
		

	m_activity_culling = false;
	m_suspend = false;
	m_isclearingZbuffer = true;
	m_tempObjectList = new CListValue();
	m_objectlist = new CListValue();
	m_parentlist = new CListValue();
	m_lightlist= new CListValue();
	m_euthanasyobjects = new CListValue();

	m_logicmgr = new SCA_LogicManager();
	
	m_timemgr = new SCA_TimeEventManager(m_logicmgr);
	m_keyboardmgr = new SCA_KeyboardManager(m_logicmgr,keyboarddevice);
	m_mousemgr = new SCA_MouseManager(m_logicmgr,mousedevice);
	
//	m_solidScene = DT_CreateScene();
//	m_respTable = DT_CreateRespTable();

	SCA_AlwaysEventManager* alwaysmgr = new SCA_AlwaysEventManager(m_logicmgr);
	//KX_TouchEventManager* touchmgr = new KX_TouchEventManager(m_logicmgr, m_respTable, m_solidScene);
	SCA_PropertyEventManager* propmgr = new SCA_PropertyEventManager(m_logicmgr);
	SCA_RandomEventManager* rndmgr = new SCA_RandomEventManager(m_logicmgr);
	KX_RayEventManager* raymgr = new KX_RayEventManager(m_logicmgr);

	KX_NetworkEventManager* netmgr = new KX_NetworkEventManager(m_logicmgr, ndi);

	m_logicmgr->RegisterEventManager(alwaysmgr);
	m_logicmgr->RegisterEventManager(propmgr);
	m_logicmgr->RegisterEventManager(m_keyboardmgr);
	m_logicmgr->RegisterEventManager(m_mousemgr);
	//m_logicmgr->RegisterEventManager(touchmgr);
	m_logicmgr->RegisterEventManager(m_timemgr);
	m_logicmgr->RegisterEventManager(rndmgr);
	m_logicmgr->RegisterEventManager(raymgr);
	m_logicmgr->RegisterEventManager(netmgr);

	//m_sumoScene = new SM_Scene();
	//m_sumoScene->setSecondaryRespTable(m_respTable);
	m_soundScene = new SND_Scene(adi);
	assert (m_networkDeviceInterface != NULL);
	m_networkScene = new NG_NetworkScene(m_networkDeviceInterface);
	
	m_rootnode = NULL;

	m_bucketmanager=new RAS_BucketManager();

	m_canvasDesignWidth = 0;
	m_canvasDesignHeight = 0;
}



KX_Scene::~KX_Scene()
{
//	int numobj = m_objectlist->GetCount();

	//int numrootobjects = GetRootParentList()->GetCount();
	for (int i = 0; i < GetRootParentList()->GetCount(); i++)
	{
		KX_GameObject* parentobj = (KX_GameObject*) GetRootParentList()->GetValue(i);
		this->RemoveObject(parentobj);
	}

	if(m_objectlist)
		m_objectlist->Release();
	
	if (m_parentlist)
		m_parentlist->Release();
	
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

	if (m_soundScene)
		delete m_soundScene;

	if (m_networkScene)
		delete m_networkScene;
	
	if (m_bucketmanager)
	{
		delete m_bucketmanager;
	}
}




void KX_Scene::SetProjectionMatrix(MT_CmMatrix4x4& pmat)
{
	m_projectionmat = pmat;
}



RAS_BucketManager* KX_Scene::GetBucketManager()
{
	return m_bucketmanager;
}



CListValue* KX_Scene::GetObjectList()
{
	return m_objectlist;
}



CListValue* KX_Scene::GetRootParentList()
{
	return m_parentlist;
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



SND_Scene* KX_Scene::GetSoundScene()
{
	return m_soundScene;
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
	NewRemoveObject(orgobj);

	if (node)
		delete node;
}

KX_GameObject* KX_Scene::AddNodeReplicaObject(class SG_IObject* node, class CValue* gameobj)
{
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
	SG_Node* rootnode = (replicanode == m_rootnode ? NULL : m_rootnode);

	replicanode->SetSGClientObject(newobj);

	// this is the list of object that are send to the graphics pipeline
	m_objectlist->Add(newobj);
	newobj->Bucketize();

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
	
	return newobj;
}



// before calling this method KX_Scene::ReplicateLogic(), make sure to
// have called 'GameObject::ReParentLogic' for each object this
// hierarchy that's because first ALL bricks must exist in the new
// replica of the hierarchy in order to make cross-links work properly
// !
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
		cont->UnlinkAllSensors();
		cont->UnlinkAllActuators();
		
		// now relink each sensor
		for (vector<SCA_ISensor*>::iterator its = linkedsensors.begin();!(its==linkedsensors.end());its++)
		{
			SCA_ISensor* oldsensor = (*its);
			STR_String name = oldsensor->GetName();
			//find this name in the list
			SCA_ISensor* newsensor = newobj->FindSensor(name);
		
			if (newsensor)
			{
				// relink this newsensor to the controller
				m_logicmgr->RegisterToSensor(cont,newsensor);
			}
			else
			{
				// it can be linked somewhere in the hierarchy or...
				for (vector<KX_GameObject*>::iterator git = m_logicHierarchicalGameObjects.begin();
				!(git==m_logicHierarchicalGameObjects.end());++git)
				{
					newsensor = (*git)->FindSensor(name);
					if (newsensor)
						break;
				} 

				if (newsensor)
				{
					// relink this newsensor to the controller somewhere else within this
					// hierarchy
					m_logicmgr->RegisterToSensor(cont,newsensor);
				}
				else
				{
					// must be an external sensor, so...
					m_logicmgr->RegisterToSensor(cont,oldsensor);
				}
			}
		}
		
		// now relink each actuator
		for (vector<SCA_IActuator*>::iterator ita = linkedactuators.begin();!(ita==linkedactuators.end());ita++)
		{
			SCA_IActuator* oldactuator = (*ita);
			STR_String name = oldactuator->GetName();
			//find this name in the list
			SCA_IActuator* newactuator = newobj->FindActuator(name);
			if (newactuator)
			{
				// relink this newsensor to the controller
				m_logicmgr->RegisterToActuator(cont,newactuator);
				newactuator->SetUeberExecutePriority(m_ueberExecutionPriority);
			}
			else
			{
				// it can be linked somewhere in the hierarchy or...
				for (vector<KX_GameObject*>::iterator git = m_logicHierarchicalGameObjects.begin();
				!(git==m_logicHierarchicalGameObjects.end());++git)
				{
					newactuator= (*git)->FindActuator(name);
					if (newactuator)
						break;
				} 

				if (newactuator)
				{
					// relink this actuator to the controller somewhere else within this
					// hierarchy
					m_logicmgr->RegisterToActuator(cont,newactuator);
					newactuator->SetUeberExecutePriority(m_ueberExecutionPriority);
				}
				else
				{
					// must be an external actuator, so...
					m_logicmgr->RegisterToActuator(cont,oldactuator);
				}
			}
		}
	}
}



SCA_IObject* KX_Scene::AddReplicaObject(class CValue* originalobject,
										class CValue* parentobject,
										int lifespan)
{

	m_logicHierarchicalGameObjects.clear();
	m_map_gameobject_to_replica.clear();

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
		replica->SetProperty("::timebomb",new CFloatValue(lifespan*0.02));
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
		replica->GetSGNode()->AddChild(childreplicanode);
	}

	//	relink any pointers as necessary, sort of a temporary solution
	vector<KX_GameObject*>::iterator git;
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		(*git)->Relink(&m_map_gameobject_to_replica);
	}

	// now replicate logic
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		(*git)->ReParentLogic();
	}
	
	// replicate crosslinks etc. between logic bricks
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		ReplicateLogic((*git));
	}
	
	MT_Point3 newpos = ((KX_GameObject*) parentobject)->NodeGetWorldPosition();
	replica->NodeSetLocalPosition(newpos);

	MT_Matrix3x3 newori = ((KX_GameObject*) parentobject)->NodeGetWorldOrientation();
	replica->NodeSetLocalOrientation(newori);

	if (replica->GetPhysicsController())
	{
		replica->GetPhysicsController()->setPosition(newpos);
		replica->GetPhysicsController()->setOrientation(newori.getRotation());
	}

	// here we want to set the relative scale: the rootnode's scale will override all other
	// scalings, so lets better prepare for it

	// get the rootnode's scale
	MT_Vector3 newscale = parentobj->GetSGNode()->GetRootSGParent()->GetLocalScale();

	// set the replica's relative scale with the rootnode's scale
	replica->NodeSetRelativeScale(newscale);

	replica->GetSGNode()->UpdateWorldData(0);
	
	return replica;
}



void KX_Scene::RemoveObject(class CValue* gameobj)
{
	KX_GameObject* newobj = (KX_GameObject*) gameobj;

	// first disconnect child from parent
	SG_Node* node = newobj->GetSGNode();

	if (node)
	{
		node->DisconnectFromParent();

		// recursively destruct
		node->Destruct();
	}
}



void KX_Scene::DelayedRemoveObject(class CValue* gameobj)
{
	//KX_GameObject* newobj = (KX_GameObject*) gameobj;
	if (!m_euthanasyobjects->SearchValue(gameobj))
	{
		m_euthanasyobjects->Add(gameobj->AddRef());
	} 
}



void KX_Scene::NewRemoveObject(class CValue* gameobj)
{
	KX_GameObject* newobj = (KX_GameObject*) gameobj;
	//SM_Object* sumoObj = newobj->GetSumoObject();
	//if (sumoObj)
	//{
	//	this->GetSumoScene()->remove(*sumoObj);
	//}
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
		(*itc)->UnlinkAllSensors();
		(*itc)->UnlinkAllActuators();
	}

	SCA_ActuatorList& actuators = newobj->GetActuators();
	for (SCA_ActuatorList::iterator ita = actuators.begin();
		 !(ita==actuators.end());ita++)
	{
		m_logicmgr->RemoveDestroyedActuator(*ita);
	}

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
	if (m_objectlist->RemoveValue(newobj))
		newobj->Release();
	if (m_tempObjectList->RemoveValue(newobj))
		newobj->Release();
	if (m_parentlist->RemoveValue(newobj))
		newobj->Release();
	if (m_euthanasyobjects->RemoveValue(newobj))
		newobj->Release();
}



void KX_Scene::ReplaceMesh(class CValue* gameobj,void* meshobj)
{
	KX_GameObject* newobj = (KX_GameObject*) gameobj;
	newobj->RemoveMeshes();
	newobj->AddMesh((RAS_MeshObject*)meshobj);
	newobj->Bucketize();
}



MT_CmMatrix4x4& KX_Scene::GetViewMatrix()
{
	MT_Scalar cammat[16];
	m_active_camera->GetWorldToCamera().getValue(cammat);
	m_viewmat = cammat;
	return m_viewmat;
}



MT_CmMatrix4x4& KX_Scene::GetProjectionMatrix()
{
	return m_projectionmat;
}


KX_Camera* KX_Scene::FindCamera(KX_Camera* cam)
{
	set<KX_Camera*>::iterator it = m_cameras.begin();

	while ( (it != m_cameras.end()) 
			&& ((*it) != cam) ) {
	  it++;
	}

	return ((it == m_cameras.end()) ? NULL : (*it));
}


KX_Camera* KX_Scene::FindCamera(STR_String& name)
{
	set<KX_Camera*>::iterator it = m_cameras.begin();

	while ( (it != m_cameras.end()) 
			&& ((*it)->GetName() != name) ) {
	  it++;
	}

	return ((it == m_cameras.end()) ? NULL : (*it));
}

void KX_Scene::AddCamera(KX_Camera* cam)
{
	m_cameras.insert(cam);
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



void KX_Scene::UpdateMeshTransformations()
{
	// do this incrementally in the future
	for (int i = 0; i < m_objectlist->GetCount(); i++)
	{
		KX_GameObject* gameobj = (KX_GameObject*)m_objectlist->GetValue(i);
		gameobj->GetOpenGLMatrix();
		gameobj->UpdateNonDynas();
	}
}

void KX_Scene::CalculateVisibleMeshes(RAS_IRasterizer* rasty)
{
	
	// do this incrementally in the future
	for (int i = 0; i < m_objectlist->GetCount(); i++)
	{
		KX_GameObject* gameobj = (KX_GameObject*)m_objectlist->GetValue(i);
		
		int nummeshes = gameobj->GetMeshCount();
		
		for (int m=0;m<nummeshes;m++)
		{
			// this adds the vertices to the display list
			(gameobj->GetMesh(m))->SchedulePolygons(rasty->GetDrawingMode(),rasty);
			// Visibility/ non-visibility are marked
			// elsewhere now.
			gameobj->MarkVisible();
		}
	}
}

// logic stuff
void KX_Scene::LogicBeginFrame(double curtime,double deltatime)
{
	// have a look at temp objects ...
	int lastobj = m_tempObjectList->GetCount() - 1;
	
	for (int i = lastobj; i >= 0; i--)
	{
		CValue* objval = m_tempObjectList->GetValue(i);
		CFloatValue* propval = (CFloatValue*) objval->GetProperty("::timebomb");
		
		if (propval)
		{
			float timeleft = propval->GetNumber() - deltatime;
			
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
	m_logicmgr->BeginFrame(curtime,deltatime);
}



void KX_Scene::LogicUpdateFrame(double curtime,double deltatime)
{
	m_logicmgr->UpdateFrame(curtime,deltatime);
}



void KX_Scene::LogicEndFrame()
{
	m_logicmgr->EndFrame();
	int numobj = m_euthanasyobjects->GetCount();

	for (int i = numobj - 1; i >= 0; i--)
	{
		KX_GameObject* gameobj = (KX_GameObject*)m_euthanasyobjects->GetValue(i);
		this->RemoveObject(gameobj);
	}
	
	numobj = m_euthanasyobjects->GetCount();
	if (numobj != 0)
	{
		// huh?
		int ii=0;
	}
	// numobj is 0 we hope
}



/**
  * UpdateParents: SceneGraph transformation update.
  */
void KX_Scene::UpdateParents(double curtime)
{
//	int numrootobjects = GetRootParentList()->GetCount();

	for (int i=0; i<GetRootParentList()->GetCount(); i++)
	{
		KX_GameObject* parentobj = (KX_GameObject*)GetRootParentList()->GetValue(i);
		parentobj->NodeUpdateGS(curtime,true);
	}
}



RAS_MaterialBucket* KX_Scene::FindBucket(class RAS_IPolyMaterial* polymat)
{
	return m_bucketmanager->RAS_BucketManagerFindBucket(polymat);
}



void KX_Scene::RenderBuckets(const MT_Transform & cameratransform,
							 class RAS_IRasterizer* rasty,
							 class RAS_IRenderTools* rendertools)
{
	m_bucketmanager->Renderbuckets(cameratransform,rasty,rendertools);
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
