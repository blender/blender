/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "WM_api.hh"
#include "WM_types.hh"

#include "DNA_modifier_types.h"
#include "DNA_node_types.h"

#include "BKE_compute_context_cache.hh"
#include "BKE_context.hh"
#include "BKE_geometry_nodes_gizmos_transforms.hh"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_instances.hh"
#include "BKE_main_invariants.hh"
#include "BKE_modifier.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_object.hh"

#include "BLI_math_base_safe.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_rotation.hh"

#include "RNA_access.hh"

#include "MOD_nodes.hh"

#include "NOD_geometry_nodes_gizmos.hh"
#include "NOD_geometry_nodes_log.hh"

#include "UI_resources.hh"

#include "ED_gizmo_library.hh"
#include "ED_node.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "view3d_intern.hh"

namespace blender::ed::view3d::geometry_nodes_gizmos {
namespace geo_eval_log = nodes::geo_eval_log;
using geo_eval_log::GeoTreeLog;

static bool gizmo_is_interacting(const wmGizmo &gizmo)
{
  return gizmo.interaction_data != nullptr;
}

static ThemeColorID get_gizmo_theme_color_id(const GeometryNodeGizmoColor color_id)
{
  switch (color_id) {
    case GEO_NODE_GIZMO_COLOR_PRIMARY:
      return TH_GIZMO_PRIMARY;
    case GEO_NODE_GIZMO_COLOR_SECONDARY:
      return TH_GIZMO_SECONDARY;
    case GEO_NODE_GIZMO_COLOR_X:
      return TH_AXIS_X;
    case GEO_NODE_GIZMO_COLOR_Y:
      return TH_AXIS_Y;
    case GEO_NODE_GIZMO_COLOR_Z:
      return TH_AXIS_Z;
  }
  return TH_GIZMO_PRIMARY;
}

static ThemeColorID get_axis_theme_color_id(const int axis)
{
  return std::array{TH_AXIS_X, TH_AXIS_Y, TH_AXIS_Z}[axis];
}

static void get_axis_gizmo_colors(const int axis, float *r_color, float *r_color_hi)
{
  const ThemeColorID theme_id = get_axis_theme_color_id(axis);
  UI_GetThemeColor3fv(theme_id, r_color);
  UI_GetThemeColor3fv(theme_id, r_color_hi);
  r_color[3] = 0.6f;
  r_color_hi[3] = 1.0f;
}

static void make_matrix_orthonormal_but_keep_z_axis(float4x4 &m)
{
  /* Without this, the gizmo may be skewed. */
  m.x_axis() = math::normalize(math::cross(m.y_axis(), m.z_axis()));
  m.y_axis() = math::normalize(math::cross(m.z_axis(), m.x_axis()));
  m.z_axis() = math::normalize(m.z_axis());
  BLI_assert(math::is_orthonormal(float3x3(m)));
}

static float4x4 matrix_from_position_and_up_direction(const float3 &position,
                                                      const float3 &direction,
                                                      const math::AxisSigned direction_axis)
{
  BLI_assert(math::is_unit_scale(direction));
  math::Quaternion rotation;
  const float3 base_direction = math::to_vector<float3>(direction_axis);
  rotation_between_vecs_to_quat(&rotation.w, base_direction, direction);
  float4x4 mat = math::from_rotation<float4x4>(rotation);
  mat.location() = position;
  return mat;
}

struct UpdateReport {
  bool missing_socket_logs = false;
  bool invalid_transform = false;
};

using ApplyChangeFn = std::function<void(
    StringRef socket_identifier, FunctionRef<void(bke::SocketValueVariant &value)> modify_value)>;

struct GizmosUpdateParams {
  const bContext &C;
  /* Transform of the object and geometry that the gizmo belongs to. */
  float4x4 parent_transform;
  const bNode &gizmo_node;
  GeoTreeLog &tree_log;
  UpdateReport &r_report;
  nodes::inverse_eval::ElemVariant elem;

  template<typename T> [[nodiscard]] bool get_input_value(const StringRef identifier, T &r_value)
  {
    const bNodeSocket &socket = *this->gizmo_node.input_by_identifier(identifier);
    const std::optional<T> value_opt = this->tree_log.find_primitive_socket_value<T>(socket);
    if (!value_opt) {
      return false;
    }
    r_value = *value_opt;
    return true;
  }
};

class NodeGizmos {
 public:
  /**
   * Should be called when the gizmo is modified. It encapsulates the complexity of handling
   * multi-input gizmo sockets and the backpropagation of the change through the node tree. Search
   * for `apply_change =` to find where this is set.
   */
  ApplyChangeFn apply_change;

  virtual ~NodeGizmos() = default;

  /**
   * Called after the initial construction to build the individual gizmos. The gizmos have to be
   * added to the given group.
   */
  virtual void create_gizmos(wmGizmoGroup &gzgroup) = 0;

  /** Update the styling, transforms and target property of the gizmos. */
  virtual void update(GizmosUpdateParams & /*params*/) {}

  /** Get a list of all owned gizmos. */
  virtual Vector<wmGizmo *> get_all_gizmos() = 0;

  void hide_all()
  {
    for (wmGizmo *gizmo : this->get_all_gizmos()) {
      WM_gizmo_set_flag(gizmo, WM_GIZMO_HIDDEN, true);
    }
  }

  void show_all()
  {
    for (wmGizmo *gizmo : this->get_all_gizmos()) {
      WM_gizmo_set_flag(gizmo, WM_GIZMO_HIDDEN, false);
    }
  }

