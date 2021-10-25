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

/** \file gameengine/Converter/KX_BlenderSceneConverter.cpp
 *  \ingroup bgeconv
 */

#ifdef _MSC_VER
#  pragma warning (disable:4786)  /* suppress stl-MSVC debug info warning */
#endif

#include "KX_Scene.h"
#include "KX_GameObject.h"
#include "KX_IpoConvert.h"
#include "RAS_MeshObject.h"
#include "KX_PhysicsEngineEnums.h"
#include "PHY_IPhysicsEnvironment.h"
#include "KX_KetsjiEngine.h"
#include "KX_PythonInit.h" // So we can handle adding new text datablocks for Python to import
#include "BL_Material.h"
#include "BL_ActionActuator.h"
#include "KX_BlenderMaterial.h"


#include "BL_System.h"

#include "DummyPhysicsEnvironment.h"


#ifdef WITH_BULLET
#include "CcdPhysicsEnvironment.h"
#endif

#include "KX_LibLoadStatus.h"
#include "KX_BlenderScalarInterpolator.h"
#include "BL_BlenderDataConversion.h"
#include "KX_WorldInfo.h"

/* This little block needed for linking to Blender... */
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

/* This list includes only data type definitions */
#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "BKE_main.h"
#include "BKE_fcurve.h"

#include "BLI_math.h"

extern "C"
{
#include "DNA_object_types.h"
#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "BLI_blenlib.h"
#include "MEM_guardedalloc.h"
#include "BKE_global.h"
#include "BKE_animsys.h"
#include "BKE_library.h"
#include "BKE_material.h" // BKE_material_copy
#include "BKE_mesh.h" // BKE_mesh_copy
#include "DNA_space_types.h"
#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "RNA_define.h"
#include "../../blender/editors/include/ED_keyframing.h"
}

/* Only for dynamic loading and merging */
#include "RAS_BucketManager.h" // XXX cant stay
#include "KX_BlenderSceneConverter.h"
#include "KX_MeshProxy.h"
extern "C" {
	#include "PIL_time.h"
	#include "BKE_context.h"
	#include "BLO_readfile.h"
	#include "BKE_idcode.h"
	#include "BKE_report.h"
	#include "DNA_space_types.h"
	#include "DNA_windowmanager_types.h" /* report api */
	#include "../../blender/blenlib/BLI_linklist.h"
}

#include "BLI_task.h"

// This is used to avoid including BLI_task.h in KX_BlenderSceneConverter.h
typedef struct ThreadInfo {
	TaskPool *m_pool;
	ThreadMutex m_mutex;
} ThreadInfo;

KX_BlenderSceneConverter::KX_BlenderSceneConverter(
							Main *maggie,
							KX_KetsjiEngine *engine)
							:m_maggie(maggie),
							m_ketsjiEngine(engine),
							m_alwaysUseExpandFraming(false),
							m_usemat(false),
							m_useglslmat(false),
							m_use_mat_cache(true)
{
	BKE_main_id_tag_all(maggie, LIB_TAG_DOIT, false);  /* avoid re-tagging later on */
	m_newfilename = "";
	m_threadinfo = new ThreadInfo();
	m_threadinfo->m_pool = BLI_task_pool_create(engine->GetTaskScheduler(), NULL);
	BLI_mutex_init(&m_threadinfo->m_mutex);
}

KX_BlenderSceneConverter::~KX_BlenderSceneConverter()
{
	// clears meshes, and hashmaps from blender to gameengine data
	// delete sumoshapes

	int numAdtLists = m_map_blender_to_gameAdtList.size();
	for (int i = 0; i < numAdtLists; i++) {
		BL_InterpolatorList *adtList = *m_map_blender_to_gameAdtList.at(i);

		delete (adtList);
	}

	vector<pair<KX_Scene *, KX_WorldInfo *> >::iterator itw = m_worldinfos.begin();
	while (itw != m_worldinfos.end()) {
		delete itw->second;
		itw++;
	}
	m_worldinfos.clear();

	vector<pair<KX_Scene *,RAS_IPolyMaterial *> >::iterator itp = m_polymaterials.begin();
	while (itp != m_polymaterials.end()) {
		delete itp->second;
		itp++;
	}
	m_polymaterials.clear();

	// delete after RAS_IPolyMaterial
	vector<pair<KX_Scene *,BL_Material *> >::iterator itmat = m_materials.begin();
	while (itmat != m_materials.end()) {
		delete itmat->second;
		itmat++;
	}
	m_materials.clear();

	vector<pair<KX_Scene *,RAS_MeshObject *> >::iterator itm = m_meshobjects.begin();
	while (itm != m_meshobjects.end()) {
		delete itm->second;
		itm++;
	}
	m_meshobjects.clear();

	/* free any data that was dynamically loaded */
	while (m_DynamicMaggie.size() != 0) {
		FreeBlendFile(m_DynamicMaggie[0]);
	}

	m_DynamicMaggie.clear();

	if (m_threadinfo) {
		/* Thread infos like mutex must be freed after FreeBlendFile function.
		Because it needs to lock the mutex, even if there's no active task when it's
		in the scene converter destructor. */
		BLI_task_pool_free(m_threadinfo->m_pool);
		BLI_mutex_end(&m_threadinfo->m_mutex);
		delete m_threadinfo;
	}
}

void KX_BlenderSceneConverter::SetNewFileName(const STR_String &filename)
{
	m_newfilename = filename;
}

bool KX_BlenderSceneConverter::TryAndLoadNewFile()
{
	bool result = false;

	return result;
}

Scene *KX_BlenderSceneConverter::GetBlenderSceneForName(const STR_String &name)
{
	Scene *sce;

	/**
	 * Find the specified scene by name, or NULL if nothing matches.
	 */
	if ((sce = (Scene *)BLI_findstring(&m_maggie->scene, name.ReadPtr(), offsetof(ID, name) + 2)))
		return sce;

	for (vector<Main *>::iterator it=m_DynamicMaggie.begin(); !(it == m_DynamicMaggie.end()); it++) {
		Main *main = *it;

		if ((sce= (Scene *)BLI_findstring(&main->scene, name.ReadPtr(), offsetof(ID, name) + 2)))
			return sce;
	}

	return NULL;
}

