/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"

#include "GEO_foreach_geometry.hh"
#include "GEO_join_geometries.hh"

namespace blender::geometry {

static void extract_real_geometries_recursive(
    bke::GeometrySet &geometry,
    Vector<int> &path,
    Map<bke::GeometrySet, Vector<Vector<int>>> &r_real_geometries)
{
  bke::GeometrySet real_geometry = geometry;
  real_geometry.remove(bke::GeometryComponent::Type::Instance);
  geometry.keep_only({bke::GeometryComponent::Type::Instance});

  r_real_geometries.lookup_or_add_default(std::move(real_geometry)).append(path);

  bke::Instances *instances = geometry.get_instances_for_write();
  if (!instances) {
    return;
  }
  instances->ensure_geometry_instances();
  MutableSpan<bke::InstanceReference> references = instances->references_for_write();
  for (const int i : references.index_range()) {
    bke::InstanceReference &reference = references[i];
    if (reference.type() == bke::InstanceReference::Type::GeometrySet) {
      bke::GeometrySet &sub_geometry = reference.geometry_set();
      path.append(i);
      extract_real_geometries_recursive(sub_geometry, path, r_real_geometries);
      path.pop_last();
    }
  }
}

static void reinsert_modified_geometry_recursive(bke::GeometrySet &geometry,
                                                 const bke::GeometrySet &geometry_to_insert,
                                                 const Span<int> path)
{
  if (path.is_empty()) {
    /* Instance references must not be merged here as that could invalidate the paths. */
    const bool allow_merging_instance_references = false;
    /* Important to pass the old geometry first, so that the instance reference paths stay
     * valid. */
    geometry = join_geometries(
        {geometry, geometry_to_insert}, {}, {}, allow_merging_instance_references);
    return;
  }
  bke::Instances *instances = geometry.get_instances_for_write();
  BLI_assert(instances);
  const int reference_i = path.first();
  const MutableSpan<bke::InstanceReference> references = instances->references_for_write();
  BLI_assert(reference_i < references.size());
  bke::InstanceReference &reference = references[reference_i];
  BLI_assert(reference.type() == bke::InstanceReference::Type::GeometrySet);
  bke::GeometrySet &sub_geometry = reference.geometry_set();
  reinsert_modified_geometry_recursive(sub_geometry, geometry_to_insert, path.drop_front(1));
}

struct GeometryWithPaths {
  bke::GeometrySet geometry;
  Vector<Vector<int>> paths;
};

void foreach_real_geometry(bke::GeometrySet &geometry,
                           FunctionRef<void(bke::GeometrySet &geometry_set)> fn)
{
  /* Afterwards the geometry does not have realized geometry anymore. It has been extracted and
   * will be reinserted afterwards. */
  Map<bke::GeometrySet, Vector<Vector<int>>> real_geometries;
  {
    Vector<int> path;
    extract_real_geometries_recursive(geometry, path, real_geometries);
  }
  /* Take the geometries out of the map so that they can be edited in-place. As keys in the #Map
   * the geometries are const and thus can't be modified. */
  Vector<GeometryWithPaths> geometries_with_paths;
  for (auto &&item : real_geometries.items()) {
    geometries_with_paths.append({item.key, std::move(item.value)});
  }
  /* Clear to avoid extra references to the geometries which prohibit editing them in-place. */
  real_geometries.clear();

  /* Actually modify the geometries in parallel. */
  threading::parallel_for(geometries_with_paths.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      bke::GeometrySet &geometry_to_modify = geometries_with_paths[i].geometry;
      fn(geometry_to_modify);
    }
  });

  /* Reinsert modified geometries. */
  for (GeometryWithPaths &geometry_with_paths : geometries_with_paths) {
    for (const Span<int> path : geometry_with_paths.paths) {
      reinsert_modified_geometry_recursive(geometry, geometry_with_paths.geometry, path);
    }
  }
}

}  // namespace blender::geometry
