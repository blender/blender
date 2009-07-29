#include "COLLADAFWRoot.h"
#include "COLLADAFWIWriter.h"
#include "COLLADAFWStableHeaders.h"
#include "COLLADAFWAnimationCurve.h"
#include "COLLADAFWAnimationList.h"
#include "COLLADAFWCamera.h"
#include "COLLADAFWColorOrTexture.h"
#include "COLLADAFWEffect.h"
#include "COLLADAFWFloatOrDoubleArray.h"
#include "COLLADAFWGeometry.h"
#include "COLLADAFWImage.h"
#include "COLLADAFWIndexList.h"
#include "COLLADAFWInstanceGeometry.h"
#include "COLLADAFWLight.h"
#include "COLLADAFWMaterial.h"
#include "COLLADAFWMesh.h"
#include "COLLADAFWMeshPrimitiveWithFaceVertexCount.h"
#include "COLLADAFWNode.h"
#include "COLLADAFWPolygons.h"
#include "COLLADAFWRotate.h"
#include "COLLADAFWSampler.h"
#include "COLLADAFWScale.h"
#include "COLLADAFWSkinController.h"
#include "COLLADAFWSkinControllerData.h"
#include "COLLADAFWTransformation.h"
#include "COLLADAFWTranslate.h"
#include "COLLADAFWTypes.h"
#include "COLLADAFWVisualScene.h"
#include "COLLADAFWFileInfo.h"
#include "COLLADAFWArrayPrimitiveType.h"

#include "COLLADASaxFWLLoader.h"

// TODO move "extern C" into header files
extern "C" 
{
#include "ED_keyframing.h"
#include "ED_armature.h"

#include "BKE_main.h"
#include "BKE_customdata.h"
#include "BKE_library.h"
#include "BKE_texture.h"
#include "BKE_fcurve.h"
#include "BKE_depsgraph.h"
#include "BLI_util.h"
}
#include "BKE_armature.h"
#include "BKE_mesh.h"
#include "BKE_global.h"
#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_image.h"
#include "BKE_material.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_lamp_types.h"
#include "DNA_armature_types.h"
#include "DNA_anim_types.h"
#include "DNA_curve_types.h"
#include "DNA_texture_types.h"
#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "DocumentImporter.h"

#include <string>
#include <map>


// #define COLLADA_DEBUG

char *CustomData_get_layer_name(const struct CustomData *data, int type, int n);

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

class UnitConverter
{
private:
	COLLADAFW::FileInfo::Unit unit;
	COLLADAFW::FileInfo::UpAxisType up_axis;

public:

	UnitConverter() : unit(), up_axis(COLLADAFW::FileInfo::Z_UP) {}

	void read_asset(const COLLADAFW::FileInfo* asset)
	{
	}

	// TODO
	// convert vector vec from COLLADA format to Blender
	void convertVec3(float *vec)
	{
	}
		
	// TODO need also for angle conversion, time conversion...

	void mat4_from_dae_mat4(float out[][4], const COLLADABU::Math::Matrix4& in) {
		// in DAE, matrices use columns vectors, (see comments in COLLADABUMathMatrix4.h)
		// so here, to make a blender matrix, we simply swap columns and rows
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				out[i][j] = in[j][i];
			}
		}
	}
};

typedef std::map<COLLADAFW::TextureMapId, std::vector<MTex*> > TexIndexTextureArrayMap;

class ArmatureImporter
{
private:
	Scene *scene;
	UnitConverter *unit_converter;

	// to build armature bones from inverse bind matrices
	struct JointData {
		float inv_bind_mat[4][4]; // joint inverse bind matrix
		Object *ob_arm;			  // armature object
	};
	std::map<int, JointData> joint_index_to_joint_info_map;
	std::map<COLLADAFW::UniqueId, int> joint_id_to_joint_index_map;

	/*
	struct ArmatureData {
		bArmature *arm;
		COLLADAFW::SkinController *controller;
	};
	std::map<COLLADAFW::UniqueId, ArmatureData> controller_id_to_arm_info_map;
	*/

	std::vector<COLLADAFW::Node*> root_joints;
	std::map<COLLADAFW::UniqueId, COLLADAFW::UniqueId> controller_id_to_geom_id_map;

	JointData *get_joint_data(COLLADAFW::Node *node)
	{
		const COLLADAFW::UniqueId& joint_id = node->getUniqueId();

		if (joint_id_to_joint_index_map.find(joint_id) == joint_id_to_joint_index_map.end()) {
			fprintf(stderr, "Cannot find a joint index by joint id for %s.\n",
					node->getOriginalId().c_str());
			return NULL;
		}

		int joint_index = joint_id_to_joint_index_map[joint_id];

		return &joint_index_to_joint_info_map[joint_index];
	}

	void create_bone(COLLADAFW::Node *node, EditBone *parent, bArmature *arm)
	{
		JointData* jd = get_joint_data(node);

		if (jd) {
			float mat[4][4];

			// get original world-space matrix
			Mat4Invert(mat, jd->inv_bind_mat);

			// TODO rename from Node "name" attrs later
			EditBone *bone = addEditBone(arm, "Bone");

			if (parent) bone->parent = parent;

			// set head
			VecCopyf(bone->head, mat[3]);

			// set tail, can't set it to head because 0-length bones are not allowed
			float vec[3] = {0.0f, 0.5f, 0.0f};
			VecAddf(bone->tail, bone->head, vec);

			// set parent tail
			if (parent)
				VecCopyf(parent->tail, bone->head);

			COLLADAFW::NodePointerArray& children = node->getChildNodes();
			for (int i = 0; i < children.getCount(); i++) {
				create_bone(children[i], bone, arm);
			}
		}
	}

	void create_bone_branch(COLLADAFW::Node *root)
	{
		JointData* jd = get_joint_data(root);
		if (!jd) return;

		Object *ob_arm = jd->ob_arm;
		
		// enter armature edit mode
		ED_armature_to_edit(ob_arm);

		COLLADAFW::NodePointerArray& children = root->getChildNodes();
		for (int i = 0; i < children.getCount(); i++) {
			create_bone(children[i], NULL, (bArmature*)ob_arm->data);
		}

		// exit armature edit mode
		ED_armature_from_edit(scene, ob_arm);
	}

public:

	ArmatureImporter(UnitConverter *conv, Scene *sce) : unit_converter(conv), scene(sce) {}

	void add_root_joint(COLLADAFW::Node *node)
	{
		root_joints.push_back(node);
	}

	bool has_root_joints()
	{
		return root_joints.size() > 0;
	}

