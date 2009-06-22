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


// not good idea - there are for example blender Scene and COLLADASW::Scene
//using namespace COLLADASW;


class GeometryExporter : COLLADASW::LibraryGeometries
{
public:
	GeometryExporter(COLLADASW::StreamWriter *sw) : COLLADASW::LibraryGeometries(sw) {}
	
	
	void exportGeom(Scene *sce)
	{
		//opens <library_geometries>
		openLibrary();
		
		// iterate over objects in scene
		Base *base= (Base*) sce->base.first;
		while(base) {
			
			Object *ob = base->object;
			
			// only meshes
			if (ob->type == OB_MESH && ob->data) {
				
				DerivedMesh *dm = mesh_get_derived_final(sce, ob, CD_MASK_BAREMESH);
				MVert *mverts = dm->getVertArray(dm);
				MFace *mfaces = dm->getFaceArray(dm);
				int totfaces = dm->getNumFaces(dm);
				int totverts = dm->getNumVerts(dm);
				bool checkTexcoords = false;

				std::string geom_name(ob->id.name);

				//openMesh(geoId, geoName, meshId)
				openMesh(geom_name, "", "");

				//writes <source> for vertex coords
				createVertsSource(sce, mSW, geom_name, dm);
				//writes <source> for normal coords
				createNormalsSource(sce, mSW, geom_name, dm);
				//writes <source> for uv coords
				//if mesh has uv coords
				checkTexcoords = createTexcoordsSource(sce, mSW, geom_name, dm, (Mesh*)ob->data);

				//<vertices>
				COLLADASW::Vertices verts(mSW);
				verts.setId(getIdBySemantics(geom_name, COLLADASW::VERTEX));
				COLLADASW::InputList &input_list = verts.getInputList();
				COLLADASW::Input input(COLLADASW::POSITION,
									   getUrlBySemantics(geom_name, COLLADASW::POSITION));
				input_list.push_back(input);
				verts.add();
				
				//<triangles>
				COLLADASW::Triangles tris(mSW);
				//sets count attribute in <triangles>
				tris.setCount(getTriCount(mfaces, totfaces));
				
				COLLADASW::InputList &til = tris.getInputList();
				/*added semantic, source, offset attributes to <input> */
				
			   //creates list of attributes in <triangles> <input> for vertices 
				COLLADASW::Input input2(COLLADASW::VERTEX,
										getUrlBySemantics(geom_name, COLLADASW::VERTEX), 0);
				//creates list of attributes in <triangles> <input> for normals
				COLLADASW::Input input3(COLLADASW::NORMAL,
										getUrlBySemantics(geom_name, COLLADASW::NORMAL), 0);
				
				til.push_back(input2);
				til.push_back(input3);
				
				//if mesh has uv coords writes <input> attributes for TEXCOORD
				if (checkTexcoords == true)
					{
						COLLADASW::Input input4(COLLADASW::TEXCOORD,
												getUrlBySemantics(geom_name, COLLADASW::TEXCOORD), 1, 1);
						til.push_back(input4);
						//XXX
						tris.setMaterial("material-symbol");
					}
				//performs the actual writing
				tris.prepareToAppendValues();
				
				int i;
				int texindex = 0;
				//writes data to <p>
				for (i = 0; i < totfaces; i++) {
					MFace *f = &mfaces[i];
					//if mesh has uv coords writes uv and
					//vertex indexes
					if (checkTexcoords == true)	{
						// if triangle
						if (f->v4 == 0) {
							tris.appendValues(f->v1);
							tris.appendValues(texindex++);
							tris.appendValues(f->v2);
							tris.appendValues(texindex++);
							tris.appendValues(f->v3);
							tris.appendValues(texindex++);
						}
						// quad
						else {
							tris.appendValues(f->v1);
							tris.appendValues(texindex++);
							tris.appendValues(f->v2);
							tris.appendValues(texindex++);
							tris.appendValues(f->v3);
							tris.appendValues(texindex++);
							tris.appendValues(f->v3);
							tris.appendValues(texindex++);
							tris.appendValues(f->v4);
							tris.appendValues(texindex++);
							tris.appendValues(f->v1);
							tris.appendValues(texindex++);
						}
					}
					//if mesh has no uv coords writes only 
					//vertex indexes
					else {
						// if triangle
						if (f->v4 == 0) {
							tris.appendValues(f->v1, f->v2, f->v3);	
						}
						// quad
						else {
							tris.appendValues(f->v1, f->v2, f->v3);
							tris.appendValues(f->v3, f->v4, f->v1);
						}
						
					} 
				}

				tris.closeElement();
				tris.finish();
					
				closeMesh();
				closeGeometry();
					
				dm->release(dm);
						
				   
			}
			base= base->next;
		}

		closeLibrary();
	}

