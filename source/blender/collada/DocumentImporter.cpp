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
#include "COLLADAFWSampler.h"
#include "COLLADAFWSkinController.h"
#include "COLLADAFWSkinControllerData.h"
#include "COLLADAFWTransformation.h"
#include "COLLADAFWTranslate.h"
#include "COLLADAFWRotate.h"
#include "COLLADAFWScale.h"
#include "COLLADAFWMatrix.h"
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
#include "ED_mesh.h" // ED_vgroup_vert_add, ...
#include "ED_anim_api.h"
#include "WM_types.h"
#include "WM_api.h"

#include "BKE_main.h"
#include "BKE_customdata.h"
#include "BKE_library.h"
#include "BKE_texture.h"
#include "BKE_fcurve.h"
#include "BKE_depsgraph.h"
#include "BLI_util.h"
#include "BKE_displist.h"
#include "BLI_arithb.h"
}
#include "BKE_armature.h"
#include "BKE_mesh.h"
#include "BKE_global.h"
#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_image.h"
#include "BKE_material.h"
#include "BKE_utildefines.h"
#include "BKE_action.h"

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
#include "collada_internal.h"

#include <string>
#include <map>

#include <math.h>
#include <float.h>

// #define COLLADA_DEBUG

char *CustomData_get_layer_name(const struct CustomData *data, int type, int n);

// armature module internal func, it's not good to use it here? (Arystan)
struct EditBone *addEditBone(struct bArmature *arm, char *name);

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

// works for COLLADAFW::Node, COLLADAFW::Geometry
template<class T>
const char *get_dae_name(T *node)
{
	const std::string& name = node->getName();
	return name.size() ? name.c_str() : node->getOriginalId().c_str();
}

// use this for retrieving bone names, since these must be unique
template<class T>
const char *get_joint_name(T *node)
{
	const std::string& id = node->getOriginalId();
	return id.size() ? id.c_str() : node->getName().c_str();
}

float get_float_value(const COLLADAFW::FloatOrDoubleArray& array, int index)
{
	if (index >= array.getValuesCount())
		return 0.0f;

	if (array.getType() == COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT)
		return array.getFloatValues()->getData()[index];
	else 
		return array.getDoubleValues()->getData()[index];
}

typedef std::map<COLLADAFW::TextureMapId, std::vector<MTex*> > TexIndexTextureArrayMap;

class TransformReader : public TransformBase
{
protected:

	UnitConverter *unit_converter;

	struct Animation {
		Object *ob;
		COLLADAFW::Node *node;
		COLLADAFW::Transformation *tm; // which transform is animated by an AnimationList->id
	};

public:

	TransformReader(UnitConverter* conv) : unit_converter(conv) {}

	void get_node_mat(float mat[][4], COLLADAFW::Node *node, std::map<COLLADAFW::UniqueId, Animation> *animation_map,
					  Object *ob)
	{
		float cur[4][4];
		float copy[4][4];

		Mat4One(mat);
		
		for (int i = 0; i < node->getTransformations().getCount(); i++) {

			COLLADAFW::Transformation *tm = node->getTransformations()[i];
			COLLADAFW::Transformation::TransformationType type = tm->getTransformationType();

			switch(type) {
			case COLLADAFW::Transformation::TRANSLATE:
				{
					COLLADAFW::Translate *tra = (COLLADAFW::Translate*)tm;
					COLLADABU::Math::Vector3& t = tra->getTranslation();

					Mat4One(cur);
					cur[3][0] = (float)t[0];
					cur[3][1] = (float)t[1];
					cur[3][2] = (float)t[2];
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
					
					QuatToMat4(quat, cur);
				}
				break;
			case COLLADAFW::Transformation::SCALE:
				{
					COLLADABU::Math::Vector3& s = ((COLLADAFW::Scale*)tm)->getScale();
					float size[3] = {(float)s[0], (float)s[1], (float)s[2]};
					SizeToMat4(size, cur);
				}
				break;
			case COLLADAFW::Transformation::MATRIX:
				{
					unit_converter->mat4_from_dae(cur, ((COLLADAFW::Matrix*)tm)->getMatrix());
				}
				break;
			case COLLADAFW::Transformation::LOOKAT:
			case COLLADAFW::Transformation::SKEW:
				fprintf(stderr, "LOOKAT and SKEW transformations are not supported yet.\n");
				break;
			}

			Mat4CpyMat4(copy, mat);
			Mat4MulMat4(mat, cur, copy);

			if (animation_map) {
				// AnimationList that drives this Transformation
				const COLLADAFW::UniqueId& anim_list_id = tm->getAnimationList();
			
				// store this so later we can link animation data with ob
				Animation anim = {ob, node, tm};
				(*animation_map)[anim_list_id] = anim;
			}
		}
	}
};

// only for ArmatureImporter to "see" MeshImporter::get_object_by_geom_uid
class MeshImporterBase
{
public:
	virtual Object *get_object_by_geom_uid(const COLLADAFW::UniqueId& geom_uid) = 0;
};

// ditto as above
class AnimationImporterBase
{
public:
	virtual void change_eul_to_quat(Object *ob, bAction *act) = 0;
};

class ArmatureImporter : private TransformReader
{
private:
	Scene *scene;
	UnitConverter *unit_converter;

	// std::map<int, JointData> joint_index_to_joint_info_map;
	// std::map<COLLADAFW::UniqueId, int> joint_id_to_joint_index_map;

	struct LeafBone {
		// COLLADAFW::Node *node;
		EditBone *bone;
		char name[32];
		float mat[4][4]; // bone matrix, derived from inv_bind_mat
	};
	std::vector<LeafBone> leaf_bones;
	// int bone_direction_row; // XXX not used
	float leaf_bone_length;
	int totbone;
	// XXX not used
	// float min_angle; // minimum angle between bone head-tail and a row of bone matrix

#if 0
	struct ArmatureJoints {
		Object *ob_arm;
		std::vector<COLLADAFW::Node*> root_joints;
	};
	std::vector<ArmatureJoints> armature_joints;
#endif

	Object *empty; // empty for leaf bones

	std::map<COLLADAFW::UniqueId, COLLADAFW::UniqueId> geom_uid_by_controller_uid;
	std::map<COLLADAFW::UniqueId, COLLADAFW::Node*> joint_by_uid; // contains all joints
	std::vector<COLLADAFW::Node*> root_joints;

	std::vector<Object*> armature_objects;

	MeshImporterBase *mesh_importer;
	AnimationImporterBase *anim_importer;

	// This is used to store data passed in write_controller_data.
	// Arrays from COLLADAFW::SkinControllerData lose ownership, so do this class members
	// so that arrays don't get freed until we free them explicitly.
	class SkinInfo
	{
	private:
		// to build armature bones from inverse bind matrices
		struct JointData {
			float inv_bind_mat[4][4]; // joint inverse bind matrix
			COLLADAFW::UniqueId joint_uid; // joint node UID
			// Object *ob_arm;			  // armature object
		};

		float bind_shape_matrix[4][4];

		// data from COLLADAFW::SkinControllerData, each array should be freed
		COLLADAFW::UIntValuesArray joints_per_vertex;
		COLLADAFW::UIntValuesArray weight_indices;
		COLLADAFW::IntValuesArray joint_indices;
		// COLLADAFW::FloatOrDoubleArray weights;
		std::vector<float> weights;

		std::vector<JointData> joint_data; // index to this vector is joint index

		UnitConverter *unit_converter;

		Object *ob_arm;
		COLLADAFW::UniqueId controller_uid;

	public:

		SkinInfo() {}

		SkinInfo(const SkinInfo& skin) : weights(skin.weights),
										 joint_data(skin.joint_data),
										 unit_converter(skin.unit_converter),
										 ob_arm(skin.ob_arm),
										 controller_uid(skin.controller_uid)
		{
			Mat4CpyMat4(bind_shape_matrix, (float (*)[4])skin.bind_shape_matrix);

			transfer_uint_array_data_const(skin.joints_per_vertex, joints_per_vertex);
			transfer_uint_array_data_const(skin.weight_indices, weight_indices);
			transfer_int_array_data_const(skin.joint_indices, joint_indices);
		}

		SkinInfo(UnitConverter *conv) : unit_converter(conv), ob_arm(NULL) {}

