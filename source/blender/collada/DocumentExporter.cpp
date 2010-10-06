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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Jan Diederich, Tod Liverseed.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

extern "C" 
{
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_armature_types.h"
#include "DNA_modifier_types.h"
#include "DNA_userdef_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_fcurve.h"
#include "BKE_animsys.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"
#include "ED_keyframing.h"
#ifdef NAN_BUILDINFO
extern char build_rev[];
#endif
}

#include "MEM_guardedalloc.h"

#include "BKE_blender.h" // version info
#include "BKE_scene.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_action.h" // pose functions
#include "BKE_armature.h"
#include "BKE_image.h"
#include "BKE_utildefines.h"
#include "BKE_object.h"

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_listbase.h"

#include "COLLADASWAsset.h"
#include "COLLADASWLibraryVisualScenes.h"
#include "COLLADASWNode.h"
#include "COLLADASWSource.h"
#include "COLLADASWInstanceGeometry.h"
#include "COLLADASWInputList.h"
#include "COLLADASWPrimitves.h"
#include "COLLADASWVertices.h"
#include "COLLADASWLibraryAnimations.h"
#include "COLLADASWLibraryImages.h"
#include "COLLADASWLibraryEffects.h"
#include "COLLADASWImage.h"
#include "COLLADASWEffectProfile.h"
#include "COLLADASWColorOrTexture.h"
#include "COLLADASWParamTemplate.h"
#include "COLLADASWParamBase.h"
#include "COLLADASWSurfaceInitOption.h"
#include "COLLADASWSampler.h"
#include "COLLADASWScene.h"
#include "COLLADASWTechnique.h"
#include "COLLADASWTexture.h"
#include "COLLADASWLibraryMaterials.h"
#include "COLLADASWBindMaterial.h"
#include "COLLADASWInstanceCamera.h"
#include "COLLADASWInstanceLight.h"
#include "COLLADASWConstants.h"
#include "COLLADASWLibraryControllers.h"
#include "COLLADASWInstanceController.h"
#include "COLLADASWBaseInputElement.h"

#include "collada_internal.h"
#include "DocumentExporter.h"

#include "ArmatureExporter.h"
#include "CameraExporter.h"
#include "GeometryExporter.h"
#include "LightExporter.h"

// can probably go after refactor is complete
#include "InstanceWriter.h"
#include "TransformWriter.h"

#include <vector>
#include <algorithm> // std::find

char *bc_CustomData_get_layer_name(const struct CustomData *data, int type, int n)
{
	int layer_index = CustomData_get_layer_index(data, type);
	if(layer_index < 0) return NULL;

	return data->layers[layer_index+n].name;
}

char *bc_CustomData_get_active_layer_name(const CustomData *data, int type)
{
	/* get the layer index of the active layer of type */
	int layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return NULL;

	return data->layers[layer_index].name;
}


/*
  Utilities to avoid code duplication.
  Definition can take some time to understand, but they should be useful.
*/


template<class Functor>
void forEachObjectInScene(Scene *sce, Functor &f)
{
	Base *base= (Base*) sce->base.first;
	while(base) {
		Object *ob = base->object;
			
		f(ob);

		base= base->next;
	}
}

// used in forEachMaterialInScene
template <class MaterialFunctor>
class ForEachMaterialFunctor
{
	std::vector<std::string> mMat; // contains list of material names, to avoid duplicate calling of f
	MaterialFunctor *f;
public:
	ForEachMaterialFunctor(MaterialFunctor *f) : f(f) { }
	void operator ()(Object *ob)
	{
		int a;
		for(a = 0; a < ob->totcol; a++) {

			Material *ma = give_current_material(ob, a+1);

			if (!ma) continue;

			std::string translated_id = translate_id(id_name(ma));
			if (find(mMat.begin(), mMat.end(), translated_id) == mMat.end()) {
				(*this->f)(ma, ob);

				mMat.push_back(translated_id);
			}
		}
	}
};

// calls f for each unique material linked to each object in sce
// f should have
// void operator()(Material* ma)
template<class Functor>
void forEachMaterialInScene(Scene *sce, Functor &f)
{
	ForEachMaterialFunctor<Functor> matfunc(&f);
	GeometryFunctor gf;
	gf.forEachMeshObjectInScene<ForEachMaterialFunctor<Functor> >(sce, matfunc);
}

// OB_MESH is assumed
std::string getActiveUVLayerName(Object *ob)
{
	Mesh *me = (Mesh*)ob->data;

	int num_layers = CustomData_number_of_layers(&me->fdata, CD_MTFACE);
	if (num_layers)
		return std::string(bc_CustomData_get_active_layer_name(&me->fdata, CD_MTFACE));
		
	return "";
}

class SceneExporter: COLLADASW::LibraryVisualScenes, protected TransformWriter, protected InstanceWriter
{
	ArmatureExporter *arm_exporter;
public:
	SceneExporter(COLLADASW::StreamWriter *sw, ArmatureExporter *arm) : COLLADASW::LibraryVisualScenes(sw),
																		arm_exporter(arm) {}
	