	// here we add bones to armature, having armatures previously created in write_controller
	void build_armatures()
	{
		this->scene = scene;
		std::vector<COLLADAFW::Node*>::iterator it;
		for (it = root_joints.begin(); it != root_joints.end(); it++) {
			create_bone_branch(*it);
		}
	}

	bool write_skin_controller_data(const COLLADAFW::SkinControllerData* skin)
	{
		// use inverse bind matrices to construct armature
		// it is safe to invert them to get the original matrices
		// because if they are inverse matrices, they can be inverted

		// just do like so:
		// - create armature
		// - enter editmode
		// - add edit bones and head/tail properties using matrices and parent-child info
		// - exit edit mode

		// store join inv bind matrix to use it later in armature construction
		const COLLADAFW::Matrix4Array& inv_bind_mats = skin->getInverseBindMatrices();
		int i;
		for (i = 0; i < skin->getJointsCount(); i++) {
			JointData jd;
			unit_converter->mat4_from_dae_mat4(jd.inv_bind_mat, inv_bind_mats[i]);
			joint_index_to_joint_info_map[i] = jd;
		}

		return true;
	}

	bool write_controller(const COLLADAFW::Controller* controller)
	{
		// here we:
		// - create armature
		// - create EditBones, not setting parent-child relationships
		// - store armature

		const COLLADAFW::UniqueId& skin_id = controller->getUniqueId();

		if (controller->getControllerType() == COLLADAFW::Controller::CONTROLLER_TYPE_SKIN) {

			Object *ob_arm = add_object(this->scene, OB_ARMATURE);

			COLLADAFW::SkinController *skinco = (COLLADAFW::SkinController*)controller;
			const COLLADAFW::UniqueId& id = skinco->getSkinControllerData();

			// to find geom id by controller id
			this->controller_id_to_geom_id_map[skin_id] = skinco->getSource();
			
			// "Node" ids
			const COLLADAFW::UniqueIdArray& joint_ids = skinco->getJoints();

			int i;
			for (i = 0; i < joint_ids.getCount(); i++) {

				// store armature pointer
				JointData& jd = joint_index_to_joint_info_map[i];
				jd.ob_arm = ob_arm;

				// now we'll be able to get inv bind matrix from joint id
				joint_id_to_joint_index_map[joint_ids[i]] = i;
			}
		}
		// morph controller
		else {
		}

		return true;
	}

	COLLADAFW::UniqueId *get_geometry_uid(const COLLADAFW::UniqueId& controller_uid)
	{
		if (controller_id_to_geom_id_map.find(controller_uid) == controller_id_to_geom_id_map.end())
			return NULL;

		return &controller_id_to_geom_id_map[controller_uid];
	}
};

class MeshImporter
{
private:

	Scene *scene;
	ArmatureImporter *armature_importer;

	std::map<COLLADAFW::UniqueId, Mesh*> uid_mesh_map; // geometry unique id-to-mesh map
	// this structure is used to assign material indices to faces
	// it holds a portion of Mesh faces and corresponds to a DAE primitive list (<triangles>, <polylist>, etc.)
	struct Primitive {
		MFace *mface;
		unsigned int totface;
	};
	typedef std::map<COLLADAFW::MaterialId, std::vector<Primitive> > MaterialIdPrimitiveArrayMap;
	std::map<COLLADAFW::UniqueId, MaterialIdPrimitiveArrayMap> geom_uid_mat_mapping_map; // crazy name!
	
	class UVDataWrapper
	{
		COLLADAFW::MeshVertexData *mVData;
	public:
		UVDataWrapper(COLLADAFW::MeshVertexData& vdata) : mVData(&vdata)
		{}

#ifdef COLLADA_DEBUG
		void print()
		{
			fprintf(stderr, "UVs:\n");
			COLLADAFW::ArrayPrimitiveType<float>* values = mVData->getFloatValues();
			for (int i = 0; i < values->getCount(); i += 2) {
				fprintf(stderr, "%.1f, %.1f\n", (*values)[i], (*values)[i+1]);
			}
			fprintf(stderr, "\n");
		}
#endif

		void getUV(int uv_set_index, int uv_index[2], float *uv)
		{
			switch(mVData->getType()) {
			case COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT:
				{
					COLLADAFW::ArrayPrimitiveType<float>* values = mVData->getFloatValues();
					if (values->empty()) return;
					uv[0] = (*values)[uv_index[0]];
					uv[1] = (*values)[uv_index[1]];
					
					break;
				}
			case COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE:
				{
					COLLADAFW::ArrayPrimitiveType<double>* values = mVData->getDoubleValues();
					if (values->empty()) return;
					uv[0] = (float)(*values)[uv_index[0]];
					uv[1] = (float)(*values)[uv_index[1]];
					
					break;
				}
			}
		}
	};

	void set_face_indices(MFace *mface, unsigned int *indices, bool quad)
	{
		mface->v1 = indices[0];
		mface->v2 = indices[1];
		mface->v3 = indices[2];
		if (quad) mface->v4 = indices[3];
		else mface->v4 = 0;
#ifdef COLLADA_DEBUG
		fprintf(stderr, "%u, %u, %u \n", indices[0], indices[1], indices[2]);
#endif
	}

	// change face indices order so that v4 is not 0
	void rotate_face_indices(MFace *mface) {
		mface->v4 = mface->v1;
		mface->v1 = mface->v2;
		mface->v2 = mface->v3;
		mface->v3 = 0;
	}