void KX_BlenderSceneConverter::ConvertScene(KX_Scene *destinationscene, RAS_IRasterizer *rendertools,
											RAS_ICanvas *canvas, bool libloading)
{
	//find out which physics engine
	Scene *blenderscene = destinationscene->GetBlenderScene();

	PHY_IPhysicsEnvironment *phy_env = NULL;

	e_PhysicsEngine physics_engine = UseBullet;
	// hook for registration function during conversion.
	m_currentScene = destinationscene;
	destinationscene->SetSceneConverter(this);

	// This doesn't really seem to do anything except cause potential issues
	// when doing threaded conversion, so it's disabled for now.
	// SG_SetActiveStage(SG_STAGE_CONVERTER);

	switch (blenderscene->gm.physicsEngine) {
#ifdef WITH_BULLET
	case WOPHY_BULLET:
		{
			SYS_SystemHandle syshandle = SYS_GetSystem(); /*unused*/
			int visualizePhysics = SYS_GetCommandLineInt(syshandle, "show_physics", 0);

			phy_env = CcdPhysicsEnvironment::Create(blenderscene, visualizePhysics);
			physics_engine = UseBullet;
			break;
		}
#endif
	default:
	case WOPHY_NONE:
		{
			// We should probably use some sort of factory here
			phy_env = new DummyPhysicsEnvironment();
			physics_engine = UseNone;
			break;
		}
	}

	destinationscene->SetPhysicsEnvironment(phy_env);

	BL_ConvertBlenderObjects(
		m_maggie,
		destinationscene,
		m_ketsjiEngine,
		physics_engine,
		rendertools,
		canvas,
		this,
		m_alwaysUseExpandFraming,
		libloading);

	//These lookup are not needed during game
	m_map_blender_to_gameactuator.clear();
	m_map_blender_to_gamecontroller.clear();
	m_map_blender_to_gameobject.clear();

	//Clearing this lookup table has the effect of disabling the cache of meshes
	//between scenes, even if they are shared in the blend file.
	//This cache mecanism is buggy so I leave it disable and the memory leak
	//that would result from this is fixed in RemoveScene()
	m_map_mesh_to_gamemesh.clear();
}

// This function removes all entities stored in the converter for that scene
// It should be used instead of direct delete scene
// Note that there was some provision for sharing entities (meshes...) between
// scenes but that is now disabled so all scene will have their own copy
// and we can delete them here. If the sharing is reactivated, change this code too..
// (see KX_BlenderSceneConverter::ConvertScene)
void KX_BlenderSceneConverter::RemoveScene(KX_Scene *scene)
{
	int i, size;
	// delete the scene first as it will stop the use of entities
	delete scene;
	// delete the entities of this scene
	vector<pair<KX_Scene *, KX_WorldInfo *> >::iterator worldit;
	size = m_worldinfos.size();
	for (i = 0, worldit = m_worldinfos.begin(); i < size; ) {
		if (worldit->first == scene) {
			delete worldit->second;
			*worldit = m_worldinfos.back();
			m_worldinfos.pop_back();
			size--;
		} 
		else {
			i++;
			worldit++;
		}
	}

	vector<pair<KX_Scene *, RAS_IPolyMaterial *> >::iterator polymit;
	size = m_polymaterials.size();
	for (i = 0, polymit = m_polymaterials.begin(); i < size; ) {
		if (polymit->first == scene) {
			m_polymat_cache[scene].erase(polymit->second->GetBlenderMaterial());
			delete polymit->second;
			*polymit = m_polymaterials.back();
			m_polymaterials.pop_back();
			size--;
		} 
		else {
			i++;
			polymit++;
		}
	}

	m_polymat_cache.erase(scene);

	vector<pair<KX_Scene *, BL_Material *> >::iterator matit;
	size = m_materials.size();
	for (i = 0, matit = m_materials.begin(); i < size; ) {
		if (matit->first == scene) {
			m_mat_cache[scene].erase(matit->second->material);
			delete matit->second;
			*matit = m_materials.back();
			m_materials.pop_back();
			size--;
		} 
		else {
			i++;
			matit++;
		}
	}

	m_mat_cache.erase(scene);

	vector<pair<KX_Scene *, RAS_MeshObject *> >::iterator meshit;
	size = m_meshobjects.size();
	for (i = 0, meshit = m_meshobjects.begin(); i < size; ) {
		if (meshit->first == scene) {
			delete meshit->second;
			*meshit = m_meshobjects.back();
			m_meshobjects.pop_back();
			size--;
		} 
		else {
			i++;
			meshit++;
		}
	}
}

// use blender materials
void KX_BlenderSceneConverter::SetMaterials(bool val)
{
	m_usemat = val;
	m_useglslmat = false;
}

void KX_BlenderSceneConverter::SetGLSLMaterials(bool val)
{
	m_usemat = val;
	m_useglslmat = val;
}

void KX_BlenderSceneConverter::SetCacheMaterials(bool val)
{
	m_use_mat_cache = val;
}

bool KX_BlenderSceneConverter::GetMaterials()
{
	return m_usemat;
}

bool KX_BlenderSceneConverter::GetGLSLMaterials()
{
	return m_useglslmat;
}

bool KX_BlenderSceneConverter::GetCacheMaterials()
{
	return m_use_mat_cache;
}

void KX_BlenderSceneConverter::RegisterBlenderMaterial(BL_Material *mat)
{
	// First make sure we don't register the material twice
	vector<pair<KX_Scene *, BL_Material *> >::iterator it;
	for (it = m_materials.begin(); it != m_materials.end(); ++it)
		if (it->second == mat)
			return;

	m_materials.push_back(pair<KX_Scene *, BL_Material *> (m_currentScene, mat));
}

void KX_BlenderSceneConverter::SetAlwaysUseExpandFraming(bool to_what)
{
	m_alwaysUseExpandFraming= to_what;
}

void KX_BlenderSceneConverter::RegisterGameObject(KX_GameObject *gameobject, Object *for_blenderobject) 
{
	/* only maintained while converting, freed during game runtime */
	m_map_blender_to_gameobject.insert(CHashedPtr(for_blenderobject), gameobject);
}

/* only need to run this during conversion since
 * m_map_blender_to_gameobject is freed after conversion */
void KX_BlenderSceneConverter::UnregisterGameObject(KX_GameObject *gameobject) 
{
	Object *bobp = gameobject->GetBlenderObject();
	if (bobp) {
		CHashedPtr bptr(bobp);
		KX_GameObject **gobp = m_map_blender_to_gameobject[bptr];
		if (gobp && *gobp == gameobject) {
			// also maintain m_map_blender_to_gameobject if the gameobject
			// being removed is matching the blender object
			m_map_blender_to_gameobject.remove(bptr);
		}
	}
}

KX_GameObject *KX_BlenderSceneConverter::FindGameObject(Object *for_blenderobject) 
{
	KX_GameObject **obp = m_map_blender_to_gameobject[CHashedPtr(for_blenderobject)];

	return obp ? *obp : NULL;
}

void KX_BlenderSceneConverter::RegisterGameMesh(RAS_MeshObject *gamemesh, Mesh *for_blendermesh)
{
	if (for_blendermesh) { /* dynamically loaded meshes we don't want to keep lookups for */
		m_map_mesh_to_gamemesh.insert(CHashedPtr(for_blendermesh),gamemesh);
	}
	m_meshobjects.push_back(pair<KX_Scene *, RAS_MeshObject *> (m_currentScene,gamemesh));
}

