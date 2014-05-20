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

/** \file KX_Scene.h
 *  \ingroup ketsji
 */

#ifndef __KX_SCENE_H__
#define __KX_SCENE_H__


#include "KX_PhysicsEngineEnums.h"

#include <vector>
#include <set>
#include <list>

#include "CTR_Map.h"
#include "CTR_HashedPtr.h"
#include "SG_IObject.h"
#include "SCA_IScene.h"
#include "MT_Transform.h"

#include "RAS_FramingManager.h"
#include "RAS_Rect.h"


#include "PyObjectPlus.h"
#include "RAS_2DFilterManager.h"

/**
 * \section Forward declarations
 */
struct SM_MaterialProps;
struct SM_ShapeProps;
struct Scene;

class CTR_HashedPtr;
class CListValue;
class CValue;
class SCA_LogicManager;
class SCA_KeyboardManager;
class SCA_TimeEventManager;
class SCA_MouseManager;
class SCA_ISystem;
class SCA_IInputDevice;
class NG_NetworkDeviceInterface;
class NG_NetworkScene;
class SG_IObject;
class SG_Node;
class SG_Tree;
class KX_WorldInfo;
class KX_Camera;
class KX_GameObject;
class KX_LightObject;
class RAS_BucketManager;
class RAS_MaterialBucket;
class RAS_IPolyMaterial;
class RAS_IRasterizer;
class RAS_IRenderTools;
class SCA_JoystickManager;
class btCollisionShape;
class KX_BlenderSceneConverter;
struct KX_ClientObjectInfo;
class KX_ObstacleSimulation;

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

/* for ID freeing */
#define IS_TAGGED(_id) ((_id) && (((ID *)_id)->flag & LIB_DOIT))

/**
 * The KX_Scene holds all data for an independent scene. It relates
 * KX_Objects to the specific objects in the modules.
 * */
class KX_Scene : public PyObjectPlus, public SCA_IScene
{
	Py_Header

#ifdef WITH_PYTHON
	PyObject*	m_attr_dict;
	PyObject*	m_draw_call_pre;
	PyObject*	m_draw_call_post;
#endif

	struct CullingInfo {
		int m_layer;
		CullingInfo(int layer) : m_layer(layer) {}
	};

protected:
	RAS_BucketManager*	m_bucketmanager;
	CListValue*			m_tempObjectList;

	/**
	 * The list of objects which have been removed during the
	 * course of one frame. They are actually destroyed in 
	 * LogicEndFrame() via a call to RemoveObject().
	 */
	CListValue*	m_euthanasyobjects;

	CListValue*			m_objectlist;
	CListValue*			m_parentlist; // all 'root' parents
	CListValue*			m_lightlist;
	CListValue*			m_inactivelist;	// all objects that are not in the active layer
	CListValue*			m_animatedlist; // all animated objects
	
	SG_QList			m_sghead;		// list of nodes that needs scenegraph update
										// the Dlist is not object that must be updated
										// the Qlist is for objects that needs to be rescheduled
										// for updates after udpate is over (slow parent, bone parent)


	/**
	 * The set of cameras for this scene
	 */
	std::list<class KX_Camera*>       m_cameras;

	/**
	 * The set of fonts for this scene
	 */
	std::list<class KX_FontObject*>   m_fonts;


	/**
	 * Various SCA managers used by the scene
	 */
	SCA_LogicManager*		m_logicmgr;
	SCA_KeyboardManager*	m_keyboardmgr;
	SCA_MouseManager*		m_mousemgr;
	SCA_TimeEventManager*	m_timemgr;

	// Scene converter where many scene entities are registered
	// Used to deregister objects that are deleted
	class KX_BlenderSceneConverter*		m_sceneConverter;
	/**
	 * physics engine abstraction
	 */
	//e_PhysicsEngine m_physicsEngine; //who needs this ?
	class PHY_IPhysicsEnvironment*		m_physicsEnvironment;

	/**
	 * Does this scene clear the z-buffer?
	 */
	bool m_isclearingZbuffer;

	/**
	 * The name of the scene
	 */
	STR_String	m_sceneName;
	
	/**
	 * stores the world-settings for a scene
	 */
	KX_WorldInfo* m_worldinfo;

	/**
	 * \section Different scenes, linked to ketsji scene
	 */

	/**
	 * Network scene.
	 */
	NG_NetworkDeviceInterface*	m_networkDeviceInterface;
	NG_NetworkScene* m_networkScene;

	/**
	 * A temporary variable used to parent objects together on
	 * replication. Don't get confused by the name it is not
	 * the scene's root node!
	 */
	SG_Node* m_rootnode;