	void exportScene(Scene *sce) {
 		// <library_visual_scenes> <visual_scene>
		std::string id_naming = id_name(sce);
		openVisualScene(translate_id(id_naming), id_naming);

		// write <node>s
		//forEachMeshObjectInScene(sce, *this);
		//forEachCameraObjectInScene(sce, *this);
		//forEachLampObjectInScene(sce, *this);
		exportHierarchy(sce);

		// </visual_scene> </library_visual_scenes>
		closeVisualScene();

		closeLibrary();
	}

	void exportHierarchy(Scene *sce)
	{
		Base *base= (Base*) sce->base.first;
		while(base) {
			Object *ob = base->object;

			if (!ob->parent) {
				switch(ob->type) {
				case OB_MESH:
				case OB_CAMERA:
				case OB_LAMP:
				case OB_EMPTY:
				case OB_ARMATURE:
					// write nodes....
					writeNodes(ob, sce);
					break;
				}
			}

			base= base->next;
		}
	}


	// called for each object
	//void operator()(Object *ob) {
	void writeNodes(Object *ob, Scene *sce)
	{
		COLLADASW::Node node(mSW);
		node.setNodeId(translate_id(id_name(ob)));
		node.setType(COLLADASW::Node::NODE);

		node.start();

		bool is_skinned_mesh = arm_exporter->is_skinned_mesh(ob);

		if (ob->type == OB_MESH && is_skinned_mesh)
			// for skinned mesh we write obmat in <bind_shape_matrix>
			TransformWriter::add_node_transform_identity(node);
		else
			TransformWriter::add_node_transform_ob(node, ob);
		
		// <instance_geometry>
		if (ob->type == OB_MESH) {
			if (is_skinned_mesh) {
				arm_exporter->add_instance_controller(ob);
			}
			else {
				COLLADASW::InstanceGeometry instGeom(mSW);
				instGeom.setUrl(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, get_geometry_id(ob)));

				InstanceWriter::add_material_bindings(instGeom.getBindMaterial(), ob);
			
				instGeom.add();
			}
		}

		// <instance_controller>
		else if (ob->type == OB_ARMATURE) {
			arm_exporter->add_armature_bones(ob, sce);

			// XXX this looks unstable...
			node.end();
		}
		
		// <instance_camera>
		else if (ob->type == OB_CAMERA) {
			COLLADASW::InstanceCamera instCam(mSW, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, get_camera_id(ob)));
			instCam.add();
		}
		
		// <instance_light>
		else if (ob->type == OB_LAMP) {
			COLLADASW::InstanceLight instLa(mSW, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, get_light_id(ob)));
			instLa.add();
		}

		// empty object
		else if (ob->type == OB_EMPTY) {
		}

		// write nodes for child objects
		Base *b = (Base*) sce->base.first;
		while(b) {
			// cob - child object
			Object *cob = b->object;

			if (cob->parent == ob) {
				switch(cob->type) {
				case OB_MESH:
				case OB_CAMERA:
				case OB_LAMP:
				case OB_EMPTY:
				case OB_ARMATURE:
					// write node...
					writeNodes(cob, sce);
					break;
				}
			}

			b = b->next;
		}

		if (ob->type != OB_ARMATURE)
			node.end();
	}
};

class ImagesExporter: COLLADASW::LibraryImages
{
	const char *mfilename;
	std::vector<std::string> mImages; // contains list of written images, to avoid duplicates
public:
	ImagesExporter(COLLADASW::StreamWriter *sw, const char* filename) : COLLADASW::LibraryImages(sw), mfilename(filename)
	{}
	
	void exportImages(Scene *sce)
	{
		openLibrary();

		forEachMaterialInScene(sce, *this);

		closeLibrary();
	}

	void operator()(Material *ma, Object *ob)
	{
		int a;
		for (a = 0; a < MAX_MTEX; a++) {
			MTex *mtex = ma->mtex[a];
			if (mtex && mtex->tex && mtex->tex->ima) {

				Image *image = mtex->tex->ima;
				std::string name(id_name(image));
				name = translate_id(name);
				char rel[FILE_MAX];
				char abs[FILE_MAX];
				char src[FILE_MAX];
				char dir[FILE_MAX];
				
				BLI_split_dirfile(mfilename, dir, NULL);

				BKE_rebase_path(abs, sizeof(abs), rel, sizeof(rel), G.sce, image->name, dir);

				if (abs[0] != '\0') {

					// make absolute source path
					BLI_strncpy(src, image->name, sizeof(src));
					BLI_path_abs(src, G.sce);

					// make dest directory if it doesn't exist
					BLI_make_existing_file(abs);
				
					if (BLI_copy_fileops(src, abs) != 0) {
						fprintf(stderr, "Cannot copy image to file's directory. \n");
					}
				} 
				
				if (find(mImages.begin(), mImages.end(), name) == mImages.end()) {
					COLLADASW::Image img(COLLADABU::URI(COLLADABU::URI::nativePathToUri(rel)), name);
					img.add(mSW);

					mImages.push_back(name);
				}
			}
		}
	}
};

