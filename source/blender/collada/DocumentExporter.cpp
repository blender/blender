#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"

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

			if (find(mMat.begin(), mMat.end(), std::string(ma->id.name)) == mMat.end()) {
				(*this->f)(ma);

				mMat.push_back(ma->id.name);
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
		std::string geom_name(ob->id.name);

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
			polylist.setMaterial(ma->id.name);
				
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
									0);
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
		openVisualScene(sce->id.name, "");

		// write <node>s
		forEachMeshObjectInScene(sce, *this);
		
		// </visual_scene> </library_visual_scenes>
		closeVisualScene();

		closeLibrary();
	}

	// called for each object
	void operator()(Object *ob) {
		COLLADASW::Node node(mSW);
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

		COLLADASW::InstanceGeometry instGeom(mSW);
		std::string ob_name(ob->id.name);
		instGeom.setUrl(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, ob_name));
				
		for(int a = 0; a < ob->totcol; a++)	{
			Material *ma = give_current_material(ob, a+1);
						
			COLLADASW::BindMaterial& bm = instGeom.getBindMaterial();
			COLLADASW::InstanceMaterialList& iml = bm.getInstanceMaterialList();
			std::string matid = std::string(ma->id.name);
			COLLADASW::InstanceMaterial im(matid, COLLADASW::URI
										   (COLLADABU::Utils::EMPTY_STRING,
											matid));
			//iterate over all textures
			//if any add to list
			/*int c = 0;
			for (int b = 0; b < MAX_MTEX; b++) {
				MTex *mtex = ma->mtex[b];
				if (mtex && mtex->tex && mtex->tex->ima) {
					char texcoord[30];
					sprintf(texcoord, "%d", c);
					COLLADASW::BindVertexInput bvi(std::string("myUVs") + texcoord, "TEXCOORD", 0);
					c++;
					im.push_back(bvi);
				}
			}
			*/
		    iml.push_back(im);
		}

		instGeom.add();

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

	void operator()(Material *ma)
	{
		int a;
		for (a = 0; a < MAX_MTEX; a++) {
			MTex *mtex = ma->mtex[a];
			if (mtex && mtex->tex && mtex->tex->ima) {

				Image *image = mtex->tex->ima;
				std::string name(image->id.name);

				if (find(mImages.begin(), mImages.end(), name) == mImages.end()) {
					COLLADASW::Image img(COLLADABU::URI(image->name), image->id.name, "");
					img.add(mSW);
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

	void operator()(Material *ma)
	{
		openEffect(std::string(ma->id.name) + "-effect");
		
		COLLADASW::EffectProfile ep(mSW);
		
		ep.setProfileType(COLLADASW::EffectProfile::COMMON);
		
		std::vector<int> mtexindices = countmtex(ma);
		
		//for (int a = 0; a < mtexindices.size(); a++) {
			
			//open <profile_common>
		ep.openProfile();
			/*
			//need this for making each texcoord unique
			char texcoord[30];
			sprintf(texcoord, "%d", a);
			
			//<newparam> <surface> <init_from>
			Image *ima = ma->mtex[mtexindices[a]]->tex->ima;
			COLLADASW::Surface surface(COLLADASW::Surface::SURFACE_TYPE_2D,
									   ima->id.name + COLLADASW::Surface::SURFACE_SID_SUFFIX);
			COLLADASW::SurfaceInitOption sio(COLLADASW::SurfaceInitOption::INIT_FROM);
			sio.setImageReference(ima->id.name);
			surface.setInitOption(sio);
			
			//<newparam> <sampler> <source>
			COLLADASW::Sampler sampler(COLLADASW::Sampler::SAMPLER_TYPE_2D,
									   ima->id.name + COLLADASW::Surface::SURFACE_SID_SUFFIX);
			
			//<lambert> <diffuse> <texture>	
			COLLADASW::Texture texture(ima->id.name);
			texture.setTexcoord(std::string("myUVs") + texcoord);
			texture.setSurface(surface);
			texture.setSampler(sampler);
			
			//<texture>
			COLLADASW::ColorOrTexture cot(texture);
			ep.setDiffuse(cot);
			*/
		if (ma->spec_shader == MA_SPEC_BLINN) {
			ep.setShaderType(COLLADASW::EffectProfile::BLINN);
		}
		else if (ma->spec_shader == MA_SPEC_PHONG) {
			ep.setShaderType(COLLADASW::EffectProfile::PHONG);
		}
		else {
			// XXX write error 
			ep.setShaderType(COLLADASW::EffectProfile::LAMBERT);
		}
			
		// emission 
		COLLADASW::ColorOrTexture cot_col = getcol(0.0f, 0.0f, 0.0f, 1.0f);
		ep.setEmission(cot_col);
		
		// diffuse
		cot_col = getcol(ma->r, ma->g, ma->b, 1.0f);
		ep.setDiffuse(cot_col);
		
		// ambient
		cot_col = getcol(ma->ambr, ma->ambg, ma->ambb, 1.0f);
		ep.setAmbient(cot_col);
			
		// reflective, reflectivity
		if (ma->mode & MA_RAYMIRROR) {
			cot_col = getcol(ma->mirr, ma->mirg, ma->mirb, 1.0f);
			ep.setReflective(cot_col);
			ep.setReflectivity(ma->ray_mirror);
		}
		else {
			cot_col = getcol(0.0f, 0.0f, 0.0f, 1.0f);
			ep.setReflective(cot_col);
			ep.setReflectivity(0.0f);
		}
		
		// transparent, transparency
		if (ep.getShaderType() != COLLADASW::EffectProfile::BLINN) {
			cot_col = getcol(0.0f, 0.0f, 0.0f, 1.0f);
			ep.setTransparent(cot_col);
		}
		ep.setTransparency(ma->alpha);
		
		// index of refraction
		if (ma->mode & MA_RAYTRANSP) {
			ep.setIndexOfRefraction(ma->ang);
		}
		else {
			ep.setIndexOfRefraction(1.0f);
		}
		
		// specular, shininess, diffuse
		if (ep.getShaderType() != COLLADASW::EffectProfile::LAMBERT) {
			ep.setShininess(ma->spec);
			cot_col = getcol(ma->specr, ma->specg, ma->specb, 1.0f);
			ep.setSpecular(cot_col);
		}
		
		// performs the actual writing
		ep.addProfileElements();
		ep.closeProfile();
		
		//}
		
		closeEffect();
	}
	
	COLLADASW::ColorOrTexture getcol(float r, float g, float b, float a)
	{
		COLLADASW::Color color(r,g,b,a);
		COLLADASW::ColorOrTexture cot_col(color);
		return cot_col;
	}
	
	//returns the array of mtex indices which have image 
	//need this for exporting textures
	std::vector<int> countmtex(Material *ma)
	{
		std::vector<int> mtexindices;
		for (int a = 0; a < 18; a++){
			if (!ma->mtex[a]){
				continue;
			}
			Tex *tex = ma->mtex[a]->tex;
			if(!tex){
				continue;
			}
			Image *ima = tex->ima;
			if(!ima){
				continue;
			}
			mtexindices.push_back(a);
		}
		return mtexindices;
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

	void operator()(Material *ma)
	{
		std::string name(ma->id.name);

		openMaterial(name);

		std::string efid = name + "-effect";
		addInstanceEffect(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, efid));

		closeMaterial();
	}
};


void DocumentExporter::exportCurrentScene(Scene *sce, const char* filename)
{
	COLLADABU::NativeString native_filename =
		COLLADABU::NativeString(std::string(filename));
	COLLADASW::StreamWriter sw(native_filename);

	//open <Collada>
	sw.startDocument();

	//<asset>
	COLLADASW::Asset asset(&sw);
	asset.setUpAxisType(COLLADASW::Asset::Z_UP);
	asset.add();
	
	//<library_images>
	ImagesExporter ie(&sw);
	ie.exportImages(sce);
	
	//<library_effects>
	EffectsExporter ee(&sw);
	ee.exportEffects(sce);
	
	//<library_materials>
	MaterialsExporter me(&sw);
	me.exportMaterials(sce);

	//<library_geometries>
	GeometryExporter ge(&sw);
	ge.exportGeom(sce);
	
	//<library_visual_scenes>
	SceneExporter se(&sw);
	se.exportScene(sce);
	
	//<scene>
	std::string scene_name(sce->id.name);
	COLLADASW::Scene scene(&sw, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING,
											   scene_name));
	scene.add();
	
	//close <Collada>
	sw.endDocument();

}

void DocumentExporter::exportScenes(const char* filename)
{
}
