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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

#ifndef __BKE_MATERIAL_H__
#define __BKE_MATERIAL_H__

/** \file
 * \ingroup bke
 * \brief General operations, lookup, etc. for materials.
 */
#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct Main;
struct Material;
struct Object;
struct Scene;
struct bNode;

/* Module */

void BKE_materials_init(void);
void BKE_materials_exit(void);

/* Materials */

void BKE_object_materials_test(struct Main *bmain, struct Object *ob, struct ID *id);
void BKE_objects_materials_test_all(struct Main *bmain, struct ID *id);
void BKE_object_material_resize(struct Main *bmain,
                                struct Object *ob,
                                const short totcol,
                                bool do_id_user);
void BKE_object_material_remap(struct Object *ob, const unsigned int *remap);
void BKE_object_material_remap_calc(struct Object *ob_dst,
                                    struct Object *ob_src,
                                    short *remap_src_to_dst);
struct Material *BKE_material_add(struct Main *bmain, const char *name);
struct Material *BKE_gpencil_material_add(struct Main *bmain, const char *name);
struct Material *BKE_material_copy(struct Main *bmain, const struct Material *ma);
struct Material *BKE_material_localize(struct Material *ma);
void BKE_gpencil_material_attr_init(struct Material *ma);

/* UNUSED */
// void automatname(struct Material *);

/* material slots */

struct Material ***BKE_object_material_array_p(struct Object *ob);
short *BKE_object_material_len_p(struct Object *ob);
struct Material ***BKE_id_material_array_p(struct ID *id); /* same but for ID's */
short *BKE_id_material_len_p(struct ID *id);

enum {
  /* use existing link option */
  BKE_MAT_ASSIGN_EXISTING,
  BKE_MAT_ASSIGN_USERPREF,
  BKE_MAT_ASSIGN_OBDATA,
  BKE_MAT_ASSIGN_OBJECT,
};

struct Material **BKE_object_material_get_p(struct Object *ob, short act);
struct Material *BKE_object_material_get(struct Object *ob, short act);
void BKE_id_material_assign(struct Main *bmain, struct ID *id, struct Material *ma, short act);
void BKE_object_material_assign(
    struct Main *bmain, struct Object *ob, struct Material *ma, short act, int assign_type);
void BKE_object_material_array_assign(struct Main *bmain,
                                      struct Object *ob,
                                      struct Material ***matar,
                                      short totcol);

short BKE_object_material_slot_find_index(struct Object *ob, struct Material *ma);
bool BKE_object_material_slot_add(struct Main *bmain, struct Object *ob);
bool BKE_object_material_slot_remove(struct Main *bmain, struct Object *ob);
bool BKE_object_material_slot_used(struct ID *id, short actcol);

struct Material *BKE_gpencil_material(struct Object *ob, short act);
struct MaterialGPencilStyle *BKE_gpencil_material_settings(struct Object *ob, short act);

void BKE_texpaint_slot_refresh_cache(struct Scene *scene, struct Material *ma);
void BKE_texpaint_slots_refresh_object(struct Scene *scene, struct Object *ob);
struct bNode *BKE_texpaint_slot_material_find_node(struct Material *ma, short texpaint_slot);

/* rna api */
void BKE_id_material_resize(struct Main *bmain, struct ID *id, short totcol, bool do_id_user);
void BKE_id_material_append(struct Main *bmain, struct ID *id, struct Material *ma);
struct Material *BKE_id_material_pop(struct Main *bmain,
                                     struct ID *id,
                                     /* index is an int because of RNA. */
                                     int index);
void BKE_id_material_clear(struct Main *bmain, struct ID *id);
/* rendering */

void ramp_blend(int type, float r_col[3], const float fac, const float col[3]);

/* copy/paste */
void BKE_material_copybuf_clear(void);
void BKE_material_copybuf_free(void);
void BKE_material_copybuf_copy(struct Main *bmain, struct Material *ma);
void BKE_material_copybuf_paste(struct Main *bmain, struct Material *ma);

/* Default Materials */

struct Material *BKE_material_default_empty(void);
struct Material *BKE_material_default_surface(void);
struct Material *BKE_material_default_volume(void);
struct Material *BKE_material_default_gpencil(void);

void BKE_material_defaults_free_gpu(void);

/* Dependency graph evaluation. */

struct Depsgraph;

void BKE_material_eval(struct Depsgraph *depsgraph, struct Material *material);

#ifdef __cplusplus
}
#endif

#endif