class EffectsExporter: COLLADASW::LibraryEffects
{
public:
	EffectsExporter(COLLADASW::StreamWriter *sw) : COLLADASW::LibraryEffects(sw){}
	void exportEffects(Scene *sce)
	{
		openLibrary();

		forEachMaterialInScene(sce, *this);

		closeLibrary();
	}

	void operator()(Material *ma, Object *ob)
	{
		// create a list of indices to textures of type TEX_IMAGE
		std::vector<int> tex_indices;
		createTextureIndices(ma, tex_indices);

		openEffect(translate_id(id_name(ma)) + "-effect");
		
		COLLADASW::EffectProfile ep(mSW);
		ep.setProfileType(COLLADASW::EffectProfile::COMMON);
		ep.openProfile();
		// set shader type - one of three blinn, phong or lambert
		if (ma->spec_shader == MA_SPEC_BLINN) {
			ep.setShaderType(COLLADASW::EffectProfile::BLINN);
			// shininess
			ep.setShininess(ma->har);
		}
		else if (ma->spec_shader == MA_SPEC_PHONG) {
			ep.setShaderType(COLLADASW::EffectProfile::PHONG);
			// shininess
			ep.setShininess(ma->har);
		}
		else {
			// XXX write warning "Current shader type is not supported" 
			ep.setShaderType(COLLADASW::EffectProfile::LAMBERT);
		}
		// index of refraction
		if (ma->mode & MA_RAYTRANSP) {
			ep.setIndexOfRefraction(ma->ang);
		}
		else {
			ep.setIndexOfRefraction(1.0f);
		}
	
		COLLADASW::ColorOrTexture cot;

		// transparency
		if (ma->mode & MA_TRANSP) {
			// Tod: because we are in A_ONE mode transparency is calculated like this:
			ep.setTransparency(ma->alpha);
			// cot = getcol(1.0f, 1.0f, 1.0f, 1.0f);
			// ep.setTransparent(cot);
		}

		// emission
		cot=getcol(ma->emit, ma->emit, ma->emit, 1.0f);
		ep.setEmission(cot);

		// diffuse multiplied by diffuse intensity
		cot = getcol(ma->r * ma->ref, ma->g * ma->ref, ma->b * ma->ref, 1.0f);
		ep.setDiffuse(cot);

		// ambient
		cot = getcol(ma->ambr, ma->ambg, ma->ambb, 1.0f);
		ep.setAmbient(cot);

		// reflective, reflectivity
		if (ma->mode & MA_RAYMIRROR) {
			cot = getcol(ma->mirr, ma->mirg, ma->mirb, 1.0f);
			ep.setReflective(cot);
			ep.setReflectivity(ma->ray_mirror);
		}
		// else {
		// 	cot = getcol(ma->specr, ma->specg, ma->specb, 1.0f);
		// 	ep.setReflective(cot);
		// 	ep.setReflectivity(ma->spec);
		// }

		// specular
		if (ep.getShaderType() != COLLADASW::EffectProfile::LAMBERT) {
			cot = getcol(ma->specr * ma->spec, ma->specg * ma->spec, ma->specb * ma->spec, 1.0f);
			ep.setSpecular(cot);
		}	

		// XXX make this more readable if possible

		// create <sampler> and <surface> for each image
		COLLADASW::Sampler samplers[MAX_MTEX];
		//COLLADASW::Surface surfaces[MAX_MTEX];
		//void *samp_surf[MAX_MTEX][2];
		void *samp_surf[MAX_MTEX][1];
		
		// image to index to samp_surf map
		// samp_surf[index] stores 2 pointers, sampler and surface
		std::map<std::string, int> im_samp_map;

		unsigned int a, b;
		for (a = 0, b = 0; a < tex_indices.size(); a++) {
			MTex *t = ma->mtex[tex_indices[a]];
			Image *ima = t->tex->ima;
			
			// Image not set for texture
			if(!ima) continue;
			
			std::string key(id_name(ima));
			key = translate_id(key);

			// create only one <sampler>/<surface> pair for each unique image
			if (im_samp_map.find(key) == im_samp_map.end()) {
				// //<newparam> <surface> <init_from>
				// COLLADASW::Surface surface(COLLADASW::Surface::SURFACE_TYPE_2D,
				// 						   key + COLLADASW::Surface::SURFACE_SID_SUFFIX);
				// COLLADASW::SurfaceInitOption sio(COLLADASW::SurfaceInitOption::INIT_FROM);
				// sio.setImageReference(key);
				// surface.setInitOption(sio);

				// COLLADASW::NewParamSurface surface(mSW);
				// surface->setParamType(COLLADASW::CSW_SURFACE_TYPE_2D);
				
				//<newparam> <sampler> <source>
				COLLADASW::Sampler sampler(COLLADASW::Sampler::SAMPLER_TYPE_2D,
										   key + COLLADASW::Sampler::SAMPLER_SID_SUFFIX,
										   key + COLLADASW::Sampler::SURFACE_SID_SUFFIX);
				sampler.setImageId(key);
				// copy values to arrays since they will live longer
				samplers[a] = sampler;
				//surfaces[a] = surface;
				
				// store pointers so they can be used later when we create <texture>s
				samp_surf[b][0] = &samplers[a];
				//samp_surf[b][1] = &surfaces[a];
				
				im_samp_map[key] = b;
				b++;
			}
		}

		// used as fallback when MTex->uvname is "" (this is pretty common)
		// it is indeed the correct value to use in that case
		std::string active_uv(getActiveUVLayerName(ob));

		// write textures
		// XXX very slow
		for (a = 0; a < tex_indices.size(); a++) {
			MTex *t = ma->mtex[tex_indices[a]];
			Image *ima = t->tex->ima;
			
			// Image not set for texture
			if(!ima) continue;

			// we assume map input is always TEXCO_UV

			std::string key(id_name(ima));
			key = translate_id(key);
			int i = im_samp_map[key];
			COLLADASW::Sampler *sampler = (COLLADASW::Sampler*)samp_surf[i][0];
			//COLLADASW::Surface *surface = (COLLADASW::Surface*)samp_surf[i][1];

			std::string uvname = strlen(t->uvname) ? t->uvname : active_uv;

			// color
			if (t->mapto & MAP_COL) {
				ep.setDiffuse(createTexture(ima, uvname, sampler));
			}
			// ambient
			if (t->mapto & MAP_AMB) {
				ep.setAmbient(createTexture(ima, uvname, sampler));
			}
			// specular
			if (t->mapto & MAP_SPEC) {
				ep.setSpecular(createTexture(ima, uvname, sampler));
			}
			// emission
			if (t->mapto & MAP_EMIT) {
				ep.setEmission(createTexture(ima, uvname, sampler));
			}
			// reflective
			if (t->mapto & MAP_REF) {
				ep.setReflective(createTexture(ima, uvname, sampler));
			}
			// alpha
			if (t->mapto & MAP_ALPHA) {
				ep.setTransparent(createTexture(ima, uvname, sampler));
			}
			// extension:
			// Normal map --> Must be stored with <extra> tag as different technique, 
			// since COLLADA doesn't support normal maps, even in current COLLADA 1.5.
			if (t->mapto & MAP_NORM) {
				COLLADASW::Texture texture(key);
				texture.setTexcoord(uvname);
				texture.setSampler(*sampler);
				// technique FCOLLADA, with the <bump> tag, is most likely the best understood,
				// most widespread de-facto standard.
				texture.setProfileName("FCOLLADA");
				texture.setChildElementName("bump");
				ep.addExtraTechniqueColorOrTexture(COLLADASW::ColorOrTexture(texture));
			}
		}
		// performs the actual writing
		ep.addProfileElements();
		bool twoSided = false;
		if (ob->type == OB_MESH && ob->data) {
			Mesh *me = (Mesh*)ob->data;
			if (me->flag & ME_TWOSIDED)
				twoSided = true;
		}
		if (twoSided)
			ep.addExtraTechniqueParameter("GOOGLEEARTH", "show_double_sided", 1);
		ep.addExtraTechniques(mSW);

		ep.closeProfile();
		if (twoSided)
			mSW->appendTextBlock("<extra><technique profile=\"MAX3D\"><double_sided>1</double_sided></technique></extra>");
		closeEffect();	
	}
	
