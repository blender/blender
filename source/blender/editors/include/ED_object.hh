/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_string_ref.hh"

#include "DNA_object_enums.h"
#include "DNA_userdef_enums.h"
#include "DNA_windowmanager_types.h"

struct Base;
struct Depsgraph;
struct EnumPropertyItem;
struct ID;
struct GpencilModifierData;
struct Main;
struct ModifierData;
struct Object;
struct PointerRNA;
struct PropertyRNA;
struct ReportList;
struct Scene;
struct ShaderFxData;
struct View3D;
struct ViewLayer;
struct XFormObjectData;
struct bConstraint;
struct bContext;
struct bPoseChannel;
struct uiLayout;
struct wmKeyConfig;
struct wmOperator;
struct wmOperatorType;

/* object_edit.cc */

/** `context.object` */
Object *ED_object_context(const bContext *C);
/**
 * Find the correct active object per context (`context.object` or `context.active_object`)
 * \note context can be NULL when called from a enum with #PROP_ENUM_NO_CONTEXT.
 */
Object *ED_object_active_context(const bContext *C);
void ED_collection_hide_menu_draw(const bContext *C, uiLayout *layout);

/**
 * Return an array of objects:
 * - When in the property space, return the pinned or active object.
 * - When in edit-mode/pose-mode, return an array of objects in the mode.
 * - Otherwise return selected objects,
 *   the callers \a filter_fn needs to check of they are editable
 *   (assuming they need to be modified).
 */
Object **ED_object_array_in_mode_or_selected(bContext *C,
                                             bool (*filter_fn)(const Object *ob, void *user_data),
                                             void *filter_user_data,
                                             uint *r_objects_len);

/* `object_utils.cc` */

bool ED_object_calc_active_center_for_editmode(Object *obedit,
                                               bool select_only,
                                               float r_center[3]);
bool ED_object_calc_active_center_for_posemode(Object *ob, bool select_only, float r_center[3]);
bool ED_object_calc_active_center(Object *ob, bool select_only, float r_center[3]);

/* Object Data Container helper API. */
struct XFormObjectData_Container;
XFormObjectData_Container *ED_object_data_xform_container_create();
void ED_object_data_xform_container_destroy(XFormObjectData_Container *xds);
/**
 * This may be called multiple times with the same data.
 * Each time, the original transformations are re-applied, instead of accumulating the changes.
 */
void ED_object_data_xform_container_update_all(XFormObjectData_Container *xds,
                                               Main *bmain,
                                               Depsgraph *depsgraph);
void ED_object_data_xform_container_item_ensure(XFormObjectData_Container *xds, Object *ob);

/* Object Skip-Child Container helper API. */
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
XFormObjectSkipChild_Container *ED_object_xform_skip_child_container_create();
void ED_object_xform_skip_child_container_item_ensure_from_array(
    XFormObjectSkipChild_Container *xcs,
    const Scene *scene,
    ViewLayer *view_layer,
    Object **objects,
    uint objects_len);
void ED_object_xform_skip_child_container_destroy(XFormObjectSkipChild_Container *xcs);
void ED_object_xform_skip_child_container_update_all(XFormObjectSkipChild_Container *xcs,
                                                     Main *bmain,
                                                     Depsgraph *depsgraph);
void ED_object_xform_skip_child_container_item_ensure(XFormObjectSkipChild_Container *xcs,
                                                      Object *ob,
                                                      Object *ob_parent_recurse,
                                                      int mode);

void ED_object_xform_array_m4(Object **objects, uint objects_len, const float matrix[4][4]);

/* `object_ops.cc` */

void ED_operatortypes_object();
void ED_operatormacros_object();
void ED_keymap_object(wmKeyConfig *keyconf);

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
extern EnumPropertyItem prop_clear_parent_types[];
/** Operator Property: `OBJECT_OT_parent_set`. */
extern EnumPropertyItem prop_make_parent_types[];
#endif

/**
 * Set the object's parent, return true if successful.
 */
bool ED_object_parent_set(ReportList *reports,
                          const bContext *C,
                          Scene *scene,
                          Object *const ob,
                          Object *const par,
                          int partype,
                          bool xmirror,
                          bool keep_transform,
                          const int vert_par[3]);
void ED_object_parent_clear(Object *ob, int type);

