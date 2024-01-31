/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <optional>
#include <string>

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

/**
 * Free (or release) any data used by this shape-key (does not free the key itself).
 */
void BKE_key_free_data(Key *key);
void BKE_key_free_nolib(Key *key);
Key *BKE_key_add(Main *bmain, ID *id);
/**
 * Sort shape keys after a change.
 * This assumes that at most one key was moved,
 * which is a valid assumption for the places it's currently being called.
 */
void BKE_key_sort(Key *key);

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
 *
 * \param obdata: if given, also update that geometry with the result of the shape keys evaluation.
 */
float *BKE_key_evaluate_object_ex(
    Object *ob, int *r_totelem, float *arr, size_t arr_size, ID *obdata);
float *BKE_key_evaluate_object(Object *ob, int *r_totelem);

/**
 * \param shape_index: The index to use or all (when -1).
 */
int BKE_keyblock_element_count_from_shape(const Key *key, int shape_index);
int BKE_keyblock_element_count(const Key *key);

/**
 * \param shape_index: The index to use or all (when -1).
 */
size_t BKE_keyblock_element_calc_size_from_shape(const Key *key, int shape_index);
size_t BKE_keyblock_element_calc_size(const Key *key);

bool BKE_key_idtype_support(short id_type);

Key **BKE_key_from_id_p(ID *id);
Key *BKE_key_from_id(ID *id);
Key **BKE_key_from_object_p(Object *ob);
Key *BKE_key_from_object(Object *ob);
/**
 * Only the active key-block.
 */
KeyBlock *BKE_keyblock_from_object(Object *ob);
KeyBlock *BKE_keyblock_from_object_reference(Object *ob);

KeyBlock *BKE_keyblock_add(Key *key, const char *name);
/**
 * \note sorting is a problematic side effect in some cases,
 * better only do this explicitly by having its own function,
 *
 * \param key: The key datablock to add to.
 * \param name: Optional name for the new keyblock.
 * \param do_force: always use ctime even for relative keys.
 */
KeyBlock *BKE_keyblock_add_ctime(Key *key, const char *name, bool do_force);
/**
 * Get the appropriate #KeyBlock given an index (0 refers to the basis key). Key may be null.
 */
KeyBlock *BKE_keyblock_find_by_index(Key *key, int index);
/**
 * Get the appropriate #KeyBlock given a name to search for.
 */
KeyBlock *BKE_keyblock_find_name(Key *key, const char name[]);

KeyBlock *BKE_keyblock_find_uid(Key *key, int uid);

/**
 * \brief copy shape-key attributes, but not key data or name/UID.
 */
void BKE_keyblock_copy_settings(KeyBlock *kb_dst, const KeyBlock *kb_src);
/**
 * Get RNA-Path for 'value' setting of the given shape-key.
 * \note the user needs to free the returned string once they're finished with it.
 */
std::optional<std::string> BKE_keyblock_curval_rnapath_get(const Key *key, const KeyBlock *kb);

/* conversion functions */
/* NOTE: 'update_from' versions do not (re)allocate mem in kb, while 'convert_from' do. */

void BKE_keyblock_update_from_lattice(const Lattice *lt, KeyBlock *kb);
void BKE_keyblock_convert_from_lattice(const Lattice *lt, KeyBlock *kb);
void BKE_keyblock_convert_to_lattice(const KeyBlock *kb, Lattice *lt);

int BKE_keyblock_curve_element_count(const ListBase *nurb);
void BKE_keyblock_curve_data_transform(const ListBase *nurb,
                                       const float mat[4][4],
                                       const void *src,
                                       void *dst);
void BKE_keyblock_update_from_curve(const Curve *cu, KeyBlock *kb, const ListBase *nurb);
void BKE_keyblock_convert_from_curve(const Curve *cu, KeyBlock *kb, const ListBase *nurb);
void BKE_keyblock_convert_to_curve(KeyBlock *kb, Curve *cu, ListBase *nurb);

void BKE_keyblock_update_from_mesh(const Mesh *mesh, KeyBlock *kb);
void BKE_keyblock_convert_from_mesh(const Mesh *mesh, const Key *key, KeyBlock *kb);
void BKE_keyblock_convert_to_mesh(const KeyBlock *kb, float (*vert_positions)[3], int totvert);

/**
 * Computes normals (vertices, faces and/or loops ones) of given mesh for given shape key.
 *
 * \param kb: the KeyBlock to use to compute normals.
 * \param mesh: the Mesh to apply key-block to.
 * \param r_vert_normals: if non-NULL, an array of vectors, same length as number of vertices.
 * \param r_face_normals: if non-NULL, an array of vectors, same length as number of faces.
 * \param r_loop_normals: if non-NULL, an array of vectors, same length as number of loops.
 */
void BKE_keyblock_mesh_calc_normals(const KeyBlock *kb,
                                    Mesh *mesh,
                                    float (*r_vert_normals)[3],
                                    float (*r_face_normals)[3],
                                    float (*r_loop_normals)[3]);

void BKE_keyblock_update_from_vertcos(const Object *ob, KeyBlock *kb, const float (*vertCos)[3]);
void BKE_keyblock_convert_from_vertcos(const Object *ob, KeyBlock *kb, const float (*vertCos)[3]);
float (*BKE_keyblock_convert_to_vertcos(const Object *ob, const KeyBlock *kb))[3];

/** RAW coordinates offsets. */
void BKE_keyblock_update_from_offset(const Object *ob, KeyBlock *kb, const float (*ofs)[3]);

/* other management */

/**
 * Move shape key from org_index to new_index. Safe, clamps index to valid range,
 * updates reference keys, the object's active shape index,
 * the 'frame' value in case of absolute keys, etc.
 * Note indices are expected in real values (not *fake* `shapenr +1` ones).
 *
 * \param org_index: if < 0, current object's active shape will be used as shape-key to move.
 * \return true if something was done, else false.
 */
bool BKE_keyblock_move(Object *ob, int org_index, int new_index);

/**
 * Check if given key-block (as index) is used as basis by others in given key.
 */
bool BKE_keyblock_is_basis(const Key *key, int index);

/**
 * Returns a newly allocated array containing true for every key that has this one as basis.
 * If none are found, returns null.
 */
bool *BKE_keyblock_get_dependent_keys(const Key *key, int index);

/* -------------------------------------------------------------------- */
/** \name Key-Block Data Access
 * \{ */

/**
 * \param shape_index: The index to use or all (when -1).
 */
void BKE_keyblock_data_get_from_shape(const Key *key, float (*arr)[3], int shape_index);
void BKE_keyblock_data_get(const Key *key, float (*arr)[3]);

/**
 * Set the data to all key-blocks (or shape_index if != -1).
 */
void BKE_keyblock_data_set_with_mat4(Key *key,
                                     int shape_index,
                                     const float (*coords)[3],
                                     const float mat[4][4]);
/**
 * Set the data for all key-blocks (or shape_index if != -1),
 * transforming by \a mat.
 */
void BKE_keyblock_curve_data_set_with_mat4(
    Key *key, const ListBase *nurb, int shape_index, const void *data, const float mat[4][4]);
/**
 * Set the data for all key-blocks (or shape_index if != -1).
 */
void BKE_keyblock_data_set(Key *key, int shape_index, const void *data);

/** \} */
