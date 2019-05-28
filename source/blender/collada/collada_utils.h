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

#ifndef __COLLADA_UTILS_H__
#define __COLLADA_UTILS_H__

#include "COLLADAFWMeshPrimitive.h"
#include "COLLADAFWGeometry.h"
#include "COLLADAFWFloatOrDoubleArray.h"
#include "COLLADAFWTypes.h"
#include "COLLADASWEffectProfile.h"
#include "COLLADAFWColorOrTexture.h"

#include <vector>
#include <map>
#include <set>
#include <algorithm>

extern "C" {
#include "DNA_object_types.h"
#include "DNA_anim_types.h"
#include "DNA_constraint_types.h"
#include "DNA_mesh_types.h"
#include "DNA_light_types.h"
#include "DNA_camera_types.h"

#include "DNA_customdata_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "BLI_linklist.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "BKE_main.h"
#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_idprop.h"
#include "BKE_node.h"
}

#include "DEG_depsgraph_query.h"

#include "ImportSettings.h"
#include "ExportSettings.h"
#include "collada_internal.h"
#include "BCSampleData.h"
#include "BlenderContext.h"

constexpr int LIMITTED_PRECISION = 6;

struct Depsgraph;

typedef std::map<COLLADAFW::UniqueId, Image *> UidImageMap;
typedef std::map<std::string, Image *> KeyImageMap;
typedef std::map<COLLADAFW::TextureMapId, std::vector<MTex *>> TexIndexTextureArrayMap;
typedef std::set<Object *> BCObjectSet;

extern void bc_update_scene(BlenderContext &blender_context, float ctime);

/* Action helpers */

std::vector<bAction *> bc_getSceneActions(const bContext *C, Object *ob, bool all_actions);

/* Action helpers */

inline bAction *bc_getSceneObjectAction(Object *ob)
{
  return (ob->adt && ob->adt->action) ? ob->adt->action : NULL;
}

/* Returns Light Action or NULL */
inline bAction *bc_getSceneLightAction(Object *ob)
{
  if (ob->type != OB_LAMP)
    return NULL;

  Light *lamp = (Light *)ob->data;
  return (lamp->adt && lamp->adt->action) ? lamp->adt->action : NULL;
}

/* Return Camera Action or NULL */
inline bAction *bc_getSceneCameraAction(Object *ob)
{
  if (ob->type != OB_CAMERA)
    return NULL;

  Camera *camera = (Camera *)ob->data;
  return (camera->adt && camera->adt->action) ? camera->adt->action : NULL;
}

/* returns material action or NULL */
inline bAction *bc_getSceneMaterialAction(Material *ma)
{
  if (ma == NULL)
    return NULL;

  return (ma->adt && ma->adt->action) ? ma->adt->action : NULL;
}

inline void bc_setSceneObjectAction(bAction *action, Object *ob)
{
  if (ob->adt)
    ob->adt->action = action;
}

std::string bc_get_action_id(std::string action_name,
                             std::string ob_name,
                             std::string channel_type,
                             std::string axis_name,
                             std::string axis_separator = "_");

extern float bc_get_float_value(const COLLADAFW::FloatOrDoubleArray &array, unsigned int index);
extern int bc_test_parent_loop(Object *par, Object *ob);

extern bool bc_validateConstraints(bConstraint *con);

bool bc_set_parent(Object *ob, Object *par, bContext *C, bool is_parent_space = true);
extern Object *bc_add_object(
    Main *bmain, Scene *scene, ViewLayer *view_layer, int type, const char *name);
extern Mesh *bc_get_mesh_copy(BlenderContext &blender_context,
                              Object *ob,
                              BC_export_mesh_type export_mesh_type,
                              bool apply_modifiers,
                              bool triangulate);

extern Object *bc_get_assigned_armature(Object *ob);
extern bool bc_has_object_type(LinkNode *export_set, short obtype);

extern char *bc_CustomData_get_layer_name(const CustomData *data, int type, int n);
extern char *bc_CustomData_get_active_layer_name(const CustomData *data, int type);

extern void bc_bubble_sort_by_Object_name(LinkNode *export_set);
extern bool bc_is_root_bone(Bone *aBone, bool deform_bones_only);
extern int bc_get_active_UVLayer(Object *ob);

std::string bc_find_bonename_in_path(std::string path, std::string probe);

inline std::string bc_string_after(const std::string &s, const char c)
{
  size_t i = s.rfind(c, s.length());
  if (i != std::string::npos) {
    return (s.substr(i + 1, s.length() - i));
  }
  return (s);
}

inline bool bc_startswith(std::string const &value, std::string const &starting)
{
  if (starting.size() > value.size())
    return false;
  return (value.substr(0, starting.size()) == starting);
}

#if 0 /* UNUSED */
inline bool bc_endswith(std::string const &value, std::string const &ending)
{
  if (ending.size() > value.size())
    return false;
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}
#endif

extern std::string bc_replace_string(std::string data,
                                     const std::string &pattern,
                                     const std::string &replacement);
extern std::string bc_url_encode(std::string data);
extern void bc_match_scale(Object *ob, UnitConverter &bc_unit, bool scale_to_scene);
extern void bc_match_scale(std::vector<Object *> *objects_done,
                           UnitConverter &unit_converter,
                           bool scale_to_scene);

extern void bc_decompose(float mat[4][4], float *loc, float eul[3], float quat[4], float *size);
extern void bc_rotate_from_reference_quat(float quat_to[4],
                                          float quat_from[4],
                                          float mat_to[4][4]);

