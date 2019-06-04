/*
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
 */

/** \file
 * \ingroup collada
 */

/* COLLADABU_ASSERT, may be able to remove later */
#include "COLLADABUPlatform.h"

#include "COLLADAFWGeometry.h"
#include "COLLADAFWMeshPrimitive.h"
#include "COLLADAFWMeshVertexData.h"

#include <set>
#include <string>
extern "C" {
#include "DNA_modifier_types.h"
#include "DNA_customdata_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_constraint_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_armature_types.h"

#include "BLI_math.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_constraint.h"
#include "BKE_key.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "ED_armature.h"
#include "ED_screen.h"
#include "ED_node.h"
#include "ED_object.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h" /* XXX hrm, see if we can do without this */
#include "WM_types.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"
#if 0
#  include "NOD_common.h"
#endif
}

#include "collada_utils.h"
#include "ExportSettings.h"
#include "BlenderContext.h"

float bc_get_float_value(const COLLADAFW::FloatOrDoubleArray &array, unsigned int index)
{
  if (index >= array.getValuesCount()) {
    return 0.0f;
  }

  if (array.getType() == COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT) {
    return array.getFloatValues()->getData()[index];
  }
  else {
    return array.getDoubleValues()->getData()[index];
  }
}

/* copied from /editors/object/object_relations.c */
int bc_test_parent_loop(Object *par, Object *ob)
{
  /* test if 'ob' is a parent somewhere in par's parents */

  if (par == NULL) {
    return 0;
  }
  if (ob == par) {
    return 1;
  }

  return bc_test_parent_loop(par->parent, ob);
}

bool bc_validateConstraints(bConstraint *con)
{
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);

  /* these we can skip completely (invalid constraints...) */
  if (cti == NULL) {
    return false;
  }
  if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) {
    return false;
  }

  /* these constraints can't be evaluated anyway */
  if (cti->evaluate_constraint == NULL) {
    return false;
  }

  /* influence == 0 should be ignored */
  if (con->enforce == 0.0f) {
    return false;
  }

  /* validation passed */
  return true;
}

bool bc_set_parent(Object *ob, Object *par, bContext *C, bool is_parent_space)
{
  Scene *scene = CTX_data_scene(C);
  int partype = PAR_OBJECT;
  const bool xmirror = false;
  const bool keep_transform = false;

  if (par && is_parent_space) {
    mul_m4_m4m4(ob->obmat, par->obmat, ob->obmat);
  }

  bool ok = ED_object_parent_set(NULL, C, scene, ob, par, partype, xmirror, keep_transform, NULL);
  return ok;
}

std::vector<bAction *> bc_getSceneActions(const bContext *C, Object *ob, bool all_actions)
{
  std::vector<bAction *> actions;
  if (all_actions) {
    Main *bmain = CTX_data_main(C);
    ID *id;

    for (id = (ID *)bmain->actions.first; id; id = (ID *)(id->next)) {
      bAction *act = (bAction *)id;
      /* XXX This currently creates too many actions.
       * TODO Need to check if the action is compatible to the given object. */
      actions.push_back(act);
    }
  }
  else {
    bAction *action = bc_getSceneObjectAction(ob);
    actions.push_back(action);
  }

  return actions;
}

std::string bc_get_action_id(std::string action_name,
                             std::string ob_name,
                             std::string channel_type,
                             std::string axis_name,
                             std::string axis_separator)
{
  std::string result = action_name + "_" + channel_type;
  if (ob_name.length() > 0) {
    result = ob_name + "_" + result;
  }
  if (axis_name.length() > 0) {
    result += axis_separator + axis_name;
  }
  return translate_id(result);
}

void bc_update_scene(BlenderContext &blender_context, float ctime)
{
  Main *bmain = blender_context.get_main();
  Scene *scene = blender_context.get_scene();
  Depsgraph *depsgraph = blender_context.get_depsgraph();

  /* See remark in physics_fluid.c lines 395...) */
  // BKE_scene_update_for_newframe(ev_context, bmain, scene, scene->lay);
  BKE_scene_frame_set(scene, ctime);
  ED_update_for_newframe(bmain, depsgraph);
}