RAS_MeshObject *KX_BlenderSceneConverter::FindGameMesh(Mesh *for_blendermesh)
{
	RAS_MeshObject **meshp = m_map_mesh_to_gamemesh[CHashedPtr(for_blendermesh)];

	if (meshp) {
		return *meshp;
	} 
	else {
		return NULL;
	}
}

void KX_BlenderSceneConverter::RegisterPolyMaterial(RAS_IPolyMaterial *polymat)
{
	// First make sure we don't register the material twice
	vector<pair<KX_Scene *, RAS_IPolyMaterial *> >::iterator it;
	for (it = m_polymaterials.begin(); it != m_polymaterials.end(); ++it)
		if (it->second == polymat)
			return;
	m_polymaterials.push_back(pair<KX_Scene *, RAS_IPolyMaterial *> (m_currentScene, polymat));
}

void KX_BlenderSceneConverter::CachePolyMaterial(KX_Scene *scene, Material *mat, RAS_IPolyMaterial *polymat)
{
	if (m_use_mat_cache && mat)
		m_polymat_cache[scene][mat] = polymat;
}

RAS_IPolyMaterial *KX_BlenderSceneConverter::FindCachedPolyMaterial(KX_Scene *scene, Material *mat)
{
	return (m_use_mat_cache) ? m_polymat_cache[scene][mat] : NULL;
}

void KX_BlenderSceneConverter::CacheBlenderMaterial(KX_Scene *scene, Material *mat, BL_Material *blmat)
{
	if (m_use_mat_cache && mat)
		m_mat_cache[scene][mat] = blmat;
}

BL_Material *KX_BlenderSceneConverter::FindCachedBlenderMaterial(KX_Scene *scene, Material *mat)
{
	return (m_use_mat_cache) ? m_mat_cache[scene][mat] : NULL;
}

void KX_BlenderSceneConverter::RegisterInterpolatorList(BL_InterpolatorList *actList, bAction *for_act)
{
	m_map_blender_to_gameAdtList.insert(CHashedPtr(for_act), actList);
}

BL_InterpolatorList *KX_BlenderSceneConverter::FindInterpolatorList(bAction *for_act)
{
	BL_InterpolatorList **listp = m_map_blender_to_gameAdtList[CHashedPtr(for_act)];
	return listp ? *listp : NULL;
}

void KX_BlenderSceneConverter::RegisterGameActuator(SCA_IActuator *act, bActuator *for_actuator)
{
	m_map_blender_to_gameactuator.insert(CHashedPtr(for_actuator), act);
}

SCA_IActuator *KX_BlenderSceneConverter::FindGameActuator(bActuator *for_actuator)
{
	SCA_IActuator **actp = m_map_blender_to_gameactuator[CHashedPtr(for_actuator)];
	return actp ? *actp : NULL;
}

void KX_BlenderSceneConverter::RegisterGameController(SCA_IController *cont, bController *for_controller)
{
	m_map_blender_to_gamecontroller.insert(CHashedPtr(for_controller), cont);
}

SCA_IController *KX_BlenderSceneConverter::FindGameController(bController *for_controller)
{
	SCA_IController **contp = m_map_blender_to_gamecontroller[CHashedPtr(for_controller)];
	return contp ? *contp : NULL;
}

void KX_BlenderSceneConverter::RegisterWorldInfo(KX_WorldInfo *worldinfo)
{
	m_worldinfos.push_back(pair<KX_Scene *, KX_WorldInfo *> (m_currentScene, worldinfo));
}

void KX_BlenderSceneConverter::ResetPhysicsObjectsAnimationIpo(bool clearIpo)
{
	//TODO this entire function is deprecated, written for 2.4x
	//the functionality should be rewritten, currently it does nothing

	KX_SceneList *scenes = m_ketsjiEngine->CurrentScenes();
	int numScenes = scenes->size();
	int i;
	for (i = 0; i < numScenes; i++) {
		KX_Scene *scene = scenes->at(i);
		CListValue *parentList = scene->GetRootParentList();
		int numObjects = parentList->GetCount();
		int g;
		for (g = 0; g < numObjects; g++) {
			KX_GameObject *gameObj = (KX_GameObject *)parentList->GetValue(g);
			if (gameObj->IsRecordAnimation()) {
				Object *blenderObject = gameObj->GetBlenderObject();
				if (blenderObject) {
#if 0
					//erase existing ipo's
					Ipo* ipo = blenderObject->ipo;//findIpoForName(blenderObject->id.name+2);
					if (ipo) { 	//clear the curve data
						if (clearIpo) {//rcruiz
							IpoCurve *icu1;

							int numCurves = 0;
							for ( icu1 = (IpoCurve*)ipo->curve.first; icu1;  ) {

								IpoCurve* tmpicu = icu1;

								/*int i;
								BezTriple *bezt;
								for ( bezt = tmpicu->bezt, i = 0;	i < tmpicu->totvert; i++, bezt++) {
									printf("(%f,%f,%f),(%f,%f,%f),(%f,%f,%f)\n",bezt->vec[0][0],bezt->vec[0][1],bezt->vec[0][2],bezt->vec[1][0],bezt->vec[1][1],bezt->vec[1][2],bezt->vec[2][0],bezt->vec[2][1],bezt->vec[2][2]);
								}*/

								icu1 = icu1->next;
								numCurves++;

								BLI_remlink( &( blenderObject->ipo->curve ), tmpicu );
								if ( tmpicu->bezt )
									MEM_freeN( tmpicu->bezt );
								MEM_freeN( tmpicu );
								localDel_ipoCurve( tmpicu );
							}
						}
					} 
					else {
						ipo = NULL; // XXX add_ipo(blenderObject->id.name+2, ID_OB);
						blenderObject->ipo = ipo;
					}
#endif
				}
			}
		}
	}
}

void KX_BlenderSceneConverter::resetNoneDynamicObjectToIpo()
{
	//TODO the functionality should be rewritten
}