	/**
	 * The active camera for the scene
	 */
	KX_Camera* m_active_camera;

	/**
	 * Another temporary variable outstaying its welcome
	 * used in AddReplicaObject to map game objects to their
	 * replicas so pointers can be updated.
	 */
	CTR_Map	<CTR_HashedPtr, void*> m_map_gameobject_to_replica;

	/**
	 * Another temporary variable outstaying its welcome
	 * used in AddReplicaObject to keep a record of all added 
	 * objects. Logic can only be updated when all objects 
	 * have been updated. This stores a list of the new objects.
	 */
	std::vector<KX_GameObject*>	m_logicHierarchicalGameObjects;
	
	/**
	 * This temporary variable will contain the list of 
	 * object that can be added during group instantiation.
	 * objects outside this list will not be added (can 
	 * happen with children that are outside the group).
	 * Used in AddReplicaObject. If the list is empty, it
	 * means don't care.
	 */
	std::set<CValue*>	m_groupGameObjects;
	
	/** 
	 * Pointer to system variable passed in in constructor
	 * only used in constructor so we do not need to keep it
	 * around in this class.
	 */

	SCA_ISystem* m_kxsystem;

	/**
	 * The execution priority of replicated object actuators?
	 */
	int	m_ueberExecutionPriority;

	/**
	 * Activity 'bubble' settings :
	 * Suspend (freeze) the entire scene.
	 */
	bool m_suspend;

	/**
	 * Radius in Manhattan distance of the box for activity culling.
	 */
	float m_activity_box_radius;

	/**
	 * Toggle to enable or disable activity culling.
	 */
	bool m_activity_culling;
	
	/**
	 * Toggle to enable or disable culling via DBVT broadphase of Bullet.
	 */
	bool m_dbvt_culling;
	
	/**
	 * Occlusion culling resolution
	 */ 
	int m_dbvt_occlusion_res;

	/**
	 * The framing settings used by this scene
	 */

	RAS_FrameSettings m_frame_settings;

	/** 
	 * This scenes viewport into the game engine
	 * canvas.Maintained externally, initially [0,0] -> [0,0]
	 */
	RAS_Rect m_viewport;
	
	/**
	 * Visibility testing functions.
	 */
	void MarkVisible(SG_Tree *node, RAS_IRasterizer* rasty, KX_Camera*cam,int layer=0);
	void MarkSubTreeVisible(SG_Tree *node, RAS_IRasterizer* rasty, bool visible, KX_Camera*cam,int layer=0);
	void MarkVisible(RAS_IRasterizer* rasty, KX_GameObject* gameobj, KX_Camera*cam, int layer=0);
	static void PhysicsCullingCallback(KX_ClientObjectInfo* objectInfo, void* cullingInfo);

	double				m_suspendedtime;
	double				m_suspendeddelta;

	struct Scene* m_blenderScene;

	RAS_2DFilterManager m_filtermanager;

	KX_ObstacleSimulation* m_obstacleSimulation;

public:
	KX_Scene(class SCA_IInputDevice* keyboarddevice,
		class SCA_IInputDevice* mousedevice,
		class NG_NetworkDeviceInterface* ndi,
		const STR_String& scenename,
		struct Scene* scene,
		class RAS_ICanvas* canvas);

	virtual
	~KX_Scene();

	RAS_BucketManager* GetBucketManager();
	RAS_MaterialBucket*	FindBucket(RAS_IPolyMaterial* polymat, bool &bucketCreated);
	void RenderBuckets(const MT_Transform& cameratransform,
	                   RAS_IRasterizer* rasty);

	/**
	 * Update all transforms according to the scenegraph.
	 */
	static bool KX_ScenegraphUpdateFunc(SG_IObject* node,void* gameobj,void* scene);
	static bool KX_ScenegraphRescheduleFunc(SG_IObject* node,void* gameobj,void* scene);
	void UpdateParents(double curtime);
	void DupliGroupRecurse(CValue* gameobj, int level);
	bool IsObjectInGroup(CValue* gameobj)
	{ 
		return (m_groupGameObjects.empty() || 
				m_groupGameObjects.find(gameobj) != m_groupGameObjects.end());
	}
	void AddObjectDebugProperties(class KX_GameObject* gameobj);
	SCA_IObject* AddReplicaObject(CValue* gameobj,
	                              CValue* locationobj,
	                              int lifespan=0);
	KX_GameObject* AddNodeReplicaObject(SG_IObject* node,
	                                    CValue* gameobj);
	void RemoveNodeDestructObject(SG_IObject* node,
	                              CValue* gameobj);
	void RemoveObject(CValue* gameobj);
	void DelayedRemoveObject(CValue* gameobj);
	