Object *bc_add_object(Main *bmain, Scene *scene, ViewLayer *view_layer, int type, const char *name)
{
  Object *ob = BKE_object_add_only_object(bmain, type, name);

  ob->data = BKE_object_obdata_add_from_type(bmain, type, name);
  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

  LayerCollection *layer_collection = BKE_layer_collection_get_active(view_layer);
  BKE_collection_object_add(bmain, layer_collection->collection, ob);

  Base *base = BKE_view_layer_base_find(view_layer, ob);
  /* TODO: is setting active needed? */
  BKE_view_layer_base_select_and_set_active(view_layer, base);

  return ob;
}

Mesh *bc_get_mesh_copy(BlenderContext &blender_context,
                       Object *ob,
                       BC_export_mesh_type export_mesh_type,
                       bool apply_modifiers,
                       bool triangulate)
{
  CustomData_MeshMasks mask = CD_MASK_MESH;
  Mesh *tmpmesh = NULL;
  if (apply_modifiers) {
#if 0 /* Not supported by new system currently... */
    switch (export_mesh_type) {
      case BC_MESH_TYPE_VIEW: {
        dm = mesh_create_derived_view(depsgraph, scene, ob, &mask);
        break;
      }
      case BC_MESH_TYPE_RENDER: {
        dm = mesh_create_derived_render(depsgraph, scene, ob, &mask);
        break;
      }
    }
#else
    Depsgraph *depsgraph = blender_context.get_depsgraph();
    Scene *scene_eval = blender_context.get_evaluated_scene();
    Object *ob_eval = blender_context.get_evaluated_object(ob);
    tmpmesh = mesh_get_eval_final(depsgraph, scene_eval, ob_eval, &mask);
#endif
  }
  else {
    tmpmesh = (Mesh *)ob->data;
  }

  BKE_id_copy_ex(NULL, &tmpmesh->id, (ID **)&tmpmesh, LIB_ID_COPY_LOCALIZE);

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

/* Use bubble sort algorithm for sorting the export set */
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
        current->link = b;
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
      if (!(bone->flag & BONE_NO_DEFORM)) {
        root = bone;
      }
      bone = bone->parent;
    }
    return (aBone == root);
  }
  else {
    return !(aBone->parent);
  }
}

int bc_get_active_UVLayer(Object *ob)
{
  Mesh *me = (Mesh *)ob->data;
  return CustomData_get_active_layer_index(&me->ldata, CD_MLOOPUV);
}

std::string bc_url_encode(std::string data)
{
  /* XXX We probably do not need to do a full encoding.
   * But in case that is necessary,then it can be added here.
   */
  return bc_replace_string(data, "#", "%23");
}

