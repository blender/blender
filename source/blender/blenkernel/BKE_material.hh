/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief General operations, lookup, etc. for materials.
 */

#include <optional>

struct ID;
struct Main;
struct Material;
struct Object;
struct Scene;
struct bNode;
struct Depsgraph;
struct MaterialGPencilStyle;

/* -------------------------------------------------------------------- */
/** \name Module
 * \{ */

void BKE_materials_init();
void BKE_materials_exit();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Materials
 * \{ */

/** Make the object's material array the same size as its data ID's material array. */
void BKE_object_materials_sync_length(Main *bmain, Object *ob, ID *id);

/** Ensure that every object using this data has a material array of the correct size. */
void BKE_objects_materials_sync_length_all(Main *bmain, ID *id);

void BKE_object_material_resize(Main *bmain, Object *ob, short totcol, bool do_id_user);
void BKE_object_material_remap(Object *ob, const unsigned int *remap);
/**
 * Calculate a material remapping from \a ob_src to \a ob_dst.
 *
 * \param remap_src_to_dst: An array the size of `ob_src->totcol`
 * where index values are filled in which map to \a ob_dst materials.
 */
void BKE_object_material_remap_calc(Object *ob_dst, Object *ob_src, short *remap_src_to_dst);
/**
 * Copy materials from evaluated geometry to the original geometry of an object.
 */
void BKE_object_material_from_eval_data(Main *bmain, Object *ob_orig, const ID *data_eval);
Material *BKE_material_add(Main *bmain, const char *name);
Material *BKE_gpencil_material_add(Main *bmain, const char *name);
void BKE_gpencil_material_attr_init(Material *ma);
void BKE_material_make_node_previews_dirty(Material *ma);

/* UNUSED */
// void automatname(Material *);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Slots
 * \{ */

Material ***BKE_object_material_array_p(Object *ob);
short *BKE_object_material_len_p(Object *ob);
/**
 * \note Same as #BKE_object_material_len_p but for ID's.
 */
Material ***BKE_id_material_array_p(ID *id); /* same but for ID's */
short *BKE_id_material_len_p(ID *id);

enum {
  /* use existing link option */
  BKE_MAT_ASSIGN_EXISTING,
  BKE_MAT_ASSIGN_USERPREF,
  BKE_MAT_ASSIGN_OBDATA,
  BKE_MAT_ASSIGN_OBJECT,
};

Material **BKE_object_material_get_p(Object *ob, short act);
Material *BKE_object_material_get(Object *ob, short act);
void BKE_id_material_assign(Main *bmain, ID *id, Material *ma, short act);
void BKE_object_material_assign(Main *bmain, Object *ob, Material *ma, short act, int assign_type);

/**
 * Similar to #BKE_object_material_assign with #BKE_MAT_ASSIGN_OBDATA type,
 * but does not scan whole Main for other usages of the same obdata. Only
 * use in cases where you know that the object's obdata is only used by this one
 * object.
 */
void BKE_object_material_assign_single_obdata(Main *bmain, Object *ob, Material *ma, short act);
/**
 * \warning this calls many more update calls per object then are needed, could be optimized.
 */
void BKE_object_material_array_assign(
    Main *bmain, Object *ob, Material ***matar, int totcol, bool to_object_only);

short BKE_object_material_slot_find_index(Object *ob, Material *ma);
/**
 * \param set_active: Set the newly added slot as active material slot of the object. Usually that
 * is wanted when adding a material slot, so it's the default.
 */
bool BKE_object_material_slot_add(Main *bmain, Object *ob, bool set_active = true);
bool BKE_object_material_slot_remove(Main *bmain, Object *ob);
bool BKE_object_material_slot_used(Object *object, short actcol);

int BKE_object_material_index_get(Object *ob, const Material *ma);
/**
 * A version of #BKE_object_material_index_get that takes an index to test first.
 *
 * \param hint_index: When this index is in a valid range, test it first.
 * Useful when an active-index is preferred but may not match the material.
 */
int BKE_object_material_index_get_with_hint(Object *ob, const Material *ma, int hint_index);

int BKE_object_material_ensure(Main *bmain, Object *ob, Material *material);

Material *BKE_gpencil_material(Object *ob, short act);
MaterialGPencilStyle *BKE_gpencil_material_settings(Object *ob, short act);

void BKE_texpaint_slot_refresh_cache(Scene *scene, Material *ma, const Object *ob);
void BKE_texpaint_slots_refresh_object(Scene *scene, Object *ob);
bNode *BKE_texpaint_slot_material_find_node(Material *ma, short texpaint_slot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name RNA API
 * \{ */

void BKE_id_materials_copy(Main *bmain, ID *id_src, ID *id_dst);
void BKE_id_material_resize(Main *bmain, ID *id, short totcol, bool do_id_user);
void BKE_id_material_append(Main *bmain, ID *id, Material *ma);
Material *BKE_id_material_pop(Main *bmain,
                              ID *id,
                              /* index is an int because of RNA. */
                              int index);
void BKE_id_material_clear(Main *bmain, ID *id);

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
Material *BKE_object_material_get_eval(Object *ob, short act);
/**
 * Same as above, but uses the given geometry data instead of `ob.data`. This is useful for
 * instances. The alternative is to use #BKE_object_replace_data_on_shallow_copy which is more
 * hacky.
 */
const Material *BKE_object_material_get_eval(const Object &ob, const ID &data, short act);
/**
 * Gets the number of material slots on the evaluated object.
 * This is the maximum of the number of material slots on the object and geometry.
 */
int BKE_object_material_count_eval(const Object *ob);
/** Same as above but allows using a custom ID as data instead of Object.data. */
int BKE_object_material_count_eval(const Object &ob, const ID &data);

/**
 * Returns the maximum material index used by the geometry. This returns zero if the geometry is
 * empty or if all material indices are negative.
 */
std::optional<int> BKE_id_material_index_max_eval(const ID &id);
/** Returns how many material slots the geometry may use (based on the maximum material index). */
int BKE_id_material_used_eval(const ID &id);

/**
 * Gets the number of material slots used by the geometry. The corresponding material for each slot
 * can be retrieved with #BKE_object_material_get_eval.
 *
 * These two functions give the same result when the mesh is provided itself, or an object that
 * uses the mesh.
 *
 * NOTE: This may be higher or lower than the number of material slots on the object or
 * object-data. However, it is always at least 1 (the fallback).
 */
int BKE_id_material_used_with_fallback_eval(const ID &id);
int BKE_object_material_used_with_fallback_eval(const Object &ob);

void BKE_id_material_eval_assign(ID *id, int slot, Material *material);
/**
 * Add an empty material slot if the id has no material slots. This material slot allows the
 * material to be overwritten by object-linked materials.
 */
void BKE_id_material_eval_ensure_default_slot(ID *id);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rendering
 * \{ */

/**
 * \param r_col: current value.
 * \param col: new value.
 * \param fac: Zero for is no change.
 */
void ramp_blend(int type, float r_col[4], float fac, const float col[4]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Default Materials
 *
 * TODO: Explain expected usages? Seems to be primarily defined for GPU/viewport code?
 *
 *  \warning _NEVER_ use these materials as fallback data for regular ID data. They should only be
 * used as template/copy source, or in some very specific, local and short-lived contexts.
 * \{ */

Material *BKE_material_default_empty();
Material *BKE_material_default_holdout();
Material *BKE_material_default_surface();
Material *BKE_material_default_volume();
Material *BKE_material_default_gpencil();

void BKE_material_defaults_free_gpu();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dependency graph evaluation
 * \{ */

void BKE_material_eval(Depsgraph *depsgraph, Material *material);

/** \} */