	COLLADASW::ColorOrTexture createTexture(Image *ima,
											std::string& uv_layer_name,
											COLLADASW::Sampler *sampler
											/*COLLADASW::Surface *surface*/)
	{
		
		COLLADASW::Texture texture(translate_id(id_name(ima)));
		texture.setTexcoord(uv_layer_name);
		//texture.setSurface(*surface);
		texture.setSampler(*sampler);
		
		COLLADASW::ColorOrTexture cot(texture);
		return cot;
	}
	
	COLLADASW::ColorOrTexture getcol(float r, float g, float b, float a)
	{
		COLLADASW::Color color(r,g,b,a);
		COLLADASW::ColorOrTexture cot(color);
		return cot;
	}
	
	//returns the array of mtex indices which have image 
	//need this for exporting textures
	void createTextureIndices(Material *ma, std::vector<int> &indices)
	{
		indices.clear();

		for (int a = 0; a < MAX_MTEX; a++) {
			if (ma->mtex[a] &&
				ma->mtex[a]->tex &&
				ma->mtex[a]->tex->type == TEX_IMAGE &&
				ma->mtex[a]->texco == TEXCO_UV){
				indices.push_back(a);
			}
		}
	}
};

class MaterialsExporter: COLLADASW::LibraryMaterials
{
public:
	MaterialsExporter(COLLADASW::StreamWriter *sw): COLLADASW::LibraryMaterials(sw){}
	void exportMaterials(Scene *sce)
	{
		openLibrary();

		forEachMaterialInScene(sce, *this);

		closeLibrary();
	}

	void operator()(Material *ma, Object *ob)
	{
		std::string name(id_name(ma));

		openMaterial(translate_id(name), name);

		std::string efid = translate_id(name) + "-effect";
		addInstanceEffect(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, efid));

		closeMaterial();
	}
};

// TODO: it would be better to instantiate animations rather than create a new one per object
// COLLADA allows this through multiple <channel>s in <animation>.
// For this to work, we need to know objects that use a certain action.
class AnimationExporter: COLLADASW::LibraryAnimations
{
	Scene *scene;

public:

