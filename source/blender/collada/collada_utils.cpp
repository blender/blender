/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Nathan Letwory.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/collada_utils.cpp
 *  \ingroup collada
 */


/* COLLADABU_ASSERT, may be able to remove later */
#include "COLLADABUPlatform.h"

#include "COLLADAFWGeometry.h"
#include "COLLADAFWMeshPrimitive.h"
#include "COLLADAFWMeshVertexData.h"

#include <set>

extern "C" {
#include "DNA_modifier_types.h"
#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_armature_types.h"

#include "BLI_math.h"
#include "BLI_linklist.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_scene.h"
#include "BKE_DerivedMesh.h"
#include "BKE_main.h"

#include "ED_armature.h"

#include "WM_api.h" // XXX hrm, see if we can do without this
#include "WM_types.h"

#include "bmesh.h"
#include "bmesh_tools.h"
}

#include "collada_utils.h"
#include "ExportSettings.h"

float bc_get_float_value(const COLLADAFW::FloatOrDoubleArray& array, unsigned int index)
{
	if (index >= array.getValuesCount())
		return 0.0f;

	if (array.getType() == COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT)
		return array.getFloatValues()->getData()[index];
	else
		return array.getDoubleValues()->getData()[index];
}

// copied from /editors/object/object_relations.c
int bc_test_parent_loop(Object *par, Object *ob)
{
	/* test if 'ob' is a parent somewhere in par's parents */

	if (par == NULL) return 0;
	if (ob == par) return 1;

	return bc_test_parent_loop(par->parent, ob);
}

// a shortened version of parent_set_exec()
// if is_parent_space is true then ob->obmat will be multiplied by par->obmat before parenting
int bc_set_parent(Object *ob, Object *par, bContext *C, bool is_parent_space)
{
	Object workob;
	Scene *sce = CTX_data_scene(C);

	if (!par || bc_test_parent_loop(par, ob))
		return false;

	ob->parent = par;
	ob->partype = PAROBJECT;

	ob->parsubstr[0] = 0;

	if (is_parent_space) {
		float mat[4][4];
		// calc par->obmat
		BKE_object_where_is_calc(sce, par);

		// move child obmat into world space
		mul_m4_m4m4(mat, par->obmat, ob->obmat);
		copy_m4_m4(ob->obmat, mat);
	}

	// apply child obmat (i.e. decompose it into rot/loc/size)
	BKE_object_apply_mat4(ob, ob->obmat, 0, 0);

	// compute parentinv
	BKE_object_workob_calc_parent(sce, ob, &workob);
	invert_m4_m4(ob->parentinv, workob.obmat);

	DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA);
	DAG_id_tag_update(&par->id, OB_RECALC_OB);

	/** done once after import */
#if 0
	DAG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
#endif

	return true;
}

EvaluationContext *bc_get_evaluation_context(Main *bmain)
{
	return bmain->eval_ctx;
}

void bc_update_scene(Main *bmain, Scene *scene, float ctime)
{
	BKE_scene_frame_set(scene, ctime);
	EvaluationContext *ev_context = bc_get_evaluation_context(bmain);
	BKE_scene_update_for_newframe(ev_context, bmain, scene, scene->lay);
}

Object *bc_add_object(Main *bmain, Scene *scene, int type, const char *name)
{
	Object *ob = BKE_object_add_only_object(bmain, type, name);

	ob->data = BKE_object_obdata_add_from_type(bmain, type, name);
	ob->lay = scene->lay;
	DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);

	BKE_scene_base_select(scene, BKE_scene_base_add(scene, ob));

	return ob;
}

Mesh *bc_get_mesh_copy(
        Main *bmain, Scene *scene, Object *ob, BC_export_mesh_type export_mesh_type, bool apply_modifiers, bool triangulate)
{
	Mesh *tmpmesh;
	CustomDataMask mask = CD_MASK_MESH;
	Mesh *mesh = (Mesh *)ob->data;
	DerivedMesh *dm = NULL;
	if (apply_modifiers) {
		switch (export_mesh_type) {
			case BC_MESH_TYPE_VIEW:
			{
				dm = mesh_create_derived_view(scene, ob, mask);
				break;
			}
			case BC_MESH_TYPE_RENDER:
			{
				dm = mesh_create_derived_render(scene, ob, mask);
				break;
			}
		}
	}
	else {
		dm = mesh_create_derived((Mesh *)ob->data, NULL);
	}

	tmpmesh = BKE_mesh_add(bmain, "ColladaMesh"); // name is not important here
	DM_to_mesh(dm, tmpmesh, ob, CD_MASK_MESH, true);
	tmpmesh->flag = mesh->flag;

	if (triangulate) {
		bc_triangulate_mesh(tmpmesh);
	}
	BKE_mesh_tessface_ensure(tmpmesh);
	return tmpmesh;
}

