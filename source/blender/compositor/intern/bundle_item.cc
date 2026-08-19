/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_ref.hh"

#include "COM_bundle_item.hh"
#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::compositor {

BundleItem::BundleItem(Context &context, Result &result)
    : result_(context.create_result(result.type(), result.precision()))
{
  result_.share_data(result);
}

nodes::BundleItemValue BundleItem::new_bundle_item_value(Context &context, Result &result)
{
  return nodes::BundleItemValue{nodes::BundleItemInternalValue{
      ImplicitSharingPtr<const BundleItem>(new BundleItem(context, result))}};
}

Result BundleItem::get_result(Context &context, const nodes::BundleItemValue &item_value)
{
  /* Handle a special case where nested bundles might be stored in BundleItemSocketValue as
   * opposed to BundleItemInternalValue. */
  std::optional<nodes::BundlePtr> child_bundle = item_value.as<nodes::BundlePtr>();
  if (child_bundle.has_value()) {
    Result result = context.create_result(ResultType::Bundle);
    result.allocate_single_value();
    result.set_single_value(child_bundle.value());
    return result;
  }

  /* Otherwise, the item value is stored in a BundleItemInternalValue. */
  BLI_assert(std::get_if<nodes::BundleItemInternalValue>(&item_value.value));
  std::optional<BundleItemPtr> bundle_item = item_value.as<BundleItemPtr>();
  BLI_assert(bundle_item.has_value());

  const Result &bundle_result = bundle_item.value()->result_;
  Result result = context.create_result(bundle_result.type(), bundle_result.precision());
  result.share_data(bundle_result);
  return result;
}

StringRefNull BundleItem::type_name() const
{
  return "Compositor Bundle Item";
}

void BundleItem::delete_self()
{
  result_.release();
  delete this;
}

}  // namespace blender::compositor
