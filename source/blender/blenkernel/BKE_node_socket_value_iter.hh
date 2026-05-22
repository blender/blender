/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_function_ref.hh"

namespace blender::fn {
class GField;
}

namespace blender::bke {

struct GeometrySet;
class SocketValueVariant;
class AttributeAccessor;
class MutableAttributeAccessor;

}  // namespace blender::bke

namespace blender::bke::socket_value_visitor {

/**
 * Besides general iteration settings, this contains two kinds of callbacks:
 * - `check_*`: Gets readonly access to a value which should return either `needs_edit(...)` or
 *    `continue_check(...)` depending on the use-case.
 * - `edit_*`: Gets write access to a value which should be edited.
 */
struct VisitParams {
  /** Used when this a check function is used to determine whether the value needs to be edited. */
  static bool needs_edit(const bool value)
  {
    return value;
  }
  /** Used when just scanning all values recursively without the intention to edit them. */
  static bool continue_check(const bool value)
  {
    return !value;
  }

  /**
   * Also iterate over:
   * - Captured closure values, and closure default inputs.
   */
  bool check_non_editable = false;
  /**
   * By default, all instanced objects/collections are also iterated over. Those can optionally be
   * ignored.
   */
  bool ignore_non_geometry_instances = false;

  /* Note, since these are #FunctionRef, you must not assign a lambda to them directly! Instead the
   * lambda must be stored in a separate variable first. */

  FunctionRef<bool(const AttributeAccessor &)> check_AttributeAccessor;
  FunctionRef<void(MutableAttributeAccessor &)> edit_AttributeAccessor;

  FunctionRef<bool(const fn::GField &)> check_GField;
  FunctionRef<void(fn::GField &)> edit_GField;
};

/**
 * Recursively iterate over the contained values. This requires that both `check_*` and `edit_*`
 * callbacks are provided. The editing happens in two passes, first it is checked whether something
 * needs to be edited before it is actually done. This is necessary to avoid breaking implicit
 * sharing when nothing is changed.
 */
void edit_recursive(SocketValueVariant &value, const VisitParams &params);
void edit_recursive(GeometrySet &value, const VisitParams &params);

/**
 * Recursively call the provided `check_*` callbacks on the contained values.
 */
void check_recursive(const SocketValueVariant &value, const VisitParams &params);
void check_recursive(const GeometrySet &value, const VisitParams &params);

}  // namespace blender::bke::socket_value_visitor
