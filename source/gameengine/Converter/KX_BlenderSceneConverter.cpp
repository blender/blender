/**
 * $Id$
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

#ifdef WIN32
	#pragma warning (disable:4786) // suppress stl-MSVC debug info warning
#endif

#include "KX_Scene.h"
#include "KX_GameObject.h"
#include "KX_BlenderSceneConverter.h"
#include "KX_IpoConvert.h"
#include "RAS_MeshObject.h"
#include "KX_PhysicsEngineEnums.h"
#include "PHY_IPhysicsEnvironment.h"
#include "KX_KetsjiEngine.h"
#include "KX_IPhysicsController.h"
#include "BL_Material.h"
#include "SYS_System.h"

#include "DummyPhysicsEnvironment.h"

//to decide to use sumo/ode or dummy physics - defines USE_ODE
#include "KX_ConvertPhysicsObject.h"

#ifdef USE_BULLET
#include "CcdPhysicsEnvironment.h"
#endif

#ifdef USE_ODE
#include "OdePhysicsEnvironment.h"
#endif //USE_ODE

#ifdef USE_SUMO_SOLID
#include "SumoPhysicsEnvironment.h"
#endif

#include "KX_BlenderSceneConverter.h"
#include "KX_BlenderScalarInterpolator.h"
#include "BL_BlenderDataConversion.h"
#include "BlenderWorldInfo.h"
#include "KX_Scene.h"

/* This little block needed for linking to Blender... */
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

/* This list includes only data type definitions */
#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "BKE_main.h"

#include "BLI_arithb.h"

extern "C"
{
#include "DNA_object_types.h"
#include "DNA_curve_types.h"
#include "BLI_blenlib.h"
#include "MEM_guardedalloc.h"
#include "BSE_editipo.h"
#include "BSE_editipo_types.h"
#include "DNA_ipo_types.h"
#include "BKE_global.h"
#include "DNA_space_types.h"
}


KX_BlenderSceneConverter::KX_BlenderSceneConverter(
							struct Main* maggie,
							struct SpaceIpo*	sipo,
							class KX_KetsjiEngine* engine
							)
							: m_maggie(maggie),
							m_sipo(sipo),
							m_ketsjiEngine(engine),
							m_alwaysUseExpandFraming(false),
							m_usemat(false),
							m_useglslmat(false)
{
	m_newfilename = "";
}


KX_BlenderSceneConverter::~KX_BlenderSceneConverter()
{
	// clears meshes, and hashmaps from blender to gameengine data
	int i;
	// delete sumoshapes
	

	int numipolists = m_map_blender_to_gameipolist.size();
	for (i=0; i<numipolists; i++) {
		BL_InterpolatorList *ipoList= *m_map_blender_to_gameipolist.at(i);

		delete (ipoList);
	}

	vector<pair<KX_Scene*,KX_WorldInfo*> >::iterator itw = m_worldinfos.begin();
	while (itw != m_worldinfos.end()) {
		delete (*itw).second;
		itw++;
	}

	vector<pair<KX_Scene*,RAS_IPolyMaterial*> >::iterator itp = m_polymaterials.begin();
	while (itp != m_polymaterials.end()) {
		delete (*itp).second;
		itp++;
	}

	// delete after RAS_IPolyMaterial
	vector<pair<KX_Scene*,BL_Material *> >::iterator itmat = m_materials.begin();
	while (itmat != m_materials.end()) {
		delete (*itmat).second;
		itmat++;
	}	


	vector<pair<KX_Scene*,RAS_MeshObject*> >::iterator itm = m_meshobjects.begin();
	while (itm != m_meshobjects.end()) {
		delete (*itm).second;
		itm++;
	}
	
#ifdef USE_SUMO_SOLID
	KX_ClearSumoSharedShapes();
#endif

#ifdef USE_BULLET
	KX_ClearBulletSharedShapes();
#endif

}



void KX_BlenderSceneConverter::SetNewFileName(const STR_String& filename)
{
	m_newfilename = filename;
}



bool KX_BlenderSceneConverter::TryAndLoadNewFile()
{
	bool result = false;

	// find the file
/*	if ()
	{
		result = true;
	}
	// if not, clear the newfilename
	else
	{
		m_newfilename = "";	
	}
*/
	return result;
}

