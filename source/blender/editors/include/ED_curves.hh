/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BKE_attribute.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"

#include "BLI_index_mask.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "ED_select_utils.h"

struct bContext;
struct Curves;
struct UndoType;
struct SelectPick_Params;
struct ViewContext;
struct rcti;
struct TransVertStore;
struct wmKeyConfig;

/* -------------------------------------------------------------------- */
/** \name C Wrappers
 * \{ */

void ED_operatortypes_curves();
void ED_curves_undosys_type(UndoType *ut);
void ED_keymap_curves(wmKeyConfig *keyconf);

/**
 * Return an owning pointer to an array of point normals the same size as the number of control
 * points. The normals depend on the normal mode for each curve and the "tilt" attribute and may be
 * calculated for the evaluated points and sampled back to the control points.
 */
float (*ED_curves_point_normals_array_create(const Curves *curves_id))[3];

/** \} */

namespace blender::ed::curves {

bool object_has_editable_curves(const Main &bmain, const Object &object);
bke::CurvesGeometry primitive_random_sphere(int curves_size, int points_per_curve);
VectorSet<Curves *> get_unique_editable_curves(const bContext &C);
void ensure_surface_deformation_node_exists(bContext &C, Object &curves_ob);

/**
 * Allocate an array of `TransVert` for cursor/selection snapping (See
 * `ED_transverts_create_from_obedit` in `view3d_snap.c`).
 * \note: the `TransVert` elements in \a tvs are expected to write to the positions of \a curves.
 */
void transverts_from_curves_positions_create(bke::CurvesGeometry &curves, TransVertStore *tvs);

/* -------------------------------------------------------------------- */
/** \name Poll Functions
 * \{ */

bool editable_curves_with_surface_poll(bContext *C);
bool editable_curves_in_edit_mode_poll(bContext *C);
bool curves_with_surface_poll(bContext *C);
bool editable_curves_poll(bContext *C);
bool curves_poll(bContext *C);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mask Functions
 * \{ */

/**
 * Return a mask of all the end points in the curves.
 * \param amount_start: The amount of points to mask from the front.
 * \param amount_end: The amount of points to mask from the back.
 * \param inverted: Invert the resulting mask.
 */
IndexMask end_points(const bke::CurvesGeometry &curves,
                     int amount_start,
                     int amount_end,
                     bool inverted,
                     IndexMaskMemory &memory);

/**
 * Return a mask of random points or curves.
 *
 * \param random_seed: The seed for the \a RandomNumberGenerator.
 * \param probability: Determins how likely a point/curve will be chosen. If set to 0.0, nothing
 * will be in the mask, if set to 1.0 everything will be in the mask.
 */
IndexMask random_mask(const bke::CurvesGeometry &curves,
                      eAttrDomain selection_domain,
                      uint32_t random_seed,
                      float probability,
                      IndexMaskMemory &memory);

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
void fill_selection_false(GMutableSpan selection, const IndexMask &mask);
void fill_selection_true(GMutableSpan selection, const IndexMask &mask);

/**
 * Return true if any element is selected, on either domain with either type.
 */
bool has_anything_selected(const bke::CurvesGeometry &curves);

/**
 * Return true if any element in the span is selected, on either domain with either type.
 */
bool has_anything_selected(GSpan selection);
bool has_anything_selected(const VArray<bool> &varray, IndexRange range_to_check);

/**
 * Find curves that have any point selected (a selection factor greater than zero),
 * or curves that have their own selection factor greater than zero.
 */
IndexMask retrieve_selected_curves(const Curves &curves_id, IndexMaskMemory &memory);

/**
 * Find points that are selected (a selection factor greater than zero),
 * or points in curves with a selection factor greater than zero).
 */
IndexMask retrieve_selected_points(const bke::CurvesGeometry &curves, IndexMaskMemory &memory);
IndexMask retrieve_selected_points(const Curves &curves_id, IndexMaskMemory &memory);

/**
 * If the ".selection" attribute doesn't exist, create it with the requested type (bool or float).
 */
bke::GSpanAttributeWriter ensure_selection_attribute(bke::CurvesGeometry &curves,
                                                     eAttrDomain selection_domain,
                                                     eCustomDataType create_type);

/** Apply a change to a single curve or point. Avoid using this when affecting many elements. */
void apply_selection_operation_at_index(GMutableSpan selection, int index, eSelectOp sel_op);

/**
 * (De)select all the curves.
 *
 * \param action: One of SEL_TOGGLE, SEL_SELECT, SEL_DESELECT, or SEL_INVERT. See
 * "ED_select_utils.h".
 */
void select_all(bke::CurvesGeometry &curves, eAttrDomain selection_domain, int action);

/**
 * Select the points of all curves that have at least one point selected.
 */
void select_linked(bke::CurvesGeometry &curves);

/**
 * Select alternated points in strokes with already selected points
 */
void select_alternate(bke::CurvesGeometry &curves, const bool deselect_ends);

/**
 * (De)select all the adjacent points of the current selected points.
 */
void select_adjacent(bke::CurvesGeometry &curves, bool deselect);

/**
 * Helper struct for `closest_elem_find_screen_space`.
 */
struct FindClosestData {
  int index = -1;
  float distance = FLT_MAX;
};

/**
 * Find the closest screen-space point or curve in projected region-space.
 *
 * \return A new point or curve closer than the \a initial input, if one exists.
 */
std::optional<FindClosestData> closest_elem_find_screen_space(const ViewContext &vc,
                                                              const Object &object,
                                                              bke::CurvesGeometry &curves,
                                                              Span<float3> deformed_positions,
                                                              eAttrDomain domain,
                                                              int2 coord,
                                                              const FindClosestData &initial);

/**
 * Select points or curves in a (screen-space) rectangle.
 */
bool select_box(const ViewContext &vc,
                bke::CurvesGeometry &curves,
                Span<float3> deformed_positions,
                eAttrDomain selection_domain,
                const rcti &rect,
                eSelectOp sel_op);

/**
 * Select points or curves in a (screen-space) poly shape.
 */
bool select_lasso(const ViewContext &vc,
                  bke::CurvesGeometry &curves,
                  Span<float3> deformed_positions,
                  eAttrDomain selection_domain,
                  Span<int2> coords,
                  eSelectOp sel_op);

/**
 * Select points or curves in a (screen-space) circle.
 */
bool select_circle(const ViewContext &vc,
                   bke::CurvesGeometry &curves,
                   Span<float3> deformed_positions,
                   eAttrDomain selection_domain,
                   int2 coord,
                   float radius,
                   eSelectOp sel_op);
/** \} */

/* -------------------------------------------------------------------- */
/** \name Editing
 * \{ */

/**
 * Remove (dissolve) selected curves or points based on the ".selection" attribute.
 * \returns true if any point or curve was removed.
 */
bool remove_selection(bke::CurvesGeometry &curves, eAttrDomain selection_domain);

/** \} */

}  // namespace blender::ed::curves
