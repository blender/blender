#include "COLLADAFWStableHeaders.h"
#include "COLLADAFWIWriter.h"
#include "COLLADAFWRoot.h"
#include "COLLADAFWNode.h"
#include "COLLADAFWVisualScene.h"
#include "COLLADAFWInstanceGeometry.h"
#include "COLLADAFWFileInfo.h"
#include "COLLADAFWRoot.h"
#include "COLLADAFWLight.h"
#include "COLLADAFWImage.h"
#include "COLLADAFWMaterial.h"
#include "COLLADAFWGeometry.h"
#include "COLLADAFWMesh.h"
#include "COLLADAFWFloatOrDoubleArray.h"
#include "COLLADAFWArrayPrimitiveType.h"
#include "COLLADAFWMeshPrimitiveWithFaceVertexCount.h"
#include "COLLADAFWPolygons.h"

#include "COLLADASaxFWLLoader.h"

// TODO move "extern C" into header files
extern "C" 
{
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_customdata.h"
#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_image.h"
#include "BKE_material.h"
}

#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "DocumentImporter.h"


const char *primTypeToStr(COLLADAFW::MeshPrimitive::PrimitiveType type)
{
	using namespace COLLADAFW;
	
	switch (type) {
	case MeshPrimitive::LINES:
		return "LINES";
	case MeshPrimitive::LINE_STRIPS:
		return "LINESTRIPS";
	case MeshPrimitive::POLYGONS:
		return "POLYGONS";
	case MeshPrimitive::POLYLIST:
		return "POLYLIST";
	case MeshPrimitive::TRIANGLES:
		return "TRIANGLES";
	case MeshPrimitive::TRIANGLE_FANS:
		return "TRIANGLE_FANS";
	case MeshPrimitive::TRIANGLE_STRIPS:
		return "TRIANGLE_FANS";
	case MeshPrimitive::POINTS:
		return "POINTS";
	case MeshPrimitive::UNDEFINED_PRIMITIVE_TYPE:
		return "UNDEFINED_PRIMITIVE_TYPE";
	}
	return "UNKNOWN";
}

const char *geomTypeToStr(COLLADAFW::Geometry::GeometryType type)
{
	switch (type) {
	case COLLADAFW::Geometry::GEO_TYPE_MESH:
		return "MESH";
	case COLLADAFW::Geometry::GEO_TYPE_SPLINE:
		return "SPLINE";
	case COLLADAFW::Geometry::GEO_TYPE_CONVEX_MESH:
		return "CONVEX_MESH";
	}
	return "UNKNOWN";
}

/** Class that needs to be implemented by a writer. 
	IMPORTANT: The write functions are called in arbitrary order.*/
class Writer: public COLLADAFW::IWriter
{
private:
	std::string mFilename;
	
	std::vector<COLLADAFW::VisualScene> mVisualScenes;

	bContext *mContext;

	class UnitConverter
	{
	private:
		COLLADAFW::FileInfo::Unit mUnit;
		COLLADAFW::FileInfo::UpAxisType mUpAxis;
	public:
		UnitConverter(COLLADAFW::FileInfo::UpAxisType upAxis, COLLADAFW::FileInfo::Unit& unit) :
			mUpAxis(upAxis), mUnit(unit)
		{
		}

		// TODO
		// convert vector vec from COLLADA format to Blender
		void convertVec3(float *vec)
		{
		}
		
		// TODO need also for angle conversion, time conversion...
	};

public:

	/** Constructor. */
	Writer(bContext *C, const char *filename) : mContext(C), mFilename(filename) {};

	/** Destructor. */
	~Writer() {};

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
		continue to to load. The writer should undo all operations that have been performed.
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
		// using mVisualScenes, do:
		// - write <node> data to Objects: materials, transforms, etc.

		// TODO: import materials (<instance_material> inside <instance_geometry>) and textures

