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
#pragma once

/** \file
 * \ingroup bke
 */
struct Curve;
struct ID;
struct Key;
struct KeyBlock;
struct Lattice;
struct ListBase;
struct Main;
struct Mesh;
struct Object;

/* Kernel prototypes */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Free (or release) any data used by this shapekey (does not free the key itself).
 */
void BKE_key_free_data(struct Key *key);
void BKE_key_free_nolib(struct Key *key);
struct Key *BKE_key_add(struct Main *bmain, struct ID *id);
/**
 * Sort shape keys after a change.
 * This assumes that at most one key was moved,
 * which is a valid assumption for the places it's currently being called.
 */
void BKE_key_sort(struct Key *key);

void key_curve_position_weights(float t, float data[4], int type);
/**
 * First derivative.
 */
void key_curve_tangent_weights(float t, float data[4], int type);
/**
 * Second derivative.
 */
void key_curve_normal_weights(float t, float data[4], int type);

/**
 * Returns key coordinates (+ tilt) when key applied, NULL otherwise.
 */
float *BKE_key_evaluate_object_ex(struct Object *ob, int *r_totelem, float *arr, size_t arr_size);
float *BKE_key_evaluate_object(struct Object *ob, int *r_totelem);

/**
 * \param shape_index: The index to use or all (when -1).
 */
int BKE_keyblock_element_count_from_shape(const struct Key *key, int shape_index);
int BKE_keyblock_element_count(const struct Key *key);

/**
 * \param shape_index: The index to use or all (when -1).
 */
size_t BKE_keyblock_element_calc_size_from_shape(const struct Key *key, int shape_index);
size_t BKE_keyblock_element_calc_size(const struct Key *key);

bool BKE_key_idtype_support(short id_type);

struct Key **BKE_key_from_id_p(struct ID *id);
struct Key *BKE_key_from_id(struct ID *id);
struct Key **BKE_key_from_object_p(const struct Object *ob);
struct Key *BKE_key_from_object(const struct Object *ob);
/**
 * Only the active key-block.
 */
struct KeyBlock *BKE_keyblock_from_object(struct Object *ob);
struct KeyBlock *BKE_keyblock_from_object_reference(struct Object *ob);

struct KeyBlock *BKE_keyblock_add(struct Key *key, const char *name);
/**
 * \note sorting is a problematic side effect in some cases,
 * better only do this explicitly by having its own function,
 *
 * \param key: The key datablock to add to.
 * \param name: Optional name for the new keyblock.
 * \param do_force: always use ctime even for relative keys.
 */
struct KeyBlock *BKE_keyblock_add_ctime(struct Key *key, const char *name, bool do_force);
/**
 * Get the appropriate #KeyBlock given an index.
 */
struct KeyBlock *BKE_keyblock_from_key(struct Key *key, int index);
/**
 * Get the appropriate #KeyBlock given a name to search for.
 */
struct KeyBlock *BKE_keyblock_find_name(struct Key *key, const char name[]);
/**
 * \brief copy shape-key attributes, but not key data or name/UID.
 */
void BKE_keyblock_copy_settings(struct KeyBlock *kb_dst, const struct KeyBlock *kb_src);
/**
 * Get RNA-Path for 'value' setting of the given shape-key.
 * \note the user needs to free the returned string once they're finished with it.
 */
char *BKE_keyblock_curval_rnapath_get(struct Key *key, struct KeyBlock *kb);

/* conversion functions */
/* NOTE: 'update_from' versions do not (re)allocate mem in kb, while 'convert_from' do. */

void BKE_keyblock_update_from_lattice(struct Lattice *lt, struct KeyBlock *kb);
void BKE_keyblock_convert_from_lattice(struct Lattice *lt, struct KeyBlock *kb);
void BKE_keyblock_convert_to_lattice(struct KeyBlock *kb, struct Lattice *lt);

