#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"

extern "C" 
{
#include "BKE_DerivedMesh.h"
}
#include "BKE_scene.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"

#include "BLI_arithb.h"

#include "DocumentExporter.h"

#include <COLLADASWAsset.h>
#include <COLLADASWLibraryVisualScenes.h>
#include <COLLADASWNode.h>
#include <COLLADASWLibraryGeometries.h>
#include <COLLADASWSource.h>
#include <COLLADASWInstanceGeometry.h>
#include <COLLADASWInputList.h>
#include <COLLADASWPrimitves.h>
#include <COLLADASWVertices.h>
#include <COLLADASWLibraryImages.h>
#include <COLLADASWLibraryEffects.h>
#include <COLLADASWImage.h>
#include <COLLADASWEffectProfile.h>
#include <COLLADASWColorOrTexture.h>
#include <COLLADASWParamTemplate.h>
#include <COLLADASWParamBase.h>
#include <COLLADASWSurfaceInitOption.h>
#include <COLLADASWSampler.h>
#include <COLLADASWScene.h>
#include <COLLADASWSurface.h>
#include <COLLADASWTechnique.h>
#include <COLLADASWTexture.h>
#include <COLLADASWLibraryMaterials.h>
#include <COLLADASWBindMaterial.h>
#include <COLLADASWLibraryCameras.h>
#include <COLLADASWLibraryLights.h>
#include <COLLADASWInstanceCamera.h>
#include <COLLADASWInstanceLight.h>
#include <COLLADASWCameraOptic.h>

#include <vector>
#include <algorithm> // std::find
#include <math.h>

// TODO: this can handy in BLI_arith.b
// This function assumes that quat is normalized.
// The following document was used as reference:
// http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToAngle/index.htm
void QuatToAxisAngle(float *q, float *axis, float *angle)
{
	// quat to axis angle
	*angle = 2 * acos(q[0]);
	float divisor = sqrt(1 - q[0] * q[0]);

	// test to avoid divide by zero, divisor is always positive
	if (divisor < 0.001f ) {
		axis[0] = 1.0f;
		axis[1] = 0.0f;
		axis[2] = 0.0f;
	}
	else {
		axis[0] = q[1] / divisor;
		axis[1] = q[2] / divisor;
		axis[2] = q[3] / divisor;
	}
}

char *CustomData_get_layer_name(const struct CustomData *data, int type, int n)
{
	int layer_index = CustomData_get_layer_index(data, type);
	if(layer_index < 0) return NULL;

	return data->layers[layer_index+n].name;
}

char *CustomData_get_active_layer_name(const CustomData *data, int type)
{
	/* get the layer index of the active layer of type */
	int layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return NULL;

	return data->layers[layer_index].name;
}

std::string id_name(void *id)
{
	return ((ID*)id)->name + 2;
}

/*
  Utilities to avoid code duplication.
  Definition can take some time to understand, but they should be useful.
*/

// f should have
// void operator()(Object* ob)
template<class Functor>
void forEachMeshObjectInScene(Scene *sce, Functor &f)
{
	Base *base= (Base*) sce->base.first;
	while(base) {
		Object *ob = base->object;
			
		if (ob->type == OB_MESH && ob->data) {
			f(ob);
		}
		base= base->next;
	}
}

template<class Functor>
void forEachCameraObjectInScene(Scene *sce, Functor &f)
{
	Base *base= (Base*) sce->base.first;
	while(base) {
		Object *ob = base->object;
			
		if (ob->type == OB_CAMERA && ob->data) {
			f(ob);
		}
		base= base->next;
	}
}