	void set_face_uv(MTFace *mtface, UVDataWrapper &uvs, int uv_set_index,
					COLLADAFW::IndexList& index_list, int index, bool quad)
	{
		int uv_indices[4][2];

		// per face vertex indices, this means for quad we have 4 indices, not 8
		COLLADAFW::UIntValuesArray& indices = index_list.getIndices();

		// make indices into FloatOrDoubleArray
		for (int i = 0; i < (quad ? 4 : 3); i++) {
			int uv_index = indices[index + i];
			uv_indices[i][0] = uv_index * 2;
			uv_indices[i][1] = uv_index * 2 + 1;
		}

		uvs.getUV(uv_set_index, uv_indices[0], mtface->uv[0]);
		uvs.getUV(uv_set_index, uv_indices[1], mtface->uv[1]);
		uvs.getUV(uv_set_index, uv_indices[2], mtface->uv[2]);

		if (quad) uvs.getUV(uv_set_index, uv_indices[3], mtface->uv[3]);

#ifdef COLLADA_DEBUG
		if (quad) {
			fprintf(stderr, "face uv:\n"
					"((%d, %d), (%d, %d), (%d, %d), (%d, %d))\n"
					"((%.1f, %.1f), (%.1f, %.1f), (%.1f, %.1f), (%.1f, %.1f))\n",

					uv_indices[0][0], uv_indices[0][1],
					uv_indices[1][0], uv_indices[1][1],
					uv_indices[2][0], uv_indices[2][1],
					uv_indices[3][0], uv_indices[3][1],

					mtface->uv[0][0], mtface->uv[0][1],
					mtface->uv[1][0], mtface->uv[1][1],
					mtface->uv[2][0], mtface->uv[2][1],
					mtface->uv[3][0], mtface->uv[3][1]);
		}
		else {
			fprintf(stderr, "face uv:\n"
					"((%d, %d), (%d, %d), (%d, %d))\n"
					"((%.1f, %.1f), (%.1f, %.1f), (%.1f, %.1f))\n",

					uv_indices[0][0], uv_indices[0][1],
					uv_indices[1][0], uv_indices[1][1],
					uv_indices[2][0], uv_indices[2][1],

					mtface->uv[0][0], mtface->uv[0][1],
					mtface->uv[1][0], mtface->uv[1][1],
					mtface->uv[2][0], mtface->uv[2][1]);
		}
#endif
	}

#ifdef COLLADA_DEBUG
	void print_index_list(COLLADAFW::IndexList& index_list)
	{
		fprintf(stderr, "Index list for \"%s\":\n", index_list.getName().c_str());
		for (int i = 0; i < index_list.getIndicesCount(); i += 2) {
			fprintf(stderr, "%u, %u\n", index_list.getIndex(i), index_list.getIndex(i + 1));
		}
		fprintf(stderr, "\n");
	}
#endif

	bool is_nice_mesh(COLLADAFW::Mesh *mesh)
	{
		COLLADAFW::MeshPrimitiveArray& prim_arr = mesh->getMeshPrimitives();
		int i;

		const char *name = get_dae_name(mesh);
		
		for (i = 0; i < prim_arr.getCount(); i++) {
			
			COLLADAFW::MeshPrimitive *mp = prim_arr[i];
			COLLADAFW::MeshPrimitive::PrimitiveType type = mp->getPrimitiveType();

			const char *type_str = primTypeToStr(type);
			
			// OpenCollada passes POLYGONS type for <polylist>
			if (type == COLLADAFW::MeshPrimitive::POLYLIST || type == COLLADAFW::MeshPrimitive::POLYGONS) {

				COLLADAFW::Polygons *mpvc = (COLLADAFW::Polygons*)mp;
				COLLADAFW::Polygons::VertexCountArray& vca = mpvc->getGroupedVerticesVertexCountArray();
				
				for(int j = 0; j < vca.getCount(); j++){
					int count = vca[j];
					if (count != 3 && count != 4) {
						fprintf(stderr, "Primitive %s in %s has at least one face with vertex count > 4 or < 3\n",
								type_str, name);
						return false;
					}
				}
					
			}
			else if(type != COLLADAFW::MeshPrimitive::TRIANGLES) {
				fprintf(stderr, "Primitive type %s is not supported.\n", type_str);
				return false;
			}
		}
		
		if (mesh->getPositions().empty()) {
			fprintf(stderr, "Mesh %s has no vertices.\n", name);
			return false;
		}

		return true;
	}

	const char *get_dae_name(COLLADAFW::Geometry *geom)
	{
		const std::string& name = geom->getName();
		return name.size() ? name.c_str() : geom->getOriginalId().c_str();
	}

	void read_vertices(COLLADAFW::Mesh *mesh, Mesh *me)
	{
		// vertices	
		me->totvert = mesh->getPositions().getFloatValues()->getCount() / 3;
		me->mvert = (MVert*)CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, NULL, me->totvert);

		const COLLADAFW::MeshVertexData& pos = mesh->getPositions();
		MVert *mvert;
		int i, j;

		for (i = 0, mvert = me->mvert; i < me->totvert; i++, mvert++) {
			j = i * 3;

			if (pos.getType() == COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT) {
				const float *array = pos.getFloatValues()->getData();
				mvert->co[0] = array[j];
				mvert->co[1] = array[j + 1];
				mvert->co[2] = array[j + 2];
			}
			else if (pos.getType() == COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE){
				const double *array = pos.getDoubleValues()->getData();
				mvert->co[0] = (float)array[j];
				mvert->co[1] = (float)array[j + 1];
				mvert->co[2] = (float)array[j + 2];
			}
			else {
				fprintf(stderr, "Cannot read vertex positions: unknown data type.\n");
				break;
			}
		}
	}

	// TODO: import uv set names
	void read_faces(COLLADAFW::Mesh *mesh, Mesh *me)
	{
		int i;

		// allocate faces
		me->totface = mesh->getFacesCount();
		me->mface = (MFace*)CustomData_add_layer(&me->fdata, CD_MFACE, CD_CALLOC, NULL, me->totface);
		
		// allocate UV layers
		int totuvset = mesh->getUVCoords().getInputInfosArray().getCount();

		for (i = 0; i < totuvset; i++) {
			CustomData_add_layer(&me->fdata, CD_MTFACE, CD_CALLOC, NULL, me->totface);
			//this->set_layername_map[i] = CustomData_get_layer_name(&me->fdata, CD_MTFACE, i);
		}

		// activate the first uv layer
		if (totuvset) me->mtface = (MTFace*)CustomData_get_layer_n(&me->fdata, CD_MTFACE, 0);

		UVDataWrapper uvs(mesh->getUVCoords());

#ifdef COLLADA_DEBUG
		uvs.print();
#endif

		MFace *mface = me->mface;

		MaterialIdPrimitiveArrayMap mat_prim_map;

		int face_index = 0;

		COLLADAFW::MeshPrimitiveArray& prim_arr = mesh->getMeshPrimitives();

		for (i = 0; i < prim_arr.getCount(); i++) {
			
 			COLLADAFW::MeshPrimitive *mp = prim_arr[i];

			// faces
			size_t prim_totface = mp->getFaceCount();
			unsigned int *indices = mp->getPositionIndices().getData();
			int j, k;
			int type = mp->getPrimitiveType();
			int index = 0;
			
			// since we cannot set mface->mat_nr here, we store a portion of me->mface in Primitive
			Primitive prim = {mface, 0};
			COLLADAFW::IndexListArray& index_list_array = mp->getUVCoordIndicesArray();

#ifdef COLLADA_DEBUG
			fprintf(stderr, "Primitive %d:\n", i);
			for (int j = 0; j < totuvset; j++) {
				print_index_list(*index_list_array[j]);
			}
#endif
			
			if (type == COLLADAFW::MeshPrimitive::TRIANGLES) {
				for (j = 0; j < prim_totface; j++){
					
					set_face_indices(mface, indices, false);
					indices += 3;

					for (k = 0; k < totuvset; k++) {
						// get mtface by face index and uv set index
						MTFace *mtface = (MTFace*)CustomData_get_layer_n(&me->fdata, CD_MTFACE, k);
						set_face_uv(&mtface[face_index], uvs, k, *index_list_array[k], index, false);
					}
					
					index += 3;
					mface++;
					face_index++;
					prim.totface++;
				}
			}
			else if (type == COLLADAFW::MeshPrimitive::POLYLIST || type == COLLADAFW::MeshPrimitive::POLYGONS) {
				COLLADAFW::Polygons *mpvc =	(COLLADAFW::Polygons*)mp;
				COLLADAFW::Polygons::VertexCountArray& vcounta = mpvc->getGroupedVerticesVertexCountArray();
				
				for (j = 0; j < prim_totface; j++) {

					// face
					int vcount = vcounta[j];

					set_face_indices(mface, indices, vcount == 4);
					indices += vcount;
					
					// do the trick if needed
					if (vcount == 4 && mface->v4 == 0)
						rotate_face_indices(mface);

					// set mtface for each uv set
					// it is assumed that all primitives have equal number of UV sets

					for (k = 0; k < totuvset; k++) {
						// get mtface by face index and uv set index
						MTFace *mtface = (MTFace*)CustomData_get_layer_n(&me->fdata, CD_MTFACE, k);
						set_face_uv(&mtface[face_index], uvs, k, *index_list_array[k], index, mface->v4 != 0);
					}

					index += mface->v4 ? 4 : 3;
					mface++;
					face_index++;
					prim.totface++;
				}
			}
			
		   	mat_prim_map[mp->getMaterialId()].push_back(prim);
		}

		geom_uid_mat_mapping_map[mesh->getUniqueId()] = mat_prim_map;
	}

