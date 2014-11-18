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

struct BMEdge;
struct BMFace;
struct BMVert;
struct BPoint;
struct Base;
struct BezTriple;
struct Curve;
struct EditBone;
struct EnumPropertyItem;
struct ID;
struct KeyBlock;
struct Lattice;
struct Main;
struct Mesh;
struct MetaElem;
struct ModifierData;
struct HookModifierData;
struct Nurb;
struct Object;
struct ReportList;
struct Scene;
struct View3D;
struct ViewContext;
struct bConstraint;
struct bContext;
struct bPoseChannel;
struct wmEvent;
struct wmKeyConfig;
struct wmKeyMap;
struct wmOperator;
struct wmOperatorType;
struct PointerRNA;
struct PropertyRNA;
struct EnumPropertyItem;

/* object_edit.c */
struct Object *ED_object_context(struct bContext *C);               /* context.object */
struct Object *ED_object_active_context(struct bContext *C); /* context.object or context.active_object */

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
	PAR_VERTEX_TRI
} eParentType;

#ifdef __RNA_TYPES_H__
extern struct EnumPropertyItem prop_clear_parent_types[];
extern struct EnumPropertyItem prop_make_parent_types[];
#endif

bool ED_object_parent_set(struct ReportList *reports, struct Main *bmain, struct Scene *scene, struct Object *ob,
                          struct Object *par, int partype, const bool xmirror, const bool keep_transform,
                          const int vert_par[3]);
void ED_object_parent_clear(struct Object *ob, const int type);
struct Base *ED_object_scene_link(struct Scene *scene, struct Object *ob);

void ED_keymap_proportional_cycle(struct wmKeyConfig *keyconf, struct wmKeyMap *keymap);
void ED_keymap_proportional_obmode(struct wmKeyConfig *keyconf, struct wmKeyMap *keymap);
void ED_keymap_proportional_maskmode(struct wmKeyConfig *keyconf, struct wmKeyMap *keymap);
void ED_keymap_proportional_editmode(struct wmKeyConfig *keyconf, struct wmKeyMap *keymap,
                                     const bool do_connected);

/* send your own notifier for select! */
void ED_base_object_select(struct Base *base, short mode);
/* includes notifier */
void ED_base_object_activate(struct bContext *C, struct Base *base);

void ED_base_object_free_and_unlink(struct Main *bmain, struct Scene *scene, struct Base *base);

/* single object duplicate, if (dupflag == 0), fully linked, else it uses the flags given */
struct Base *ED_object_add_duplicate(struct Main *bmain, struct Scene *scene, struct Base *base, int dupflag);

void ED_object_parent(struct Object *ob, struct Object *parent, const int type, const char *substr);

bool ED_object_mode_compat_set(struct bContext *C, struct Object *ob, int mode, struct ReportList *reports);
void ED_object_toggle_modes(struct bContext *C, int mode);

/* bitflags for enter/exit editmode */
#define EM_FREEDATA     1
#define EM_FREEUNDO     2
#define EM_WAITCURSOR   4
#define EM_DO_UNDO      8
#define EM_IGNORE_LAYER 16
void ED_object_editmode_exit(struct bContext *C, int flag);
void ED_object_editmode_enter(struct bContext *C, int flag);
bool ED_object_editmode_load(struct Object *obedit);

void ED_object_location_from_view(struct bContext *C, float loc[3]);
void ED_object_rotation_from_view(struct bContext *C, float rot[3], const char align_axis);
void ED_object_base_init_transform(struct bContext *C, struct Base *base, const float loc[3], const float rot[3]);
float ED_object_new_primitive_matrix(struct bContext *C, struct Object *editob,
                                     const float loc[3], const float rot[3], float primmat[4][4],
                                     bool apply_diameter);

void ED_object_add_unit_props(struct wmOperatorType *ot);
void ED_object_add_generic_props(struct wmOperatorType *ot, bool do_editmode);
bool ED_object_add_generic_get_opts(struct bContext *C, struct wmOperator *op, const char view_align_axis,
                                    float loc[3], float rot[3],
                                    bool *enter_editmode, unsigned int *layer, bool *is_view_aligned);

struct Object *ED_object_add_type(
        struct bContext *C, int type, const float loc[3], const float rot[3],
        bool enter_editmode, unsigned int layer) ATTR_RETURNS_NONNULL;

void ED_object_single_users(struct Main *bmain, struct Scene *scene, const bool full, const bool copy_groups);
void ED_object_single_user(struct Main *bmain, struct Scene *scene, struct Object *ob);

/* object motion paths */
void ED_objects_clear_paths(struct bContext *C);
void ED_objects_recalculate_paths(struct bContext *C, struct Scene *scene);

/* constraints */
struct ListBase *get_active_constraints(struct Object *ob);
struct ListBase *get_constraint_lb(struct Object *ob, struct bConstraint *con, struct bPoseChannel **r_pchan);
struct bConstraint *get_active_constraint(struct Object *ob);

void object_test_constraints(struct Object *ob);

void ED_object_constraint_set_active(struct Object *ob, struct bConstraint *con);
void ED_object_constraint_update(struct Object *ob);
void ED_object_constraint_dependency_update(struct Main *bmain, struct Object *ob);

/* object_lattice.c */
bool mouse_lattice(struct bContext *C, const int mval[2], bool extend, bool deselect, bool toggle);
void undo_push_lattice(struct bContext *C, const char *name);

/* object_lattice.c */

void ED_setflagsLatt(struct Object *obedit, int flag);

/* object_modifier.c */
enum {
	MODIFIER_APPLY_DATA = 1,
	MODIFIER_APPLY_SHAPE
};

struct ModifierData *ED_object_modifier_add(struct ReportList *reports, struct Main *bmain, struct Scene *scene,
                                            struct Object *ob, const char *name, int type);
bool ED_object_modifier_remove(struct ReportList *reports, struct Main *bmain,
                               struct Object *ob, struct ModifierData *md);
void ED_object_modifier_clear(struct Main *bmain, struct Object *ob);
int ED_object_modifier_move_down(struct ReportList *reports, struct Object *ob, struct ModifierData *md);
int ED_object_modifier_move_up(struct ReportList *reports, struct Object *ob, struct ModifierData *md);
int ED_object_modifier_convert(struct ReportList *reports, struct Main *bmain, struct Scene *scene,
                               struct Object *ob, struct ModifierData *md);
int ED_object_modifier_apply(struct ReportList *reports, struct Scene *scene,
                             struct Object *ob, struct ModifierData *md, int mode);
int ED_object_modifier_copy(struct ReportList *reports, struct Object *ob, struct ModifierData *md);

bool ED_object_iter_other(struct Main *bmain, struct Object *orig_ob, const bool include_orig,
                          bool (*callback)(struct Object *ob, void *callback_data),
                          void *callback_data);

bool ED_object_multires_update_totlevels_cb(struct Object *ob, void *totlevel_v);

/* object_select.c */
void ED_object_select_linked_by_id(struct bContext *C, struct ID *id);

struct EnumPropertyItem *ED_object_vgroup_selection_itemf_helper(
        const struct bContext *C,
        struct PointerRNA *ptr,
        struct PropertyRNA *prop,
        bool *r_free,
        const unsigned int selection_mask);

void ED_object_check_force_modifiers(struct Main *bmain, struct Scene *scene, struct Object *object);

#ifdef __cplusplus
}
#endif

#endif /* __ED_OBJECT_H__ */