/**
 * Simple API for object selection, rather than just using the flag
 * this takes into account the 'restrict selection in 3d view' flag.
 * deselect works always, the restriction just prevents selection
 *
 * \note Caller must send a `NC_SCENE | ND_OB_SELECT` notifier
 * (or a `NC_SCENE | ND_OB_VISIBLE` in case of visibility toggling).
 */
void ED_object_base_select(Base *base, eObjectSelect_Mode mode);
/**
 * Change active base, it includes the notifier
 */
void ED_object_base_activate(bContext *C, Base *base);
void ED_object_base_activate_with_mode_exit_if_needed(bContext *C, Base *base);
/**
 * Call when the active base has changed.
 */
void ED_object_base_active_refresh(Main *bmain, Scene *scene, ViewLayer *view_layer);
/**
 * Remove base from a specific scene.
 * \note now unlinks constraints as well.
 */
void ED_object_base_free_and_unlink(Main *bmain, Scene *scene, Object *ob);
/**
 * Remove base from a specific scene.
 * `ob` must not be indirectly used.
 */
void ED_object_base_free_and_unlink_no_indirect_check(Main *bmain, Scene *scene, Object *ob);
bool ED_object_base_deselect_all_ex(
    const Scene *scene, ViewLayer *view_layer, View3D *v3d, int action, bool *r_any_visible);
bool ED_object_base_deselect_all(const Scene *scene,
                                 ViewLayer *view_layer,
                                 View3D *v3d,
                                 int action);

/**
 * Single object duplicate, if `dupflag == 0`, fully linked, else it uses the flags given.
 * Leaves selection of base/object unaltered.
 * \note don't call this within a loop since clear_* functions loop over the entire database.
 * \note caller must do `DAG_relations_tag_update(bmain);`
 * this is not done automatic since we may duplicate many objects in a batch.
 */
Base *ED_object_add_duplicate(
    Main *bmain, Scene *scene, ViewLayer *view_layer, Base *base, eDupli_ID_Flags dupflag);

void ED_object_parent(Object *ob, Object *parent, int type, const char *substr);
char *ED_object_ot_drop_named_material_tooltip(bContext *C, const char *name, const int mval[2]);
char *ED_object_ot_drop_geometry_nodes_tooltip(bContext *C,
                                               PointerRNA *properties,
                                               const int mval[2]);

/* bitflags for enter/exit editmode */
enum {
  EM_FREEDATA = (1 << 0),
  EM_NO_CONTEXT = (1 << 1),
};
/**
 * \param flag:
 * - If #EM_FREEDATA isn't in the flag, use ED_object_editmode_load directly.
 */
bool ED_object_editmode_exit_ex(Main *bmain, Scene *scene, Object *obedit, int flag);
bool ED_object_editmode_exit(bContext *C, int flag);

/**
 * Support freeing edit-mode data without flushing it back to the object.
 *
 * \return true if data was freed.
 */
bool ED_object_editmode_free_ex(Main *bmain, Object *obedit);

bool ED_object_editmode_exit_multi_ex(Main *bmain, Scene *scene, ViewLayer *view_layer, int flag);
bool ED_object_editmode_exit_multi(bContext *C, int flag);

bool ED_object_editmode_enter_ex(Main *bmain, Scene *scene, Object *ob, int flag);
bool ED_object_editmode_enter(bContext *C, int flag);
bool ED_object_editmode_load(Main *bmain, Object *obedit);

void ED_object_vpaintmode_enter_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob);
void ED_object_vpaintmode_enter(bContext *C, Depsgraph *depsgraph);
void ED_object_wpaintmode_enter_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob);
void ED_object_wpaintmode_enter(bContext *C, Depsgraph *depsgraph);

void ED_object_vpaintmode_exit_ex(Object *ob);
void ED_object_vpaintmode_exit(bContext *C);
void ED_object_wpaintmode_exit_ex(Object *ob);
void ED_object_wpaintmode_exit(bContext *C);

void ED_object_texture_paint_mode_enter_ex(Main *bmain,
                                           Scene *scene,
                                           Depsgraph *depsgraph,
                                           Object *ob);
void ED_object_texture_paint_mode_enter(bContext *C);

void ED_object_texture_paint_mode_exit_ex(Main *bmain, Scene *scene, Object *ob);
void ED_object_texture_paint_mode_exit(bContext *C);

bool ED_object_particle_edit_mode_supported(const Object *ob);
void ED_object_particle_edit_mode_enter_ex(Depsgraph *depsgraph, Scene *scene, Object *ob);
void ED_object_particle_edit_mode_enter(bContext *C);

