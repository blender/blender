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
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#ifndef __ED_OBJECT_H__
#define __ED_OBJECT_H__

#include "BLI_compiler_attrs.h"
#include "DNA_object_enums.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Base;
struct Depsgraph;
struct EnumPropertyItem;
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
struct wmWindowManager;

/* object_edit.c */
/* context.object */
struct Object *ED_object_context(struct bContext *C);
/* context.object or context.active_object */
struct Object *ED_object_active_context(struct bContext *C);
void ED_collection_hide_menu_draw(const struct bContext *C, struct uiLayout *layout);

/* object_utils.c */
bool ED_object_calc_active_center_for_editmode(struct Object *obedit,
                                               const bool select_only,
                                               float r_center[3]);
bool ED_object_calc_active_center_for_posemode(struct Object *ob,
                                               const bool select_only,
                                               float r_center[3]);
bool ED_object_calc_active_center(struct Object *ob, const bool select_only, float r_center[3]);

/* Object Data Container helper API. */
struct XFormObjectData_Container;
struct XFormObjectData_Container *ED_object_data_xform_container_create(void);
void ED_object_data_xform_container_destroy(struct XFormObjectData_Container *xds);
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

bool ED_object_parent_set(struct ReportList *reports,
                          const struct bContext *C,
                          struct Scene *scene,
                          struct Object *ob,
                          struct Object *par,
                          int partype,
                          const bool xmirror,
                          const bool keep_transform,
                          const int vert_par[3]);
void ED_object_parent_clear(struct Object *ob, const int type);

void ED_object_base_select(struct Base *base, eObjectSelect_Mode mode);
void ED_object_base_activate(struct bContext *C, struct Base *base);
void ED_object_base_active_refresh(struct Main *bmain,
                                   struct Scene *scene,
                                   struct ViewLayer *view_layer);
void ED_object_base_free_and_unlink(struct Main *bmain, struct Scene *scene, struct Object *ob);
bool ED_object_base_deselect_all_ex(struct ViewLayer *view_layer,
                                    struct View3D *v3d,
                                    int action,
                                    bool *r_any_visible);
bool ED_object_base_deselect_all(struct ViewLayer *view_layer, struct View3D *v3d, int action);

/* single object duplicate, if (dupflag == 0), fully linked, else it uses the flags given */
struct Base *ED_object_add_duplicate(struct Main *bmain,
                                     struct Scene *scene,
                                     struct ViewLayer *view_layer,
                                     struct Base *base,
                                     int dupflag);

void ED_object_parent(struct Object *ob,
                      struct Object *parent,
                      const int type,
                      const char *substr);

/* bitflags for enter/exit editmode */
enum {
  EM_FREEDATA = (1 << 0),
  EM_NO_CONTEXT = (1 << 1),
};
bool ED_object_editmode_exit_ex(struct Main *bmain,
                                struct Scene *scene,
                                struct Object *obedit,
                                int flag);
bool ED_object_editmode_exit(struct bContext *C, int flag);

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

void ED_object_texture_paint_mode_enter_ex(struct Main *bmain, struct Scene *scene, Object *ob);
void ED_object_texture_paint_mode_enter(struct bContext *C);

void ED_object_texture_paint_mode_exit_ex(struct Main *bmain, struct Scene *scene, Object *ob);
void ED_object_texture_paint_mode_exit(struct bContext *C);

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
                                   const bool force_dyntopo,
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
void ED_object_rotation_from_quat(float rot[3], const float quat[4], const char align_axis);
void ED_object_rotation_from_view(struct bContext *C, float rot[3], const char align_axis);
void ED_object_base_init_transform_on_add(struct Object *object,
                                          const float loc[3],
                                          const float rot[3]);
float ED_object_new_primitive_matrix(struct bContext *C,
                                     struct Object *editob,
                                     const float loc[3],
                                     const float rot[3],
                                     float primmat[4][4]);

/* Avoid allowing too much insane values even by typing
 * (typos can hang/crash Blender otherwise). */
#define OBJECT_ADD_SIZE_MAXF 1.0e12f

void ED_object_add_unit_props_size(struct wmOperatorType *ot);
void ED_object_add_unit_props_radius_ex(struct wmOperatorType *ot, float default_value);
void ED_object_add_unit_props_radius(struct wmOperatorType *ot);
void ED_object_add_generic_props(struct wmOperatorType *ot, bool do_editmode);
void ED_object_add_mesh_props(struct wmOperatorType *ot);
bool ED_object_add_generic_get_opts(struct bContext *C,
                                    struct wmOperator *op,
                                    const char view_align_axis,
                                    float loc[3],
                                    float rot[3],
                                    float scale[3],
                                    bool *enter_editmode,
                                    unsigned short *local_view_bits,
                                    bool *is_view_aligned);

struct Object *ED_object_add_type(struct bContext *C,
                                  int type,
                                  const char *name,
                                  const float loc[3],
                                  const float rot[3],
                                  bool enter_editmode,
                                  unsigned short local_view_bits)
    ATTR_NONNULL(1) ATTR_RETURNS_NONNULL;

