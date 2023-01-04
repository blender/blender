/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct bContext;

#ifdef __cplusplus
extern "C" {
#endif

void ED_operatortypes_curves(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#  include "BKE_attribute.hh"
#  include "BLI_index_mask.hh"
#  include "BLI_vector.hh"
#  include "BLI_vector_set.hh"

#  include "BKE_curves.hh"

namespace blender::ed::curves {

bke::CurvesGeometry primitive_random_sphere(int curves_size, int points_per_curve);
VectorSet<Curves *> get_unique_editable_curves(const bContext &C);
void ensure_surface_deformation_node_exists(bContext &C, Object &curves_ob);

/* -------------------------------------------------------------------- */
/** \name Poll Functions
 * \{ */

bool editable_curves_with_surface_poll(bContext *C);
bool curves_with_surface_poll(bContext *C);
bool editable_curves_poll(bContext *C);
bool curves_poll(bContext *C);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Selection
 *
 * Selection on curves can be stored on either attribute domain: either per-curve or per-point. It
 * can be stored with a float or boolean data-type. The boolean data-type is faster, smaller, and
 * corresponds better to edit-mode selections, but the float data type is useful for soft selection
 * (like masking) in sculpt mode.
 *
 * The attribute API is used to do the necessary type and domain conversions when necessary, and
 * can handle most interaction with the selection attribute, but these functions implement some
 * helpful utilities on top of that.
 * \{ */

void fill_selection_false(GMutableSpan span);
void fill_selection_true(GMutableSpan span);

/**
 * Return true if any element is selected, on either domain with either type.
 */
bool has_anything_selected(const Curves &curves_id);

/**
 * Find curves that have any point selected (a selection factor greater than zero),
 * or curves that have their own selection factor greater than zero.
 */
IndexMask retrieve_selected_curves(const Curves &curves_id, Vector<int64_t> &r_indices);

/**
 * Find points that are selected (a selection factor greater than zero),
 * or points in curves with a selection factor greater than zero).
 */
IndexMask retrieve_selected_points(const Curves &curves_id, Vector<int64_t> &r_indices);

/**
 * If the ".selection" attribute doesn't exist, create it with the requested type (bool or float).
 */
void ensure_selection_attribute(Curves &curves_id, const eCustomDataType create_type);

/** \} */

}  // namespace blender::ed::curves
#endif
