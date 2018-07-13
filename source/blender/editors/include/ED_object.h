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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_object.h
 *  \ingroup editors
 */

#ifndef __ED_OBJECT_H__
#define __ED_OBJECT_H__

#ifdef __cplusplus
extern "C" {
#endif

struct bFaceMap;
struct Base;
struct EnumPropertyItem;
struct ID;
struct Main;
struct Menu;
struct ModifierData;
struct Object;
struct ReportList;
struct Scene;
struct ViewLayer;
struct bConstraint;
struct bContext;
struct bPoseChannel;
struct wmKeyConfig;
struct wmKeyMap;
struct wmOperator;
struct wmOperatorType;
struct wmWindow;
struct wmWindowManager;
struct PointerRNA;
struct PropertyRNA;
struct EnumPropertyItem;
struct Depsgraph;
struct uiLayout;

#include "DNA_object_enums.h"
#include "BLI_compiler_attrs.h"

/* object_edit.c */
struct Object *ED_object_context(struct bContext *C);               /* context.object */
struct Object *ED_object_active_context(struct bContext *C); /* context.object or context.active_object */
void ED_hide_collections_menu_draw(const struct bContext *C, struct uiLayout *layout);

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

#ifdef __RNA_TYPES_H__
extern struct EnumPropertyItem prop_clear_parent_types[];
extern struct EnumPropertyItem prop_make_parent_types[];
#endif

bool ED_object_parent_set(struct ReportList *reports, const struct bContext *C, struct Scene *scene, struct Object *ob,
                          struct Object *par, int partype, const bool xmirror, const bool keep_transform,
                          const int vert_par[3]);
void ED_object_parent_clear(struct Object *ob, const int type);

void ED_keymap_proportional_cycle(struct wmKeyConfig *keyconf, struct wmKeyMap *keymap);
void ED_keymap_proportional_obmode(struct wmKeyConfig *keyconf, struct wmKeyMap *keymap);
void ED_keymap_proportional_maskmode(struct wmKeyConfig *keyconf, struct wmKeyMap *keymap);
void ED_keymap_proportional_editmode(struct wmKeyConfig *keyconf, struct wmKeyMap *keymap,
                                     const bool do_connected);

void ED_object_base_select(struct Base *base, eObjectSelect_Mode mode);
void ED_object_base_activate(struct bContext *C, struct Base *base);
void ED_object_base_free_and_unlink(struct Main *bmain, struct Scene *scene, struct Object *ob);

/* single object duplicate, if (dupflag == 0), fully linked, else it uses the flags given */
struct Base *ED_object_add_duplicate(struct Main *bmain, struct Scene *scene, struct ViewLayer *view_layer, struct Base *base, int dupflag);

void ED_object_parent(struct Object *ob, struct Object *parent, const int type, const char *substr);

/* bitflags for enter/exit editmode */
enum {
	EM_FREEDATA         = (1 << 0),
	EM_WAITCURSOR       = (1 << 1),
	EM_IGNORE_LAYER     = (1 << 3),
	EM_NO_CONTEXT       = (1 << 4),
};
bool ED_object_editmode_exit_ex(
        struct Main *bmain, struct Scene *scene, struct Object *obedit, int flag);
bool ED_object_editmode_exit(struct bContext *C, int flag);

bool ED_object_editmode_enter_ex(struct Main *bmain, struct Scene *scene, struct Object *ob, int flag);
bool ED_object_editmode_enter(struct bContext *C, int flag);
bool ED_object_editmode_load(struct Main *bmain, struct Object *obedit);

bool ED_object_editmode_calc_active_center(struct Object *obedit, const bool select_only, float r_center[3]);


void ED_object_vpaintmode_enter_ex(
        struct Main *bmain, struct Depsgraph *depsgraph, struct wmWindowManager *wm,
        struct Scene *scene, struct Object *ob);
void ED_object_vpaintmode_enter(struct bContext *C);
void ED_object_wpaintmode_enter_ex(
        struct Main *bmain, struct Depsgraph *depsgraph, struct wmWindowManager *wm,
        struct Scene *scene, struct Object *ob);
void ED_object_wpaintmode_enter(struct bContext *C);

void ED_object_vpaintmode_exit_ex(struct Object *ob);
void ED_object_vpaintmode_exit(struct bContext *C);
void ED_object_wpaintmode_exit_ex(struct Object *ob);
void ED_object_wpaintmode_exit(struct bContext *C);

void ED_object_sculptmode_enter_ex(
        struct Main *bmain, struct Depsgraph *depsgraph,
        struct Scene *scene, struct Object *ob,
        struct ReportList *reports);
void ED_object_sculptmode_enter(struct bContext *C, struct ReportList *reports);
void ED_object_sculptmode_exit_ex(
        struct Depsgraph *depsgraph,
        struct Scene *scene, struct Object *ob);
void ED_object_sculptmode_exit(struct bContext *C);

void ED_object_location_from_view(struct bContext *C, float loc[3]);
void ED_object_rotation_from_quat(float rot[3], const float quat[4], const char align_axis);
void ED_object_rotation_from_view(struct bContext *C, float rot[3], const char align_axis);
void ED_object_base_init_transform(struct bContext *C, struct Base *base, const float loc[3], const float rot[3]);
float ED_object_new_primitive_matrix(
        struct bContext *C, struct Object *editob,
        const float loc[3], const float rot[3], float primmat[4][4]);


/* Avoid allowing too much insane values even by typing (typos can hang/crash Blender otherwise). */
#define OBJECT_ADD_SIZE_MAXF 1.0e12f

void ED_object_add_unit_props(struct wmOperatorType *ot);
void ED_object_add_generic_props(struct wmOperatorType *ot, bool do_editmode);
void ED_object_add_mesh_props(struct wmOperatorType *ot);
bool ED_object_add_generic_get_opts(struct bContext *C, struct wmOperator *op, const char view_align_axis,
                                    float loc[3], float rot[3],
                                    bool *enter_editmode, unsigned int *layer, bool *is_view_aligned);

struct Object *ED_object_add_type(
        struct bContext *C,
        int type, const char *name, const float loc[3], const float rot[3],
        bool enter_editmode, unsigned int layer)
        ATTR_NONNULL(1) ATTR_RETURNS_NONNULL;

void ED_object_single_users(struct Main *bmain, struct Scene *scene, const bool full, const bool copy_groups);
void ED_object_single_user(struct Main *bmain, struct Scene *scene, struct Object *ob);

/* object motion paths */
void ED_objects_clear_paths(struct bContext *C, bool only_selected);
void ED_objects_recalculate_paths(struct bContext *C, struct Scene *scene);

/* constraints */
struct ListBase *get_active_constraints(struct Object *ob);
struct ListBase *get_constraint_lb(struct Object *ob, struct bConstraint *con, struct bPoseChannel **r_pchan);
struct bConstraint *get_active_constraint(struct Object *ob);

void object_test_constraints(struct Main *bmain, struct Object *ob);

void ED_object_constraint_set_active(struct Object *ob, struct bConstraint *con);
void ED_object_constraint_update(struct Main *bmain, struct Object *ob);
void ED_object_constraint_dependency_update(struct Main *bmain, struct Object *ob);

void ED_object_constraint_tag_update(struct Main *bmain, struct Object *ob, struct bConstraint *con);
void ED_object_constraint_dependency_tag_update(struct Main *bmain, struct Object *ob, struct bConstraint *con);

/* object_modes.c */
bool ED_object_mode_compat_test(const struct Object *ob, eObjectMode mode);
bool ED_object_mode_compat_set(struct bContext *C, struct Object *ob, eObjectMode mode, struct ReportList *reports);
void ED_object_mode_toggle(struct bContext *C, eObjectMode mode);
void ED_object_mode_set(struct bContext *C, eObjectMode mode);

bool ED_object_mode_generic_enter(
        struct bContext *C,
        eObjectMode object_mode);
void ED_object_mode_generic_exit(
        struct Main *bmain,
        struct Depsgraph *depsgraph,
        struct Scene *scene, struct Object *ob);
bool ED_object_mode_generic_has_data(
        struct Depsgraph *depsgraph,
        struct Object *ob);

bool ED_object_mode_generic_exists(
        struct wmWindowManager *wm, struct Object *ob,
        eObjectMode object_mode);

/* object_modifier.c */
enum {
	MODIFIER_APPLY_DATA = 1,
	MODIFIER_APPLY_SHAPE
};

struct ModifierData *ED_object_modifier_add(
        struct ReportList *reports, struct Main *bmain, struct Scene *scene,
        struct Object *ob, const char *name, int type);
bool ED_object_modifier_remove(struct ReportList *reports, struct Main *bmain,
                               struct Object *ob, struct ModifierData *md);
void ED_object_modifier_clear(struct Main *bmain, struct Object *ob);
int ED_object_modifier_move_down(struct ReportList *reports, struct Object *ob, struct ModifierData *md);
int ED_object_modifier_move_up(struct ReportList *reports, struct Object *ob, struct ModifierData *md);
int ED_object_modifier_convert(
        struct ReportList *reports, struct Main *bmain, struct Scene *scene,
        struct ViewLayer *view_layer, struct Object *ob, struct ModifierData *md);
int ED_object_modifier_apply(
        struct Main *bmain, struct ReportList *reports, struct Depsgraph *depsgraph, struct Scene *scene,
        struct Object *ob, struct ModifierData *md, int mode);
int ED_object_modifier_copy(struct ReportList *reports, struct Object *ob, struct ModifierData *md);

bool ED_object_iter_other(
        struct Main *bmain, struct Object *orig_ob, const bool include_orig,
        bool (*callback)(struct Object *ob, void *callback_data),
        void *callback_data);

bool ED_object_multires_update_totlevels_cb(struct Object *ob, void *totlevel_v);

/* object_select.c */
void ED_object_select_linked_by_id(struct bContext *C, struct ID *id);

const struct EnumPropertyItem *ED_object_vgroup_selection_itemf_helper(
        const struct bContext *C,
        struct PointerRNA *ptr,
        struct PropertyRNA *prop,
        bool *r_free,
        const unsigned int selection_mask);

void ED_object_check_force_modifiers(
        struct Main *bmain, struct Scene *scene, struct Object *object);

/* object_facemap_ops.c */
void ED_object_facemap_face_add(struct Object *ob, struct bFaceMap *fmap, int facenum);
void ED_object_facemap_face_remove(struct Object *ob, struct bFaceMap *fmap, int facenum);

#ifdef __cplusplus
}
#endif

#endif /* __ED_OBJECT_H__ */
