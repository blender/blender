/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_implicit_sharing.hh"

#include "BKE_curves.hh"

#include "sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

bke::SpanAttributeWriter<float> float_selection_ensure(Curves &curves_id)
{
  /* TODO: Use a generic attribute conversion utility instead of this function. */
  bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

  if (const auto meta_data = attributes.lookup_meta_data(".selection")) {
    if (meta_data->data_type == bke::AttrType::Bool) {
      const VArray<float> selection = *attributes.lookup<float>(".selection");
      auto *dst = new ImplicitSharedValue<Array<float>>(selection.size());
      selection.materialize(dst->data);

      attributes.remove(".selection");
      attributes.add(".selection",
                     meta_data->domain,
                     bke::AttrType::Float,
                     bke::AttributeInitShared(dst->data.data(), *dst));
      dst->remove_user_and_delete_if_last();
    }
  }
  else {
    attributes.add(".selection",
                   bke::AttrDomain(curves_id.selection_domain),
                   bke::AttrType::Float,
                   bke::AttributeInitValue(1.0f));
  }

  return curves.attributes_for_write().lookup_for_write_span<float>(".selection");
}

}  // namespace blender::ed::sculpt_paint
