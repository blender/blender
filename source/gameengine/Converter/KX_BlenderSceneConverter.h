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
#ifndef __KX_BLENDERSCENECONVERTER_H
#define __KX_BLENDERSCENECONVERTER_H

#include "GEN_Map.h"

#include "KX_ISceneConverter.h"
#include "KX_HashedPtr.h"
#include "KX_IpoConvert.h"

class KX_WorldInfo;
class SCA_IActuator;
class SCA_IController;
class RAS_MeshObject;
class RAS_IPolyMaterial;
class BL_InterpolatorList;

class KX_BlenderSceneConverter : public KX_ISceneConverter
{
	vector<KX_WorldInfo*>	m_worldinfos;
	vector<RAS_IPolyMaterial*> m_polymaterials;
	vector<RAS_MeshObject*> m_meshobjects;

	GEN_Map<CHashedPtr,struct Object*> m_map_gameobject_to_blender;
	GEN_Map<CHashedPtr,KX_GameObject*> m_map_blender_to_gameobject;

	GEN_Map<CHashedPtr,RAS_MeshObject*> m_map_mesh_to_gamemesh;
//	GEN_Map<CHashedPtr,DT_ShapeHandle> m_map_gamemesh_to_sumoshape;
	
	GEN_Map<CHashedPtr,SCA_IActuator*> m_map_blender_to_gameactuator;
	GEN_Map<CHashedPtr,SCA_IController*> m_map_blender_to_gamecontroller;
	
	GEN_Map<CHashedPtr,BL_InterpolatorList*> m_map_blender_to_gameipolist;
	
	struct Main*			m_maggie;
	STR_String				m_newfilename;
	class KX_KetsjiEngine*	m_ketsjiEngine;
	bool					m_alwaysUseExpandFraming;
	
public:
	KX_BlenderSceneConverter(
		struct Main* maggie,
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

	void SetNewFileName(const STR_String& filename);
	bool TryAndLoadNewFile();

	void SetAlwaysUseExpandFraming(bool to_what);
	
	void RegisterGameObject(KX_GameObject *gameobject, struct Object *for_blenderobject);
	KX_GameObject *FindGameObject(struct Object *for_blenderobject);
	struct Object *FindBlenderObject(KX_GameObject *for_gameobject);

	void RegisterGameMesh(RAS_MeshObject *gamemesh, struct Mesh *for_blendermesh);
	RAS_MeshObject *FindGameMesh(struct Mesh *for_blendermesh, unsigned int onlayer);

//	void RegisterSumoShape(DT_ShapeHandle shape, RAS_MeshObject *for_gamemesh);
//	DT_ShapeHandle FindSumoShape(RAS_MeshObject *for_gamemesh);

	void RegisterPolyMaterial(RAS_IPolyMaterial *polymat);
	
	void RegisterInterpolatorList(BL_InterpolatorList *ipoList, struct Ipo *for_ipo);
	BL_InterpolatorList *FindInterpolatorList(struct Ipo *for_ipo);

	void RegisterGameActuator(SCA_IActuator *act, struct bActuator *for_actuator);
	SCA_IActuator *FindGameActuator(struct bActuator *for_actuator);

	void RegisterGameController(SCA_IController *cont, struct bController *for_controller);
	SCA_IController *FindGameController(struct bController *for_controller);

	void RegisterWorldInfo(KX_WorldInfo *worldinfo);
};

#endif //__KX_BLENDERSCENECONVERTER_H

