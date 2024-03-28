/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_compiler_attrs.h"

struct bDeformGroup;
struct EnumPropertyItem;
struct ID;
struct MDeformVert;
struct Object;
struct PointerRNA;
struct PropertyRNA;

#define WEIGHT_REPLACE 1
#define WEIGHT_ADD 2
#define WEIGHT_SUBTRACT 3

namespace blender::ed::object {

bool vgroup_sync_from_pose(Object *ob);
void vgroup_select_by_name(Object *ob, const char *name);
/**
 * Removes out of range #MDeformWeights
 */
void vgroup_data_clamp_range(ID *id, int total);
/**
 * Matching index only.
 */
bool vgroup_array_copy(Object *ob, Object *ob_from);
bool vgroup_parray_alloc(ID *id, MDeformVert ***dvert_arr, int *dvert_tot, bool use_vert_sel);
/**
 * For use with tools that use vgroup_parray_alloc with \a use_vert_sel ==
 * true. This finds the unselected mirror deform verts and copies the weights to them from the
 * selected.
 *
 * \note \a dvert_array has mirrored weights filled in,
 * in case cleanup operations are needed on both.
 */
void vgroup_parray_mirror_sync(Object *ob,
                               MDeformVert **dvert_array,
                               int dvert_tot,
                               const bool *vgroup_validmap,
                               int vgroup_tot);
/**
 * Fill in the pointers for mirror verts (as if all mirror verts were selected too).
 *
 * similar to #vgroup_parray_mirror_sync but only fill in mirror points.
 */
void vgroup_parray_mirror_assign(Object *ob, MDeformVert **dvert_array, int dvert_tot);
void vgroup_parray_remove_zero(MDeformVert **dvert_array,
                               int dvert_tot,
                               const bool *vgroup_validmap,
                               int vgroup_tot,
                               float epsilon,
                               bool keep_single);
void vgroup_parray_to_weight_array(const MDeformVert **dvert_array,
                                   int dvert_tot,
                                   float *dvert_weights,
                                   int def_nr);
void vgroup_parray_from_weight_array(MDeformVert **dvert_array,
                                     int dvert_tot,
                                     const float *dvert_weights,
                                     int def_nr,
                                     bool remove_zero);
void vgroup_mirror(Object *ob,
                   bool mirror_weights,
                   bool flip_vgroups,
                   bool all_vgroups,
                   bool use_topology,
                   int *r_totmirr,
                   int *r_totfail);

/**
 * Called while not in editmode.
 */
void vgroup_vert_add(Object *ob, bDeformGroup *dg, int vertnum, float weight, int assignmode);
/**
 * Mesh object mode, lattice can be in edit-mode.
 */
void vgroup_vert_remove(Object *ob, bDeformGroup *dg, int vertnum);
float vgroup_vert_weight(Object *ob, bDeformGroup *dg, int vertnum);
/**
 * Use when adjusting the active vertex weight and apply to mirror vertices.
 */
void vgroup_vert_active_mirror(Object *ob, int def_nr);

const EnumPropertyItem *vgroup_selection_itemf_helper(const bContext *C,
                                                      PointerRNA *ptr,
                                                      PropertyRNA *prop,
                                                      bool *r_free,
                                                      unsigned int selection_mask);

}  // namespace blender::ed::object
