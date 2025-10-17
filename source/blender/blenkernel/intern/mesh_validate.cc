/* SPDX-FileCopyrightText: 2011-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <climits>
#include <fmt/format.h>

#include "BKE_attribute_legacy_convert.hh"
#include "BKE_attribute_math.hh"
#include "CLG_log.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_index_ranges_builder.hh"
#include "BLI_ordered_edge.hh"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"

#include "DEG_depsgraph.hh"

static CLG_LogRef LOG = {"geom.mesh"};

namespace blender::bke {

/* Helper class to collect error messages in parallel. */
class ErrorMessages {
  Mutex mutex_;
  bool verbose_;

 public:
  Vector<std::string> messages;

  ErrorMessages(const bool verbose) : verbose_(verbose) {}
  ~ErrorMessages()
  {
    std::sort(messages.begin(), messages.end());
    for (const std::string &message : messages) {
      CLOG_ERROR(&LOG, "%s", message.c_str());
    }
  }

  void add(std::string message)
  {
    if (!verbose_) {
      return;
    }
    std::lock_guard lock(mutex_);
    this->messages.append(std::move(message));
  }

  template<typename... T> void add(fmt::format_string<T...> fmt, T &&...args)
  {
    if (!verbose_) {
      return;
    }
    this->add(fmt::format(fmt, args...));
  }
};

template<typename... T>
static void print_error_with_indices(const IndexMask &mask,
                                     fmt::format_string<T...> fmt,
                                     T &&...args)
{
  fmt::memory_buffer buffer;
  fmt::appender dst(buffer);
  fmt::format_to(dst, fmt, std::forward<T>(args)...);
  fmt::format_to(dst, " at indices: ");
  mask.foreach_index([&](const int index, const int pos) {
    if (pos != 0) {
      fmt::format_to(dst, ", ");
    }
    fmt::format_to(dst, "{}", index);
  });
  CLOG_ERROR(&LOG, "%s", fmt::to_string(buffer).c_str());
}

static IndexMask find_edges_bad_verts(const Mesh &mesh,
                                      IndexMaskMemory &memory,
                                      const bool verbose)
{
  const IndexRange verts_range(mesh.verts_num);
  const Span<int2> edges = mesh.edges();

  ErrorMessages errors(verbose);
  return IndexMask::from_predicate(
      edges.index_range(), GrainSize(4096), memory, [&](const int edge_i) {
        const int2 edge = edges[edge_i];
        if (edge[0] == edge[1]) {
          errors.add("Edge {} has equal vertex indices {}", edge_i, edge[0]);
          return true;
        }
        if (!verts_range.contains(edge[0]) || !verts_range.contains(edge[1])) {
          errors.add("Edge {} has out of range vertex ({}, {})", edge_i, edge[0], edge[1]);
          return true;
        }
        return false;
      });
}

using EdgeMap = VectorSet<OrderedEdge,
                          32,
                          DefaultProbingStrategy,
                          DefaultHash<OrderedEdge>,
                          DefaultEquality<OrderedEdge>,
                          SimpleVectorSetSlot<OrderedEdge, int>,
                          GuardedAllocator>;

static IndexMask find_edges_duplicates(const Mesh &mesh,
                                       const IndexMask &mask,
                                       IndexMaskMemory &memory,
                                       const bool verbose,
                                       EdgeMap &unique_edges)
{
  const Span<int2> edges = mesh.edges();
  BitVector<> duplicate_edges(edges.size());
  ErrorMessages errors(verbose);
  mask.foreach_index([&](const int edge_i) {
    const int2 edge = edges[edge_i];
    if (!unique_edges.add(edge)) {
      errors.add("Edge {} is a duplicate of {}", edge_i, unique_edges.index_of(edge));
      duplicate_edges[edge_i].set();
    }
  });
  return IndexMask::from_bits(mask, duplicate_edges, memory);
}

