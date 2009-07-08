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
#include "COLLADAFWMeshVertexData.h"
#include "COLLADAFWFloatOrDoubleArray.h"
#include "COLLADAFWArrayPrimitiveType.h"
#include "COLLADAFWMeshPrimitiveWithFaceVertexCount.h"
#include "COLLADAFWPolygons.h"
#include "COLLADAFWTransformation.h"
#include "COLLADAFWTranslate.h"
#include "COLLADAFWScale.h"
#include "COLLADAFWRotate.h"
#include "COLLADAFWEffect.h"
#include "COLLADAFWIndexList.h"

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
#include "DNA_material_types.h"

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
	std::map<COLLADAFW::UniqueId, Material*> uid_material_map;
	std::map<COLLADAFW::UniqueId, Material*> uid_effect_map;

	// this structure is used to assign material indices to faces
	// when materials are assigned to an object
	struct Primitive {
		MFace *mface;
		int totface;
	};
	typedef std::map<COLLADAFW::MaterialId, std::vector<Primitive> > MaterialIdPrimitiveArrayMap;
	// amazing name!
	std::map<COLLADAFW::UniqueId, MaterialIdPrimitiveArrayMap> geom_uid_mat_mapping_map;

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

	class UVDataWrapper {
		COLLADAFW::MeshVertexData *mVData;
	public:
		UVDataWrapper(COLLADAFW::MeshVertexData& vdata) : mVData(&vdata)
		{}

		void getUV(int uv_set_index, int uv_index, float *uv)
		{
			//int uv_coords_index = mVData->getInputInfosArray()[uv_set_index]->getCount() * uv_set_index + uv_index * 2;
			int uv_coords_index = uv_index * 2;
// 			int uv_coords_index = mVData->getLength(uv_set_index) * uv_set_index + uv_index * 2;
			switch(mVData->getType()) {
			case COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT:
				{
					COLLADAFW::ArrayPrimitiveType<float>* values = mVData->getFloatValues();					
					uv[0] = (*values)[uv_coords_index];
					uv[1] = (*values)[uv_coords_index + 1];
					
					break;
				}
			case COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE:
				{
					COLLADAFW::ArrayPrimitiveType<double>* values = mVData->getDoubleValues();
					
					uv[0] = (float)(*values)[uv_coords_index];
					uv[1] = (float)(*values)[uv_coords_index + 1];
					
					break;
				}
			}
			//uv[0] = mVData;
			//uv[1] = ...;
		}
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
			
			const COLLADAFW::UniqueId& geom_uid = geom[0]->getInstanciatedObjectId();
			if (uid_mesh_map.find(geom_uid) == uid_mesh_map.end()) {
				// XXX report to user
				// this could happen if a mesh was not created
				// (e.g. if it contains unsupported geometry)
				fprintf(stderr, "Couldn't find a mesh by UID.\n");
				continue;
			}
			
			set_mesh(ob, uid_mesh_map[geom_uid]);
			
			float rot[3][3];
			Mat3One(rot);
			int k;
			// transform Object
			for (k = 0; k < node->getTransformations().getCount(); k ++) {
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

			// assign materials to object
			// assign material indices to mesh faces
			for (k = 0; k < geom[0]->getMaterialBindings().getCount(); k++) {
				
				const COLLADAFW::UniqueId& mat_uid =
					geom[0]->getMaterialBindings()[k].getReferencedMaterial();

				if (uid_material_map.find(mat_uid) == uid_material_map.end()) {
					// This should not happen
					fprintf(stderr, "Cannot find material by UID.\n");
					continue;
				}

				assign_material(ob, uid_material_map[mat_uid], ob->totcol + 1);

				MaterialIdPrimitiveArrayMap& mat_prim_map = geom_uid_mat_mapping_map[geom_uid];
				COLLADAFW::MaterialId mat_id = geom[0]->getMaterialBindings()[k].getMaterialId();
				
				// if there's geometry that uses this material,
				// set mface->mat_nr=k for each face in that geometry
				if (mat_prim_map.find(mat_id) != mat_prim_map.end()) {

					std::vector<Primitive>& prims = mat_prim_map[mat_id];

					std::vector<Primitive>::iterator it;

					for (it = prims.begin(); it != prims.end(); it++) {
						Primitive& prim = *it;

						int l = 0;
						while (l++ < prim.totface) {
							prim.mface->mat_nr = k;
							prim.mface++;
						}
					}
				}
			}
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

	// utility functions

	void set_tri_or_quad_uv(MTFace *mtface, UVDataWrapper &uvs, int uv_set_index,
					COLLADAFW::IndexList& index_list, int index, bool quad)
	{
		int uv_indices[4] = {
			index_list.getIndex(index),
			index_list.getIndex(index + 1),
			index_list.getIndex(index + 2),
			0
		};

		if (quad) uv_indices[3] = index_list.getIndex(index + 3);

		uvs.getUV(uv_set_index, uv_indices[0], mtface->uv[0]);
		uvs.getUV(uv_set_index, uv_indices[1], mtface->uv[1]);
		uvs.getUV(uv_set_index, uv_indices[2], mtface->uv[2]);

		if (quad) uvs.getUV(uv_set_index, uv_indices[3], mtface->uv[3]);
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
		
		// UVs
		int totuvset = cmesh->getUVCoords().getInputInfosArray().getCount();

		for (i = 0; i < totuvset; i++) {
			// add new CustomData layer
			CustomData_add_layer(&me->fdata, CD_MTFACE, CD_CALLOC, NULL, totface);
		}

		if (totuvset) me->mtface = (MTFace*)CustomData_get_layer_n(&me->fdata, CD_MTFACE, 0);

		UVDataWrapper uvs(cmesh->getUVCoords());
		
		// read faces
		MFace *mface = me->mface;

		MaterialIdPrimitiveArrayMap mat_prim_map;

		// TODO: import uv set names

		for (i = 0; i < prim_arr.getCount(); i++) {
			
 			COLLADAFW::MeshPrimitive *mp = prim_arr[i];

			// faces
			size_t prim_totface = mp->getFaceCount();
			unsigned int *indices = mp->getPositionIndices().getData();
			int k;
			int type = mp->getPrimitiveType();
			int index = 0;
			
			// since we cannot set mface->mat_nr here, we store part of me->mface in Primitive
			Primitive prim = {mface, 0};
			COLLADAFW::IndexListArray& index_list_array = mp->getUVCoordIndicesArray();
			
			if (type == COLLADAFW::MeshPrimitive::TRIANGLES) {
				for (k = 0; k < prim_totface; k++){
					mface->v1 = indices[0];
					mface->v2 = indices[1];
					mface->v3 = indices[2];
					
					indices += 3;

					for (int j = 0; j < totuvset; j++) {
						// k - face index, j - uv set index

						// get mtface by face index (k) and uv set index
						MTFace *mtface = (MTFace*)CustomData_get_layer_n(&me->fdata, CD_MTFACE, j);
						set_tri_or_quad_uv(&mtface[k], uvs, j, *index_list_array[j], index, false);
					}
					index += 3;
					mface++;
					prim.totface++;
				}
			}
			else if (type == COLLADAFW::MeshPrimitive::POLYLIST || type == COLLADAFW::MeshPrimitive::POLYGONS) {
				COLLADAFW::Polygons *mpvc =	(COLLADAFW::Polygons*)mp;
				COLLADAFW::Polygons::VertexCountArray& vca =
					mpvc->getGroupedVerticesVertexCountArray();
				for (k = 0; k < prim_totface; k++) {

					// face
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

						// trick
						if (mface->v4 == 0) {
							mface->v4 = mface->v1;
							mface->v1 = mface->v2;
							mface->v2 = mface->v3;
							mface->v3 = 0;
						}

						indices +=4;
						
					}

					// set mtface for each uv set
					// it is assumed that all primitives have equal number of UV sets

					for (int j = 0; j < totuvset; j++) {
						// k - face index, j - uv set index

						// get mtface by face index (k) and uv set index
						MTFace *mtface = (MTFace*)CustomData_get_layer_n(&me->fdata, CD_MTFACE, j);

						set_tri_or_quad_uv(&mtface[k], uvs, j, *index_list_array[j], index, mface->v4 != 0);
					}
					index += mface->v4 ? 4 : 3;
					mface++;
					prim.totface++;
				}
			}
			// XXX primitive could have no materials
			// check if primitive has material
			mat_prim_map[mp->getMaterialId()].push_back(prim);
		}
		
		geom_uid_mat_mapping_map[cgeom->getUniqueId()] = mat_prim_map;
		
		// normals
		mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);
		return true;
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
		
		COLLADAFW::CommonEffectPointerArray ef_array = effect->getCommonEffects();
		if (ef_array.getCount() < 1) {
			fprintf(stderr, "Effect hasn't got any common effects.\n");
			return true;
		}
		// XXX TODO: Take all common effects
		// Currently only first <effect_common> is supported
		COLLADAFW::EffectCommon *ef = ef_array[0];
		COLLADAFW::EffectCommon::ShaderType shader = ef->getShaderType();
		
		// blinn
		if (shader == COLLADAFW::EffectCommon::SHADER_BLINN) {
			ma->spec_shader = MA_SPEC_BLINN;
			ma->spec = ef->getShininess().getFloatValue();
		}
		// phong
		else if (shader == COLLADAFW::EffectCommon::SHADER_PHONG) {
			ma->spec_shader = MA_SPEC_PHONG;
			ma->spec = ef->getShininess().getFloatValue();
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
		
		COLLADAFW::Color col;
		// diffuse
		if (ef->getDiffuse().isColor()) {
			col = ef->getDiffuse().getColor();
			ma->r = col.getRed();
			ma->g = col.getGreen();
			ma->b = col.getBlue();
		}
		// ambient
		if (ef->getAmbient().isColor()) {
			col = ef->getAmbient().getColor();
			ma->ambr = col.getRed();
			ma->ambg = col.getGreen();
			ma->ambb = col.getBlue();
		}
		// specular
		if (ef->getSpecular().isColor()) {
			col = ef->getSpecular().getColor();
			ma->specr = col.getRed();
			ma->specg = col.getGreen();
			ma->specb = col.getBlue();
		}
		// reflective
		if (ef->getReflective().isColor()) {
			col = ef->getReflective().getColor();
			ma->mirr = col.getRed();
			ma->mirg = col.getGreen();
			ma->mirb = col.getBlue();
		}
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
		const std::string& filepath = image->getImageURI().toNativePath();
		BKE_add_image_file((char*)filepath.c_str(), 0);
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