Object *bc_get_assigned_armature(Object *ob)
{
	Object *ob_arm = NULL;

	if (ob->parent && ob->partype == PARSKEL && ob->parent->type == OB_ARMATURE) {
		ob_arm = ob->parent;
	}
	else {
		ModifierData *mod;
		for (mod = (ModifierData *)ob->modifiers.first; mod; mod = mod->next) {
			if (mod->type == eModifierType_Armature) {
				ob_arm = ((ArmatureModifierData *)mod)->object;
			}
		}
	}

	return ob_arm;
}

// Returns the highest selected ancestor
// returns NULL if no ancestor is selected
// IMPORTANT: This function expects that
// all exported objects have set:
// ob->id.tag & LIB_TAG_DOIT
Object *bc_get_highest_selected_ancestor_or_self(LinkNode *export_set, Object *ob)
{
	Object *ancestor = ob;
	while (ob->parent && bc_is_marked(ob->parent)) {
		ob = ob->parent;
		ancestor = ob;
	}
	return ancestor;
}


bool bc_is_base_node(LinkNode *export_set, Object *ob)
{
	Object *root = bc_get_highest_selected_ancestor_or_self(export_set, ob);
	return (root == ob);
}

bool bc_is_in_Export_set(LinkNode *export_set, Object *ob)
{
	return (BLI_linklist_index(export_set, ob) != -1);
}

bool bc_has_object_type(LinkNode *export_set, short obtype)
{
	LinkNode *node;

	for (node = export_set; node; node = node->next) {
		Object *ob = (Object *)node->link;
		/* XXX - why is this checking for ob->data? - we could be looking for empties */
		if (ob->type == obtype && ob->data) {
			return true;
		}
	}
	return false;
}

int bc_is_marked(Object *ob)
{
	return ob && (ob->id.tag & LIB_TAG_DOIT);
}

void bc_remove_mark(Object *ob)
{
	ob->id.tag &= ~LIB_TAG_DOIT;
}

void bc_set_mark(Object *ob)
{
	ob->id.tag |= LIB_TAG_DOIT;
}

// Use bubble sort algorithm for sorting the export set
void bc_bubble_sort_by_Object_name(LinkNode *export_set)
{
	bool sorted = false;
	LinkNode *node;
	for (node = export_set; node->next && !sorted; node = node->next) {

		sorted = true;

		LinkNode *current;
		for (current = export_set; current->next; current = current->next) {
			Object *a = (Object *)current->link;
			Object *b = (Object *)current->next->link;

			if (strcmp(a->id.name, b->id.name) > 0) {
				current->link       = b;
				current->next->link = a;
				sorted = false;
			}

		}
	}
}

/* Check if a bone is the top most exportable bone in the bone hierarchy.
 * When deform_bones_only == false, then only bones with NO parent
 * can be root bones. Otherwise the top most deform bones in the hierarchy
 * are root bones.
 */
bool bc_is_root_bone(Bone *aBone, bool deform_bones_only)
{
	if (deform_bones_only) {
		Bone *root = NULL;
		Bone *bone = aBone;
		while (bone) {
			if (!(bone->flag & BONE_NO_DEFORM))
				root = bone;
			bone = bone->parent;
		}
		return (aBone == root);
	}
	else
		return !(aBone->parent);
}

int bc_get_active_UVLayer(Object *ob)
{
	Mesh *me = (Mesh *)ob->data;
	return CustomData_get_active_layer_index(&me->fdata, CD_MTFACE);
}

std::string bc_url_encode(std::string data)
{
	/* XXX We probably do not need to do a full encoding.
	 * But in case that is necessary,then it can be added here.
	 */
	return bc_replace_string(data,"#", "%23");
}