		// nobody owns the data after this, so it should be freed manually with releaseMemory
		template <class T>
		void transfer_array_data(T& src, T& dest)
		{
			dest.setData(src.getData(), src.getCount());
			src.yieldOwnerShip();
			dest.yieldOwnerShip();
		}

		// when src is const we cannot src.yieldOwnerShip, this is used by copy constructor
		void transfer_int_array_data_const(const COLLADAFW::IntValuesArray& src, COLLADAFW::IntValuesArray& dest)
		{
			dest.setData((int*)src.getData(), src.getCount());
			dest.yieldOwnerShip();
		}

		void transfer_uint_array_data_const(const COLLADAFW::UIntValuesArray& src, COLLADAFW::UIntValuesArray& dest)
		{
			dest.setData((unsigned int*)src.getData(), src.getCount());
			dest.yieldOwnerShip();
		}

		void borrow_skin_controller_data(const COLLADAFW::SkinControllerData* skin)
		{
			transfer_array_data((COLLADAFW::UIntValuesArray&)skin->getJointsPerVertex(), joints_per_vertex);
			transfer_array_data((COLLADAFW::UIntValuesArray&)skin->getWeightIndices(), weight_indices);
			transfer_array_data((COLLADAFW::IntValuesArray&)skin->getJointIndices(), joint_indices);
			// transfer_array_data(skin->getWeights(), weights);

			// cannot transfer data for FloatOrDoubleArray, copy values manually
			const COLLADAFW::FloatOrDoubleArray& weight = skin->getWeights();
			for (int i = 0; i < weight.getValuesCount(); i++)
				weights.push_back(get_float_value(weight, i));

			unit_converter->mat4_from_dae(bind_shape_matrix, skin->getBindShapeMatrix());
		}
			
		void free()
		{
			joints_per_vertex.releaseMemory();
			weight_indices.releaseMemory();
			joint_indices.releaseMemory();
			// weights.releaseMemory();
		}

		// using inverse bind matrices to construct armature
		// it is safe to invert them to get the original matrices
		// because if they are inverse matrices, they can be inverted
		void add_joint(const COLLADABU::Math::Matrix4& matrix)
		{
			JointData jd;
			unit_converter->mat4_from_dae(jd.inv_bind_mat, matrix);
			joint_data.push_back(jd);
		}

		// called from write_controller
		Object *create_armature(const COLLADAFW::SkinController* co, Scene *scene)
		{
			ob_arm = add_object(scene, OB_ARMATURE);

			controller_uid = co->getUniqueId();

			const COLLADAFW::UniqueIdArray& joint_uids = co->getJoints();
			for (int i = 0; i < joint_uids.getCount(); i++) {
				joint_data[i].joint_uid = joint_uids[i];

				// // store armature pointer
				// JointData& jd = joint_index_to_joint_info_map[i];
				// jd.ob_arm = ob_arm;

				// now we'll be able to get inv bind matrix from joint id
				// joint_id_to_joint_index_map[joint_ids[i]] = i;
			}

			return ob_arm;
		}

		bool get_joint_inv_bind_matrix(float inv_bind_mat[][4], COLLADAFW::Node *node)
		{
			const COLLADAFW::UniqueId& uid = node->getUniqueId();
			std::vector<JointData>::iterator it;
			for (it = joint_data.begin(); it != joint_data.end(); it++) {
				if ((*it).joint_uid == uid) {
					Mat4CpyMat4(inv_bind_mat, (*it).inv_bind_mat);
					return true;
				}
			}

			return false;
		}

		Object *get_armature()
		{
			return ob_arm;
		}

		const COLLADAFW::UniqueId& get_controller_uid()
		{
			return controller_uid;
		}

		// some nodes may not be referenced by SkinController,
		// in this case to determine if the node belongs to this armature,
		// we need to search down the tree
		bool uses_joint(COLLADAFW::Node *node)
		{
			const COLLADAFW::UniqueId& uid = node->getUniqueId();
			std::vector<JointData>::iterator it;
			for (it = joint_data.begin(); it != joint_data.end(); it++) {
				if ((*it).joint_uid == uid)
					return true;
			}

			COLLADAFW::NodePointerArray& children = node->getChildNodes();
			for (int i = 0; i < children.getCount(); i++) {
				if (this->uses_joint(children[i]))
					return true;
			}

			return false;
		}

		void link_armature(bContext *C, Object *ob, std::map<COLLADAFW::UniqueId, COLLADAFW::Node*>& joint_by_uid,
						   TransformReader *tm)
		{
			tm->decompose(bind_shape_matrix, ob->loc, ob->rot, ob->size);

			ob->parent = ob_arm;
			ob->partype = PARSKEL;
			ob->recalc |= OB_RECALC_OB|OB_RECALC_DATA;

			((bArmature*)ob_arm->data)->deformflag = ARM_DEF_VGROUP;

			// we need armature matrix here... where do we get it from I wonder...
			// root node/joint? or node with <instance_controller>?
			float parmat[4][4];
			Mat4One(parmat);
			Mat4Invert(ob->parentinv, parmat);

			// create all vertex groups
			std::vector<JointData>::iterator it;
			int joint_index;
			for (it = joint_data.begin(), joint_index = 0; it != joint_data.end(); it++, joint_index++) {
				const char *name = "Group";

				// name group by joint node name
				if (joint_by_uid.find((*it).joint_uid) != joint_by_uid.end()) {
					name = get_joint_name(joint_by_uid[(*it).joint_uid]);
				}

				ED_vgroup_add_name(ob, (char*)name);
			}

			// <vcount> - number of joints per vertex - joints_per_vertex
			// <v> - [[bone index, weight index] * joints per vertex] * vertices - weight indices
			// ^ bone index can be -1 meaning weight toward bind shape, how to express this in Blender?

			// for each vertex in weight indices
			//   for each bone index in vertex
			//     add vertex to group at group index
			//     treat group index -1 specially

			// get def group by index with BLI_findlink

			for (int vertex = 0, weight = 0; vertex < joints_per_vertex.getCount(); vertex++) {

				int limit = weight + joints_per_vertex[vertex];
				for ( ; weight < limit; weight++) {
					int joint = joint_indices[weight], joint_weight = weight_indices[weight];

					// -1 means "weight towards the bind shape", we just don't assign it to any group
					if (joint != -1) {
						bDeformGroup *def = (bDeformGroup*)BLI_findlink(&ob->defbase, joint);

						ED_vgroup_vert_add(ob, def, vertex, weights[joint_weight], WEIGHT_REPLACE);
					}
				}
			}

			DAG_scene_sort(CTX_data_scene(C));
			ED_anim_dag_flush_update(C);
			WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
		}

		bPoseChannel *get_pose_channel_from_node(COLLADAFW::Node *node)
		{
			return get_pose_channel(ob_arm->pose, get_joint_name(node));
		}
	};

	std::map<COLLADAFW::UniqueId, SkinInfo> skin_by_data_uid; // data UID = skin controller data UID
#if 0
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
#endif