  /** Returns true if any of the gizmos is currently interacted with. */
  bool is_any_interacting()
  {
    bool any_interacting = false;
    for (const wmGizmo *gizmo : this->get_all_gizmos()) {
      any_interacting |= gizmo_is_interacting(*gizmo);
    }
    return any_interacting;
  }
};

class LinearGizmo : public NodeGizmos {
 private:
  wmGizmo *gizmo_ = nullptr;

  struct EditData {
    /** An additional that has to be applied because the gizmo has been scaled. */
    float factor_from_transform = 1.0f;
    float current_value = 0.0f;
  } edit_data_;

 public:
  void create_gizmos(wmGizmoGroup &gzgroup) override
  {
    gizmo_ = WM_gizmo_new("GIZMO_GT_arrow_3d", &gzgroup, nullptr);
  }

  Vector<wmGizmo *> get_all_gizmos() override
  {
    return {gizmo_};
  }

  void update(GizmosUpdateParams &params) override
  {
    const auto &storage = *static_cast<const NodeGeometryLinearGizmo *>(params.gizmo_node.storage);
    const bool is_interacting = gizmo_is_interacting(*gizmo_);

    this->update_style(storage);

    if (is_interacting) {
      return;
    }
    if (!this->update_transform(params)) {
      return;
    }
    this->update_target_property();
  }

  void update_style(const NodeGeometryLinearGizmo &storage)
  {
    /* Make sure the enum values are in sync. */
    static_assert(int(GEO_NODE_LINEAR_GIZMO_DRAW_STYLE_ARROW) == int(ED_GIZMO_ARROW_STYLE_NORMAL));
    static_assert(int(GEO_NODE_LINEAR_GIZMO_DRAW_STYLE_BOX) == int(ED_GIZMO_ARROW_STYLE_BOX));
    static_assert(int(GEO_NODE_LINEAR_GIZMO_DRAW_STYLE_CROSS) == int(ED_GIZMO_ARROW_STYLE_CROSS));
    RNA_enum_set(gizmo_->ptr, "draw_style", storage.draw_style);

    WM_gizmo_set_line_width(gizmo_, 1.0f);

    const float length = (storage.draw_style == GEO_NODE_LINEAR_GIZMO_DRAW_STYLE_BOX) ? 0.8f :
                                                                                        1.0f;
    RNA_float_set(gizmo_->ptr, "length", length);

    const ThemeColorID color_theme_id = get_gizmo_theme_color_id(
        GeometryNodeGizmoColor(storage.color_id));
    UI_GetThemeColor3fv(color_theme_id, gizmo_->color);
    UI_GetThemeColor3fv(TH_GIZMO_HI, gizmo_->color_hi);
  }

  bool update_transform(GizmosUpdateParams &params)
  {
    float3 position;
    float3 direction;
    if (!params.get_input_value("Position", position) ||
        !params.get_input_value("Direction", direction))
    {
      params.r_report.missing_socket_logs = true;
      return false;
    }
    direction = math::normalize(direction);
    if (math::is_zero(direction)) {
      params.r_report.invalid_transform = true;
      return false;
    }

    const float4x4 gizmo_base_transform = matrix_from_position_and_up_direction(
        position, direction, math::AxisSigned::Z_POS);

    float4x4 gizmo_transform = params.parent_transform * gizmo_base_transform;
    edit_data_.factor_from_transform = safe_divide(1.0f, math::length(gizmo_transform.z_axis()));
    make_matrix_orthonormal_but_keep_z_axis(gizmo_transform);
    copy_m4_m4(gizmo_->matrix_basis, gizmo_transform.ptr());
    return true;
  }

  void update_target_property()
  {
    /* Always reset to 0 when not interacting. */
    edit_data_.current_value = 0.0f;

    wmGizmoPropertyFnParams fn_params{};
    fn_params.user_data = this;
    fn_params.value_set_fn =
        [](const wmGizmo * /*gz*/, wmGizmoProperty *gz_prop, const void *value_ptr) {
          LinearGizmo &self = *static_cast<LinearGizmo *>(gz_prop->custom_func.user_data);
          const float new_gizmo_value = *static_cast<const float *>(value_ptr);
          self.edit_data_.current_value = new_gizmo_value;
          const float offset = new_gizmo_value * self.edit_data_.factor_from_transform;
          self.apply_change("Value", [&](bke::SocketValueVariant &value_variant) {
            value_variant.set(value_variant.get<float>() + offset);
          });
        };
    fn_params.value_get_fn =
        [](const wmGizmo * /*gz*/, wmGizmoProperty *gz_prop, void *value_ptr) {
          LinearGizmo &self = *static_cast<LinearGizmo *>(gz_prop->custom_func.user_data);
          *static_cast<float *>(value_ptr) = self.edit_data_.current_value;
        };
    WM_gizmo_target_property_def_func(gizmo_, "offset", &fn_params);
  }
};

class DialGizmo : public NodeGizmos {
 private:
  wmGizmo *gizmo_ = nullptr;

  struct EditData {
    bool is_negative_transform = false;
    float current_value = 0.0f;
  } edit_data_;

 public:
  void create_gizmos(wmGizmoGroup &gzgroup) override
  {
    gizmo_ = WM_gizmo_new("GIZMO_GT_dial_3d", &gzgroup, nullptr);
  }

  Vector<wmGizmo *> get_all_gizmos() override
  {
    return {gizmo_};
  }

  void update(GizmosUpdateParams &params) override
  {
    const auto &storage = *static_cast<const NodeGeometryDialGizmo *>(params.gizmo_node.storage);
    const bool is_interacting = gizmo_is_interacting(*gizmo_);

    this->update_style(storage, is_interacting);

    if (is_interacting) {
      return;
    }
    if (!this->update_transform(params)) {
      return;
    }
    this->update_target_property();
  }

