/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "DNA_object_enums.h"
#include "DNA_userdef_enums.h"
#include "DNA_windowmanager_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Base;
struct Depsgraph;
struct EnumPropertyItem;
struct ID;
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
struct bFaceMap;
struct bPoseChannel;
struct uiLayout;
struct wmKeyConfig;
struct wmOperator;
struct wmOperatorType;

/* object_edit.c */

/** `context.object` */
struct Object *ED_object_context(const struct bContext *C);
/**
 * Find the correct active object per context (`context.object` or `context.active_object`)
 * \note context can be NULL when called from a enum with #PROP_ENUM_NO_CONTEXT.
 */
struct Object *ED_object_active_context(const struct bContext *C);
void ED_collection_hide_menu_draw(const struct bContext *C, struct uiLayout *layout);

/**
 * Return an array of objects:
 * - When in the property space, return the pinned or active object.
 * - When in edit-mode/pose-mode, return an array of objects in the mode.
 * - Otherwise return selected objects,
 *   the callers \a filter_fn needs to check of they are editable
 *   (assuming they need to be modified).
 */
Object **ED_object_array_in_mode_or_selected(struct bContext *C,
                                             bool (*filter_fn)(const struct Object *ob,
                                                               void *user_data),
                                             void *filter_user_data,
                                             uint *r_objects_len);

/* object_utils.c */

bool ED_object_calc_active_center_for_editmode(struct Object *obedit,
                                               bool select_only,
                                               float r_center[3]);
bool ED_object_calc_active_center_for_posemode(struct Object *ob,
                                               bool select_only,
                                               float r_center[3]);
bool ED_object_calc_active_center(struct Object *ob, bool select_only, float r_center[3]);

/* Object Data Container helper API. */
struct XFormObjectData_Container;
struct XFormObjectData_Container *ED_object_data_xform_container_create(void);
void ED_object_data_xform_container_destroy(struct XFormObjectData_Container *xds);
/**
 * This may be called multiple times with the same data.
 * Each time, the original transformations are re-applied, instead of accumulating the changes.
 */
void ED_object_data_xform_container_update_all(struct XFormObjectData_Container *xds,
                                               struct Main *bmain,
                                               struct Depsgraph *depsgraph);
void ED_object_data_xform_container_item_ensure(struct XFormObjectData_Container *xds,
                                                struct Object *ob);

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
struct XFormObjectSkipChild_Container *ED_object_xform_skip_child_container_create(void);
void ED_object_xform_skip_child_container_item_ensure_from_array(
    struct XFormObjectSkipChild_Container *xcs,
    const struct Scene *scene,
    struct ViewLayer *view_layer,
    struct Object **objects,
    uint objects_len);
void ED_object_xform_skip_child_container_destroy(struct XFormObjectSkipChild_Container *xcs);
void ED_object_xform_skip_child_container_update_all(struct XFormObjectSkipChild_Container *xcs,
                                                     struct Main *bmain,
                                                     struct Depsgraph *depsgraph);
void ED_object_xform_skip_child_container_item_ensure(struct XFormObjectSkipChild_Container *xcs,
                                                      struct Object *ob,
                                                      struct Object *ob_parent_recurse,
                                                      int mode);

void ED_object_xform_array_m4(struct Object **objects, uint objects_len, const float matrix[4][4]);

/* object_ops.c */

void ED_operatortypes_object(void);
void ED_operatormacros_object(void);
void ED_keymap_object(struct wmKeyConfig *keyconf);

/* object_relations.c */

typedef enum eParentType {
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
} eParentType;

typedef enum eObjectSelect_Mode {
  BA_DESELECT = 0,
  BA_SELECT = 1,
  BA_INVERT = 2,
} eObjectSelect_Mode;

typedef enum eObClearParentTypes {
  CLEAR_PARENT_ALL = 0,
  CLEAR_PARENT_KEEP_TRANSFORM,
  CLEAR_PARENT_INVERSE,
} eObClearParentTypes;

#ifdef __RNA_TYPES_H__
extern struct EnumPropertyItem prop_clear_parent_types[];
extern struct EnumPropertyItem prop_make_parent_types[];
#endif

/**
 * Set the object's parent, return true if successful.
 */