public:

	MeshImporter(ArmatureImporter *arm, Scene *sce) : scene(sce), armature_importer(arm) {}
	
	
	MTex *assign_textures_to_uvlayer(COLLADAFW::InstanceGeometry::TextureCoordinateBinding &ctexture,
									 Mesh *me, TexIndexTextureArrayMap& texindex_texarray_map,
									 MTex *color_texture)
	{
		
		COLLADAFW::TextureMapId texture_index = ctexture.textureMapId;
		
		char *uvname = CustomData_get_layer_name(&me->fdata, CD_MTFACE, ctexture.setIndex);
		
		if (texindex_texarray_map.find(texture_index) == texindex_texarray_map.end()) {
			
			fprintf(stderr, "Cannot find texture array by texture index.\n");
			return NULL;
		}
		
		std::vector<MTex*> textures = texindex_texarray_map[texture_index];
		
		std::vector<MTex*>::iterator it;
		
		for (it = textures.begin(); it != textures.end(); it++) {
			
			MTex *texture = *it;
			
			if (texture) {
				strcpy(texture->uvname, uvname);
				if (texture->mapto == MAP_COL) color_texture = texture;
			}
		}
		return color_texture;
	}
	
	MTFace *assign_material_to_geom(COLLADAFW::InstanceGeometry::MaterialBinding cmaterial,
									std::map<COLLADAFW::UniqueId, Material*>& uid_material_map,
									Object *ob, const COLLADAFW::UniqueId *geom_uid, 
									MTex **color_texture, char *layername, MTFace *texture_face,
									std::map<Material*, TexIndexTextureArrayMap>& material_texture_mapping_map, int mat_index)
	{
		Mesh *me = (Mesh*)ob->data;
		const COLLADAFW::UniqueId& ma_uid = cmaterial.getReferencedMaterial();
		
		// do we know this material?
		if (uid_material_map.find(ma_uid) == uid_material_map.end()) {
			
			fprintf(stderr, "Cannot find material by UID.\n");
			return NULL;
		}
		
		Material *ma = uid_material_map[ma_uid];
		assign_material(ob, ma, ob->totcol + 1);
		
		COLLADAFW::InstanceGeometry::TextureCoordinateBindingArray& tex_array = 
			cmaterial.getTextureCoordinateBindingArray();
		TexIndexTextureArrayMap texindex_texarray_map = material_texture_mapping_map[ma];
		unsigned int i;
		// loop through <bind_vertex_inputs>
		for (i = 0; i < tex_array.getCount(); i++) {
			
			*color_texture = assign_textures_to_uvlayer(tex_array[i], me, texindex_texarray_map,
														*color_texture);
		}
		
		// if material has color texture
		if (*color_texture && strlen((*color_texture)->uvname)) {
			// set tface
			if (strcmp(layername, (*color_texture)->uvname) != 0) {
				
				texture_face = (MTFace*)CustomData_get_layer_named(&me->fdata, CD_MTFACE,
																   (*color_texture)->uvname);
				strcpy(layername, (*color_texture)->uvname);
			}
		}
		
		MaterialIdPrimitiveArrayMap& mat_prim_map = geom_uid_mat_mapping_map[*geom_uid];
		COLLADAFW::MaterialId mat_id = cmaterial.getMaterialId();
		
		// assign material indices to mesh faces
		if (mat_prim_map.find(mat_id) != mat_prim_map.end()) {
			
			std::vector<Primitive>& prims = mat_prim_map[mat_id];
			
			std::vector<Primitive>::iterator it;
			
			for (it = prims.begin(); it != prims.end(); it++) {
				Primitive& prim = *it;
				i = 0;
				while (i++ < prim.totface) {
					prim.mface->mat_nr = mat_index;
					prim.mface++;
					// bind image to tface
					if (texture_face && (*color_texture)) {
						texture_face->mode = TF_TEX;
						texture_face->tpage = (Image*)(*color_texture)->tex->ima;
						texture_face++;
					}
				}
			}
		}
		
		return texture_face;
	}
	
	
	Object *create_mesh_object(COLLADAFW::Node *node, COLLADAFW::InstanceGeometry *geom,
							   bool isController,
							   std::map<COLLADAFW::UniqueId, Material*>& uid_material_map,
							   std::map<Material*, TexIndexTextureArrayMap>& material_texture_mapping_map)
	{
		const COLLADAFW::UniqueId *geom_uid = &geom->getInstanciatedObjectId();
		
		// check if node instanciates controller or geometry
		if (isController) {
			
			geom_uid = armature_importer->get_geometry_uid(*geom_uid);
			
			if (!geom_uid) {
				fprintf(stderr, "Couldn't find a mesh UID by controller's UID.\n");
				return NULL;
			}
		}
		else {
			
			if (uid_mesh_map.find(*geom_uid) == uid_mesh_map.end()) {
				// this could happen if a mesh was not created
				// (e.g. if it contains unsupported geometry)
				fprintf(stderr, "Couldn't find a mesh by UID.\n");
				return NULL;
			}
		}
		if (!uid_mesh_map[*geom_uid]) return NULL;
		
		Object *ob = add_object(scene, OB_MESH);
		
		// name Object
		const std::string& id = node->getOriginalId();
		if (id.length())
			rename_id(&ob->id, (char*)id.c_str());
		
		// replace ob->data freeing the old one
		Mesh *old_mesh = (Mesh*)ob->data;

		set_mesh(ob, uid_mesh_map[*geom_uid]);
		
		if (old_mesh->id.us == 0) free_libblock(&G.main->mesh, old_mesh);
		
		char layername[100];
		MTFace *texture_face = NULL;
		MTex *color_texture = NULL;
		
		COLLADAFW::InstanceGeometry::MaterialBindingArray& mat_array = geom->getMaterialBindings();
		
		// loop through geom's materials
		for (unsigned int i = 0; i < mat_array.getCount(); i++)	{
			
			texture_face = assign_material_to_geom(mat_array[i], uid_material_map, ob, geom_uid,
												   &color_texture, layername, texture_face,
												   material_texture_mapping_map, i);
		}
			
		return ob;
	}

	// create a mesh storing a pointer in a map so it can be retrieved later by geometry UID
	bool write_geometry(const COLLADAFW::Geometry* geom) 
	{
		// TODO: import also uvs, normals
		// XXX what to do with normal indices?
		// XXX num_normals may be != num verts, then what to do?

		// check geometry->getType() first
		if (geom->getType() != COLLADAFW::Geometry::GEO_TYPE_MESH) {
			// TODO: report warning
			fprintf(stderr, "Mesh type %s is not supported\n", geomTypeToStr(geom->getType()));
			return true;
		}
		
		COLLADAFW::Mesh *mesh = (COLLADAFW::Mesh*)geom;

		if (!is_nice_mesh(mesh)) {
			fprintf(stderr, "Ignoring mesh %s\n", get_dae_name(mesh));
			return true;
		}
		
		const std::string& str_geom_id = mesh->getOriginalId();
		Mesh *me = add_mesh((char*)str_geom_id.c_str());

		// store the Mesh pointer to link it later with an Object
		this->uid_mesh_map[mesh->getUniqueId()] = me;
		
		read_vertices(mesh, me);

		read_faces(mesh, me);
		
 		mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);