std::string bc_replace_string(std::string data, const std::string& pattern,
                              const std::string& replacement)
{
	size_t pos = 0;
	while ((pos = data.find(pattern, pos)) != std::string::npos) {
		data.replace(pos, pattern.length(), replacement);
		pos += replacement.length();
	}
	return data;
}

/**
 * Calculate a rescale factor such that the imported scene's scale
 * is preserved. I.e. 1 meter in the import will also be
 * 1 meter in the current scene.
 */

void bc_match_scale(Object *ob, UnitConverter &bc_unit, bool scale_to_scene)
{
	if (scale_to_scene) {
		mul_m4_m4m4(ob->obmat, bc_unit.get_scale(), ob->obmat);
	}
	mul_m4_m4m4(ob->obmat, bc_unit.get_rotation(), ob->obmat);
	BKE_object_apply_mat4(ob, ob->obmat, 0, 0);
}

void bc_match_scale(std::vector<Object *> *objects_done,
	                UnitConverter &bc_unit,
	                bool scale_to_scene)
{
	for (std::vector<Object *>::iterator it = objects_done->begin();
			it != objects_done->end();
			++it)
	{
		Object *ob = *it;
		if (ob -> parent == NULL) {
			bc_match_scale(*it, bc_unit, scale_to_scene);
		}
	}
}

/*
 * Convenience function to get only the needed components of a matrix
 */
void bc_decompose(float mat[4][4], float *loc, float eul[3], float quat[4], float *size)
{
	if (size) {
		mat4_to_size(size, mat);
	}

	if (eul) {
		mat4_to_eul(eul, mat);
	}

	if (quat) {
		mat4_to_quat(quat, mat);
	}

	if (loc) {
		copy_v3_v3(loc, mat[3]);
	}
}

/*
 * Create rotation_quaternion from a delta rotation and a reference quat
 *
 * Input:
 * mat_from: The rotation matrix before rotation
 * mat_to  : The rotation matrix after rotation
 * qref    : the quat corresponding to mat_from
 *
 * Output:
 * rot     : the calculated result (quaternion)
 *
 */
void bc_rotate_from_reference_quat(float quat_to[4], float quat_from[4], float mat_to[4][4])
{
	float qd[4];
	float matd[4][4];
	float mati[4][4];
	float mat_from[4][4];
	quat_to_mat4(mat_from, quat_from);

	// Calculate the difference matrix matd between mat_from and mat_to
	invert_m4_m4(mati, mat_from);
	mul_m4_m4m4(matd, mati, mat_to);

	mat4_to_quat(qd, matd);

	mul_qt_qtqt(quat_to, qd, quat_from); // rot is the final rotation corresponding to mat_to
}

void bc_triangulate_mesh(Mesh *me)
{
	bool use_beauty  = false;
	bool tag_only    = false;
	int  quad_method = MOD_TRIANGULATE_QUAD_SHORTEDGE; /* XXX: The triangulation method selection could be offered in the UI */

	const struct BMeshCreateParams bm_create_params = {0};
	BMesh *bm = BM_mesh_create(
	        &bm_mesh_allocsize_default,
	        &bm_create_params);
	BMeshFromMeshParams bm_from_me_params = {0};
	bm_from_me_params.calc_face_normal = true;
	BM_mesh_bm_from_me(bm, me, &bm_from_me_params);
	BM_mesh_triangulate(bm, quad_method, use_beauty, tag_only, NULL, NULL, NULL);

	BMeshToMeshParams bm_to_me_params = {0};
	bm_to_me_params.calc_object_remap = false;
	BM_mesh_bm_to_me(NULL, bm, me, &bm_to_me_params);
	BM_mesh_free(bm);
}

/*
 * A bone is a leaf when it has no children or all children are not connected.
 */
bool bc_is_leaf_bone(Bone *bone)
{
	for (Bone *child = (Bone *)bone->childbase.first; child; child = child->next) {
		if (child->flag & BONE_CONNECTED)
			return false;
	}
	return true;
}

EditBone *bc_get_edit_bone(bArmature * armature, char *name) {
	EditBone  *eBone;

	for (eBone = (EditBone *)armature->edbo->first; eBone; eBone = eBone->next) {
		if (STREQ(name, eBone->name))
			return eBone;
	}

	return NULL;

}
int bc_set_layer(int bitfield, int layer)
{
	return bc_set_layer(bitfield, layer, true); /* enable */
}