	int NewRemoveObject(CValue* gameobj);
	void ReplaceMesh(CValue* gameobj,
	                 void* meshob, bool use_gfx, bool use_phys);

	void AddAnimatedObject(CValue* gameobj);

	/**
	 * \section Logic stuff
	 * Initiate an update of the logic system.
	 */
	void LogicBeginFrame(double curtime);
	void LogicUpdateFrame(double curtime, bool frame);
	void UpdateAnimations(double curtime);

		void
	LogicEndFrame(
	);

		CListValue*
	GetTempObjectList(
	);

		CListValue*
	GetObjectList(
	);

		CListValue*
	GetInactiveList(
	);

		CListValue*
	GetRootParentList(
	);

		CListValue*
	GetLightList(
	);

		SCA_LogicManager *
	GetLogicManager(
	);

		SCA_TimeEventManager *
	GetTimeEventManager(
	);

	/** Font Routines */

	/** Find a font in the scene by pointer. */
		KX_FontObject*              
	FindFont(
		KX_FontObject*
	);

	/** Add a camera to this scene. */
		void                    
	AddFont(
		KX_FontObject*
	);

	/** Render the fonts in this scene. */
		void
	RenderFonts(
	);

	/** Camera Routines */

		std::list<class KX_Camera*>*
	GetCameras(
	);
 

	/** Find a camera in the scene by pointer. */
		KX_Camera*              
	FindCamera(
		KX_Camera*
	);

	/** Find a scene in the scene by name. */
		KX_Camera*              
	FindCamera(
		STR_String&
	);

	/** Add a camera to this scene. */
		void                    
	AddCamera(
		KX_Camera*
	);

	/** Find the currently active camera. */
		KX_Camera*
	GetActiveCamera(
	);

	/** 
	 * Set this camera to be the active camera in the scene. If the
	 * camera is not present in the camera list, it will be added
	 */

		void
	SetActiveCamera(
		class KX_Camera*
	);

	/**
	 * Move this camera to the end of the list so that it is rendered last.
	 * If the camera is not on the list, it will be added
	 */
		void
	SetCameraOnTop(
		class KX_Camera*
	);

	/**
	 * Activates new desired canvas width set at design time.
	 * \param width	The new desired width.
	 */
		void
	SetCanvasDesignWidth(
		unsigned int width
	);
	/**
	 * Activates new desired canvas height set at design time.
	 * \param width	The new desired height.
	 */
		void
	SetCanvasDesignHeight(
		unsigned int height
	);
	/**
	 * Returns the current desired canvas width set at design time.
	 * \return The desired width.
	 */
		unsigned int
	GetCanvasDesignWidth(
		void
	) const;

	/**
	 * Returns the current desired canvas height set at design time.
	 * \return The desired height.
	 */
		unsigned int
	GetCanvasDesignHeight(
		void
	) const;

	/**
	 * Set the framing options for this scene
	 */

		void
	SetFramingType(
		RAS_FrameSettings & frame_settings
	);

	/**
	 * Return a const reference to the framing 
	 * type set by the above call.
	 * The contents are not guaranteed to be sensible
	 * if you don't call the above function.
	 */

	const
		RAS_FrameSettings &
	GetFramingType(
	) const;

	/**
	 * Store the current scene's viewport on the 
	 * game engine canvas.
	 */
	void SetSceneViewport(const RAS_Rect &viewport);

	/**
	 * Get the current scene's viewport on the
	 * game engine canvas. This maintained 
	 * externally in KX_GameEngine
	 */
	const RAS_Rect& GetSceneViewport() const;
	
	/**
	 * \section Accessors to different scenes of this scene
	 */
	void SetNetworkDeviceInterface(NG_NetworkDeviceInterface* newInterface);
	void SetNetworkScene(NG_NetworkScene *newScene);
	void SetWorldInfo(class KX_WorldInfo* wi);
	KX_WorldInfo* GetWorldInfo();
	void CalculateVisibleMeshes(RAS_IRasterizer* rasty, KX_Camera *cam, int layer=0);
	KX_Camera* GetpCamera();
	NG_NetworkDeviceInterface* GetNetworkDeviceInterface();
	NG_NetworkScene* GetNetworkScene();
	KX_BlenderSceneConverter *GetSceneConverter() { return m_sceneConverter; }

	/**
	 * Replicate the logic bricks associated to this object.
	 */