std::string bc_replace_string(std::string data,
                              const std::string &pattern,
                              const std::string &replacement)
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
  for (std::vector<Object *>::iterator it = objects_done->begin(); it != objects_done->end();
       ++it) {
    Object *ob = *it;
    if (ob->parent == NULL) {
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
 */
void bc_rotate_from_reference_quat(float quat_to[4], float quat_from[4], float mat_to[4][4])
{
  float qd[4];
  float matd[4][4];
  float mati[4][4];
  float mat_from[4][4];
  quat_to_mat4(mat_from, quat_from);

  /* Calculate the difference matrix matd between mat_from and mat_to */
  invert_m4_m4(mati, mat_from);
  mul_m4_m4m4(matd, mati, mat_to);

  mat4_to_quat(qd, matd);

  mul_qt_qtqt(quat_to, qd, quat_from); /* rot is the final rotation corresponding to mat_to */
}

void bc_triangulate_mesh(Mesh *me)
{
  bool use_beauty = false;
  bool tag_only = false;

  /* XXX: The triangulation method selection could be offered in the UI. */
  int quad_method = MOD_TRIANGULATE_QUAD_SHORTEDGE;

  const struct BMeshCreateParams bm_create_params = {0};
  BMesh *bm = BM_mesh_create(&bm_mesh_allocsize_default, &bm_create_params);
  BMeshFromMeshParams bm_from_me_params = {0};
  bm_from_me_params.calc_face_normal = true;
  BM_mesh_bm_from_me(bm, me, &bm_from_me_params);
  BM_mesh_triangulate(bm, quad_method, use_beauty, 4, tag_only, NULL, NULL, NULL);

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
    if (child->flag & BONE_CONNECTED) {
      return false;
    }
  }
  return true;
}

EditBone *bc_get_edit_bone(bArmature *armature, char *name)
{
  EditBone *eBone;

  for (eBone = (EditBone *)armature->edbo->first; eBone; eBone = eBone->next) {
    if (STREQ(name, eBone->name)) {
      return eBone;
    }
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

  if (enable) {
    bitfield |= bit;
  }
  else {
    bitfield &= ~bit;
  }

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
  if (result == NULL) {
    result = new BoneExtensionMap();
    extended_bone_maps[key] = result;
  }
  return *result;
}

BoneExtensionManager::~BoneExtensionManager()
{
  std::map<std::string, BoneExtensionMap *>::iterator map_it;
  for (map_it = extended_bone_maps.begin(); map_it != extended_bone_maps.end(); ++map_it) {
    BoneExtensionMap *extended_bones = map_it->second;
    for (BoneExtensionMap::iterator ext_it = extended_bones->begin();
         ext_it != extended_bones->end();
         ++ext_it) {
      if (ext_it->second != NULL) {
        delete ext_it->second;
      }
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

inline bool isInteger(const std::string &s)
{
  if (s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) {
    return false;
  }

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
    if (isInteger(layer)) {
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

    if (pos > 31) {
      fprintf(stderr,
              "Too many layers in Import. Layer %s mapped to Blender layer 31\n",
              layer.c_str());
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
  for (int i = 0; i < 32; i++) {
    if (bit & bitfield) {
      ss << sep << i;
      sep = " ";
    }
    bit = bit << 1;
  }
  return ss.str();
}

int BoneExtended::get_bone_layers()
{
  /* ensure that the bone is in at least one bone layer! */
  return (bone_layers == 0) ? 1 : bone_layers;
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
  if (idgroup == NULL) {
    IDPropertyTemplate val = {0};
    idgroup = IDP_New(IDP_GROUP, &val, "RNA_EditBone ID properties");
    ebone->prop = idgroup;
  }

  IDPropertyTemplate val = {0};
  val.array.len = 16;
  val.array.type = IDP_FLOAT;

  IDProperty *data = IDP_New(IDP_ARRAY, &val, key);
  float *array = (float *)IDP_Array(data);
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      array[4 * i + j] = mat[i][j];
    }
  }

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
  if (ebone->prop == NULL) {
    IDPropertyTemplate val = {0};
    ebone->prop = IDP_New(IDP_GROUP, &val, "RNA_EditBone ID properties");
  }

  IDProperty *pgroup = (IDProperty *)ebone->prop;
  IDPropertyTemplate val = {0};
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
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        mat[i][j] = array[4 * i + j];
      }
    }
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
  if (!enabled) {
    return false;
  }

  return (bc_get_IDProperty(bone, key + "_x") || bc_get_IDProperty(bone, key + "_y") ||
          bc_get_IDProperty(bone, key + "_z"));
}

void bc_enable_fcurves(bAction *act, char *bone_name)
{
  FCurve *fcu;
  char prefix[200];

  if (bone_name) {
    BLI_snprintf(prefix, sizeof(prefix), "pose.bones[\"%s\"]", bone_name);
  }

  for (fcu = (FCurve *)act->curves.first; fcu; fcu = fcu->next) {
    if (bone_name) {
      if (STREQLEN(fcu->rna_path, prefix, strlen(prefix))) {
        fcu->flag &= ~FCURVE_DISABLED;
      }
      else {
        fcu->flag |= FCURVE_DISABLED;
      }
    }
    else {
      fcu->flag &= ~FCURVE_DISABLED;
    }
  }
}

bool bc_bone_matrix_local_get(Object *ob, Bone *bone, Matrix &mat, bool for_opensim)
{

  /* Ok, lets be super cautious and check if the bone exists */
  bPose *pose = ob->pose;
  bPoseChannel *pchan = BKE_pose_channel_find_name(pose, bone->name);
  if (!pchan) {
    return false;
  }

  bAction *action = bc_getSceneObjectAction(ob);
  bPoseChannel *parchan = pchan->parent;

  bc_enable_fcurves(action, bone->name);
  float ipar[4][4];

  if (bone->parent) {
    invert_m4_m4(ipar, parchan->pose_mat);
    mul_m4_m4m4(mat, ipar, pchan->pose_mat);
  }
  else {
    copy_m4_m4(mat, pchan->pose_mat);
  }

  /* OPEN_SIM_COMPATIBILITY
   * AFAIK animation to second life is via BVH, but no
   * reason to not have the collada-animation be correct */
  if (for_opensim) {
    float temp[4][4];
    copy_m4_m4(temp, bone->arm_mat);
    temp[3][0] = temp[3][1] = temp[3][2] = 0.0f;
    invert_m4(temp);

    mul_m4_m4m4(mat, mat, temp);

    if (bone->parent) {
      copy_m4_m4(temp, bone->parent->arm_mat);
      temp[3][0] = temp[3][1] = temp[3][2] = 0.0f;

      mul_m4_m4m4(mat, temp, mat);
    }
  }
  bc_enable_fcurves(action, NULL);
  return true;
}

bool bc_is_animated(BCMatrixSampleMap &values)
{
  static float MIN_DISTANCE = 0.00001;

  if (values.size() < 2) {
    return false; /* need at least 2 entries to be not flat */
  }

  BCMatrixSampleMap::iterator it;
  const BCMatrix *refmat = NULL;
  for (it = values.begin(); it != values.end(); ++it) {
    const BCMatrix *matrix = it->second;

    if (refmat == NULL) {
      refmat = matrix;
      continue;
    }

    if (!matrix->in_range(*refmat, MIN_DISTANCE)) {
      return true;
    }
  }
  return false;
}

bool bc_has_animations(Object *ob)
{
  /* Check for object, light and camera transform animations */
  if ((bc_getSceneObjectAction(ob) && bc_getSceneObjectAction(ob)->curves.first) ||
      (bc_getSceneLightAction(ob) && bc_getSceneLightAction(ob)->curves.first) ||
      (bc_getSceneCameraAction(ob) && bc_getSceneCameraAction(ob)->curves.first)) {
    return true;
  }

  /* Check Material Effect parameter animations. */
  for (int a = 0; a < ob->totcol; a++) {
    Material *ma = give_current_material(ob, a + 1);
    if (!ma) {
      continue;
    }
    if (ma->adt && ma->adt->action && ma->adt->action->curves.first) {
      return true;
    }
  }

  Key *key = BKE_key_from_object(ob);
  if ((key && key->adt && key->adt->action) && key->adt->action->curves.first) {
    return true;
  }

  return false;
}

bool bc_has_animations(Scene *sce, LinkNode *export_set)
{
  LinkNode *node;
  if (export_set) {
    for (node = export_set; node; node = node->next) {
      Object *ob = (Object *)node->link;

      if (bc_has_animations(ob)) {
        return true;
      }
    }
  }
  return false;
}

void bc_add_global_transform(Matrix &to_mat,
                             const Matrix &from_mat,
                             const BCMatrix &global_transform,
                             const bool invert)
{
  copy_m4_m4(to_mat, from_mat);
  bc_add_global_transform(to_mat, global_transform, invert);
}

void bc_add_global_transform(Vector &to_vec,
                             const Vector &from_vec,
                             const BCMatrix &global_transform,
                             const bool invert)
{
  copy_v3_v3(to_vec, from_vec);
  bc_add_global_transform(to_vec, global_transform, invert);
}

void bc_add_global_transform(Matrix &to_mat, const BCMatrix &global_transform, const bool invert)
{
  BCMatrix mat(to_mat);
  mat.add_transform(global_transform, invert);
  mat.get_matrix(to_mat);
}

void bc_add_global_transform(Vector &to_vec, const BCMatrix &global_transform, const bool invert)
{
  Matrix mat;
  Vector from_vec;
  copy_v3_v3(from_vec, to_vec);
  global_transform.get_matrix(mat, false, 6, invert);
  mul_v3_m4v3(to_vec, mat, from_vec);
}

void bc_apply_global_transform(Matrix &to_mat, const BCMatrix &global_transform, const bool invert)
{
  BCMatrix mat(to_mat);
  mat.apply_transform(global_transform, invert);
  mat.get_matrix(to_mat);
}

void bc_apply_global_transform(Vector &to_vec, const BCMatrix &global_transform, const bool invert)
{
  Matrix transform;
  global_transform.get_matrix(transform);
  mul_v3_m4v3(to_vec, transform, to_vec);
}

/**
 * Check if custom information about bind matrix exists and modify the from_mat
 * accordingly.
 *
 * Note: This is old style for Blender <= 2.78 only kept for compatibility
 */
void bc_create_restpose_mat(BCExportSettings &export_settings,
                            Bone *bone,
                            float to_mat[4][4],
                            float from_mat[4][4],
                            bool use_local_space)
{
  float loc[3];
  float rot[3];
  float scale[3];
  static const float V0[3] = {0, 0, 0};

  if (!has_custom_props(bone, export_settings.get_keep_bind_info(), "restpose_loc") &&
      !has_custom_props(bone, export_settings.get_keep_bind_info(), "restpose_rot") &&
      !has_custom_props(bone, export_settings.get_keep_bind_info(), "restpose_scale")) {
    /* No need */
    copy_m4_m4(to_mat, from_mat);
    return;
  }

  bc_decompose(from_mat, loc, rot, NULL, scale);
  loc_eulO_size_to_mat4(to_mat, loc, rot, scale, 6);

  if (export_settings.get_keep_bind_info()) {
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

  if (export_settings.get_keep_bind_info()) {
    if (bc_get_IDProperty(bone, "restpose_rot_x")) {
      rot[0] = DEG2RADF(bc_get_property(bone, "restpose_rot_x", 0));
    }
    if (bc_get_IDProperty(bone, "restpose_rot_y")) {
      rot[1] = DEG2RADF(bc_get_property(bone, "restpose_rot_y", 0));
    }
    if (bc_get_IDProperty(bone, "restpose_rot_z")) {
      rot[2] = DEG2RADF(bc_get_property(bone, "restpose_rot_z", 0));
    }
  }

  if (export_settings.get_keep_bind_info()) {
    bc_get_property_vector(bone, "restpose_scale", scale, scale);
  }

  loc_eulO_size_to_mat4(to_mat, loc, rot, scale, 6);
}

void bc_sanitize_v3(float v[3], int precision)
{
  for (int i = 0; i < 3; i++) {
    double val = (double)v[i];
    val = double_round(val, precision);
    v[i] = (float)val;
  }
}

void bc_sanitize_v3(double v[3], int precision)
{
  for (int i = 0; i < 3; i++) {
    v[i] = double_round(v[i], precision);
  }
}

void bc_copy_m4_farray(float r[4][4], float *a)
{
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      r[i][j] = *a++;
    }
  }
}

void bc_copy_farray_m4(float *r, float a[4][4])
{
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      *r++ = a[i][j];
    }
  }
}