// this generates ipo curves for position, rotation, allowing to use game physics in animation
void KX_BlenderSceneConverter::WritePhysicsObjectToAnimationIpo(int frameNumber)
{
	KX_SceneList *scenes = m_ketsjiEngine->CurrentScenes();
	int numScenes = scenes->size();
	int i;
	for (i = 0; i < numScenes; i++) {
		KX_Scene *scene = scenes->at(i);
		//PHY_IPhysicsEnvironment* physEnv = scene->GetPhysicsEnvironment();
		CListValue *parentList = scene->GetObjectList();
		int numObjects = parentList->GetCount();
		int g;
		for (g = 0; g < numObjects; g++) {
			KX_GameObject *gameObj = (KX_GameObject *)parentList->GetValue(g);
			Object *blenderObject = gameObj->GetBlenderObject();
			if (blenderObject && blenderObject->parent == NULL && gameObj->IsRecordAnimation()) {
				if (blenderObject->adt == NULL)
					BKE_animdata_add_id(&blenderObject->id);

				if (blenderObject->adt) {
					const MT_Point3 &position = gameObj->NodeGetWorldPosition();
					//const MT_Vector3& scale = gameObj->NodeGetWorldScaling();
					const MT_Matrix3x3 &orn = gameObj->NodeGetWorldOrientation();

					position.getValue(blenderObject->loc);

					float tmat[3][3];
					for (int r = 0; r < 3; r++)
						for (int c = 0; c < 3; c++)
							tmat[r][c] = (float)orn[c][r];

					mat3_to_compatible_eul(blenderObject->rot, blenderObject->rot, tmat);

					insert_keyframe(NULL, &blenderObject->id, NULL, NULL, "location", -1, (float)frameNumber, BEZT_KEYTYPE_JITTER, INSERTKEY_FAST);
					insert_keyframe(NULL, &blenderObject->id, NULL, NULL, "rotation_euler", -1, (float)frameNumber, BEZT_KEYTYPE_JITTER, INSERTKEY_FAST);

#if 0
					const MT_Point3& position = gameObj->NodeGetWorldPosition();
					//const MT_Vector3& scale = gameObj->NodeGetWorldScaling();
					const MT_Matrix3x3& orn = gameObj->NodeGetWorldOrientation();
					
					float eulerAngles[3];
					float eulerAnglesOld[3] = {0.0f, 0.0f, 0.0f};
					float tmat[3][3];
					
					// XXX animato
					Ipo* ipo = blenderObject->ipo;

					//create the curves, if not existing, set linear if new

					IpoCurve *icu_lx = findIpoCurve((IpoCurve *)ipo->curve.first,"LocX");
					if (!icu_lx) {
						icu_lx = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_LOC_X, 1);
						if (icu_lx) icu_lx->ipo = IPO_LIN;
					}
					IpoCurve *icu_ly = findIpoCurve((IpoCurve *)ipo->curve.first,"LocY");
					if (!icu_ly) {
						icu_ly = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_LOC_Y, 1);
						if (icu_ly) icu_ly->ipo = IPO_LIN;
					}
					IpoCurve *icu_lz = findIpoCurve((IpoCurve *)ipo->curve.first,"LocZ");
					if (!icu_lz) {
						icu_lz = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_LOC_Z, 1);
						if (icu_lz) icu_lz->ipo = IPO_LIN;
					}
					IpoCurve *icu_rx = findIpoCurve((IpoCurve *)ipo->curve.first,"RotX");
					if (!icu_rx) {
						icu_rx = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_ROT_X, 1);
						if (icu_rx) icu_rx->ipo = IPO_LIN;
					}
					IpoCurve *icu_ry = findIpoCurve((IpoCurve *)ipo->curve.first,"RotY");
					if (!icu_ry) {
						icu_ry = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_ROT_Y, 1);
						if (icu_ry) icu_ry->ipo = IPO_LIN;
					}
					IpoCurve *icu_rz = findIpoCurve((IpoCurve *)ipo->curve.first,"RotZ");
					if (!icu_rz) {
						icu_rz = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_ROT_Z, 1);
						if (icu_rz) icu_rz->ipo = IPO_LIN;
					}
					
					if (icu_rx) eulerAnglesOld[0] = eval_icu( icu_rx, frameNumber - 1 ) / ((180 / 3.14159265f) / 10);
					if (icu_ry) eulerAnglesOld[1] = eval_icu( icu_ry, frameNumber - 1 ) / ((180 / 3.14159265f) / 10);
					if (icu_rz) eulerAnglesOld[2] = eval_icu( icu_rz, frameNumber - 1 ) / ((180 / 3.14159265f) / 10);
					
					// orn.getValue((float *)tmat); // uses the wrong ordering, cant use this
					for (int r = 0; r < 3; r++)
						for (int c = 0; c < 3; c++)
							tmat[r][c] = orn[c][r];
					
					// mat3_to_eul( eulerAngles,tmat); // better to use Mat3ToCompatibleEul
					mat3_to_compatible_eul( eulerAngles, eulerAnglesOld,tmat);
					
					//eval_icu
					for (int x = 0; x < 3; x++)
						eulerAngles[x] *= (float) ((180 / 3.14159265f) / 10.0);
					
					//fill the curves with data
					if (icu_lx) insert_vert_icu(icu_lx, frameNumber, position.x(), 1);
					if (icu_ly) insert_vert_icu(icu_ly, frameNumber, position.y(), 1);
					if (icu_lz) insert_vert_icu(icu_lz, frameNumber, position.z(), 1);
					if (icu_rx) insert_vert_icu(icu_rx, frameNumber, eulerAngles[0], 1);
					if (icu_ry) insert_vert_icu(icu_ry, frameNumber, eulerAngles[1], 1);
					if (icu_rz) insert_vert_icu(icu_rz, frameNumber, eulerAngles[2], 1);
					
					// Handles are corrected at the end, testhandles_ipocurve isn't needed yet
#endif
				}
			}
		}
	}
}

void KX_BlenderSceneConverter::TestHandlesPhysicsObjectToAnimationIpo()
{
	KX_SceneList *scenes = m_ketsjiEngine->CurrentScenes();
	int numScenes = scenes->size();
	int i;
	for (i = 0; i < numScenes; i++) {
		KX_Scene *scene = scenes->at(i);
		//PHY_IPhysicsEnvironment* physEnv = scene->GetPhysicsEnvironment();
		CListValue *parentList = scene->GetRootParentList();
		int numObjects = parentList->GetCount();
		int g;
		for (g = 0; g < numObjects; g++) {
			KX_GameObject *gameObj = (KX_GameObject *)parentList->GetValue(g);
			if (gameObj->IsRecordAnimation()) {
				Object *blenderObject = gameObj->GetBlenderObject();
				if (blenderObject && blenderObject->adt) {
					bAction *act = verify_adt_action(&blenderObject->id, false);
					FCurve *fcu;

					if (!act) {
						continue;
					}

					/* for now, not much choice but to run this on all curves... */
					for (fcu = (FCurve *)act->curves.first; fcu; fcu = fcu->next) {
						/* Note: calling `sort_time_fcurve()` here is not needed, since
						 *       all keys have been added in 'right' order. */
						calchandles_fcurve(fcu);
					}
#if 0
					// XXX animato
					Ipo* ipo = blenderObject->ipo;

					//create the curves, if not existing
					//testhandles_ipocurve checks for NULL
					testhandles_ipocurve(findIpoCurve((IpoCurve *)ipo->curve.first,"LocX"));
					testhandles_ipocurve(findIpoCurve((IpoCurve *)ipo->curve.first,"LocY"));
					testhandles_ipocurve(findIpoCurve((IpoCurve *)ipo->curve.first,"LocZ"));
					testhandles_ipocurve(findIpoCurve((IpoCurve *)ipo->curve.first,"RotX"));
					testhandles_ipocurve(findIpoCurve((IpoCurve *)ipo->curve.first,"RotY"));
					testhandles_ipocurve(findIpoCurve((IpoCurve *)ipo->curve.first,"RotZ"));
#endif
				}
			}
		}
	}
}