static IndexMask find_faces_bad_offsets(const Mesh &mesh,
                                        IndexMaskMemory &memory,
                                        const bool verbose)
{
  if (mesh.faces_num == 0) {
    return {};
  }
  const Span<int> face_offsets = mesh.face_offsets();
  ErrorMessages errors(verbose);
  if (face_offsets.last() != mesh.corners_num) {
    errors.add("Face offsets last value is {}, expected {}. Considering all faces invalid",
               face_offsets.last(),
               mesh.corners_num);
    return IndexMask(mesh.faces_num);
  }
  if (face_offsets.first() != 0) {
    errors.add("Faces offsets do not start at 0. Considering all faces invalid");
    return IndexMask(mesh.faces_num);
  }
  return IndexMask::from_predicate(
      IndexRange(mesh.faces_num), GrainSize(4096), memory, [&](const int face_i) {
        const int face_start = face_offsets[face_i];
        const int face_size = face_offsets[face_i + 1] - face_start;
        if (face_size < 3) {
          errors.add("Face {} has invalid size {}", face_i, face_size);
          return true;
        }
        return false;
      });
}

static IndexMask find_faces_bad_verts(const Mesh &mesh,
                                      const IndexMask &mask,
                                      IndexMaskMemory &memory,
                                      const bool verbose)
{
  const IndexRange verts_range(mesh.verts_num);
  const OffsetIndices<int> faces(mesh.face_offsets(), offset_indices::NoSortCheck());
  const Span<int> corner_verts = mesh.corner_verts();
  ErrorMessages errors(verbose);
  return IndexMask::from_predicate(mask, GrainSize(512), memory, [&](const int face_i) {
    const IndexRange face = faces[face_i];
    for (const int vert : corner_verts.slice(face)) {
      if (!verts_range.contains(vert)) {
        errors.add("Face {} has invalid vertex {}", face_i, vert);
        return true;
      }
    }
    return false;
  });
}

static IndexMask find_faces_duplicate_verts(const Mesh &mesh,
                                            const IndexMask &mask,
                                            IndexMaskMemory &memory,
                                            const bool verbose)
{
  const IndexRange verts_range(mesh.verts_num);
  const OffsetIndices<int> faces(mesh.face_offsets(), offset_indices::NoSortCheck());
  const Span<int> corner_verts = mesh.corner_verts();
  ErrorMessages errors(verbose);
  return IndexMask::from_predicate(mask, GrainSize(512), memory, [&](const int face_i) {
    Set<int, 16> set;
    const IndexRange face = faces[face_i];
    for (const int vert : corner_verts.slice(face)) {
      if (!set.add(vert)) {
        errors.add("Face {} has duplicate vertex {}", face_i, vert);
        return true;
      }
    }
    return false;
  });
}

static IndexMask find_faces_missing_edges(const Mesh &mesh,
                                          const IndexMask &mask,
                                          const EdgeMap &unique_edges,
                                          IndexMaskMemory &memory,
                                          const bool verbose)
{
  const IndexRange edges_range(mesh.edges_num);
  const OffsetIndices<int> faces(mesh.face_offsets(), offset_indices::NoSortCheck());
  const Span<int> corner_verts = mesh.corner_verts();

  ErrorMessages errors(verbose);
  return IndexMask::from_predicate(mask, GrainSize(1024), memory, [&](const int face_i) {
    const IndexRange face = faces[face_i];
    for (const int corner : face) {
      const int corner_next = mesh::face_corner_next(face, corner);
      const OrderedEdge actual_edge(corner_verts[corner], corner_verts[corner_next]);
      if (!unique_edges.contains(actual_edge)) {
        errors.add(
            "Face {} has missing edge ({}, {})", face_i, actual_edge.v_low, actual_edge.v_high);
        return true;
      }
    }
    return false;
  });
}