void ED_object_single_user(struct Main *bmain, struct Scene *scene, struct Object *ob);

/* object motion paths */
void ED_objects_clear_paths(struct bContext *C, bool only_selected);

/* Corresponds to eAnimvizCalcRange. */
typedef enum eObjectPathCalcRange {
  OBJECT_PATH_CALC_RANGE_CURRENT_FRAME,
  OBJECT_PATH_CALC_RANGE_CHANGED,
  OBJECT_PATH_CALC_RANGE_FULL,
} eObjectPathCalcRange;

void ED_objects_recalculate_paths(struct bContext *C,
                                  struct Scene *scene,
                                  eObjectPathCalcRange range);

/* constraints */
struct ListBase *ED_object_constraint_list_from_context(struct Object *ob);
struct ListBase *ED_object_constraint_list_from_constraint(struct Object *ob,
                                                           struct bConstraint *con,
                                                           struct bPoseChannel **r_pchan);
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

/* object_modes.c */
bool ED_object_mode_compat_test(const struct Object *ob, eObjectMode mode);
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
bool ED_object_mode_generic_has_data(struct Depsgraph *depsgraph, struct Object *ob);

/* object_modifier.c */
enum {
  MODIFIER_APPLY_DATA = 1,
  MODIFIER_APPLY_SHAPE,
};

struct ModifierData *ED_object_modifier_add(struct ReportList *reports,
                                            struct Main *bmain,
                                            struct Scene *scene,
                                            struct Object *ob,
                                            const char *name,
                                            int type);
bool ED_object_modifier_remove(struct ReportList *reports,
                               struct Main *bmain,
                               struct Object *ob,
                               struct ModifierData *md);
void ED_object_modifier_clear(struct Main *bmain, struct Object *ob);
bool ED_object_modifier_move_down(struct ReportList *reports,
                                  struct Object *ob,
                                  struct ModifierData *md);
bool ED_object_modifier_move_up(struct ReportList *reports,
                                struct Object *ob,
                                struct ModifierData *md);
bool ED_object_modifier_move_to_index(struct ReportList *reports,
                                      struct Object *ob,
                                      struct ModifierData *md,
                                      const int index);

int ED_object_modifier_convert(struct ReportList *reports,
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
                              int mode);
int ED_object_modifier_copy(struct ReportList *reports,
                            struct Object *ob,
                            struct ModifierData *md);

bool ED_object_iter_other(struct Main *bmain,
                          struct Object *orig_ob,
                          const bool include_orig,
                          bool (*callback)(struct Object *ob, void *callback_data),
                          void *callback_data);

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
int ED_object_gpencil_modifier_move_down(struct ReportList *reports,
                                         struct Object *ob,
                                         struct GpencilModifierData *md);
int ED_object_gpencil_modifier_move_up(struct ReportList *reports,
                                       struct Object *ob,
                                       struct GpencilModifierData *md);
int ED_object_gpencil_modifier_apply(struct Main *bmain,
                                     struct ReportList *reports,
                                     struct Depsgraph *depsgraph,
                                     struct Object *ob,
                                     struct GpencilModifierData *md,
                                     int mode);
int ED_object_gpencil_modifier_copy(struct ReportList *reports,
                                    struct Object *ob,
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

/* object_select.c */
void ED_object_select_linked_by_id(struct bContext *C, struct ID *id);

const struct EnumPropertyItem *ED_object_vgroup_selection_itemf_helper(
    const struct bContext *C,
    struct PointerRNA *ptr,
    struct PropertyRNA *prop,
    bool *r_free,
    const unsigned int selection_mask);

void ED_object_check_force_modifiers(struct Main *bmain,
                                     struct Scene *scene,
                                     struct Object *object);

struct Base *ED_object_find_first_by_data_id(struct ViewLayer *view_layer, struct ID *id);

bool ED_object_jump_to_object(struct bContext *C, struct Object *ob, const bool reveal_hidden);
bool ED_object_jump_to_bone(struct bContext *C,
                            struct Object *ob,
                            const char *bone_name,
                            const bool reveal_hidden);

/* object_facemap_ops.c */
void ED_object_facemap_face_add(struct Object *ob, struct bFaceMap *fmap, int facenum);
void ED_object_facemap_face_remove(struct Object *ob, struct bFaceMap *fmap, int facenum);

/* object_data_transform.c */
struct XFormObjectData *ED_object_data_xform_create_ex(struct ID *id, bool is_edit_mode);
struct XFormObjectData *ED_object_data_xform_create(struct ID *id);
struct XFormObjectData *ED_object_data_xform_create_from_edit_mode(ID *id);

void ED_object_data_xform_destroy(struct XFormObjectData *xod);

void ED_object_data_xform_by_mat4(struct XFormObjectData *xod, const float mat[4][4]);

void ED_object_data_xform_restore(struct XFormObjectData *xod);
void ED_object_data_xform_tag_update(struct XFormObjectData *xod);

#ifdef __cplusplus
}
#endif

#endif /* __ED_OBJECT_H__ */