#ifdef WITH_PYTHON
PyObject *KX_BlenderSceneConverter::GetPyNamespace()
{
	return m_ketsjiEngine->GetPyNamespace();
}
#endif

vector<Main *> &KX_BlenderSceneConverter::GetMainDynamic()
{
	return m_DynamicMaggie;
}

Main *KX_BlenderSceneConverter::GetMainDynamicPath(const char *path)
{
	for (vector<Main *>::iterator it = m_DynamicMaggie.begin(); !(it == m_DynamicMaggie.end()); it++)
		if (BLI_path_cmp((*it)->name, path) == 0)
			return *it;
	
	return NULL;
}

void KX_BlenderSceneConverter::MergeAsyncLoads()
{
	vector<KX_Scene *> *merge_scenes;

	vector<KX_LibLoadStatus *>::iterator mit;
	vector<KX_Scene *>::iterator sit;

	BLI_mutex_lock(&m_threadinfo->m_mutex);

	for (mit = m_mergequeue.begin(); mit != m_mergequeue.end(); ++mit) {
		merge_scenes = (vector<KX_Scene *> *)(*mit)->GetData();

		for (sit=merge_scenes->begin(); sit!=merge_scenes->end(); ++sit) {
			(*mit)->GetMergeScene()->MergeScene(*sit);
			delete (*sit);
		}

		delete merge_scenes;
		(*mit)->SetData(NULL);

		(*mit)->Finish();
	}

	m_mergequeue.clear();

	BLI_mutex_unlock(&m_threadinfo->m_mutex);
}

void KX_BlenderSceneConverter::FinalizeAsyncLoads()
{
	// Finish all loading libraries.
	if (m_threadinfo) {
		BLI_task_pool_work_and_wait(m_threadinfo->m_pool);
	}
	// Merge all libraries data in the current scene, to avoid memory leak of unmerged scenes.
	MergeAsyncLoads();
}

void KX_BlenderSceneConverter::AddScenesToMergeQueue(KX_LibLoadStatus *status)
{
	BLI_mutex_lock(&m_threadinfo->m_mutex);
	m_mergequeue.push_back(status);
	BLI_mutex_unlock(&m_threadinfo->m_mutex);
}

static void async_convert(TaskPool *pool, void *ptr, int UNUSED(threadid))
{
	KX_Scene *new_scene = NULL;
	KX_LibLoadStatus *status = (KX_LibLoadStatus *)ptr;
	vector<Scene *> *scenes = (vector<Scene *> *)status->GetData();
	vector<KX_Scene *> *merge_scenes = new vector<KX_Scene *>(); // Deleted in MergeAsyncLoads

	for (unsigned int i = 0; i < scenes->size(); ++i) {
		new_scene = status->GetEngine()->CreateScene((*scenes)[i], true);

		if (new_scene)
			merge_scenes->push_back(new_scene);

		status->AddProgress((1.0f / scenes->size()) * 0.9f); // We'll call conversion 90% and merging 10% for now
	}

	delete scenes;
	status->SetData(merge_scenes);

	status->GetConverter()->AddScenesToMergeQueue(status);
}

KX_LibLoadStatus *KX_BlenderSceneConverter::LinkBlendFileMemory(void *data, int length, const char *path, char *group, KX_Scene *scene_merge, char **err_str, short options)
{
	BlendHandle *bpy_openlib = BLO_blendhandle_from_memory(data, length);

	// Error checking is done in LinkBlendFile
	return LinkBlendFile(bpy_openlib, path, group, scene_merge, err_str, options);
}

KX_LibLoadStatus *KX_BlenderSceneConverter::LinkBlendFilePath(const char *filepath, char *group, KX_Scene *scene_merge, char **err_str, short options)
{
	BlendHandle *bpy_openlib = BLO_blendhandle_from_file(filepath, NULL);

	// Error checking is done in LinkBlendFile
	return LinkBlendFile(bpy_openlib, filepath, group, scene_merge, err_str, options);
}

static void load_datablocks(Main *main_tmp, BlendHandle *bpy_openlib, const char *path, int idcode)
{
	LinkNode *names = NULL;

	int totnames_dummy;
	names = BLO_blendhandle_get_datablock_names(bpy_openlib, idcode, &totnames_dummy);
	
	int i = 0;
	LinkNode *n = names;
	while (n) {
		BLO_library_link_named_part(main_tmp, &bpy_openlib, idcode, (char *)n->link);
		n = (LinkNode *)n->next;
		i++;
	}
	BLI_linklist_free(names, free);	/* free linklist *and* each node's data */
}

