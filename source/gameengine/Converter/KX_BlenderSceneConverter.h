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

/** \file KX_BlenderSceneConverter.h
 *  \ingroup bgeconv
 */

#ifndef __KX_BLENDERSCENECONVERTER_H__
#define __KX_BLENDERSCENECONVERTER_H__

#include "KX_HashedPtr.h"
#include "CTR_Map.h"
#include <stdio.h>

#include "KX_ISceneConverter.h"
#include "KX_IpoConvert.h"

#include <map>

using namespace std;

class KX_WorldInfo;
class SCA_IActuator;
class SCA_IController;
class RAS_MeshObject;
class RAS_IPolyMaterial;
class BL_InterpolatorList;
class BL_Material;
struct Main;
struct Scene;
struct ThreadInfo;
struct Material;

typedef map<KX_Scene*, map<Material*, BL_Material*> > MaterialCache;
typedef map<KX_Scene*, map<Material*, RAS_IPolyMaterial*> > PolyMaterialCache;

class KX_BlenderSceneConverter : public KX_ISceneConverter
{
	// Use vector of pairs to allow removal of entities between scene switch
	vector<pair<KX_Scene*,KX_WorldInfo*> >	m_worldinfos;
	vector<pair<KX_Scene*,RAS_IPolyMaterial*> > m_polymaterials;
	vector<pair<KX_Scene*,RAS_MeshObject*> > m_meshobjects;
	vector<pair<KX_Scene*,BL_Material *> >	m_materials;

	vector<class KX_LibLoadStatus*> m_mergequeue;
	ThreadInfo	*m_threadinfo;

	// Cached material conversions
	MaterialCache m_mat_cache;
	PolyMaterialCache m_polymat_cache;

	// Saved KX_LibLoadStatus objects
	map<char *, class KX_LibLoadStatus*> m_status_map;

	// Should also have a list of collision shapes. 
	// For the time being this is held in KX_Scene::m_shapes

	CTR_Map<CHashedPtr,KX_GameObject*>	m_map_blender_to_gameobject;		/* cleared after conversion */
	CTR_Map<CHashedPtr,RAS_MeshObject*>	m_map_mesh_to_gamemesh;				/* cleared after conversion */
	CTR_Map<CHashedPtr,SCA_IActuator*>	m_map_blender_to_gameactuator;		/* cleared after conversion */
	CTR_Map<CHashedPtr,SCA_IController*>m_map_blender_to_gamecontroller;	/* cleared after conversion */
	
	CTR_Map<CHashedPtr,BL_InterpolatorList*> m_map_blender_to_gameAdtList;
	
	Main*					m_maggie;
	vector<struct Main*>	m_DynamicMaggie;

	STR_String				m_newfilename;
	class KX_KetsjiEngine*	m_ketsjiEngine;
	class KX_Scene*			m_currentScene;	// Scene being converted
	bool					m_alwaysUseExpandFraming;
	bool					m_usemat;
	bool					m_useglslmat;
	bool					m_use_mat_cache;

public:
	KX_BlenderSceneConverter(
		Main* maggie,
		class KX_KetsjiEngine* engine
	);

	virtual ~KX_BlenderSceneConverter();

	/* Scenename: name of the scene to be converted.
	 * destinationscene: pass an empty scene, everything goes into this
	 * dictobj: python dictionary (for pythoncontrollers)
	 */
	virtual void	ConvertScene(
						class KX_Scene* destinationscene,
						class RAS_IRasterizer* rendertools,
						class RAS_ICanvas* canvas,
						bool libloading=false
					);
	virtual void RemoveScene(class KX_Scene *scene);

	void SetNewFileName(const STR_String& filename);
	bool TryAndLoadNewFile();

	void SetAlwaysUseExpandFraming(bool to_what);
	
	void RegisterGameObject(KX_GameObject *gameobject, struct Object *for_blenderobject);
	void UnregisterGameObject(KX_GameObject *gameobject);
	KX_GameObject *FindGameObject(struct Object *for_blenderobject);

	void RegisterGameMesh(RAS_MeshObject *gamemesh, struct Mesh *for_blendermesh);
	RAS_MeshObject *FindGameMesh(struct Mesh *for_blendermesh/*, unsigned int onlayer*/);

	void RegisterPolyMaterial(RAS_IPolyMaterial *polymat);
	void CachePolyMaterial(KX_Scene *scene, Material *mat, RAS_IPolyMaterial *polymat);
	RAS_IPolyMaterial *FindCachedPolyMaterial(KX_Scene *scene, Material *mat);

