/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "BLI_task.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_mesh.hh"

namespace blender::bke {

template<typename T>
static void flip_corner_data(const OffsetIndices<int> faces,
                             const IndexMask &face_selection,
                             MutableSpan<T> data)
{
  face_selection.foreach_index(GrainSize(1024),
                               [&](const int i) { data.slice(faces[i].drop_front(1)).reverse(); });
}

template<typename T>
static void flip_custom_data_type(const OffsetIndices<int> faces,
                                  CustomData &loop_data,
                                  const IndexMask &face_selection,
                                  const eCustomDataType data_type)
{
  BLI_assert(sizeof(T) == CustomData_sizeof(data_type));
  for (const int i : IndexRange(CustomData_number_of_layers(&loop_data, data_type))) {
    T *data = static_cast<T *>(
        CustomData_get_layer_n_for_write(&loop_data, data_type, i, faces.total_size()));
    flip_corner_data(faces, face_selection, MutableSpan(data, faces.total_size()));
  }
}

void mesh_flip_faces(Mesh &mesh, const IndexMask &selection)
{
  if (mesh.faces_num == 0 || selection.is_empty()) {
    return;
  }

  const OffsetIndices faces = mesh.faces();
  MutableSpan<int> corner_verts = mesh.corner_verts_for_write();
  MutableSpan<int> corner_edges = mesh.corner_edges_for_write();

  selection.foreach_index(GrainSize(1024), [&](const int i) {
    const IndexRange face = faces[i];
    for (const int j : IndexRange(face.size() / 2)) {
      const int a = face[j + 1];
      const int b = face.last(j);
      std::swap(corner_verts[a], corner_verts[b]);
      std::swap(corner_edges[a - 1], corner_edges[b]);
    }
  });

  flip_custom_data_type<float4x4>(faces, mesh.loop_data, selection, CD_TANGENT);
  flip_custom_data_type<float4>(faces, mesh.loop_data, selection, CD_MLOOPTANGENT);
  flip_custom_data_type<short2>(faces, mesh.loop_data, selection, CD_CUSTOMLOOPNORMAL);
  flip_custom_data_type<float>(faces, mesh.loop_data, selection, CD_PAINT_MASK);
  flip_custom_data_type<GridPaintMask>(faces, mesh.loop_data, selection, CD_GRID_PAINT_MASK);
  flip_custom_data_type<OrigSpaceLoop>(faces, mesh.loop_data, selection, CD_ORIGSPACE_MLOOP);
  flip_custom_data_type<MDisps>(faces, mesh.loop_data, selection, CD_MDISPS);
  if (MDisps *mdisp = static_cast<MDisps *>(
          CustomData_get_layer_for_write(&mesh.loop_data, CD_MDISPS, mesh.totloop)))
  {
    selection.foreach_index(GrainSize(512), [&](const int i) {
      for (const int corner : faces[i]) {
        BKE_mesh_mdisp_flip(&mdisp[corner], true);
      }
    });
  }

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
          flip_corner_data(faces, selection, attribute.span.typed<T>());
        });
        attribute.finish();
        return true;
      });

  BKE_mesh_tag_face_winding_changed(&mesh);
}

}  // namespace blender::bke