  void update_style(const NodeGeometryDialGizmo &storage, const bool is_interacting)
  {
    WM_gizmo_set_flag(gizmo_, WM_GIZMO_DRAW_VALUE, true);
    WM_gizmo_set_line_width(gizmo_, 2.0f);
    RNA_boolean_set(gizmo_->ptr, "wrap_angle", false);

    int draw_options = RNA_enum_get(gizmo_->ptr, "draw_options");
    SET_FLAG_FROM_TEST(draw_options, is_interacting, ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_VALUE);
    RNA_enum_set(gizmo_->ptr, "draw_options", draw_options);

    const ThemeColorID color_theme_id = get_gizmo_theme_color_id(
        GeometryNodeGizmoColor(storage.color_id));
    UI_GetThemeColor3fv(color_theme_id, gizmo_->color);
    UI_GetThemeColor3fv(TH_GIZMO_HI, gizmo_->color_hi);
  }

  bool update_transform(GizmosUpdateParams &params)
  {
    float3 position;
    float3 up;
    bool screen_space;
    float radius;
    if (!params.get_input_value("Position", position) || !params.get_input_value("Up", up) ||
        !params.get_input_value("Screen Space", screen_space) ||
        !params.get_input_value("Radius", radius))
    {
      params.r_report.missing_socket_logs = true;
      return false;
    }
    up = math::normalize(up);

    if (math::is_zero(up) || math::is_zero(radius)) {
      params.r_report.invalid_transform = true;
      return false;
    }

    const float4x4 gizmo_base_transform = matrix_from_position_and_up_direction(
        position, up, math::AxisSigned::Z_NEG);
    float4x4 gizmo_transform = params.parent_transform * gizmo_base_transform;
    edit_data_.is_negative_transform = math::determinant(gizmo_transform) < 0.0f;
    make_matrix_orthonormal_but_keep_z_axis(gizmo_transform);
    copy_m4_m4(gizmo_->matrix_basis, gizmo_transform.ptr());

    WM_gizmo_set_flag(gizmo_, WM_GIZMO_DRAW_NO_SCALE, !screen_space);
    float transform_scale = 1.0f;
    if (!screen_space) {
      /* We can't scale the dial gizmo non-uniformly, so just take the average of the scale in each
       * axis for now. */
      transform_scale = math::average(math::to_scale(params.parent_transform));
    }
    copy_m4_m4(gizmo_->matrix_offset,
               math::from_scale<float4x4>(float3(radius * transform_scale)).ptr());

    return true;
  }

  void update_target_property()
  {
    edit_data_.current_value = 0.0f;

    wmGizmoPropertyFnParams params{};
    params.user_data = this;
    params.value_set_fn =
        [](const wmGizmo * /*gz*/, wmGizmoProperty *gz_prop, const void *value_ptr) {
          DialGizmo &self = *static_cast<DialGizmo *>(gz_prop->custom_func.user_data);
          const float new_gizmo_value = *static_cast<const float *>(value_ptr);
          self.edit_data_.current_value = new_gizmo_value;
          float offset = new_gizmo_value;
          if (self.edit_data_.is_negative_transform) {
            offset = -offset;
          }
          self.apply_change("Value", [&](bke::SocketValueVariant &value_variant) {
            value_variant.set(value_variant.get<float>() + offset);
          });
        };
    params.value_get_fn = [](const wmGizmo * /*gz*/, wmGizmoProperty *gz_prop, void *value_ptr) {
      DialGizmo &self = *static_cast<DialGizmo *>(gz_prop->custom_func.user_data);
      *static_cast<float *>(value_ptr) = self.edit_data_.current_value;
    };
    WM_gizmo_target_property_def_func(gizmo_, "offset", &params);
  }
};

class TransformGizmos : public NodeGizmos {
 private:
  std::array<wmGizmo *, 3> translation_gizmos_ = {};
  std::array<wmGizmo *, 3> rotation_gizmos_ = {};
  std::array<wmGizmo *, 3> scale_gizmos_ = {};

  bool any_translation_visible_ = false;
  bool any_rotation_visible_ = false;
  bool any_scale_visible_ = false;

  int transform_orientation_ = V3D_ORIENT_GLOBAL;

  /**
   * Transformation of the object and potentially crazy-space transforms applied on top of the
   * gizmos.
   */
  float4x4 parent_transform_;

  struct EditData {
    float3 current_translation;
    float3 current_rotation;
    float3 current_scale;
  } edit_data_;

 public:
  void create_gizmos(wmGizmoGroup &gzgroup) override
  {
    /* Translation */
    for (const int axis : IndexRange(3)) {
      translation_gizmos_[axis] = WM_gizmo_new("GIZMO_GT_arrow_3d", &gzgroup, nullptr);
    }

    /* Rotation */
    for (const int axis : IndexRange(3)) {
      rotation_gizmos_[axis] = WM_gizmo_new("GIZMO_GT_dial_3d", &gzgroup, nullptr);
    }

    /* Scale */
    for (const int axis : IndexRange(3)) {
      scale_gizmos_[axis] = WM_gizmo_new("GIZMO_GT_arrow_3d", &gzgroup, nullptr);
    }
  }

  Vector<wmGizmo *> get_all_gizmos() override
  {
    Vector<wmGizmo *> gizmos;
    gizmos.extend(translation_gizmos_);
    gizmos.extend(rotation_gizmos_);
    gizmos.extend(scale_gizmos_);
    return gizmos;
  }