	AnimationExporter(COLLADASW::StreamWriter *sw): COLLADASW::LibraryAnimations(sw) {}

	void exportAnimations(Scene *sce)
	{
		this->scene = sce;

		openLibrary();
		
		forEachObjectInScene(sce, *this);
		
		closeLibrary();
	}

	// called for each exported object
	void operator() (Object *ob) 
	{
		if (!ob->adt || !ob->adt->action) return;
		
		FCurve *fcu = (FCurve*)ob->adt->action->curves.first;
		
		if (ob->type == OB_ARMATURE) {
			if (!ob->data) return;

			bArmature *arm = (bArmature*)ob->data;
			for (Bone *bone = (Bone*)arm->bonebase.first; bone; bone = bone->next)
				write_bone_animation(ob, bone);
		}
		else {
			while (fcu) {
				// TODO "rotation_quaternion" is also possible for objects (although euler is default)
				if ((!strcmp(fcu->rna_path, "location") || !strcmp(fcu->rna_path, "scale")) ||
					(!strcmp(fcu->rna_path, "rotation_euler") && ob->rotmode == ROT_MODE_EUL))
					dae_animation(fcu, id_name(ob));

				fcu = fcu->next;
			}
		}
	}

protected:

	void dae_animation(FCurve *fcu, std::string ob_name)
	{
		const char *axis_names[] = {"X", "Y", "Z"};
		const char *axis_name = NULL;
		char anim_id[200];
		
		if (fcu->array_index < 3)
			axis_name = axis_names[fcu->array_index];

		BLI_snprintf(anim_id, sizeof(anim_id), "%s_%s_%s", (char*)translate_id(ob_name).c_str(),
					 fcu->rna_path, axis_names[fcu->array_index]);

		// check rna_path is one of: rotation, scale, location

		openAnimation(anim_id, COLLADABU::Utils::EMPTY_STRING);

		// create input source
		std::string input_id = create_source_from_fcurve(Sampler::INPUT, fcu, anim_id, axis_name);

		// create output source
		std::string output_id = create_source_from_fcurve(Sampler::OUTPUT, fcu, anim_id, axis_name);

		// create interpolations source
		std::string interpolation_id = create_interpolation_source(fcu->totvert, anim_id, axis_name);

		std::string sampler_id = std::string(anim_id) + SAMPLER_ID_SUFFIX;
		COLLADASW::LibraryAnimations::Sampler sampler(sampler_id);
		std::string empty;
		sampler.addInput(Sampler::INPUT, COLLADABU::URI(empty, input_id));
		sampler.addInput(Sampler::OUTPUT, COLLADABU::URI(empty, output_id));

		// this input is required
		sampler.addInput(Sampler::INTERPOLATION, COLLADABU::URI(empty, interpolation_id));

		addSampler(sampler);

		std::string target = translate_id(ob_name)
			+ "/" + get_transform_sid(fcu->rna_path, -1, axis_name, true);
		addChannel(COLLADABU::URI(empty, sampler_id), target);

		closeAnimation();
	}

	void write_bone_animation(Object *ob_arm, Bone *bone)
	{
		if (!ob_arm->adt)
			return;

		for (int i = 0; i < 3; i++)
			sample_and_write_bone_animation(ob_arm, bone, i);

		for (Bone *child = (Bone*)bone->childbase.first; child; child = child->next)
			write_bone_animation(ob_arm, child);
	}

	void sample_and_write_bone_animation(Object *ob_arm, Bone *bone, int transform_type)
	{
		bArmature *arm = (bArmature*)ob_arm->data;
		int flag = arm->flag;
		std::vector<float> fra;
		char prefix[256];

		BLI_snprintf(prefix, sizeof(prefix), "pose.bones[\"%s\"]", bone->name);

		bPoseChannel *pchan = get_pose_channel(ob_arm->pose, bone->name);
		if (!pchan)
			return;

		switch (transform_type) {
		case 0:
			find_rotation_frames(ob_arm, fra, prefix, pchan->rotmode);
			break;
		case 1:
			find_frames(ob_arm, fra, prefix, "scale");
			break;
		case 2:
			find_frames(ob_arm, fra, prefix, "location");
			break;
		default:
			return;
		}

		// exit rest position
		if (flag & ARM_RESTPOS) {
			arm->flag &= ~ARM_RESTPOS;
			where_is_pose(scene, ob_arm);
		}

		if (fra.size()) {
			float *v = (float*)MEM_callocN(sizeof(float) * 3 * fra.size(), "temp. anim frames");
			sample_animation(v, fra, transform_type, bone, ob_arm);

			if (transform_type == 0) {
				// write x, y, z curves separately if it is rotation
				float *c = (float*)MEM_callocN(sizeof(float) * fra.size(), "temp. anim frames");
				for (int i = 0; i < 3; i++) {
					for (unsigned int j = 0; j < fra.size(); j++)
						c[j] = v[j * 3 + i];

					dae_bone_animation(fra, c, transform_type, i, id_name(ob_arm), bone->name);
				}
				MEM_freeN(c);
			}
			else {
				// write xyz at once if it is location or scale
				dae_bone_animation(fra, v, transform_type, -1, id_name(ob_arm), bone->name);
			}

			MEM_freeN(v);
		}

		// restore restpos
		if (flag & ARM_RESTPOS) 
			arm->flag = flag;
		where_is_pose(scene, ob_arm);
	}