int bc_set_layer(int bitfield, int layer, bool enable)
{
	int bit = 1u << layer;

	if (enable)
		bitfield |= bit;
	else
		bitfield &= ~bit;

	return bitfield;
}

/**
 * This method creates a new extension map when needed.
 * \note The ~BoneExtensionManager destructor takes care
 * to delete the created maps when the manager is removed.
 */
BoneExtensionMap &BoneExtensionManager::getExtensionMap(bArmature *armature)
{
	std::string key = armature->id.name;
	BoneExtensionMap *result = extended_bone_maps[key];
	if (result == NULL)
	{
		result = new BoneExtensionMap();
		extended_bone_maps[key] = result;
	}
	return *result;
}

BoneExtensionManager::~BoneExtensionManager()
{
	std::map<std::string, BoneExtensionMap *>::iterator map_it;
	for (map_it = extended_bone_maps.begin(); map_it != extended_bone_maps.end(); ++map_it)
	{
		BoneExtensionMap *extended_bones = map_it->second;
		for (BoneExtensionMap::iterator ext_it = extended_bones->begin(); ext_it != extended_bones->end(); ++ext_it) {
			if (ext_it->second != NULL)
				delete ext_it->second;
		}
		extended_bones->clear();
		delete extended_bones;
	}
}

/**
 * BoneExtended is a helper class needed for the Bone chain finder
 * See ArmatureImporter::fix_leaf_bones()
 * and ArmatureImporter::connect_bone_chains()
 */

BoneExtended::BoneExtended(EditBone *aBone)
{
	this->set_name(aBone->name);
	this->chain_length = 0;
	this->is_leaf = false;
	this->tail[0] = 0.0f;
	this->tail[1] = 0.5f;
	this->tail[2] = 0.0f;
	this->use_connect = -1;
	this->roll = 0;
	this->bone_layers = 0;

	this->has_custom_tail = false;
	this->has_custom_roll = false;
}

char *BoneExtended::get_name()
{
	return name;
}

void BoneExtended::set_name(char *aName)
{
	BLI_strncpy(name, aName, MAXBONENAME);
}

int BoneExtended::get_chain_length()
{
	return chain_length;
}

void BoneExtended::set_chain_length(const int aLength)
{
	chain_length = aLength;
}

void BoneExtended::set_leaf_bone(bool state)
{
	is_leaf = state;
}

bool BoneExtended::is_leaf_bone()
{
	return is_leaf;
}

void BoneExtended::set_roll(float roll)
{
	this->roll = roll;
	this->has_custom_roll = true;
}

bool BoneExtended::has_roll()
{
	return this->has_custom_roll;
}

float BoneExtended::get_roll()
{
	return this->roll;
}

void BoneExtended::set_tail(float vec[])
{
	this->tail[0] = vec[0];
	this->tail[1] = vec[1];
	this->tail[2] = vec[2];
	this->has_custom_tail = true;
}

bool BoneExtended::has_tail()
{
	return this->has_custom_tail;
}

float *BoneExtended::get_tail()
{
	return this->tail;
}