template<class Functor>
void forEachLampObjectInScene(Scene *sce, Functor &f)
{
	Base *base= (Base*) sce->base.first;
	while(base) {
		Object *ob = base->object;
			
		if (ob->type == OB_LAMP && ob->data) {
			f(ob);
		}
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

			if (find(mMat.begin(), mMat.end(), id_name(ma)) == mMat.end()) {
				(*this->f)(ma, ob);

				mMat.push_back(id_name(ma));
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
	forEachMeshObjectInScene(sce, matfunc);
}

// OB_MESH is assumed
std::string getActiveUVLayerName(Object *ob)
{
	Mesh *me = (Mesh*)ob->data;

	int num_layers = CustomData_number_of_layers(&me->fdata, CD_MTFACE);
	if (num_layers)
		return std::string(CustomData_get_active_layer_name(&me->fdata, CD_MTFACE));
		
	return "";
}

// TODO: optimize UV sets by making indexed list with duplicates removed
class GeometryExporter : COLLADASW::LibraryGeometries
{
	Scene *mScene;
public:
	GeometryExporter(COLLADASW::StreamWriter *sw) : COLLADASW::LibraryGeometries(sw) {}

	void exportGeom(Scene *sce)
	{
		openLibrary();

		mScene = sce;
		forEachMeshObjectInScene(sce, *this);

		closeLibrary();
	}

	void operator()(Object *ob)
	{
		// XXX don't use DerivedMesh, Mesh instead?
		
		DerivedMesh *dm = mesh_get_derived_final(mScene, ob, CD_MASK_BAREMESH);
		Mesh *me = (Mesh*)ob->data;
		std::string geom_name(id_name(ob));
		
		// openMesh(geoId, geoName, meshId)
		openMesh(geom_name, "", "");
		
		// writes <source> for vertex coords
		createVertsSource(geom_name, dm);
		
		// writes <source> for normal coords
		createNormalsSource(geom_name, dm);

		int has_uvs = CustomData_has_layer(&me->fdata, CD_MTFACE);
		
		// writes <source> for uv coords if mesh has uv coords
		if (has_uvs) {
			createTexcoordsSource(geom_name, dm, (Mesh*)ob->data);
		}
		// <vertices>
		COLLADASW::Vertices verts(mSW);
		verts.setId(getIdBySemantics(geom_name, COLLADASW::VERTEX));
		COLLADASW::InputList &input_list = verts.getInputList();
		COLLADASW::Input input(COLLADASW::POSITION,
							   getUrlBySemantics(geom_name, COLLADASW::POSITION));
		input_list.push_back(input);
		verts.add();

		// XXX slow		
		if (ob->totcol) {
			for(int a = 0; a < ob->totcol; a++)	{
				createPolylist(true, a, has_uvs, ob, dm, geom_name);
			}
		}
		else {
			createPolylist(false, 0, has_uvs, ob, dm, geom_name);
		}
		
		closeMesh();
		closeGeometry();
		
		dm->release(dm);
		
	}

	// powerful because it handles both cases when there is material and when there's not
	void createPolylist(bool has_material,
						int material_index,
						bool has_uvs,
						Object *ob,
						DerivedMesh *dm,
						std::string& geom_name)
	{
		MFace *mfaces = dm->getFaceArray(dm);
		int totfaces = dm->getNumFaces(dm);
		Mesh *me = (Mesh*)ob->data;

		// <vcount>
		int i;
		int faces_in_polylist = 0;
		std::vector<unsigned long> vcount_list;

		// count faces with this material
		for (i = 0; i < totfaces; i++) {
			MFace *f = &mfaces[i];
			
			if ((has_material && f->mat_nr == material_index) || !has_material) {
				faces_in_polylist++;
				if (f->v4 == 0) {
					vcount_list.push_back(3);
				}
				else {
					vcount_list.push_back(4);
				}
			}
		}

		// no faces using this material
		if (faces_in_polylist == 0) {
			return;
		}
			
		Material *ma = has_material ? give_current_material(ob, material_index + 1) : NULL;
		COLLADASW::Polylist polylist(mSW);
			
		// sets count attribute in <polylist>
		polylist.setCount(faces_in_polylist);
			
		// sets material name
		if (has_material)
			polylist.setMaterial(id_name(ma));
				
		COLLADASW::InputList &til = polylist.getInputList();
			
		// creates <input> in <polylist> for vertices 
		COLLADASW::Input input1(COLLADASW::VERTEX, getUrlBySemantics
								(geom_name, COLLADASW::VERTEX), 0);
			
		// creates <input> in <polylist> for normals
		COLLADASW::Input input2(COLLADASW::NORMAL, getUrlBySemantics
								(geom_name, COLLADASW::NORMAL), 0);
			
		til.push_back(input1);
		til.push_back(input2);
			
		// if mesh has uv coords writes <input> for TEXCOORD
		int num_layers = CustomData_number_of_layers(&me->fdata, CD_MTFACE);

		for (i = 0; i < num_layers; i++) {
			char *name = CustomData_get_layer_name(&me->fdata, CD_MTFACE, i);
			COLLADASW::Input input3(COLLADASW::TEXCOORD,
									makeUrl(makeTexcoordSourceId(geom_name, i)),
									1, // offset always 1, this is only until we have optimized UV sets
									i  // set number equals UV layer index
									);
			til.push_back(input3);
		}
			
		// sets <vcount>
		polylist.setVCountList(vcount_list);
			
		// performs the actual writing
		polylist.prepareToAppendValues();
			
		// <p>
		int texindex = 0;
		for (i = 0; i < totfaces; i++) {
			MFace *f = &mfaces[i];

			if ((has_material && f->mat_nr == material_index) || !has_material) {

				unsigned int *v = &f->v1;
				for (int j = 0; j < (f->v4 == 0 ? 3 : 4); j++) {
					polylist.appendValues(v[j]);

					if (has_uvs)
						polylist.appendValues(texindex + j);
				}
			}

			texindex += 3;
			if (f->v4 != 0)
				texindex++;
		}
			
		polylist.finish();
	}
	
	// creates <source> for positions
	void createVertsSource(std::string geom_name, DerivedMesh *dm)
	{
		int totverts = dm->getNumVerts(dm);
		MVert *verts = dm->getVertArray(dm);
		
		
		COLLADASW::FloatSourceF source(mSW);
		source.setId(getIdBySemantics(geom_name, COLLADASW::POSITION));
		source.setArrayId(getIdBySemantics(geom_name, COLLADASW::POSITION) +
						  ARRAY_ID_SUFFIX);
		source.setAccessorCount(totverts);
		source.setAccessorStride(3);
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		param.push_back("X");
		param.push_back("Y");
		param.push_back("Z");
		/*main function, it creates <source id = "">, <float_array id = ""
		  count = ""> */
		source.prepareToAppendValues();
		//appends data to <float_array>
		int i = 0;
		for (i = 0; i < totverts; i++) {
			source.appendValues(verts[i].co[0], verts[i].co[1], verts[i].co[2]);
			
		}
		
		source.finish();
	
	}

	std::string makeTexcoordSourceId(std::string& geom_name, int layer_index)
	{
		char suffix[20];
		sprintf(suffix, "-%d", layer_index);
		return getIdBySemantics(geom_name, COLLADASW::TEXCOORD) + suffix;
	}

	//creates <source> for texcoords
	void createTexcoordsSource(std::string geom_name, DerivedMesh *dm, Mesh *me)
	{

		int totfaces = dm->getNumFaces(dm);
		MFace *mfaces = dm->getFaceArray(dm);
		int totuv = 0;
		int i;

		// count totuv
		for (i = 0; i < totfaces; i++) {
			MFace *f = &mfaces[i];
			if (f->v4 == 0) {
				totuv+=3;
			}
			else {
				totuv+=4;
			}
		}

		int num_layers = CustomData_number_of_layers(&me->fdata, CD_MTFACE);

		// write <source> for each layer
		// each <source> will get id like meshName + "map-channel-1"
		for (int a = 0; a < num_layers; a++) {
			MTFace *tface = (MTFace*)CustomData_get_layer_n(&me->fdata, CD_MTFACE, a);
			char *name = CustomData_get_layer_name(&me->fdata, CD_MTFACE, a);
			
			COLLADASW::FloatSourceF source(mSW);
			std::string layer_id = makeTexcoordSourceId(geom_name, a);
			source.setId(layer_id);
			source.setArrayId(layer_id + ARRAY_ID_SUFFIX);
			
			source.setAccessorCount(totuv);
			source.setAccessorStride(2);
			COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
			param.push_back("X");
			param.push_back("Y");
			
			source.prepareToAppendValues();
			
			for (i = 0; i < totfaces; i++) {
				MFace *f = &mfaces[i];
				
				for (int j = 0; j < (f->v4 == 0 ? 3 : 4); j++) {
					source.appendValues(tface[i].uv[j][0],
										tface[i].uv[j][1]);
				}
			}
			
			source.finish();
		}
	}


	//creates <source> for normals
	void createNormalsSource(std::string geom_name, DerivedMesh *dm)
	{
		int totverts = dm->getNumVerts(dm);
		MVert *verts = dm->getVertArray(dm);
		
		COLLADASW::FloatSourceF source(mSW);
		source.setId(getIdBySemantics(geom_name, COLLADASW::NORMAL));
		source.setArrayId(getIdBySemantics(geom_name, COLLADASW::NORMAL) +
						  ARRAY_ID_SUFFIX);
		source.setAccessorCount(totverts);
		source.setAccessorStride(3);
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		param.push_back("X");
		param.push_back("Y");
		param.push_back("Z");
		
		source.prepareToAppendValues();
		
		int i = 0;
		
		for( i = 0; i < totverts; ++i ){
			
			source.appendValues(float(verts[i].no[0]/32767.0),
								float(verts[i].no[1]/32767.0),
								float(verts[i].no[2]/32767.0));
				
		}
		source.finish();
	}
	
	std::string getIdBySemantics(std::string geom_name, COLLADASW::Semantics type, std::string other_suffix = "") {
		return geom_name + getSuffixBySemantic(type) + other_suffix;
	}
	
	
	COLLADASW::URI getUrlBySemantics(std::string geom_name, COLLADASW::Semantics type, std::string other_suffix = "") {
		
		std::string id(getIdBySemantics(geom_name, type, other_suffix));
		return COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, id);
		
	}

	COLLADASW::URI makeUrl(std::string id)
	{
		return COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, id);
	}
	

	/*	int getTriCount(MFace *faces, int totface) {
		int i;
		int tris = 0;
		for (i = 0; i < totface; i++) {
			// if quad
			if (faces[i].v4 != 0)
				tris += 2;
			else
				tris++;
		}

		return tris;
		}*/
};

