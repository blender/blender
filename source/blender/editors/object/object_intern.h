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
#ifndef ED_OBJECT_INTERN_H
#define ED_OBJECT_INTERN_H

struct wmOperatorType;
struct KeyBlock;
struct Lattice;
struct Curve;
struct Object;
struct Mesh;

/* internal exports only */
#define CLEAR_OBJ_ROTATION 0
#define CLEAR_OBJ_LOCATION 1
#define CLEAR_OBJ_SCALE 2
#define CLEAR_OBJ_ORIGIN 3

/* object_edit.c */
void OBJECT_OT_editmode_toggle(struct wmOperatorType *ot);
void OBJECT_OT_parent_set(struct wmOperatorType *ot);
void OBJECT_OT_parent_clear(struct wmOperatorType *ot);
void OBJECT_OT_track_set(struct wmOperatorType *ot);
void OBJECT_OT_track_clear(struct wmOperatorType *ot);
void OBJECT_OT_selection_de_select_all(struct wmOperatorType *ot);
void OBJECT_OT_selection_invert(struct wmOperatorType *ot);
void OBJECT_OT_selection_random(struct wmOperatorType *ot);
void OBJECT_OT_selection_by_type(struct wmOperatorType *ot);
void OBJECT_OT_selection_by_layer(struct wmOperatorType *ot);
void OBJECT_OT_selection_linked(struct wmOperatorType *ot);
void OBJECT_OT_location_clear(struct wmOperatorType *ot);
void OBJECT_OT_rotation_clear(struct wmOperatorType *ot);
void OBJECT_OT_scale_clear(struct wmOperatorType *ot);
void OBJECT_OT_origin_clear(struct wmOperatorType *ot);
void OBJECT_OT_restrictview_set(struct wmOperatorType *ot);
void OBJECT_OT_restrictview_clear(struct wmOperatorType *ot);
void OBJECT_OT_slowparent_set(struct wmOperatorType *ot);
void OBJECT_OT_slowparent_clear(struct wmOperatorType *ot);
void OBJECT_OT_center_set(struct wmOperatorType *ot);
void OBJECT_OT_dupli_set_real(struct wmOperatorType *ot);
void OBJECT_OT_object_add(struct wmOperatorType *ot);
void OBJECT_OT_duplicate_add(struct wmOperatorType *ot);
void OBJECT_OT_object_delete(struct wmOperatorType *ot);

void OBJECT_OT_mesh_add(struct wmOperatorType *ot);
void OBJECT_OT_curve_add(struct wmOperatorType *ot);
void OBJECT_OT_surface_add(struct wmOperatorType *ot);
void OBJECT_OT_text_add(struct wmOperatorType *ot);
void OBJECT_OT_armature_add(struct wmOperatorType *ot);
	/* only used as menu */
void OBJECT_OT_primitive_add(struct wmOperatorType *ot);

/* editlattice.c */
void free_editLatt(Object *ob);
void make_editLatt(Object *obedit);
void load_editLatt(Object *obedit);
void remake_editLatt(Object *obedit);

/* editgroup.c */
void GROUP_OT_group_create(struct wmOperatorType *ot);
void GROUP_OT_group_remove(struct wmOperatorType *ot);
void GROUP_OT_objects_add_active(struct wmOperatorType *ot);
void GROUP_OT_objects_remove_active(struct wmOperatorType *ot);

#endif /* ED_OBJECT_INTERN_H */