Scene *KX_BlenderSceneConverter::GetBlenderSceneForName(const STR_String& name)
{
	Scene *sce;

	/**
	 * Find the specified scene by name, or the first
	 * scene if nothing matches (shouldn't happen).
	 */

	for (sce= (Scene*) m_maggie->scene.first; sce; sce= (Scene*) sce->id.next)
		if (name == (sce->id.name+2))
			return sce;

	return (Scene*)m_maggie->scene.first;

}
#include "KX_PythonInit.h"

#ifdef USE_BULLET

#include "LinearMath/btIDebugDraw.h"


struct	BlenderDebugDraw : public btIDebugDraw
{
	BlenderDebugDraw () :
		m_debugMode(0) 
	{
	}
	
	int m_debugMode;

	virtual void	drawLine(const btVector3& from,const btVector3& to,const btVector3& color)
	{
		if (m_debugMode >0)
		{
			MT_Vector3 kxfrom(from[0],from[1],from[2]);
			MT_Vector3 kxto(to[0],to[1],to[2]);
			MT_Vector3 kxcolor(color[0],color[1],color[2]);

			KX_RasterizerDrawDebugLine(kxfrom,kxto,kxcolor);
		}
	}
	
	virtual void	reportErrorWarning(const char* warningString)
	{

	}

	virtual void	drawContactPoint(const btVector3& PointOnB,const btVector3& normalOnB,float distance,int lifeTime,const btVector3& color)
	{
		//not yet
	}

	virtual void	setDebugMode(int debugMode)
	{
		m_debugMode = debugMode;
	}
	virtual int		getDebugMode() const
	{
		return m_debugMode;
	}
		
};

#endif

void KX_BlenderSceneConverter::ConvertScene(const STR_String& scenename,
											class KX_Scene* destinationscene,
											PyObject* dictobj,
											class SCA_IInputDevice* keyinputdev,
											class RAS_IRenderTools* rendertools,
											class RAS_ICanvas* canvas)
{
	//find out which physics engine
	Scene *blenderscene = GetBlenderSceneForName(scenename);

	e_PhysicsEngine physics_engine = UseBullet;
	// hook for registration function during conversion.
	m_currentScene = destinationscene;
	destinationscene->SetSceneConverter(this);

	if (blenderscene)
	{
	
		if (blenderscene->world)
		{
			switch (blenderscene->world->physicsEngine)
			{
			case WOPHY_BULLET:
				{
					physics_engine = UseBullet;
					break;
				}
                                
				case WOPHY_ODE:
				{
					physics_engine = UseODE;
					break;
				}
				case WOPHY_DYNAMO:
				{
					physics_engine = UseDynamo;
					break;
				}
				case WOPHY_SUMO:
				{
					physics_engine = UseSumo; 
					break;
				}
				case WOPHY_NONE:
				{
					physics_engine = UseNone;
				}
			}
		  
		}
	}

	switch (physics_engine)
	{
#ifdef USE_BULLET
		case UseBullet:
			{
				CcdPhysicsEnvironment* ccdPhysEnv = new CcdPhysicsEnvironment();
				ccdPhysEnv->setDebugDrawer(new BlenderDebugDraw());
				ccdPhysEnv->setDeactivationLinearTreshold(0.8f); // default, can be overridden by Python
				ccdPhysEnv->setDeactivationAngularTreshold(1.0f); // default, can be overridden by Python

				SYS_SystemHandle syshandle = SYS_GetSystem(); /*unused*/
				int visualizePhysics = SYS_GetCommandLineInt(syshandle,"show_physics",0);
				if (visualizePhysics)
					ccdPhysEnv->setDebugMode(btIDebugDraw::DBG_DrawWireframe|btIDebugDraw::DBG_DrawAabb|btIDebugDraw::DBG_DrawContactPoints|btIDebugDraw::DBG_DrawText);
		
				//todo: get a button in blender ?
				//disable / enable debug drawing (contact points, aabb's etc)	
				//ccdPhysEnv->setDebugMode(1);
				destinationscene->SetPhysicsEnvironment(ccdPhysEnv);
				break;
			}
#endif

#ifdef USE_SUMO_SOLID
		case UseSumo:
			destinationscene ->SetPhysicsEnvironment(new SumoPhysicsEnvironment());
			break;
#endif
#ifdef USE_ODE

		case UseODE:
			destinationscene ->SetPhysicsEnvironment(new ODEPhysicsEnvironment());
			break;
#endif //USE_ODE
	
		case UseDynamo:
		{
		}
		
		default:
		case UseNone:
			physics_engine = UseNone;
			destinationscene ->SetPhysicsEnvironment(new DummyPhysicsEnvironment());
			break;
	}

