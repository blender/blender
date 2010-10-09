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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Nathan Letwory.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
// TODO:
// * name imported objects
// * import object rotation as euler

#include <string>
#include <map>
#include <algorithm> // sort()

#include <math.h>
#include <float.h>

#include "COLLADAFWRoot.h"
#include "COLLADAFWIWriter.h"
#include "COLLADAFWStableHeaders.h"
#include "COLLADAFWCamera.h"
#include "COLLADAFWColorOrTexture.h"
#include "COLLADAFWEffect.h"
#include "COLLADAFWImage.h"
#include "COLLADAFWIndexList.h"
#include "COLLADAFWInstanceGeometry.h"
#include "COLLADAFWLight.h"
#include "COLLADAFWMaterial.h"
#include "COLLADAFWMeshPrimitiveWithFaceVertexCount.h"
#include "COLLADAFWPolygons.h"
#include "COLLADAFWSampler.h"
#include "COLLADAFWTypes.h"
#include "COLLADAFWVisualScene.h"
#include "COLLADAFWArrayPrimitiveType.h"
#include "COLLADAFWLibraryNodes.h"

#include "COLLADASaxFWLLoader.h"

#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_texture.h"
#include "BKE_fcurve.h"
#include "BKE_depsgraph.h"
#include "BLI_path_util.h"
#include "BKE_scene.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_material.h"
#include "BKE_utildefines.h"
#include "BKE_image.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"

#include "RNA_access.h"

#include "MEM_guardedalloc.h"

#include "DocumentImporter.h"
#include "collada_internal.h"

#include "TransformReader.h"
#include "AnimationImporter.h"
#include "ArmatureImporter.h"
#include "MeshImporter.h"
#include "collada_utils.h"


/*
  COLLADA Importer limitations:
  - no multiple scene import, all objects are added to active scene
 */

// #define COLLADA_DEBUG
// creates empties for each imported bone on layer 2, for debugging
// #define ARMATURE_TEST

/** Class that needs to be implemented by a writer. 
	IMPORTANT: The write functions are called in arbitrary order.*/
class Writer: public COLLADAFW::IWriter
{
private:
	std::string mFilename;
	
	bContext *mContext;

	UnitConverter unit_converter;
	ArmatureImporter armature_importer;
	MeshImporter mesh_importer;
	AnimationImporter anim_importer;

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

	// animation
	// std::map<COLLADAFW::UniqueId, std::vector<FCurve*> > uid_fcurve_map;
	// Nodes don't share AnimationLists (Arystan)
	// std::map<COLLADAFW::UniqueId, Animation> uid_animated_map; // AnimationList->uniqueId to AnimatedObject map

public:

	/** Constructor. */
	Writer(bContext *C, const char *filename) : mFilename(filename), mContext(C),
												armature_importer(&unit_converter, &mesh_importer, &anim_importer, CTX_data_scene(C)),
												mesh_importer(&unit_converter, &armature_importer, CTX_data_scene(C)),
												anim_importer(&unit_converter, &armature_importer, CTX_data_scene(C)) {}

	/** Destructor. */
	~Writer() {}

	bool write()
	{
		COLLADASaxFWL::Loader loader;
		COLLADAFW::Root root(&loader, this);

		// XXX report error
		if (!root.loadDocument(mFilename))
			return false;

		return true;
	}

	/** This method will be called if an error in the loading process occurred and the loader cannot
		continue to load. The writer should undo all operations that have been performed.
		@param errorMessage A message containing informations about the error that occurred.
	*/
	virtual void cancel(const COLLADAFW::String& errorMessage)
	{
		// TODO: if possible show error info
		//
		// Should we get rid of invisible Meshes that were created so far
		// or maybe create objects at coordinate space origin?
		//
		// The latter sounds better.
	}

	/** This is the method called. The writer hast to prepare to receive data.*/
	virtual void start()
	{
	}