KX_LibLoadStatus *KX_BlenderSceneConverter::LinkBlendFile(BlendHandle *bpy_openlib, const char *path, char *group, KX_Scene *scene_merge, char **err_str, short options)
{
	Main *main_newlib; /* stored as a dynamic 'main' until we free it */
	const int idcode = BKE_idcode_from_name(group);
	ReportList reports;
	static char err_local[255];

//	TIMEIT_START(bge_link_blend_file);

	KX_LibLoadStatus *status;

	/* only scene and mesh supported right now */
	if (idcode != ID_SCE && idcode != ID_ME && idcode != ID_AC) {
		snprintf(err_local, sizeof(err_local), "invalid ID type given \"%s\"\n", group);
		*err_str = err_local;
		BLO_blendhandle_close(bpy_openlib);
		return NULL;
	}
	
	if (GetMainDynamicPath(path)) {
		snprintf(err_local, sizeof(err_local), "blend file already open \"%s\"\n", path);
		*err_str = err_local;
		BLO_blendhandle_close(bpy_openlib);
		return NULL;
	}

	if (bpy_openlib == NULL) {
		snprintf(err_local, sizeof(err_local), "could not open blendfile \"%s\"\n", path);
		*err_str = err_local;
		return NULL;
	}

	main_newlib = BKE_main_new();
	BKE_reports_init(&reports, RPT_STORE);

	short flag = 0; /* don't need any special options */
	/* created only for linking, then freed */
	Main *main_tmp = BLO_library_link_begin(main_newlib, &bpy_openlib, (char *)path);

	load_datablocks(main_tmp, bpy_openlib, path, idcode);

	if (idcode == ID_SCE && options & LIB_LOAD_LOAD_SCRIPTS) {
		load_datablocks(main_tmp, bpy_openlib, path, ID_TXT);
	}

	/* now do another round of linking for Scenes so all actions are properly loaded */
	if (idcode == ID_SCE && options & LIB_LOAD_LOAD_ACTIONS) {
		load_datablocks(main_tmp, bpy_openlib, path, ID_AC);
	}

	BLO_library_link_end(main_tmp, &bpy_openlib, flag, NULL, NULL);

	BLO_blendhandle_close(bpy_openlib);

	BKE_reports_clear(&reports);
	/* done linking */
	
	/* needed for lookups*/
	GetMainDynamic().push_back(main_newlib);
	BLI_strncpy(main_newlib->name, path, sizeof(main_newlib->name));
	
	
	status = new KX_LibLoadStatus(this, m_ketsjiEngine, scene_merge, path);

	if (idcode == ID_ME) {
		/* Convert all new meshes into BGE meshes */
		ID *mesh;
	
		for (mesh = (ID *)main_newlib->mesh.first; mesh; mesh = (ID *)mesh->next ) {
			if (options & LIB_LOAD_VERBOSE)
				printf("MeshName: %s\n", mesh->name + 2);
			RAS_MeshObject *meshobj = BL_ConvertMesh((Mesh *)mesh, NULL, scene_merge, this, false); // For now only use the libloading option for scenes, which need to handle materials/shaders
			scene_merge->GetLogicManager()->RegisterMeshName(meshobj->GetName(), meshobj);
		}
	}
	else if (idcode == ID_AC) {
		/* Convert all actions */
		ID *action;

		for (action= (ID *)main_newlib->action.first; action; action = (ID *)action->next) {
			if (options & LIB_LOAD_VERBOSE)
				printf("ActionName: %s\n", action->name + 2);
			scene_merge->GetLogicManager()->RegisterActionName(action->name + 2, action);
		}
	}
	else if (idcode == ID_SCE) {
		/* Merge all new linked in scene into the existing one */
		ID *scene;
		// scenes gets deleted by the thread when it's done using it (look in async_convert())
		vector<Scene *> *scenes = (options & LIB_LOAD_ASYNC) ? new vector<Scene *>() : NULL;

		for (scene = (ID *)main_newlib->scene.first; scene; scene = (ID *)scene->next ) {
			if (options & LIB_LOAD_VERBOSE)
				printf("SceneName: %s\n", scene->name + 2);
			
			if (options & LIB_LOAD_ASYNC) {
				scenes->push_back((Scene *)scene);
			} 
			else {
				/* merge into the base  scene */
				KX_Scene* other = m_ketsjiEngine->CreateScene((Scene *)scene, true);
				scene_merge->MergeScene(other);
			
				// RemoveScene(other); // Don't run this, it frees the entire scene converter data, just delete the scene
				delete other;
			}
		}

		if (options & LIB_LOAD_ASYNC) {
			status->SetData(scenes);
			BLI_task_pool_push(m_threadinfo->m_pool, async_convert, (void *)status, false, TASK_PRIORITY_LOW);
		}

#ifdef WITH_PYTHON
		/* Handle any text datablocks */
		if (options & LIB_LOAD_LOAD_SCRIPTS)
			addImportMain(main_newlib);
#endif

		/* Now handle all the actions */
		if (options & LIB_LOAD_LOAD_ACTIONS) {
			ID *action;

			for (action = (ID *)main_newlib->action.first; action; action = (ID *)action->next) {
				if (options & LIB_LOAD_VERBOSE)
					printf("ActionName: %s\n", action->name + 2);
				scene_merge->GetLogicManager()->RegisterActionName(action->name + 2, action);
			}
		}
	}

	if (!(options & LIB_LOAD_ASYNC))
		status->Finish();

//	TIMEIT_END(bge_link_blend_file);

	m_status_map[main_newlib->name] = status;
	return status;
}

/* Note m_map_*** are all ok and don't need to be freed
 * most are temp and NewRemoveObject frees m_map_gameobject_to_blender */