// 		make_edges(me, 0);

		return true;
	}

};

/*

  COLLADA Importer limitations:

  - no multiple scene import, all objects are added to active scene

 */
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

	std::map<COLLADAFW::UniqueId, Image*> uid_image_map;
	std::map<COLLADAFW::UniqueId, Material*> uid_material_map;
	std::map<COLLADAFW::UniqueId, Material*> uid_effect_map;
	std::map<COLLADAFW::UniqueId, Camera*> uid_camera_map;
	std::map<COLLADAFW::UniqueId, Lamp*> uid_lamp_map;
	// maps for assigning textures to uv layers
	//std::map<COLLADAFW::TextureMapId, char*> set_layername_map;
	std::map<Material*, TexIndexTextureArrayMap> material_texture_mapping_map;
	// animation
	std::map<COLLADAFW::UniqueId, std::vector<FCurve*> > uid_fcurve_map;
	struct AnimatedTransform {
		Object *ob;
		// COLLADAFW::Node *node;
		COLLADAFW::Transformation *tm; // which transform is animated by an AnimationList->id
	};
	// Nodes don't share AnimationLists (Arystan)
	std::map<COLLADAFW::UniqueId, AnimatedTransform> uid_animated_map; // AnimationList->uniqueId to AnimatedObject map