	void create_bone(SkinInfo& skin, COLLADAFW::Node *node, EditBone *parent, int totchild,
					 float parent_mat[][4], bArmature *arm)
	{
		float joint_inv_bind_mat[4][4];

		// JointData* jd = get_joint_data(node);

		float mat[4][4];

		if (skin.get_joint_inv_bind_matrix(joint_inv_bind_mat, node)) {
			// get original world-space matrix
			Mat4Invert(mat, joint_inv_bind_mat);
		}
		// create a bone even if there's no joint data for it (i.e. it has no influence)
		else {
			float obmat[4][4];

			// object-space
			get_node_mat(obmat, node, NULL, NULL);

			// get world-space
			if (parent)
				Mat4MulMat4(mat, obmat, parent_mat);
			else
				Mat4CpyMat4(mat, obmat);
		}

		// TODO rename from Node "name" attrs later
		EditBone *bone = addEditBone(arm, (char*)get_joint_name(node));
		totbone++;

		if (parent) bone->parent = parent;

		// set head
		VecCopyf(bone->head, mat[3]);

		// set tail, don't set it to head because 0-length bones are not allowed
		float vec[3] = {0.0f, 0.5f, 0.0f};
		VecAddf(bone->tail, bone->head, vec);

		// set parent tail
		if (parent && totchild == 1) {
			VecCopyf(parent->tail, bone->head);

			// XXX increase this to prevent "very" small bones?
			const float epsilon = 0.000001f;

			// derive leaf bone length
			float length = VecLenf(parent->head, parent->tail);
			if ((length < leaf_bone_length || totbone == 0) && length > epsilon) {
				leaf_bone_length = length;
			}

			// treat zero-sized bone like a leaf bone
			if (length <= epsilon) {
				add_leaf_bone(parent_mat, parent);
			}

			/*
#if 0
			// and which row in mat is bone direction
			float vec[3];
			VecSubf(vec, parent->tail, parent->head);
#ifdef COLLADA_DEBUG
			printvecf("tail - head", vec);
			printmatrix4("matrix", parent_mat);
#endif
			for (int i = 0; i < 3; i++) {
#ifdef COLLADA_DEBUG
				char *axis_names[] = {"X", "Y", "Z"};
				printf("%s-axis length is %f\n", axis_names[i], VecLength(parent_mat[i]));
#endif
				float angle = VecAngle2(vec, parent_mat[i]);
				if (angle < min_angle) {
#ifdef COLLADA_DEBUG
					printvecf("picking", parent_mat[i]);
					printf("^ %s axis of %s's matrix\n", axis_names[i], get_dae_name(node));
#endif
					bone_direction_row = i;
					min_angle = angle;
				}
			}
#endif
			*/
		}

		COLLADAFW::NodePointerArray& children = node->getChildNodes();
		for (int i = 0; i < children.getCount(); i++) {
			create_bone(skin, children[i], bone, children.getCount(), mat, arm);
		}

		// in second case it's not a leaf bone, but we handle it the same way
		if (!children.getCount() || children.getCount() > 1) {
			add_leaf_bone(mat, bone);
		}
	}

	void add_leaf_bone(float mat[][4], EditBone *bone)
	{
		LeafBone leaf;

		leaf.bone = bone;
		Mat4CpyMat4(leaf.mat, mat);
		BLI_strncpy(leaf.name, bone->name, sizeof(leaf.name));

		leaf_bones.push_back(leaf);
	}

	void fix_leaf_bones()
	{
		// just setting tail for leaf bones here

		std::vector<LeafBone>::iterator it;
		for (it = leaf_bones.begin(); it != leaf_bones.end(); it++) {
			LeafBone& leaf = *it;

			// pointing up
			float vec[3] = {0.0f, 0.0f, 1.0f};

			VecMulf(vec, leaf_bone_length);

			VecCopyf(leaf.bone->tail, leaf.bone->head);
			VecAddf(leaf.bone->tail, leaf.bone->head, vec);
		}
	}

	void set_leaf_bone_shapes(Object *ob_arm)
	{
		bPose *pose = ob_arm->pose;

		std::vector<LeafBone>::iterator it;
		for (it = leaf_bones.begin(); it != leaf_bones.end(); it++) {
			LeafBone& leaf = *it;

			bPoseChannel *pchan = get_pose_channel(pose, leaf.name);
			if (pchan) {
				pchan->custom = get_empty_for_leaves();
			}
			else {
				fprintf(stderr, "Cannot find a pose channel for leaf bone %s\n", leaf.name);
			}
		}
	}

	void set_euler_rotmode()
	{
		// just set rotmode = ROT_MODE_EUL on pose channel for each joint

		std::map<COLLADAFW::UniqueId, COLLADAFW::Node*>::iterator it;

		for (it = joint_by_uid.begin(); it != joint_by_uid.end(); it++) {

			COLLADAFW::Node *joint = it->second;

			std::map<COLLADAFW::UniqueId, SkinInfo>::iterator sit;
			
			for (sit = skin_by_data_uid.begin(); sit != skin_by_data_uid.end(); sit++) {
				SkinInfo& skin = sit->second;

				if (skin.uses_joint(joint)) {
					bPoseChannel *pchan = skin.get_pose_channel_from_node(joint);

					if (pchan) {
						pchan->rotmode = ROT_MODE_EUL;
					}
					else {
						fprintf(stderr, "Cannot find pose channel for %s.\n", get_joint_name(joint));
					}

					break;
				}
			}
		}
	}

	Object *get_empty_for_leaves()
	{
		if (empty) return empty;
		
		empty = add_object(scene, OB_EMPTY);
		empty->empty_drawtype = OB_EMPTY_SPHERE;

		return empty;
	}

#if 0
	Object *find_armature(COLLADAFW::Node *node)
	{
		JointData* jd = get_joint_data(node);
		if (jd) return jd->ob_arm;

		COLLADAFW::NodePointerArray& children = node->getChildNodes();
		for (int i = 0; i < children.getCount(); i++) {
			Object *ob_arm = find_armature(children[i]);
			if (ob_arm) return ob_arm;
		}

		return NULL;
	}

	ArmatureJoints& get_armature_joints(Object *ob_arm)
	{
		// try finding it
		std::vector<ArmatureJoints>::iterator it;
		for (it = armature_joints.begin(); it != armature_joints.end(); it++) {
			if ((*it).ob_arm == ob_arm) return *it;
		}

		// not found, create one
		ArmatureJoints aj;
		aj.ob_arm = ob_arm;
		armature_joints.push_back(aj);

		return armature_joints.back();
	}
#endif

	void create_armature_bones(SkinInfo& skin)
	{
		// just do like so:
		// - get armature
		// - enter editmode
		// - add edit bones and head/tail properties using matrices and parent-child info
		// - exit edit mode
		// - set a sphere shape to leaf bones

		Object *ob_arm = skin.get_armature();

		// enter armature edit mode
		ED_armature_to_edit(ob_arm);

		leaf_bones.clear();
		totbone = 0;
		// bone_direction_row = 1; // TODO: don't default to Y but use asset and based on it decide on default row
		leaf_bone_length = 0.1f;
		// min_angle = 360.0f;		// minimum angle between bone head-tail and a row of bone matrix

		// create bones

		std::vector<COLLADAFW::Node*>::iterator it;
		for (it = root_joints.begin(); it != root_joints.end(); it++) {
			// since root_joints may contain joints for multiple controllers, we need to filter
			if (skin.uses_joint(*it)) {
				create_bone(skin, *it, NULL, (*it)->getChildNodes().getCount(), NULL, (bArmature*)ob_arm->data);
			}
		}

		fix_leaf_bones();

		// exit armature edit mode
		ED_armature_from_edit(ob_arm);
		ED_armature_edit_free(ob_arm);
		DAG_id_flush_update(&ob_arm->id, OB_RECALC_OB|OB_RECALC_DATA);

		set_leaf_bone_shapes(ob_arm);

		set_euler_rotmode();
	}
	

public:

	ArmatureImporter(UnitConverter *conv, MeshImporterBase *mesh, AnimationImporterBase *anim, Scene *sce) :
		TransformReader(conv), scene(sce), empty(NULL), mesh_importer(mesh), anim_importer(anim) {}

	~ArmatureImporter()
	{
		// free skin controller data if we forget to do this earlier
		std::map<COLLADAFW::UniqueId, SkinInfo>::iterator it;
		for (it = skin_by_data_uid.begin(); it != skin_by_data_uid.end(); it++) {
			it->second.free();
		}
	}

	// root - if this joint is the top joint in hierarchy, if a joint
	// is a child of a node (not joint), root should be true since
	// this is where we build armature bones from
	void add_joint(COLLADAFW::Node *node, bool root)
	{
		joint_by_uid[node->getUniqueId()] = node;
		if (root) root_joints.push_back(node);
	}

#if 0
	void add_root_joint(COLLADAFW::Node *node)
	{
		// root_joints.push_back(node);
		Object *ob_arm = find_armature(node);
		if (ob_arm)	{
			get_armature_joints(ob_arm).root_joints.push_back(node);
		}
#ifdef COLLADA_DEBUG
		else {
			fprintf(stderr, "%s cannot be added to armature.\n", get_joint_name(node));
		}
#endif
	}
#endif

