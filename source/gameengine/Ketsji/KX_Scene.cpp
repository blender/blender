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
 * Ketsji scene. Holds references to all scene data.
 */

#ifdef WIN32
#pragma warning (disable : 4786)
#endif //WIN32


#include "KX_Scene.h"
#include "MT_assert.h"

#include "KX_KetsjiEngine.h"
#include "KX_BlenderMaterial.h"
#include "RAS_IPolygonMaterial.h"
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
#include "SCA_ActuatorEventManager.h"
#include "KX_Camera.h"
#include "SCA_JoystickManager.h"

#include "RAS_MeshObject.h"
#include "BL_SkinMeshObject.h"

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
#include "BKE_anim.h"

#include "KX_SG_NodeRelationships.h"

#include "KX_NetworkEventManager.h"
#include "NG_NetworkScene.h"
#include "PHY_IPhysicsEnvironment.h"
#include "KX_IPhysicsController.h"
#include "KX_BlenderSceneConverter.h"

#include "BL_ShapeDeformer.h"
#include "BL_DeformableGameObject.h"

// to get USE_BULLET!
#include "KX_ConvertPhysicsObject.h"

#ifdef USE_BULLET
#include "CcdPhysicsEnvironment.h"
#include "CcdPhysicsController.h"
#endif

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

SG_Callbacks KX_Scene::m_callbacks = SG_Callbacks(KX_SceneReplicationFunc,KX_SceneDestructionFunc,KX_GameObject::UpdateTransformFunc);

// temporarily var until there is a button in the userinterface
// (defined in KX_PythonInit.cpp)
extern bool gUseVisibilityTemp;

KX_Scene::KX_Scene(class SCA_IInputDevice* keyboarddevice,
				   class SCA_IInputDevice* mousedevice,
				   class NG_NetworkDeviceInterface *ndi,
				   class SND_IAudioDevice* adi,
				   const STR_String& sceneName): 
	PyObjectPlus(&KX_Scene::Type),
	m_keyboardmgr(NULL),
	m_mousemgr(NULL),
	m_sceneConverter(NULL),
	m_physicsEnvironment(0),
	m_sceneName(sceneName),
	m_adi(adi),
	m_networkDeviceInterface(ndi),
	m_active_camera(NULL),
	m_ueberExecutionPriority(0)
{
	m_suspendedtime = 0.0;
	m_suspendeddelta = 0.0;

	m_activity_culling = false;
	m_suspend = false;
	m_isclearingZbuffer = true;
	m_tempObjectList = new CListValue();
	m_objectlist = new CListValue();
	m_parentlist = new CListValue();
	m_lightlist= new CListValue();
	m_inactivelist = new CListValue();
	m_euthanasyobjects = new CListValue();
	m_delayReleaseObjects = new CListValue();

	m_logicmgr = new SCA_LogicManager();
	
	m_timemgr = new SCA_TimeEventManager(m_logicmgr);
	m_keyboardmgr = new SCA_KeyboardManager(m_logicmgr,keyboarddevice);
	m_mousemgr = new SCA_MouseManager(m_logicmgr,mousedevice);
	
	SCA_AlwaysEventManager* alwaysmgr = new SCA_AlwaysEventManager(m_logicmgr);
	SCA_PropertyEventManager* propmgr = new SCA_PropertyEventManager(m_logicmgr);
	SCA_ActuatorEventManager* actmgr = new SCA_ActuatorEventManager(m_logicmgr);
	SCA_RandomEventManager* rndmgr = new SCA_RandomEventManager(m_logicmgr);
	KX_RayEventManager* raymgr = new KX_RayEventManager(m_logicmgr);

	KX_NetworkEventManager* netmgr = new KX_NetworkEventManager(m_logicmgr, ndi);
	
	SCA_JoystickManager *joymgr	= new SCA_JoystickManager(m_logicmgr);

	m_logicmgr->RegisterEventManager(alwaysmgr);
	m_logicmgr->RegisterEventManager(propmgr);
	m_logicmgr->RegisterEventManager(actmgr);
	m_logicmgr->RegisterEventManager(m_keyboardmgr);
	m_logicmgr->RegisterEventManager(m_mousemgr);
	m_logicmgr->RegisterEventManager(m_timemgr);
	m_logicmgr->RegisterEventManager(rndmgr);
	m_logicmgr->RegisterEventManager(raymgr);
	m_logicmgr->RegisterEventManager(netmgr);
	m_logicmgr->RegisterEventManager(joymgr);

	m_soundScene = new SND_Scene(adi);
	MT_assert (m_networkDeviceInterface != NULL);
	m_networkScene = new NG_NetworkScene(m_networkDeviceInterface);
	
	m_rootnode = NULL;

	m_bucketmanager=new RAS_BucketManager();

	m_canvasDesignWidth = 0;
	m_canvasDesignHeight = 0;
	
	m_attrlist = PyDict_New(); /* new ref */
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
	if (m_delayReleaseObjects)
		m_delayReleaseObjects->Release();

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
#ifdef USE_BULLET
	// This is a fix for memory leaks in bullet: the collision shapes is not destroyed 
	// when the physical controllers are destroyed. The reason is that shapes are shared
	// between replicas of an object. There is no reference count in Bullet so the
	// only workaround that does not involve changes in Bullet is to save in this array
	// the list of shapes that are created when the scene is created (see KX_ConvertPhysicsObjects.cpp)
	class btCollisionShape* shape;
	class btTriangleMeshShape* meshShape;
	vector<class btCollisionShape*>::iterator it = m_shapes.begin();
	while (it != m_shapes.end()) {
		shape = *it;
		if (shape->getShapeType() == TRIANGLE_MESH_SHAPE_PROXYTYPE)
		{
			meshShape = static_cast<btTriangleMeshShape*>(shape);
			// shapes based on meshes use an interface that contains the vertices.
			// Again the idea is to be able to share the interface between shapes but
			// this is not used in Blender: each base object will have its own interface 
			btStridingMeshInterface* meshInterface = meshShape->getMeshInterface();
			if (meshInterface)
				delete meshInterface;
		}
		delete shape;
		it++;
	}
#endif
	//Py_DECREF(m_attrlist);
}