	void sample_animation(float *v, std::vector<float> &frames, int type, Bone *bone, Object *ob_arm)
	{
		bPoseChannel *pchan, *parchan = NULL;
		bPose *pose = ob_arm->pose;

		pchan = get_pose_channel(pose, bone->name);

		if (!pchan)
			return;

		parchan = pchan->parent;

		enable_fcurves(ob_arm->adt->action, bone->name);

		std::vector<float>::iterator it;
		for (it = frames.begin(); it != frames.end(); it++) {
			float mat[4][4], ipar[4][4];

			float ctime = bsystem_time(scene, ob_arm, *it, 0.0f);

			BKE_animsys_evaluate_animdata(&ob_arm->id, ob_arm->adt, *it, ADT_RECALC_ANIM);
			where_is_pose_bone(scene, ob_arm, pchan, ctime, 1);

			// compute bone local mat
			if (bone->parent) {
				invert_m4_m4(ipar, parchan->pose_mat);
				mul_m4_m4m4(mat, pchan->pose_mat, ipar);
			}
			else
				copy_m4_m4(mat, pchan->pose_mat);

			switch (type) {
			case 0:
				mat4_to_eul(v, mat);
				break;
			case 1:
				mat4_to_size(v, mat);
				break;
			case 2:
				copy_v3_v3(v, mat[3]);
				break;
			}

			v += 3;
		}

		enable_fcurves(ob_arm->adt->action, NULL);
	}

	// dae_bone_animation -> add_bone_animation
	// (blend this into dae_bone_animation)
	void dae_bone_animation(std::vector<float> &fra, float *v, int tm_type, int axis, std::string ob_name, std::string bone_name)
	{
		const char *axis_names[] = {"X", "Y", "Z"};
		const char *axis_name = NULL;
		char anim_id[200];
		bool is_rot = tm_type == 0;
		
		if (!fra.size())
			return;

		char rna_path[200];
		BLI_snprintf(rna_path, sizeof(rna_path), "pose.bones[\"%s\"].%s", bone_name.c_str(),
					 tm_type == 0 ? "rotation_quaternion" : (tm_type == 1 ? "scale" : "location"));

		if (axis > -1)
			axis_name = axis_names[axis];
		
		std::string transform_sid = get_transform_sid(NULL, tm_type, axis_name, false);
		
		BLI_snprintf(anim_id, sizeof(anim_id), "%s_%s_%s", (char*)translate_id(ob_name).c_str(),
					 (char*)translate_id(bone_name).c_str(), (char*)transform_sid.c_str());

		openAnimation(anim_id, COLLADABU::Utils::EMPTY_STRING);

		// create input source
		std::string input_id = create_source_from_vector(Sampler::INPUT, fra, is_rot, anim_id, axis_name);

		// create output source
		std::string output_id;
		if (axis == -1)
			output_id = create_xyz_source(v, fra.size(), anim_id);
		else
			output_id = create_source_from_array(Sampler::OUTPUT, v, fra.size(), is_rot, anim_id, axis_name);

		// create interpolations source
		std::string interpolation_id = create_interpolation_source(fra.size(), anim_id, axis_name);

		std::string sampler_id = std::string(anim_id) + SAMPLER_ID_SUFFIX;
		COLLADASW::LibraryAnimations::Sampler sampler(sampler_id);
		std::string empty;
		sampler.addInput(Sampler::INPUT, COLLADABU::URI(empty, input_id));
		sampler.addInput(Sampler::OUTPUT, COLLADABU::URI(empty, output_id));

		// TODO create in/out tangents source

		// this input is required
		sampler.addInput(Sampler::INTERPOLATION, COLLADABU::URI(empty, interpolation_id));

		addSampler(sampler);

		std::string target = translate_id(ob_name + "_" + bone_name) + "/" + transform_sid;
		addChannel(COLLADABU::URI(empty, sampler_id), target);

		closeAnimation();
	}

	float convert_time(float frame)
	{
		return FRA2TIME(frame);
	}

	float convert_angle(float angle)
	{
		return COLLADABU::Math::Utils::radToDegF(angle);
	}

	std::string get_semantic_suffix(Sampler::Semantic semantic)
	{
		switch(semantic) {
		case Sampler::INPUT:
			return INPUT_SOURCE_ID_SUFFIX;
		case Sampler::OUTPUT:
			return OUTPUT_SOURCE_ID_SUFFIX;
		case Sampler::INTERPOLATION:
			return INTERPOLATION_SOURCE_ID_SUFFIX;
		case Sampler::IN_TANGENT:
			return INTANGENT_SOURCE_ID_SUFFIX;
		case Sampler::OUT_TANGENT:
			return OUTTANGENT_SOURCE_ID_SUFFIX;
		default:
			break;
		}
		return "";
	}