int BKE_keyblock_curve_element_count(const struct ListBase *nurb);
void BKE_keyblock_curve_data_transform(const struct ListBase *nurb,
                                       const float mat[4][4],
                                       const void *src,
                                       void *dst);
void BKE_keyblock_update_from_curve(struct Curve *cu, struct KeyBlock *kb, struct ListBase *nurb);
void BKE_keyblock_convert_from_curve(struct Curve *cu, struct KeyBlock *kb, struct ListBase *nurb);
void BKE_keyblock_convert_to_curve(struct KeyBlock *kb, struct Curve *cu, struct ListBase *nurb);

void BKE_keyblock_update_from_mesh(struct Mesh *me, struct KeyBlock *kb);
void BKE_keyblock_convert_from_mesh(struct Mesh *me, struct Key *key, struct KeyBlock *kb);
void BKE_keyblock_convert_to_mesh(struct KeyBlock *kb, struct Mesh *me);
/**
 * Computes normals (vertices, polygons and/or loops ones) of given mesh for given shape key.
 *
 * \param kb: the KeyBlock to use to compute normals.
 * \param mesh: the Mesh to apply key-block to.
 * \param r_vertnors: if non-NULL, an array of vectors, same length as number of vertices.
 * \param r_polynors: if non-NULL, an array of vectors, same length as number of polygons.
 * \param r_loopnors: if non-NULL, an array of vectors, same length as number of loops.
 */
void BKE_keyblock_mesh_calc_normals(struct KeyBlock *kb,
                                    struct Mesh *mesh,
                                    float (*r_vertnors)[3],
                                    float (*r_polynors)[3],
                                    float (*r_loopnors)[3]);

void BKE_keyblock_update_from_vertcos(struct Object *ob,
                                      struct KeyBlock *kb,
                                      const float (*vertCos)[3]);
void BKE_keyblock_convert_from_vertcos(struct Object *ob,
                                       struct KeyBlock *kb,
                                       const float (*vertCos)[3]);
float (*BKE_keyblock_convert_to_vertcos(struct Object *ob, struct KeyBlock *kb))[3];

void BKE_keyblock_update_from_offset(struct Object *ob,
                                     struct KeyBlock *kb,
                                     const float (*ofs)[3]);

/* other management */

/**
 * Move shape key from org_index to new_index. Safe, clamps index to valid range,
 * updates reference keys, the object's active shape index,
 * the 'frame' value in case of absolute keys, etc.
 * Note indices are expected in real values (not 'fake' shapenr +1 ones).
 *
 * \param org_index: if < 0, current object's active shape will be used as skey to move.
 * \return true if something was done, else false.
 */
bool BKE_keyblock_move(struct Object *ob, int org_index, int new_index);

/**
 * Check if given key-block (as index) is used as basis by others in given key.
 */
bool BKE_keyblock_is_basis(struct Key *key, int index);

/* -------------------------------------------------------------------- */
/** \name Key-Block Data Access
 * \{ */

/**
 * \param shape_index: The index to use or all (when -1).
 */
void BKE_keyblock_data_get_from_shape(const struct Key *key, float (*arr)[3], int shape_index);
void BKE_keyblock_data_get(const struct Key *key, float (*arr)[3]);

/**
 * Set the data to all key-blocks (or shape_index if != -1).
 */
void BKE_keyblock_data_set_with_mat4(struct Key *key,
                                     int shape_index,
                                     const float (*coords)[3],
                                     const float mat[4][4]);
/**
 * Set the data for all key-blocks (or shape_index if != -1),
 * transforming by \a mat.
 */
void BKE_keyblock_curve_data_set_with_mat4(struct Key *key,
                                           const struct ListBase *nurb,
                                           int shape_index,
                                           const void *data,
                                           const float mat[4][4]);
/**
 * Set the data for all key-blocks (or shape_index if != -1).
 */
void BKE_keyblock_data_set(struct Key *key, int shape_index, const void *data);

/** \} */

#ifdef __cplusplus
};
#endif