	void RegisterBlenderMaterial(BL_Material *mat);
	void CacheBlenderMaterial(KX_Scene *scene, Material *mat, BL_Material *blmat);
	BL_Material *FindCachedBlenderMaterial(KX_Scene *scene, Material *mat);
	
	void RegisterInterpolatorList(BL_InterpolatorList *actList, struct bAction *for_act);
	BL_InterpolatorList *FindInterpolatorList(struct bAction *for_act);

	void RegisterGameActuator(SCA_IActuator *act, struct bActuator *for_actuator);
	SCA_IActuator *FindGameActuator(struct bActuator *for_actuator);

	void RegisterGameController(SCA_IController *cont, struct bController *for_controller);
	SCA_IController *FindGameController(struct bController *for_controller);

	void RegisterWorldInfo(KX_WorldInfo *worldinfo);

	virtual void	ResetPhysicsObjectsAnimationIpo(bool clearIpo);

	///this is for reseting the position,rotation and scale of the gameobjet that is not dynamic
	virtual	void	resetNoneDynamicObjectToIpo();
	
	///this generates ipo curves for position, rotation, allowing to use game physics in animation
	virtual void	WritePhysicsObjectToAnimationIpo(int frameNumber);
	virtual void	TestHandlesPhysicsObjectToAnimationIpo();

	// use blender materials
	virtual void SetMaterials(bool val);
	virtual bool GetMaterials();

	// use blender glsl materials
	virtual void SetGLSLMaterials(bool val);
	virtual bool GetGLSLMaterials();

	// cache materials during conversion
	virtual void SetCacheMaterials(bool val);
	virtual bool GetCacheMaterials();

	struct Scene* GetBlenderSceneForName(const STR_String& name);

//	struct Main* GetMain() { return m_maggie; }
	struct Main*		  GetMainDynamicPath(const char *path);
	vector<struct Main*> &GetMainDynamic();
	
	class KX_LibLoadStatus *LinkBlendFileMemory(void *data, int length, const char *path, char *group, KX_Scene *scene_merge, char **err_str, short options);
	class KX_LibLoadStatus *LinkBlendFilePath(const char *path, char *group, KX_Scene *scene_merge, char **err_str, short options);
	class KX_LibLoadStatus *LinkBlendFile(struct BlendHandle *bpy_openlib, const char *path, char *group, KX_Scene *scene_merge, char **err_str, short options);
	bool MergeScene(KX_Scene *to, KX_Scene *from);
	RAS_MeshObject *ConvertMeshSpecial(KX_Scene* kx_scene, Main *maggie, const char *name);
	bool FreeBlendFile(struct Main *maggie);
	bool FreeBlendFile(const char *path);

	virtual void MergeAsyncLoads();
	void AddScenesToMergeQueue(class KX_LibLoadStatus *status);
 
	void PrintStats() {
		printf("BGE STATS!\n");

		printf("\nAssets...\n");
		printf("\t m_worldinfos: %d\n", (int)m_worldinfos.size());
		printf("\t m_polymaterials: %d\n", (int)m_polymaterials.size());
		printf("\t m_meshobjects: %d\n", (int)m_meshobjects.size());
		printf("\t m_materials: %d\n", (int)m_materials.size());

		printf("\nMappings...\n");
		printf("\t m_map_blender_to_gameobject: %d\n", m_map_blender_to_gameobject.size());
		printf("\t m_map_mesh_to_gamemesh: %d\n", m_map_mesh_to_gamemesh.size());
		printf("\t m_map_blender_to_gameactuator: %d\n", m_map_blender_to_gameactuator.size());
		printf("\t m_map_blender_to_gamecontroller: %d\n", m_map_blender_to_gamecontroller.size());
		printf("\t m_map_blender_to_gameAdtList: %d\n", m_map_blender_to_gameAdtList.size());

#ifdef WITH_CXX_GUARDEDALLOC
		MEM_printmemlist_pydict();
#endif
//		/printf("\t m_ketsjiEngine->m_scenes: %d\n", m_ketsjiEngine->CurrentScenes()->size());
	}
	
	/* LibLoad Options */
	enum 
	{
		LIB_LOAD_LOAD_ACTIONS = 1,
		LIB_LOAD_VERBOSE = 2,
		LIB_LOAD_LOAD_SCRIPTS = 4,
		LIB_LOAD_ASYNC = 8,
	};



#ifdef WITH_PYTHON
	PyObject *GetPyNamespace();
#endif
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:KX_BlenderSceneConverter")
#endif
};

#endif  /* __KX_BLENDERSCENECONVERTER_H__ */