bool KX_BlenderSceneConverter::FreeBlendFile(Main *maggie)
{
	int maggie_index = -1;
	int i = 0;

	if (maggie == NULL)
		return false;

	// If the given library is currently in loading, we do nothing.
	if (m_status_map.count(maggie->name)) {
		BLI_mutex_lock(&m_threadinfo->m_mutex);
		const bool finished = m_status_map[maggie->name]->IsFinished();
		BLI_mutex_unlock(&m_threadinfo->m_mutex);

		if (!finished) {
			printf("Library (%s) is currently being loaded asynchronously, and cannot be freed until this process is done\n", maggie->name);
			return false;
		}
	}

	/* tag all false except the one we remove */
	for (vector<Main *>::iterator it = m_DynamicMaggie.begin(); !(it == m_DynamicMaggie.end()); it++) {
		Main *main = *it;
		if (main != maggie) {
			BKE_main_id_tag_all(main, LIB_TAG_DOIT, false);
		}
		else {
			maggie_index = i;
		}
		i++;
	}

	/* should never happen but just to be safe */
	if (maggie_index == -1)
		return false;

	m_DynamicMaggie.erase(m_DynamicMaggie.begin() + maggie_index);
	BKE_main_id_tag_all(maggie, LIB_TAG_DOIT, true);

	/* free all tagged objects */
	KX_SceneList *scenes = m_ketsjiEngine->CurrentScenes();
	int numScenes = scenes->size();

	for (int scene_idx = 0; scene_idx < numScenes; scene_idx++) {
		KX_Scene *scene = scenes->at(scene_idx);
		if (IS_TAGGED(scene->GetBlenderScene())) {
			m_ketsjiEngine->RemoveScene(scene->GetName());
			m_mat_cache.erase(scene);
			m_polymat_cache.erase(scene);
			scene_idx--;
			numScenes--;
		}
		else {
			/* in case the mesh might be refered to later */
			{
				CTR_Map<STR_HashedString, void *> &mapStringToMeshes = scene->GetLogicManager()->GetMeshMap();
				
				for (int i = 0; i < mapStringToMeshes.size(); i++) {
					RAS_MeshObject *meshobj = (RAS_MeshObject *) *mapStringToMeshes.at(i);
					if (meshobj && IS_TAGGED(meshobj->GetMesh())) {
						STR_HashedString mn = meshobj->GetName();
						mapStringToMeshes.remove(mn);
						m_map_mesh_to_gamemesh.remove(CHashedPtr(meshobj->GetMesh()));
						i--;
					}
				}
			}

			/* Now unregister actions */
			{
				CTR_Map<STR_HashedString, void *> &mapStringToActions = scene->GetLogicManager()->GetActionMap();

				for (int i = 0; i < mapStringToActions.size(); i++) {
					ID *action = (ID*) *mapStringToActions.at(i);

					if (IS_TAGGED(action)) {
						STR_HashedString an = action->name + 2;
						mapStringToActions.remove(an);
						m_map_blender_to_gameAdtList.remove(CHashedPtr(action));
						i--;
					}
				}
			}
			
			//scene->FreeTagged(); /* removed tagged objects and meshes*/
			CListValue *obj_lists[] = {scene->GetObjectList(), scene->GetInactiveList(), NULL};

			for (int ob_ls_idx = 0; obj_lists[ob_ls_idx]; ob_ls_idx++) {
				CListValue *obs = obj_lists[ob_ls_idx];
				RAS_MeshObject *mesh;

				for (int ob_idx = 0; ob_idx < obs->GetCount(); ob_idx++) {
					KX_GameObject *gameobj = (KX_GameObject*)obs->GetValue(ob_idx);
					if (IS_TAGGED(gameobj->GetBlenderObject())) {
						int size_before = obs->GetCount();

						/* Eventually calls RemoveNodeDestructObject
						 * frees m_map_gameobject_to_blender from UnregisterGameObject */
						scene->RemoveObject(gameobj);

						if (size_before != obs->GetCount())
							ob_idx--;
						else {
							printf("ERROR COULD NOT REMOVE \"%s\"\n", gameobj->GetName().ReadPtr());
						}
					}
					else {
						gameobj->RemoveTaggedActions();
						/* free the mesh, we could be referecing a linked one! */
						int mesh_index = gameobj->GetMeshCount();
						while (mesh_index--) {
							mesh = gameobj->GetMesh(mesh_index);
							if (IS_TAGGED(mesh->GetMesh())) {
								gameobj->RemoveMeshes(); /* XXX - slack, should only remove meshes that are library items but mostly objects only have 1 mesh */
								break;
							}
							else {
								/* also free the mesh if it's using a tagged material */
								int mat_index = mesh->NumMaterials();
								while (mat_index--) {
									if (IS_TAGGED(mesh->GetMeshMaterial(mat_index)->m_bucket->GetPolyMaterial()->GetBlenderMaterial())) {
										gameobj->RemoveMeshes(); /* XXX - slack, same as above */
										break;
									}
								}
							}
						}

						/* make sure action actuators are not referencing tagged actions */
						for (unsigned int act_idx = 0; act_idx < gameobj->GetActuators().size(); act_idx++) {
							if (gameobj->GetActuators()[act_idx]->IsType(SCA_IActuator::KX_ACT_ACTION)) {
								BL_ActionActuator *act = (BL_ActionActuator *)gameobj->GetActuators()[act_idx];
								if (IS_TAGGED(act->GetAction()))
									act->SetAction(NULL);
							}
						}
					}
				}
			}
		}
	}

	int size;

	// delete the entities of this scene
	/* TODO - */
#if 0
	vector<pair<KX_Scene*,KX_WorldInfo*> >::iterator worldit;
	size = m_worldinfos.size();
	for (i=0, worldit=m_worldinfos.begin(); i<size; ) {
		if ((*worldit).second) {
			delete (*worldit).second;
			*worldit = m_worldinfos.back();
			m_worldinfos.pop_back();
			size--;
		} else {
			i++;
			worldit++;
		}
	}
#endif


	/* Worlds don't reference original blender data so we need to make a set from them */
	typedef std::set<KX_WorldInfo *> KX_WorldInfoSet;
	KX_WorldInfoSet worldset;
	for (int scene_idx = 0; scene_idx < numScenes; scene_idx++) {
		KX_Scene *scene = scenes->at(scene_idx);
		if (scene->GetWorldInfo())
			worldset.insert(scene->GetWorldInfo());
	}

	vector<pair<KX_Scene *, KX_WorldInfo *> >::iterator worldit;
	size = m_worldinfos.size();
	for (i = 0, worldit = m_worldinfos.begin(); i < size;) {
		if (worldit->second && (worldset.count(worldit->second)) == 0) {
			delete worldit->second;
			*worldit = m_worldinfos.back();
			m_worldinfos.pop_back();
			size--;
		} 
		else {
			i++;
			worldit++;
		}
	}
	worldset.clear();
	/* done freeing the worlds */

	vector<pair<KX_Scene *, RAS_IPolyMaterial *> >::iterator polymit;
	size = m_polymaterials.size();

	for (i = 0, polymit = m_polymaterials.begin(); i < size; ) {
		RAS_IPolyMaterial *mat = polymit->second;
		Material *bmat = NULL;

		KX_BlenderMaterial *bl_mat = static_cast<KX_BlenderMaterial *>(mat);
		bmat = bl_mat->GetBlenderMaterial();

		if (IS_TAGGED(bmat)) {
			/* only remove from bucket */
			polymit->first->GetBucketManager()->RemoveMaterial(mat);
		}

		i++;
		polymit++;
	}

	for (i = 0, polymit = m_polymaterials.begin(); i < size; ) {
		RAS_IPolyMaterial *mat = polymit->second;
		Material *bmat = NULL;

		KX_BlenderMaterial *bl_mat = static_cast<KX_BlenderMaterial*>(mat);
		bmat = bl_mat->GetBlenderMaterial();

		if (IS_TAGGED(bmat)) {
			// Remove the poly material coresponding to this Blender Material.
			m_polymat_cache[polymit->first].erase(bmat);
			delete polymit->second;
			*polymit = m_polymaterials.back();
			m_polymaterials.pop_back();
			size--;
		} else {
			i++;
			polymit++;
		}
	}

	vector<pair<KX_Scene *, BL_Material *> >::iterator matit;
	size = m_materials.size();
	for (i = 0, matit = m_materials.begin(); i < size; ) {
		BL_Material *mat = matit->second;
		if (IS_TAGGED(mat->material)) {
			// Remove the bl material coresponding to this Blender Material.
			m_mat_cache[matit->first].erase(mat->material);
			delete matit->second;
			*matit = m_materials.back();
			m_materials.pop_back();
			size--;
		} 
		else {
			i++;
			matit++;
		}
	}

	vector<pair<KX_Scene *, RAS_MeshObject *> >::iterator meshit;
	RAS_BucketManager::BucketList::iterator bit;
	list<RAS_MeshSlot>::iterator msit;
	RAS_BucketManager::BucketList buckets;

	size = m_meshobjects.size();
	for (i = 0, meshit = m_meshobjects.begin(); i < size;) {
		RAS_MeshObject *me = meshit->second;
		if (IS_TAGGED(me->GetMesh())) {
			// Before deleting the mesh object, make sure the rasterizer is
			// no longer referencing it.
			buckets = meshit->first->GetBucketManager()->GetSolidBuckets();
			for (bit = buckets.begin(); bit != buckets.end(); bit++) {
				msit = (*bit)->msBegin();

				while (msit != (*bit)->msEnd()) {
					if (msit->m_mesh == meshit->second)
						(*bit)->RemoveMesh(&(*msit++));
					else
						msit++;
				}
			}

			// And now the alpha buckets
			buckets = meshit->first->GetBucketManager()->GetAlphaBuckets();
			for (bit = buckets.begin(); bit != buckets.end(); bit++) {
				msit = (*bit)->msBegin();

				while (msit != (*bit)->msEnd()) {
					if (msit->m_mesh == meshit->second)
						(*bit)->RemoveMesh(&(*msit++));
					else
						msit++;
				}
			}

			// Now it should be safe to delete
			delete meshit->second;
			*meshit = m_meshobjects.back();
			m_meshobjects.pop_back();
			size--;
		} 
		else {
			i++;
			meshit++;
		}
	}

#ifdef WITH_PYTHON
	/* make sure this maggie is removed from the import list if it's there
	 * (this operation is safe if it isn't in the list) */
	removeImportMain(maggie);
#endif

	delete m_status_map[maggie->name];
	m_status_map.erase(maggie->name);

	BKE_main_free(maggie);

	return true;
}