  void update(GizmosUpdateParams &params) override
  {
    const auto &storage = *static_cast<const NodeGeometryTransformGizmo *>(
        params.gizmo_node.storage);

    this->update_visibility(params, storage);
    this->update_translate_style();
    this->update_rotate_style();
    this->update_scale_style();

    float3 position;
    math::Quaternion rotation;
    if (!params.get_input_value("Position", position) ||
        !params.get_input_value("Rotation", rotation))
    {
      params.r_report.missing_socket_logs = true;
      return;
    }

    float4x4 base_transform_from_socket = math::from_rotation<float4x4>(rotation);
    base_transform_from_socket.location() = position;

    Scene &scene = *CTX_data_scene(&params.C);
    const TransformOrientationSlot &orientation_slot = scene.orientation_slots[0];
    transform_orientation_ = orientation_slot.type;

    parent_transform_ = params.parent_transform;

    this->update_translate_transform_and_target_property(params, base_transform_from_socket);
    this->update_rotate_transform_and_target_property(params, base_transform_from_socket);
    this->update_scale_transform_and_target_property(params, base_transform_from_socket);
  }

  void update_visibility(GizmosUpdateParams &params, const NodeGeometryTransformGizmo &storage)
  {
    any_translation_visible_ = false;
    any_rotation_visible_ = false;
    any_scale_visible_ = false;

    const auto &elem = std::get<nodes::inverse_eval::MatrixElem>(params.elem.elem);

    for (const int axis : IndexRange(3)) {
      const bool translation_used = (storage.flag &
                                     (GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_X << axis)) &&
                                    elem.translation;
      const bool rotation_used = (storage.flag &
                                  (GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_X << axis)) &&
                                 elem.rotation;
      const bool scale_used = (storage.flag & (GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_X << axis)) &&
                              elem.scale;

      WM_gizmo_set_flag(translation_gizmos_[axis], WM_GIZMO_HIDDEN, !translation_used);
      WM_gizmo_set_flag(rotation_gizmos_[axis], WM_GIZMO_HIDDEN, !rotation_used);
      WM_gizmo_set_flag(scale_gizmos_[axis], WM_GIZMO_HIDDEN, !scale_used);

      any_translation_visible_ |= translation_used;
      any_rotation_visible_ |= rotation_used;
      any_scale_visible_ |= scale_used;
    }
  }

  void update_translate_style()
  {
    for (const int axis : IndexRange(3)) {
      wmGizmo *gizmo = translation_gizmos_[axis];
      get_axis_gizmo_colors(axis, gizmo->color, gizmo->color_hi);
      WM_gizmo_set_line_width(gizmo, 2.0f);

      float start = 0.0f;
      float length = 1.0f;
      if (any_rotation_visible_) {
        start = 1.125;
        length = 0.0f;
      }
      else if (any_scale_visible_) {
        start = 1.0f;
        length = 0.0f;
      }

      unit_m4(gizmo->matrix_offset);
      gizmo->matrix_offset[3][2] = start;
      RNA_float_set(gizmo->ptr, "length", length);
      WM_gizmo_set_flag(gizmo, WM_GIZMO_DRAW_OFFSET_SCALE, true);
    }
  }

  void update_rotate_style()
  {
    for (const int axis : IndexRange(3)) {
      wmGizmo *gizmo = rotation_gizmos_[axis];
      get_axis_gizmo_colors(axis, gizmo->color, gizmo->color_hi);

      const bool is_interacting = gizmo_is_interacting(*gizmo);
      int draw_options = RNA_enum_get(gizmo->ptr, "draw_options");
      /* The clipping currently looks a bit weird without the white circle around the gizmo.
       * However, without clipping it looks also very confusing sometimes. */
      draw_options |= ED_GIZMO_DIAL_DRAW_FLAG_CLIP;
      SET_FLAG_FROM_TEST(draw_options, is_interacting, ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_VALUE);
      RNA_enum_set(gizmo->ptr, "draw_options", draw_options);

      WM_gizmo_set_flag(gizmo, WM_GIZMO_DRAW_VALUE, true);
      WM_gizmo_set_line_width(gizmo, 3.0f);
      RNA_boolean_set(gizmo->ptr, "wrap_angle", false);
    }
  }

  void update_scale_style()
  {
    for (const int axis : IndexRange(3)) {
      wmGizmo *gizmo = scale_gizmos_[axis];
      get_axis_gizmo_colors(axis, gizmo->color, gizmo->color_hi);
      RNA_enum_set(gizmo->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_BOX);

      const float length = (any_translation_visible_ || any_rotation_visible_) ? 0.775f : 1.0f;
      RNA_float_set(gizmo->ptr, "length", length);

      WM_gizmo_set_line_width(gizmo, 2.0f);
    }
  }