static IndexMask find_faces_bad_edges(const Mesh &mesh,
                                      const IndexMask &mask,
                                      const EdgeMap &unique_edges,
                                      IndexMaskMemory &memory,
                                      const bool verbose,
                                      Vector<Vector<std::pair<int, int>>> &r_corner_edge_fixes)
{
  const IndexRange edges_range(mesh.edges_num);
  const Span<int2> edges = mesh.edges();
  const OffsetIndices<int> faces(mesh.face_offsets(), offset_indices::NoSortCheck());
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int> corner_edges = mesh.corner_edges();

  ErrorMessages errors(verbose);
  threading::EnumerableThreadSpecific<Vector<std::pair<int, int>>> all_replacements;
  IndexMask faces_bad_edges = IndexMask::from_batch_predicate(
      mask,
      GrainSize(4096),
      memory,
      [&](const IndexMaskSegment universe_segment, IndexRangesBuilder<int16_t> &builder) {
        Vector<std::pair<int, int>> &replacements = all_replacements.local();
        for (const int face_i : universe_segment) {
          const IndexRange face = faces[face_i];

          bool has_invalid_edge = false;
          for (const int corner : face) {
            const int corner_next = mesh::face_corner_next(face, corner);
            const OrderedEdge actual_edge(corner_verts[corner], corner_verts[corner_next]);
            const int actual_edge_index = unique_edges.index_of(actual_edge);
            const int edge_index = corner_edges[corner];
            if (!edges_range.contains(edge_index)) {
              errors.add("Corner {} has out of range edge index {}. Expected {}",
                         corner,
                         edge_index,
                         actual_edge_index);
              replacements.append({corner, actual_edge_index});
              has_invalid_edge = true;
              continue;
            }
            if (OrderedEdge(edges[edge_index]) != actual_edge) {
              errors.add("Corner {} has incorrect edge index {}. Expected {}",
                         corner,
                         edge_index,
                         actual_edge_index);
              replacements.append({corner, actual_edge_index});
              has_invalid_edge = true;
              continue;
            }
          }
          if (has_invalid_edge) {
            builder.add(face_i);
          }
        }
        return universe_segment.offset();
      });

  for (Vector<std::pair<int, int>> &replacements : all_replacements) {
    if (!replacements.is_empty()) {
      r_corner_edge_fixes.append(std::move(replacements));
    }
  }

  return faces_bad_edges;
}

static IndexMask find_duplicate_faces(const Mesh &mesh,
                                      const IndexMask &mask,
                                      IndexMaskMemory &memory,
                                      const bool verbose)
{
  const OffsetIndices<int> faces(mesh.face_offsets(), offset_indices::NoSortCheck());
  const Span<int> corner_verts = mesh.corner_verts();

  Array<int> sorted_corner_verts(mesh.corners_num);
  mask.foreach_index(GrainSize(1024), [&](const int face_i) {
    const IndexRange face = faces[face_i];
    MutableSpan<int> sorted_face_verts = sorted_corner_verts.as_mutable_span().slice(face);
    sorted_face_verts.copy_from(corner_verts.slice(face));
    std::sort(sorted_face_verts.begin(), sorted_face_verts.end());
  });

  using FaceMap = VectorSet<Span<int>,
                            32,
                            DefaultProbingStrategy,
                            DefaultHash<Span<int>>,
                            DefaultEquality<Span<int>>,
                            SimpleVectorSetSlot<Span<int>, int>>;

  FaceMap face_hash;
  face_hash.reserve(mesh.faces_num);

  BitVector<> duplicate_faces(mesh.faces_num, false);
  ErrorMessages errors(verbose);
  mask.foreach_index([&](int face_i) {
    const IndexRange face = faces[face_i];
    const Span<int> face_verts = sorted_corner_verts.as_span().slice(face);
    if (!face_hash.add(face_verts)) {
      errors.add("Face {} is a duplicate of {}", face_i, face_hash.index_of(face_verts));
      duplicate_faces[face_i].set();
    }
  });

  return IndexMask::from_bits(duplicate_faces, memory);
}