	void ReplicateLogic(class KX_GameObject* newobj);
	static SG_Callbacks	m_callbacks;

	const STR_String& GetName();
	
	// Suspend the entire scene.
	void Suspend();

	// Resume a suspended scene.
	void Resume();

	// Update the mesh for objects based on level of detail settings
	void UpdateObjectLods(void);
	
	// Update the activity box settings for objects in this scene, if needed.
	void UpdateObjectActivity(void);

	// Enable/disable activity culling.
	void SetActivityCulling(bool b);

	// Set the radius of the activity culling box.
	void SetActivityCullingRadius(float f);
	bool IsSuspended();
	bool IsClearingZBuffer();
	void EnableZBufferClearing(bool isclearingZbuffer);
	// use of DBVT tree for camera culling
	void SetDbvtCulling(bool b) { m_dbvt_culling = b; }
	bool GetDbvtCulling() { return m_dbvt_culling; }
	void SetDbvtOcclusionRes(int i) { m_dbvt_occlusion_res = i; }
	int GetDbvtOcclusionRes() { return m_dbvt_occlusion_res; }
	
	void SetSceneConverter(class KX_BlenderSceneConverter* sceneConverter);

	class PHY_IPhysicsEnvironment*		GetPhysicsEnvironment()
	{
		return m_physicsEnvironment;
	}

	void SetPhysicsEnvironment(class PHY_IPhysicsEnvironment*	physEnv);

	void	SetGravity(const MT_Vector3& gravity);
	MT_Vector3 GetGravity();

	short GetAnimationFPS();
	
	/**
	 * Sets the node tree for this scene.
	 */
	void SetNodeTree(SG_Tree* root);

	/**
	 * 2D Filters
	 */
	void Update2DFilter(std::vector<STR_String>& propNames, void* gameObj, RAS_2DFilterManager::RAS_2DFILTER_MODE filtermode, int pass, STR_String& text);
	void Render2DFilters(RAS_ICanvas* canvas);

	KX_ObstacleSimulation* GetObstacleSimulation() { return m_obstacleSimulation; }

#ifdef WITH_PYTHON
	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

	KX_PYMETHOD_DOC(KX_Scene, addObject);
	KX_PYMETHOD_DOC(KX_Scene, end);
	KX_PYMETHOD_DOC(KX_Scene, restart);
	KX_PYMETHOD_DOC(KX_Scene, replace);
	KX_PYMETHOD_DOC(KX_Scene, suspend);
	KX_PYMETHOD_DOC(KX_Scene, resume);
	KX_PYMETHOD_DOC(KX_Scene, get);
	KX_PYMETHOD_DOC(KX_Scene, drawObstacleSimulation);


	/* attributes */
	static PyObject*	pyattr_get_name(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_objects(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_objects_inactive(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_lights(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_cameras(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_active_camera(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_active_camera(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject*	pyattr_get_drawing_callback_pre(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_drawing_callback_pre(void *selv_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject*	pyattr_get_drawing_callback_post(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_drawing_callback_post(void *selv_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject*	pyattr_get_gravity(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_gravity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);

	virtual PyObject *py_repr(void) { return PyUnicode_From_STR_String(GetName()); }
	
	/* getitem/setitem */
	static PyMappingMethods	Mapping;
	static PySequenceMethods	Sequence;

	/**
	 * Run the registered python drawing functions.
	 */
	void RunDrawingCallbacks(PyObject *cb_list);
	
	PyObject *GetPreDrawCB() { return m_draw_call_pre; }
	PyObject *GetPostDrawCB() { return m_draw_call_post; }
#endif

	/**
	 * Sets the time the scene was suspended
	 */ 
	void setSuspendedTime(double suspendedtime);
	/**
	 * Returns the "curtime" the scene was suspended
	 */ 
	double getSuspendedTime();
	/**
	 * Sets the difference between the local time of the scene (when it
	 * was running and not suspended) and the "curtime"
	 */ 
	void setSuspendedDelta(double suspendeddelta);
	/**
	 * Returns the difference between the local time of the scene (when it
	 * was running and not suspended) and the "curtime"
	 */
	double getSuspendedDelta();
	/**
	 * Returns the Blender scene this was made from
	 */
	struct Scene *GetBlenderScene() { return m_blenderScene; }

	bool MergeScene(KX_Scene *other);


	//void PrintStats(int verbose_level) {
	//	m_bucketmanager->PrintStats(verbose_level)
	//}
};

typedef std::vector<KX_Scene*> KX_SceneList;

#endif  /* __KX_SCENE_H__ */