  void update_translate_transform_and_target_property(GizmosUpdateParams &params,
                                                      const float4x4 &base_transform_from_socket)
  {
    for (const int axis_i : IndexRange(3)) {
      const math::Axis axis = math::Axis::from_int(axis_i);
      wmGizmo *gizmo = translation_gizmos_[axis_i];
      if (gizmo_is_interacting(*gizmo)) {
        continue;
      }

      const float4x4 gizmo_transform = get_axis_gizmo_matrix_basis(
          axis, base_transform_from_socket, params);
      copy_m4_m4(gizmo->matrix_basis, gizmo_transform.ptr());

      edit_data_.current_translation[axis_i] = 0.0f;

      wmGizmoPropertyFnParams params{};
      params.user_data = this;
      params.value_set_fn = [](const wmGizmo *gz,
                               wmGizmoProperty *gz_prop,
                               const void *value_ptr) {
        TransformGizmos &self = *static_cast<TransformGizmos *>(gz_prop->custom_func.user_data);
        const int axis_i = Span(self.translation_gizmos_).first_index(const_cast<wmGizmo *>(gz));
        const float new_gizmo_value = *static_cast<const float *>(value_ptr);
        self.edit_data_.current_translation[axis_i] = new_gizmo_value;
        float3 translation{};
        translation[axis_i] = new_gizmo_value;
        self.apply_change("Value", [&](bke::SocketValueVariant &value_variant) {
          float4x4 value = value_variant.get<float4x4>();
          const float3x3 orientation = float3x3(value);
          float3 offset{};
          if (self.transform_orientation_ == V3D_ORIENT_GLOBAL) {
            offset = math::transform_direction(math::invert(self.parent_transform_), translation);
          }
          else {
            const float factor = safe_divide(
                1.0f, math::length((self.parent_transform_.view<3, 3>() * orientation)[axis_i]));
            offset = math::transform_direction(orientation, translation) * factor;
          }
          value.location() += offset;
          value_variant.set(value);
        });
      };
      params.value_get_fn = [](const wmGizmo *gz, wmGizmoProperty *gz_prop, void *value_ptr) {
        TransformGizmos &self = *static_cast<TransformGizmos *>(gz_prop->custom_func.user_data);
        const int axis_i = Span(self.translation_gizmos_).first_index(const_cast<wmGizmo *>(gz));
        *static_cast<float *>(value_ptr) = self.edit_data_.current_translation[axis_i];
      };
      WM_gizmo_target_property_def_func(gizmo, "offset", &params);
    }
  }

  void update_rotate_transform_and_target_property(GizmosUpdateParams &params,
                                                   const float4x4 &base_transform_from_socket)
  {
    for (const int axis_i : IndexRange(3)) {
      const math::Axis axis = math::Axis::from_int(axis_i);
      wmGizmo *gizmo = rotation_gizmos_[axis_i];
      if (gizmo_is_interacting(*gizmo)) {
        continue;
      }

      const float4x4 gizmo_transform = get_axis_gizmo_matrix_basis(
          axis, base_transform_from_socket, params);
      copy_m4_m4(gizmo->matrix_basis, gizmo_transform.ptr());

      edit_data_.current_rotation[axis_i] = 0.0f;

      wmGizmoPropertyFnParams params{};
      params.user_data = this;
      params.value_set_fn = [](const wmGizmo *gz,
                               wmGizmoProperty *gz_prop,
                               const void *value_ptr) {
        TransformGizmos &self = *static_cast<TransformGizmos *>(gz_prop->custom_func.user_data);
        const int axis_i = Span(self.rotation_gizmos_).first_index(const_cast<wmGizmo *>(gz));
        const math::Axis axis = math::Axis::from_int(axis_i);
        const float new_gizmo_value = *static_cast<const float *>(value_ptr);
        self.edit_data_.current_rotation[axis_i] = new_gizmo_value;
        self.apply_change("Value", [&](bke::SocketValueVariant &value_variant) {
          float4x4 value = value_variant.get<float4x4>();
          float3 local_rotation_axis;
          if (self.transform_orientation_ == V3D_ORIENT_GLOBAL) {
            local_rotation_axis = math::normalize(math::transform_direction(
                math::invert(float3x3(self.parent_transform_)), math::to_vector<float3>(axis)));
          }
          else {
            local_rotation_axis = math::normalize(float3(value[axis_i]));
          }
          float3x3 rotation_matrix;
          rotation_matrix = math::from_rotation<float3x3>(
              math::AxisAngle(local_rotation_axis, -new_gizmo_value));
          value.view<3, 3>() = rotation_matrix * value.view<3, 3>();
          value_variant.set(value);
        });
      };
      params.value_get_fn = [](const wmGizmo *gz, wmGizmoProperty *gz_prop, void *value_ptr) {
        TransformGizmos &self = *static_cast<TransformGizmos *>(gz_prop->custom_func.user_data);
        const int axis_i = Span(self.rotation_gizmos_).first_index(const_cast<wmGizmo *>(gz));
        *static_cast<float *>(value_ptr) = self.edit_data_.current_rotation[axis_i];
      };
      WM_gizmo_target_property_def_func(gizmo, "offset", &params);
    }
  }