inline bool isInteger(const std::string & s)
{
	if (s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false;

	char *p;
	strtol(s.c_str(), &p, 10);

	return (*p == 0);
}

void BoneExtended::set_bone_layers(std::string layerString, std::vector<std::string> &layer_labels)
{
	std::stringstream ss(layerString);
	std::string layer;
	int pos;

	while (ss >> layer) {

		/* Blender uses numbers to specify layers*/
		if (isInteger(layer))
		{
			pos = atoi(layer.c_str());
			if (pos >= 0 && pos < 32) {
				this->bone_layers = bc_set_layer(this->bone_layers, pos);
				continue;
			}
		}

		/* layer uses labels (not supported by blender). Map to layer numbers:*/
		pos = find(layer_labels.begin(), layer_labels.end(), layer) - layer_labels.begin();
		if (pos >= layer_labels.size()) {
			layer_labels.push_back(layer); /* remember layer number for future usage*/
		}

		if (pos > 31)
		{
			fprintf(stderr, "Too many layers in Import. Layer %s mapped to Blender layer 31\n", layer.c_str());
			pos = 31;
		}

		/* If numeric layers and labeled layers are used in parallel (unlikely),
		 * we get a potential mixup. Just leave as is for now.
		 */
		this->bone_layers = bc_set_layer(this->bone_layers, pos);

	}
}

std::string BoneExtended::get_bone_layers(int bitfield)
{
	std::string result = "";
	std::string sep = "";
	int bit = 1u;

	std::ostringstream ss;
	for (int i = 0; i < 32; i++)
	{
		if (bit & bitfield)
		{
			ss << sep << i;
			sep = " ";
		}
		bit = bit << 1;
	}
	return ss.str();
}

int BoneExtended::get_bone_layers()
{
	return (bone_layers == 0) ? 1 : bone_layers; // ensure that the bone is in at least one bone layer!
}


void BoneExtended::set_use_connect(int use_connect)
{
	this->use_connect = use_connect;
}

int BoneExtended::get_use_connect()
{
	return this->use_connect;
}

/**
 * Stores a 4*4 matrix as a custom bone property array of size 16
 */
void bc_set_IDPropertyMatrix(EditBone *ebone, const char *key, float mat[4][4])
{
	IDProperty *idgroup = (IDProperty *)ebone->prop;
	if (idgroup == NULL)
	{
		IDPropertyTemplate val = { 0 };
		idgroup = IDP_New(IDP_GROUP, &val, "RNA_EditBone ID properties");
		ebone->prop = idgroup;
	}

	IDPropertyTemplate val = { 0 };
	val.array.len = 16;
	val.array.type = IDP_FLOAT;

	IDProperty *data = IDP_New(IDP_ARRAY, &val, key);
	float *array = (float *)IDP_Array(data);
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			array[4 * i + j] = mat[i][j];

	IDP_AddToGroup(idgroup, data);
}

#if 0
/**
 * Stores a Float value as a custom bone property
 *
 * Note: This function is currently not needed. Keep for future usage
 */
static void bc_set_IDProperty(EditBone *ebone, const char *key, float value)
{
	if (ebone->prop == NULL)
	{
		IDPropertyTemplate val = { 0 };
		ebone->prop = IDP_New(IDP_GROUP, &val, "RNA_EditBone ID properties");
	}

	IDProperty *pgroup = (IDProperty *)ebone->prop;
	IDPropertyTemplate val = { 0 };
	IDProperty *prop = IDP_New(IDP_FLOAT, &val, key);
	IDP_Float(prop) = value;
	IDP_AddToGroup(pgroup, prop);

}
#endif

/**
 * Get a custom property when it exists.
 * This function is also used to check if a property exists.
 */
IDProperty *bc_get_IDProperty(Bone *bone, std::string key)
{
	return (bone->prop == NULL) ? NULL : IDP_GetPropertyFromGroup(bone->prop, key.c_str());
}

/**
 * Read a custom bone property and convert to float
 * Return def if the property does not exist.
 */
float bc_get_property(Bone *bone, std::string key, float def)
{
	float result = def;
	IDProperty *property = bc_get_IDProperty(bone, key);
	if (property) {
		switch (property->type) {
			case IDP_INT:
				result = (float)(IDP_Int(property));
				break;
			case IDP_FLOAT:
				result = (float)(IDP_Float(property));
				break;
			case IDP_DOUBLE:
				result = (float)(IDP_Double(property));
				break;
			default:
				result = def;
		}
	}
	return result;
}

/**
 * Read a custom bone property and convert to matrix
 * Return true if conversion was successful
 *
 * Return false if:
 * - the property does not exist
 * - is not an array of size 16
 */
bool bc_get_property_matrix(Bone *bone, std::string key, float mat[4][4])
{
	IDProperty *property = bc_get_IDProperty(bone, key);
	if (property && property->type == IDP_ARRAY && property->len == 16) {
		float *array = (float *)IDP_Array(property);
		for (int i = 0; i < 4; i++)
			for (int j = 0; j < 4; j++)
				mat[i][j] = array[4 * i + j];
		return true;
	}
	return false;
}

/**
 * get a vector that is stored in 3 custom properties (used in Blender <= 2.78)
 */
void bc_get_property_vector(Bone *bone, std::string key, float val[3], const float def[3])
{
	val[0] = bc_get_property(bone, key + "_x", def[0]);
	val[1] = bc_get_property(bone, key + "_y", def[1]);
	val[2] = bc_get_property(bone, key + "_z", def[2]);
}

/**
 * Check if vector exist stored in 3 custom properties (used in Blender <= 2.78)
 */
static bool has_custom_props(Bone *bone, bool enabled, std::string key)
{
	if (!enabled)
		return false;

	return (bc_get_IDProperty(bone, key + "_x")
		||	bc_get_IDProperty(bone, key + "_y")
		||	bc_get_IDProperty(bone, key + "_z"));

}

/**
 * Check if custom information about bind matrix exists and modify the from_mat
 * accordingly.
 *
 * Note: This is old style for Blender <= 2.78 only kept for compatibility
 */
void bc_create_restpose_mat(const ExportSettings *export_settings, Bone *bone, float to_mat[4][4], float from_mat[4][4], bool use_local_space)
{
	float loc[3];
	float rot[3];
	float scale[3];
	static const float V0[3] = { 0, 0, 0 };

	if (!has_custom_props(bone, export_settings->keep_bind_info, "restpose_loc") &&
		!has_custom_props(bone, export_settings->keep_bind_info, "restpose_rot") &&
		!has_custom_props(bone, export_settings->keep_bind_info, "restpose_scale"))
	{
		/* No need */
		copy_m4_m4(to_mat, from_mat);
		return;
	}

	bc_decompose(from_mat, loc, rot, NULL, scale);
	loc_eulO_size_to_mat4(to_mat, loc, rot, scale, 6);

	if (export_settings->keep_bind_info) {
		bc_get_property_vector(bone, "restpose_loc", loc, loc);

		if (use_local_space && bone->parent) {
			Bone *b = bone;
			while (b->parent) {
				b = b->parent;
				float ploc[3];
				bc_get_property_vector(b, "restpose_loc", ploc, V0);
				loc[0] += ploc[0];
				loc[1] += ploc[1];
				loc[2] += ploc[2];
			}
		}
	}

	if (export_settings->keep_bind_info) {
		if (bc_get_IDProperty(bone, "restpose_rot_x"))
		    rot[0] = DEG2RADF(bc_get_property(bone, "restpose_rot_x", 0));
		if (bc_get_IDProperty(bone, "restpose_rot_y"))
			rot[1] = DEG2RADF(bc_get_property(bone, "restpose_rot_y", 0));
		if (bc_get_IDProperty(bone, "restpose_rot_z"))
			rot[2] = DEG2RADF(bc_get_property(bone, "restpose_rot_z", 0));
	}

	if (export_settings->keep_bind_info) {
		bc_get_property_vector(bone, "restpose_scale", scale, scale);
	}

	loc_eulO_size_to_mat4(to_mat, loc, rot, scale, 6);

}

/*
 * Make 4*4 matrices better readable
 */
void bc_sanitize_mat(float mat[4][4], int precision)
{
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			mat[i][j] = double_round(mat[i][j], precision);
}

void bc_sanitize_mat(double mat[4][4], int precision)
{
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			mat[i][j] = double_round(mat[i][j], precision);
}

void bc_copy_m4_farray(float r[4][4], float *a)
{
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			r[i][j] = *a++;
}

void bc_copy_farray_m4(float *r, float a[4][4])
{
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			*r++ = a[i][j];

}

/*
* Returns name of Active UV Layer or empty String if no active UV Layer defined.
* Assuming the Object is of type MESH
*/
std::string bc_get_active_uvlayer_name(Object *ob)
{
	Mesh *me = (Mesh *)ob->data;
	return bc_get_active_uvlayer_name(me);
}

/**
 * Returns name of Active UV Layer or empty String if no active UV Layer defined
 */
std::string bc_get_active_uvlayer_name(Mesh *me)
{
	int num_layers = CustomData_number_of_layers(&me->fdata, CD_MTFACE);
	if (num_layers) {
		char *layer_name = bc_CustomData_get_active_layer_name(&me->fdata, CD_MTFACE);
		if (layer_name) {
			return std::string(layer_name);
		}
	}
	return "";
}

/*
 * Returns UV Layer name or empty string if layer index is out of range
 */
std::string bc_get_uvlayer_name(Mesh *me, int layer)
{
	int num_layers = CustomData_number_of_layers(&me->fdata, CD_MTFACE);
	if (num_layers && layer < num_layers) {
		char *layer_name = bc_CustomData_get_layer_name(&me->fdata, CD_MTFACE, layer);
		if (layer_name) {
			return std::string(layer_name);
		}
	}
	return "";
}

/**********************************************************************
*
* Return the list of Mesh objects with assigned UVtextures and Images
* Note: We need to create artificaial materials for each of them
*
***********************************************************************/
std::set<Object *> bc_getUVTexturedObjects(Scene *sce, bool all_uv_layers)
{
	std::set <Object *> UVObjects;
	Base *base = (Base *)sce->base.first;

	while (base) {
		Object *ob = base->object;
		bool has_uvimage = false;
		if (ob->type == OB_MESH) {
			Mesh *me = (Mesh *)ob->data;
			int active_uv_layer = CustomData_get_active_layer_index(&me->pdata, CD_MTEXPOLY);

			for (int i = 0; i < me->pdata.totlayer && !has_uvimage; i++) {
				if (all_uv_layers || active_uv_layer == i)
				{
					if (me->pdata.layers[i].type == CD_MTEXPOLY) {
						MTexPoly *txface = (MTexPoly *)me->pdata.layers[i].data;
						MPoly *mpoly = me->mpoly;
						for (int j = 0; j < me->totpoly; j++, mpoly++, txface++) {

							Image *ima = txface->tpage;
							if (ima != NULL) {
								has_uvimage = true;
								break;
							}
						}
					}
				}
			}

			if (has_uvimage) {
				UVObjects.insert(ob);
			}
		}
		base = base->next;
	}
	return UVObjects;
}

/**********************************************************************
*
* Return the list of UV Texture images from all exported Mesh Items
* Note: We need to create one artificial material for each Image.
*
***********************************************************************/
std::set<Image *> bc_getUVImages(Scene *sce, bool all_uv_layers)
{
	std::set <Image *> UVImages;
	Base *base = (Base *)sce->base.first;

	while (base) {
		Object *ob = base->object;
		bool has_uvimage = false;
		if (ob->type == OB_MESH) {
			Mesh *me = (Mesh *)ob->data;
			int active_uv_layer = CustomData_get_active_layer_index(&me->pdata, CD_MTEXPOLY);

			for (int i = 0; i < me->pdata.totlayer && !has_uvimage; i++) {
				if (all_uv_layers || active_uv_layer == i)
				{
					if (me->pdata.layers[i].type == CD_MTEXPOLY) {
						MTexPoly *txface = (MTexPoly *)me->pdata.layers[i].data;
						MPoly *mpoly = me->mpoly;
						for (int j = 0; j < me->totpoly; j++, mpoly++, txface++) {

							Image *ima = txface->tpage;
							if (ima != NULL) {
								if (UVImages.find(ima) == UVImages.end())
									UVImages.insert(ima);
							}
						}
					}
				}
			}
		}
		base = base->next;
	}
	return UVImages;
}

/**********************************************************************
*
* Return the list of UV Texture images for the given Object
* Note: We need to create one artificial material for each Image.
*
***********************************************************************/
std::set<Image *> bc_getUVImages(Object *ob, bool all_uv_layers)
{
	std::set <Image *> UVImages;

	bool has_uvimage = false;
	if (ob->type == OB_MESH) {
		Mesh *me = (Mesh *)ob->data;
		int active_uv_layer = CustomData_get_active_layer_index(&me->pdata, CD_MTEXPOLY);

		for (int i = 0; i < me->pdata.totlayer && !has_uvimage; i++) {
			if (all_uv_layers || active_uv_layer == i)
			{
				if (me->pdata.layers[i].type == CD_MTEXPOLY) {
					MTexPoly *txface = (MTexPoly *)me->pdata.layers[i].data;
					MPoly *mpoly = me->mpoly;
					for (int j = 0; j < me->totpoly; j++, mpoly++, txface++) {

						Image *ima = txface->tpage;
						if (ima != NULL) {
							if (UVImages.find(ima) == UVImages.end())
								UVImages.insert(ima);
						}
					}
				}
			}
		}
	}
	return UVImages;
}
