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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DocumentImporter.h
 *  \ingroup collada
 */

#ifndef __DOCUMENTIMPORTER_H__
#define __DOCUMENTIMPORTER_H__

#include "COLLADAFWIWriter.h"
#include "COLLADAFWMaterial.h"
#include "COLLADAFWEffect.h"
#include "COLLADAFWColor.h"
#include "COLLADAFWImage.h"
#include "COLLADAFWInstanceGeometry.h"
#include "COLLADAFWController.h"
#include "COLLADAFWMorphController.h"
#include "COLLADAFWSkinController.h"
#include "COLLADAFWEffectCommon.h"


#include "BKE_object.h"

#include "TransformReader.h"
#include "AnimationImporter.h"
#include "ArmatureImporter.h"
#include "MeshImporter.h"



struct Main;
struct bContext;

/** Importer class. */
class DocumentImporter : COLLADAFW::IWriter
{
public:
	//! Enumeration to denote the stage of import
	enum ImportStage {
		General,		//!< First pass to collect all data except controller
		Controller,		//!< Second pass to collect controller data
	};
	/** Constructor */
	DocumentImporter(bContext *C, const char *filename);

	/** Destructor */
	~DocumentImporter();

	/** Function called by blender UI */
	bool import();

	/** these should not be here */
	Object* create_camera_object(COLLADAFW::InstanceCamera*, Scene*);
	Object* create_lamp_object(COLLADAFW::InstanceLight*, Scene*);
	Object* create_instance_node(Object*, COLLADAFW::Node*, COLLADAFW::Node*, Scene*, bool);
	void write_node(COLLADAFW::Node*, COLLADAFW::Node*, Scene*, Object*, bool);
	MTex* create_texture(COLLADAFW::EffectCommon*, COLLADAFW::Texture&, Material*, int, TexIndexTextureArrayMap&);
	void write_profile_COMMON(COLLADAFW::EffectCommon*, Material*);
	void translate_anim_recursive(COLLADAFW::Node*, COLLADAFW::Node*, Object*);

	/** This method will be called if an error in the loading process occurred and the loader cannot
	continue to load. The writer should undo all operations that have been performed.
	\param errorMessage A message containing informations about the error that occurred.
	*/
	void cancel(const COLLADAFW::String& errorMessage);

	/** This is the method called. The writer hast to prepare to receive data.*/
	void start();

	/** This method is called after the last write* method. No other methods will be called after this.*/
	void finish();

	bool writeGlobalAsset(const COLLADAFW::FileInfo*);

	bool writeScene(const COLLADAFW::Scene*);

	bool writeVisualScene(const COLLADAFW::VisualScene*);

	bool writeLibraryNodes(const COLLADAFW::LibraryNodes*);

	bool writeAnimation(const COLLADAFW::Animation*);

	bool writeAnimationList(const COLLADAFW::AnimationList*);

	bool writeGeometry(const COLLADAFW::Geometry*);

	bool writeMaterial(const COLLADAFW::Material*);

	bool writeEffect(const COLLADAFW::Effect*);

	bool writeCamera(const COLLADAFW::Camera*);

	bool writeImage(const COLLADAFW::Image*);

	bool writeLight(const COLLADAFW::Light*);

	bool writeSkinControllerData(const COLLADAFW::SkinControllerData*);

	bool writeController(const COLLADAFW::Controller*);

	bool writeFormulas(const COLLADAFW::Formulas*);

	bool writeKinematicsScene(const COLLADAFW::KinematicsScene*);

	/** Add element and data for UniqueId */
	bool addExtraTags(const COLLADAFW::UniqueId &uid, ExtraTags *extra_tags);
	/** Get an extisting ExtraTags for uid */
	ExtraTags* getExtraTags(const COLLADAFW::UniqueId &uid);

private:

	/** Current import stage we're in. */
	ImportStage mImportStage;
	std::string mFilename;

	bContext *mContext;

	UnitConverter unit_converter;
	ArmatureImporter armature_importer;
	MeshImporter mesh_importer;
	AnimationImporter anim_importer;
	
	/** TagsMap typedef for uid_tags_map. */
	typedef std::map<std::string, ExtraTags*> TagsMap;
	/** Tags map of unique id as a string and ExtraTags instance. */
	TagsMap uid_tags_map;

	std::map<COLLADAFW::UniqueId, Image*> uid_image_map;
	std::map<COLLADAFW::UniqueId, Material*> uid_material_map;
	std::map<COLLADAFW::UniqueId, Material*> uid_effect_map;
	std::map<COLLADAFW::UniqueId, Camera*> uid_camera_map;
	std::map<COLLADAFW::UniqueId, Lamp*> uid_lamp_map;
	std::map<Material*, TexIndexTextureArrayMap> material_texture_mapping_map;
	std::map<COLLADAFW::UniqueId, Object*> object_map;
	std::map<COLLADAFW::UniqueId, COLLADAFW::Node*> node_map;
	std::vector<const COLLADAFW::VisualScene*> vscenes;
	std::vector<Object*> libnode_ob;
	
	std::map<COLLADAFW::UniqueId, COLLADAFW::Node*> root_map; // find root joint by child joint uid, for bone tree evaluation during resampling
	std::map<COLLADAFW::UniqueId, const COLLADAFW::Object*> FW_object_map;

};

#endif
