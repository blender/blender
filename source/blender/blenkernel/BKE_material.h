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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_MATERIAL_H__
#define __BKE_MATERIAL_H__

/** \file BKE_material.h
 *  \ingroup bke
 *  \brief General operations, lookup, etc. for materials.
 */
#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct Material;
struct ID;
struct Object;
struct Mesh;
struct MTFace;
struct Scene;

/* materials */

void init_def_material(void);
void BKE_material_free(struct Material *sc); 
void BKE_material_free_ex(struct Material *ma, bool do_id_user);
void test_object_materials(struct Main *bmain, struct ID *id);
void BKE_material_resize_object(struct Object *ob, const short totcol, bool do_id_user);
void init_material(struct Material *ma);
struct Material *BKE_material_add(struct Main *bmain, const char *name);
struct Material *BKE_material_copy(struct Material *ma);
struct Material *localize_material(struct Material *ma);
struct Material *give_node_material(struct Material *ma); /* returns node material or self */
void BKE_material_make_local(struct Material *ma);
void extern_local_matarar(struct Material **matar, short totcol);

/* UNUSED */
// void automatname(struct Material *);

/* material slots */

struct Material ***give_matarar(struct Object *ob);
short *give_totcolp(struct Object *ob);
struct Material ***give_matarar_id(struct ID *id); /* same but for ID's */
short *give_totcolp_id(struct ID *id);

enum {
	BKE_MAT_ASSIGN_USERPREF,
	BKE_MAT_ASSIGN_OBDATA,
	BKE_MAT_ASSIGN_OBJECT
};

struct Material *give_current_material(struct Object *ob, short act);
struct ID *material_from(struct Object *ob, short act);
void assign_material_id(struct ID *id, struct Material *ma, short act);
void assign_material(struct Object *ob, struct Material *ma, short act, int assign_type);
void assign_matarar(struct Object *ob, struct Material ***matar, short totcol);

short find_material_index(struct Object *ob, struct Material *ma);

bool object_add_material_slot(struct Object *ob);
bool object_remove_material_slot(struct Object *ob);

void BKE_texpaint_slot_refresh_cache(struct Material *ma, bool use_nodes);
void BKE_texpaint_slots_refresh_object(struct Object *ob, bool use_nodes);
void BKE_texpaint_slots_clear(struct Material *ma);

/* rna api */
void BKE_material_resize_id(struct ID *id, short totcol, bool do_id_user);
void BKE_material_append_id(struct ID *id, struct Material *ma);
struct Material *BKE_material_pop_id(struct ID *id, int index, bool update_data); /* index is an int because of RNA */
void BKE_material_clear_id(struct ID *id, bool update_data);
/* rendering */

void init_render_material(struct Material *, int, float *);
void init_render_materials(struct Main *, int, float *);
void end_render_material(struct Material *);
void end_render_materials(struct Main *);

bool material_in_material(struct Material *parmat, struct Material *mat);

void ramp_blend(int type, float r_col[3], const float fac, const float col[3]);

/* driver update hacks */
void material_drivers_update(struct Scene *scene, struct Material *mat, float ctime);

/* copy/paste */
void clear_matcopybuf(void);
void free_matcopybuf(void);
void copy_matcopybuf(struct Material *ma);
void paste_matcopybuf(struct Material *ma);

/* handle backward compatibility for tface/materials called from doversion */	
int do_version_tface(struct Main *main);

#ifdef __cplusplus
}
#endif

#endif
