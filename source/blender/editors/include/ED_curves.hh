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

#include "DNA_view3d_types.h"
#include "DNA_windowmanager_enums.h"

#include "ED_select_utils.hh"
#include "ED_view3d.hh"

struct bContext;
struct Curves;
struct UndoType;
struct rcti;
struct TransVertStore;
struct wmKeyConfig;
struct wmOperator;
struct wmKeyMap;
struct EnumPropertyItem;
namespace blender::bke {
enum class AttrDomain : int8_t;
struct GSpanAttributeWriter;
}  // namespace blender::bke

namespace blender::ed::curves {

void operatortypes_curves();
void operatormacros_curves();
void undosys_type_register(UndoType *ut);
void keymap_curves(wmKeyConfig *keyconf);

void ED_operatortypes_curves_pen();
void ED_curves_pentool_modal_keymap(wmKeyConfig *keyconf);

namespace pen_tool {

enum class ElementMode : int8_t {
  None = 0,
  Point = 1,
  Edge = 2,
  HandleLeft = 3,
  HandleRight = 4,
};

struct ClosestElement {
  float distance_squared = std::numeric_limits<float>::max();
  ElementMode element_mode;
  int point_index = -1;
  int curve_index = -1;
  float edge_t = -1.0f;
  int drawing_index = -1;

  bool is_closer(const float new_distance_squared,
                 const ElementMode new_element_mode,
                 const float threshold_distance) const;
};

class PenToolOperation {
 public:
  ViewContext vc;

  float threshold_distance;
  float threshold_distance_edge;

  bool extrude_point;
  bool delete_point;
  bool insert_point;
  bool move_seg;
  bool select_point;
  bool move_point;
  bool cycle_handle_type;
  int extrude_handle;
  float radius;

  bool move_entire;
  bool snap_angle;
  bool move_handle;

  bool point_added;
  bool point_removed;
  /* Used to go back to `aligned` after `move_handle` becomes `false` */
  bool handle_moved;

  float4x4 projection;
  float2 mouse_co;
  float2 xy;
  float2 prev_xy;
  float2 center_of_mass_co;
  ClosestElement closest_element;

  std::optional<int> active_drawing_index;
  Vector<float4x4> layer_to_world_per_curves;
  /* Only used for Grease Pencil. */
  Vector<float4x4> layer_to_object_per_curves;

  virtual ~PenToolOperation() = default;

  virtual float3 project(const float2 &screen_co) const = 0;
  virtual IndexMask all_selected_points(int curves_index, IndexMaskMemory &memory) const = 0;
  virtual IndexMask visible_bezier_handle_points(int curves_index,
                                                 IndexMaskMemory &memory) const = 0;
  virtual IndexMask editable_curves(int curves_index, IndexMaskMemory &memory) const = 0;
  virtual void tag_curve_changed(int curves_index) const = 0;
  virtual bke::CurvesGeometry &get_curves(int curves_index) const = 0;
  virtual IndexRange curves_range() const = 0;
  virtual void single_point_attributes(bke::CurvesGeometry &curves, int curves_index) const = 0;
  /**
   * Will return true if a new curve can be created, and report any errors.
   */
  virtual bool can_create_new_curve(wmOperator *op) const = 0;
  virtual void update_view(bContext *C) const = 0;
  virtual std::optional<wmOperatorStatus> initialize(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent *event) = 0;

  float2 layer_to_screen(const float4x4 &layer_to_object, const float3 &point) const;

  float3 screen_to_layer(const float4x4 &layer_to_world,
                         const float2 &screen_co,
                         const float3 &depth_point_layer) const;

  wmOperatorStatus invoke(bContext *C, wmOperator *op, const wmEvent *event);
  wmOperatorStatus modal(bContext *C, wmOperator *op, const wmEvent *event);
};

void pen_tool_common_props(wmOperatorType *ot);
wmKeyMap *ensure_keymap(wmKeyConfig *keyconf);

}  // namespace pen_tool

/**
 * Return an owning pointer to an array of point normals the same size as the number of control
 * points. The normals depend on the normal mode for each curve and the "tilt" attribute and may be
 * calculated for the evaluated points and sampled back to the control points.
 */
float (*point_normals_array_create(const Curves *curves_id))[3];

/**
 * Get selection attribute names need for given curve and domain.
 * Possible outcomes:
 * [".selection", ".selection_handle_left", ".selection_handle_right"] if Bezier curves are
 * present, [".selection"] otherwise.
 */
Span<StringRef> get_curves_selection_attribute_names(const bke::CurvesGeometry &curves);

/**
 * Get writable positions per selection attribute for given curve.
 */
Vector<MutableSpan<float3>> get_curves_positions_for_write(bke::CurvesGeometry &curves);

/**
 * Get read-only positions per selection attribute for given curve.
 */
Vector<Span<float3>> get_curves_positions(const bke::CurvesGeometry &curves);

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

/**
 * Get the position span associated with the given selection attribute name.
 */
std::optional<Span<float3>> get_selection_attribute_positions(
    const bke::CurvesGeometry &curves,
    const bke::crazyspace::GeometryDeformation &deformation,
    StringRef attribute_name);

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
                                    eHandleDisplay handle_display,
                                    SelectionRangeFn range_consumer);

