/**
 * $Id$
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32

	#pragma warning (disable:4786) // suppress stl-MSVC debug info warning
#endif


#include "KX_Scene.h"
#include "KX_GameObject.h"
#include "KX_IpoConvert.h"
#include "RAS_MeshObject.h"
#include "KX_PhysicsEngineEnums.h"

#include "DummyPhysicsEnvironment.h"

//to decide to use sumo/ode or dummy physics - defines USE_ODE
#include "KX_ConvertPhysicsObject.h"

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


KX_BlenderSceneConverter::KX_BlenderSceneConverter(
							struct Main* maggie,
							class KX_KetsjiEngine* engine
							)
							: m_maggie(maggie),
							m_ketsjiEngine(engine),
							m_alwaysUseExpandFraming(false)
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

	vector<KX_WorldInfo*>::iterator itw = m_worldinfos.begin();
	while (itw != m_worldinfos.end()) {
		delete (*itw);
		itw++;
	}

	vector<RAS_IPolyMaterial*>::iterator itp = m_polymaterials.begin();
	while (itp != m_polymaterials.end()) {
		delete (*itp);
		itp++;
	}
	
	vector<RAS_MeshObject*>::iterator itm = m_meshobjects.begin();
	while (itm != m_meshobjects.end()) {
		delete (*itm);
		itm++;
	}
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



	/**
	 * Find the specified scene by name, or the first
	 * scene if nothing matches (shouldn't happen).
	 */
static struct Scene *GetSceneForName2(struct Main *maggie, const STR_String& scenename) {
	Scene *sce;

	for (sce= (Scene*) maggie->scene.first; sce; sce= (Scene*) sce->id.next)
		if (scenename == (sce->id.name+2))
			return sce;

	return (Scene*) maggie->scene.first;
}


void KX_BlenderSceneConverter::ConvertScene(const STR_String& scenename,
											class KX_Scene* destinationscene,
											PyObject* dictobj,
											class SCA_IInputDevice* keyinputdev,
											class RAS_IRenderTools* rendertools,
											class RAS_ICanvas* canvas)
{
	//find out which physics engine
	Scene *blenderscene = GetSceneForName2(m_maggie, scenename);

	e_PhysicsEngine physics_engine = UseSumo;

	if (blenderscene)
	{
		int i=0;
		
		if (blenderscene->world)
		{
			
			switch (blenderscene->world->physicsEngine)
			{
				
			case 1:
				{
					physics_engine = UseNone;
					break;
				};
			case 2:
				{
					physics_engine = UseSumo;
					break;
				}
			case 3:
				{
					physics_engine = UseODE;
					break;
				}
			case 4:
				{
					physics_engine = UseDynamo;
					break;
				}
			default:
				{
					physics_engine = UseODE;
				}
			}
		}
	}

	switch (physics_engine)
	{

	case UseSumo:
		{
#ifdef USE_SUMO_SOLID

			PHY_IPhysicsEnvironment* physEnv = 
				new SumoPhysicsEnvironment();
#else
			physics_engine = UseNone;
			
			PHY_IPhysicsEnvironment* physEnv = 
				new DummyPhysicsEnvironment();

#endif
			destinationscene ->SetPhysicsEnvironment(physEnv);
			break;
		}
	case UseODE:
		{
#ifdef USE_ODE

			PHY_IPhysicsEnvironment* physEnv = 
				new ODEPhysicsEnvironment();
#else
			PHY_IPhysicsEnvironment* physEnv = 
				new DummyPhysicsEnvironment();

#endif //USE_ODE

			destinationscene ->SetPhysicsEnvironment(physEnv);
			break;
		}
	case UseDynamo:
		{
		}

	case UseNone:
		{
		};
	default:
		{
			physics_engine = UseNone;
			
			PHY_IPhysicsEnvironment* physEnv = 
				new DummyPhysicsEnvironment();
			destinationscene ->SetPhysicsEnvironment(physEnv);

		}
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

	m_map_blender_to_gameactuator.clear();
	m_map_blender_to_gamecontroller.clear();

	m_map_blender_to_gameobject.clear();
	m_map_mesh_to_gamemesh.clear();
	m_map_gameobject_to_blender.clear();
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
	m_meshobjects.push_back(gamemesh);
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
	m_polymaterials.push_back(polymat);
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
	m_worldinfos.push_back(worldinfo);
}