static void remove_invalid_faces(Mesh &mesh, const IndexMask &valid_faces)
{
  const int valid_faces_num = valid_faces.size();
  const OffsetIndices<int> old_faces(mesh.face_offsets(), offset_indices::NoSortCheck());

  Vector<int> new_face_offsets(valid_faces_num + 1);
  const OffsetIndices new_faces = offset_indices::gather_selected_offsets(
      old_faces, valid_faces, new_face_offsets);

  for (CustomDataLayer &layer : MutableSpan(mesh.face_data.layers, mesh.face_data.totlayer)) {
    const eCustomDataType cd_type = eCustomDataType(layer.type);
    if (CD_TYPE_AS_MASK(cd_type) & CD_MASK_PROP_ALL) {
      const CPPType &type = *bke::custom_data_type_to_cpp_type(cd_type);
      const GSpan src(type, layer.data, mesh.faces_num);

      void *dst_data = MEM_malloc_arrayN(valid_faces_num, type.size, __func__);
      GMutableSpan dst(type, dst_data, valid_faces_num);

      array_utils::gather(src, valid_faces, dst);

      layer.sharing_info->remove_user_and_delete_if_last();
      layer.data = dst_data;
      layer.sharing_info = implicit_sharing::info_for_mem_free(dst_data);
    }
    else if (cd_type == CD_ORIGINDEX) {
      const Span src(static_cast<const int *>(layer.data), mesh.edges_num);

      int *dst_data = MEM_malloc_arrayN<int>(valid_faces_num, __func__);
      MutableSpan dst(dst_data, valid_faces_num);

      array_utils::gather(src, valid_faces, dst);

      layer.sharing_info->remove_user_and_delete_if_last();
      layer.data = dst_data;
      layer.sharing_info = implicit_sharing::info_for_mem_free(dst_data);
    }
  }

  for (CustomDataLayer &layer : MutableSpan(mesh.corner_data.layers, mesh.corner_data.totlayer)) {
    const eCustomDataType cd_type = eCustomDataType(layer.type);
    if (CD_TYPE_AS_MASK(cd_type) & CD_MASK_PROP_ALL) {
      const CPPType &type = *bke::custom_data_type_to_cpp_type(cd_type);
      const GSpan src(type, layer.data, mesh.corners_num);

      void *dst_data = MEM_malloc_arrayN(new_faces.total_size(), type.size, __func__);
      GMutableSpan dst(type, dst_data, new_faces.total_size());

      bke::attribute_math::gather_group_to_group(old_faces, new_faces, valid_faces, src, dst);

      layer.sharing_info->remove_user_and_delete_if_last();
      layer.data = dst_data;
      layer.sharing_info = implicit_sharing::info_for_mem_free(dst_data);
    }
    else if (ELEM(cd_type,
                  CD_NORMAL,
                  CD_ORIGINDEX,
                  CD_MDISPS,
                  CD_GRID_PAINT_MASK,
                  CD_ORIGSPACE_MLOOP))
    {
      const size_t elem_size = CustomData_sizeof(cd_type);
      const void *src = layer.data;

      void *dst = MEM_malloc_arrayN(new_faces.total_size(), elem_size, __func__);

      valid_faces.foreach_index(GrainSize(512), [&](const int64_t src_i, const int64_t dst_i) {
        CustomData_copy_elements(cd_type,
                                 POINTER_OFFSET(src, elem_size * old_faces[src_i].start()),
                                 POINTER_OFFSET(dst, elem_size * new_faces[dst_i].start()),
                                 new_faces[dst_i].size());
      });

      layer.sharing_info->remove_user_and_delete_if_last();
      layer.data = dst;
      layer.sharing_info = implicit_sharing::info_for_mem_free(dst);
    }
  }

  mesh.faces_num = new_faces.size();
  mesh.corners_num = new_faces.total_size();
  mesh.face_offset_indices = new_face_offsets.release().data;
  mesh.runtime->face_offsets_sharing_info->remove_user_and_delete_if_last();
  mesh.runtime->face_offsets_sharing_info = implicit_sharing::info_for_mem_free(
      mesh.face_offset_indices);

  mesh.tag_topology_changed();
}