  void update_scale_transform_and_target_property(GizmosUpdateParams &params,
                                                  const float4x4 &base_transform_from_socket)
  {
    for (const int axis_i : IndexRange(3)) {
      const math::Axis axis = math::Axis::from_int(axis_i);
      wmGizmo *gizmo = scale_gizmos_[axis_i];
      if (gizmo_is_interacting(*gizmo)) {
        continue;
      }

      const float4x4 gizmo_transform = get_axis_gizmo_matrix_basis(
          axis, base_transform_from_socket, params);
      copy_m4_m4(gizmo->matrix_basis, gizmo_transform.ptr());

      edit_data_.current_scale[axis_i] = 0.0f;

      wmGizmoPropertyFnParams params{};
      params.user_data = this;
      params.value_set_fn = [](const wmGizmo *gz,
                               wmGizmoProperty *gz_prop,
                               const void *value_ptr) {
        TransformGizmos &self = *static_cast<TransformGizmos *>(gz_prop->custom_func.user_data);
        const int axis_i = Span(self.scale_gizmos_).first_index(const_cast<wmGizmo *>(gz));
        const math::Axis axis = math::Axis::from_int(axis_i);
        const float new_gizmo_value = *static_cast<const float *>(value_ptr);
        self.edit_data_.current_scale[axis_i] = new_gizmo_value;
        float3 scale{1.0f, 1.0f, 1.0f};
        scale[axis_i] += new_gizmo_value;
        self.apply_change("Value", [&](bke::SocketValueVariant &value_variant) {
          float4x4 value = value_variant.get<float4x4>();
          float3 local_scale_axis;
          if (self.transform_orientation_ == V3D_ORIENT_GLOBAL) {
            local_scale_axis = math::normalize(math::transform_direction(
                math::invert(float3x3(self.parent_transform_)), math::to_vector<float3>(axis)));
          }
          else {
            local_scale_axis = math::normalize(float3(value[axis_i]));
          }
          const float3x3 rotation_matrix = math::from_rotation<float3x3>(
              math::AxisAngle(local_scale_axis, math::to_vector<float3>(axis)));
          const float3x3 scale_matrix = math::invert(rotation_matrix) *
                                        math::from_scale<float3x3>(scale) * rotation_matrix;
          value.view<3, 3>() = scale_matrix * value.view<3, 3>();
          value_variant.set(value);
        });
      };
      params.value_get_fn = [](const wmGizmo *gz, wmGizmoProperty *gz_prop, void *value_ptr) {
        TransformGizmos &self = *static_cast<TransformGizmos *>(gz_prop->custom_func.user_data);
        const int axis_i = Span(self.scale_gizmos_).first_index(const_cast<wmGizmo *>(gz));
        *static_cast<float *>(value_ptr) = self.edit_data_.current_scale[axis_i];
      };
      WM_gizmo_target_property_def_func(gizmo, "offset", &params);
    }
  }

  float4x4 get_axis_gizmo_matrix_basis(const math::Axis axis,
                                       const float4x4 &base_transform_from_socket,
                                       const GizmosUpdateParams &params) const
  {
    float4x4 gizmo_transform;
    const float3 global_location =
        (params.parent_transform * base_transform_from_socket).location();
    const float3 axis_direction = math::to_vector<float3>(axis);
    float3 global_direction{};
    if (transform_orientation_ == V3D_ORIENT_GLOBAL) {
      global_direction = axis_direction;
    }
    else {
      global_direction = math::transform_direction(params.parent_transform.view<3, 3>() *
                                                       base_transform_from_socket.view<3, 3>(),
                                                   axis_direction);
    }
    global_direction = math::normalize(global_direction);
    gizmo_transform.location() = global_location;
    return matrix_from_position_and_up_direction(
        global_location, global_direction, math::AxisSigned::Z_POS);
  }
};

/** Uniquely identifies a gizmo node. */
struct GeoNodesObjectGizmoID {
  const Object *object_orig;
  bke::NodeGizmoID gizmo_id;

  BLI_STRUCT_EQUALITY_OPERATORS_2(GeoNodesObjectGizmoID, object_orig, gizmo_id)