void KX_Scene::AddShape(class btCollisionShape*shape)
{
	m_shapes.push_back(shape);
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
	if (NewRemoveObject(orgobj) != 0)
	{
		// object is not yet deleted (this can happen when it hangs in an add object actuator
		// last object created reference. It's a bad situation, don't know how to fix it exactly
		// The least I can do, is remove the reference to the node in the object as the node
		// will in any case be deleted. This ensures that the object will not try to use the node
		// when it is finally deleted (see KX_GameObject destructor)
		orgobj->SetSGNode(NULL);
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
// It is VERY important that the order of sensors and actuators in
// the replicated object is preserved: it is is used to reconnect the logic.
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
		cont->UnlinkAllSensors();
		cont->UnlinkAllActuators();
		
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

	if (!groupobj->IsDupliGroup() ||
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
		if (gameobj->GetParent() != NULL)
		{
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

		MT_Matrix3x3 newori = groupobj->NodeGetWorldOrientation() * gameobj->NodeGetWorldOrientation();
		replica->NodeSetLocalOrientation(newori);

		MT_Point3 newpos = groupobj->NodeGetWorldPosition() + 
			newscale*(groupobj->NodeGetWorldOrientation() * gameobj->NodeGetWorldPosition());
		replica->NodeSetLocalPosition(newpos);

		if (replica->GetPhysicsController())
		{
			// not required, already done in NodeSetLocalOrientation..
			//replica->GetPhysicsController()->setPosition(newpos);
			//replica->GetPhysicsController()->setOrientation(newori.getRotation());
			// Scaling has been set relatively hereabove, this does not 
			// set the scaling of the controller. I don't know why it's just the
			// relative scale and not the full scale that has to be put here...
			replica->GetPhysicsController()->setScaling(newscale);
		}

		replica->GetSGNode()->UpdateWorldData(0);
		replica->GetSGNode()->SetBBox(gameobj->GetSGNode()->BBox());
		replica->GetSGNode()->SetRadius(gameobj->GetSGNode()->Radius());
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
	
	// get the rootnode's scale
	MT_Vector3 newscale = parentobj->GetSGNode()->GetRootSGParent()->GetLocalScale();

	// set the replica's relative scale with the rootnode's scale
	replica->NodeSetRelativeScale(newscale);

	if (replica->GetPhysicsController())
	{
		// not needed, already done in NodeSetLocalPosition()
		//replica->GetPhysicsController()->setPosition(newpos);
		//replica->GetPhysicsController()->setOrientation(newori.getRotation());
		replica->GetPhysicsController()->setScaling(newscale);
	}

	// here we want to set the relative scale: the rootnode's scale will override all other
	// scalings, so lets better prepare for it


	replica->GetSGNode()->UpdateWorldData(0);
	replica->GetSGNode()->SetBBox(originalobj->GetSGNode()->BBox());
	replica->GetSGNode()->SetRadius(originalobj->GetSGNode()->Radius());
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

	// first disconnect child from parent
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

void KX_Scene::DelayedReleaseObject(CValue* gameobj)
{
	m_delayReleaseObjects->Add(gameobj->AddRef());
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
		m_logicmgr->RemoveDestroyedActuator(*ita);
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

	if (m_sceneConverter)
		m_sceneConverter->UnregisterGameObject(newobj);
	// return value will be 0 if the object is actually deleted (all reference gone)
	return ret;
}



void KX_Scene::ReplaceMesh(class CValue* obj,void* meshobj)
{
	KX_GameObject* gameobj = static_cast<KX_GameObject*>(obj);
	RAS_MeshObject* mesh = static_cast<RAS_MeshObject*>(meshobj);

	if(!gameobj || !mesh)
	{
		std::cout << "warning: invalid object, mesh will not be replaced" << std::endl;
		return;
	}

	gameobj->RemoveMeshes();
	gameobj->AddMesh(mesh);
	
	if (gameobj->m_isDeformable)
	{
		BL_DeformableGameObject* newobj = static_cast<BL_DeformableGameObject*>( gameobj );
		
		if (newobj->m_pDeformer)
		{
			delete newobj->m_pDeformer;
			newobj->m_pDeformer = NULL;
		}

		if (mesh->m_class == 1) 
		{
			// we must create a new deformer but which one?
			KX_GameObject* parentobj = newobj->GetParent();
			// this always return the original game object (also for replicate)
			Object* blendobj = newobj->GetBlenderObject();
			// object that owns the new mesh
			Object* oldblendobj = static_cast<struct Object*>(m_logicmgr->FindBlendObjByGameMeshName(mesh->GetName()));
			Mesh* blendmesh = mesh->GetMesh();

			bool bHasShapeKey = blendmesh->key != NULL && blendmesh->key->type==KEY_RELATIVE;
			bool bHasDvert = blendmesh->dvert != NULL;
			bool bHasArmature = 
				parentobj &&								// current parent is armature
				parentobj->GetGameObjectType() == SCA_IObject::OBJ_ARMATURE &&
				oldblendobj &&								// needed for mesh deform
				blendobj->parent &&							// original object had armature (not sure this test is needed)
				blendobj->parent->type == OB_ARMATURE && 
				blendobj->partype==PARSKEL && 
				blendmesh->dvert!=NULL;						// mesh has vertex group
			bool releaseParent = true;

			if (bHasShapeKey)
			{
				BL_ShapeDeformer* shapeDeformer;
				if (bHasArmature) 
				{
					shapeDeformer = new BL_ShapeDeformer(
						newobj,
						oldblendobj, blendobj,
						static_cast<BL_SkinMeshObject*>(mesh),
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
						static_cast<BL_SkinMeshObject*>(mesh),
						false,
						NULL
					);
				}
				newobj->m_pDeformer = shapeDeformer;
			}
			else if (bHasArmature) 
			{
				BL_SkinDeformer* skinDeformer = new BL_SkinDeformer(
					newobj,
					oldblendobj, blendobj,
					static_cast<BL_SkinMeshObject*>(mesh),
					true,
					static_cast<BL_ArmatureObject*>( parentobj )
				);
				releaseParent= false;
				newobj->m_pDeformer = skinDeformer;
			}
			else if (bHasDvert)
			{
				BL_MeshDeformer* meshdeformer = new BL_MeshDeformer(
					newobj, oldblendobj, static_cast<BL_SkinMeshObject*>(mesh)
				);
				newobj->m_pDeformer = meshdeformer;
			}

			// release parent reference if its not being used 
			if( releaseParent && parentobj)
				parentobj->Release();
		}
	}
	gameobj->Bucketize();
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
//		gameobj->UpdateNonDynas();
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
			gameobj->MarkVisible(visible);
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
	if (!gameobj->GetVisible())
		return;
	
	// Shadow lamp layers
	if(layer && !(gameobj->GetLayer() & layer)) {
		gameobj->MarkVisible(false);
		return;
	}

	// If Frustum culling is off, the object is always visible.
	bool vis = !cam->GetFrustumCulling();
	
	// If the camera is inside this node, then the object is visible.
	if (!vis)
	{
		vis = gameobj->GetSGNode()->inside( GetActiveCamera()->GetCameraLocation() );
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
		gameobj->MarkVisible();
	} else {
		gameobj->MarkVisible(false);
	}
}

void KX_Scene::CalculateVisibleMeshes(RAS_IRasterizer* rasty,KX_Camera* cam, int layer)
{
// FIXME: When tree is operational
#if 1
	// do this incrementally in the future
	for (int i = 0; i < m_objectlist->GetCount(); i++)
	{
		MarkVisible(rasty, static_cast<KX_GameObject*>(m_objectlist->GetValue(i)), cam, layer);
	}
#else
	if (cam->GetFrustumCulling())
		MarkVisible(m_objecttree, rasty, cam, layer);
	else
		MarkSubTreeVisible(m_objecttree, rasty, true, cam, layer);
#endif
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
	int i;
	for (i = numobj - 1; i >= 0; i--)
	{
		KX_GameObject* gameobj = (KX_GameObject*)m_euthanasyobjects->GetValue(i);
		// KX_Scene::RemoveObject will also remove the object from this list
		// that's why we start from the end
		this->RemoveObject(gameobj);
	}

	numobj=	m_delayReleaseObjects->GetCount();
	for (i = numobj-1;i>=0;i--)
	{
		KX_GameObject* gameobj = (KX_GameObject*)m_delayReleaseObjects->GetValue(i);
		// This list is not for object removal, but just object release
		gameobj->Release();
	}
	// empty the list as we have removed all references
	m_delayReleaseObjects->Resize(0);	
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

void KX_Scene::SetNodeTree(SG_Tree* root)
{
	m_objecttree = root;
}

void KX_Scene::SetSceneConverter(class KX_BlenderSceneConverter* sceneConverter)
{
	m_sceneConverter = sceneConverter;
}

void KX_Scene::SetPhysicsEnvironment(class PHY_IPhysicsEnvironment* physEnv)
{
	m_physicsEnvironment = physEnv;

	KX_TouchEventManager* touchmgr = new KX_TouchEventManager(m_logicmgr, physEnv);
	m_logicmgr->RegisterEventManager(touchmgr);
	return;
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

//----------------------------------------------------------------------------
//Python

PyMethodDef KX_Scene::Methods[] = {
	KX_PYMETHODTABLE(KX_Scene, getLightList),
	KX_PYMETHODTABLE(KX_Scene, getObjectList),
	KX_PYMETHODTABLE(KX_Scene, getName),
	
	{NULL,NULL} //Sentinel
};

PyTypeObject KX_Scene::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
		0,
		"KX_Scene",
		sizeof(KX_Scene),
		0,
		PyDestructor,
		0,
		__getattr,
		__setattr,
		0, //&MyPyCompare,
		__repr,
		0, //&cvalue_as_number,
		0,
		0,
		0,
		0, 0, 0, 0, 0, 0
};

PyParentObject KX_Scene::Parents[] = {
	&KX_Scene::Type,
		&CValue::Type,
		NULL
};

PyObject* KX_Scene::_getattr(const STR_String& attr)
{
	if (attr == "name")
		return PyString_FromString(GetName());
	
	if (attr == "active_camera")
	{
		KX_Camera *camera = GetActiveCamera();
		camera->AddRef();
		return (PyObject*) camera;
	}
	
	if (attr == "suspended")
		return PyInt_FromLong(m_suspend);
	
	if (attr == "activity_culling")
		return PyInt_FromLong(m_activity_culling);
	
	if (attr == "activity_culling_radius")
		return PyFloat_FromDouble(m_activity_box_radius);
	
	PyObject* value = PyDict_GetItemString(m_attrlist, const_cast<char *>(attr.ReadPtr()));
	if (value)
	{
		Py_INCREF(value);
		return value;
	}
	
	_getattr_up(PyObjectPlus);
}

int KX_Scene::_delattr(const STR_String &attr)
{
	PyDict_DelItemString(m_attrlist, const_cast<char *>(attr.ReadPtr()));
	return 0;
}

int KX_Scene::_setattr(const STR_String &attr, PyObject *pyvalue)
{

	if (!PyDict_SetItemString(m_attrlist, const_cast<char *>(attr.ReadPtr()), pyvalue))
		return 0;

	return PyObjectPlus::_setattr(attr, pyvalue);
}

KX_PYMETHODDEF_DOC(KX_Scene, getLightList,
"getLightList() -> list [KX_Light]\n"
"Returns a list of all lights in the scene.\n"
)
{
	m_lightlist->AddRef();
	return (PyObject*) m_lightlist;
}

KX_PYMETHODDEF_DOC(KX_Scene, getObjectList,
"getObjectList() -> list [KX_GameObject]\n"
"Returns a list of all game objects in the scene.\n"
)
{
	m_objectlist->AddRef();
	return (PyObject*) m_objectlist;
}

KX_PYMETHODDEF_DOC(KX_Scene, getName,
"getName() -> string\n"
"Returns the name of the scene.\n"
)
{
	return PyString_FromString(GetName());
}