public:

	/** Constructor. */
	Writer(bContext *C, const char *filename) : mContext(C), mFilename(filename),
												armature_importer(&unit_converter, CTX_data_scene(C)),
												mesh_importer(&armature_importer, CTX_data_scene(C)) {};

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
	}

	/** When this method is called, the writer must write the global document asset.
		@return The writer should return true, if writing succeeded, false otherwise.*/
	virtual bool writeGlobalAsset ( const COLLADAFW::FileInfo* asset ) 
	{
		// XXX take up_axis, unit into account
		// COLLADAFW::FileInfo::Unit unit = asset->getUnit();
		// COLLADAFW::FileInfo::UpAxisType upAxis = asset->getUpAxisType();
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
	
	void write_node (COLLADAFW::Node *node, Scene *sce, Object *par = NULL)
	{
		// XXX linking object with the first <instance_geometry>, though a node may have more of them...
		// maybe join multiple <instance_...> meshes into 1, and link object with it? not sure...
		if (node->getType() != COLLADAFW::Node::NODE) {

			if (node->getType() == COLLADAFW::Node::JOINT) {
				armature_importer.add_root_joint(node);
			}

			return;
		}
		
		COLLADAFW::InstanceGeometryPointerArray &geom = node->getInstanceGeometries();
		COLLADAFW::InstanceCameraPointerArray &camera = node->getInstanceCameras();
		COLLADAFW::InstanceLightPointerArray &lamp = node->getInstanceLights();
		COLLADAFW::InstanceControllerPointerArray &controller = node->getInstanceControllers();
		COLLADAFW::InstanceNodePointerArray &inst_node = node->getInstanceNodes();
		Object *ob = NULL;
		int k;
		
		// <instance_geometry>
		if (geom.getCount() != 0) {
			ob = mesh_importer.create_mesh_object(node, geom[0], false, uid_material_map, material_texture_mapping_map);
		}
		// <instance_camera>
		else if (camera.getCount() != 0) {
			const COLLADAFW::UniqueId& cam_uid = camera[0]->getInstanciatedObjectId();
			if (uid_camera_map.find(cam_uid) == uid_camera_map.end()) {	
				fprintf(stderr, "Couldn't find camera by UID. \n");
				return;
			}
			ob = add_object(sce, OB_CAMERA);
			Camera *cam = uid_camera_map[cam_uid];
			Camera *old_cam = (Camera*)ob->data;
			old_cam->id.us--;
			ob->data = cam;
			if (old_cam->id.us == 0) free_libblock(&G.main->camera, old_cam);
		}
		// <instance_light>
		else if (lamp.getCount() != 0) {
			const COLLADAFW::UniqueId& lamp_uid = lamp[0]->getInstanciatedObjectId();
			if (uid_lamp_map.find(lamp_uid) == uid_lamp_map.end()) {	
				fprintf(stderr, "Couldn't find lamp by UID. \n");
				return;
			}
			ob = add_object(sce, OB_LAMP);
			Lamp *la = uid_lamp_map[lamp_uid];
			Lamp *old_lamp = (Lamp*)ob->data;
			old_lamp->id.us--;
			ob->data = la;
			if (old_lamp->id.us == 0) free_libblock(&G.main->lamp, old_lamp);
		}
		// <instance_controller>
		else if (controller.getCount() != 0) {
			COLLADAFW::InstanceController *geom = (COLLADAFW::InstanceController*)controller[0];
			ob = mesh_importer.create_mesh_object(node, geom, true, uid_material_map, material_texture_mapping_map);
		}
		// XXX <node> - this is not supported yet
		else if (inst_node.getCount() != 0) {
			return;
		}
		// if node is empty - create empty object
		// XXX empty node may not mean it is empty object, not sure about this
		else {
			ob = add_object(sce, OB_EMPTY);
		}
		// just checking if object wasn't created
		if (ob == NULL) return;
		// if par was given make this object child of the previous 
		if (par != NULL) {
			Object workob;

			ob->parent = par;

			// doing what 'set parent' operator does
			par->recalc |= OB_RECALC_OB;
			ob->parsubstr[0] = 0;
			
			DAG_scene_sort(sce);
			// since ob->obmat is identity, this is not needed?
			/*what_does_parent(sce, ob, &workob);
			Mat4Invert(ob->parentinv, workob.obmat);

			ob->recalc |= OB_RECALC_OB|OB_RECALC_DATA;
			ob->partype = PAROBJECT;
			DAG_scene_sort(sce);*/
		}
		// transform Object
		float rot[3][3];
		Mat3One(rot);
		
		// transform Object and store animation linking info
		for (k = 0; k < node->getTransformations().getCount(); k ++) {
			
			COLLADAFW::Transformation *tm = node->getTransformations()[k];
			COLLADAFW::Transformation::TransformationType type = tm->getTransformationType();

			switch(type) {
			case COLLADAFW::Transformation::TRANSLATE:
				{
					COLLADAFW::Translate *tra = (COLLADAFW::Translate*)tm;
					COLLADABU::Math::Vector3& t = tra->getTranslation();
					ob->loc[0] = (float)t[0];
					ob->loc[1] = (float)t[1];
					ob->loc[2] = (float)t[2];
				}
				break;
			case COLLADAFW::Transformation::ROTATE:
				{
					COLLADAFW::Rotate *ro = (COLLADAFW::Rotate*)tm;
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
					COLLADABU::Math::Vector3& s = ((COLLADAFW::Scale*)tm)->getScale();
					ob->size[0] = (float)s[0];
					ob->size[1] = (float)s[1];
					ob->size[2] = (float)s[2];
				}
				break;
			case COLLADAFW::Transformation::MATRIX:
			case COLLADAFW::Transformation::LOOKAT:
			case COLLADAFW::Transformation::SKEW:
				fprintf(stderr, "MATRIX, LOOKAT and SKEW transformations are not supported yet.\n");
				break;
			}
			
			// AnimationList that drives this Transformation
			const COLLADAFW::UniqueId& anim_list_id = tm->getAnimationList();
			
			// store this so later we can link animation data with ob
			AnimatedTransform anim = {ob, tm};
			this->uid_animated_map[anim_list_id] = anim;
		}
		Mat3ToEul(rot, ob->rot);
		
		// if node has child nodes write them
		COLLADAFW::NodePointerArray &child_nodes = node->getChildNodes();
		for (k = 0; k < child_nodes.getCount(); k++) {	
			
			COLLADAFW::Node *child_node = child_nodes[k];
			write_node(child_node, sce, ob);
		}
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

		// TODO: create a new scene except the selected <visual_scene> - use current blender
		// scene for it
		Scene *sce = CTX_data_scene(mContext);

		for (int i = 0; i < visualScene->getRootNodes().getCount(); i++) {
			COLLADAFW::Node *node = visualScene->getRootNodes()[i];
			const COLLADAFW::Node::NodeType& type = node->getType();

			if (type == COLLADAFW::Node::NODE) {
				write_node(node, sce);
			}
			else if (type == COLLADAFW::Node::JOINT){
				armature_importer.add_root_joint(node);
			}
		}

		if (armature_importer.has_root_joints()) {
			armature_importer.build_armatures();
		}
		
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
	MTex *create_texture(COLLADAFW::EffectCommon *ef, COLLADAFW::Texture &ctex, Material *ma, int i, TexIndexTextureArrayMap &texindex_texarray_map)
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
		ma->mtex[i]->tex = add_texture("texture");
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
			// XXX setting specular hardness instead of specularity intensity
			ma->har = ef->getShininess().getFloatValue() * 4;
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
		COLLADAFW::Texture ctex;
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
			ctex = ef->getDiffuse().getTexture(); 
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
			ctex = ef->getAmbient().getTexture(); 
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
			ctex = ef->getSpecular().getTexture(); 
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
			ctex = ef->getReflective().getTexture(); 
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
			ctex = ef->getEmission().getTexture(); 
			mtex = create_texture(ef, ctex, ma, i, texindex_texarray_map);
			if (mtex != NULL) {
				mtex->mapto = MAP_EMIT; 
				i++;
			}
		}
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
			fprintf(stderr, "<effect> hasn't got <profile_COMMON>s.\n Currently we support only them. \n");
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
		std::string name = camera->getOriginalId();
		Camera *cam = (Camera*)add_camera((char*)name.c_str());
		if (!cam) {
			fprintf(stderr, "Cannot create camera. \n");
			return true;
		}
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

		BLI_split_dirfile_basic(filename, dir, NULL);
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
		std::string name = light->getOriginalId();
		Lamp *lamp = (Lamp*)add_lamp((char*)name.c_str());
		if (!lamp) {
			fprintf(stderr, "Cannot create lamp. \n");
			return true;
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
			}
			break;
		case COLLADAFW::Light::DIRECTIONAL_LIGHT:
			{
				lamp->type = LA_SUN;
			}
			break;
		case COLLADAFW::Light::POINT_LIGHT:
			{
				lamp->type = LA_AREA;
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
		
		// XXX import light options*/
		return true;
	}
	
	float get_float(COLLADAFW::FloatOrDoubleArray array, int i)
	{
		switch(array.getType()) {
		case COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT:
			{
				COLLADAFW::ArrayPrimitiveType<float> *values = array.getFloatValues();
				if (!values->empty())
					return (*values)[i];
				else return 0;
			}
		case COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE:
			{
				COLLADAFW::ArrayPrimitiveType<double> *values = array.getDoubleValues();
				if (!values->empty())
					return (float)(*values)[i];
				else return 0;
			}
		}
	}
	
	void write_curves(const COLLADAFW::Animation* anim,
					  COLLADAFW::AnimationCurve *curve,
					  COLLADAFW::FloatOrDoubleArray input,
					  COLLADAFW::FloatOrDoubleArray output,
					  COLLADAFW::FloatOrDoubleArray intan,
					  COLLADAFW::FloatOrDoubleArray outtan, size_t dim, float fps)
	{
		int i;
		char *path = "location";
		if (dim == 1) {
			// create fcurve
			FCurve *fcu = (FCurve*)MEM_callocN(sizeof(FCurve), "FCurve");
			if (!fcu) {
				fprintf(stderr, "Cannot create fcurve. \n");
				return;
			}

			fcu->flag = (FCURVE_VISIBLE|FCURVE_AUTO_HANDLES|FCURVE_SELECTED);
			fcu->rna_path = BLI_strdupn(path, strlen(path));
			fcu->array_index = 0;
			fcu->totvert = curve->getKeyCount();
			
			// create beztriple for each key
			for (i = 0; i < curve->getKeyCount(); i++) {
				BezTriple bez;
				memset(&bez, 0, sizeof(BezTriple));
				// intangent
				bez.vec[0][0] = get_float(intan, i + i) * fps;
				bez.vec[0][1] = get_float(intan, i + i + 1);
				// input, output
				bez.vec[1][0] = get_float(input, i) * fps;
				bez.vec[1][1] = get_float(output, i);
				// outtangent
				bez.vec[2][0] = get_float(outtan, i + i) * fps;
				bez.vec[2][1] = get_float(outtan, i + i + 1);
				bez.ipo = U.ipo_new; /* use default interpolation mode here... */
				bez.f1 = bez.f2 = bez.f3 = SELECT;
				bez.h1 = bez.h2 = HD_AUTO;
				insert_bezt_fcurve(fcu, &bez);
				calchandles_fcurve(fcu);
			}
			// map fcurve to animation's UID
			this->uid_fcurve_map[anim->getUniqueId()].push_back(fcu);
		}
		else if(dim == 3) {
			for (i = 0; i < dim; i++ ) {
				// create fcurve
				FCurve *fcu = (FCurve*)MEM_callocN(sizeof(FCurve), "FCurve");
				if (!fcu) {
					fprintf(stderr, "Cannot create fcurve. \n");
					continue;
				}
				
				fcu->flag = (FCURVE_VISIBLE|FCURVE_AUTO_HANDLES|FCURVE_SELECTED);
				fcu->rna_path = BLI_strdupn(path, strlen(path));
				fcu->array_index = 0;
				fcu->totvert = curve->getKeyCount();
				
				// create beztriple for each key
				for (int j = 0; j < curve->getKeyCount(); j++) {
					BezTriple bez;
					memset(&bez, 0, sizeof(BezTriple));
					// intangent
					bez.vec[0][0] = get_float(intan, j * 6 + i + i) * fps;
					bez.vec[0][1] = get_float(intan, j * 6 + i + i + 1);
					// input, output
					bez.vec[1][0] = get_float(input, j) * fps; 
					bez.vec[1][1] = get_float(output, j * 3 + i);
					// outtangent
					bez.vec[2][0] = get_float(outtan, j * 6 + i + i) * fps;
					bez.vec[2][1] = get_float(outtan, j * 6 + i + i + 1);
					bez.ipo = U.ipo_new; /* use default interpolation mode here... */
					bez.f1 = bez.f2 = bez.f3 = SELECT;
					bez.h1 = bez.h2 = HD_AUTO;
					insert_bezt_fcurve(fcu, &bez);
					calchandles_fcurve(fcu);
				}
				// map fcurve to animation's UID
				this->uid_fcurve_map[anim->getUniqueId()].push_back(fcu);
				
			}
		}
	}
	
	// this function is called only for animations that pass COLLADAFW::validate
	virtual bool writeAnimation( const COLLADAFW::Animation* anim ) 
	{
		if (anim->getAnimationType() == COLLADAFW::Animation::ANIMATION_CURVE) {
			COLLADAFW::AnimationCurve *curve = (COLLADAFW::AnimationCurve*)anim;
			Scene *scene = CTX_data_scene(mContext);
			float fps = (float)FPS;
			// I wonder how do we use this (Arystan)
			size_t dim = curve->getOutDimension();
			
			// XXX Don't know if it's necessary
			// Should we check outPhysicalDimension?
			if (curve->getInPhysicalDimension() != COLLADAFW::PHYSICAL_DIMENSION_TIME) {
				fprintf(stderr, "Inputs physical dimension is not time. \n");
				return true;
			}
			COLLADAFW::FloatOrDoubleArray input = curve->getInputValues();
			COLLADAFW::FloatOrDoubleArray output = curve->getOutputValues();
			COLLADAFW::FloatOrDoubleArray intan = curve->getInTangentValues();
			COLLADAFW::FloatOrDoubleArray outtan = curve->getOutTangentValues();
			// a curve can have mixed interpolation type,
			// in this case curve->getInterpolationTypes returns a list of interpolation types per key
			COLLADAFW::AnimationCurve::InterpolationType interp = curve->getInterpolationType();
			
			if (interp != COLLADAFW::AnimationCurve::INTERPOLATION_MIXED) {
				switch (interp) {
				case COLLADAFW::AnimationCurve::INTERPOLATION_LINEAR:
					// support this
					write_curves(anim, curve, input, output, intan, outtan, dim, fps);
					break;
				case COLLADAFW::AnimationCurve::INTERPOLATION_BEZIER:
					// and this
					write_curves(anim, curve, input, output, intan, outtan, dim, fps);
					break;
				case COLLADAFW::AnimationCurve::INTERPOLATION_CARDINAL:
				case COLLADAFW::AnimationCurve::INTERPOLATION_HERMITE:
				case COLLADAFW::AnimationCurve::INTERPOLATION_BSPLINE:
				case COLLADAFW::AnimationCurve::INTERPOLATION_STEP:
					fprintf(stderr, "CARDINAL, HERMITE, BSPLINE and STEP anim interpolation types not supported yet.\n");
					break;
				}
			}
			else {
				// not supported yet
				fprintf(stderr, "MIXED anim interpolation type is not supported yet.\n");
			}
		}
		else {
			fprintf(stderr, "FORMULA animation type is not supported yet.\n");
		}
		
		return true;
	}
	
	void change_fcurve(Object *ob, const COLLADAFW::UniqueId& anim_id, char *rna_path, int array_index)
	{
		if (uid_fcurve_map.find(anim_id) == uid_fcurve_map.end()) {
			fprintf(stderr, "Cannot find fcurves by UID.\n");
			return;
		}
		ID *id = &ob->id;
		bAction *act;
		if (!ob->adt || !ob->adt->action)
			act = verify_adt_action(id, 1);
		else 
			act = verify_adt_action(id, 0);
		if (!ob->adt || !ob->adt->action) {
			fprintf(stderr, "Cannot create anim data or action for this object. \n");
			return;
		}
		FCurve *fcu;
		std::vector<FCurve*> fcurves = uid_fcurve_map[anim_id];
		std::vector<FCurve*>::iterator it;
		int i = 0;
		for (it = fcurves.begin(); it != fcurves.end(); it++) {
			fcu = *it;
			strcpy(fcu->rna_path, rna_path);
			if (array_index == -1)
				fcu->array_index = i;
			else
				fcu->array_index = array_index;
			// convert degrees to radians for rotation
			if (strcmp(rna_path, "rotation") == 0) {
				for(int j = 0; j < fcu->totvert; j++) {
					float rot_intan = fcu->bezt[j].vec[0][1];
					float rot_output = fcu->bezt[j].vec[1][1];
					float rot_outtan = fcu->bezt[j].vec[2][1];
				    fcu->bezt[j].vec[0][1] = rot_intan * M_PI / 180.0f;
					fcu->bezt[j].vec[1][1] = rot_output * M_PI / 180.0f;
					fcu->bezt[j].vec[2][1] = rot_outtan * M_PI / 180.0f;
				}
			}
			i++;
			BLI_addtail(&act->curves, fcu);
		}
	}
	
	// called on post-process stage after writeVisualScenes
	virtual bool writeAnimationList( const COLLADAFW::AnimationList* animationList ) 
	{
		const COLLADAFW::UniqueId& anim_list_id = animationList->getUniqueId();

		// possible in case we cannot interpret some transform
		if (uid_animated_map.find(anim_list_id) == uid_animated_map.end()) {
			return true;
		}
		
		// what does this AnimationList animate?
		AnimatedTransform& animated = uid_animated_map[anim_list_id];
		char *loc = "location";
		char *rotate = "rotation";
		char *scale = "scale";
		Object *ob = animated.ob;
		
		const COLLADAFW::AnimationList::AnimationBindings& bindings = animationList->getAnimationBindings();
		switch (animated.tm->getTransformationType()) {
		case COLLADAFW::Transformation::TRANSLATE:
			{
				for (int i = 0; i < bindings.getCount(); i++) {
					const COLLADAFW::AnimationList::AnimationBinding& binding = bindings[i];
					COLLADAFW::UniqueId anim_uid = binding.animation;
					
					switch (binding.animationClass) {
					case COLLADAFW::AnimationList::POSITION_X:
						change_fcurve(ob, anim_uid, loc, 0);
						break;
					case COLLADAFW::AnimationList::POSITION_Y:
						change_fcurve(ob, anim_uid, loc, 1);
						break;
					case COLLADAFW::AnimationList::POSITION_Z:
						change_fcurve(ob, anim_uid, loc, 2);
						break;
					case COLLADAFW::AnimationList::POSITION_XYZ:
						change_fcurve(ob, anim_uid, loc, -1);
						break;
					default:
						fprintf(stderr, "AnimationClass %d is not supported for TRANSLATE transformation.\n", binding.animationClass);
					}
				}
			}
			break;
		case COLLADAFW::Transformation::ROTATE:
			{
				COLLADAFW::Rotate* rot = (COLLADAFW::Rotate*)animated.tm;
				COLLADABU::Math::Vector3& axis = rot->getRotationAxis();
				
				for (int i = 0; i < bindings.getCount(); i++) {
					const COLLADAFW::AnimationList::AnimationBinding& binding = bindings[i];
					COLLADAFW::UniqueId anim_uid = binding.animation;
					
					switch (binding.animationClass) {
					case COLLADAFW::AnimationList::ANGLE:
						if (COLLADABU::Math::Vector3::UNIT_X == axis) {
							change_fcurve(ob, anim_uid, rotate, 0);
						}
						else if (COLLADABU::Math::Vector3::UNIT_Y == axis) {
							change_fcurve(ob, anim_uid, rotate, 1);
						}
						else if (COLLADABU::Math::Vector3::UNIT_Z == axis) {
							change_fcurve(ob, anim_uid, rotate, 2);
						}
						break;
					case COLLADAFW::AnimationList::AXISANGLE:
						// convert axis-angle to quat? or XYZ?
						break;
					default:
						fprintf(stderr, "AnimationClass %d is not supported for ROTATE transformation.\n",
								binding.animationClass);
					}
				}
			}
			break;
		case COLLADAFW::Transformation::SCALE:
			{
				// same as for TRANSLATE
				for (int i = 0; i < bindings.getCount(); i++) {
					const COLLADAFW::AnimationList::AnimationBinding& binding = bindings[i];
					COLLADAFW::UniqueId anim_uid = binding.animation;
					
					switch (binding.animationClass) {
					case COLLADAFW::AnimationList::POSITION_X:
						change_fcurve(ob, anim_uid, scale, 0);
						break;
					case COLLADAFW::AnimationList::POSITION_Y:
						change_fcurve(ob, anim_uid, scale, 1);
						break;
					case COLLADAFW::AnimationList::POSITION_Z:
						change_fcurve(ob, anim_uid, scale, 2);
						break;
					case COLLADAFW::AnimationList::POSITION_XYZ:
						change_fcurve(ob, anim_uid, scale, -1);
						break;
					default:
						fprintf(stderr, "AnimationClass %d is not supported for TRANSLATE transformation.\n", binding.animationClass);
					}
				}
			}
			break;
		case COLLADAFW::Transformation::MATRIX:
		case COLLADAFW::Transformation::SKEW:
		case COLLADAFW::Transformation::LOOKAT:
			fprintf(stderr, "Animation of MATRIX, SKEW and LOOKAT transformations is not supported yet.\n");
			break;
		}
		
		return true;
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
};

void DocumentImporter::import(bContext *C, const char *filename)
{
	Writer w(C, filename);
	w.write();
}