static void remove_invalid_edges(Mesh &mesh, const IndexMask &valid_edges)
{
  const int valid_edges_num = valid_edges.size();
  for (CustomDataLayer &layer : MutableSpan(mesh.edge_data.layers, mesh.edge_data.totlayer)) {
    const eCustomDataType cd_type = eCustomDataType(layer.type);
    if (CD_TYPE_AS_MASK(cd_type) & CD_MASK_PROP_ALL) {
      const CPPType &type = *bke::custom_data_type_to_cpp_type(cd_type);
      const GSpan src(type, layer.data, mesh.edges_num);

      void *dst_data = MEM_malloc_arrayN(valid_edges_num, type.size, __func__);
      GMutableSpan dst(type, dst_data, valid_edges_num);

      array_utils::gather(src, valid_edges, dst);

      layer.sharing_info->remove_user_and_delete_if_last();
      layer.data = dst_data;
      layer.sharing_info = implicit_sharing::info_for_mem_free(dst_data);
    }
    else if (cd_type == CD_ORIGINDEX) {
      const Span src(static_cast<const int *>(layer.data), mesh.edges_num);

      int *dst_data = MEM_malloc_arrayN<int>(valid_edges_num, __func__);
      MutableSpan dst(dst_data, valid_edges_num);

      array_utils::gather(src, valid_edges, dst);

      layer.sharing_info->remove_user_and_delete_if_last();
      layer.data = dst_data;
      layer.sharing_info = implicit_sharing::info_for_mem_free(dst_data);
    }
  }

  mesh.edges_num = valid_edges.size();

  Array<int> all_edges_to_valid_edges(mesh.edges_num);
  index_mask::build_reverse_map(valid_edges, all_edges_to_valid_edges.as_mutable_span());
  MutableSpan<int> corner_edges = mesh.corner_edges_for_write();
  threading::parallel_for(corner_edges.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      corner_edges[i] = all_edges_to_valid_edges[corner_edges[i]];
    }
  });

  mesh.tag_topology_changed();
}

static bool validate_vertex_groups(const Mesh &mesh, const bool verbose, Mesh *mesh_mut)
{
  const Span<MDeformVert> dverts = mesh.deform_verts();
  if (dverts.is_empty()) {
    return true;
  }
  Mutex mutex;
  Vector<std::pair<int, Vector<std::string, 0>>> errors;
  Vector<std::pair<int, Vector<MDeformWeight, 0>>> replacements;
  threading::parallel_for(dverts.index_range(), 2048, [&](const IndexRange range) {
    for (const int vert : range) {
      const MDeformVert &dvert = dverts[vert];

      Vector<std::string> errors;
      bool invalid = false;
      Vector<MDeformWeight, 64> fixed_weights;
      for (const MDeformWeight &dw : Span(dvert.dw, dvert.totweight)) {
        const uint def_nr = dw.def_nr;
        if (dw.def_nr > INT_MAX) {
          invalid = true;
          if (verbose) {
            std::lock_guard lock(mutex);
            errors.append(fmt::format("Vertex {} has invalid deform group {}", vert, def_nr));
          }
          continue;
        }
        if (!std::isfinite(dw.weight)) {
          invalid = true;
          if (verbose) {
            std::lock_guard lock(mutex);
            errors.append(fmt::format(
                "Vertex {} deform group {} has invalid weight {}", vert, def_nr, dw.weight));
          }
          if (mesh_mut) {
            fixed_weights.append({def_nr, 0.0f});
          }
          continue;
        }
        if (dw.weight < 0.0f || dw.weight > 1.0f) {
          invalid = true;
          if (verbose) {
            std::lock_guard lock(mutex);
            errors.append(fmt::format(
                "Vertex {} deform group {} has invalid weight {}", vert, def_nr, dw.weight));
          }
          if (mesh_mut) {
            fixed_weights.append({def_nr, std::clamp(dw.weight, 0.0f, 1.0f)});
          }
          continue;
        }
        if (mesh_mut) {
          fixed_weights.append(dw);
        }
      }

      if (invalid) {
        std::lock_guard lock(mutex);
        replacements.append({vert, std::move(fixed_weights)});
      }
    }
  });
  if (replacements.is_empty()) {
    return true;
  }
  if (verbose) {
    for (const auto &[vert, errors] : errors) {
      for (const std::string &error : errors) {
        CLOG_ERROR(&LOG, "%s", error.c_str());
      }
    }
  }
  if (mesh_mut) {
    MutableSpan<MDeformVert> dverts = mesh_mut->deform_verts_for_write();
    for (auto &[vert, weights] : replacements) {
      MEM_freeN(dverts[vert].dw);
      dverts[vert].totweight = weights.size();
      dverts[vert].dw = weights.release().data;
    }
  }
  return false;
}