/**
 * Same logic as in foreach_selectable_point_range, just ranges reference curves instead of
 * positions directly. Further positions can be referenced by using `curves.points_by_curve()`
 * in a callback function.
 */
void foreach_selectable_curve_range(const bke::CurvesGeometry &curves,
                                    const bke::crazyspace::GeometryDeformation &deformation,
                                    eHandleDisplay handle_display,
                                    SelectionRangeFn range_consumer);

bool object_has_editable_curves(const Main &bmain, const Object &object);
bke::CurvesGeometry primitive_random_sphere(int curves_size, int points_per_curve);
VectorSet<Curves *> get_unique_editable_curves(const bContext &C);
void ensure_surface_deformation_node_exists(bContext &C, Object &curves_ob);

/**
 * Allocate an array of #TransVert for cursor/selection snapping (See
 * #ED_transverts_create_from_obedit in `view3d_snap.cc`).
 * \note The #TransVert elements in \a tvs are expected to write to the positions of \a curves.
 */
void transverts_from_curves_positions_create(bke::CurvesGeometry &curves,
                                             TransVertStore *tvs,
                                             const bool skip_handles);

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
void CURVES_OT_select_linked_pick(wmOperatorType *ot);
void CURVES_OT_separate(wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mask Functions
 * \{ */

/**
 * Create a mask for all curves that have at least one point in the point mask.
 */
IndexMask curve_mask_from_points(const bke::CurvesGeometry &curves,
                                 const IndexMask &point_mask,
                                 const GrainSize grain_size,
                                 IndexMaskMemory &memory);

/**
 * Return a mask of all the end points in the curves.
 * \param curves_mask: (optional) The curves that should be used in the resulting point mask.
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

void fill_selection_false(GMutableSpan selection);
void fill_selection_true(GMutableSpan selection);
void fill_selection(GMutableSpan selection, bool value);
void fill_selection_false(GMutableSpan selection, const IndexMask &mask);
void fill_selection_true(GMutableSpan selection, const IndexMask &mask);

/**
 * Return true if any element is selected, on either domain with either type.
 */
bool has_anything_selected(const bke::CurvesGeometry &curves);
bool has_anything_selected(const bke::CurvesGeometry &curves, bke::AttrDomain selection_domain);
bool has_anything_selected(const bke::CurvesGeometry &curves,
                           bke::AttrDomain selection_domain,
                           const IndexMask &mask);

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
IndexMask retrieve_selected_points(const Curves &curves_id, IndexMaskMemory &memory);
/**
 * Find points that are selected, for a given attribute_name, requires mask of all Bezier points.
 * Note: When retrieving ".selection_handle_left" or ".selection_handle_right" all non-Bezier
 * points will be deselected even if the raw attribute is selected.
 */
IndexMask retrieve_selected_points(const bke::CurvesGeometry &curves,
                                   StringRef attribute_name,
                                   const IndexMask &bezier_points,
                                   IndexMaskMemory &memory);

/**
 * Find points that are selected (a selection factor greater than zero) or have
 * any of their Bezier handle selected.
 */
IndexMask retrieve_all_selected_points(const bke::CurvesGeometry &curves,
                                       int handle_display,
                                       IndexMaskMemory &memory);

/**
 * If the selection_id attribute doesn't exist, create it with the requested type (bool or float).
 */
bke::GSpanAttributeWriter ensure_selection_attribute(bke::CurvesGeometry &curves,
                                                     bke::AttrDomain selection_domain,
                                                     bke::AttrType create_type,
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
 * \param mask: (optional) The elements that should be affected. This mask should be in the domain
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
 * \param curves_mask: (optional) The curves that should be affected.
 */
void select_linked(bke::CurvesGeometry &curves);
void select_linked(bke::CurvesGeometry &curves, const IndexMask &curves_mask);

/**
 * Select alternated points in strokes with already selected points
 *
 * \param curves_mask: (optional) The curves that should be affected.
 */
void select_alternate(bke::CurvesGeometry &curves, const bool deselect_ends);
void select_alternate(bke::CurvesGeometry &curves,
                      const IndexMask &curves_mask,
                      const bool deselect_ends);

/**
 * (De)select all the adjacent points of the current selected points.
 *
 * \param curves_mask: (optional) The curves that should be affected.
 */
void select_adjacent(bke::CurvesGeometry &curves, bool deselect);
void select_adjacent(bke::CurvesGeometry &curves, const IndexMask &curves_mask, bool deselect);

/**
 * Helper struct for `closest_elem_find_screen_space`.
 */
struct FindClosestData {
  int index = -1;
  float distance_sq = FLT_MAX;
};

/**
 * Find the closest screen-space point or curve in projected region-space.
 *
 * \return A new point or curve closer than the \a initial input, if one exists.
 */
std::optional<FindClosestData> closest_elem_find_screen_space(const ViewContext &vc,
                                                              OffsetIndices<int> points_by_curve,
                                                              Span<float3> deformed_positions,
                                                              const VArray<bool> &cyclic,
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
                const IndexMask &selection_mask,
                const IndexMask &bezier_mask,
                bke::AttrDomain selection_domain,
                const rcti &rect,
                eSelectOp sel_op);

/**
 * Select points or curves in a (screen-space) poly shape.
 */
bool select_lasso(const ViewContext &vc,
                  bke::CurvesGeometry &curves,
                  const bke::crazyspace::GeometryDeformation &deformation,
                  const float4x4 &projection,
                  const IndexMask &selection_mask,
                  const IndexMask &bezier_mask,
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
                   const IndexMask &selection_mask,
                   const IndexMask &bezier_mask,
                   bke::AttrDomain selection_domain,
                   int2 coord,
                   float radius,
                   eSelectOp sel_op);

/**
 * Mask of points adjacent to a selected point, or unselected point if deselect is true.
 */
IndexMask select_adjacent_mask(const bke::CurvesGeometry &curves,
                               StringRef attribute_name,
                               bool deselect,
                               IndexMaskMemory &memory);
IndexMask select_adjacent_mask(const bke::CurvesGeometry &curves,
                               const IndexMask &curves_mask,
                               StringRef attribute_name,
                               bool deselect,
                               IndexMaskMemory &memory);

/**
 * Select points or curves in a (screen-space) rectangle.
 */
IndexMask select_box_mask(const ViewContext &vc,
                          const bke::CurvesGeometry &curves,
                          const bke::crazyspace::GeometryDeformation &deformation,
                          const float4x4 &projection,
                          const IndexMask &selection_mask,
                          const IndexMask &bezier_mask,
                          bke::AttrDomain selection_domain,
                          StringRef attribute_name,
                          const rcti &rect,
                          IndexMaskMemory &memory);

/**
 * Select points or curves in a (screen-space) poly shape.
 */
IndexMask select_lasso_mask(const ViewContext &vc,
                            const bke::CurvesGeometry &curves,
                            const bke::crazyspace::GeometryDeformation &deformation,
                            const float4x4 &projection,
                            const IndexMask &selection_mask,
                            const IndexMask &bezier_mask,
                            bke::AttrDomain selection_domain,
                            StringRef attribute_name,
                            Span<int2> lasso_coords,
                            IndexMaskMemory &memory);

/**
 * Select points or curves in a (screen-space) circle.
 */
IndexMask select_circle_mask(const ViewContext &vc,
                             const bke::CurvesGeometry &curves,
                             const bke::crazyspace::GeometryDeformation &deformation,
                             const float4x4 &projection,
                             const IndexMask &selection_mask,
                             const IndexMask &bezier_mask,
                             bke::AttrDomain selection_domain,
                             StringRef attribute_name,
                             int2 coord,
                             float radius,
                             IndexMaskMemory &memory);
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

void separate_points(const bke::CurvesGeometry &curves,
                     const IndexMask &points_to_separate,
                     bke::CurvesGeometry &separated,
                     bke::CurvesGeometry &retained);

bke::CurvesGeometry split_points(const bke::CurvesGeometry &curves,
                                 const IndexMask &points_to_split);

/**
 * Adds new curves to \a curves.
 * \param new_sizes: The new size for each curve. Sizes must be > 0.
 */
void add_curves(bke::CurvesGeometry &curves, Span<int> new_sizes);

/**
 * Resizes the curves in \a curves.
 * \param curves_to_resize: The curves that should be resized.
 * \param new_sizes: The new size for each curve in \a curves_to_resize. If the size is smaller,
 * the curve is trimmed from the end. If the size is larger, the end is extended and all attributes
 * are default initialized. Sizes must be > 0.
 */
void resize_curves(bke::CurvesGeometry &curves,
                   const IndexMask &curves_to_resize,
                   Span<int> new_sizes);
/**
 * Reorders the curves in \a curves.
 * \param old_by_new_indices_map: An index mapping where each value is the target index for the
 * reorder curves.
 */
void reorder_curves(bke::CurvesGeometry &curves, Span<int> old_by_new_indices_map);

wmOperatorStatus join_objects_exec(bContext *C, wmOperator *op);

enum class SetHandleType : uint8_t {
  Free = 0,
  Auto = 1,
  Vector = 2,
  Align = 3,
  Toggle = 4,
};

extern const EnumPropertyItem rna_enum_set_handle_type_items[];

/** \} */

}  // namespace blender::ed::curves