bool KX_BlenderSceneConverter::FreeBlendFile(const char *path)
{
	return FreeBlendFile(GetMainDynamicPath(path));
}

bool KX_BlenderSceneConverter::MergeScene(KX_Scene *to, KX_Scene *from)
{
	{
		vector<pair<KX_Scene *, KX_WorldInfo *> >::iterator itp = m_worldinfos.begin();
		while (itp != m_worldinfos.end()) {
			if (itp->first == from)
				itp->first = to;
			itp++;
		}
	}

	{
		vector<pair<KX_Scene *, RAS_IPolyMaterial *> >::iterator itp = m_polymaterials.begin();
		while (itp != m_polymaterials.end()) {
			if (itp->first == from) {
				itp->first = to;

				/* also switch internal data */
				RAS_IPolyMaterial *mat = itp->second;
				mat->Replace_IScene(to);
			}
			itp++;
		}
	}

	{
		vector<pair<KX_Scene *, RAS_MeshObject *> >::iterator itp = m_meshobjects.begin();
		while (itp != m_meshobjects.end()) {
			if (itp->first == from)
				itp->first = to;
			itp++;
		}
	}

	{
		vector<pair<KX_Scene *, BL_Material *> >::iterator itp = m_materials.begin();
		while (itp != m_materials.end()) {
			if (itp->first == from)
				itp->first = to;
			itp++;
		}
	}

	MaterialCache::iterator matcacheit = m_mat_cache.find(from);
	if (matcacheit != m_mat_cache.end()) {
		// Merge cached BL_Material map.
		m_mat_cache[to].insert(matcacheit->second.begin(), matcacheit->second.end());
		m_mat_cache.erase(matcacheit);
	}

	PolyMaterialCache::iterator polymatcacheit = m_polymat_cache.find(from);
	if (polymatcacheit != m_polymat_cache.end()) {
		// Merge cached RAS_IPolyMaterial map.
		m_polymat_cache[to].insert(polymatcacheit->second.begin(), polymatcacheit->second.end());
		m_polymat_cache.erase(polymatcacheit);
	}

	return true;
}

/* This function merges a mesh from the current scene into another main
 * it does not convert */
RAS_MeshObject *KX_BlenderSceneConverter::ConvertMeshSpecial(KX_Scene *kx_scene, Main *maggie, const char *name)
{
	/* Find a mesh in the current main */
	ID *me= static_cast<ID *>(BLI_findstring(&m_maggie->mesh, name, offsetof(ID, name) + 2));
	Main *from_maggie = m_maggie;

	if (me == NULL) {
		// The mesh wasn't in the current main, try any dynamic (i.e., LibLoaded) ones
		vector<Main *>::iterator it;

		for (it = GetMainDynamic().begin(); it != GetMainDynamic().end(); it++) {
			me = static_cast<ID *>(BLI_findstring(&(*it)->mesh, name, offsetof(ID, name) + 2));
			from_maggie = *it;

			if (me)
				break;
		}
	}

	if (me == NULL) {
		printf("Could not be found \"%s\"\n", name);
		return NULL;
	}

	/* Watch this!, if its used in the original scene can cause big troubles */
	if (me->us > 0) {
#ifdef DEBUG
		printf("Mesh has a user \"%s\"\n", name);
#endif
		me = (ID*)BKE_mesh_copy(from_maggie, (Mesh*)me);
		id_us_min(me);
	}
	BLI_remlink(&from_maggie->mesh, me); /* even if we made the copy it needs to be removed */
	BLI_addtail(&maggie->mesh, me);

	/* Must copy the materials this uses else we cant free them */
	{
		Mesh *mesh = (Mesh *)me;

		/* ensure all materials are tagged */
		for (int i = 0; i < mesh->totcol; i++) {
			if (mesh->mat[i])
				mesh->mat[i]->id.tag &= ~LIB_TAG_DOIT;
		}

		for (int i = 0; i < mesh->totcol; i++) {
			Material *mat_old = mesh->mat[i];

			/* if its tagged its a replaced material */
			if (mat_old && (mat_old->id.tag & LIB_TAG_DOIT) == 0) {
				Material *mat_old = mesh->mat[i];
				Material *mat_new = BKE_material_copy(from_maggie, mat_old);

				mat_new->id.tag |= LIB_TAG_DOIT;
				id_us_min(&mat_old->id);

				BLI_remlink(&from_maggie->mat, mat_new); // BKE_material_copy uses G.main, and there is no BKE_material_copy_ex
				BLI_addtail(&maggie->mat, mat_new);

				mesh->mat[i] = mat_new;

				/* the same material may be used twice */
				for (int j = i + 1; j < mesh->totcol; j++) {
					if (mesh->mat[j] == mat_old) {
						mesh->mat[j] = mat_new;
						id_us_plus(&mat_new->id);
						id_us_min(&mat_old->id);
					}
				}
			}
		}
	}

	m_currentScene = kx_scene; // This needs to be set in case we LibLoaded earlier
	RAS_MeshObject *meshobj = BL_ConvertMesh((Mesh *)me, NULL, kx_scene, this, false);
	kx_scene->GetLogicManager()->RegisterMeshName(meshobj->GetName(),meshobj);
	m_map_mesh_to_gamemesh.clear(); /* This is at runtime so no need to keep this, BL_ConvertMesh adds */
	return meshobj;
}