void bc_copy_darray_m4d(double *r, double a[4][4])
{
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      *r++ = a[i][j];
    }
  }
}

void bc_copy_v44_m4d(std::vector<std::vector<double>> &r, double (&a)[4][4])
{
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      r[i][j] = a[i][j];
    }
  }
}

void bc_copy_m4d_v44(double (&r)[4][4], std::vector<std::vector<double>> &a)
{
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      r[i][j] = a[i][j];
    }
  }
}

/**
 * Returns name of Active UV Layer or empty String if no active UV Layer defined
 */
std::string bc_get_active_uvlayer_name(Mesh *me)
{
  int num_layers = CustomData_number_of_layers(&me->ldata, CD_MLOOPUV);
  if (num_layers) {
    char *layer_name = bc_CustomData_get_active_layer_name(&me->ldata, CD_MLOOPUV);
    if (layer_name) {
      return std::string(layer_name);
    }
  }
  return "";
}

/**
 * Returns name of Active UV Layer or empty String if no active UV Layer defined.
 * Assuming the Object is of type MESH
 */
std::string bc_get_active_uvlayer_name(Object *ob)
{
  Mesh *me = (Mesh *)ob->data;
  return bc_get_active_uvlayer_name(me);
}

/**
 * Returns UV Layer name or empty string if layer index is out of range
 */
