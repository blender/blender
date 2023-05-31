/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

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

/* -------------------------------------------------------------------- */
/** \name Module
 * \{ */

void BKE_materials_init(void);
void BKE_materials_exit(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Materials
 * \{ */

void BKE_object_materials_test(struct Main *bmain, struct Object *ob, struct ID *id);
void BKE_objects_materials_test_all(struct Main *bmain, struct ID *id);
void BKE_object_material_resize(struct Main *bmain,
                                struct Object *ob,
                                short totcol,
                                bool do_id_user);
void BKE_object_material_remap(struct Object *ob, const unsigned int *remap);
/**
 * Calculate a material remapping from \a ob_src to \a ob_dst.
 *
 * \param remap_src_to_dst: An array the size of `ob_src->totcol`
 * where index values are filled in which map to \a ob_dst materials.
 */
void BKE_object_material_remap_calc(struct Object *ob_dst,
                                    struct Object *ob_src,
                                    short *remap_src_to_dst);
/**
 * Copy materials from evaluated geometry to the original geometry of an object.
 */
void BKE_object_material_from_eval_data(struct Main *bmain,
                                        struct Object *ob_orig,
                                        const struct ID *data_eval);
struct Material *BKE_material_add(struct Main *bmain, const char *name);
struct Material *BKE_gpencil_material_add(struct Main *bmain, const char *name);
void BKE_gpencil_material_attr_init(struct Material *ma);

/* UNUSED */
// void automatname(struct Material *);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Slots
 * \{ */

struct Material ***BKE_object_material_array_p(struct Object *ob);
short *BKE_object_material_len_p(struct Object *ob);
/**
 * \note Same as #BKE_object_material_len_p but for ID's.
 */
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

/**
 * Similar to #BKE_object_material_assign with #BKE_MAT_ASSIGN_OBDATA type,
 * but does not scan whole Main for other usages of the same obdata. Only
 * use in cases where you know that the object's obdata is only used by this one
 * object.
 */
void BKE_object_material_assign_single_obdata(struct Main *bmain,
                                              struct Object *ob,
                                              struct Material *ma,
                                              short act);
/**
 * \warning this calls many more update calls per object then are needed, could be optimized.
 */
void BKE_object_material_array_assign(struct Main *bmain,
                                      struct Object *ob,
                                      struct Material ***matar,
                                      int totcol,
                                      bool to_object_only);

short BKE_object_material_slot_find_index(struct Object *ob, struct Material *ma);
bool BKE_object_material_slot_add(struct Main *bmain, struct Object *ob);
bool BKE_object_material_slot_remove(struct Main *bmain, struct Object *ob);
bool BKE_object_material_slot_used(struct Object *object, short actcol);

struct Material *BKE_gpencil_material(struct Object *ob, short act);
struct MaterialGPencilStyle *BKE_gpencil_material_settings(struct Object *ob, short act);

void BKE_texpaint_slot_refresh_cache(struct Scene *scene,
                                     struct Material *ma,
                                     const struct Object *ob);
void BKE_texpaint_slots_refresh_object(struct Scene *scene, struct Object *ob);
struct bNode *BKE_texpaint_slot_material_find_node(struct Material *ma, short texpaint_slot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name RNA API
 * \{ */

void BKE_id_materials_copy(struct Main *bmain, struct ID *id_src, struct ID *id_dst);
void BKE_id_material_resize(struct Main *bmain, struct ID *id, short totcol, bool do_id_user);
void BKE_id_material_append(struct Main *bmain, struct ID *id, struct Material *ma);
struct Material *BKE_id_material_pop(struct Main *bmain,
                                     struct ID *id,
                                     /* index is an int because of RNA. */
                                     int index);
void BKE_id_material_clear(struct Main *bmain, struct ID *id);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Evaluation API
 * \{ */

/**
 * On evaluated objects the number of materials on an object and its data might go out of sync.
 * This is because during evaluation materials can be added/removed on the object data.
 *
 * For rendering or exporting we generally use the materials on the object data. However, some
 * material indices might be overwritten by the object.
 */
struct Material *BKE_object_material_get_eval(struct Object *ob, short act);
int BKE_object_material_count_eval(struct Object *ob);
void BKE_id_material_eval_assign(struct ID *id, int slot, struct Material *material);
/**
 * Add an empty material slot if the id has no material slots. This material slot allows the
 * material to be overwritten by object-linked materials.
 */
void BKE_id_material_eval_ensure_default_slot(struct ID *id);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rendering
 * \{ */

/**
 * \param r_col: current value.
 * \param col: new value.
 * \param fac: Zero for is no change.
 */
void ramp_blend(int type, float r_col[3], float fac, const float col[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy/Paste
 * \{ */

void BKE_material_copybuf_clear(void);
void BKE_material_copybuf_free(void);
void BKE_material_copybuf_copy(struct Main *bmain, struct Material *ma);
/**
 * \return true when the material was modified.
 */
bool BKE_material_copybuf_paste(struct Main *bmain, struct Material *ma);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Default Materials
 * \{ */

struct Material *BKE_material_default_empty(void);
struct Material *BKE_material_default_holdout(void);
struct Material *BKE_material_default_surface(void);
struct Material *BKE_material_default_volume(void);
struct Material *BKE_material_default_gpencil(void);

void BKE_material_defaults_free_gpu(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dependency graph evaluation
 * \{ */

struct Depsgraph;

void BKE_material_eval(struct Depsgraph *depsgraph, struct Material *material);

/** \} */

#ifdef __cplusplus
}
#endif