bool ED_object_parent_set(struct ReportList *reports,
                          const struct bContext *C,
                          struct Scene *scene,
                          struct Object *const ob,
                          struct Object *const par,
                          int partype,
                          bool xmirror,
                          bool keep_transform,
                          const int vert_par[3]);
void ED_object_parent_clear(struct Object *ob, int type);

/**
 * Simple API for object selection, rather than just using the flag
 * this takes into account the 'restrict selection in 3d view' flag.
 * deselect works always, the restriction just prevents selection
 *
 * \note Caller must send a `NC_SCENE | ND_OB_SELECT` notifier
 * (or a `NC_SCENE | ND_OB_VISIBLE` in case of visibility toggling).
 */
void ED_object_base_select(struct Base *base, eObjectSelect_Mode mode);
/**
 * Change active base, it includes the notifier
 */
void ED_object_base_activate(struct bContext *C, struct Base *base);
void ED_object_base_activate_with_mode_exit_if_needed(struct bContext *C, struct Base *base);
/**
 * Call when the active base has changed.
 */
void ED_object_base_active_refresh(struct Main *bmain,
                                   struct Scene *scene,
                                   struct ViewLayer *view_layer);
/**
 * Remove base from a specific scene.
 * \note now unlinks constraints as well.
 */
void ED_object_base_free_and_unlink(struct Main *bmain, struct Scene *scene, struct Object *ob);
/**
 * Remove base from a specific scene.
 * `ob` must not be indirectly used.
 */
void ED_object_base_free_and_unlink_no_indirect_check(struct Main *bmain,
                                                      struct Scene *scene,
                                                      struct Object *ob);
bool ED_object_base_deselect_all_ex(const struct Scene *scene,
                                    struct ViewLayer *view_layer,
                                    struct View3D *v3d,
                                    int action,
                                    bool *r_any_visible);
bool ED_object_base_deselect_all(const struct Scene *scene,
                                 struct ViewLayer *view_layer,
                                 struct View3D *v3d,
                                 int action);

/**
 * Single object duplicate, if `dupflag == 0`, fully linked, else it uses the flags given.
 * Leaves selection of base/object unaltered.
 * \note don't call this within a loop since clear_* functions loop over the entire database.
 * \note caller must do `DAG_relations_tag_update(bmain);`
 * this is not done automatic since we may duplicate many objects in a batch.
 */
struct Base *ED_object_add_duplicate(struct Main *bmain,
                                     struct Scene *scene,
                                     struct ViewLayer *view_layer,
                                     struct Base *base,
                                     eDupli_ID_Flags dupflag);

void ED_object_parent(struct Object *ob, struct Object *parent, int type, const char *substr);
char *ED_object_ot_drop_named_material_tooltip(struct bContext *C,
                                               const char *name,
                                               const int mval[2]);