	void add_source_parameters(COLLADASW::SourceBase::ParameterNameList& param,
							   Sampler::Semantic semantic, bool is_rot, const char *axis)
	{
		switch(semantic) {
		case Sampler::INPUT:
			param.push_back("TIME");
			break;
		case Sampler::OUTPUT:
			if (is_rot) {
				param.push_back("ANGLE");
			}
			else {
				if (axis) {
					param.push_back(axis);
				}
				else {
					param.push_back("X");
					param.push_back("Y");
					param.push_back("Z");
				}
			}
			break;
		case Sampler::IN_TANGENT:
		case Sampler::OUT_TANGENT:
			param.push_back("X");
			param.push_back("Y");
			break;
		default:
			break;
		}
	}

	void get_source_values(BezTriple *bezt, Sampler::Semantic semantic, bool rotation, float *values, int *length)
	{
		switch (semantic) {
		case Sampler::INPUT:
			*length = 1;
			values[0] = convert_time(bezt->vec[1][0]);
			break;
		case Sampler::OUTPUT:
			*length = 1;
			if (rotation) {
				values[0] = convert_angle(bezt->vec[1][1]);
			}
			else {
				values[0] = bezt->vec[1][1];
			}
			break;
		case Sampler::IN_TANGENT:
		case Sampler::OUT_TANGENT:
			// XXX
			*length = 2;
			break;
		default:
			*length = 0;
			break;
		}
	}

	std::string create_source_from_fcurve(Sampler::Semantic semantic, FCurve *fcu, const std::string& anim_id, const char *axis_name)
	{
		std::string source_id = anim_id + get_semantic_suffix(semantic);

		//bool is_rotation = !strcmp(fcu->rna_path, "rotation");
		bool is_rotation = false;
		
		if (strstr(fcu->rna_path, "rotation")) is_rotation = true;
		
		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(fcu->totvert);
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		add_source_parameters(param, semantic, is_rotation, axis_name);

		source.prepareToAppendValues();

		for (unsigned int i = 0; i < fcu->totvert; i++) {
			float values[3]; // be careful!
			int length = 0;

			get_source_values(&fcu->bezt[i], semantic, is_rotation, values, &length);
			for (int j = 0; j < length; j++)
				source.appendValues(values[j]);
		}

		source.finish();

		return source_id;
	}

	std::string create_source_from_array(Sampler::Semantic semantic, float *v, int tot, bool is_rot, const std::string& anim_id, const char *axis_name)
	{
		std::string source_id = anim_id + get_semantic_suffix(semantic);

		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(tot);
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		add_source_parameters(param, semantic, is_rot, axis_name);

		source.prepareToAppendValues();

		for (int i = 0; i < tot; i++) {
			float val = v[i];
			if (semantic == Sampler::INPUT)
				val = convert_time(val);
			else if (is_rot)
				val = convert_angle(val);
			source.appendValues(val);
		}

		source.finish();

		return source_id;
	}

	std::string create_source_from_vector(Sampler::Semantic semantic, std::vector<float> &fra, bool is_rot, const std::string& anim_id, const char *axis_name)
	{
		std::string source_id = anim_id + get_semantic_suffix(semantic);

		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(fra.size());
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		add_source_parameters(param, semantic, is_rot, axis_name);

		source.prepareToAppendValues();

		std::vector<float>::iterator it;
		for (it = fra.begin(); it != fra.end(); it++) {
			float val = *it;
			if (semantic == Sampler::INPUT)
				val = convert_time(val);
			else if (is_rot)
				val = convert_angle(val);
			source.appendValues(val);
		}

		source.finish();

		return source_id;
	}

	// only used for sources with OUTPUT semantic
	std::string create_xyz_source(float *v, int tot, const std::string& anim_id)
	{
		Sampler::Semantic semantic = Sampler::OUTPUT;
		std::string source_id = anim_id + get_semantic_suffix(semantic);

		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(tot);
		source.setAccessorStride(3);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		add_source_parameters(param, semantic, false, NULL);

		source.prepareToAppendValues();

		for (int i = 0; i < tot; i++) {
			source.appendValues(*v, *(v + 1), *(v + 2));
			v += 3;
		}

		source.finish();

		return source_id;
	}

	std::string create_interpolation_source(int tot, const std::string& anim_id, const char *axis_name)
	{
		std::string source_id = anim_id + get_semantic_suffix(Sampler::INTERPOLATION);

		COLLADASW::NameSource source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(tot);
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		param.push_back("INTERPOLATION");

		source.prepareToAppendValues();

		for (int i = 0; i < tot; i++) {
			source.appendValues(LINEAR_NAME);
		}

		source.finish();

		return source_id;
	}

	// for rotation, axis name is always appended and the value of append_axis is ignored
	std::string get_transform_sid(char *rna_path, int tm_type, const char *axis_name, bool append_axis)
	{
		std::string tm_name;

		// when given rna_path, determine tm_type from it
		if (rna_path) {
			char *name = extract_transform_name(rna_path);

			if (strstr(name, "rotation"))
				tm_type = 0;
			else if (!strcmp(name, "scale"))
				tm_type = 1;
			else if (!strcmp(name, "location"))
				tm_type = 2;
			else
				tm_type = -1;
		}

		switch (tm_type) {
		case 0:
			return std::string("rotation") + std::string(axis_name) + ".ANGLE";
		case 1:
			tm_name = "scale";
			break;
		case 2:
			tm_name = "location";
			break;
		default:
			tm_name = "";
			break;
		}

		if (tm_name.size()) {
			if (append_axis)
				return tm_name + std::string(".") + std::string(axis_name);
			else
				return tm_name;
		}

		return std::string("");
	}