void ED_object_particle_edit_mode_exit_ex(Scene *scene, Object *ob);
void ED_object_particle_edit_mode_exit(bContext *C);

void ED_object_sculptmode_enter_ex(Main *bmain,
                                   Depsgraph *depsgraph,
                                   Scene *scene,
                                   Object *ob,
                                   bool force_dyntopo,
                                   ReportList *reports);
void ED_object_sculptmode_enter(bContext *C, Depsgraph *depsgraph, ReportList *reports);
void ED_object_sculptmode_exit_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob);
void ED_object_sculptmode_exit(bContext *C, Depsgraph *depsgraph);

void ED_object_location_from_view(bContext *C, float loc[3]);
void ED_object_rotation_from_quat(float rot[3], const float quat[4], char align_axis);
void ED_object_rotation_from_view(bContext *C, float rot[3], char align_axis);
void ED_object_base_init_transform_on_add(Object *object, const float loc[3], const float rot[3]);
/**
 * Uses context to figure out transform for primitive.
 * Returns standard diameter.
 */
float ED_object_new_primitive_matrix(bContext *C,
                                     Object *obedit,
                                     const float loc[3],
                                     const float rot[3],
                                     const float scale[3],
                                     float primmat[4][4]);

/**
 * Avoid allowing too much insane values even by typing (typos can hang/crash Blender otherwise).
 */
#define OBJECT_ADD_SIZE_MAXF 1.0e12f

