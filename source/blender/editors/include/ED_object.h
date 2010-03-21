/**
 * $Id$
 *
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
#ifndef ED_OBJECT_H
#define ED_OBJECT_H

struct wmKeyConfig;
struct wmKeyMap;
struct Scene;
struct Object;
struct bContext;
struct Base;
struct View3D;
struct bConstraint;
struct bPoseChannel;
struct KeyBlock;
struct Lattice;
struct Mesh;
struct Curve;
struct ReportList;
struct ModifierData;
struct wmOperatorType;
struct wmOperator;
struct wmEvent;

/* object_edit.c */
void ED_operatortypes_object(void);
void ED_operatormacros_object(void);
void ED_keymap_object(struct wmKeyConfig *keyconf);

/* generic editmode keys like pet */
void ED_object_generic_keymap(struct wmKeyConfig *keyconf, struct wmKeyMap *keymap, int do_pet);

	/* send your own notifier for select! */
void ED_base_object_select(struct Base *base, short mode);
	/* includes notifier */
void ED_base_object_activate(struct bContext *C, struct Base *base);

void ED_base_object_free_and_unlink(struct Scene *scene, struct Base *base);

	/* single object duplicate, if dupflag==0, fully linked, else it uses the flags given */
struct Base *ED_object_add_duplicate(struct Scene *scene, struct Base *base, int dupflag);

void ED_object_parent(struct Object *ob, struct Object *parent, int type, const char *substr);

void ED_object_toggle_modes(struct bContext *C, int mode);

/* bitflags for enter/exit editmode */
#define EM_FREEDATA		1
#define EM_FREEUNDO		2
#define EM_WAITCURSOR	4
#define EM_DO_UNDO		8
#define EM_IGNORE_LAYER	16
void ED_object_exit_editmode(struct bContext *C, int flag);
void ED_object_enter_editmode(struct bContext *C, int flag);

void ED_object_location_from_view(struct bContext *C, float *loc);
void ED_object_rotation_from_view(struct bContext *C, float *rot);
void ED_object_base_init_transform(struct bContext *C, struct Base *base, float *loc, float *rot);
float ED_object_new_primitive_matrix(struct bContext *C, float *loc, float *rot, float primmat[][4]);

void ED_object_add_generic_props(struct wmOperatorType *ot, int do_editmode);
int ED_object_add_generic_invoke(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
void ED_object_add_generic_get_opts(struct wmOperator *op, float *loc, float *rot, int *enter_editmode, unsigned int *layer);
struct Object *ED_object_add_type(struct bContext *C, int type, float *loc, float *rot, int enter_editmode, unsigned int layer);

void ED_object_single_users(struct Scene *scene, int full);

/* cleanup */
int object_is_libdata(struct Object *ob);
int object_data_is_libdata(struct Object *ob);

/* object motion paths */
void ED_objects_clear_paths(struct bContext *C, struct Scene *scene);
void ED_objects_recalculate_paths(struct bContext *C, struct Scene *scene);

/* constraints */
struct ListBase *get_active_constraints(struct Object *ob);
struct ListBase *get_constraint_lb(struct Object *ob, struct bConstraint *con, struct bPoseChannel **pchan_r);
struct bConstraint *get_active_constraint(struct Object *ob);

void object_test_constraints(struct Object *ob);

void ED_object_constraint_set_active(struct Object *ob, struct bConstraint *con);
void ED_object_constraint_update(struct Object *ob);
void ED_object_constraint_dependency_update(struct Scene *scene, struct Object *ob);

/* object_lattice.c */
int  mouse_lattice(struct bContext *C, short mval[2], int extend);
void undo_push_lattice(struct bContext *C, char *name);

/* object_lattice.c */

void ED_setflagsLatt(struct Object *obedit, int flag);

/* object_modifier.c */
enum {
	MODIFIER_APPLY_DATA=1,
	MODIFIER_APPLY_SHAPE,
} eModifier_Apply_Mode;

struct ModifierData *ED_object_modifier_add(struct ReportList *reports, struct Scene *scene, struct Object *ob, char *name, int type);
int ED_object_modifier_remove(struct ReportList *reports, struct Scene *scene, struct Object *ob, struct ModifierData *md);
int ED_object_modifier_move_down(struct ReportList *reports, struct Object *ob, struct ModifierData *md);
int ED_object_modifier_move_up(struct ReportList *reports, struct Object *ob, struct ModifierData *md);
int ED_object_modifier_convert(struct ReportList *reports, struct Scene *scene, struct Object *ob, struct ModifierData *md);
int ED_object_modifier_apply(struct ReportList *reports, struct Scene *scene, struct Object *ob, struct ModifierData *md, int mode);
int ED_object_modifier_copy(struct ReportList *reports, struct Object *ob, struct ModifierData *md);

#endif /* ED_OBJECT_H */

