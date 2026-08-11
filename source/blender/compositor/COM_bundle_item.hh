/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

#include "NOD_geometry_nodes_bundle.hh"

#include "COM_result.hh"

namespace blender::compositor {

class Context;

/* A thin wrapper around a compositor result to be added and retrieved from bundles. */
class BundleItem : public nodes::BundleItemInternalValueMixin {
 private:
  Result result_;

 public:
  /* Constructs the bundle item by shallow copying the given result to the result_ member. */
  BundleItem(Context &context, Result &result);

  /* Constructs a new BundleItemValue to be inserted into a bundle, which is essentially a wrapper
   * around the constructor. */
  static nodes::BundleItemValue new_bundle_item_value(Context &context, Result &result);

  /* Get a copy of the result stored inside the bundle item given a bundle item value. Releasing
   * the returned result is the caller's responsibility. */
  static Result get_result(Context &context, const nodes::BundleItemValue &item_value);

  /* A name for logging. */
  StringRefNull type_name() const override;

 private:
  /* Release the stored result and delete self. */
  void delete_self() override;
};

using BundleItemPtr = ImplicitSharingPtr<BundleItem>;

}  // namespace blender::compositor