	// here we add bones to armatures, having armatures previously created in write_controller
	void make_armatures(bContext *C)
	{
		std::map<COLLADAFW::UniqueId, SkinInfo>::iterator it;
		for (it = skin_by_data_uid.begin(); it != skin_by_data_uid.end(); it++) {

			SkinInfo& skin = it->second;

			create_armature_bones(skin);

			// link armature with an object
			Object *ob = mesh_importer->get_object_by_geom_uid(*get_geometry_uid(skin.get_controller_uid()));
			if (ob) {
				skin.link_armature(C, ob, joint_by_uid, this);
			}
			else {
				fprintf(stderr, "Cannot find object to link armature with.\n");
			}

			// free memory stolen from SkinControllerData
			skin.free();
		}
	}

#if 0
	// link with meshes, create vertex groups, assign weights
	void link_armature(Object *ob_arm, const COLLADAFW::UniqueId& geom_id, const COLLADAFW::UniqueId& controller_data_id)
	{
		Object *ob = mesh_importer->get_object_by_geom_uid(geom_id);

		if (!ob) {
			fprintf(stderr, "Cannot find object by geometry UID.\n");
			return;
		}

		if (skin_by_data_uid.find(controller_data_id) == skin_by_data_uid.end()) {
			fprintf(stderr, "Cannot find skin info by controller data UID.\n");
			return;
		}

		SkinInfo& skin = skin_by_data_uid[conroller_data_id];

		// create vertex groups
	}
#endif

	bool write_skin_controller_data(const COLLADAFW::SkinControllerData* data)
	{
		// at this stage we get vertex influence info that should go into me->verts and ob->defbase
		// there's no info to which object this should be long so we associate it with skin controller data UID

		// don't forget to call unique_vertexgroup_name before we copy

		// controller data uid -> [armature] -> joint data, 
		// [mesh object]
		// 

		SkinInfo skin(unit_converter);
		skin.borrow_skin_controller_data(data);

		// store join inv bind matrix to use it later in armature construction
		const COLLADAFW::Matrix4Array& inv_bind_mats = data->getInverseBindMatrices();
		for (int i = 0; i < data->getJointsCount(); i++) {
			skin.add_joint(inv_bind_mats[i]);
		}

		skin_by_data_uid[data->getUniqueId()] = skin;

		return true;
	}

	bool write_controller(const COLLADAFW::Controller* controller)
	{
		// - create and store armature object

		const COLLADAFW::UniqueId& skin_id = controller->getUniqueId();

		if (controller->getControllerType() == COLLADAFW::Controller::CONTROLLER_TYPE_SKIN) {

			COLLADAFW::SkinController *co = (COLLADAFW::SkinController*)controller;

			// to find geom id by controller id
			geom_uid_by_controller_uid[skin_id] = co->getSource();

			const COLLADAFW::UniqueId& data_uid = co->getSkinControllerData();
			if (skin_by_data_uid.find(data_uid) == skin_by_data_uid.end()) {
				fprintf(stderr, "Cannot find skin by controller data UID.\n");
				return true;
			}

			Object *ob_arm = skin_by_data_uid[data_uid].create_armature(co, scene);

			armature_objects.push_back(ob_arm);
		}
		// morph controller
		else {
			// shape keys? :)
			fprintf(stderr, "Morph controller is not supported yet.\n");
		}

		return true;
	}

	COLLADAFW::UniqueId *get_geometry_uid(const COLLADAFW::UniqueId& controller_uid)
	{
		if (geom_uid_by_controller_uid.find(controller_uid) == geom_uid_by_controller_uid.end())
			return NULL;

		return &geom_uid_by_controller_uid[controller_uid];
	}

	Object *get_armature_for_joint(COLLADAFW::Node *node)
	{
		std::map<COLLADAFW::UniqueId, SkinInfo>::iterator it;
		for (it = skin_by_data_uid.begin(); it != skin_by_data_uid.end(); it++) {
			SkinInfo& skin = it->second;

			if (skin.uses_joint(node))
				return skin.get_armature();
		}

		return NULL;
	}

	void get_rna_path_for_joint(COLLADAFW::Node *node, char *joint_path, size_t count)
	{
		BLI_snprintf(joint_path, count, "pose.pose_channels[\"%s\"]", get_joint_name(node));
	}
	
	void fix_animation()
	{
		/* Change Euler rotation to Quaternion for bone animation */
		std::vector<Object*>::iterator it;
		for (it = armature_objects.begin(); it != armature_objects.end(); it++) {
			Object *ob = *it;
			if (!ob || !ob->adt || !ob->adt->action) continue;
			anim_importer->change_eul_to_quat(ob, ob->adt->action);
		}
	}
};

class MeshImporter : public MeshImporterBase
{
private:

	Scene *scene;
	ArmatureImporter *armature_importer;