std::string bc_get_uvlayer_name(Mesh *me, int layer)
{
  int num_layers = CustomData_number_of_layers(&me->ldata, CD_MLOOPUV);
  if (num_layers && layer < num_layers) {
    char *layer_name = bc_CustomData_get_layer_name(&me->ldata, CD_MLOOPUV, layer);
    if (layer_name) {
      return std::string(layer_name);
    }
  }
  return "";
}

std::string bc_find_bonename_in_path(std::string path, std::string probe)
{
  std::string result;
  char *boneName = BLI_str_quoted_substrN(path.c_str(), probe.c_str());
  if (boneName) {
    result = std::string(boneName);
    MEM_freeN(boneName);
  }
  return result;
}

static bNodeTree *prepare_material_nodetree(Material *ma)
{
  if (ma->nodetree == NULL) {
    ma->nodetree = ntreeAddTree(NULL, "Shader Nodetree", "ShaderNodeTree");
    ma->use_nodes = true;
  }
  return ma->nodetree;
}

bNode *bc_add_node(
    bContext *C, bNodeTree *ntree, int node_type, int locx, int locy, std::string label)
{
  bNode *node = nodeAddStaticNode(C, ntree, node_type);
  if (node) {
    if (label.length() > 0) {
      strcpy(node->label, label.c_str());
    }
    node->locx = locx;
    node->locy = locy;
    node->flag |= NODE_SELECT;
  }
  return node;
}

