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
#include "COLLADAFWTransformation.h"
#include "COLLADAFWTranslate.h"
#include "COLLADAFWScale.h"
#include "COLLADAFWRotate.h"

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
#include "BKE_library.h"
}

#include "BLI_arithb.h"

#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "DocumentImporter.h"

#include <string>
#include <map>

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

	std::map<COLLADAFW::UniqueId, Mesh*> uid_mesh_map; // geometry unique id-to-mesh map

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
		// This method is guaranteed to be called _after_ writeGeometry, writeMaterial, etc.

		// for each <node> in <visual_scene>:
		// create an Object
		// if Mesh (previously created in writeGeometry) to which <node> corresponds exists, link Object with that mesh

		// update: since we cannot link a Mesh with Object in
		// writeGeometry because <geometry> does not reference <node>,
		// we link Objects with Meshes here
		
		// XXX it's better to take Id than name

		// TODO: create a new scene except the selected <visual_scene> - use current blender
		// scene for it
		Scene *sce = CTX_data_scene(mContext);
// 		Scene *sce = add_scene(visualScene->getName());
		int i = 0;

		for (i = 0; i < visualScene->getRootNodes().getCount(); i++) {
			COLLADAFW::Node *node = visualScene->getRootNodes()[i];

			// TODO: check node type
			if (node->getType() != COLLADAFW::Node::NODE) {
				continue;
			}

			Object *ob = add_object(sce, OB_MESH);

			const std::string& id = node->getOriginalId();
			if (id.length())
				rename_id(&ob->id, (char*)id.c_str());


			// XXX
			// linking object with the first <instance_geometry>
			// though a node may have more of them...

			// TODO: join multiple <instance_geometry> meshes into 1, and link object with it

			COLLADAFW::InstanceGeometryPointerArray &geom = node->getInstanceGeometries();
			if (geom.getCount() < 1) {
				fprintf(stderr, "Node hasn't got any geometry.\n");
				continue;
			}

			const COLLADAFW::UniqueId& uid = geom[0]->getInstanciatedObjectId();
			if (uid_mesh_map.find(uid) == uid_mesh_map.end()) {
				// XXX report to user
				// this could happen if a mesh was not created
				// (e.g. if it contains unsupported geometry)
				fprintf(stderr, "Couldn't find a mesh by UID.\n");
				continue;
			}

			set_mesh(ob, uid_mesh_map[uid]);

			float rot[3][3];
			Mat3One(rot);
			
			// transform Object
			for (int k = 0; k < node->getTransformations().getCount(); k ++) {
				COLLADAFW::Transformation *transform = node->getTransformations()[k];
				COLLADAFW::Transformation::TransformationType type = transform->getTransformationType();
				switch(type) {
				case COLLADAFW::Transformation::TRANSLATE:
					{
						COLLADAFW::Translate *tra = (COLLADAFW::Translate*)transform;
						COLLADABU::Math::Vector3& t = tra->getTranslation();
						// X
						ob->loc[0] = (float)t[0];
						// Y
						ob->loc[1] = (float)t[1];
						// Z
						ob->loc[2] = (float)t[2];
					}
					break;
				case COLLADAFW::Transformation::ROTATE:
					{
						COLLADAFW::Rotate *ro = (COLLADAFW::Rotate*)transform;
						COLLADABU::Math::Vector3& raxis = ro->getRotationAxis();
						float angle = (float)(ro->getRotationAngle() * M_PI / 180.0f);
						float axis[] = {raxis[0], raxis[1], raxis[2]};
						float quat[4];
						float rot_copy[3][3];
						float mat[3][3];
						AxisAngleToQuat(quat, axis, angle);

						QuatToMat3(quat, mat);
						Mat3CpyMat3(rot_copy, rot);
						Mat3MulMat3(rot, rot_copy, mat);
					}
					break;
				case COLLADAFW::Transformation::SCALE:
					{
						COLLADABU::Math::Vector3& s = ((COLLADAFW::Scale*)transform)->getScale();
						// X
						ob->size[0] = (float)s[0];
						// Y
						ob->size[1] = (float)s[1];
						// Z
						ob->size[2] = (float)s[2];
					}
					break;
				case COLLADAFW::Transformation::MATRIX:
					break;
				case COLLADAFW::Transformation::LOOKAT:
					break;
				case COLLADAFW::Transformation::SKEW:
					break;
				}
			}

			Mat3ToEul(rot, ob->rot);

		}

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
		// - write geometry

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
			
			COLLADAFW::MeshPrimitive *mp = prim_arr[i];
			COLLADAFW::MeshPrimitive::PrimitiveType type = mp->getPrimitiveType();

			const char *type_str = primTypeToStr(type);
			
			// OpenCollada passes POLYGONS type for <polylist>
			if (type == COLLADAFW::MeshPrimitive::POLYLIST || type == COLLADAFW::MeshPrimitive::POLYGONS) {

				COLLADAFW::Polygons *mpvc = (COLLADAFW::Polygons*)mp;
				COLLADAFW::Polygons::VertexCountArray& vca = mpvc->getGroupedVerticesVertexCountArray();
				
				bool ok = true;
				for(int j = 0; j < vca.getCount(); j++){
					int count = vca[j];
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
		//size_t totnorm = cmesh->getNormals().getFloatValues()->getCount() / 3;
		
		/*if (cmesh->hasNormals() && totnorm != totvert) {
			fprintf(stderr, "Per-face normals are not supported.\n");
			return true;
			}*/
		
		const std::string& str_geom_id = cgeom->getOriginalId();
		Mesh *me = add_mesh((char*)str_geom_id.c_str());

		// store mesh ptr
		// to link it later with Object
		this->uid_mesh_map[cgeom->getUniqueId()] = me;
		
		// vertices	
		me->mvert = (MVert*)CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, NULL, totvert);
		me->totvert = totvert;
		
		float *pos_float_array = cmesh->getPositions().getFloatValues()->getData();
		//float *normals_float_array = NULL;

		/*if (cmesh->hasNormals())
			normals_float_array = cmesh->getNormals().getFloatValues()->getData();
		*/
		MVert *mvert = me->mvert;
		i = 0;
		while (i < totvert) {
			// fill mvert
			mvert->co[0] = pos_float_array[0];
			mvert->co[1] = pos_float_array[1];
			mvert->co[2] = pos_float_array[2];

			/*if (normals_float_array) {
				mvert->no[0] = (short)(32767.0 * normals_float_array[0]);
				mvert->no[1] = (short)(32767.0 * normals_float_array[1]);
				mvert->no[2] = (short)(32767.0 * normals_float_array[2]);
				normals_float_array += 3;
				}*/
			
			pos_float_array += 3;
			mvert++;
			i++;
		}

		// count totface
		int totface = cmesh->getFacesCount();

		// allocate faces
		me->mface = (MFace*)CustomData_add_layer(&me->fdata, CD_MFACE, CD_CALLOC, NULL, totface);
		me->totface = totface;

		// read faces
		MFace *mface = me->mface;
		for (i = 0; i < prim_arr.getCount(); i++){
			
 			COLLADAFW::MeshPrimitive *mp = prim_arr[i];
			
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
			else if (type == COLLADAFW::MeshPrimitive::POLYLIST || type == COLLADAFW::MeshPrimitive::POLYGONS) {
				COLLADAFW::Polygons *mpvc =	(COLLADAFW::Polygons*)mp;
				COLLADAFW::Polygons::VertexCountArray& vca =
					mpvc->getGroupedVerticesVertexCountArray();
				for (k = 0; k < prim_totface; k++) {
					
					if (vca[k] == 3){
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
		
		mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);
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
