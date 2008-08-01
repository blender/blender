/**
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
 */
#ifndef __KX_BLENDERSCENECONVERTER_H
#define __KX_BLENDERSCENECONVERTER_H

#include "KX_HashedPtr.h"
#include "GEN_Map.h"

#include "KX_ISceneConverter.h"
#include "KX_IpoConvert.h"

class KX_WorldInfo;
class SCA_IActuator;
class SCA_IController;
class RAS_MeshObject;
class RAS_IPolyMaterial;
class BL_InterpolatorList;
class BL_Material;
struct IpoCurve;
struct Main;
struct SpaceIpo;

class KX_BlenderSceneConverter : public KX_ISceneConverter
{
	// Use vector of pairs to allow removal of entities between scene switch
	vector<pair<KX_Scene*,KX_WorldInfo*> >	m_worldinfos;
	vector<pair<KX_Scene*,RAS_IPolyMaterial*> > m_polymaterials;
	vector<pair<KX_Scene*,RAS_MeshObject*> > m_meshobjects;
	vector<pair<KX_Scene*,BL_Material *> >	m_materials;
	// Should also have a list of collision shapes. 
	// For the time being this is held in KX_Scene::m_shapes

	GEN_Map<CHashedPtr,struct Object*> m_map_gameobject_to_blender;
	GEN_Map<CHashedPtr,KX_GameObject*> m_map_blender_to_gameobject;

	GEN_Map<CHashedPtr,RAS_MeshObject*> m_map_mesh_to_gamemesh;
//	GEN_Map<CHashedPtr,DT_ShapeHandle> m_map_gamemesh_to_sumoshape;
	
	GEN_Map<CHashedPtr,SCA_IActuator*> m_map_blender_to_gameactuator;
	GEN_Map<CHashedPtr,SCA_IController*> m_map_blender_to_gamecontroller;
	
	GEN_Map<CHashedPtr,BL_InterpolatorList*> m_map_blender_to_gameipolist;
	
	Main*					m_maggie;
	SpaceIpo*				m_sipo;

	STR_String				m_newfilename;
	class KX_KetsjiEngine*	m_ketsjiEngine;
	class KX_Scene*			m_currentScene;	// Scene being converted
	bool					m_alwaysUseExpandFraming;
	bool					m_usemat;
	bool					m_useglslmat;

	void localDel_ipoCurve ( IpoCurve * icu ,struct SpaceIpo*	sipo);
//	struct Ipo* findIpoForName(char* objName);

public:
	KX_BlenderSceneConverter(
		Main* maggie,
		SpaceIpo *sipo,
		class KX_KetsjiEngine* engine
	);

	virtual ~KX_BlenderSceneConverter();

	/* Scenename: name of the scene to be converted.
	 * destinationscene: pass an empty scene, everything goes into this
	 * dictobj: python dictionary (for pythoncontrollers)
	 */
	virtual void	ConvertScene(
						const STR_String& scenename,
						class KX_Scene* destinationscene,
						PyObject* dictobj,
						class SCA_IInputDevice* keyinputdev,
						class RAS_IRenderTools* rendertools,
						class RAS_ICanvas* canvas
					);
	virtual void RemoveScene(class KX_Scene *scene);

	void SetNewFileName(const STR_String& filename);
	bool TryAndLoadNewFile();

	void SetAlwaysUseExpandFraming(bool to_what);
	
	void RegisterGameObject(KX_GameObject *gameobject, struct Object *for_blenderobject);
	void UnregisterGameObject(KX_GameObject *gameobject);
	KX_GameObject *FindGameObject(struct Object *for_blenderobject);
	struct Object *FindBlenderObject(KX_GameObject *for_gameobject);

	void RegisterGameMesh(RAS_MeshObject *gamemesh, struct Mesh *for_blendermesh);
	RAS_MeshObject *FindGameMesh(struct Mesh *for_blendermesh, unsigned int onlayer);

//	void RegisterSumoShape(DT_ShapeHandle shape, RAS_MeshObject *for_gamemesh);
//	DT_ShapeHandle FindSumoShape(RAS_MeshObject *for_gamemesh);

	void RegisterPolyMaterial(RAS_IPolyMaterial *polymat);

	void RegisterBlenderMaterial(BL_Material *mat);
	
	void RegisterInterpolatorList(BL_InterpolatorList *ipoList, struct Ipo *for_ipo);
	BL_InterpolatorList *FindInterpolatorList(struct Ipo *for_ipo);

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

};

#endif //__KX_BLENDERSCENECONVERTER_H