void ED_object_add_unit_props_size(wmOperatorType *ot);
void ED_object_add_unit_props_radius_ex(wmOperatorType *ot, float default_value);
void ED_object_add_unit_props_radius(wmOperatorType *ot);
void ED_object_add_generic_props(wmOperatorType *ot, bool do_editmode);
void ED_object_add_mesh_props(wmOperatorType *ot);
bool ED_object_add_generic_get_opts(bContext *C,
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
Object *ED_object_add_type_with_obdata(bContext *C,
                                       int type,
                                       const char *name,
                                       const float loc[3],
                                       const float rot[3],
                                       bool enter_editmode,
                                       ushort local_view_bits,
                                       ID *obdata);
Object *ED_object_add_type(bContext *C,
                           int type,
                           const char *name,
                           const float loc[3],
                           const float rot[3],
                           bool enter_editmode,
                           unsigned short local_view_bits) ATTR_NONNULL(1) ATTR_RETURNS_NONNULL;

/**
 * Not an especially efficient function, only added so the single user button can be functional.
 */
void ED_object_single_user(Main *bmain, Scene *scene, Object *ob);

void ED_object_single_obdata_user(Main *bmain, Scene *scene, Object *ob);

/* object motion paths */

/**
 * Clear motion paths for all objects.
 */
void ED_objects_clear_paths(bContext *C, bool only_selected);

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
void ED_objects_recalculate_paths(bContext *C,
                                  Scene *scene,
                                  eObjectPathCalcRange range,
                                  ListBase *ld_objects);

void ED_objects_recalculate_paths_selected(bContext *C, Scene *scene, eObjectPathCalcRange range);

void ED_objects_recalculate_paths_visible(bContext *C, Scene *scene, eObjectPathCalcRange range);

/* constraints */
/**
 * If object is in pose-mode, return active bone constraints, else object constraints.
 * No constraints are returned for a bone on an inactive bone-layer.
 */
ListBase *ED_object_constraint_active_list(Object *ob);
/**
 * Get the constraints for the active pose bone. Bone may be on an inactive bone-layer
 * (unlike #ED_object_constraint_active_list, such constraints are not excluded here).
 */
ListBase *ED_object_pose_constraint_list(const bContext *C);
/**
 * Find the list that a given constraint belongs to,
 * and/or also get the posechannel this is from (if applicable).
 */
ListBase *ED_object_constraint_list_from_constraint(Object *ob,
                                                    bConstraint *con,
                                                    bPoseChannel **r_pchan);
/**
 * Single constraint.
 */
bConstraint *ED_object_constraint_active_get(Object *ob);

void object_test_constraints(Main *bmain, Object *ob);

void ED_object_constraint_active_set(Object *ob, bConstraint *con);
void ED_object_constraint_update(Main *bmain, Object *ob);
void ED_object_constraint_dependency_update(Main *bmain, Object *ob);

void ED_object_constraint_tag_update(Main *bmain, Object *ob, bConstraint *con);
void ED_object_constraint_dependency_tag_update(Main *bmain, Object *ob, bConstraint *con);

bool ED_object_constraint_move_to_index(Object *ob, bConstraint *con, int index);
void ED_object_constraint_link(Main *bmain, Object *ob_dst, ListBase *dst, ListBase *src);
void ED_object_constraint_copy_for_object(Main *bmain, Object *ob_dst, bConstraint *con);
void ED_object_constraint_copy_for_pose(Main *bmain,
                                        Object *ob_dst,
                                        bPoseChannel *pchan,
                                        bConstraint *con);

/* object_modes.cc */

/**
 * Checks the mode to be set is compatible with the object
 * should be made into a generic function
 */
bool ED_object_mode_compat_test(const Object *ob, eObjectMode mode);
/**
 * Sets the mode to a compatible state (use before entering the mode).
 *
 * This is so each mode's exec function can call
 */
bool ED_object_mode_compat_set(bContext *C, Object *ob, eObjectMode mode, ReportList *reports);
bool ED_object_mode_set_ex(bContext *C, eObjectMode mode, bool use_undo, ReportList *reports);
bool ED_object_mode_set(bContext *C, eObjectMode mode);

void ED_object_mode_generic_exit(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob);
bool ED_object_mode_generic_has_data(Depsgraph *depsgraph, const Object *ob);

void ED_object_posemode_set_for_weight_paint(bContext *C,
                                             Main *bmain,
                                             Object *ob,
                                             bool is_mode_set);

/**
 * Return the index of an object in a mode (typically edit/pose mode).
 *
 * Useful for operators with multi-mode editing to be able to redo an action on an object
 * by it's index which (unlike pointers) the operator can store for redo.
 *
 * The indices aren't intended to be useful from Python scripts,
 * although they are not prevented from passing them in, this is mainly to enable redo.
 * For scripts it's more convenient to set the object active before operating on it.
 *
 * \note The active object is always index 0.
 */
int ED_object_in_mode_to_index(const Scene *scene,
                               ViewLayer *view_layer,
                               eObjectMode mode,
                               const Object *ob);

/**
 * Access the object from the index returned by #ED_object_in_mode_to_index.
 */
Object *ED_object_in_mode_from_index(const Scene *scene,
                                     ViewLayer *view_layer,
                                     eObjectMode mode,
                                     int index);

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
ModifierData *ED_object_modifier_add(
    ReportList *reports, Main *bmain, Scene *scene, Object *ob, const char *name, int type);
bool ED_object_modifier_remove(
    ReportList *reports, Main *bmain, Scene *scene, Object *ob, ModifierData *md);
void ED_object_modifier_clear(Main *bmain, Scene *scene, Object *ob);
bool ED_object_modifier_move_down(ReportList *reports,
                                  eReportType error_type,
                                  Object *ob,
                                  ModifierData *md);
bool ED_object_modifier_move_up(ReportList *reports,
                                eReportType error_type,
                                Object *ob,
                                ModifierData *md);
bool ED_object_modifier_move_to_index(ReportList *reports,
                                      eReportType error_type,
                                      Object *ob,
                                      ModifierData *md,
                                      int index,
                                      bool allow_partial);

bool ED_object_modifier_convert_psys_to_mesh(ReportList *reports,
                                             Main *bmain,
                                             Depsgraph *depsgraph,
                                             Scene *scene,
                                             ViewLayer *view_layer,
                                             Object *ob,
                                             ModifierData *md);
bool ED_object_modifier_apply(Main *bmain,
                              ReportList *reports,
                              Depsgraph *depsgraph,
                              Scene *scene,
                              Object *ob,
                              ModifierData *md,
                              int mode,
                              bool keep_modifier);
bool ED_object_modifier_copy(
    ReportList *reports, Main *bmain, Scene *scene, Object *ob, ModifierData *md);
void ED_object_modifier_link(bContext *C, Object *ob_dst, Object *ob_src);
void ED_object_modifier_copy_to_object(bContext *C,
                                       Object *ob_dst,
                                       Object *ob_src,
                                       ModifierData *md);

/**
 * If the object data of 'orig_ob' has other users, run 'callback' on
 * each of them.
 *
 * If include_orig is true, the callback will run on 'orig_ob' too.
 *
 * If the callback ever returns true, iteration will stop and the
 * function value will be true. Otherwise the function returns false.
 */
bool ED_object_iter_other(Main *bmain,
                          Object *orig_ob,
                          bool include_orig,
                          bool (*callback)(Object *ob, void *callback_data),
                          void *callback_data);

/**
 * Use with #ED_object_iter_other(). Sets the total number of levels
 * for any multi-res modifiers on the object to the int pointed to by callback_data.
 */
bool ED_object_multires_update_totlevels_cb(Object *ob, void *totlevel_v);

/* object_greasepencil_modifier.c */

GpencilModifierData *ED_object_gpencil_modifier_add(
    ReportList *reports, Main *bmain, Scene *scene, Object *ob, const char *name, int type);
bool ED_object_gpencil_modifier_remove(ReportList *reports,
                                       Main *bmain,
                                       Object *ob,
                                       GpencilModifierData *md);
void ED_object_gpencil_modifier_clear(Main *bmain, Object *ob);
bool ED_object_gpencil_modifier_move_down(ReportList *reports,
                                          Object *ob,
                                          GpencilModifierData *md);
bool ED_object_gpencil_modifier_move_up(ReportList *reports, Object *ob, GpencilModifierData *md);
bool ED_object_gpencil_modifier_move_to_index(ReportList *reports,
                                              Object *ob,
                                              GpencilModifierData *md,
                                              int index);
bool ED_object_gpencil_modifier_apply(Main *bmain,
                                      ReportList *reports,
                                      Depsgraph *depsgraph,
                                      Object *ob,
                                      GpencilModifierData *md,
                                      int mode);
bool ED_object_gpencil_modifier_copy(ReportList *reports, Object *ob, GpencilModifierData *md);
void ED_object_gpencil_modifier_copy_to_object(Object *ob_dst, GpencilModifierData *md);

/* `object_shader_fx.cc` */

ShaderFxData *ED_object_shaderfx_add(
    ReportList *reports, Main *bmain, Scene *scene, Object *ob, const char *name, int type);
bool ED_object_shaderfx_remove(ReportList *reports, Main *bmain, Object *ob, ShaderFxData *fx);
void ED_object_shaderfx_clear(Main *bmain, Object *ob);
int ED_object_shaderfx_move_down(ReportList *reports, Object *ob, ShaderFxData *fx);
int ED_object_shaderfx_move_up(ReportList *reports, Object *ob, ShaderFxData *fx);
bool ED_object_shaderfx_move_to_index(ReportList *reports,
                                      Object *ob,
                                      ShaderFxData *fx,
                                      int index);
void ED_object_shaderfx_link(Object *dst, Object *src);
void ED_object_shaderfx_copy(Object *dst, ShaderFxData *fx);

/* `object_select.cc` */

void ED_object_select_linked_by_id(bContext *C, ID *id);

const EnumPropertyItem *ED_object_vgroup_selection_itemf_helper(const bContext *C,
                                                                PointerRNA *ptr,
                                                                PropertyRNA *prop,
                                                                bool *r_free,
                                                                unsigned int selection_mask);

void ED_object_check_force_modifiers(Main *bmain, Scene *scene, Object *object);

/**
 * If id is not already an Object, try to find an object that uses it as data.
 * Prefers active, then selected, then visible/selectable.
 */
Base *ED_object_find_first_by_data_id(const Scene *scene, ViewLayer *view_layer, ID *id);

/**
 * Select and make the target object active in the view layer.
 * If already selected, selection isn't changed.
 *
 * \returns false if not found in current view layer
 */
bool ED_object_jump_to_object(bContext *C, Object *ob, bool reveal_hidden);
/**
 * Select and make the target object and bone active.
 * Switches to Pose mode if in Object mode so the selection is visible.
 * Un-hides the target bone and bone layer if necessary.
 *
 * \returns false if object not in layer, bone not found, or other error
 */
bool ED_object_jump_to_bone(bContext *C, Object *ob, const char *bone_name, bool reveal_hidden);

/* object_data_transform.cc */

XFormObjectData *ED_object_data_xform_create_ex(ID *id, bool is_edit_mode);
XFormObjectData *ED_object_data_xform_create(ID *id);
XFormObjectData *ED_object_data_xform_create_from_edit_mode(ID *id);

void ED_object_data_xform_destroy(XFormObjectData *xod_base);

void ED_object_data_xform_by_mat4(XFormObjectData *xod, const float mat[4][4]);

void ED_object_data_xform_restore(XFormObjectData *xod);
void ED_object_data_xform_tag_update(XFormObjectData *xod);

namespace blender::ed::object {

void ui_template_modifier_asset_menu_items(uiLayout &layout, bContext &C, StringRef catalog_path);

}
