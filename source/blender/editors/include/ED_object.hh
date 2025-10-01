/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include <string>

#include "BLI_compiler_attrs.h"
#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_object_enums.h"
#include "DNA_userdef_enums.h"

struct Base;
struct Depsgraph;
struct EnumPropertyItem;
struct ID;
struct KeyBlock;
struct GpencilModifierData;
struct ListBase;
struct Main;
struct ModifierData;
struct Object;
struct PointerRNA;
struct ReportList;
struct Scene;
struct ShaderFxData;
struct View3D;
struct ViewLayer;
struct bConstraint;
struct bContext;
struct bPoseChannel;
struct uiLayout;
struct wmKeyConfig;
struct wmOperator;
struct wmOperatorType;
enum eReportType : uint16_t;

namespace blender::ed::object {

struct XFormObjectData;

/* `object_edit.cc` */

/** `context.object` */
Object *context_object(const bContext *C);
/**
 * Find the correct active object per context (`context.object` or `context.active_object`)
 * \note context can be NULL when called from a enum with #PROP_ENUM_NO_CONTEXT.
 */
Object *context_active_object(const bContext *C);
void collection_hide_menu_draw(const bContext *C, uiLayout *layout);

/**
 * Return an array of objects:
 * - When in the property space, return the pinned or active object.
 * - When in edit-mode/pose-mode, return an array of objects in the mode.
 * - Otherwise return selected objects,
 *   the callers \a filter_fn needs to check of they are editable
 *   (assuming they need to be modified).
 */
blender::Vector<Object *> objects_in_mode_or_selected(
    bContext *C, bool (*filter_fn)(const Object *ob, void *user_data), void *filter_user_data);

/**
 * Set the active material by index.
 *
 * \param index: A zero based index. This will be clamped to the valid range.
 * \return true if the material index changed.
 */
bool material_active_index_set(Object *ob, int index);

/* `object_shapekey.cc` */

/**
 * Checks if the currently active Edit Mode on the object is targeting a locked shape key,
 * and produces an error message if so (unless \a reports is null).
 * \return true if the shape key was locked.
 */
bool shape_key_report_if_locked(const Object *obedit, ReportList *reports);

/**
 * Checks if the active shape key of the object is locked, and produces an error message
 * if so (unless \a reports is null).
 * \return true if the shape key was locked.
 */
bool shape_key_report_if_active_locked(Object *ob, ReportList *reports);

/**
 * Checks if any of the shape keys of the object are locked, and produces an error message if so
 * (unless \a reports is null).
 * \return true if a shape key was locked.
 */
bool shape_key_report_if_any_locked(Object *ob, ReportList *reports);

/**
 * Return whether this shapekey is considered 'selected'.
 *
 * The active shapekey is always considered 'selected', even though it may not
 * have its selection flag set.
 */
bool shape_key_is_selected(const Object &object, const KeyBlock &kb, int keyblock_index);

void shape_key_mirror(Object *ob, KeyBlock *kb, bool use_topology, int &totmirr, int &totfail);

/* `object_utils.cc` */

bool calc_active_center_for_editmode(Object *obedit, bool select_only, float r_center[3]);
bool calc_active_center_for_posemode(Object *ob, bool select_only, float r_center[3]);
bool calc_active_center(Object *ob, bool select_only, float r_center[3]);

/* Object Data Container helper API. */
struct XFormObjectData_Container;
XFormObjectData_Container *data_xform_container_create();
void data_xform_container_destroy(XFormObjectData_Container *xds);
/**
 * This may be called multiple times with the same data.
 * Each time, the original transformations are re-applied, instead of accumulating the changes.
 */
void data_xform_container_update_all(XFormObjectData_Container *xds,
                                     Main *bmain,
                                     Depsgraph *depsgraph);
void data_xform_container_item_ensure(XFormObjectData_Container *xds, Object *ob);

/** Object Skip-Child Container helper API. */
enum {
  /**
   * The parent is transformed, this is held in place.
   */
  XFORM_OB_SKIP_CHILD_PARENT_IS_XFORM = 1,
  /**
   * The same as #XFORM_OB_SKIP_CHILD_PARENT_IS_XFORM,
   * however this objects parent isn't transformed directly.
   */
  XFORM_OB_SKIP_CHILD_PARENT_IS_XFORM_INDIRECT = 3,
  /**
   * Use the parent invert matrix to apply transformation,
   * this is needed, because breaks in the selection chain prevents this from being transformed.
   * This is used to add the transform which would have been added
   * if there weren't breaks in the parent/child chain.
   */
  XFORM_OB_SKIP_CHILD_PARENT_APPLY = 2,
};
struct XFormObjectSkipChild_Container;
XFormObjectSkipChild_Container *xform_skip_child_container_create();
void xform_skip_child_container_item_ensure_from_array(XFormObjectSkipChild_Container *xcs,
                                                       const Scene *scene,
                                                       ViewLayer *view_layer,
                                                       Object **objects,
                                                       uint objects_len);
void object_xform_skip_child_container_destroy(XFormObjectSkipChild_Container *xcs);
void object_xform_skip_child_container_update_all(XFormObjectSkipChild_Container *xcs,
                                                  Main *bmain,
                                                  Depsgraph *depsgraph);
void object_xform_skip_child_container_item_ensure(XFormObjectSkipChild_Container *xcs,
                                                   Object *ob,
                                                   Object *ob_parent_recurse,
                                                   int mode);

void object_xform_array_m4(Object **objects, uint objects_len, const float matrix[4][4]);

/* `object_ops.cc` */

void operatortypes_object();
void operatormacros_object();
void keymap_object(wmKeyConfig *keyconf);

/* `object_relations.cc` */

enum eParentType {
  PAR_OBJECT,
  PAR_ARMATURE,
  PAR_ARMATURE_NAME,
  PAR_ARMATURE_ENVELOPE,
  PAR_ARMATURE_AUTO,
  PAR_BONE,
  PAR_BONE_RELATIVE,
  PAR_CURVE,
  PAR_FOLLOW,
  PAR_PATH_CONST,
  PAR_LATTICE,
  PAR_VERTEX,
  PAR_VERTEX_TRI,
};

enum eObjectSelect_Mode {
  BA_DESELECT = 0,
  BA_SELECT = 1,
  BA_INVERT = 2,
};

enum eObClearParentTypes {
  CLEAR_PARENT_ALL = 0,
  CLEAR_PARENT_KEEP_TRANSFORM,
  CLEAR_PARENT_INVERSE,
};

#ifdef __RNA_TYPES_H__
/** Operator Property: `OBJECT_OT_parent_clear`. */
extern const EnumPropertyItem prop_clear_parent_types[];
/** Operator Property: `OBJECT_OT_parent_set`. */
extern const EnumPropertyItem prop_make_parent_types[];
#endif

/**
 * Set the object's parent, return true if successful.
 */
bool parent_set(ReportList *reports,
                const bContext *C,
                Scene *scene,
                Object *const ob,
                Object *const par,
                int partype,
                bool xmirror,
                bool keep_transform,
                const int vert_par[3]);
void parent_clear(Object *ob, int type);

/**
 * Simple API for object selection, rather than just using the flag
 * this takes into account the 'restrict selection in 3d view' flag.
 * deselect works always, the restriction just prevents selection
 *
 * \note Caller must send a `NC_SCENE | ND_OB_SELECT` notifier
 * (or a `NC_SCENE | ND_OB_VISIBLE` in case of visibility toggling).
 */
void base_select(Base *base, eObjectSelect_Mode mode);
/**
 * Change active base, it includes the notifier
 */
void base_activate(bContext *C, Base *base);
void base_activate_with_mode_exit_if_needed(bContext *C, Base *base);
/**
 * Call when the active base has changed.
 */
void base_active_refresh(Main *bmain, Scene *scene, ViewLayer *view_layer);
/**
 * Remove base from a specific scene.
 * \note now unlinks constraints as well.
 */
void base_free_and_unlink(Main *bmain, Scene *scene, Object *ob);
/**
 * Remove base from a specific scene.
 * `ob` must not be indirectly used.
 */
void base_free_and_unlink_no_indirect_check(Main *bmain, Scene *scene, Object *ob);
bool base_deselect_all_ex(
    const Scene *scene, ViewLayer *view_layer, View3D *v3d, int action, bool *r_any_visible);
bool base_deselect_all(const Scene *scene, ViewLayer *view_layer, View3D *v3d, int action);

/**
 * Single object duplicate, if `dupflag == 0`, fully linked, else it uses the flags given.
 * Leaves selection of base/object unaltered.
 * \note don't call this within a loop since clear_* functions loop over the entire database.
 * \note caller must do `DAG_relations_tag_update(bmain);`
 * this is not done automatic since we may duplicate many objects in a batch.
 */
Base *add_duplicate(
    Main *bmain, Scene *scene, ViewLayer *view_layer, Base *base, eDupli_ID_Flags dupflag);

void parent_set(Object *ob, Object *parent, int type, const char *substr);
std::string drop_named_material_tooltip(bContext *C, const char *name, const int mval[2]);
std::string drop_geometry_nodes_tooltip(bContext *C, PointerRNA *properties, const int mval[2]);

/** Bit-flags for enter/exit edit-mode. */
enum {
  EM_FREEDATA = (1 << 0),
  EM_NO_CONTEXT = (1 << 1),
};
/**
 * \param flag:
 * - If #EM_FREEDATA isn't in the flag, use #editmode_load directly.
 */
bool editmode_exit_ex(Main *bmain, Scene *scene, Object *obedit, int flag);
bool editmode_exit(bContext *C, int flag);

/**
 * Support freeing edit-mode data without flushing it back to the object.
 *
 * \return true if data was freed.
 */
bool editmode_free_ex(Main *bmain, Object *obedit);

bool editmode_exit_multi_ex(Main *bmain, Scene *scene, ViewLayer *view_layer, int flag);
bool editmode_exit_multi(bContext *C, int flag);

bool editmode_enter_ex(Main *bmain, Scene *scene, Object *ob, int flag);
bool editmode_enter(bContext *C, int flag);
bool editmode_load(Main *bmain, Object *obedit);

void location_from_view(bContext *C, float loc[3]);
void rotation_from_quat(float rot[3], const float quat[4], char align_axis);
void rotation_from_view(bContext *C, float rot[3], char align_axis);
void init_transform_on_add(Object *object, const float loc[3], const float rot[3]);
/**
 * Uses context to figure out transform for primitive.
 * Returns standard diameter.
 */
float new_primitive_matrix(bContext *C,
                           Object *obedit,
                           const float loc[3],
                           const float rot[3],
                           const float scale[3],
                           float primmat[4][4]);

/**
 * Avoid allowing too much insane values even by typing (typos can hang/crash Blender otherwise).
 */
#define OBJECT_ADD_SIZE_MAXF 1.0e12f

void add_unit_props_size(wmOperatorType *ot);
void add_unit_props_radius_ex(wmOperatorType *ot, float default_value);
void add_unit_props_radius(wmOperatorType *ot);
void add_generic_props(wmOperatorType *ot, bool do_editmode);
void add_mesh_props(wmOperatorType *ot);
void add_generic_get_opts(bContext *C,
                          wmOperator *op,
                          char view_align_axis,
                          float r_loc[3],
                          float r_rot[3],
                          float r_scale[3],
                          bool *r_enter_editmode,
                          unsigned short *r_local_view_bits,
                          bool *r_is_view_aligned);

/**
 * For object add primitive operators, or for object creation when `obdata != NULL`.
 * \param obdata: Assigned to #Object.data, with increased user count.
 *
 * \note Do not call undo push in this function (users of this function have to).
 */
Object *add_type_with_obdata(bContext *C,
                             int type,
                             const char *name,
                             const float loc[3],
                             const float rot[3],
                             bool enter_editmode,
                             ushort local_view_bits,
                             ID *obdata);
Object *add_type(bContext *C,
                 int type,
                 const char *name,
                 const float loc[3],
                 const float rot[3],
                 bool enter_editmode,
                 unsigned short local_view_bits) ATTR_NONNULL(1) ATTR_RETURNS_NONNULL;

/**
 * Not an especially efficient function, only added so the single user button can be functional.
 */
void object_single_user_make(Main *bmain, Scene *scene, Object *ob);

void single_obdata_user_make(Main *bmain, Scene *scene, Object *ob);

/* object motion paths */

/**
 * Clear motion paths for all objects.
 */
void motion_paths_clear(bContext *C, bool only_selected);

/* Corresponds to eAnimvizCalcRange. */
enum eObjectPathCalcRange {
  OBJECT_PATH_CALC_RANGE_CURRENT_FRAME,
  OBJECT_PATH_CALC_RANGE_CHANGED,
  OBJECT_PATH_CALC_RANGE_FULL,
};

/**
 * For the objects with animation: update paths for those that have got them
 * This should selectively update paths that exist.
 *
 * To be called from various tools that do incremental updates
 */
void motion_paths_recalc(bContext *C,
                         Scene *scene,
                         eObjectPathCalcRange range,
                         ListBase *ld_objects);

void motion_paths_recalc_selected(bContext *C, Scene *scene, eObjectPathCalcRange range);

void motion_paths_recalc_visible(bContext *C, Scene *scene, eObjectPathCalcRange range);

/* constraints */
/**
 * If object is in pose-mode, return active bone constraints, else object constraints.
 * No constraints are returned for a bone on an inactive bone-layer.
 */
ListBase *constraint_active_list(Object *ob);
/**
 * Get the constraints for the active pose bone. Bone may be on an inactive bone-layer
 * (unlike #constraint_active_list, such constraints are not excluded here).
 */
ListBase *pose_constraint_list(const bContext *C);
/**
 * Find the list that a given constraint belongs to,
 * and/or also get the pose-channel this is from (if applicable).
 */
ListBase *constraint_list_from_constraint(Object *ob, bConstraint *con, bPoseChannel **r_pchan);
/**
 * Single constraint.
 */
bConstraint *constraint_active_get(Object *ob);

void object_test_constraints(Main *bmain, Object *ob);

void constraint_active_set(Object *ob, bConstraint *con);
void constraint_update(Main *bmain, Object *ob);
void constraint_dependency_update(Main *bmain, Object *ob);

void constraint_tag_update(Main *bmain, Object *ob, bConstraint *con);
void constraint_dependency_tag_update(Main *bmain, Object *ob, bConstraint *con);

bool constraint_move_to_index(Object *ob, bConstraint *con, int index);
void constraint_link(Main *bmain, Object *ob_dst, ListBase *dst, ListBase *src);
void constraint_copy_for_object(Main *bmain, Object *ob_dst, bConstraint *con);
void constraint_copy_for_pose(Main *bmain, Object *ob_dst, bPoseChannel *pchan, bConstraint *con);

/* `object_modes.cc` */

/**
 * Checks the mode to be set is compatible with the object
 * should be made into a generic function
 */
bool mode_compat_test(const Object *ob, eObjectMode mode);
/**
 * Set the provided object's mode to one that is compatible with the provided mode.
 *
 * \returns true if the provided object's mode matches the provided mode, or if the function was
 * able to set the object back into Object Mode.
 *
 * This is so each mode toggle operator exec function can call this function to ensure the current
 * mode runtime data is cleaned up prior to entering a new mode.
 */
bool mode_compat_set(bContext *C, Object *ob, eObjectMode mode, ReportList *reports);
bool mode_set_ex(bContext *C, eObjectMode mode, bool use_undo, ReportList *reports);
bool mode_set(bContext *C, eObjectMode mode);

void mode_generic_exit(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob);
bool mode_generic_has_data(Depsgraph *depsgraph, const Object *ob);

void posemode_set_for_weight_paint(bContext *C, Main *bmain, Object *ob, bool is_mode_set);

/**
 * Return the index of an object in a mode (typically edit/pose mode).
 *
 * Useful for operators with multi-mode editing to be able to redo an action on an object
 * by its index which (unlike pointers) the operator can store for redo.
 *
 * The indices aren't intended to be useful from Python scripts,
 * although they are not prevented from passing them in, this is mainly to enable redo.
 * For scripts it's more convenient to set the object active before operating on it.
 *
 * \note The active object is always index 0.
 */
int object_in_mode_to_index(const Scene *scene,
                            ViewLayer *view_layer,
                            eObjectMode mode,
                            const Object *ob);

/**
 * Access the object from the index returned by #object_in_mode_to_index.
 */
Object *object_in_mode_from_index(const Scene *scene,
                                  ViewLayer *view_layer,
                                  eObjectMode mode,
                                  int index);

/**
 * Retrieve the alpha factors of the currently active mode transfer overlay animations. The key is
 * the object ID name to prevent possible storage of stale pointers and because the #session_uid
 * isn't available on evaluated objects.
 */
Map<std::string, float, 1> mode_transfer_overlay_current_state();

/* `object_modifier.cc` */

enum {
  MODIFIER_APPLY_DATA = 1,
  MODIFIER_APPLY_SHAPE,
};

/**
 * Add a modifier to given object, including relevant extra processing needed by some physics types
 * (particles, simulations...).
 *
 * \param scene: is only used to set current frame in some cases, and may be NULL.
 */
ModifierData *modifier_add(
    ReportList *reports, Main *bmain, Scene *scene, Object *ob, const char *name, int type);
bool modifier_remove(ReportList *reports, Main *bmain, Scene *scene, Object *ob, ModifierData *md);
void modifiers_clear(Main *bmain, Scene *scene, Object *ob);
bool modifier_move_down(ReportList *reports, eReportType error_type, Object *ob, ModifierData *md);
bool modifier_move_up(ReportList *reports, eReportType error_type, Object *ob, ModifierData *md);
bool modifier_move_to_index(ReportList *reports,
                            eReportType error_type,
                            Object *ob,
                            ModifierData *md,
                            int index,
                            bool allow_partial);

bool convert_psys_to_mesh(ReportList *reports,
                          Main *bmain,
                          Depsgraph *depsgraph,
                          Scene *scene,
                          ViewLayer *view_layer,
                          Object *ob,
                          ModifierData *md);
bool modifier_apply(Main *bmain,
                    ReportList *reports,
                    Depsgraph *depsgraph,
                    Scene *scene,
                    Object *ob,
                    ModifierData *md,
                    int mode,
                    bool keep_modifier,
                    bool do_all_keyframes);
bool modifier_copy(ReportList *reports, Main *bmain, Scene *scene, Object *ob, ModifierData *md);
void modifier_link(bContext *C, Object *ob_dst, Object *ob_src);
bool modifier_copy_to_object(Main *bmain,
                             const Scene *scene,
                             const Object *ob_src,
                             const ModifierData *md,
                             Object *ob_dst,
                             ReportList *reports);
/**
 * If the object data of 'orig_ob' has other users, run 'callback' on
 * each of them.
 *
 * If include_orig is true, the callback will run on 'orig_ob' too.
 *
 * If the callback ever returns true, iteration will stop and the
 * function value will be true. Otherwise the function returns false.
 */
bool iter_other(Main *bmain,
                Object *orig_ob,
                bool include_orig,
                bool (*callback)(Object *ob, void *callback_data),
                void *callback_data);

/**
 * Use with #iter_other(). Sets the total number of levels
 * for any multi-res modifiers on the object to the int pointed to by callback_data.
 */
bool multires_update_totlevels(Object *ob, void *totlevel_v);

/* `object_shader_fx.cc` */

ShaderFxData *shaderfx_add(
    ReportList *reports, Main *bmain, Scene *scene, Object *ob, const char *name, int type);
bool shaderfx_remove(ReportList *reports, Main *bmain, Object *ob, ShaderFxData *fx);
void shaderfx_clear(Main *bmain, Object *ob);
int shaderfx_move_down(ReportList *reports, Object *ob, ShaderFxData *fx);
int shaderfx_move_up(ReportList *reports, Object *ob, ShaderFxData *fx);
bool shaderfx_move_to_index(ReportList *reports, Object *ob, ShaderFxData *fx, int index);
void shaderfx_link(Object *dst, Object *src);
void shaderfx_copy(Object *dst, ShaderFxData *fx);

/* `object_select.cc` */

void select_linked_by_id(bContext *C, ID *id);

void check_force_modifiers(Main *bmain, Scene *scene, Object *object);

/**
 * If id is not already an Object, try to find an object that uses it as data.
 * Prefers active, then selected, then visible/selectable.
 */
Base *find_first_by_data_id(const Scene *scene, ViewLayer *view_layer, ID *id);

/**
 * Select and make the target object active in the view layer.
 * If already selected, selection isn't changed.
 *
 * \returns false if not found in current view layer
 */
bool jump_to_object(bContext *C, Object *ob, bool reveal_hidden);
/**
 * Select and make the target object and bone active.
 * Switches to Pose mode if in Object mode so the selection is visible.
 * Un-hides the target bone and bone layer if necessary.
 *
 * \returns false if object not in layer, bone not found, or other error
 */
bool jump_to_bone(bContext *C, Object *ob, const char *bone_name, bool reveal_hidden);

/* `object_data_transform.cc` */

struct XFormObjectData {
  ID *id;
  XFormObjectData() = default;
  virtual ~XFormObjectData() = default;
};

std::unique_ptr<XFormObjectData> data_xform_create(ID *id);
std::unique_ptr<XFormObjectData> data_xform_create_from_edit_mode(ID *id);

void data_xform_by_mat4(XFormObjectData &xod, const float4x4 &transform);

void data_xform_restore(XFormObjectData &xod);
void data_xform_tag_update(XFormObjectData &xod);

void ui_template_modifier_asset_menu_items(uiLayout &layout,
                                           StringRef catalog_path,
                                           bool skip_essentials);

}  // namespace blender::ed::object