class SceneExporter: COLLADASW::LibraryVisualScenes
{
public:
	SceneExporter(COLLADASW::StreamWriter *sw) : COLLADASW::LibraryVisualScenes(sw) {}
	
	void exportScene(Scene *sce) {
 		// <library_visual_scenes> <visual_scene>
		openVisualScene(id_name(sce), "");

		// write <node>s
		forEachMeshObjectInScene(sce, *this);
		forEachCameraObjectInScene(sce, *this);
		forEachLampObjectInScene(sce, *this);
		
		// </visual_scene> </library_visual_scenes>
		closeVisualScene();

		closeLibrary();
	}

	// called for each object
	void operator()(Object *ob) {
		COLLADASW::Node node(mSW);
		std::string ob_name(id_name(ob));
		node.start();

		node.addTranslate(ob->loc[0], ob->loc[1], ob->loc[2]);
		
		// when animation time comes, replace a single <rotate> with 3, one for each axis
		float quat[4];
		float axis[3];
		float angle;
		double angle_deg;
		EulToQuat(ob->rot, quat);
		NormalQuat(quat);
		QuatToAxisAngle(quat, axis, &angle);
		angle_deg = angle * 180.0f / M_PI;
		node.addRotate(axis[0], axis[1], axis[2], angle_deg);
		node.addScale(ob->size[0], ob->size[1], ob->size[2]);
		
		// <instance_geometry>
		if (ob->type == OB_MESH) {
			COLLADASW::InstanceGeometry instGeom(mSW);
			instGeom.setUrl(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, ob_name));
			
			for(int a = 0; a < ob->totcol; a++)	{
				Material *ma = give_current_material(ob, a+1);
				
				COLLADASW::BindMaterial& bm = instGeom.getBindMaterial();
				COLLADASW::InstanceMaterialList& iml = bm.getInstanceMaterialList();
				std::string matid(id_name(ma));
				COLLADASW::InstanceMaterial im(matid, COLLADASW::URI
											   (COLLADABU::Utils::EMPTY_STRING,
												matid));
				
				// create <bind_vertex_input> for each uv layer
				Mesh *me = (Mesh*)ob->data;
				int totlayer = CustomData_number_of_layers(&me->fdata, CD_MTFACE);
				
				for (int b = 0; b < totlayer; b++) {
					char *name = CustomData_get_layer_name(&me->fdata, CD_MTFACE, b);
					im.push_back(COLLADASW::BindVertexInput(name, "TEXCOORD", b));
				}
				
				iml.push_back(im);
			}
			
			instGeom.add();
		}
		
