/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "curves_sculpt_intern.hh"

#include "ED_curves_sculpt.hh"

namespace blender::ed::sculpt_paint {

bke::SpanAttributeWriter<float> float_selection_ensure(Curves &curves_id)
{
  /* TODO: Use a generic attribute conversion utility instead of this function. */
  bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

  if (const auto meta_data = attributes.lookup_meta_data(".selection")) {
    if (meta_data->data_type == CD_PROP_BOOL) {
      const VArray<float> selection = *attributes.lookup<float>(".selection");
      float *dst = static_cast<float *>(
          MEM_malloc_arrayN(selection.size(), sizeof(float), __func__));
      selection.materialize({dst, selection.size()});

      attributes.remove(".selection");
      attributes.add(
          ".selection", meta_data->domain, CD_PROP_FLOAT, bke::AttributeInitMoveArray(dst));
    }
  }
  else {
    const bke::AttrDomain domain = bke::AttrDomain(curves_id.selection_domain);
    const int64_t size = attributes.domain_size(domain);
    attributes.add(".selection",
                   domain,
                   CD_PROP_FLOAT,
                   bke::AttributeInitVArray(VArray<float>::ForSingle(1.0f, size)));
  }

  return curves.attributes_for_write().lookup_for_write_span<float>(".selection");
}

}  // namespace blender::ed::sculpt_paint