	/** This method is called after the last write* method. No other methods will be called after this.*/
	virtual void finish()
	{
		std::vector<const COLLADAFW::VisualScene*>::iterator it;
		for (it = vscenes.begin(); it != vscenes.end(); it++) {
			PointerRNA sceneptr, unit_settings;
			PropertyRNA *system, *scale;
			// TODO: create a new scene except the selected <visual_scene> - use current blender scene for it
			Scene *sce = CTX_data_scene(mContext);
			
			// for scene unit settings: system, scale_length
			RNA_id_pointer_create(&sce->id, &sceneptr);
			unit_settings = RNA_pointer_get(&sceneptr, "unit_settings");
			system = RNA_struct_find_property(&unit_settings, "system");
			scale = RNA_struct_find_property(&unit_settings, "scale_length");
			
			switch(unit_converter.isMetricSystem()) {
				case UnitConverter::Metric:
					RNA_property_enum_set(&unit_settings, system, USER_UNIT_METRIC);
					break;
				case UnitConverter::Imperial:
					RNA_property_enum_set(&unit_settings, system, USER_UNIT_IMPERIAL);
					break;
				default:
					RNA_property_enum_set(&unit_settings, system, USER_UNIT_NONE);
					break;
			}
			RNA_property_float_set(&unit_settings, scale, unit_converter.getLinearMeter());
			
			const COLLADAFW::NodePointerArray& roots = (*it)->getRootNodes();

			for (unsigned int i = 0; i < roots.getCount(); i++) {
				write_node(roots[i], NULL, sce, NULL, false);
			}
		}

		armature_importer.make_armatures(mContext);

#if 0
		armature_importer.fix_animation();
#endif

		for (std::vector<const COLLADAFW::VisualScene*>::iterator it = vscenes.begin(); it != vscenes.end(); it++) {
			const COLLADAFW::NodePointerArray& roots = (*it)->getRootNodes();

			for (unsigned int i = 0; i < roots.getCount(); i++)
				translate_anim_recursive(roots[i]);
		}

		if (libnode_ob.size()) {
			Scene *sce = CTX_data_scene(mContext);

			fprintf(stderr, "got %u library nodes to free\n", libnode_ob.size());
			// free all library_nodes
			std::vector<Object*>::iterator it;
			for (it = libnode_ob.begin(); it != libnode_ob.end(); it++) {
				Object *ob = *it;

				Base *base = object_in_scene(ob, sce);
				if (base) {
					BLI_remlink(&sce->base, base);
					free_libblock_us(&G.main->object, base->object);
					if (sce->basact==base)
						sce->basact= NULL;
					MEM_freeN(base);
				}
			}
			libnode_ob.clear();

			DAG_scene_sort(CTX_data_main(mContext), sce);
			DAG_ids_flush_update(CTX_data_main(mContext), 0);
		}
	}


	void translate_anim_recursive(COLLADAFW::Node *node, COLLADAFW::Node *par = NULL, Object *parob = NULL)
	{
		if (par && par->getType() == COLLADAFW::Node::JOINT) {
			// par is root if there's no corresp. key in root_map
			if (root_map.find(par->getUniqueId()) == root_map.end())
				root_map[node->getUniqueId()] = par;
			else
				root_map[node->getUniqueId()] = root_map[par->getUniqueId()];
		}

		COLLADAFW::Transformation::TransformationType types[] = {
			COLLADAFW::Transformation::ROTATE,
			COLLADAFW::Transformation::SCALE,
			COLLADAFW::Transformation::TRANSLATE,
			COLLADAFW::Transformation::MATRIX
		};

		unsigned int i;
		Object *ob;

		for (i = 0; i < 4; i++)
			ob = anim_importer.translate_animation(node, object_map, root_map, types[i]);

		COLLADAFW::NodePointerArray &children = node->getChildNodes();
		for (i = 0; i < children.getCount(); i++) {
			translate_anim_recursive(children[i], node, ob);
		}
	}