bNode *bc_add_node(bContext *C, bNodeTree *ntree, int node_type, int locx, int locy)
{
  return bc_add_node(C, ntree, node_type, locx, locy, "");
}

#if 0
/* experimental, probably not used */
static bNodeSocket *bc_group_add_input_socket(bNodeTree *ntree,
                                              bNode *to_node,
                                              int to_index,
                                              std::string label)
{
  bNodeSocket *to_socket = (bNodeSocket *)BLI_findlink(&to_node->inputs, to_index);

  //bNodeSocket *socket = ntreeAddSocketInterfaceFromSocket(ntree, to_node, to_socket);
  //return socket;

  bNodeSocket *gsock = ntreeAddSocketInterfaceFromSocket(ntree, to_node, to_socket);
  bNode *inputGroup = ntreeFindType(ntree, NODE_GROUP_INPUT);
  node_group_input_verify(ntree, inputGroup, (ID *)ntree);
  bNodeSocket *newsock = node_group_input_find_socket(inputGroup, gsock->identifier);
  nodeAddLink(ntree, inputGroup, newsock, to_node, to_socket);
  strcpy(newsock->name, label.c_str());
  return newsock;
}

static bNodeSocket *bc_group_add_output_socket(bNodeTree *ntree,
                                               bNode *from_node,
                                               int from_index,
                                               std::string label)
{
  bNodeSocket *from_socket = (bNodeSocket *)BLI_findlink(&from_node->outputs, from_index);

  //bNodeSocket *socket = ntreeAddSocketInterfaceFromSocket(ntree, to_node, to_socket);
  //return socket;

  bNodeSocket *gsock = ntreeAddSocketInterfaceFromSocket(ntree, from_node, from_socket);
  bNode *outputGroup = ntreeFindType(ntree, NODE_GROUP_OUTPUT);
  node_group_output_verify(ntree, outputGroup, (ID *)ntree);
  bNodeSocket *newsock = node_group_output_find_socket(outputGroup, gsock->identifier);
  nodeAddLink(ntree, from_node, from_socket, outputGroup, newsock);
  strcpy(newsock->name, label.c_str());
  return newsock;
}