extern void bc_triangulate_mesh(Mesh *me);
extern bool bc_is_leaf_bone(Bone *bone);
extern EditBone *bc_get_edit_bone(bArmature *armature, char *name);
extern int bc_set_layer(int bitfield, int layer, bool enable);
extern int bc_set_layer(int bitfield, int layer);

inline bool bc_in_range(float a, float b, float range)
{
  return fabsf(a - b) < range;
}
void bc_copy_m4_farray(float r[4][4], float *a);
void bc_copy_farray_m4(float *r, float a[4][4]);
void bc_copy_darray_m4d(double *r, double a[4][4]);
void bc_copy_m4d_v44(double (&r)[4][4], std::vector<std::vector<double>> &a);
void bc_copy_v44_m4d(std::vector<std::vector<double>> &a, double (&r)[4][4]);

void bc_sanitize_mat(float mat[4][4], int precision);
void bc_sanitize_mat(double mat[4][4], int precision);
void bc_sanitize_v3(double v[3], int precision);
void bc_sanitize_v3(float v[3], int precision);

extern IDProperty *bc_get_IDProperty(Bone *bone, std::string key);
extern void bc_set_IDProperty(EditBone *ebone, const char *key, float value);
extern void bc_set_IDPropertyMatrix(EditBone *ebone, const char *key, float mat[4][4]);

extern float bc_get_property(Bone *bone, std::string key, float def);
extern void bc_get_property_vector(Bone *bone, std::string key, float val[3], const float def[3]);
extern bool bc_get_property_matrix(Bone *bone, std::string key, float mat[4][4]);

extern void bc_enable_fcurves(bAction *act, char *bone_name);
extern bool bc_bone_matrix_local_get(Object *ob, Bone *bone, Matrix &mat, bool for_opensim);
extern bool bc_is_animated(BCMatrixSampleMap &values);
extern bool bc_has_animations(Scene *sce, LinkNode *node);
extern bool bc_has_animations(Object *ob);

extern void bc_add_global_transform(Matrix &to_mat,
                                    const Matrix &from_mat,
                                    const BCMatrix &global_transform,
                                    const bool invert = false);
extern void bc_add_global_transform(Vector &to_vec,
                                    const Vector &from_vec,
                                    const BCMatrix &global_transform,
                                    const bool invert = false);
extern void bc_add_global_transform(Vector &to_vec,
                                    const BCMatrix &global_transform,
                                    const bool invert = false);
extern void bc_add_global_transform(Matrix &to_mat,
                                    const BCMatrix &global_transform,
                                    const bool invert = false);
extern void bc_apply_global_transform(Matrix &to_mat,
                                      const BCMatrix &global_transform,
                                      const bool invert = false);
extern void bc_apply_global_transform(Vector &to_vec,
                                      const BCMatrix &global_transform,
                                      const bool invert = false);
extern void bc_create_restpose_mat(BCExportSettings &export_settings,
                                   Bone *bone,
                                   float to_mat[4][4],
                                   float from_mat[4][4],
                                   bool use_local_space);

class ColladaBaseNodes {
 private:
  std::vector<Object *> base_objects;

 public:
  void add(Object *ob)
  {
    base_objects.push_back(ob);
  }

  bool contains(Object *ob)
  {
    std::vector<Object *>::iterator it = std::find(base_objects.begin(), base_objects.end(), ob);
    return (it != base_objects.end());
  }

  int size()
  {
    return base_objects.size();
  }

  Object *get(int index)
  {
    return base_objects[index];
  }
};

class BCPolygonNormalsIndices {
  std::vector<unsigned int> normal_indices;

 public:
  void add_index(unsigned int index)
  {
    normal_indices.push_back(index);
  }

  unsigned int operator[](unsigned int i)
  {
    return normal_indices[i];
  }
};

class BoneExtended {

 private:
  char name[MAXBONENAME];
  int chain_length;
  bool is_leaf;
  float tail[3];
  float roll;

  int bone_layers;
  int use_connect;
  bool has_custom_tail;
  bool has_custom_roll;

 public:
  BoneExtended(EditBone *aBone);

  void set_name(char *aName);
  char *get_name();

  void set_chain_length(const int aLength);
  int get_chain_length();

  void set_leaf_bone(bool state);
  bool is_leaf_bone();

  void set_bone_layers(std::string layers, std::vector<std::string> &layer_labels);
  int get_bone_layers();
  static std::string get_bone_layers(int bitfield);

  void set_roll(float roll);
  bool has_roll();
  float get_roll();

  void set_tail(float vec[]);
  float *get_tail();
  bool has_tail();

  void set_use_connect(int use_connect);
  int get_use_connect();
};

/* a map to store bone extension maps
 * std:string     : an armature name
 * BoneExtended * : a map that contains extra data for bones
 */
typedef std::map<std::string, BoneExtended *> BoneExtensionMap;

/*
 * A class to organise bone extendion data for multiple Armatures.
 * this is needed for the case where a Collada file contains 2 or more
 * separate armatures.
 */
class BoneExtensionManager {
 private:
  std::map<std::string, BoneExtensionMap *> extended_bone_maps;

 public:
  BoneExtensionMap &getExtensionMap(bArmature *armature);
  ~BoneExtensionManager();
};

void bc_add_default_shader(bContext *C, Material *ma);
bNode *bc_get_master_shader(Material *ma);
COLLADASW::ColorOrTexture bc_get_cot(float r, float g, float b, float a);
COLLADASW::ColorOrTexture bc_get_base_color(bNode *shader);
bool bc_get_reflectivity(bNode *shader, double &reflectivity);
double bc_get_reflectivity(Material *ma);
COLLADASW::ColorOrTexture bc_get_base_color(Material *ma);

#endif