		std::vector<COLLADAFW::VisualScene>::iterator it = mVisualScenes.begin();
		for (; it != mVisualScenes.end(); it++) {
			COLLADAFW::VisualScene &visscene = *it;

			// create new blender scene

			// create Objects from <node>s inside this <visual_scene>

			// link each Object with a Mesh
			// for each Object's <instance_geometry> there should already exist a Mesh
		}
	}

	/** When this method is called, the writer must write the global document asset.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeGlobalAsset ( const COLLADAFW::FileInfo* asset ) 
	{
		// XXX take up_axis, unit into account
		// COLLADAFW::FileInfo::Unit unit = asset->getUnit();
		// COLLADAFW::FileInfo::UpAxisType upAxis = asset->getUpAxisType();

		return true;
	}

	/** When this method is called, the writer must write the scene.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeScene ( const COLLADAFW::Scene* scene ) 
	{
		// XXX could store the scene id, but do nothing for now
		return true;
	}

	/** When this method is called, the writer must write the entire visual scene.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeVisualScene ( const COLLADAFW::VisualScene* visualScene ) 
	{
		// for each <node> in <visual_scene>:
		// create an Object
		// if Mesh (previously created in writeGeometry) to which <node> corresponds exists, link Object with that mesh

		// update: since we cannot link a Mesh with Object in
		// writeGeometry because <geometry> does not reference <node>,
		// we link Objects with Meshes at the end with method "finish".
		mVisualScenes.push_back(*visualScene);

		return true;
	}

	/** When this method is called, the writer must handle all nodes contained in the 
		library nodes.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeLibraryNodes ( const COLLADAFW::LibraryNodes* libraryNodes ) 
	{
		return true;
	}

	/** When this method is called, the writer must write the geometry.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeGeometry ( const COLLADAFW::Geometry* cgeom ) 
	{
		// - create a mesh object
		// - enter editmode getting editmesh
		// - write geometry
		// - exit editmode
		// 
		// - unlink mesh from object
		// - remove object

		// - ignore usupported primitive types

		// TODO: import also uvs, normals
		// XXX what to do with normal indices?
		// XXX num_normals may be != num verts, then what to do?

		// check geometry->getType() first
		if (cgeom->getType() != COLLADAFW::Geometry::GEO_TYPE_MESH) {
			// TODO: report warning
			fprintf(stderr, "Mesh type %s is not supported\n", geomTypeToStr(cgeom->getType()));
			return true;
		}

		COLLADAFW::Mesh *cmesh = (COLLADAFW::Mesh*)cgeom;

		// first check if we can import this mesh
		COLLADAFW::MeshPrimitiveArray& prim_arr = cmesh->getMeshPrimitives();

		int i;
		
		for (i = 0; i < prim_arr.getCount(); i++) {
			
			COLLADAFW::MeshPrimitive *mp = prim_arr.getData()[i];
			COLLADAFW::MeshPrimitive::PrimitiveType type = mp->getPrimitiveType();

			const char *type_str = primTypeToStr(type);

			if (type == COLLADAFW::MeshPrimitive::POLYLIST) {

				COLLADAFW::Polygons *mpvc = (COLLADAFW::Polygons*)mp;
				COLLADAFW::Polygons::VertexCountArray& vca = mpvc->getGroupedVerticesVertexCountArray();
				
				bool ok = true;
				for(int j = 0; j < vca.getCount(); j++){
					int count = vca.getData()[j];
					if (count != 3 && count != 4) {
						fprintf(stderr, "%s has at least one face with vertex count > 4 or < 3\n",
								type_str);
						return true;
					}
				}
					
			}
			else if(type != COLLADAFW::MeshPrimitive::TRIANGLES) {
				fprintf(stderr, "Primitive type %s is not supported.\n", type_str);
				return true;
			}
		}
		
		size_t totvert = cmesh->getPositions().getFloatValues()->getCount() / 3;
		size_t totnorm = cmesh->getNormals().getFloatValues()->getCount() / 3;
		
		if (cmesh->hasNormals() && totnorm != totvert) {
			fprintf(stderr, "Per-face normals are not supported.\n");
			return true;
		}
		
		Mesh *me = add_mesh((char*)cgeom->getOriginalId().c_str());
		
		// vertices	
		me->mvert = (MVert*)CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, NULL, totvert);
		me->totvert = totvert;
		
		float *pos_float_array = cmesh->getPositions().getFloatValues()->getData();
		float *normals_float_array = NULL;

		if (cmesh->hasNormals())
			normals_float_array = cmesh->getNormals().getFloatValues()->getData();
		
		MVert *mvert = me->mvert;
		i = 0;
		while (i < totvert) {
			// fill mvert
			mvert->co[0] = pos_float_array[0];
			mvert->co[1] = pos_float_array[1];
			mvert->co[2] = pos_float_array[2];

			if (normals_float_array) {
				mvert->no[0] = (short)(32767.0 * normals_float_array[0]);
				mvert->no[1] = (short)(32767.0 * normals_float_array[1]);
				mvert->no[2] = (short)(32767.0 * normals_float_array[2]);
				normals_float_array += 3;
			}
			
			pos_float_array += 3;
			mvert++;
			i++;
		}

		// count totface
		int totface = 0;

		for (i = 0; i < prim_arr.getCount(); i++) {
			COLLADAFW::MeshPrimitive *mp = prim_arr.getData()[i];
			totface += mp->getFaceCount();
		}

		// allocate faces
		me->mface = (MFace*)CustomData_add_layer(&me->fdata, CD_MFACE, CD_CALLOC, NULL, totface);
		me->totface = totface;

		// read faces
		MFace *mface = me->mface;
		for (i = 0; i < prim_arr.getCount(); i++){
			
 			COLLADAFW::MeshPrimitive *mp = prim_arr.getData()[i];
			
			// faces
			size_t prim_totface = mp->getFaceCount();
			unsigned int *indices = mp->getPositionIndices().getData();
			int k;
			int type = mp->getPrimitiveType();
			
			if (type == COLLADAFW::MeshPrimitive::TRIANGLES) {
				for (k = 0; k < prim_totface; k++){
					mface->v1 = indices[0];
					mface->v2 = indices[1];
					mface->v3 = indices[2];
					indices += 3;
					mface++;
				}
			}
			else if (type == COLLADAFW::MeshPrimitive::POLYLIST) {
				COLLADAFW::Polygons *mpvc =	(COLLADAFW::Polygons*)mp;
				COLLADAFW::Polygons::VertexCountArray& vca =
					mpvc->getGroupedVerticesVertexCountArray();
				for (k = 0; k < prim_totface; k++) {
					
					if (vca.getData()[k] == 3){
						mface->v1 = indices[0];
						mface->v2 = indices[1];
						mface->v3 = indices[2];
						indices += 3;
						
					}
					else {
						mface->v1 = indices[0];
						mface->v2 = indices[1];
						mface->v3 = indices[2];
						mface->v4 = indices[3];
						indices +=4;
						
					}
					mface++;
				}
			}
		}
		
		Object *ob = add_object(CTX_data_scene(mContext), OB_MESH);
		set_mesh(ob, me);

		// XXX: don't use editors module


		// create a mesh object
// 		Object *ob = ED_object_add_type(mContext, OB_MESH);

// 		// enter editmode
// 		ED_object_enter_editmode(mContext, 0);
// 		Mesh *me = (Mesh*)ob->data;
// 		EditMesh *em = BKE_mesh_get_editmesh(me);

// 		// write geometry
// 		// currently only support <triangles>

// 		// read vertices
// 		std::vector<EditVert*> vertptr;
// 		COLLADAFW::MeshVertexData& vertdata = cmesh->getPositions();
// 		float *pos = vertdata.getFloatValues()->getData();
// 		size_t totpos = vertdata.getValuesCount() / 3;
// 		int i;

// 		for (i = 0; i < totpos; i++){
// 			float v[3] = {pos[i * 3 + 0],
// 						  pos[i * 3 + 1],
// 						  pos[i * 3 + 2]};
// 			EditVert *eve = addvertlist(em, v, 0);
// 			vertptr.push_back(eve);
// 		}

// 		COLLADAFW::MeshPrimitiveArray& apt = cmesh->getMeshPrimitives();

// 		// read primitives
// 		// TODO: support all primitive types
// 		for (i = 0; i < apt.getCount(); i++){
			
// 			COLLADAFW::MeshPrimitive *mp = apt.getData()[i];
// 			if (mp->getPrimitiveType() != COLLADAFW::MeshPrimitive::TRIANGLES){
// 				continue;
// 			}
			
// 			const size_t tottris = mp->getFaceCount();
// 			COLLADAFW::UIntValuesArray& indicesArray = mp->getPositionIndices();
// 			unsigned int *indices = indicesArray.getData();
// 			for (int j = 0; j < tottris; j++){
// 				addfacelist(em,
// 							vertptr[indices[j * 3 + 0]],
// 							vertptr[indices[j * 3 + 1]],
// 							vertptr[indices[j * 3 + 2]], 0, 0, 0);
// 			}
// 		}
		
// 		BKE_mesh_end_editmesh(me, em);

// 		// exit editmode
// 		ED_object_exit_editmode(mContext, EM_FREEDATA);

		return true;
	}

	/** When this method is called, the writer must write the material.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeMaterial( const COLLADAFW::Material* material ) 
	{
		// TODO: create and store a material.
		// Let it have 0 users for now.
		/*std::string name = material->getOriginalId();
		  add_material(name);*/
		return true;
	}

	/** When this method is called, the writer must write the effect.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeEffect( const COLLADAFW::Effect* effect ) 
	{
		return true;
	}

	/** When this method is called, the writer must write the camera.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeCamera( const COLLADAFW::Camera* camera ) 
	{
		return true;
	}

	/** When this method is called, the writer must write the image.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeImage( const COLLADAFW::Image* image ) 
	{
		/*std::string name = image->getOriginalId();
		  BKE_add_image_file(name);*/
		return true;
	}

	/** When this method is called, the writer must write the light.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeLight( const COLLADAFW::Light* light ) 
	{
		return true;
	}

	/** When this method is called, the writer must write the Animation.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeAnimation( const COLLADAFW::Animation* animation ) 
	{
		return true;
	}

	/** When this method is called, the writer must write the AnimationList.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeAnimationList( const COLLADAFW::AnimationList* animationList ) 
	{
		return true;
	}

	/** When this method is called, the writer must write the skin controller data.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeSkinControllerData( const COLLADAFW::SkinControllerData* skinControllerData ) 
	{
		return true;
	}

	/** When this method is called, the writer must write the controller.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeController( const COLLADAFW::Controller* Controller ) 
	{
		return true;
	}
};

void DocumentImporter::import(bContext *C, const char *filename)
{
	Writer w(C, filename);
	w.write();
}