	BL_ConvertBlenderObjects(m_maggie,
		scenename,
		destinationscene,
		m_ketsjiEngine,
		physics_engine,
		dictobj,
		keyinputdev,
		rendertools,
		canvas,
		this,
		m_alwaysUseExpandFraming
		);

	//These lookup are not needed during game
	m_map_blender_to_gameactuator.clear();
	m_map_blender_to_gamecontroller.clear();
	m_map_blender_to_gameobject.clear();

	//Clearing this lookup table has the effect of disabling the cache of meshes
	//between scenes, even if they are shared in the blend file.
	//This cache mecanism is buggy so I leave it disable and the memory leak
	//that would result from this is fixed in RemoveScene()
	m_map_mesh_to_gamemesh.clear();
	//Don't clear this lookup, it is needed for the baking physics into ipo animation
	//To avoid it's infinite grows, object will be unregister when they are deleted 
	//see KX_Scene::NewRemoveObject
	//m_map_gameobject_to_blender.clear();
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
	vector<pair<KX_Scene*,KX_WorldInfo*> >::iterator worldit;
	size = m_worldinfos.size();
	for (i=0, worldit=m_worldinfos.begin(); i<size; ) {
		if ((*worldit).first == scene) {
			delete (*worldit).second;
			*worldit = m_worldinfos.back();
			m_worldinfos.pop_back();
			size--;
		} else {
			i++;
			worldit++;
		}
	}

	vector<pair<KX_Scene*,RAS_IPolyMaterial*> >::iterator polymit;
	size = m_polymaterials.size();
	for (i=0, polymit=m_polymaterials.begin(); i<size; ) {
		if ((*polymit).first == scene) {
			delete (*polymit).second;
			*polymit = m_polymaterials.back();
			m_polymaterials.pop_back();
			size--;
		} else {
			i++;
			polymit++;
		}
	}

	vector<pair<KX_Scene*,BL_Material*> >::iterator matit;
	size = m_materials.size();
	for (i=0, matit=m_materials.begin(); i<size; ) {
		if ((*matit).first == scene) {
			delete (*matit).second;
			*matit = m_materials.back();
			m_materials.pop_back();
			size--;
		} else {
			i++;
			matit++;
		}
	}