void bc_make_group(bContext *C, bNodeTree *ntree, std::map<std::string, bNode *> nmap)
{
  bNode *gnode = node_group_make_from_selected(C, ntree, "ShaderNodeGroup", "ShaderNodeTree");
  bNodeTree *gtree = (bNodeTree *)gnode->id;

  bc_group_add_input_socket(gtree, nmap["main"], 0, "Diffuse");
  bc_group_add_input_socket(gtree, nmap["emission"], 0, "Emission");
  bc_group_add_input_socket(gtree, nmap["mix"], 0, "Transparency");
  bc_group_add_input_socket(gtree, nmap["emission"], 1, "Emission");
  bc_group_add_input_socket(gtree, nmap["main"], 4, "Metallic");
  bc_group_add_input_socket(gtree, nmap["main"], 5, "Specular");

  bc_group_add_output_socket(gtree, nmap["mix"], 0, "Shader");
}
#endif

static void bc_node_add_link(
    bNodeTree *ntree, bNode *from_node, int from_index, bNode *to_node, int to_index)
{
  bNodeSocket *from_socket = (bNodeSocket *)BLI_findlink(&from_node->outputs, from_index);
  bNodeSocket *to_socket = (bNodeSocket *)BLI_findlink(&to_node->inputs, to_index);

  nodeAddLink(ntree, from_node, from_socket, to_node, to_socket);
}

void bc_add_default_shader(bContext *C, Material *ma)
{
  bNodeTree *ntree = prepare_material_nodetree(ma);
  std::map<std::string, bNode *> nmap;
#if 0
  nmap["main"] = bc_add_node(C, ntree, SH_NODE_BSDF_PRINCIPLED, -300, 300);
  nmap["emission"] = bc_add_node(C, ntree, SH_NODE_EMISSION, -300, 500, "emission");
  nmap["add"] = bc_add_node(C, ntree, SH_NODE_ADD_SHADER, 100, 400);
  nmap["transparent"] = bc_add_node(C, ntree, SH_NODE_BSDF_TRANSPARENT, 100, 200);
  nmap["mix"] = bc_add_node(C, ntree, SH_NODE_MIX_SHADER, 400, 300, "transparency");
  nmap["out"] = bc_add_node(C, ntree, SH_NODE_OUTPUT_MATERIAL, 600, 300);
  nmap["out"]->flag &= ~NODE_SELECT;

  bc_node_add_link(ntree, nmap["emission"], 0, nmap["add"], 0);
  bc_node_add_link(ntree, nmap["main"], 0, nmap["add"], 1);
  bc_node_add_link(ntree, nmap["add"], 0, nmap["mix"], 1);
  bc_node_add_link(ntree, nmap["transparent"], 0, nmap["mix"], 2);

  bc_node_add_link(ntree, nmap["mix"], 0, nmap["out"], 0);
  /* experimental, probably not used. */
  bc_make_group(C, ntree, nmap);
#else
  nmap["main"] = bc_add_node(C, ntree, SH_NODE_BSDF_PRINCIPLED, 0, 300);
  nmap["out"] = bc_add_node(C, ntree, SH_NODE_OUTPUT_MATERIAL, 300, 300);
  bc_node_add_link(ntree, nmap["main"], 0, nmap["out"], 0);
#endif
}