char *ED_object_ot_drop_geometry_nodes_tooltip(struct bContext *C,
                                               struct PointerRNA *properties,
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
bool ED_object_editmode_exit_ex(struct Main *bmain,
                                struct Scene *scene,
                                struct Object *obedit,
                                int flag);
bool ED_object_editmode_exit(struct bContext *C, int flag);

/**
 * Support freeing edit-mode data without flushing it back to the object.
 *
 * \return true if data was freed.
 */
bool ED_object_editmode_free_ex(struct Main *bmain, struct Object *obedit);

bool ED_object_editmode_exit_multi_ex(struct Main *bmain,
                                      struct Scene *scene,
                                      struct ViewLayer *view_layer,
                                      int flag);
bool ED_object_editmode_exit_multi(struct bContext *C, int flag);

bool ED_object_editmode_enter_ex(struct Main *bmain,
                                 struct Scene *scene,
                                 struct Object *ob,
                                 int flag);
bool ED_object_editmode_enter(struct bContext *C, int flag);
bool ED_object_editmode_load(struct Main *bmain, struct Object *obedit);

void ED_object_vpaintmode_enter_ex(struct Main *bmain,
                                   struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *ob);
void ED_object_vpaintmode_enter(struct bContext *C, struct Depsgraph *depsgraph);
void ED_object_wpaintmode_enter_ex(struct Main *bmain,
                                   struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *ob);
void ED_object_wpaintmode_enter(struct bContext *C, struct Depsgraph *depsgraph);

void ED_object_vpaintmode_exit_ex(struct Object *ob);
void ED_object_vpaintmode_exit(struct bContext *C);
void ED_object_wpaintmode_exit_ex(struct Object *ob);
void ED_object_wpaintmode_exit(struct bContext *C);

void ED_object_texture_paint_mode_enter_ex(struct Main *bmain,
                                           struct Scene *scene,
                                           struct Depsgraph *depsgraph,
                                           Object *ob);
void ED_object_texture_paint_mode_enter(struct bContext *C);

void ED_object_texture_paint_mode_exit_ex(struct Main *bmain, struct Scene *scene, Object *ob);
void ED_object_texture_paint_mode_exit(struct bContext *C);

bool ED_object_particle_edit_mode_supported(const Object *ob);
void ED_object_particle_edit_mode_enter_ex(struct Depsgraph *depsgraph,
                                           struct Scene *scene,
                                           Object *ob);
void ED_object_particle_edit_mode_enter(struct bContext *C);

void ED_object_particle_edit_mode_exit_ex(struct Scene *scene, Object *ob);
void ED_object_particle_edit_mode_exit(struct bContext *C);

void ED_object_sculptmode_enter_ex(struct Main *bmain,
                                   struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *ob,
                                   bool force_dyntopo,
                                   struct ReportList *reports);
void ED_object_sculptmode_enter(struct bContext *C,
                                struct Depsgraph *depsgraph,
                                struct ReportList *reports);
void ED_object_sculptmode_exit_ex(struct Main *bmain,
                                  struct Depsgraph *depsgraph,
                                  struct Scene *scene,
                                  struct Object *ob);
void ED_object_sculptmode_exit(struct bContext *C, struct Depsgraph *depsgraph);

void ED_object_location_from_view(struct bContext *C, float loc[3]);
void ED_object_rotation_from_quat(float rot[3], const float quat[4], char align_axis);
void ED_object_rotation_from_view(struct bContext *C, float rot[3], char align_axis);
void ED_object_base_init_transform_on_add(struct Object *object,
                                          const float loc[3],
                                          const float rot[3]);
/**
 * Uses context to figure out transform for primitive.
 * Returns standard diameter.
 */
float ED_object_new_primitive_matrix(struct bContext *C,
                                     struct Object *obedit,
                                     const float loc[3],
                                     const float rot[3],
                                     const float scale[3],
                                     float primmat[4][4]);

/**
 * Avoid allowing too much insane values even by typing (typos can hang/crash Blender otherwise).
 */
#define OBJECT_ADD_SIZE_MAXF 1.0e12f

void ED_object_add_unit_props_size(struct wmOperatorType *ot);
void ED_object_add_unit_props_radius_ex(struct wmOperatorType *ot, float default_value);
void ED_object_add_unit_props_radius(struct wmOperatorType *ot);
void ED_object_add_generic_props(struct wmOperatorType *ot, bool do_editmode);
void ED_object_add_mesh_props(struct wmOperatorType *ot);
bool ED_object_add_generic_get_opts(struct bContext *C,
                                    struct wmOperator *op,
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
struct Object *ED_object_add_type_with_obdata(struct bContext *C,
                                              int type,
                                              const char *name,
                                              const float loc[3],
                                              const float rot[3],
                                              bool enter_editmode,
                                              ushort local_view_bits,
                                              struct ID *obdata);
struct Object *ED_object_add_type(struct bContext *C,
                                  int type,
                                  const char *name,
                                  const float loc[3],
                                  const float rot[3],
                                  bool enter_editmode,
                                  unsigned short local_view_bits)
    ATTR_NONNULL(1) ATTR_RETURNS_NONNULL;

/**
 * Not an especially efficient function, only added so the single user button can be functional.
 */
void ED_object_single_user(struct Main *bmain, struct Scene *scene, struct Object *ob);

void ED_object_single_obdata_user(struct Main *bmain, struct Scene *scene, struct Object *ob);

/* object motion paths */

/**
 * Clear motion paths for all objects.
 */
void ED_objects_clear_paths(struct bContext *C, bool only_selected);

/* Corresponds to eAnimvizCalcRange. */
typedef enum eObjectPathCalcRange {
  OBJECT_PATH_CALC_RANGE_CURRENT_FRAME,
  OBJECT_PATH_CALC_RANGE_CHANGED,
  OBJECT_PATH_CALC_RANGE_FULL,
} eObjectPathCalcRange;

/**
 * For the objects with animation: update paths for those that have got them
 * This should selectively update paths that exist.
 *
 * To be called from various tools that do incremental updates
 */
void ED_objects_recalculate_paths(struct bContext *C,
                                  struct Scene *scene,
                                  eObjectPathCalcRange range,
                                  struct ListBase *ld_objects);

void ED_objects_recalculate_paths_selected(struct bContext *C,
                                           struct Scene *scene,
                                           eObjectPathCalcRange range);

void ED_objects_recalculate_paths_visible(struct bContext *C,
                                          struct Scene *scene,
                                          eObjectPathCalcRange range);

/* constraints */
/**
 * If object is in pose-mode, return active bone constraints, else object constraints.
 * No constraints are returned for a bone on an inactive bone-layer.
 */
struct ListBase *ED_object_constraint_active_list(struct Object *ob);
/**
 * Get the constraints for the active pose bone. Bone may be on an inactive bone-layer
 * (unlike #ED_object_constraint_active_list, such constraints are not excluded here).
 */
struct ListBase *ED_object_pose_constraint_list(const struct bContext *C);
/**
 * Find the list that a given constraint belongs to,
 * and/or also get the posechannel this is from (if applicable).
 */
struct ListBase *ED_object_constraint_list_from_constraint(struct Object *ob,
                                                           struct bConstraint *con,
                                                           struct bPoseChannel **r_pchan);
/**
 * Single constraint.
 */
struct bConstraint *ED_object_constraint_active_get(struct Object *ob);

void object_test_constraints(struct Main *bmain, struct Object *ob);

void ED_object_constraint_active_set(struct Object *ob, struct bConstraint *con);
void ED_object_constraint_update(struct Main *bmain, struct Object *ob);
void ED_object_constraint_dependency_update(struct Main *bmain, struct Object *ob);

void ED_object_constraint_tag_update(struct Main *bmain,
                                     struct Object *ob,
                                     struct bConstraint *con);
void ED_object_constraint_dependency_tag_update(struct Main *bmain,
                                                struct Object *ob,
                                                struct bConstraint *con);

bool ED_object_constraint_move_to_index(struct Object *ob, struct bConstraint *con, int index);
void ED_object_constraint_link(struct Main *bmain,
                               struct Object *ob_dst,
                               struct ListBase *dst,
                               struct ListBase *src);
void ED_object_constraint_copy_for_object(struct Main *bmain,
                                          struct Object *ob_dst,
                                          struct bConstraint *con);
void ED_object_constraint_copy_for_pose(struct Main *bmain,
                                        struct Object *ob_dst,
                                        struct bPoseChannel *pchan,
                                        struct bConstraint *con);

/* object_modes.cc */

/**
 * Checks the mode to be set is compatible with the object
 * should be made into a generic function
 */
bool ED_object_mode_compat_test(const struct Object *ob, eObjectMode mode);
/**
 * Sets the mode to a compatible state (use before entering the mode).
 *
 * This is so each mode's exec function can call
 */
bool ED_object_mode_compat_set(struct bContext *C,
                               struct Object *ob,
                               eObjectMode mode,
                               struct ReportList *reports);
bool ED_object_mode_set_ex(struct bContext *C,
                           eObjectMode mode,
                           bool use_undo,
                           struct ReportList *reports);
bool ED_object_mode_set(struct bContext *C, eObjectMode mode);

void ED_object_mode_generic_exit(struct Main *bmain,
                                 struct Depsgraph *depsgraph,
                                 struct Scene *scene,
                                 struct Object *ob);
bool ED_object_mode_generic_has_data(struct Depsgraph *depsgraph, const struct Object *ob);

void ED_object_posemode_set_for_weight_paint(struct bContext *C,
                                             struct Main *bmain,
                                             struct Object *ob,
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
int ED_object_in_mode_to_index(const struct Scene *scene,
                               struct ViewLayer *view_layer,
                               eObjectMode mode,
                               const struct Object *ob);

/**
 * Access the object from the index returned by #ED_object_in_mode_to_index.
 */
Object *ED_object_in_mode_from_index(const struct Scene *scene,
                                     struct ViewLayer *view_layer,
                                     eObjectMode mode,
                                     int index);

/* object_modifier.c */

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
struct ModifierData *ED_object_modifier_add(struct ReportList *reports,
                                            struct Main *bmain,
                                            struct Scene *scene,
                                            struct Object *ob,
                                            const char *name,
                                            int type);
bool ED_object_modifier_remove(struct ReportList *reports,
                               struct Main *bmain,
                               struct Scene *scene,
                               struct Object *ob,
                               struct ModifierData *md);
void ED_object_modifier_clear(struct Main *bmain, struct Scene *scene, struct Object *ob);
bool ED_object_modifier_move_down(struct ReportList *reports,
                                  eReportType error_type,
                                  struct Object *ob,
                                  struct ModifierData *md);
bool ED_object_modifier_move_up(struct ReportList *reports,
                                eReportType error_type,
                                struct Object *ob,
                                struct ModifierData *md);
bool ED_object_modifier_move_to_index(struct ReportList *reports,
                                      eReportType error_type,
                                      struct Object *ob,
                                      struct ModifierData *md,
                                      int index,
                                      bool allow_partial);

bool ED_object_modifier_convert_psys_to_mesh(struct ReportList *reports,
                                             struct Main *bmain,
                                             struct Depsgraph *depsgraph,
                                             struct Scene *scene,
                                             struct ViewLayer *view_layer,
                                             struct Object *ob,
                                             struct ModifierData *md);
bool ED_object_modifier_apply(struct Main *bmain,
                              struct ReportList *reports,
                              struct Depsgraph *depsgraph,
                              struct Scene *scene,
                              struct Object *ob,
                              struct ModifierData *md,
                              int mode,
                              bool keep_modifier);
bool ED_object_modifier_copy(struct ReportList *reports,
                             struct Main *bmain,
                             struct Scene *scene,
                             struct Object *ob,
                             struct ModifierData *md);
void ED_object_modifier_link(struct bContext *C, struct Object *ob_dst, struct Object *ob_src);
void ED_object_modifier_copy_to_object(struct bContext *C,
                                       struct Object *ob_dst,
                                       struct Object *ob_src,
                                       struct ModifierData *md);

/**
 * If the object data of 'orig_ob' has other users, run 'callback' on
 * each of them.
 *
 * If include_orig is true, the callback will run on 'orig_ob' too.
 *
 * If the callback ever returns true, iteration will stop and the
 * function value will be true. Otherwise the function returns false.
 */
bool ED_object_iter_other(struct Main *bmain,
                          struct Object *orig_ob,
                          bool include_orig,
                          bool (*callback)(struct Object *ob, void *callback_data),
                          void *callback_data);

/**
 * Use with #ED_object_iter_other(). Sets the total number of levels
 * for any multi-res modifiers on the object to the int pointed to by callback_data.
 */
bool ED_object_multires_update_totlevels_cb(struct Object *ob, void *totlevel_v);

/* object_greasepencil_modifier.c */

struct GpencilModifierData *ED_object_gpencil_modifier_add(struct ReportList *reports,
                                                           struct Main *bmain,
                                                           struct Scene *scene,
                                                           struct Object *ob,
                                                           const char *name,
                                                           int type);
bool ED_object_gpencil_modifier_remove(struct ReportList *reports,
                                       struct Main *bmain,
                                       struct Object *ob,
                                       struct GpencilModifierData *md);
void ED_object_gpencil_modifier_clear(struct Main *bmain, struct Object *ob);
bool ED_object_gpencil_modifier_move_down(struct ReportList *reports,
                                          struct Object *ob,
                                          struct GpencilModifierData *md);
bool ED_object_gpencil_modifier_move_up(struct ReportList *reports,
                                        struct Object *ob,
                                        struct GpencilModifierData *md);
bool ED_object_gpencil_modifier_move_to_index(struct ReportList *reports,
                                              struct Object *ob,
                                              struct GpencilModifierData *md,
                                              int index);
bool ED_object_gpencil_modifier_apply(struct Main *bmain,
                                      struct ReportList *reports,
                                      struct Depsgraph *depsgraph,
                                      struct Object *ob,
                                      struct GpencilModifierData *md,
                                      int mode);
bool ED_object_gpencil_modifier_copy(struct ReportList *reports,
                                     struct Object *ob,
                                     struct GpencilModifierData *md);
void ED_object_gpencil_modifier_copy_to_object(struct Object *ob_dst,
                                               struct GpencilModifierData *md);

/* object_shader_fx.c */

struct ShaderFxData *ED_object_shaderfx_add(struct ReportList *reports,
                                            struct Main *bmain,
                                            struct Scene *scene,
                                            struct Object *ob,
                                            const char *name,
                                            int type);
bool ED_object_shaderfx_remove(struct ReportList *reports,
                               struct Main *bmain,
                               struct Object *ob,
                               struct ShaderFxData *fx);
void ED_object_shaderfx_clear(struct Main *bmain, struct Object *ob);
int ED_object_shaderfx_move_down(struct ReportList *reports,
                                 struct Object *ob,
                                 struct ShaderFxData *fx);
int ED_object_shaderfx_move_up(struct ReportList *reports,
                               struct Object *ob,
                               struct ShaderFxData *fx);
bool ED_object_shaderfx_move_to_index(struct ReportList *reports,
                                      struct Object *ob,
                                      struct ShaderFxData *fx,
                                      int index);
void ED_object_shaderfx_link(struct Object *dst, struct Object *src);
void ED_object_shaderfx_copy(struct Object *dst, struct ShaderFxData *fx);

/* object_select.c */

void ED_object_select_linked_by_id(struct bContext *C, struct ID *id);

const struct EnumPropertyItem *ED_object_vgroup_selection_itemf_helper(
    const struct bContext *C,
    struct PointerRNA *ptr,
    struct PropertyRNA *prop,
    bool *r_free,
    unsigned int selection_mask);

void ED_object_check_force_modifiers(struct Main *bmain,
                                     struct Scene *scene,
                                     struct Object *object);

/**
 * If id is not already an Object, try to find an object that uses it as data.
 * Prefers active, then selected, then visible/selectable.
 */
struct Base *ED_object_find_first_by_data_id(const struct Scene *scene,
                                             struct ViewLayer *view_layer,
                                             struct ID *id);

/**
 * Select and make the target object active in the view layer.
 * If already selected, selection isn't changed.
 *
 * \returns false if not found in current view layer
 */
bool ED_object_jump_to_object(struct bContext *C, struct Object *ob, bool reveal_hidden);
/**
 * Select and make the target object and bone active.
 * Switches to Pose mode if in Object mode so the selection is visible.
 * Un-hides the target bone and bone layer if necessary.
 *
 * \returns false if object not in layer, bone not found, or other error
 */
bool ED_object_jump_to_bone(struct bContext *C,
                            struct Object *ob,
                            const char *bone_name,
                            bool reveal_hidden);

/* object_facemap_ops.c */

/**
 * Called while not in edit-mode.
 */
void ED_object_facemap_face_add(struct Object *ob, struct bFaceMap *fmap, int facenum);
/**
 * Called while not in edit-mode.
 */
void ED_object_facemap_face_remove(struct Object *ob, struct bFaceMap *fmap, int facenum);

/* object_data_transform.c */

struct XFormObjectData *ED_object_data_xform_create_ex(struct ID *id, bool is_edit_mode);
struct XFormObjectData *ED_object_data_xform_create(struct ID *id);
struct XFormObjectData *ED_object_data_xform_create_from_edit_mode(ID *id);

void ED_object_data_xform_destroy(struct XFormObjectData *xod_base);

void ED_object_data_xform_by_mat4(struct XFormObjectData *xod, const float mat[4][4]);

void ED_object_data_xform_restore(struct XFormObjectData *xod);
void ED_object_data_xform_tag_update(struct XFormObjectData *xod);

#ifdef __cplusplus
}
#endif