	vector<pair<KX_Scene*,RAS_MeshObject*> >::iterator meshit;
	size = m_meshobjects.size();
	for (i=0, meshit=m_meshobjects.begin(); i<size; ) {
		if ((*meshit).first == scene) {
			delete (*meshit).second;
			*meshit = m_meshobjects.back();
			m_meshobjects.pop_back();
			size--;
		} else {
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

bool KX_BlenderSceneConverter::GetMaterials()
{
	return m_usemat;
}

bool KX_BlenderSceneConverter::GetGLSLMaterials()
{
	return m_useglslmat;
}

void KX_BlenderSceneConverter::RegisterBlenderMaterial(BL_Material *mat)
{
	m_materials.push_back(pair<KX_Scene*,BL_Material *>(m_currentScene,mat));
}



void KX_BlenderSceneConverter::SetAlwaysUseExpandFraming(
	bool to_what)
{
	m_alwaysUseExpandFraming= to_what;
}

	

void KX_BlenderSceneConverter::RegisterGameObject(
									KX_GameObject *gameobject, 
									struct Object *for_blenderobject) 
{
	m_map_gameobject_to_blender.insert(CHashedPtr(gameobject),for_blenderobject);
	m_map_blender_to_gameobject.insert(CHashedPtr(for_blenderobject),gameobject);
}

void KX_BlenderSceneConverter::UnregisterGameObject(
									KX_GameObject *gameobject) 
{
	m_map_gameobject_to_blender.remove(CHashedPtr(gameobject));
}


KX_GameObject *KX_BlenderSceneConverter::FindGameObject(
									struct Object *for_blenderobject) 
{
	KX_GameObject **obp= m_map_blender_to_gameobject[CHashedPtr(for_blenderobject)];
	
	return obp?*obp:NULL;
}



struct Object *KX_BlenderSceneConverter::FindBlenderObject(
									KX_GameObject *for_gameobject) 
{
	struct Object **obp= m_map_gameobject_to_blender[CHashedPtr(for_gameobject)];
	
	return obp?*obp:NULL;
}

	

void KX_BlenderSceneConverter::RegisterGameMesh(
									RAS_MeshObject *gamemesh,
									struct Mesh *for_blendermesh)
{
	m_map_mesh_to_gamemesh.insert(CHashedPtr(for_blendermesh),gamemesh);
	m_meshobjects.push_back(pair<KX_Scene*,RAS_MeshObject*>(m_currentScene,gamemesh));
}



RAS_MeshObject *KX_BlenderSceneConverter::FindGameMesh(
									struct Mesh *for_blendermesh,
									unsigned int onlayer)
{
	RAS_MeshObject** meshp = m_map_mesh_to_gamemesh[CHashedPtr(for_blendermesh)];
	
	if (meshp && onlayer==(*meshp)->GetLightLayer()) {
		return *meshp;
	} else {
		return NULL;
	}
}

	


	

void KX_BlenderSceneConverter::RegisterPolyMaterial(RAS_IPolyMaterial *polymat)
{
	m_polymaterials.push_back(pair<KX_Scene*,RAS_IPolyMaterial*>(m_currentScene,polymat));
}



void KX_BlenderSceneConverter::RegisterInterpolatorList(
									BL_InterpolatorList *ipoList,
									struct Ipo *for_ipo)
{
	m_map_blender_to_gameipolist.insert(CHashedPtr(for_ipo), ipoList);
}



BL_InterpolatorList *KX_BlenderSceneConverter::FindInterpolatorList(
									struct Ipo *for_ipo)
{
	BL_InterpolatorList **listp = m_map_blender_to_gameipolist[CHashedPtr(for_ipo)];
		
	return listp?*listp:NULL;
}



void KX_BlenderSceneConverter::RegisterGameActuator(
									SCA_IActuator *act,
									struct bActuator *for_actuator)
{
	m_map_blender_to_gameactuator.insert(CHashedPtr(for_actuator), act);
}



SCA_IActuator *KX_BlenderSceneConverter::FindGameActuator(
									struct bActuator *for_actuator)
{
	SCA_IActuator **actp = m_map_blender_to_gameactuator[CHashedPtr(for_actuator)];
	
	return actp?*actp:NULL;
}



void KX_BlenderSceneConverter::RegisterGameController(
									SCA_IController *cont,
									struct bController *for_controller)
{
	m_map_blender_to_gamecontroller.insert(CHashedPtr(for_controller), cont);
}



SCA_IController *KX_BlenderSceneConverter::FindGameController(
									struct bController *for_controller)
{
	SCA_IController **contp = m_map_blender_to_gamecontroller[CHashedPtr(for_controller)];
	
	return contp?*contp:NULL;
}



void KX_BlenderSceneConverter::RegisterWorldInfo(
									KX_WorldInfo *worldinfo)
{
	m_worldinfos.push_back(pair<KX_Scene*,KX_WorldInfo*>(m_currentScene,worldinfo));
}

/*
 * When deleting an IPO curve from Python, check if the IPO is being
 * edited and if so clear the pointer to the old curve.
 */
void KX_BlenderSceneConverter::localDel_ipoCurve ( IpoCurve * icu ,struct SpaceIpo*	sipo)
{
	if (!sipo)
		return;

	int i;
	EditIpo *ei= (EditIpo *)sipo->editipo;
	if (!ei) return;

	for(i=0; i<G.sipo->totipo; i++, ei++) {
                if ( ei->icu == icu ) {
			ei->flag &= ~(IPO_SELECT | IPO_EDIT);
			ei->icu= 0;
			return;
		}
	}
}

//quick hack
extern "C"
{
	Ipo *add_ipo( char *name, int idcode );
	char *getIpoCurveName( IpoCurve * icu );
	struct IpoCurve *verify_ipocurve(struct ID *, short, char *, char *, char *, int);
	void testhandles_ipocurve(struct IpoCurve *icu);
	void Mat3ToEul(float tmat[][3], float *eul);

}

IpoCurve* findIpoCurve(IpoCurve* first,char* searchName)
{
	IpoCurve* icu1;
	for( icu1 = first; icu1; icu1 = icu1->next ) 
	{
		char* curveName = getIpoCurveName( icu1 );
		if( !strcmp( curveName, searchName) )
		{
			return icu1;
		}
	}
	return 0;
}

// this is not longer necesary //rcruiz
/*Ipo* KX_BlenderSceneConverter::findIpoForName(char* objName)
{
	Ipo* ipo_iter = (Ipo*)m_maggie->ipo.first;

	while( ipo_iter )
	{
		if( strcmp( objName, ipo_iter->id.name + 2 ) == 0 ) 
		{
			return ipo_iter;
		}
		ipo_iter = (Ipo*)ipo_iter->id.next;
	}
	return 0;
}
*/

void	KX_BlenderSceneConverter::ResetPhysicsObjectsAnimationIpo(bool clearIpo)
{

	KX_SceneList* scenes = m_ketsjiEngine->CurrentScenes();
	int numScenes = scenes->size();
	int i;
	for (i=0;i<numScenes;i++)
	{
		KX_Scene* scene = scenes->at(i);
		//PHY_IPhysicsEnvironment* physEnv = scene->GetPhysicsEnvironment();
		CListValue* parentList = scene->GetRootParentList();
		int numObjects = parentList->GetCount();
		int g;
		for (g=0;g<numObjects;g++)
		{
			KX_GameObject* gameObj = (KX_GameObject*)parentList->GetValue(g);
			if (gameObj->IsDynamic())
			{
				//KX_IPhysicsController* physCtrl = gameObj->GetPhysicsController();
				
				Object* blenderObject = FindBlenderObject(gameObj);
				if (blenderObject)
				{
					//erase existing ipo's
					Ipo* ipo = blenderObject->ipo;//findIpoForName(blenderObject->id.name+2);
					if (ipo)
					{ 	//clear the curve data
						if (clearIpo){//rcruiz
							IpoCurve *icu1;
														
							int numCurves = 0;
							for( icu1 = (IpoCurve*)ipo->curve.first; icu1;  ) {
							
								IpoCurve* tmpicu = icu1;
								
								/*int i;
								BezTriple *bezt;
								for( bezt = tmpicu->bezt, i = 0;	i < tmpicu->totvert; i++, bezt++){
									printf("(%f,%f,%f),(%f,%f,%f),(%f,%f,%f)\n",bezt->vec[0][0],bezt->vec[0][1],bezt->vec[0][2],bezt->vec[1][0],bezt->vec[1][1],bezt->vec[1][2],bezt->vec[2][0],bezt->vec[2][1],bezt->vec[2][2]);
								}*/
								
								icu1 = icu1->next;
								numCurves++;
			
								BLI_remlink( &( blenderObject->ipo->curve ), tmpicu );
								if( tmpicu->bezt )
									MEM_freeN( tmpicu->bezt );
								MEM_freeN( tmpicu );
								localDel_ipoCurve( tmpicu ,m_sipo);
							}
					  	}
					} else
					{	ipo = add_ipo(blenderObject->id.name+2, ID_OB);
						blenderObject->ipo = ipo;

					}
				
					

					

				}
			}

		}
		
	
	}



}

void	KX_BlenderSceneConverter::resetNoneDynamicObjectToIpo(){
	
	if (addInitFromFrame){		
		KX_SceneList* scenes = m_ketsjiEngine->CurrentScenes();
		int numScenes = scenes->size();
		if (numScenes>=0){
			KX_Scene* scene = scenes->at(0);
			CListValue* parentList = scene->GetRootParentList();
			for (int ix=0;ix<parentList->GetCount();ix++){
				KX_GameObject* gameobj = (KX_GameObject*)parentList->GetValue(ix);
				if (!gameobj->IsDynamic()){
					Object* blenderobject = FindBlenderObject(gameobj);
					if (!blenderobject)
						continue;
					if (blenderobject->type==OB_ARMATURE)
						continue;
					float eu[3];
					Mat4ToEul(blenderobject->obmat,eu);					
					MT_Point3 pos = MT_Point3(
						blenderobject->obmat[3][0],
						blenderobject->obmat[3][1],
						blenderobject->obmat[3][2]
					);
					MT_Vector3 eulxyz = MT_Vector3(
						eu[0],
						eu[1],
						eu[2]
					);
					MT_Vector3 scale = MT_Vector3(
						blenderobject->size[0],
						blenderobject->size[1],
						blenderobject->size[2]
					);
					gameobj->NodeSetLocalPosition(pos);
					gameobj->NodeSetLocalOrientation(MT_Matrix3x3(eulxyz));
					gameobj->NodeSetLocalScale(scale);
					gameobj->NodeUpdateGS(0,true);
				}
			}
		}
	}
}

#define TEST_HANDLES_GAME2IPO 0

	///this generates ipo curves for position, rotation, allowing to use game physics in animation
void	KX_BlenderSceneConverter::WritePhysicsObjectToAnimationIpo(int frameNumber)
{

	KX_SceneList* scenes = m_ketsjiEngine->CurrentScenes();
	int numScenes = scenes->size();
	int i;
	for (i=0;i<numScenes;i++)
	{
		KX_Scene* scene = scenes->at(i);
		//PHY_IPhysicsEnvironment* physEnv = scene->GetPhysicsEnvironment();
		CListValue* parentList = scene->GetRootParentList();
		int numObjects = parentList->GetCount();
		int g;
		for (g=0;g<numObjects;g++)
		{
			KX_GameObject* gameObj = (KX_GameObject*)parentList->GetValue(g);
			if (gameObj->IsDynamic())
			{
				//KX_IPhysicsController* physCtrl = gameObj->GetPhysicsController();
				
				Object* blenderObject = FindBlenderObject(gameObj);
				if (blenderObject)
				{

					const MT_Matrix3x3& orn = gameObj->NodeGetWorldOrientation();
					float eulerAngles[3];	
					float tmat[3][3];
					for (int r=0;r<3;r++)
					{
						for (int c=0;c<3;c++)
						{
							tmat[r][c] = orn[c][r];
						}
					}
					Mat3ToEul(tmat, eulerAngles);
					
					for(int x = 0; x < 3; x++) {
						eulerAngles[x] *= (float) (180 / 3.14159265f);
					}

					eulerAngles[0]/=10.f;
					eulerAngles[1]/=10.f;
					eulerAngles[2]/=10.f;



					//const MT_Vector3& scale = gameObj->NodeGetWorldScaling();
					const MT_Point3& position = gameObj->NodeGetWorldPosition();
					
					Ipo* ipo = blenderObject->ipo;
					if (ipo)
					{

						//create the curves, if not existing

					IpoCurve *icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"LocX");
					if (!icu1)
						icu1 = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_LOC_X);
					
					icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"LocY");
					if (!icu1)
						icu1 = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_LOC_Y);
					
					icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"LocZ");
					if (!icu1)
						icu1 = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_LOC_Z);

					icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"RotX");
					if (!icu1)
						icu1 = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_ROT_X);

					icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"RotY");
					if (!icu1)
						icu1 = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_ROT_Y);

					icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"RotZ");
					if (!icu1)
						icu1 = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_ROT_Z);



					//fill the curves with data

						icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"LocX");
						if (icu1)
						{
							float curVal = position.x();
							insert_vert_icu(icu1, frameNumber, curVal, 0);
#ifdef TEST_HANDLES_GAME2IPO
							testhandles_ipocurve(icu1);
#endif
						}
						icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"LocY");
						if (icu1)
						{
							float curVal = position.y();
							insert_vert_icu(icu1, frameNumber, curVal, 0);
#ifdef TEST_HANDLES_GAME2IPO

							testhandles_ipocurve(icu1);
#endif
						}
						icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"LocZ");
						if (icu1)
						{
							float curVal = position.z();
							insert_vert_icu(icu1, frameNumber, curVal, 0);
#ifdef TEST_HANDLES_GAME2IPO
							testhandles_ipocurve(icu1);
#endif
						}
						icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"RotX");
						if (icu1)
						{
							float curVal = eulerAngles[0];
							insert_vert_icu(icu1, frameNumber, curVal, 0);
#ifdef TEST_HANDLES_GAME2IPO

							testhandles_ipocurve(icu1);
#endif
						}
						icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"RotY");
						if (icu1)
						{
							float curVal = eulerAngles[1];
							insert_vert_icu(icu1, frameNumber, curVal, 0);
#ifdef TEST_HANDLES_GAME2IPO

							testhandles_ipocurve(icu1);
#endif
						}
						icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"RotZ");
						if (icu1)
						{
							float curVal = eulerAngles[2];
							insert_vert_icu(icu1, frameNumber, curVal, 0);
#ifdef TEST_HANDLES_GAME2IPO
							
							testhandles_ipocurve(icu1);
#endif

						}

					}
				}
			}

		}
		
	
	}	
	

}