static bool validate_material_indices(const Mesh &mesh,
                                      const bool only_check_negative,
                                      const bool verbose,
                                      Mesh *mesh_mut)
{
  const IndexRange materials_range(only_check_negative ? std::numeric_limits<int>::max() :
                                                         std::max(int(mesh.totcol), 1));
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArray material_indices = *attributes.lookup<int>("material_index", bke::AttrDomain::Face);
  if (!material_indices) {
    return true;
  }
  if (const std::optional<int> index = material_indices.get_if_single()) {
    if (!materials_range.contains(*index)) {
      mesh_mut->attributes_for_write().remove("material_index");
      return false;
    }
    return true;
  }
  const VArraySpan<int> material_indices_span(material_indices);
  IndexMaskMemory memory;
  const IndexMask invalid_indices = IndexMask::from_predicate(
      material_indices.index_range(), GrainSize(4096), memory, [&](const int face) {
        return !materials_range.contains(material_indices_span[face]);
      });
  if (invalid_indices.is_empty()) {
    return true;
  }
  if (verbose) {
    invalid_indices.foreach_index([&](const int face) {
      CLOG_ERROR(&LOG, "Face %d has invalid material index %d", face, material_indices_span[face]);
    });
  }
  if (mesh_mut) {
    bke::MutableAttributeAccessor attributes = mesh_mut->attributes_for_write();
    bke::SpanAttributeWriter material_indices = attributes.lookup_for_write_span<int>(
        "material_index");
    index_mask::masked_fill(material_indices.span, 0, invalid_indices);
    material_indices.finish();
    DEG_id_tag_update(&mesh_mut->id, ID_RECALC_GEOMETRY_ALL_MODES);
  }
  return false;
}

static bool validate_selection_history(const Mesh &mesh, const bool verbose, Mesh *mesh_mut)
{
  if (!mesh.mselect) {
    return true;
  }
  const Span<MSelect> mselect(mesh.mselect, mesh.totselect);
  IndexMaskMemory memory;
  const IndexMask invalid = IndexMask::from_predicate(
      mselect.index_range(), GrainSize(4096), memory, [&](const int i) {
        if (mselect[i].index < 0) {
          return true;
        }
        const int domain_size = [&]() {
          switch (mselect[i].type) {
            case ME_VSEL:
              return mesh.verts_num;
            case ME_ESEL:
              return mesh.edges_num;
            case ME_FSEL:
              return mesh.faces_num;
          }
          return 0;
        }();
        if (mselect[i].index >= domain_size) {
          return true;
        }
        return false;
      });
  if (invalid.is_empty()) {
    return true;
  }

  if (verbose) {
    invalid.foreach_index([&](const int i) {
      CLOG_ERROR(&LOG,
                 "Selection element (%d, %d) has invalid index, must not be negative",
                 mselect[i].type,
                 mselect[i].index);
    });
  }
  if (mesh_mut) {
    MEM_SAFE_FREE(mesh_mut->mselect);
  }

  return false;
}

static IndexMask get_invalid_float_mask(const Span<float> values,
                                        const int floats_per_item,
                                        IndexMaskMemory &memory)
{
  if (floats_per_item == 0) {
    return {};
  }
  const int num_items = values.size() / floats_per_item;

  return IndexMask::from_predicate(
      IndexRange(num_items), GrainSize(4096), memory, [&](const int64_t index) {
        const Span<float> item_floats = values.slice(index * floats_per_item, floats_per_item);
        return std::any_of(item_floats.begin(), item_floats.end(), [](const float value) {
          return !std::isfinite(value);
        });
      });
}

