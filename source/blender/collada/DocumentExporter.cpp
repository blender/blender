#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
extern "C" 
{
#include "BKE_DerivedMesh.h"
}
#include "BKE_scene.h"

#include "DocumentExporter.h"

#include <COLLADASWAsset.h>
#include <COLLADASWLibraryVisualScenes.h>
#include <COLLADASWNode.h>
#include <COLLADASWLibraryGeometries.h>
#include <COLLADASWSource.h>
#include <COLLADASWInstanceGeometry.h>
#include <COLLADASWInputList.h>
#include <COLLADASWScene.h>
#include <COLLADASWPrimitves.h>
#include <COLLADASWVertices.h>

// not good idea - there are for example blender Scene and COLLADASW::Scene
//using namespace COLLADASW;


class GeometryExporter : COLLADASW::LibraryGeometries
{
public:
	GeometryExporter(COLLADASW::StreamWriter *sw) : COLLADASW::LibraryGeometries(sw) {}

	void exportGeom(Scene *sce)
	{
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

				std::string geom_name(ob->id.name);

				//openMesh(geoId, geoName, meshId)
				openMesh(geom_name, "", "");

				//<source>
				createSource(sce, mSW, geom_name, dm);

				//<vertices>	
				COLLADASW::Vertices verts(mSW);
				verts.setId(getIdBySemantics(geom_name, COLLADASW::VERTEX));
				COLLADASW::InputList &input_list = verts.getInputList();
				COLLADASW::Input input(COLLADASW::POSITION,
									   getUrlBySemantics(geom_name, COLLADASW::POSITION));
				input_list.push_back(input);
				verts.add();
				
				//triangles
				COLLADASW::Triangles tris(mSW);
				tris.setCount(getTriCount(mfaces, totfaces));
				//tris.setMaterial();
				COLLADASW::InputList &til = tris.getInputList();
				/*added semantic, source, offset attributes to <input>
				 I am not sure whether it's right or not
				*/

				COLLADASW::Input input2(COLLADASW::VERTEX,
										getUrlBySemantics(geom_name, COLLADASW::VERTEX), 0);
				til.push_back(input2);
							
				tris.prepareToAppendValues();

				int i;
				for (i = 0; i < totfaces; i++) {
					MFace *f = &mfaces[i];

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

				tris.closeElement();
				tris.finish();
					
				closeMesh();
				closeGeometry();
					
				dm->release(dm);
						
				   
			}
			base= base->next;
		}

		/*	//openMesh(geoId, geoName, meshId)
		void openMesh("", "", "");
		
		//<source>
		void sourceCreator(&sce);

		//<vertices>	
		Vertices verts(&sw);
		verts.setId();
		Input input(POSITION, source);
		InputList inputL(&sw);
		inputL.push_back(input);
		verts.add();

		//triangles
		PrimitivesBase pBase(&sw, CSWC::CSW_ELEMENT_TRIANGLES);
		Primitive<CSWC::CSW_ELEMENT_TRIANGLES> prim(&sw);
		prim.setCount();
		prim.setMaterial();
		InputList &til = pBase.getInputList();
		til.push_back(input);

		prim.prepareToAppendValues();
		prim.appendValues();
		prim.closeElement();
		prim.finish();
		
		closeMesh();
		closeGeometry();
		*/
	}

	void createSource(Scene *sce, COLLADASW::StreamWriter *sw,
					  std::string geom_name, DerivedMesh *dm)
	{
		int totverts = dm->getNumVerts(dm);
		MVert *verts = dm->getVertArray(dm);
		
		//Source<float, "float_array", "float"> source(sw);
		COLLADASW::FloatSourceF source(sw);
		source.setId(getIdBySemantics(geom_name, COLLADASW::POSITION));
		source.setArrayId(geom_name + ARRAY_ID_SUFFIX);
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

/*

  <library_visual_scenes>
   <visual_scene>
    <node>
	 <translate>
	 <rotate>
	 <instance_geometry>
	</node>
    ...
   </visual_scene>
  </library_visual_scenes>

  <library_geometries>
   <geometry id="">
    <mesh>
	 <source id="source_id">
	  <float_array id="" count="">
	   0.0 0.0 0.0
	  </float_array>
	 </source>

	 <vertices>
	  <input source="#source_id" ...>
	 </vertices>

	 <triangles>
	  <input>
	  <p>
	 </triangles>

	</mesh>
   </geometry>
  </library_geometries>

 */

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
	
	SceneExporter se(&sw);
	se.exportScene(sce);
	
	//<library_geometries>
	GeometryExporter ge(&sw);
	ge.exportGeom(sce);

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