  uint64_t hash() const
  {
    return get_default_hash(this->object_orig, this->gizmo_id);
  }
};

struct GeometryNodesGizmoGroup {
  /* Gizmos for all active gizmo nodes. */
  Map<GeoNodesObjectGizmoID, std::unique_ptr<NodeGizmos>> gizmos_by_node;
};

static std::unique_ptr<NodeGizmos> create_gizmo_node_gizmos(const bNode &gizmo_node)
{
  switch (gizmo_node.type_legacy) {
    case GEO_NODE_GIZMO_LINEAR:
      return std::make_unique<LinearGizmo>();
    case GEO_NODE_GIZMO_DIAL:
      return std::make_unique<DialGizmo>();
    case GEO_NODE_GIZMO_TRANSFORM:
      return std::make_unique<TransformGizmos>();
  }
  return {};
}

/** Finds the gizmo transform stored directly in the geometry, ignoring the instances. */
static const float4x4 *find_direct_gizmo_transform(const bke::GeometrySet &geometry,
                                                   const bke::NodeGizmoID &gizmo_id)
{
  if (const auto *edit_data_component = geometry.get_component<bke::GeometryComponentEditData>()) {
    if (edit_data_component->gizmo_edit_hints_) {
      if (const float4x4 *m = edit_data_component->gizmo_edit_hints_->gizmo_transforms.lookup_ptr(
              gizmo_id))
      {
        return m;
      }
    }
  }
  return nullptr;
}

/**
 * True, if the geometry contains a transform for the given gizmo. Also checks if all instances.
 */
static bool has_nested_gizmo_transform(const bke::GeometrySet &geometry,
                                       const bke::NodeGizmoID &gizmo_id)
{
  if (find_direct_gizmo_transform(geometry, gizmo_id)) {
    return true;
  }
  if (!geometry.has_instances()) {
    return false;
  }
  const bke::Instances *instances = geometry.get_instances();
  for (const bke::InstanceReference &reference : instances->references()) {
    if (reference.type() != bke::InstanceReference::Type::GeometrySet) {
      continue;
    }
    const bke::GeometrySet &reference_geometry = reference.geometry_set();
    if (has_nested_gizmo_transform(reference_geometry, gizmo_id)) {
      return true;
    }
  }
  return false;
}

static std::optional<float4x4> find_gizmo_geometry_transform_recursive(
    const bke::GeometrySet &geometry, const bke::NodeGizmoID &gizmo_id, const float4x4 &transform)
{
  if (const float4x4 *m = find_direct_gizmo_transform(geometry, gizmo_id)) {
    return transform * *m;
  }
  if (!geometry.has_instances()) {
    return std::nullopt;
  }
  const bke::Instances *instances = geometry.get_instances();
  const Span<bke::InstanceReference> references = instances->references();
  const Span<int> handles = instances->reference_handles();
  const Span<float4x4> transforms = instances->transforms();
  for (const int reference_i : references.index_range()) {
    const bke::InstanceReference &reference = references[reference_i];
    if (reference.type() != bke::InstanceReference::Type::GeometrySet) {
      continue;
    }
    const bke::GeometrySet &reference_geometry = reference.geometry_set();
    if (has_nested_gizmo_transform(reference_geometry, gizmo_id)) {
      const int index = handles.first_index_try(reference_i);
      if (index >= 0) {
        const float4x4 sub_transform = transform * transforms[index];
        if (const std::optional<float4x4> m = find_gizmo_geometry_transform_recursive(
                reference_geometry, gizmo_id, sub_transform))
        {
          return m;
        }
      }
    }
  }
  return std::nullopt;
}

/**
 * Find the geometry that the gizmo should be drawn for. This is generally either the final
 * evaluated geometry or the viewer geometry.
 */
static bke::GeometrySet find_geometry_for_gizmo(const Object &object_eval,
                                                const NodesModifierData &nmd_orig,
                                                const View3D &v3d)
{
  if (v3d.flag2 & V3D_SHOW_VIEWER) {
    const ViewerPath &viewer_path = v3d.viewer_path;
    if (const geo_eval_log::ViewerNodeLog *viewer_log =
            nmd_orig.runtime->eval_log->find_viewer_node_log_for_path(viewer_path))
    {
      if (const bke::GeometrySet *viewer_geometry = viewer_log->main_geometry()) {
        return *viewer_geometry;
      }
    }
  }
  return bke::object_get_evaluated_geometry_set(object_eval);
}

/**
 * Tries to find a transformation of the gizmo in the given geometry.
 */
static std::optional<float4x4> find_gizmo_geometry_transform(const bke::GeometrySet &geometry,
                                                             const bke::NodeGizmoID &gizmo_id)
{
  const float4x4 identity = float4x4::identity();
  return find_gizmo_geometry_transform_recursive(geometry, gizmo_id, identity);
}

static bool WIDGETGROUP_geometry_nodes_poll(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  ScrArea *area = CTX_wm_area(C);
  View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  if (v3d->gizmo_flag & V3D_GIZMO_HIDE_MODIFIER) {
    return false;
  }
  return true;
}

static void WIDGETGROUP_geometry_nodes_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  GeometryNodesGizmoGroup *gzgroup_data = MEM_new<GeometryNodesGizmoGroup>(__func__);
  gzgroup->customdata = gzgroup_data;
  gzgroup->customdata_free = [](void *data) {
    auto *gzgroup_data = static_cast<GeometryNodesGizmoGroup *>(data);
    MEM_delete(gzgroup_data);
  };
}