	//creates <source> for positions
	void createVertsSource(Scene *sce, COLLADASW::StreamWriter *sw,
					  std::string geom_name, DerivedMesh *dm)
	{
		int totverts = dm->getNumVerts(dm);
		MVert *verts = dm->getVertArray(dm);
		
		
		COLLADASW::FloatSourceF source(sw);
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
	bool createTexcoordsSource(Scene *sce, COLLADASW::StreamWriter *sw,
							   std::string geom_name, DerivedMesh *dm, Mesh *me)
	{
		
		int totfaces = dm->getNumFaces(dm);
		MTFace *tface = me->mtface;
		MFace *mfaces = dm->getFaceArray(dm);
		if(tface != NULL)
			{
				
				COLLADASW::FloatSourceF source(sw);
				source.setId(getIdBySemantics(geom_name, COLLADASW::TEXCOORD));
				source.setArrayId(getIdBySemantics(geom_name, COLLADASW::TEXCOORD) +
								  ARRAY_ID_SUFFIX);
				source.setAccessorCount(getTriCount(mfaces, totfaces) * 3);
				source.setAccessorStride(2);
				COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
				param.push_back("X");
				param.push_back("Y");
				
				source.prepareToAppendValues();
				
				int i;
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
						source.appendValues(tface[i].uv[2][0]);
						source.appendValues(tface[i].uv[2][1]);
						//uv4
						source.appendValues(tface[i].uv[3][0]);
						source.appendValues(tface[i].uv[3][1]);
						//uv1
						source.appendValues(tface[i].uv[0][0]);
						source.appendValues(tface[i].uv[0][1]);
						
					}
				}
				
				source.finish();
				return true;
			}
		return false;
	}


	//creates <source> for normals
	void createNormalsSource(Scene *sce, COLLADASW::StreamWriter *sw,
							 std::string geom_name, DerivedMesh *dm)
	{
		int totverts = dm->getNumVerts(dm);
		MVert *verts = dm->getVertArray(dm);
		
		COLLADASW::FloatSourceF source(sw);
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
	

	int getTriCount(MFace *faces, int totface) {
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
	}
};

class SceneExporter: COLLADASW::LibraryVisualScenes
{
public:
	SceneExporter(COLLADASW::StreamWriter *sw) : COLLADASW::LibraryVisualScenes(sw) {}
	
	void exportScene(Scene *sce) {
 		//<library_visual_scenes><visual_scene>
		openVisualScene(sce->id.name, "");
		
		//<node> for each mesh object
		Base *base= (Base*) sce->base.first;
		while(base) {
			Object *ob = base->object;
			
			if (ob->type == OB_MESH && ob->data) {
				
				COLLADASW::Node node(mSW);
				node.start();

				node.addTranslate(ob->loc[0], ob->loc[1], ob->loc[2]);
				// node.addRotate(); // XXX no conversion needed?
				node.addScale(ob->size[0], ob->size[1], ob->size[2]);
				
				COLLADASW::InstanceGeometry instGeom(mSW);
				std::string ob_name(ob->id.name);
				instGeom.setUrl(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING,
											   ob_name));
				
				for(int a = 0; a < ob->totcol; a++)
					{
						Material *ma = give_current_material(ob, a+1);
						
						COLLADASW::BindMaterial& bm = instGeom.getBindMaterial();
						COLLADASW::InstanceMaterialList& iml = bm.getInstanceMaterialList();
						std::string matid = std::string(ma->id.name);	
						/*COLLADASW::InstanceMaterial im("material-symbol", COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, matid));
						COLLADASW::BindVertexInput bvi("myUVs", "TEXCOORD", 1);
						im.push_back(bvi);*/
						iml.push_back(im);
					}
				instGeom.add();
				node.end();
			}
			base= base->next;
		}
		
		//</visual_scene></library_visual_scenes>
		closeVisualScene();

		closeLibrary();
	}
};

//class for exporting images
class ImagesExporter: COLLADASW::LibraryImages
{
public:
	ImagesExporter(COLLADASW::StreamWriter *sw) : COLLADASW::LibraryImages(sw)
	{}
	
	void exportImages(Scene *sce)
	{
		openLibrary();
		
		Image *image = (Image*)G.main->image.first;
		while(image) {
			
			//fileURI, imageId, imageName
			COLLADASW::Image img(COLLADABU::URI(image->name), image->id.name, "");
			img.add(mSW);
			
			image = (Image*)image->id.next;
		}
		closeLibrary();
	}
	
};

class EffectsExporter: COLLADASW::LibraryEffects
{
public:
	EffectsExporter(COLLADASW::StreamWriter *sw) : COLLADASW::LibraryEffects(sw){}
	void exportEffects(Scene *sce)
	{
		
		openLibrary();
		Material *ma = (Material*)G.main->mat.first;
		while(ma) {
			
			openEffect(std::string(ma->id.name) + "-effect");

			COLLADASW::EffectProfile ep(mSW);
			
			ep.setProfileType(COLLADASW::EffectProfile::COMMON);

			//open <profile_common>
			ep.openProfile();
			
			std::vector<int> mtexindices = countmtex(ma);
			
			for (int a = 0; a < mtexindices.size(); a++){
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
				texture.setTexcoord("myUVs");
				texture.setSurface(surface);
				texture.setSampler(sampler);

				//<texture>
				COLLADASW::ColorOrTexture cot(texture);
				ep.setDiffuse(cot, true, "");
				ep.setShaderType(COLLADASW::EffectProfile::LAMBERT);

				//todo add or find generator of technique sids
				//performs the actual writing
				ep.addProfileElements();
				ep.closeTechnique();
				//COLLADASW::Technique technique(mSW);
				//technique.closeTechnique();
				
			}
			
			ep.closeProfile();
			ma = (Material*) ma->id.next;
		}
		
		closeLibrary();

	}

	//returns the array of mtex indices which have image 
	//I need this for exporting textures
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
		
		Material *ma = (Material*)G.main->mat.first;
		
		while(ma) {
			
			openMaterial(std::string(ma->id.name));
			std::string efid = std::string(ma->id.name) + "-effect";
			addInstanceEffect(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, efid));
			closeMaterial();
			
			ma = (Material*) ma->id.next;
		}
		closeLibrary();	
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