static void validate_float_attribute(const bke::AttributeIter &iter,
                                     const int floats_per_item,
                                     const bool verbose,
                                     bool &all_attributes_valid,
                                     Mesh *mesh_mut)
{
  const GVArraySpan span = *iter.get();
  const Span<float> float_span(static_cast<const float *>(span.data()),
                               span.size_in_bytes() / sizeof(float));
  IndexMaskMemory memory;
  const IndexMask invalid = get_invalid_float_mask(float_span, floats_per_item, memory);
  if (invalid.is_empty()) {
    return;
  }
  if (verbose) {
    print_error_with_indices(invalid, "Attribute {} has invalid values", iter.name);
  }
  all_attributes_valid = false;
  if (mesh_mut) {
    bke::MutableAttributeAccessor attributes = mesh_mut->attributes_for_write();
    bke::GSpanAttributeWriter attr = attributes.lookup_for_write_span(iter.name);
    const CPPType &type = attr.span.type();
    type.fill_assign_indices(type.default_value(), attr.span.data(), invalid);
    attr.finish();
  }
}

static void validate_bool_attribute(const bke::AttributeIter &iter,
                                    const bool verbose,
                                    bool &all_attributes_valid,
                                    Mesh *mesh_mut)
{
  const VArraySpan<bool> span = *iter.get<bool>();
  const Span<int8_t> int_span = span.cast<int8_t>();
  IndexMaskMemory memory;
  const IndexMask invalid = IndexMask::from_predicate(
      int_span.index_range(), GrainSize(4096), memory, [&](const int i) {
        return !ELEM(int_span[i], 0, 1);
      });
  if (invalid.is_empty()) {
    return;
  }
  if (verbose) {
    print_error_with_indices(invalid, "Attribute {} has invalid values", iter.name);
  }
  all_attributes_valid = false;
  if (mesh_mut) {
    bke::MutableAttributeAccessor attributes = mesh_mut->attributes_for_write();
    bke::SpanAttributeWriter<bool> attr = attributes.lookup_for_write_span<bool>(iter.name);
    index_mask::masked_fill(attr.span, true, invalid);
    attr.finish();
  }
}

static bool validate_generic_attributes(const Mesh &mesh, const bool verbose, Mesh *mesh_mut)
{
  bool all_attributes_valid = true;
  mesh.attributes().foreach_attribute([&](const bke::AttributeIter &iter) {
    switch (iter.data_type) {
      case AttrType::Bool:
        validate_bool_attribute(iter, verbose, all_attributes_valid, mesh_mut);
        break;
      case AttrType::Int8:
        break;
      case AttrType::Int16_2D:
        break;
      case AttrType::Int32:
        break;
      case AttrType::Int32_2D:
        break;
      case AttrType::Float:
        validate_float_attribute(iter, 1, verbose, all_attributes_valid, mesh_mut);
        break;
      case AttrType::Float2:
        validate_float_attribute(iter, 2, verbose, all_attributes_valid, mesh_mut);
        break;
      case AttrType::Float3:
        validate_float_attribute(iter, 3, verbose, all_attributes_valid, mesh_mut);
        break;
      case AttrType::ColorFloat:
        validate_float_attribute(iter, 4, verbose, all_attributes_valid, mesh_mut);
        break;
      case AttrType::Quaternion:
        validate_float_attribute(iter, 4, verbose, all_attributes_valid, mesh_mut);
        break;
      case AttrType::Float4x4:
        validate_float_attribute(iter, 16, verbose, all_attributes_valid, mesh_mut);
        break;
      case AttrType::ColorByte:
        break;
      case AttrType::String:
        break;
    }
  });
  return all_attributes_valid;
}

