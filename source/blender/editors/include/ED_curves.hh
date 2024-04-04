/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"

#include "BLI_index_mask_fwd.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "ED_select_utils.hh"

struct bContext;
struct Curves;
struct UndoType;
struct SelectPick_Params;
struct ViewContext;
struct rcti;
struct TransVertStore;
struct wmKeyConfig;
namespace blender::bke {
enum class AttrDomain : int8_t;
struct GSpanAttributeWriter;
}  // namespace blender::bke

namespace blender::ed::curves {

void operatortypes_curves();
void operatormacros_curves();
void undosys_type_register(UndoType *ut);
void keymap_curves(wmKeyConfig *keyconf);

/**
 * Return an owning pointer to an array of point normals the same size as the number of control
 * points. The normals depend on the normal mode for each curve and the "tilt" attribute and may be
 * calculated for the evaluated points and sampled back to the control points.
 */
float (*point_normals_array_create(const Curves *curves_id))[3];

/**
 * Get selection attribute names need for given curve.
 * Possible outcomes: [".selection"] if Bezier curves are present,
 * [".selection", ".selection_handle_left", ".selection_handle_right"] otherwise. */
Span<StringRef> get_curves_selection_attribute_names(const bke::CurvesGeometry &curves);

/* Get all possible curve selection attribute names. */
Span<StringRef> get_curves_all_selection_attribute_names();

/**
 * Returns [".selection_handle_left", ".selection_handle_right"] if argument contains Bezier
 * curves, empty span otherwise.
 */
Span<StringRef> get_curves_bezier_selection_attribute_names(const bke::CurvesGeometry &curves);

/**
 * Used to select everything or to delete selection attribute so that it will not have to be
 * resized.
 */
void remove_selection_attributes(
    bke::MutableAttributeAccessor &attributes,
    Span<StringRef> selection_attribute_names = get_curves_all_selection_attribute_names());

using SelectionRangeFn = FunctionRef<void(
    IndexRange range, Span<float3> positions, StringRef selection_attribute_name)>;
/**
 * Traverses all ranges of control points possible select. Callback function is provided with a
 * range being visited, positions (deformed if possible) referenced by the range and selection
 * attribute name positions belongs to:
 *  curves.positions() belong to ".selection",
 *  curves.handle_positions_left() belong to  ".selection_handle_left",
 *  curves.handle_positions_right() belong to ".selection_handle_right".
 */
void foreach_selectable_point_range(const bke::CurvesGeometry &curves,
                                    const bke::crazyspace::GeometryDeformation &deformation,
                                    SelectionRangeFn range_consumer);

/**
 * Same logic as in foreach_selectable_point_range, just ranges reference curves instead of
 * positions directly. Further positions can be referenced by using `curves.points_by_curve()`
 * in a callback function.
 */
void foreach_selectable_curve_range(const bke::CurvesGeometry &curves,
                                    const bke::crazyspace::GeometryDeformation &deformation,
                                    SelectionRangeFn range_consumer);

bool object_has_editable_curves(const Main &bmain, const Object &object);
bke::CurvesGeometry primitive_random_sphere(int curves_size, int points_per_curve);
VectorSet<Curves *> get_unique_editable_curves(const bContext &C);
void ensure_surface_deformation_node_exists(bContext &C, Object &curves_ob);

/**
 * Allocate an array of `TransVert` for cursor/selection snapping (See
 * `ED_transverts_create_from_obedit` in `view3d_snap.cc`).
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
/** \name Operators
 * \{ */

void CURVES_OT_attribute_set(wmOperatorType *ot);
void CURVES_OT_draw(wmOperatorType *ot);
void CURVES_OT_extrude(wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mask Functions
 * \{ */

/**
 * Return a mask of all the end points in the curves.
 * \param curves_mask (optional): The curves that should be used in the resulting point mask.
 * \param amount_start: The amount of points to mask from the front.
 * \param amount_end: The amount of points to mask from the back.
 * \param inverted: Invert the resulting mask.
 */
IndexMask end_points(const bke::CurvesGeometry &curves,
                     int amount_start,
                     int amount_end,
                     bool inverted,
                     IndexMaskMemory &memory);
IndexMask end_points(const bke::CurvesGeometry &curves,
                     const IndexMask &curves_mask,
                     int amount_start,
                     int amount_end,
                     bool inverted,
                     IndexMaskMemory &memory);

/**
 * Return a mask of random points or curves.
 *
 * \param mask (optional): The elements that should be used in the resulting mask. This mask should
 * be in the same domain as the \a selection_domain. \param random_seed: The seed for the \a
 * RandomNumberGenerator. \param probability: Determines how likely a point/curve will be chosen.
 * If set to 0.0, nothing will be in the mask, if set to 1.0 everything will be in the mask.
 */
IndexMask random_mask(const bke::CurvesGeometry &curves,
                      bke::AttrDomain selection_domain,
                      uint32_t random_seed,
                      float probability,
                      IndexMaskMemory &memory);
IndexMask random_mask(const bke::CurvesGeometry &curves,
                      const IndexMask &mask,
                      bke::AttrDomain selection_domain,
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
void fill_selection(GMutableSpan selection, bool value);
void fill_selection_false(GMutableSpan selection, const IndexMask &mask);
void fill_selection_true(GMutableSpan selection, const IndexMask &mask);

/**
 * Return true if any element is selected, on either domain with either type.
 */
bool has_anything_selected(const bke::CurvesGeometry &curves);
bool has_anything_selected(const bke::CurvesGeometry &curves, bke::AttrDomain selection_domain);
bool has_anything_selected(const bke::CurvesGeometry &curves, const IndexMask &mask);

/**
 * Return true if any element in the span is selected, on either domain with either type.
 */
bool has_anything_selected(GSpan selection);
bool has_anything_selected(const VArray<bool> &varray, IndexRange range_to_check);
bool has_anything_selected(const VArray<bool> &varray, const IndexMask &indices_to_check);

/**
 * Find curves that have any point selected (a selection factor greater than zero),
 * or curves that have their own selection factor greater than zero.
 */
IndexMask retrieve_selected_curves(const bke::CurvesGeometry &curves, IndexMaskMemory &memory);
IndexMask retrieve_selected_curves(const Curves &curves_id, IndexMaskMemory &memory);

/**
 * Find points that are selected (a selection factor greater than zero),
 * or points in curves with a selection factor greater than zero).
 */
IndexMask retrieve_selected_points(const bke::CurvesGeometry &curves, IndexMaskMemory &memory);
IndexMask retrieve_selected_points(const bke::CurvesGeometry &curves,
                                   StringRef attribute_name,
                                   IndexMaskMemory &memory);
IndexMask retrieve_selected_points(const Curves &curves_id, IndexMaskMemory &memory);

/**
 * If the selection_id attribute doesn't exist, create it with the requested type (bool or float).
 */
bke::GSpanAttributeWriter ensure_selection_attribute(bke::CurvesGeometry &curves,
                                                     bke::AttrDomain selection_domain,
                                                     eCustomDataType create_type,
                                                     StringRef attribute_name = ".selection");

void foreach_selection_attribute_writer(
    bke::CurvesGeometry &curves,
    bke::AttrDomain selection_domain,
    FunctionRef<void(bke::GSpanAttributeWriter &selection)> fn);

/** Apply a change to a single curve or point. Avoid using this when affecting many elements. */
void apply_selection_operation_at_index(GMutableSpan selection, int index, eSelectOp sel_op);

/**
 * (De)select all the curves.
 *
 * \param mask (optional): The elements that should be affected. This mask should be in the domain
 * of the \a selection_domain.
 * \param action: One of #SEL_TOGGLE, #SEL_SELECT, #SEL_DESELECT, or #SEL_INVERT.
 * See `ED_select_utils.hh`.
 */
void select_all(bke::CurvesGeometry &curves, bke::AttrDomain selection_domain, int action);
void select_all(bke::CurvesGeometry &curves,
                const IndexMask &mask,
                bke::AttrDomain selection_domain,
                int action);

/**
 * Select the points of all curves that have at least one point selected.
 *
 * \param curves_mask (optional): The curves that should be affected.
 */
void select_linked(bke::CurvesGeometry &curves);
void select_linked(bke::CurvesGeometry &curves, const IndexMask &curves_mask);

/**
 * Select alternated points in strokes with already selected points
 *
 * \param curves_mask (optional): The curves that should be affected.
 */
void select_alternate(bke::CurvesGeometry &curves, const bool deselect_ends);
void select_alternate(bke::CurvesGeometry &curves,
                      const IndexMask &curves_mask,
                      const bool deselect_ends);

/**
 * (De)select all the adjacent points of the current selected points.
 *
 * \param curves_mask (optional): The curves that should be affected.
 */
void select_adjacent(bke::CurvesGeometry &curves, bool deselect);
void select_adjacent(bke::CurvesGeometry &curves, const IndexMask &curves_mask, bool deselect);

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
                                                              OffsetIndices<int> points_by_curve,
                                                              Span<float3> deformed_positions,
                                                              const float4x4 &projection,
                                                              const IndexMask &mask,
                                                              bke::AttrDomain domain,
                                                              int2 coord,
                                                              const FindClosestData &initial);

/**
 * Select points or curves in a (screen-space) rectangle.
 */
bool select_box(const ViewContext &vc,
                bke::CurvesGeometry &curves,
                const bke::crazyspace::GeometryDeformation &deformation,
                const float4x4 &projection,
                const IndexMask &mask,
                bke::AttrDomain selection_domain,
                const rcti &rect,
                eSelectOp sel_op);

/**
 * Select points or curves in a (screen-space) poly shape.
 */
bool select_lasso(const ViewContext &vc,
                  bke::CurvesGeometry &curves,
                  const bke::crazyspace::GeometryDeformation &deformation,
                  const float4x4 &projection_matrix,
                  const IndexMask &mask,
                  bke::AttrDomain selection_domain,
                  Span<int2> lasso_coords,
                  eSelectOp sel_op);

/**
 * Select points or curves in a (screen-space) circle.
 */
bool select_circle(const ViewContext &vc,
                   bke::CurvesGeometry &curves,
                   const bke::crazyspace::GeometryDeformation &deformation,
                   const float4x4 &projection,
                   const IndexMask &mask,
                   bke::AttrDomain selection_domain,
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
bool remove_selection(bke::CurvesGeometry &curves, bke::AttrDomain selection_domain);

void duplicate_points(bke::CurvesGeometry &curves, const IndexMask &mask);
void duplicate_curves(bke::CurvesGeometry &curves, const IndexMask &mask);

/** \} */

}  // namespace blender::ed::curves