COLLADASW::ColorOrTexture bc_get_base_color(Material *ma)
{
  bNode *master_shader = bc_get_master_shader(ma);
  if (ma->use_nodes && master_shader) {
    return bc_get_base_color(master_shader);
  }
  else {
    return bc_get_cot(ma->r, ma->g, ma->b, ma->a);
  }
}

COLLADASW::ColorOrTexture bc_get_base_color(bNode *shader)
{
  bNodeSocket *socket = nodeFindSocket(shader, SOCK_IN, "Base Color");
  if (socket) {
    bNodeSocketValueRGBA *dcol = (bNodeSocketValueRGBA *)socket->default_value;
    float *col = dcol->value;
    return bc_get_cot(col[0], col[1], col[2], col[3]);
  }
  else {
    return bc_get_cot(0.8, 0.8, 0.8, 1.0); /* default white */
  }
}

COLLADASW::ColorOrTexture bc_get_emission(Material *ma)
{
  bNode *master_shader = bc_get_master_shader(ma);
  if (ma->use_nodes && master_shader) {
    return bc_get_emission(master_shader);
  }
  else {
    return bc_get_cot(0, 0, 0, 1); /* default black */
  }
}

COLLADASW::ColorOrTexture bc_get_emission(bNode *shader)
{
  bNodeSocket *socket = nodeFindSocket(shader, SOCK_IN, "Emission");
  if (socket) {
    bNodeSocketValueRGBA *dcol = (bNodeSocketValueRGBA *)socket->default_value;
    float *col = dcol->value;
    return bc_get_cot(col[0], col[1], col[2], col[3]);
  }
  else {
    return bc_get_cot(0, 0, 0, 1); /* default black */
  }
}

bool bc_get_reflectivity(bNode *shader, double &reflectivity)
{
  bNodeSocket *socket = nodeFindSocket(shader, SOCK_IN, "Specular");
  if (socket) {
    bNodeSocketValueFloat *ref = (bNodeSocketValueFloat *)socket->default_value;
    reflectivity = (double)ref->value;
    return true;
  }
  return false;
}

double bc_get_alpha(Material *ma)
{
  double alpha = ma->a; /* fallback if no socket found */
  bNode *master_shader = bc_get_master_shader(ma);
  if (ma->use_nodes && master_shader) {
    bc_get_alpha(master_shader, alpha);
  }
  return alpha;
}

bool bc_get_alpha(bNode *shader, double &alpha)
{
  bNodeSocket *socket = nodeFindSocket(shader, SOCK_IN, "Alpha");
  if (socket) {
    bNodeSocketValueFloat *ref = (bNodeSocketValueFloat *)socket->default_value;
    alpha = (double)ref->value;
    return true;
  }
  return false;
}

double bc_get_reflectivity(Material *ma)
{
  double reflectivity = ma->spec; /* fallback if no socket found */
  bNode *master_shader = bc_get_master_shader(ma);
  if (ma->use_nodes && master_shader) {
    bc_get_reflectivity(master_shader, reflectivity);
  }
  return reflectivity;
}

bNode *bc_get_master_shader(Material *ma)
{
  bNodeTree *nodetree = ma->nodetree;
  if (nodetree) {
    for (bNode *node = (bNode *)nodetree->nodes.first; node; node = node->next) {
      if (node->typeinfo->type == SH_NODE_BSDF_PRINCIPLED) {
        return node;
      }
    }
  }
  return NULL;
}

COLLADASW::ColorOrTexture bc_get_cot(float r, float g, float b, float a)
{
  COLLADASW::Color color(r, g, b, a);
  COLLADASW::ColorOrTexture cot(color);
  return cot;
}