static void WIDGETGROUP_geometry_nodes_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  auto &gzgroup_data = *static_cast<GeometryNodesGizmoGroup *>(gzgroup->customdata);

  View3D *v3d = CTX_wm_view3d(C);
  if (!v3d) {
    return;
  }

  const wmWindowManager *wm = CTX_wm_manager(C);
  if (wm == nullptr) {
    return;
  }
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  /* A new map containing the active gizmos is build. This is less error prone than trying to
   * update the old map in place. */
  Map<GeoNodesObjectGizmoID, std::unique_ptr<NodeGizmos>> new_gizmos_by_node;

  /* This needs to stay around for a bit longer because the compute contexts are required when
   * applying the gizmo changes. */
  auto compute_context_cache = std::make_shared<bke::ComputeContextCache>();

  nodes::gizmos::foreach_active_gizmo(
      *C,
      *compute_context_cache,
      [&](const Object &object_orig,
          const NodesModifierData &nmd_orig,
          const ComputeContext &compute_context,
          const bNode &gizmo_node,
          const bNodeSocket &gizmo_socket) {
        const GeoNodesObjectGizmoID gizmo_id = {&object_orig,
                                                {compute_context.hash(), gizmo_node.identifier}};
        if (new_gizmos_by_node.contains(gizmo_id)) {
          /* Already handled. */
          return;
        }
        if (!nmd_orig.runtime->eval_log) {
          /* Can't create gizmos without any logged data. */
          return;
        }
        Object *object_eval = DEG_get_evaluated(depsgraph, const_cast<Object *>(&object_orig));
        if (!object_eval) {
          return;
        }

        const bke::GeometrySet geometry = find_geometry_for_gizmo(*object_eval, nmd_orig, *v3d);

        /* Figure out which parts of the gizmo are editable. */
        const nodes::inverse_eval::ElemVariant elem = nodes::gizmos::get_editable_gizmo_elem(
            compute_context, gizmo_node, gizmo_socket);

        bNodeTree &ntree = *nmd_orig.node_group;
        ntree.ensure_topology_cache();

        NodeGizmos *node_gizmos = nullptr;
        if (std::optional<std::unique_ptr<NodeGizmos>> old_gizmos =
                gzgroup_data.gizmos_by_node.pop_try(gizmo_id))
        {
          /* Gizmos for this node existed already, reuse them. */
          node_gizmos = old_gizmos->get();
          new_gizmos_by_node.add(gizmo_id, std::move(*old_gizmos));
        }
        else {
          /* There are no gizmos for this node yet, create new ones. */
          std::unique_ptr<NodeGizmos> new_node_gizmos = create_gizmo_node_gizmos(gizmo_node);
          new_node_gizmos->create_gizmos(*gzgroup);
          /* Enable undo for all geometry nodes gizmos. */
          for (wmGizmo *gizmo : new_node_gizmos->get_all_gizmos()) {
            gizmo->flag |= WM_GIZMO_NEEDS_UNDO;
          }
          node_gizmos = new_node_gizmos.get();
          new_gizmos_by_node.add(gizmo_id, std::move(new_node_gizmos));
        }

        /* Initially show all gizmos. They may be hidden as part of the update again. */
        node_gizmos->show_all();

        GeoTreeLog &tree_log = nmd_orig.runtime->eval_log->get_tree_log(compute_context.hash());
        tree_log.ensure_socket_values();
        tree_log.ensure_evaluated_gizmo_nodes();

        const std::optional<float4x4> crazy_space_geometry_transform =
            find_gizmo_geometry_transform(geometry, gizmo_id.gizmo_id);

        const bool missing_logged_data = !tree_log.evaluated_gizmo_nodes.contains(
            gizmo_node.identifier);
        if (missing_logged_data) {
          /* Rerun modifier to make sure that values are logged. */
          DEG_id_tag_update_for_side_effect_request(
              depsgraph, const_cast<ID *>(&object_orig.id), ID_RECALC_GEOMETRY);
          WM_main_add_notifier(NC_GEOM | ND_DATA, nullptr);
          node_gizmos->hide_all();
          return;
        }
        const bool missing_used_transform = gizmo_node.output_socket(0).is_logically_linked() &&
                                            !crazy_space_geometry_transform.has_value();
        if (missing_used_transform) {
          node_gizmos->hide_all();
          return;
        }

        const float4x4 object_to_world{object_eval->object_to_world()};
        const float4x4 geometry_transform = crazy_space_geometry_transform.value_or(
            float4x4::identity());

        UpdateReport report;
        GizmosUpdateParams update_params{
            *C, object_to_world * geometry_transform, gizmo_node, tree_log, report, elem};
        node_gizmos->update(update_params);

        bool any_interacting = node_gizmos->is_any_interacting();

        if (!any_interacting) {
          if (report.missing_socket_logs || report.invalid_transform) {
            /* Avoid showing gizmos which are in the wrong place. */
            node_gizmos->hide_all();
            return;
          }
          /* Update the callback to apply gizmo changes based on the new context. */
          node_gizmos->apply_change =
              [C = C,
               compute_context_cache,
               compute_context = &compute_context,
               gizmo_node_tree = &gizmo_node.owner_tree(),
               gizmo_node = &gizmo_node,
               object_orig = &object_orig,
               nmd = &nmd_orig,
               eval_log = nmd_orig.runtime->eval_log](
                  const StringRef socket_identifier,
                  const FunctionRef<void(bke::SocketValueVariant &)> modify_value) {
                gizmo_node_tree->ensure_topology_cache();
                const bNodeSocket &socket = *gizmo_node->input_by_identifier(socket_identifier);

                nodes::gizmos::apply_gizmo_change(*const_cast<bContext *>(C),
                                                  const_cast<Object &>(*object_orig),
                                                  const_cast<NodesModifierData &>(*nmd),
                                                  *eval_log,
                                                  *compute_context,
                                                  socket,
                                                  modify_value);

                Main *main = CTX_data_main(C);
                BKE_main_ensure_invariants(*main);
                WM_main_add_notifier(NC_GEOM | ND_DATA, nullptr);
              };
        }
      });

  /* Hide all except the interacting gizmo. */
  bool any_gizmo_interactive = false;
  for (const std::unique_ptr<NodeGizmos> &node_gizmos : new_gizmos_by_node.values()) {
    any_gizmo_interactive |= node_gizmos->is_any_interacting();
  }
  if (any_gizmo_interactive) {
    for (std::unique_ptr<NodeGizmos> &node_gizmos : new_gizmos_by_node.values()) {
      for (wmGizmo *gizmo : node_gizmos->get_all_gizmos()) {
        if (!gizmo_is_interacting(*gizmo)) {
          WM_gizmo_set_flag(gizmo, WM_GIZMO_HIDDEN, true);
        }
      }
    }
  }

  /* Remove gizmos that are not used anymore. */
  for (std::unique_ptr<NodeGizmos> &node_gizmos : gzgroup_data.gizmos_by_node.values()) {
    const Vector<wmGizmo *> gizmos = node_gizmos->get_all_gizmos();
    for (wmGizmo *gizmo : gizmos) {
      WM_gizmo_unlink(&gzgroup->gizmos, gzgroup->parent_gzmap, gizmo, const_cast<bContext *>(C));
    }
  }

  gzgroup_data.gizmos_by_node = std::move(new_gizmos_by_node);
}

static void WIDGETGROUP_geometry_nodes_draw_prepare(const bContext * /*C*/,
                                                    wmGizmoGroup * /*gzgroup*/)
{
}

}  // namespace blender::ed::view3d::geometry_nodes_gizmos

void VIEW3D_GGT_geometry_nodes(wmGizmoGroupType *gzgt)
{
  using namespace blender::ed::view3d::geometry_nodes_gizmos;

  gzgt->name = "Geometry Nodes Widgets";
  gzgt->idname = "VIEW3D_GGT_geometry_nodes";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_PERSISTENT | WM_GIZMOGROUPTYPE_3D);

  gzgt->poll = WIDGETGROUP_geometry_nodes_poll;
  gzgt->setup = WIDGETGROUP_geometry_nodes_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->refresh = WIDGETGROUP_geometry_nodes_refresh;
  gzgt->draw_prepare = WIDGETGROUP_geometry_nodes_draw_prepare;
}
