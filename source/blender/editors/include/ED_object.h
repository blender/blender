/**
 * $Id:
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

struct wmWindowManager;
struct Scene;
struct Object;
struct bContext;
struct Base;
struct View3D;
struct bConstraint;
struct KeyBlock;
struct Lattice;
struct Mesh;
struct Curve;
struct ReportList;
struct ModifierData;

/* object_edit.c */
void ED_operatortypes_object(void);
void ED_keymap_object(struct wmWindowManager *wm);

	/* send your own notifier for select! */
void ED_base_object_select(struct Base *base, short mode);
	/* includes notifier */
void ED_base_object_activate(struct bContext *C, struct Base *base);

void ED_base_object_free_and_unlink(struct Scene *scene, struct Base *base);

void ED_object_apply_obmat(struct Object *ob);
	/* single object duplicate, if dupflag==0, fully linked, else it uses the flags given */
struct Base *ED_object_add_duplicate(struct Scene *scene, struct Base *base, int dupflag);

void ED_object_parent(struct Object *ob, struct Object *parent, int type, const char *substr);

void ED_object_toggle_modes(struct bContext *C, int mode);

/* bitflags for enter/exit editmode */
#define EM_FREEDATA		1
#define EM_FREEUNDO		2
#define EM_WAITCURSOR	4
#define EM_DO_UNDO		8
void ED_object_exit_editmode(struct bContext *C, int flag);
void ED_object_enter_editmode(struct bContext *C, int flag);

void ED_object_base_init_from_view(struct bContext *C, struct Base *base);

void ED_object_single_users(struct Scene *scene, int full);

/* cleanup */
int object_is_libdata(struct Object *ob);
int object_data_is_libdata(struct Object *ob);

/* constraints */
struct bConstraint *add_new_constraint(short type);
void add_constraint_to_object(struct bConstraint *con, struct Object *ob);

struct ListBase *get_active_constraints(struct Object *ob);
struct bConstraint *get_active_constraint(struct Object *ob);

void object_test_constraints(struct Object *ob);

void ED_object_constraint_rename(struct Object *ob, struct bConstraint *con, char *oldname);
void ED_object_constraint_set_active(struct Object *ob, struct bConstraint *con);

/* object_lattice.c */
void mouse_lattice(struct bContext *C, short mval[2], int extend);
void undo_push_lattice(struct bContext *C, char *name);

/* object_shapekey.c */
void insert_shapekey(struct Scene *scene, struct Object *ob);
void delete_key(struct Scene *scene, struct Object *ob);
void key_to_mesh(struct KeyBlock *kb, struct Mesh *me);
void mesh_to_key(struct Mesh *me, struct KeyBlock *kb);
void key_to_latt(struct KeyBlock *kb, struct Lattice *lt);
void latt_to_key(struct Lattice *lt, struct KeyBlock *kb);
void key_to_curve(struct KeyBlock *kb, struct Curve  *cu, struct ListBase *nurb);
void curve_to_key(struct Curve *cu, struct KeyBlock *kb, struct ListBase *nurb);

/* object_modifier.c */
int ED_object_modifier_add(struct ReportList *reports, struct Scene *scene, struct Object *ob, int type);
int ED_object_modifier_remove(struct ReportList *reports, struct Scene *scene, struct Object *ob, struct ModifierData *md);
int ED_object_modifier_move_down(struct ReportList *reports, struct Object *ob, struct ModifierData *md);
int ED_object_modifier_move_up(struct ReportList *reports, struct Object *ob, struct ModifierData *md);
int ED_object_modifier_convert(struct ReportList *reports, struct Scene *scene, struct Object *ob, struct ModifierData *md);
int ED_object_modifier_apply(struct ReportList *reports, struct Scene *scene, struct Object *ob, struct ModifierData *md);
int ED_object_modifier_copy(struct ReportList *reports, struct Object *ob, struct ModifierData *md);

#endif /* ED_OBJECT_H */