	/** When this method is called, the writer must write the global document asset.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeGlobalAsset ( const COLLADAFW::FileInfo* asset ) 
	{
		unit_converter.read_asset(asset);

		return true;
	}

	/** When this method is called, the writer must write the scene.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeScene ( const COLLADAFW::Scene* scene ) 
	{
		// XXX could store the scene id, but do nothing for now
		return true;
	}
	Object *create_camera_object(COLLADAFW::InstanceCamera *camera, Scene *sce)
	{
		const COLLADAFW::UniqueId& cam_uid = camera->getInstanciatedObjectId();
		if (uid_camera_map.find(cam_uid) == uid_camera_map.end()) {	
			fprintf(stderr, "Couldn't find camera by UID.\n");
			return NULL;
		}
		Object *ob = add_object(sce, OB_CAMERA);
		Camera *cam = uid_camera_map[cam_uid];
		Camera *old_cam = (Camera*)ob->data;
		ob->data = cam;
		old_cam->id.us--;
		if (old_cam->id.us == 0)
			free_libblock(&G.main->camera, old_cam);
		return ob;
	}
	
	Object *create_lamp_object(COLLADAFW::InstanceLight *lamp, Scene *sce)
	{
		const COLLADAFW::UniqueId& lamp_uid = lamp->getInstanciatedObjectId();
		if (uid_lamp_map.find(lamp_uid) == uid_lamp_map.end()) {	
			fprintf(stderr, "Couldn't find lamp by UID. \n");
			return NULL;
		}
		Object *ob = add_object(sce, OB_LAMP);
		Lamp *la = uid_lamp_map[lamp_uid];
		Lamp *old_lamp = (Lamp*)ob->data;
		ob->data = la;
		old_lamp->id.us--;
		if (old_lamp->id.us == 0)
			free_libblock(&G.main->lamp, old_lamp);
		return ob;
	}

	Object *create_instance_node(Object *source_ob, COLLADAFW::Node *source_node, COLLADAFW::Node *instance_node, Scene *sce, bool is_library_node)
	{
		Object *obn = copy_object(source_ob);
		obn->recalc |= OB_RECALC_ALL;
		scene_add_base(sce, obn);

		if (instance_node)
			anim_importer.read_node_transform(instance_node, obn);
		else
			anim_importer.read_node_transform(source_node, obn);

		DAG_scene_sort(CTX_data_main(mContext), sce);
		DAG_ids_flush_update(CTX_data_main(mContext), 0);

		COLLADAFW::NodePointerArray &children = source_node->getChildNodes();
		if (children.getCount()) {
			for (unsigned int i = 0; i < children.getCount(); i++) {
				COLLADAFW::Node *child_node = children[i];
				const COLLADAFW::UniqueId& child_id = child_node->getUniqueId();
				if (object_map.find(child_id) == object_map.end())
					continue;
				COLLADAFW::InstanceNodePointerArray &inodes = child_node->getInstanceNodes();
				Object *new_child = NULL;
				if (inodes.getCount()) {
					const COLLADAFW::UniqueId& id = inodes[0]->getInstanciatedObjectId();
					new_child = create_instance_node(object_map[id], node_map[id], child_node, sce, is_library_node);
				}
				else {
					new_child = create_instance_node(object_map[child_id], child_node, NULL, sce, is_library_node);
				}
				bc_set_parent(new_child, obn, mContext, true);

				if (is_library_node)
					libnode_ob.push_back(new_child);
			}
		}

		return obn;
	}
	
	void write_node (COLLADAFW::Node *node, COLLADAFW::Node *parent_node, Scene *sce, Object *par, bool is_library_node)
	{
		Object *ob = NULL;
		bool is_joint = node->getType() == COLLADAFW::Node::JOINT;

		if (is_joint) {
			armature_importer.add_joint(node, parent_node == NULL || parent_node->getType() != COLLADAFW::Node::JOINT, par);
		}
		else {
			COLLADAFW::InstanceGeometryPointerArray &geom = node->getInstanceGeometries();
			COLLADAFW::InstanceCameraPointerArray &camera = node->getInstanceCameras();
			COLLADAFW::InstanceLightPointerArray &lamp = node->getInstanceLights();
			COLLADAFW::InstanceControllerPointerArray &controller = node->getInstanceControllers();
			COLLADAFW::InstanceNodePointerArray &inst_node = node->getInstanceNodes();

			// XXX linking object with the first <instance_geometry>, though a node may have more of them...
			// maybe join multiple <instance_...> meshes into 1, and link object with it? not sure...
			// <instance_geometry>
			if (geom.getCount() != 0) {
				ob = mesh_importer.create_mesh_object(node, geom[0], false, uid_material_map,
													  material_texture_mapping_map);
			}
			else if (camera.getCount() != 0) {
				ob = create_camera_object(camera[0], sce);
			}
			else if (lamp.getCount() != 0) {
				ob = create_lamp_object(lamp[0], sce);
			}
			else if (controller.getCount() != 0) {
				COLLADAFW::InstanceGeometry *geom = (COLLADAFW::InstanceGeometry*)controller[0];
				ob = mesh_importer.create_mesh_object(node, geom, true, uid_material_map, material_texture_mapping_map);
			}
			// XXX instance_node is not supported yet
			else if (inst_node.getCount() != 0) {
				const COLLADAFW::UniqueId& node_id = inst_node[0]->getInstanciatedObjectId();
				if (object_map.find(node_id) == object_map.end()) {
					fprintf(stderr, "Cannot find node to instanciate.\n");
					ob = NULL;
				}
				else {
					Object *source_ob = object_map[node_id];
					COLLADAFW::Node *source_node = node_map[node_id];

					ob = create_instance_node(source_ob, source_node, node, sce, is_library_node);
				}
			}
			// if node is empty - create empty object
			// XXX empty node may not mean it is empty object, not sure about this
			else {
				ob = add_object(sce, OB_EMPTY);
			}
			
			// check if object is not NULL
			if (!ob) return;
			
			rename_id(&ob->id, (char*)node->getOriginalId().c_str());

			object_map[node->getUniqueId()] = ob;
			node_map[node->getUniqueId()] = node;

			if (is_library_node)
				libnode_ob.push_back(ob);
		}

		anim_importer.read_node_transform(node, ob);

		if (!is_joint) {
			// if par was given make this object child of the previous 
			if (par && ob)
				bc_set_parent(ob, par, mContext);
		}

		// if node has child nodes write them
		COLLADAFW::NodePointerArray &child_nodes = node->getChildNodes();
		for (unsigned int i = 0; i < child_nodes.getCount(); i++) {	
			write_node(child_nodes[i], node, sce, ob, is_library_node);
		}
	}

	/** When this method is called, the writer must write the entire visual scene.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeVisualScene ( const COLLADAFW::VisualScene* visualScene ) 
	{
		// this method called on post process after writeGeometry, writeMaterial, etc.

		// for each <node> in <visual_scene>:
		// create an Object
		// if Mesh (previously created in writeGeometry) to which <node> corresponds exists, link Object with that mesh

		// update: since we cannot link a Mesh with Object in
		// writeGeometry because <geometry> does not reference <node>,
		// we link Objects with Meshes here

		vscenes.push_back(visualScene);
		
		return true;
	}

	/** When this method is called, the writer must handle all nodes contained in the 
		library nodes.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeLibraryNodes ( const COLLADAFW::LibraryNodes* libraryNodes ) 
	{
		Scene *sce = CTX_data_scene(mContext);

		const COLLADAFW::NodePointerArray& nodes = libraryNodes->getNodes();

		for (unsigned int i = 0; i < nodes.getCount(); i++) {
			write_node(nodes[i], NULL, sce, NULL, true);
		}

		return true;
	}

	/** When this method is called, the writer must write the geometry.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeGeometry ( const COLLADAFW::Geometry* geom ) 
	{
		return mesh_importer.write_geometry(geom);
	}

	/** When this method is called, the writer must write the material.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeMaterial( const COLLADAFW::Material* cmat ) 
	{
		const std::string& str_mat_id = cmat->getOriginalId();
		Material *ma = add_material((char*)str_mat_id.c_str());
		
		this->uid_effect_map[cmat->getInstantiatedEffect()] = ma;
		this->uid_material_map[cmat->getUniqueId()] = ma;
		
		return true;
	}
	
	// create mtex, create texture, set texture image
	MTex *create_texture(COLLADAFW::EffectCommon *ef, COLLADAFW::Texture &ctex, Material *ma,
						 int i, TexIndexTextureArrayMap &texindex_texarray_map)
	{
		COLLADAFW::SamplerPointerArray& samp_array = ef->getSamplerPointerArray();
		COLLADAFW::Sampler *sampler = samp_array[ctex.getSamplerId()];
			
		const COLLADAFW::UniqueId& ima_uid = sampler->getSourceImage();
		
		if (uid_image_map.find(ima_uid) == uid_image_map.end()) {
			fprintf(stderr, "Couldn't find an image by UID.\n");
			return NULL;
		}
		
		ma->mtex[i] = add_mtex();
		ma->mtex[i]->texco = TEXCO_UV;
		ma->mtex[i]->tex = add_texture("Texture");
		ma->mtex[i]->tex->type = TEX_IMAGE;
		ma->mtex[i]->tex->imaflag &= ~TEX_USEALPHA;
		ma->mtex[i]->tex->ima = uid_image_map[ima_uid];
		
		texindex_texarray_map[ctex.getTextureMapId()].push_back(ma->mtex[i]);
		
		return ma->mtex[i];
	}
	
	void write_profile_COMMON(COLLADAFW::EffectCommon *ef, Material *ma)
	{
		COLLADAFW::EffectCommon::ShaderType shader = ef->getShaderType();
		
		// blinn
		if (shader == COLLADAFW::EffectCommon::SHADER_BLINN) {
			ma->spec_shader = MA_SPEC_BLINN;
			ma->spec = ef->getShininess().getFloatValue();
		}
		// phong
		else if (shader == COLLADAFW::EffectCommon::SHADER_PHONG) {
			ma->spec_shader = MA_SPEC_PHONG;
			ma->har = ef->getShininess().getFloatValue();
		}
		// lambert
		else if (shader == COLLADAFW::EffectCommon::SHADER_LAMBERT) {
			ma->diff_shader = MA_DIFF_LAMBERT;
		}
		// default - lambert
		else {
			ma->diff_shader = MA_DIFF_LAMBERT;
			fprintf(stderr, "Current shader type is not supported.\n");
		}
		// reflectivity
		ma->ray_mirror = ef->getReflectivity().getFloatValue();
		// index of refraction
		ma->ang = ef->getIndexOfRefraction().getFloatValue();
		
		int i = 0;
		COLLADAFW::Color col;
		MTex *mtex = NULL;
		TexIndexTextureArrayMap texindex_texarray_map;
		
		// DIFFUSE
		// color
		if (ef->getDiffuse().isColor()) {
			col = ef->getDiffuse().getColor();
			ma->r = col.getRed();
			ma->g = col.getGreen();
			ma->b = col.getBlue();
		}
		// texture
		else if (ef->getDiffuse().isTexture()) {
			COLLADAFW::Texture ctex = ef->getDiffuse().getTexture(); 
			mtex = create_texture(ef, ctex, ma, i, texindex_texarray_map);
			if (mtex != NULL) {
				mtex->mapto = MAP_COL;
				ma->texact = (int)i;
				i++;
			}
		}
		// AMBIENT
		// color
		if (ef->getAmbient().isColor()) {
			col = ef->getAmbient().getColor();
			ma->ambr = col.getRed();
			ma->ambg = col.getGreen();
			ma->ambb = col.getBlue();
		}
		// texture
		else if (ef->getAmbient().isTexture()) {
			COLLADAFW::Texture ctex = ef->getAmbient().getTexture(); 
			mtex = create_texture(ef, ctex, ma, i, texindex_texarray_map);
			if (mtex != NULL) {
				mtex->mapto = MAP_AMB; 
				i++;
			}
		}
		// SPECULAR
		// color
		if (ef->getSpecular().isColor()) {
			col = ef->getSpecular().getColor();
			ma->specr = col.getRed();
			ma->specg = col.getGreen();
			ma->specb = col.getBlue();
		}
		// texture
		else if (ef->getSpecular().isTexture()) {
			COLLADAFW::Texture ctex = ef->getSpecular().getTexture(); 
			mtex = create_texture(ef, ctex, ma, i, texindex_texarray_map);
			if (mtex != NULL) {
				mtex->mapto = MAP_SPEC; 
				i++;
			}
		}
		// REFLECTIVE
		// color
		if (ef->getReflective().isColor()) {
			col = ef->getReflective().getColor();
			ma->mirr = col.getRed();
			ma->mirg = col.getGreen();
			ma->mirb = col.getBlue();
		}
		// texture
		else if (ef->getReflective().isTexture()) {
			COLLADAFW::Texture ctex = ef->getReflective().getTexture(); 
			mtex = create_texture(ef, ctex, ma, i, texindex_texarray_map);
			if (mtex != NULL) {
				mtex->mapto = MAP_REF; 
				i++;
			}
		}
		// EMISSION
		// color
		if (ef->getEmission().isColor()) {
			// XXX there is no emission color in blender
			// but I am not sure
		}
		// texture
		else if (ef->getEmission().isTexture()) {
			COLLADAFW::Texture ctex = ef->getEmission().getTexture(); 
			mtex = create_texture(ef, ctex, ma, i, texindex_texarray_map);
			if (mtex != NULL) {
				mtex->mapto = MAP_EMIT; 
				i++;
			}
		}
		// TRANSPARENT
		// color
	// 	if (ef->getOpacity().isColor()) {
// 			// XXX don't know what to do here
// 		}
// 		// texture
// 		else if (ef->getOpacity().isTexture()) {
// 			ctex = ef->getOpacity().getTexture();
// 			if (mtex != NULL) mtex->mapto &= MAP_ALPHA;
// 			else {
// 				mtex = create_texture(ef, ctex, ma, i, texindex_texarray_map);
// 				if (mtex != NULL) mtex->mapto = MAP_ALPHA;
// 			}
// 		}
		material_texture_mapping_map[ma] = texindex_texarray_map;
	}
	
	/** When this method is called, the writer must write the effect.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	
	virtual bool writeEffect( const COLLADAFW::Effect* effect ) 
	{
		
		const COLLADAFW::UniqueId& uid = effect->getUniqueId();
		if (uid_effect_map.find(uid) == uid_effect_map.end()) {
			fprintf(stderr, "Couldn't find a material by UID.\n");
			return true;
		}
		
		Material *ma = uid_effect_map[uid];
		
		COLLADAFW::CommonEffectPointerArray common_efs = effect->getCommonEffects();
		if (common_efs.getCount() < 1) {
			fprintf(stderr, "Couldn't find <profile_COMMON>.\n");
			return true;
		}
		// XXX TODO: Take all <profile_common>s
		// Currently only first <profile_common> is supported
		COLLADAFW::EffectCommon *ef = common_efs[0];
		write_profile_COMMON(ef, ma);
		
		return true;
	}
	
	
	/** When this method is called, the writer must write the camera.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeCamera( const COLLADAFW::Camera* camera ) 
	{
		Camera *cam = NULL;
		std::string cam_id, cam_name;
		
		cam_id = camera->getOriginalId();
		cam_name = camera->getName();
		if (cam_name.size()) cam = (Camera*)add_camera((char*)cam_name.c_str());
		else cam = (Camera*)add_camera((char*)cam_id.c_str());
		
		if (!cam) {
			fprintf(stderr, "Cannot create camera. \n");
			return true;
		}
		cam->clipsta = camera->getNearClippingPlane().getValue();
		cam->clipend = camera->getFarClippingPlane().getValue();
		
		COLLADAFW::Camera::CameraType type = camera->getCameraType();
		switch(type) {
		case COLLADAFW::Camera::ORTHOGRAPHIC:
			{
				cam->type = CAM_ORTHO;
			}
			break;
		case COLLADAFW::Camera::PERSPECTIVE:
			{
				cam->type = CAM_PERSP;
			}
			break;
		case COLLADAFW::Camera::UNDEFINED_CAMERATYPE:
			{
				fprintf(stderr, "Current camera type is not supported. \n");
				cam->type = CAM_PERSP;
			}
			break;
		}
		
		switch(camera->getDescriptionType()) {
		case COLLADAFW::Camera::ASPECTRATIO_AND_Y:
			{
				switch(cam->type) {
					case CAM_ORTHO:
						{
							double ymag = camera->getYMag().getValue();
							double aspect = camera->getAspectRatio().getValue();
							double xmag = aspect*ymag;
							cam->ortho_scale = (float)xmag;
						}
						break;
					case CAM_PERSP:
					default:
						{
							double yfov = camera->getYFov().getValue();
							double aspect = camera->getAspectRatio().getValue();
							double xfov = aspect*yfov;
							// xfov is in degrees, cam->lens is in millimiters
							cam->lens = angle_to_lens((float)xfov*(M_PI/180.0f));
						}
						break;
				}
			}
			break;
		/* XXX correct way to do following four is probably to get also render
		   size and determine proper settings from that somehow */
		case COLLADAFW::Camera::ASPECTRATIO_AND_X:
		case COLLADAFW::Camera::SINGLE_X:
		case COLLADAFW::Camera::X_AND_Y:
			{
				switch(cam->type) {
					case CAM_ORTHO:
						cam->ortho_scale = (float)camera->getXMag().getValue();
						break;
					case CAM_PERSP:
					default:
						{
							double x = camera->getXFov().getValue();
							// x is in degrees, cam->lens is in millimiters
							cam->lens = angle_to_lens((float)x*(M_PI/180.0f));
						}
						break;
				}
			}
			break;
		case COLLADAFW::Camera::SINGLE_Y:
			{
				switch(cam->type) {
					case CAM_ORTHO:
						cam->ortho_scale = (float)camera->getYMag().getValue();
						break;
					case CAM_PERSP:
					default:
						{
						double yfov = camera->getYFov().getValue();
						// yfov is in degrees, cam->lens is in millimiters
						cam->lens = angle_to_lens((float)yfov*(M_PI/180.0f));
						}
						break;
				}
			}
			break;
		case COLLADAFW::Camera::UNDEFINED:
			// read nothing, use blender defaults.
			break;
		}
		
		this->uid_camera_map[camera->getUniqueId()] = cam;
		// XXX import camera options
		return true;
	}

	/** When this method is called, the writer must write the image.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeImage( const COLLADAFW::Image* image ) 
	{
		// XXX maybe it is necessary to check if the path is absolute or relative
	    const std::string& filepath = image->getImageURI().toNativePath();
		const char *filename = (const char*)mFilename.c_str();
		char dir[FILE_MAX];
		char full_path[FILE_MAX];
		
		BLI_split_dirfile(filename, dir, NULL);
		BLI_join_dirfile(full_path, dir, filepath.c_str());
		Image *ima = BKE_add_image_file(full_path, 0);
		if (!ima) {
			fprintf(stderr, "Cannot create image. \n");
			return true;
		}
		this->uid_image_map[image->getUniqueId()] = ima;
		
		return true;
	}

	/** When this method is called, the writer must write the light.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeLight( const COLLADAFW::Light* light ) 
	{
		Lamp *lamp = NULL;
		std::string la_id, la_name;
		
		la_id = light->getOriginalId();
		la_name = light->getName();
		if (la_name.size()) lamp = (Lamp*)add_lamp((char*)la_name.c_str());
		else lamp = (Lamp*)add_lamp((char*)la_id.c_str());
		
		if (!lamp) {
			fprintf(stderr, "Cannot create lamp. \n");
			return true;
		}
		if (light->getColor().isValid()) {
			COLLADAFW::Color col = light->getColor();
			lamp->r = col.getRed();
			lamp->g = col.getGreen();
			lamp->b = col.getBlue();
		}
		COLLADAFW::Light::LightType type = light->getLightType();
		switch(type) {
		case COLLADAFW::Light::AMBIENT_LIGHT:
			{
				lamp->type = LA_HEMI;
			}
			break;
		case COLLADAFW::Light::SPOT_LIGHT:
			{
				lamp->type = LA_SPOT;
				lamp->falloff_type = LA_FALLOFF_SLIDERS;
				lamp->att1 = light->getLinearAttenuation().getValue();
				lamp->att2 = light->getQuadraticAttenuation().getValue();
				lamp->spotsize = light->getFallOffAngle().getValue();
				lamp->spotblend = light->getFallOffExponent().getValue();
			}
			break;
		case COLLADAFW::Light::DIRECTIONAL_LIGHT:
			{
				lamp->type = LA_SUN;
			}
			break;
		case COLLADAFW::Light::POINT_LIGHT:
			{
				lamp->type = LA_LOCAL;
				lamp->att1 = light->getLinearAttenuation().getValue();
				lamp->att2 = light->getQuadraticAttenuation().getValue();
			}
			break;
		case COLLADAFW::Light::UNDEFINED:
			{
				fprintf(stderr, "Current lamp type is not supported. \n");
				lamp->type = LA_LOCAL;
			}
			break;
		}
			
		this->uid_lamp_map[light->getUniqueId()] = lamp;
		return true;
	}
	
	// this function is called only for animations that pass COLLADAFW::validate
	virtual bool writeAnimation( const COLLADAFW::Animation* anim ) 
	{
		// return true;
		return anim_importer.write_animation(anim);
	}
	
	// called on post-process stage after writeVisualScenes
	virtual bool writeAnimationList( const COLLADAFW::AnimationList* animationList ) 
	{
		// return true;
		return anim_importer.write_animation_list(animationList);
	}
	
	/** When this method is called, the writer must write the skin controller data.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeSkinControllerData( const COLLADAFW::SkinControllerData* skin ) 
	{
		return armature_importer.write_skin_controller_data(skin);
	}

	// this is called on postprocess, before writeVisualScenes
	virtual bool writeController( const COLLADAFW::Controller* controller ) 
	{
		return armature_importer.write_controller(controller);
	}

	virtual bool writeFormulas( const COLLADAFW::Formulas* formulas )
	{
		return true;
	}

	virtual bool writeKinematicsScene( const COLLADAFW::KinematicsScene* kinematicsScene )
	{
		return true;
	}
};

void DocumentImporter::import(bContext *C, const char *filename)
{
	Writer w(C, filename);
	w.write();
}
