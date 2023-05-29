/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_mesh_flip_faces.hh"

#include "DNA_mesh_types.h"

#include "BLI_task.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_mesh.hh"

namespace blender::geometry {

void flip_faces(Mesh &mesh, const IndexMask &selection)
{
  if (mesh.totpoly == 0 || selection.is_empty()) {
    return;
  }

  const OffsetIndices polys = mesh.polys();
  MutableSpan<int> corner_verts = mesh.corner_verts_for_write();
  MutableSpan<int> corner_edges = mesh.corner_edges_for_write();

  selection.foreach_index(GrainSize(1024), [&](const int i) {
    const IndexRange poly = polys[i];
    for (const int j : IndexRange(poly.size() / 2)) {
      const int a = poly[j + 1];
      const int b = poly.last(j);
      std::swap(corner_verts[a], corner_verts[b]);
      std::swap(corner_edges[a - 1], corner_edges[b]);
    }
  });

  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  attributes.for_all(
      [&](const bke::AttributeIDRef &attribute_id, const bke::AttributeMetaData &meta_data) {
        if (meta_data.data_type == CD_PROP_STRING) {
          return true;
        }
        if (meta_data.domain != ATTR_DOMAIN_CORNER) {
          return true;
        }
        if (ELEM(attribute_id.name(), ".corner_vert", ".corner_edge")) {
          return true;
        }
        bke::GSpanAttributeWriter attribute = attributes.lookup_for_write_span(attribute_id);
        bke::attribute_math::convert_to_static_type(meta_data.data_type, [&](auto dummy) {
          using T = decltype(dummy);
          MutableSpan<T> dst_span = attribute.span.typed<T>();
          selection.foreach_index(GrainSize(1024), [&](const int i) {
            dst_span.slice(polys[i].drop_front(1)).reverse();
          });
        });
        attribute.finish();
        return true;
      });

  BKE_mesh_tag_face_winding_changed(&mesh);
}

}  // namespace blender::geometry