		// <instance_camera>
		else if (ob->type == OB_CAMERA) {
			COLLADASW::InstanceCamera instCam(mSW, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, ob_name));
			instCam.add();
		}
		
		// <instance_light>
		else if (ob->type == OB_LAMP) {
			COLLADASW::InstanceLight instLa(mSW, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, ob_name));
			instLa.add();
		}
		
		node.end();
	}

};

class ImagesExporter: COLLADASW::LibraryImages
{
	std::vector<std::string> mImages; // contains list of written images, to avoid duplicates
public:
	ImagesExporter(COLLADASW::StreamWriter *sw) : COLLADASW::LibraryImages(sw)
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

				if (find(mImages.begin(), mImages.end(), name) == mImages.end()) {
					COLLADASW::Image img(COLLADABU::URI(image->name), name, "");
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

		openEffect(id_name(ma) + "-effect");
		
		COLLADASW::EffectProfile ep(mSW);
		ep.setProfileType(COLLADASW::EffectProfile::COMMON);
		ep.openProfile();
		// set shader type - one of three blinn, phong or lambert
		if (ma->spec_shader == MA_SPEC_BLINN) {
			ep.setShaderType(COLLADASW::EffectProfile::BLINN);
		}
		else if (ma->spec_shader == MA_SPEC_PHONG) {
			ep.setShaderType(COLLADASW::EffectProfile::PHONG);
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
		// transparency
		ep.setTransparency(ma->alpha);
		// shininess
		ep.setShininess(ma->spec);
		// emission
		COLLADASW::ColorOrTexture cot = getcol(0.0f, 0.0f, 0.0f, 1.0f);
		ep.setEmission(cot);
		// diffuse 
		cot = getcol(ma->r, ma->g, ma->b, 1.0f);
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
		else {
			cot = getcol(0.0f, 0.0f, 0.0f, 1.0f);
			ep.setReflective(cot);
			ep.setReflectivity(0.0f);
		}
		// specular, shininess
		if (ep.getShaderType() != COLLADASW::EffectProfile::LAMBERT) {
			cot = getcol(ma->specr, ma->specg, ma->specb, 1.0f);
			ep.setSpecular(cot);
		}

		// XXX make this more readable if possible

		// create <sampler> and <surface> for each image
		COLLADASW::Sampler samplers[MAX_MTEX];
		COLLADASW::Surface surfaces[MAX_MTEX];
		void *samp_surf[MAX_MTEX][2];

		// image to index to samp_surf map
		// samp_surf[index] stores 2 pointers, sampler and surface
		std::map<std::string, int> im_samp_map;

		unsigned int a, b;
		for (a = 0, b = 0; a < tex_indices.size(); a++) {
			MTex *t = ma->mtex[tex_indices[a]];
			Image *ima = t->tex->ima;

			std::string key(id_name(ima));

			// create only one <sampler>/<surface> pair for each unique image
			if (im_samp_map.find(key) == im_samp_map.end()) {
				//<newparam> <surface> <init_from>
				COLLADASW::Surface surface(COLLADASW::Surface::SURFACE_TYPE_2D,
										   key + COLLADASW::Surface::SURFACE_SID_SUFFIX);
				COLLADASW::SurfaceInitOption sio(COLLADASW::SurfaceInitOption::INIT_FROM);
				sio.setImageReference(key);
				surface.setInitOption(sio);

				//<newparam> <sampler> <source>
				COLLADASW::Sampler sampler(COLLADASW::Sampler::SAMPLER_TYPE_2D,
										   key + COLLADASW::Surface::SURFACE_SID_SUFFIX);

				// copy values to arrays since they will live longer
				samplers[a] = sampler;
				surfaces[a] = surface;

				// store pointers so they can be used later when we create <texture>s
				samp_surf[b][0] = &samplers[a];
				samp_surf[b][1] = &surfaces[a];
				
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

			// we assume map input is always TEXTCO_UV

			std::string key(id_name(ima));
			int i = im_samp_map[key];
			COLLADASW::Sampler *sampler = (COLLADASW::Sampler*)samp_surf[i][0];
			COLLADASW::Surface *surface = (COLLADASW::Surface*)samp_surf[i][1];

			std::string uvname = strlen(t->uvname) ? t->uvname : active_uv;

			// color
			if (t->mapto & MAP_COL) {
				ep.setDiffuse(createTexture(ima, uvname, sampler, surface));
			}
			// ambient
			if (t->mapto & MAP_AMB) {
				ep.setAmbient(createTexture(ima, uvname, sampler, surface));
			}
			// specular
			if (t->mapto & MAP_SPEC) {
				ep.setSpecular(createTexture(ima, uvname, sampler, surface));
			}
			// emission
			if (t->mapto & MAP_EMIT) {
				ep.setEmission(createTexture(ima, uvname, sampler, surface));
			}
			// reflective
			if (t->mapto & MAP_REF) {
				ep.setReflective(createTexture(ima, uvname, sampler, surface));
			}
		}
		// performs the actual writing
		ep.addProfileElements();
		ep.closeProfile();
		closeEffect();	
	}
	
	COLLADASW::ColorOrTexture createTexture(Image *ima,
											std::string& uv_layer_name,
											COLLADASW::Sampler *sampler,
											COLLADASW::Surface *surface)
	{
		
		COLLADASW::Texture texture(id_name(ima));
		texture.setTexcoord(uv_layer_name);
		texture.setSurface(*surface);
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
			if (ma->mtex[a] && ma->mtex[a]->tex->type == TEX_IMAGE){
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

		openMaterial(name);

		std::string efid = name + "-effect";
		addInstanceEffect(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, efid));

		closeMaterial();
	}
};

class CamerasExporter: COLLADASW::LibraryCameras
{
public:
	CamerasExporter(COLLADASW::StreamWriter *sw): COLLADASW::LibraryCameras(sw){}
	void exportCameras(Scene *sce)
	{
		openLibrary();
		
		forEachCameraObjectInScene(sce, *this);
		
		closeLibrary();
	}
	void operator()(Object *ob)
	{
		// XXX add other params later
		Camera *cam = (Camera*)ob->data;
		std::string cam_name(id_name(ob));
		if (cam->type == CAM_PERSP) {
			COLLADASW::PerspectiveOptic persp(mSW);
			persp.setXFov(1.0);
			//persp.setYFov(1.0);
			persp.setAspectRatio(1.0);
			persp.setZFar(cam->clipend);
			persp.setZNear(cam->clipsta);
			COLLADASW::Camera ccam(mSW, &persp, cam_name);
			addCamera(ccam);
		}
		else {
			COLLADASW::OrthographicOptic ortho(mSW);
			ortho.setXMag(1.0);
			//ortho.setYMag(1.0, true);
			ortho.setAspectRatio(1.0);
			ortho.setZFar(cam->clipend);
			ortho.setZNear(cam->clipsta);
			COLLADASW::Camera ccam(mSW, &ortho, cam_name);
			addCamera(ccam);
		}
	}	
};

class LightsExporter: COLLADASW::LibraryLights
{
public:
	LightsExporter(COLLADASW::StreamWriter *sw): COLLADASW::LibraryLights(sw){}
	void exportLights(Scene *sce)
	{
		openLibrary();
		
		forEachLampObjectInScene(sce, *this);
		
		closeLibrary();
	}
	void operator()(Object *ob)
	{
		Lamp *la = (Lamp*)ob->data;
		std::string la_name(id_name(ob));
		COLLADASW::Color col(la->r, la->g, la->b);
		
		// sun
		if (la->type == LA_SUN) {
			COLLADASW::DirectionalLight cla(mSW, la_name, la->energy);
			cla.setColor(col);
			addLight(cla);
		}
		// hemi
		else if (la->type == LA_HEMI) {
			COLLADASW::AmbientLight cla(mSW, la_name, la->energy);
			cla.setColor(col);
			addLight(cla);
		}
		// spot
		// XXX add other params later
		else if (la->type == LA_SPOT) {
			COLLADASW::SpotLight cla(mSW, la_name, la->energy);
			cla.setColor(col);
			addLight(cla);
		}
		// lamp
		else if (la->type != LA_AREA) {
			COLLADASW::PointLight cla(mSW, la_name, la->energy);
			cla.setColor(col);
			addLight(cla);
		}
		else {
			// XXX write error
			return;
		}
	}
};

void DocumentExporter::exportCurrentScene(Scene *sce, const char* filename)
{
	COLLADABU::NativeString native_filename =
		COLLADABU::NativeString(std::string(filename));
	COLLADASW::StreamWriter sw(native_filename);

	// open <Collada>
	sw.startDocument();

	// <asset>
	COLLADASW::Asset asset(&sw);
	// XXX ask blender devs about this?
	asset.setUnit("meter", 1.0);
	asset.setUpAxisType(COLLADASW::Asset::Z_UP);
	asset.add();
	
	// <library_cameras>
	CamerasExporter ce(&sw);
	ce.exportCameras(sce);
	
	// <library_lights>
	LightsExporter le(&sw);
	le.exportLights(sce);
	
	// <library_images>
	ImagesExporter ie(&sw);
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
	
	// <library_visual_scenes>
	SceneExporter se(&sw);
	se.exportScene(sce);
	
	// <scene>
	std::string scene_name(id_name(sce));
	COLLADASW::Scene scene(&sw, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING,
											   scene_name));
	scene.add();
	
	// close <Collada>
	sw.endDocument();

}

void DocumentExporter::exportScenes(const char* filename)
{
}
