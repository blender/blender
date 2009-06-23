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

// utilities to avoid code duplication
// definition of these is difficult to read, but they should be useful

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
		MVert *mverts = dm->getVertArray(dm);
		MFace *mfaces = dm->getFaceArray(dm);
		int totfaces = dm->getNumFaces(dm);
		int totverts = dm->getNumVerts(dm);
		bool checkTexcoords = false;

		std::string geom_name(ob->id.name);

		//openMesh(geoId, geoName, meshId)
		openMesh(geom_name, "", "");

		//writes <source> for vertex coords
		createVertsSource(geom_name, dm);
		//writes <source> for normal coords
		createNormalsSource(geom_name, dm);
		//writes <source> for uv coords
		//if mesh has uv coords
		checkTexcoords = createTexcoordsSource(geom_name, dm, (Mesh*)ob->data);

		//<vertices>
		COLLADASW::Vertices verts(mSW);
		verts.setId(getIdBySemantics(geom_name, COLLADASW::VERTEX));
		COLLADASW::InputList &input_list = verts.getInputList();
		COLLADASW::Input input(COLLADASW::POSITION,
							   getUrlBySemantics(geom_name, COLLADASW::POSITION));
		input_list.push_back(input);
		verts.add();

		//polylist
		COLLADASW::Polylist polylist(mSW);
		
		//sets count attribute in <polylist>
		polylist.setCount(totfaces);
				
		COLLADASW::InputList &til = polylist.getInputList();
				
		//creates list of attributes in <polylist> <input> for vertices 
		COLLADASW::Input input2(COLLADASW::VERTEX, getUrlBySemantics
								(geom_name, COLLADASW::VERTEX), 0);
		//creates list of attributes in <polylist> <input> for normals
		COLLADASW::Input input3(COLLADASW::NORMAL, getUrlBySemantics
								(geom_name, COLLADASW::NORMAL), 0);
				
		til.push_back(input2);
		til.push_back(input3);
				
		//if mesh has uv coords writes <input> attributes for TEXCOORD
		if (checkTexcoords == true)
			{
				COLLADASW::Input input4(COLLADASW::TEXCOORD,
										getUrlBySemantics(geom_name, COLLADASW::TEXCOORD), 1, 0);
				til.push_back(input4);
				polylist.setMaterial("material-symbol");
			}
		
		
		//<vcount>
		int i;
		std::vector<unsigned long> VCountList;
		for (i = 0; i < totfaces; i++) {
			MFace *f = &mfaces[i];
			
			if (f->v4 == 0) {
				VCountList.push_back(3);
			}
			else {
				VCountList.push_back(4);
			}
		}
		polylist.setVCountList(VCountList);
		
		//performs the actual writing
		polylist.prepareToAppendValues();
		
		int texindex = 0;
		//<p>
		for (i = 0; i < totfaces; i++) {
			MFace *f = &mfaces[i];
			//if mesh has uv coords writes uv and
			//vertex indexes
			if (checkTexcoords == true)	{
				// if triangle
				if (f->v4 == 0) {
					polylist.appendValues(f->v1);
					polylist.appendValues(texindex++);
					polylist.appendValues(f->v2);
					polylist.appendValues(texindex++);
					polylist.appendValues(f->v3);
					polylist.appendValues(texindex++);
				}
				// quad
				else {
					polylist.appendValues(f->v1);
					polylist.appendValues(texindex++);
					polylist.appendValues(f->v2);
					polylist.appendValues(texindex++);
					polylist.appendValues(f->v3);
					polylist.appendValues(texindex++);
					//tris.appendValues(f->v3);
					//tris.appendValues(texindex++);
					polylist.appendValues(f->v4);
					polylist.appendValues(texindex++);
					//tris.appendValues(f->v1);
					//tris.appendValues(texindex++);
				}
			}
			//if mesh has no uv coords writes only 
			//vertex indexes
			else {
				// if triangle
				if (f->v4 == 0) {
					polylist.appendValues(f->v1, f->v2, f->v3);	
				}
				// quad
				else {
					polylist.appendValues(f->v1, f->v2, f->v3, f->v4);
					//tris.appendValues(f->v3, f->v4, f->v1);
				}
						
			} 
		}

		polylist.closeElement();
		polylist.finish();
					
		closeMesh();
		closeGeometry();
					
		dm->release(dm);
						
	}
	
	//creates <source> for positions
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
		/*closes <float_array>, adds
		  <technique_common>
		  <accessor source = "" count = "" stride = "" >,
		  </source> */
		source.finish();
	
	}

	//creates <source> for texcoords
	// returns true if mesh has uv data
	bool createTexcoordsSource(std::string geom_name, DerivedMesh *dm, Mesh *me)
	{
		
		int totfaces = dm->getNumFaces(dm);
		MTFace *tface = me->mtface;
		MFace *mfaces = dm->getFaceArray(dm);
		if(tface != NULL) {
				
			COLLADASW::FloatSourceF source(mSW);
			source.setId(getIdBySemantics(geom_name, COLLADASW::TEXCOORD));
			source.setArrayId(getIdBySemantics(geom_name, COLLADASW::TEXCOORD) +
							  ARRAY_ID_SUFFIX);
			//source.setAccessorCount(getTriCount(mfaces, totfaces) * 3);
			int i = 0;
			int j = 0;
			for (int i = 0; i < totfaces; i++) {
				MFace *f = &mfaces[i];
				
				if (f->v4 == 0) {
					j+=3;
				}
				else {
					j+=4;
				}
			}
			source.setAccessorCount(j);
			source.setAccessorStride(2);
			COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
			param.push_back("X");
			param.push_back("Y");
				
			source.prepareToAppendValues();
			
			for (i = 0; i < totfaces; i++) {
				MFace *f = &mfaces[i];
					
				// if triangle
				if (f->v4 == 0) {
						
					// get uv1's X coordinate
					source.appendValues(tface[i].uv[0][0]);
					// get uv1's Y coordinate
					source.appendValues(tface[i].uv[0][1]);
					// get uv2's X coordinate
					source.appendValues(tface[i].uv[1][0]);
					// etc...
					source.appendValues(tface[i].uv[1][1]);
					//uv3
					source.appendValues(tface[i].uv[2][0]);
					source.appendValues(tface[i].uv[2][1]);
						
						
				}
				// quad
				else {
						
					// get uv1's X coordinate
					source.appendValues(tface[i].uv[0][0]);
					// get uv1's Y coordinate
					source.appendValues(tface[i].uv[0][1]);
					//uv2
					source.appendValues(tface[i].uv[1][0]);
					source.appendValues(tface[i].uv[1][1]);
					//uv3
					source.appendValues(tface[i].uv[2][0]);
					source.appendValues(tface[i].uv[2][1]);
					//uv3
					//source.appendValues(tface[i].uv[2][0]);
					//source.appendValues(tface[i].uv[2][1]);
					//uv4
					source.appendValues(tface[i].uv[3][0]);
					source.appendValues(tface[i].uv[3][1]);
					//uv1
					//source.appendValues(tface[i].uv[0][0]);
					//source.appendValues(tface[i].uv[0][1]);
						
				}
			}
				
			source.finish();
			return true;
		}
		return false;
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
	
	std::string getIdBySemantics(std::string geom_name, COLLADASW::Semantics type) {
		return geom_name +
			getSuffixBySemantic(type);
	}
	

	COLLADASW::URI getUrlBySemantics(std::string geom_name, COLLADASW::Semantics type) {
		std::string id(getIdBySemantics(geom_name, type));
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
		// XXX for rotation we need to convert ob->rot (euler, I guess) to axis/angle
		// see http://www.euclideanspace.com/maths/geometry/rotations/conversions/eulerToAngle/index.htm
		// add it to BLI_arithb.h
		// node.addRotate();
		node.addScale(ob->size[0], ob->size[1], ob->size[2]);
				
		COLLADASW::InstanceGeometry instGeom(mSW);
		std::string ob_name(ob->id.name);
		instGeom.setUrl(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, ob_name));
				
		for(int a = 0; a < ob->totcol; a++)	{
			Material *ma = give_current_material(ob, a+1);
						
			COLLADASW::BindMaterial& bm = instGeom.getBindMaterial();
			COLLADASW::InstanceMaterialList& iml = bm.getInstanceMaterialList();
			std::string matid = std::string(ma->id.name);
			COLLADASW::InstanceMaterial im("material-symbol", COLLADASW::URI
										   (COLLADABU::Utils::EMPTY_STRING,
											matid));
			//iterate over all textures
			//if any add to list
			int c = 0;
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
		
		for (int a = 0; a < mtexindices.size(); a++){
			
			//open <profile_common>
			ep.openProfile();
			
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
			ep.setDiffuse(cot, true, "");
			ep.setShaderType(COLLADASW::EffectProfile::LAMBERT);

			//performs the actual writing
			ep.addProfileElements();
			ep.closeProfile();
				
		}
			
		closeEffect();
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