	char *extract_transform_name(char *rna_path)
	{
		char *dot = strrchr(rna_path, '.');
		return dot ? (dot + 1) : rna_path;
	}

	void find_frames(Object *ob, std::vector<float> &fra, const char *prefix, const char *tm_name)
	{
		FCurve *fcu= (FCurve*)ob->adt->action->curves.first;

		for (; fcu; fcu = fcu->next) {
			if (prefix && strncmp(prefix, fcu->rna_path, strlen(prefix)))
				continue;

			char *name = extract_transform_name(fcu->rna_path);
			if (!strcmp(name, tm_name)) {
				for (unsigned int i = 0; i < fcu->totvert; i++) {
					float f = fcu->bezt[i].vec[1][0];
					if (std::find(fra.begin(), fra.end(), f) == fra.end())
						fra.push_back(f);
				}
			}
		}

		// keep the keys in ascending order
		std::sort(fra.begin(), fra.end());
	}

	void find_rotation_frames(Object *ob, std::vector<float> &fra, const char *prefix, int rotmode)
	{
		if (rotmode > 0)
			find_frames(ob, fra, prefix, "rotation_euler");
		else if (rotmode == ROT_MODE_QUAT)
			find_frames(ob, fra, prefix, "rotation_quaternion");
		/*else if (rotmode == ROT_MODE_AXISANGLE)
			;*/
	}

	// enable fcurves driving a specific bone, disable all the rest
	// if bone_name = NULL enable all fcurves
	void enable_fcurves(bAction *act, char *bone_name)
	{
		FCurve *fcu;
		char prefix[200];

		if (bone_name)
			BLI_snprintf(prefix, sizeof(prefix), "pose.bones[\"%s\"]", bone_name);

		for (fcu = (FCurve*)act->curves.first; fcu; fcu = fcu->next) {
			if (bone_name) {
				if (!strncmp(fcu->rna_path, prefix, strlen(prefix)))
					fcu->flag &= ~FCURVE_DISABLED;
				else
					fcu->flag |= FCURVE_DISABLED;
			}
			else {
				fcu->flag &= ~FCURVE_DISABLED;
			}
		}
	}
};

void DocumentExporter::exportCurrentScene(Scene *sce, const char* filename)
{
	clear_global_id_map();
	
	COLLADABU::NativeString native_filename =
		COLLADABU::NativeString(std::string(filename));
	COLLADASW::StreamWriter sw(native_filename);

	// open <Collada>
	sw.startDocument();

	// <asset>
	COLLADASW::Asset asset(&sw);
	// XXX ask blender devs about this?
	asset.setUnit("decimetre", 0.1);
	asset.setUpAxisType(COLLADASW::Asset::Z_UP);
	// TODO: need an Author field in userpref
	if(strlen(U.author) > 0) {
		asset.getContributor().mAuthor = U.author;
	}
	else {
		asset.getContributor().mAuthor = "Blender User";
	}
#ifdef NAN_BUILDINFO
	char version_buf[128];
	sprintf(version_buf, "Blender %d.%02d.%d r%s", BLENDER_VERSION/100, BLENDER_VERSION%100, BLENDER_SUBVERSION, build_rev);
	asset.getContributor().mAuthoringTool = version_buf;
#else
	asset.getContributor().mAuthoringTool = "Blender 2.5x";
#endif
	asset.add();
	
	// <library_cameras>
	CamerasExporter ce(&sw);
	ce.exportCameras(sce);
	
	// <library_lights>
	LightsExporter le(&sw);
	le.exportLights(sce);

	// <library_images>
	ImagesExporter ie(&sw, filename);
	ie.exportImages(sce);
	
	// <library_effects>
	EffectsExporter ee(&sw);
	ee.exportEffects(sce);
	
	// <library_materials>
	MaterialsExporter me(&sw);
	me.exportMaterials(sce);

	// <library_geometries>
	GeometryExporter ge(&sw);
	ge.exportGeom(sce);

	// <library_animations>
	AnimationExporter ae(&sw);
	ae.exportAnimations(sce);

	// <library_controllers>
	ArmatureExporter arm_exporter(&sw);
	arm_exporter.export_controllers(sce);

	// <library_visual_scenes>
	SceneExporter se(&sw, &arm_exporter);
	se.exportScene(sce);
	
	// <scene>
	std::string scene_name(translate_id(id_name(sce)));
	COLLADASW::Scene scene(&sw, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING,
											   scene_name));
	scene.add();
	
	// close <Collada>
	sw.endDocument();

}

void DocumentExporter::exportScenes(const char* filename)
{
}

/*

NOTES:

* AnimationExporter::sample_animation enables all curves on armature, this is undesirable for a user

 */