void	KX_BlenderSceneConverter::TestHandlesPhysicsObjectToAnimationIpo()
{

	KX_SceneList* scenes = m_ketsjiEngine->CurrentScenes();
	int numScenes = scenes->size();
	int i;
	for (i=0;i<numScenes;i++)
	{
		KX_Scene* scene = scenes->at(i);
		//PHY_IPhysicsEnvironment* physEnv = scene->GetPhysicsEnvironment();
		CListValue* parentList = scene->GetRootParentList();
		int numObjects = parentList->GetCount();
		int g;
		for (g=0;g<numObjects;g++)
		{
			KX_GameObject* gameObj = (KX_GameObject*)parentList->GetValue(g);
			if (gameObj->IsDynamic())
			{
				//KX_IPhysicsController* physCtrl = gameObj->GetPhysicsController();
				
				Object* blenderObject = FindBlenderObject(gameObj);
				if (blenderObject)
				{

					const MT_Matrix3x3& orn = gameObj->NodeGetWorldOrientation();
					float eulerAngles[3];	
					float tmat[3][3];
					for (int r=0;r<3;r++)
					{
						for (int c=0;c<3;c++)
						{
							tmat[r][c] = orn[c][r];
						}
					}
					Mat3ToEul(tmat, eulerAngles);
					
					for(int x = 0; x < 3; x++) {
						eulerAngles[x] *= (float) (180 / 3.14159265f);
					}

					eulerAngles[0]/=10.f;
					eulerAngles[1]/=10.f;
					eulerAngles[2]/=10.f;



					//const MT_Vector3& scale = gameObj->NodeGetWorldScaling();
					//const MT_Point3& position = gameObj->NodeGetWorldPosition();
					
					Ipo* ipo = blenderObject->ipo;
					if (ipo)
					{

						//create the curves, if not existing

					IpoCurve *icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"LocX");
					if (!icu1)
						icu1 = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_LOC_X);
					
					icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"LocY");
					if (!icu1)
						icu1 = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_LOC_Y);
					
					icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"LocZ");
					if (!icu1)
						icu1 = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_LOC_Z);

					icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"RotX");
					if (!icu1)
						icu1 = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_ROT_X);

					icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"RotY");
					if (!icu1)
						icu1 = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_ROT_Y);

					icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"RotZ");
					if (!icu1)
						icu1 = verify_ipocurve(&blenderObject->id, ipo->blocktype, NULL, NULL, NULL, OB_ROT_Z);



					//fill the curves with data

						icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"LocX");
						if (icu1)
						{
							testhandles_ipocurve(icu1);
						}
						icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"LocY");
						if (icu1)
						{
							testhandles_ipocurve(icu1);
						}
						icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"LocZ");
						if (icu1)
						{
							testhandles_ipocurve(icu1);
						}
						icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"RotX");
						if (icu1)
						{
							testhandles_ipocurve(icu1);
						}
						icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"RotY");
						if (icu1)
						{
							testhandles_ipocurve(icu1);
						}
						icu1 = findIpoCurve((IpoCurve *)ipo->curve.first,"RotZ");
						if (icu1)
						{
							testhandles_ipocurve(icu1);
						}

					}
				}
			}

		}
		
	
	}



}