static bool mesh_validate_impl(const Mesh &mesh, const bool verbose, Mesh *mesh_mut)
{
  IndexMaskMemory memory;

  IndexMask valid_edges(mesh.edges_num);

  const IndexMask edges_bad_verts = find_edges_bad_verts(mesh, memory, verbose);
  valid_edges = IndexMask::from_difference(valid_edges, edges_bad_verts, memory);

  EdgeMap unique_edges;
  const IndexMask edges_duplicate = find_edges_duplicates(
      mesh, valid_edges, memory, verbose, unique_edges);
  valid_edges = IndexMask::from_difference(valid_edges, edges_duplicate, memory);

  IndexMask valid_faces(mesh.faces_num);

  const IndexMask faces_bad_offsets = find_faces_bad_offsets(mesh, memory, verbose);
  valid_faces = IndexMask::from_difference(valid_faces, faces_bad_offsets, memory);

  const IndexMask faces_bad_verts = find_faces_bad_verts(mesh, valid_faces, memory, verbose);
  valid_faces = IndexMask::from_difference(valid_faces, faces_bad_verts, memory);

  const IndexMask faces_duplicate_verts = find_faces_duplicate_verts(
      mesh, valid_faces, memory, verbose);
  valid_faces = IndexMask::from_difference(valid_faces, faces_duplicate_verts, memory);

  const IndexMask faces_missing_edges = find_faces_missing_edges(
      mesh, valid_faces, unique_edges, memory, verbose);
  valid_faces = IndexMask::from_difference(valid_faces, faces_missing_edges, memory);

  const IndexMask duplicate_faces = find_duplicate_faces(mesh, valid_faces, memory, verbose);
  valid_faces = IndexMask::from_difference(valid_faces, duplicate_faces, memory);

  Vector<Vector<std::pair<int, int>>> corner_edge_fixes;
  find_faces_bad_edges(mesh, valid_faces, unique_edges, memory, verbose, corner_edge_fixes);

  bool valid = valid_edges.size() == mesh.edges_num && valid_faces.size() == mesh.faces_num &&
               corner_edge_fixes.is_empty();

  if (mesh_mut) {
    Mesh &mesh = *mesh_mut;
    if (!corner_edge_fixes.is_empty()) {
      MutableSpan<int> corner_edges = mesh.corner_edges_for_write();
      for (const Span<std::pair<int, int>> replacements : corner_edge_fixes) {
        for (const std::pair<int, int> &replacement : replacements) {
          corner_edges[replacement.first] = replacement.second;
        }
      }
    }

    if (valid_faces.size() < mesh.faces_num) {
      remove_invalid_faces(mesh, valid_faces);
    }

    if (valid_edges.size() < mesh.edges_num) {
      remove_invalid_edges(mesh, valid_edges);
    }
  }

  valid &= validate_vertex_groups(mesh, verbose, mesh_mut);
  valid &= validate_material_indices(mesh, true, verbose, mesh_mut);
  valid &= validate_selection_history(mesh, verbose, mesh_mut);
  valid &= validate_generic_attributes(mesh, verbose, mesh_mut);

  if (valid) {
    return true;
  }

  if (mesh_mut) {
    DEG_id_tag_update(&mesh_mut->id, ID_RECALC_GEOMETRY_ALL_MODES);
  }

  return false;
}

static bool mesh_validate(Mesh &mesh, const bool verbose)
{
  return mesh_validate_impl(mesh, verbose, &mesh);
}

static bool mesh_is_valid(const Mesh &mesh, const bool verbose)
{
  return mesh_validate_impl(mesh, verbose, nullptr);
}

}  // namespace blender::bke

bool BKE_mesh_validate(Mesh *mesh, const bool do_verbose, const bool /*cddata_check_mask*/)
{
  if (do_verbose) {
    CLOG_INFO(&LOG, "Validating Mesh: %s", mesh->id.name + 2);
  }
  return !blender::bke::mesh_validate(*mesh, do_verbose);
}

bool BKE_mesh_is_valid(Mesh *mesh)
{
  return blender::bke::mesh_is_valid(*mesh, true);
}

bool BKE_mesh_validate_material_indices(Mesh *mesh)
{
  return !blender::bke::validate_material_indices(*mesh, false, false, mesh);
}