	std::map<COLLADAFW::UniqueId, Mesh*> uid_mesh_map; // geometry unique id-to-mesh map
	std::map<COLLADAFW::UniqueId, Object*> uid_object_map; // geom uid-to-object
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
			switch(mVData->getType()) {
			case COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT:
				{
					COLLADAFW::ArrayPrimitiveType<float>* values = mVData->getFloatValues();
					if (values->getCount()) {
						for (int i = 0; i < values->getCount(); i += 2) {
							fprintf(stderr, "%.1f, %.1f\n", (*values)[i], (*values)[i+1]);
						}
					}
				}
				break;
			case COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE:
				{
					COLLADAFW::ArrayPrimitiveType<double>* values = mVData->getDoubleValues();
					if (values->getCount()) {
						for (int i = 0; i < values->getCount(); i += 2) {
							fprintf(stderr, "%.1f, %.1f\n", (float)(*values)[i], (float)(*values)[i+1]);
						}
					}
				}
				break;
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
					
				}
				break;
			case COLLADAFW::MeshVertexData::DATA_TYPE_DOUBLE:
				{
					COLLADAFW::ArrayPrimitiveType<double>* values = mVData->getDoubleValues();
					if (values->empty()) return;
					uv[0] = (float)(*values)[uv_index[0]];
					uv[1] = (float)(*values)[uv_index[1]];
					
				}
				break;
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
		// fprintf(stderr, "%u, %u, %u \n", indices[0], indices[1], indices[2]);
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
					 COLLADAFW::IndexList& index_list, unsigned int *tris_indices)
	{
		int uv_indices[4][2];

		// per face vertex indices, this means for quad we have 4 indices, not 8
		COLLADAFW::UIntValuesArray& indices = index_list.getIndices();

		// make indices into FloatOrDoubleArray
		for (int i = 0; i < 3; i++) {
			int uv_index = indices[tris_indices[i]];
			uv_indices[i][0] = uv_index * 2;
			uv_indices[i][1] = uv_index * 2 + 1;
		}

		uvs.getUV(uv_set_index, uv_indices[0], mtface->uv[0]);
		uvs.getUV(uv_set_index, uv_indices[1], mtface->uv[1]);
		uvs.getUV(uv_set_index, uv_indices[2], mtface->uv[2]);
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
		/*if (quad) {
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
		}*/
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
					if (count < 3) {
						fprintf(stderr, "Primitive %s in %s has at least one face with vertex count < 3\n",
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
	
	int triangulate(int *indices, int vcount, MVert *verts, std::vector<unsigned int>& tri)
	{
		ListBase dispbase = {NULL, NULL};
		DispList *dl;
		float *vert;
		int i = 0;
		
		dispbase.first = dispbase.last = NULL;
		
		dl = (DispList*)MEM_callocN(sizeof(DispList), "poly disp");
		BLI_addtail(&dispbase, dl);
		dl->type = DL_INDEX3;
		dl->nr = vcount;
		dl->type = DL_POLY;
		dl->parts = 1;
		dl->col = 0;
		dl->verts = vert = (float*)MEM_callocN( sizeof(float) * 3 * vcount, "dl verts");
		dl->index = (int*)MEM_callocN(sizeof(int) * 3 * vcount, "dl index");
		
		for (i = 0; i < vcount; ++i, vert += 3) {
			MVert *mvert = &verts[indices[i]];
			vert[0] = mvert->co[0];
			vert[1] = mvert->co[1];
			vert[2] = mvert->co[2];
			//fprintf(stderr, "%.1f %.1f %.1f \n", mvert->co[0], mvert->co[1], mvert->co[2]);
		}
		
		filldisplist(&dispbase, &dispbase);

		dl = (DispList*)dispbase.first;
		int tottri = dl->parts;
		int *index = dl->index;
		
		for (i = 0; i < tottri * 3; i++, index++) {
			tri.push_back(*index);
		}

		freedisplist(&dispbase);

		return tottri;
	}
	
	int count_new_tris(COLLADAFW::Mesh *mesh, Mesh *me, int new_tris)
	{
		COLLADAFW::MeshPrimitiveArray& prim_arr = mesh->getMeshPrimitives();
		int i, j, k;
		
		for (i = 0; i < prim_arr.getCount(); i++) {
			
			COLLADAFW::MeshPrimitive *mp = prim_arr[i];
			int type = mp->getPrimitiveType();
			size_t prim_totface = mp->getFaceCount();
			unsigned int *indices = mp->getPositionIndices().getData();
			
			if (type == COLLADAFW::MeshPrimitive::POLYLIST ||
				type == COLLADAFW::MeshPrimitive::POLYGONS) {
				
				COLLADAFW::Polygons *mpvc =	(COLLADAFW::Polygons*)mp;
				COLLADAFW::Polygons::VertexCountArray& vcounta = mpvc->getGroupedVerticesVertexCountArray();
				
				for (j = 0; j < prim_totface; j++) {
					
					int vcount = vcounta[j];
					
					if (vcount > 4) {
						// create triangles using PolyFill
						int *temp_indices = (int*)MEM_callocN(sizeof(int) * vcount, "face_index");
						
						for (k = 0; k < vcount; k++) {
							temp_indices[k] = indices[k];
						}
						
						std::vector<unsigned int> tri;
						
						int totri = triangulate(temp_indices, vcount, me->mvert, tri);
						new_tris += totri - 1;
						MEM_freeN(temp_indices);
						indices += vcount;
					}
					else if (vcount == 4 || vcount == 3) {
						indices += vcount;
					}
				}
			}
		}
		return new_tris;
	}
	
	// TODO: import uv set names
	void read_faces(COLLADAFW::Mesh *mesh, Mesh *me, int new_tris)
	{
		int i;
		
		// allocate faces
		me->totface = mesh->getFacesCount() + new_tris;
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
		// uvs.print();
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
			/*
			fprintf(stderr, "Primitive %d:\n", i);
			for (int j = 0; j < totuvset; j++) {
				print_index_list(*index_list_array[j]);
			}
			*/
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
					if (vcount == 3 || vcount == 4) {
						
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
					else {
						// create triangles using PolyFill
						int *temp_indices = (int*)MEM_callocN(sizeof(int) *vcount, "face_index");
						int *temp_uv_indices = (int*)MEM_callocN(sizeof(int) *vcount, "uv_index");
						
						for (k = 0; k < vcount; k++) {
							temp_indices[k] = indices[k];
							temp_uv_indices[k] = index + k;
						}
						
						std::vector<unsigned int> tri;
						
						int totri = triangulate(temp_indices, vcount, me->mvert, tri);
						
						for (k = 0; k < tri.size() / 3; k++) {
							unsigned int tris_indices[3];
							unsigned int uv_indices[3];
							tris_indices[0] = temp_indices[tri[k * 3]];
							tris_indices[1] = temp_indices[tri[k * 3 + 1]];
							tris_indices[2] = temp_indices[tri[k * 3 + 2]];
							uv_indices[0] = temp_uv_indices[tri[k * 3]];
							uv_indices[1] = temp_uv_indices[tri[k * 3 + 1]];
							uv_indices[2] = temp_uv_indices[tri[k * 3 + 2]];
							//fprintf(stderr, "%u %u %u \n", tris_indices[0], tris_indices[1], tris_indices[2]);
							set_face_indices(mface, tris_indices, false);
							
							for (int l = 0; l < totuvset; l++) {
								// get mtface by face index and uv set index
								MTFace *mtface = (MTFace*)CustomData_get_layer_n(&me->fdata, CD_MTFACE, l);
								set_face_uv(&mtface[face_index], uvs, l, *index_list_array[l], uv_indices);
								
							}
							
							mface++;
							face_index++;
							prim.totface++;
						}
						
						index += vcount;
						indices += vcount;
						MEM_freeN(temp_indices);
						MEM_freeN(temp_uv_indices);
					}
				}
			}
			
		   	mat_prim_map[mp->getMaterialId()].push_back(prim);
		}

		geom_uid_mat_mapping_map[mesh->getUniqueId()] = mat_prim_map;
	}

public:

	MeshImporter(ArmatureImporter *arm, Scene *sce) : scene(sce), armature_importer(arm) {}

	virtual Object *get_object_by_geom_uid(const COLLADAFW::UniqueId& geom_uid)
	{
		if (uid_object_map.find(geom_uid) != uid_object_map.end())
			return uid_object_map[geom_uid];
		return NULL;
	}
	
	MTex *assign_textures_to_uvlayer(COLLADAFW::InstanceGeometry::TextureCoordinateBinding &ctexture,
									 Mesh *me, TexIndexTextureArrayMap& texindex_texarray_map,
									 MTex *color_texture)
	{
		
		COLLADAFW::TextureMapId texture_index = ctexture.textureMapId;
		
		char *uvname = CustomData_get_layer_name(&me->fdata, CD_MTFACE, ctexture.setIndex);
		
		if (texindex_texarray_map.find(texture_index) == texindex_texarray_map.end()) {
			
			fprintf(stderr, "Cannot find texture array by texture index.\n");
			return color_texture;
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
		
		// set texture face
		if (*color_texture &&
			strlen((*color_texture)->uvname) &&
			strcmp(layername, (*color_texture)->uvname) != 0) {
			
			texture_face = (MTFace*)CustomData_get_layer_named(&me->fdata, CD_MTFACE,
															   (*color_texture)->uvname);
			strcpy(layername, (*color_texture)->uvname);
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
					// bind texture images to faces
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

		// store object pointer for ArmatureImporter
		uid_object_map[*geom_uid] = ob;
		
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
		
		COLLADAFW::InstanceGeometry::MaterialBindingArray& mat_array = 
			geom->getMaterialBindings();
		
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
		
		int new_tris = 0;
		
		read_vertices(mesh, me);

		new_tris = count_new_tris(mesh, me, new_tris);
		
		read_faces(mesh, me, new_tris);
		
 		mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);

		return true;
	}

};

class AnimationImporter : private TransformReader, public AnimationImporterBase
{
private:

	ArmatureImporter *armature_importer;
	Scene *scene;

	std::map<COLLADAFW::UniqueId, std::vector<FCurve*> > uid_fcurve_map;
	std::map<COLLADAFW::UniqueId, TransformReader::Animation> uid_animated_map;
	std::map<bActionGroup*, std::vector<FCurve*> > fcurves_actionGroup_map;
	
	FCurve *create_fcurve(int array_index, char *rna_path)
	{
		FCurve *fcu = (FCurve*)MEM_callocN(sizeof(FCurve), "FCurve");
		
		fcu->flag = (FCURVE_VISIBLE|FCURVE_AUTO_HANDLES|FCURVE_SELECTED);
		fcu->rna_path = BLI_strdupn(rna_path, strlen(rna_path));
		fcu->array_index = array_index;
		return fcu;
	}
	
	void create_bezt(FCurve *fcu, float frame, float output)
	{
		BezTriple bez;
		memset(&bez, 0, sizeof(BezTriple));
		bez.vec[1][0] = frame;
		bez.vec[1][1] = output;
		bez.ipo = U.ipo_new; /* use default interpolation mode here... */
		bez.f1 = bez.f2 = bez.f3 = SELECT;
		bez.h1 = bez.h2 = HD_AUTO;
		insert_bezt_fcurve(fcu, &bez, 0);
		calchandles_fcurve(fcu);
	}

	void make_fcurves_from_animation(COLLADAFW::AnimationCurve *curve,
									 COLLADAFW::FloatOrDoubleArray& input,
									 COLLADAFW::FloatOrDoubleArray& output,
									 COLLADAFW::FloatOrDoubleArray& intan,
									 COLLADAFW::FloatOrDoubleArray& outtan, size_t dim, float fps)
	{
		int i;
		// char *path = "location";
		std::vector<FCurve*>& fcurves = uid_fcurve_map[curve->getUniqueId()];

		if (dim == 1) {
			// create fcurve
			FCurve *fcu = (FCurve*)MEM_callocN(sizeof(FCurve), "FCurve");

			fcu->flag = (FCURVE_VISIBLE|FCURVE_AUTO_HANDLES|FCURVE_SELECTED);
			// fcu->rna_path = BLI_strdupn(path, strlen(path));
			fcu->array_index = 0;
			//fcu->totvert = curve->getKeyCount();
			
			// create beztriple for each key
			for (i = 0; i < curve->getKeyCount(); i++) {
				BezTriple bez;
				memset(&bez, 0, sizeof(BezTriple));
				// intangent
				bez.vec[0][0] = get_float_value(intan, i + i) * fps;
				bez.vec[0][1] = get_float_value(intan, i + i + 1);
				// input, output
				bez.vec[1][0] = get_float_value(input, i) * fps;
				bez.vec[1][1] = get_float_value(output, i);
				// outtangent
				bez.vec[2][0] = get_float_value(outtan, i + i) * fps;
				bez.vec[2][1] = get_float_value(outtan, i + i + 1);
				
				bez.ipo = U.ipo_new; /* use default interpolation mode here... */
				bez.f1 = bez.f2 = bez.f3 = SELECT;
				bez.h1 = bez.h2 = HD_AUTO;
				insert_bezt_fcurve(fcu, &bez, 0);
				calchandles_fcurve(fcu);
			}

			fcurves.push_back(fcu);
		}
		else if(dim == 3) {
			for (i = 0; i < dim; i++ ) {
				// create fcurve
				FCurve *fcu = (FCurve*)MEM_callocN(sizeof(FCurve), "FCurve");
				
				fcu->flag = (FCURVE_VISIBLE|FCURVE_AUTO_HANDLES|FCURVE_SELECTED);
				// fcu->rna_path = BLI_strdupn(path, strlen(path));
				fcu->array_index = 0;
				//fcu->totvert = curve->getKeyCount();
				
				// create beztriple for each key
				for (int j = 0; j < curve->getKeyCount(); j++) {
					BezTriple bez;
					memset(&bez, 0, sizeof(BezTriple));
					// intangent
					bez.vec[0][0] = get_float_value(intan, j * 6 + i + i) * fps;
					bez.vec[0][1] = get_float_value(intan, j * 6 + i + i + 1);
					// input, output
					bez.vec[1][0] = get_float_value(input, j) * fps; 
					bez.vec[1][1] = get_float_value(output, j * 3 + i);
					// outtangent
					bez.vec[2][0] = get_float_value(outtan, j * 6 + i + i) * fps;
					bez.vec[2][1] = get_float_value(outtan, j * 6 + i + i + 1);

					bez.ipo = U.ipo_new; /* use default interpolation mode here... */
					bez.f1 = bez.f2 = bez.f3 = SELECT;
					bez.h1 = bez.h2 = HD_AUTO;
					insert_bezt_fcurve(fcu, &bez, 0);
					calchandles_fcurve(fcu);
				}

				fcurves.push_back(fcu);
			}
		}
	}
	
	void add_fcurves_to_object(Object *ob, std::vector<FCurve*>& curves, char *rna_path, int array_index, Animation *animated)
	{
		ID *id = &ob->id;
		bAction *act;
		bActionGroup *grp = NULL;
		
		if (!ob->adt || !ob->adt->action) act = verify_adt_action(id, 1);
		else act = verify_adt_action(id, 0);

		if (!ob->adt || !ob->adt->action) {
			fprintf(stderr, "Cannot create anim data or action for this object. \n");
			return;
		}
		
		FCurve *fcu;
		std::vector<FCurve*>::iterator it;
		int i = 0;
		
		for (it = curves.begin(); it != curves.end(); it++) {
			fcu = *it;
			fcu->rna_path = BLI_strdupn(rna_path, strlen(rna_path));
			
			if (array_index == -1) fcu->array_index = i;
			else fcu->array_index = array_index;

			// convert degrees to radians for rotation
			char *p = strstr(rna_path, "rotation");
			if (p && *(p + strlen("rotation")) == '\0') {
				for(int j = 0; j < fcu->totvert; j++) {
					float rot_intan = fcu->bezt[j].vec[0][1];
					float rot_output = fcu->bezt[j].vec[1][1];
					float rot_outtan = fcu->bezt[j].vec[2][1];
				    fcu->bezt[j].vec[0][1] = rot_intan * M_PI / 180.0f;
					fcu->bezt[j].vec[1][1] = rot_output * M_PI / 180.0f;
					fcu->bezt[j].vec[2][1] = rot_outtan * M_PI / 180.0f;
				}
			}
			
			if (ob->type == OB_ARMATURE) {
				bAction *act = ob->adt->action;
				const char *bone_name = get_joint_name(animated->node);
				
				if (bone_name) {
					/* try to find group */
					grp = action_groups_find_named(act, bone_name);
					
					/* no matching groups, so add one */
					if (grp == NULL) {
						/* Add a new group, and make it active */
						grp = (bActionGroup*)MEM_callocN(sizeof(bActionGroup), "bActionGroup");
						
						grp->flag = AGRP_SELECTED;
						BLI_snprintf(grp->name, sizeof(grp->name), bone_name);
						
						BLI_addtail(&act->groups, grp);
						BLI_uniquename(&act->groups, grp, "Group", '.', offsetof(bActionGroup, name), 64);
					}
					
					/* add F-Curve to group */
					action_groups_add_channel(act, grp, fcu);
					
				}
				if (p && *(p + strlen("rotation")) == '\0') {
					fcurves_actionGroup_map[grp].push_back(fcu);
				}
			}
			else {
				BLI_addtail(&act->curves, fcu);
			}

			i++;
		}
	}
public:

	AnimationImporter(UnitConverter *conv, ArmatureImporter *arm, Scene *scene) :
		TransformReader(conv), armature_importer(arm), scene(scene) { }

	bool write_animation( const COLLADAFW::Animation* anim ) 
	{
		float fps = (float)FPS;

		if (anim->getAnimationType() == COLLADAFW::Animation::ANIMATION_CURVE) {
			COLLADAFW::AnimationCurve *curve = (COLLADAFW::AnimationCurve*)anim;
			size_t dim = curve->getOutDimension();
			
			// XXX Don't know if it's necessary
			// Should we check outPhysicalDimension?
			if (curve->getInPhysicalDimension() != COLLADAFW::PHYSICAL_DIMENSION_TIME) {
				fprintf(stderr, "Inputs physical dimension is not time. \n");
				return true;
			}

			COLLADAFW::FloatOrDoubleArray& input = curve->getInputValues();
			COLLADAFW::FloatOrDoubleArray& output = curve->getOutputValues();
			COLLADAFW::FloatOrDoubleArray& intan = curve->getInTangentValues();
			COLLADAFW::FloatOrDoubleArray& outtan = curve->getOutTangentValues();

			// a curve can have mixed interpolation type,
			// in this case curve->getInterpolationTypes returns a list of interpolation types per key
			COLLADAFW::AnimationCurve::InterpolationType interp = curve->getInterpolationType();

			if (interp != COLLADAFW::AnimationCurve::INTERPOLATION_MIXED) {
				switch (interp) {
				case COLLADAFW::AnimationCurve::INTERPOLATION_LINEAR:
					// support this
					make_fcurves_from_animation(curve, input, output, intan, outtan, dim, fps);
					break;
				case COLLADAFW::AnimationCurve::INTERPOLATION_BEZIER:
					// and this
					make_fcurves_from_animation(curve, input, output, intan, outtan, dim, fps);
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
	
	// called on post-process stage after writeVisualScenes
	bool write_animation_list( const COLLADAFW::AnimationList* animationList ) 
	{
		const COLLADAFW::UniqueId& anim_list_id = animationList->getUniqueId();

		// possible in case we cannot interpret some transform
		if (uid_animated_map.find(anim_list_id) == uid_animated_map.end()) {
			return true;
		}

		// for bones rna_path is like: pose.pose_channels["bone-name"].rotation
		
		// what does this AnimationList animate?
		Animation& animated = uid_animated_map[anim_list_id];
		Object *ob = animated.ob;

		char rna_path[100];
		char joint_path[100];
		bool is_joint = false;

		// if ob is NULL, it should be a JOINT
		if (!ob) {
			ob = armature_importer->get_armature_for_joint(animated.node);

			if (!ob) {
				fprintf(stderr, "Cannot find armature for node %s\n", get_joint_name(animated.node));
				return true;
			}

			armature_importer->get_rna_path_for_joint(animated.node, joint_path, sizeof(joint_path));

			is_joint = true;
		}
		
		const COLLADAFW::AnimationList::AnimationBindings& bindings = animationList->getAnimationBindings();

		switch (animated.tm->getTransformationType()) {
		case COLLADAFW::Transformation::TRANSLATE:
			{
				if (is_joint)
					BLI_snprintf(rna_path, sizeof(rna_path), "%s.location", joint_path);
				else
					BLI_strncpy(rna_path, "location", sizeof(rna_path));

				for (int i = 0; i < bindings.getCount(); i++) {
					const COLLADAFW::AnimationList::AnimationBinding& binding = bindings[i];
					COLLADAFW::UniqueId anim_uid = binding.animation;

					if (uid_fcurve_map.find(anim_uid) == uid_fcurve_map.end()) {
						fprintf(stderr, "Cannot find FCurve by animation UID.\n");
						continue;
					}

					std::vector<FCurve*>& fcurves = uid_fcurve_map[anim_uid];
					
					switch (binding.animationClass) {
					case COLLADAFW::AnimationList::POSITION_X:
						add_fcurves_to_object(ob, fcurves, rna_path, 0, &animated);
						break;
					case COLLADAFW::AnimationList::POSITION_Y:
						add_fcurves_to_object(ob, fcurves, rna_path, 1, &animated);
						break;
					case COLLADAFW::AnimationList::POSITION_Z:
						add_fcurves_to_object(ob, fcurves, rna_path, 2, &animated);
						break;
					case COLLADAFW::AnimationList::POSITION_XYZ:
						add_fcurves_to_object(ob, fcurves, rna_path, -1, &animated);
						break;
					default:
						fprintf(stderr, "AnimationClass %d is not supported for TRANSLATE transformation.\n",
								binding.animationClass);
					}
				}
			}
			break;
		case COLLADAFW::Transformation::ROTATE:
			{
				if (is_joint)
					BLI_snprintf(rna_path, sizeof(rna_path), "%s.euler_rotation", joint_path);
				else
					BLI_strncpy(rna_path, "rotation", sizeof(rna_path));

				COLLADAFW::Rotate* rot = (COLLADAFW::Rotate*)animated.tm;
				COLLADABU::Math::Vector3& axis = rot->getRotationAxis();
				
				for (int i = 0; i < bindings.getCount(); i++) {
					const COLLADAFW::AnimationList::AnimationBinding& binding = bindings[i];
					COLLADAFW::UniqueId anim_uid = binding.animation;

					if (uid_fcurve_map.find(anim_uid) == uid_fcurve_map.end()) {
						fprintf(stderr, "Cannot find FCurve by animation UID.\n");
						continue;
					}

					std::vector<FCurve*>& fcurves = uid_fcurve_map[anim_uid];

					switch (binding.animationClass) {
					case COLLADAFW::AnimationList::ANGLE:
						if (COLLADABU::Math::Vector3::UNIT_X == axis) {
							add_fcurves_to_object(ob, fcurves, rna_path, 0, &animated);
						}
						else if (COLLADABU::Math::Vector3::UNIT_Y == axis) {
							add_fcurves_to_object(ob, fcurves, rna_path, 1, &animated);
						}
						else if (COLLADABU::Math::Vector3::UNIT_Z == axis) {
							add_fcurves_to_object(ob, fcurves, rna_path, 2, &animated);
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
				if (is_joint)
					BLI_snprintf(rna_path, sizeof(rna_path), "%s.scale", joint_path);
				else
					BLI_strncpy(rna_path, "scale", sizeof(rna_path));

				// same as for TRANSLATE
				for (int i = 0; i < bindings.getCount(); i++) {
					const COLLADAFW::AnimationList::AnimationBinding& binding = bindings[i];
					COLLADAFW::UniqueId anim_uid = binding.animation;

					if (uid_fcurve_map.find(anim_uid) == uid_fcurve_map.end()) {
						fprintf(stderr, "Cannot find FCurve by animation UID.\n");
						continue;
					}
					
					std::vector<FCurve*>& fcurves = uid_fcurve_map[anim_uid];
					
					switch (binding.animationClass) {
					case COLLADAFW::AnimationList::POSITION_X:
						add_fcurves_to_object(ob, fcurves, rna_path, 0, &animated);
						break;
					case COLLADAFW::AnimationList::POSITION_Y:
						add_fcurves_to_object(ob, fcurves, rna_path, 1, &animated);
						break;
					case COLLADAFW::AnimationList::POSITION_Z:
						add_fcurves_to_object(ob, fcurves, rna_path, 2, &animated);
						break;
					case COLLADAFW::AnimationList::POSITION_XYZ:
						add_fcurves_to_object(ob, fcurves, rna_path, -1, &animated);
						break;
					default:
						fprintf(stderr, "AnimationClass %d is not supported for TRANSLATE transformation.\n",
								binding.animationClass);
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

	void read_node_transform(COLLADAFW::Node *node, Object *ob)
	{
		float mat[4][4];
		TransformReader::get_node_mat(mat, node, &uid_animated_map, ob);
		if (ob)
			TransformReader::decompose(mat, ob->loc, ob->rot, ob->size);
	}
	
	virtual void change_eul_to_quat(Object *ob, bAction *act)
	{
		bActionGroup *grp;
		int i;
		
		for (grp = (bActionGroup*)act->groups.first; grp; grp = grp->next) {

			FCurve *eulcu[3] = {NULL, NULL, NULL};
			
			if (fcurves_actionGroup_map.find(grp) == fcurves_actionGroup_map.end())
				continue;

			std::vector<FCurve*> &rot_fcurves = fcurves_actionGroup_map[grp];
			
			if (rot_fcurves.size() > 3) continue;

			for (i = 0; i < rot_fcurves.size(); i++)
				eulcu[rot_fcurves[i]->array_index] = rot_fcurves[i];

			char joint_path[100];
			char rna_path[100];

			BLI_snprintf(joint_path, sizeof(joint_path), "pose.pose_channels[\"%s\"]", grp->name);
			BLI_snprintf(rna_path, sizeof(rna_path), "%s.rotation", joint_path);

			FCurve *quatcu[4] = {
				create_fcurve(0, rna_path),
				create_fcurve(1, rna_path),
				create_fcurve(2, rna_path),
				create_fcurve(3, rna_path)
			};

			for (i = 0; i < 3; i++) {

				FCurve *cu = eulcu[i];

				if (!cu) continue;

				for (int j = 0; j < cu->totvert; j++) {
					float frame = cu->bezt[j].vec[1][0];

					float eul[3] = {
						eulcu[0] ? evaluate_fcurve(eulcu[0], frame) : 0.0f,
						eulcu[1] ? evaluate_fcurve(eulcu[1], frame) : 0.0f,
						eulcu[2] ? evaluate_fcurve(eulcu[2], frame) : 0.0f
					};

					float quat[4];

					EulToQuat(eul, quat);

					for (int k = 0; k < 4; k++)
						create_bezt(quatcu[k], frame, quat[k]);
				}
			}

			// now replace old Euler curves

			for (i = 0; i < 3; i++) {
				if (!eulcu[i]) continue;

				action_groups_remove_channel(act, eulcu[i]);
				free_fcurve(eulcu[i]);
			}

			get_pose_channel(ob->pose, grp->name)->rotmode = ROT_MODE_QUAT;

			for (i = 0; i < 4; i++)
				action_groups_add_channel(act, grp, quatcu[i]);
		}

		bPoseChannel *pchan;
		for (pchan = (bPoseChannel*)ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			pchan->rotmode = ROT_MODE_QUAT;
		}
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
	AnimationImporter anim_importer;

	std::map<COLLADAFW::UniqueId, Image*> uid_image_map;
	std::map<COLLADAFW::UniqueId, Material*> uid_material_map;
	std::map<COLLADAFW::UniqueId, Material*> uid_effect_map;
	std::map<COLLADAFW::UniqueId, Camera*> uid_camera_map;
	std::map<COLLADAFW::UniqueId, Lamp*> uid_lamp_map;
	std::map<Material*, TexIndexTextureArrayMap> material_texture_mapping_map;
	// animation
	// std::map<COLLADAFW::UniqueId, std::vector<FCurve*> > uid_fcurve_map;
	// Nodes don't share AnimationLists (Arystan)
	// std::map<COLLADAFW::UniqueId, Animation> uid_animated_map; // AnimationList->uniqueId to AnimatedObject map

public:

	/** Constructor. */
	Writer(bContext *C, const char *filename) : mContext(C), mFilename(filename),
												armature_importer(&unit_converter, &mesh_importer, &anim_importer, CTX_data_scene(C)),
												mesh_importer(&armature_importer, CTX_data_scene(C)),
												anim_importer(&unit_converter, &armature_importer, CTX_data_scene(C)) {}

	/** Destructor. */
	~Writer() {}

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
		armature_importer.fix_animation();
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
	Object *create_camera_object(COLLADAFW::InstanceCamera *camera, Object *ob, Scene *sce)
	{
		const COLLADAFW::UniqueId& cam_uid = camera->getInstanciatedObjectId();
		if (uid_camera_map.find(cam_uid) == uid_camera_map.end()) {	
			fprintf(stderr, "Couldn't find camera by UID. \n");
			return NULL;
		}
		ob = add_object(sce, OB_CAMERA);
		Camera *cam = uid_camera_map[cam_uid];
		Camera *old_cam = (Camera*)ob->data;
		old_cam->id.us--;
		ob->data = cam;
		if (old_cam->id.us == 0) free_libblock(&G.main->camera, old_cam);
		return ob;
	}
	
	Object *create_lamp_object(COLLADAFW::InstanceLight *lamp, Object *ob, Scene *sce)
	{
		const COLLADAFW::UniqueId& lamp_uid = lamp->getInstanciatedObjectId();
		if (uid_lamp_map.find(lamp_uid) == uid_lamp_map.end()) {	
			fprintf(stderr, "Couldn't find lamp by UID. \n");
			return NULL;
		}
		ob = add_object(sce, OB_LAMP);
		Lamp *la = uid_lamp_map[lamp_uid];
		Lamp *old_lamp = (Lamp*)ob->data;
		old_lamp->id.us--;
		ob->data = la;
		if (old_lamp->id.us == 0) free_libblock(&G.main->lamp, old_lamp);
		return ob;
	}
	
	void write_node (COLLADAFW::Node *node, COLLADAFW::Node *parent_node, Scene *sce, Object *par)
	{
		Object *ob = NULL;

		if (node->getType() == COLLADAFW::Node::JOINT) {

			if (node->getType() == COLLADAFW::Node::JOINT) {
				armature_importer.add_joint(node, parent_node == NULL || parent_node->getType() != COLLADAFW::Node::JOINT);
			}

		}
		else {
			COLLADAFW::InstanceGeometryPointerArray &geom = node->getInstanceGeometries();
			COLLADAFW::InstanceCameraPointerArray &camera = node->getInstanceCameras();
			COLLADAFW::InstanceLightPointerArray &lamp = node->getInstanceLights();
			COLLADAFW::InstanceControllerPointerArray &controller = node->getInstanceControllers();
			COLLADAFW::InstanceNodePointerArray &inst_node = node->getInstanceNodes();

			// XXX linking object with the first <instance_geometry>, though a node may have more of them...
			// maybe join multiple <instance_...> meshes into 1, and link object with it? not sure...
			// <instance_geometry>
			if (geom.getCount() != 0) {
				ob = mesh_importer.create_mesh_object(node, geom[0], false, uid_material_map,
													  material_texture_mapping_map);
			}
			else if (camera.getCount() != 0) {
				ob = create_camera_object(camera[0], ob, sce);
			}
			else if (lamp.getCount() != 0) {
				ob = create_lamp_object(lamp[0], ob, sce);
			}
			else if (controller.getCount() != 0) {
				COLLADAFW::InstanceController *geom = (COLLADAFW::InstanceController*)controller[0];
				ob = mesh_importer.create_mesh_object(node, geom, true, uid_material_map, material_texture_mapping_map);
			}
			// XXX instance_node is not supported yet
			else if (inst_node.getCount() != 0) {
				return;
			}
			// if node is empty - create empty object
			// XXX empty node may not mean it is empty object, not sure about this
			else {
				ob = add_object(sce, OB_EMPTY);
			}
			
			// check if object is not NULL
			if (!ob) return;
			
			// if par was given make this object child of the previous 
			if (par && ob) {
				Object workob;

				ob->parent = par;

				// doing what 'set parent' operator does
				par->recalc |= OB_RECALC_OB;
				ob->parsubstr[0] = 0;
			
				DAG_scene_sort(sce);
			}
		}

		anim_importer.read_node_transform(node, ob);

		// if node has child nodes write them
		COLLADAFW::NodePointerArray &child_nodes = node->getChildNodes();
		for (int i = 0; i < child_nodes.getCount(); i++) {	
			write_node(child_nodes[i], node, sce, ob);
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

			write_node(node, NULL, sce, NULL);
		}

		armature_importer.make_armatures(mContext);
		
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
	MTex *create_texture(COLLADAFW::EffectCommon *ef, COLLADAFW::Texture &ctex, Material *ma,
						 int i, TexIndexTextureArrayMap &texindex_texarray_map)
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
		// TRANSPARENT
		// color
	// 	if (ef->getOpacity().isColor()) {
// 			// XXX don't know what to do here
// 		}
// 		// texture
// 		else if (ef->getOpacity().isTexture()) {
// 			ctex = ef->getOpacity().getTexture();
// 			if (mtex != NULL) mtex->mapto &= MAP_ALPHA;
// 			else {
// 				mtex = create_texture(ef, ctex, ma, i, texindex_texarray_map);
// 				if (mtex != NULL) mtex->mapto = MAP_ALPHA;
// 			}
// 		}
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
			fprintf(stderr, "Couldn't find <profile_COMMON>.\n");
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
		Camera *cam = NULL;
		std::string cam_id, cam_name;
		
		cam_id = camera->getOriginalId();
		cam_name = camera->getName();
		if (cam_name.size()) cam = (Camera*)add_camera((char*)cam_name.c_str());
		else cam = (Camera*)add_camera((char*)cam_id.c_str());
		
		if (!cam) {
			fprintf(stderr, "Cannot create camera. \n");
			return true;
		}
		cam->clipsta = camera->getNearClippingPlane().getValue();
		cam->clipend = camera->getFarClippingPlane().getValue();
		
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
		Lamp *lamp = NULL;
		std::string la_id, la_name;
		
		la_id = light->getOriginalId();
		la_name = light->getName();
		if (la_name.size()) lamp = (Lamp*)add_lamp((char*)la_name.c_str());
		else lamp = (Lamp*)add_lamp((char*)la_id.c_str());
		
		if (!lamp) {
			fprintf(stderr, "Cannot create lamp. \n");
			return true;
		}
		if (light->getColor().isValid()) {
			COLLADAFW::Color col = light->getColor();
			lamp->r = col.getRed();
			lamp->g = col.getGreen();
			lamp->b = col.getBlue();
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
				lamp->falloff_type = LA_FALLOFF_SLIDERS;
				lamp->att1 = light->getLinearAttenuation().getValue();
				lamp->att2 = light->getQuadraticAttenuation().getValue();
				lamp->spotsize = light->getFallOffAngle().getValue();
				lamp->spotblend = light->getFallOffExponent().getValue();
			}
			break;
		case COLLADAFW::Light::DIRECTIONAL_LIGHT:
			{
				lamp->type = LA_SUN;
			}
			break;
		case COLLADAFW::Light::POINT_LIGHT:
			{
				lamp->type = LA_LOCAL;
				lamp->att1 = light->getLinearAttenuation().getValue();
				lamp->att2 = light->getQuadraticAttenuation().getValue();
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
		return true;
	}
	
	// this function is called only for animations that pass COLLADAFW::validate
	virtual bool writeAnimation( const COLLADAFW::Animation* anim ) 
	{
		return anim_importer.write_animation(anim);
	}
	
	// called on post-process stage after writeVisualScenes
	virtual bool writeAnimationList( const COLLADAFW::AnimationList* animationList ) 
	{
		return anim_importer.write_animation_list(animationList);
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

	virtual bool writeFormulas( const COLLADAFW::Formulas* formulas )
	{
		return true;
	}

	virtual bool writeKinematicsScene( const COLLADAFW::KinematicsScene* kinematicsScene )
	{
		return true;
	}
};

void DocumentImporter::import(bContext *C, const char *filename)
{
	Writer w(C, filename);
	w.write();
}
