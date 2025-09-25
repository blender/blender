/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include <string>

#include "BLI_bounds.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_vector.h"
#include "BLI_set.hh"
#include "BLI_sort.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "BKE_context.hh"
#include "BKE_curve_legacy_convert.hh"
#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_object.hh"

#include "DEG_depsgraph_build.hh"

#include "DNA_collection_types.h"

#include "obj_export_mtl.hh"
#include "obj_import_file_reader.hh"
#include "obj_import_mesh.hh"
#include "obj_import_nurbs.hh"
#include "obj_import_objects.hh"
#include "obj_importer.hh"

namespace blender::io::obj {

static Collection *find_or_create_collection(Main *bmain,
                                             Collection *target,
                                             const std::string &geom_name,
                                             const OBJImportParams &import_params)
{
  if (target == nullptr || import_params.collection_separator == 0) {
    return target;
  }
  size_t subname_start = 0;
  size_t sep_pos = geom_name.find(import_params.collection_separator);
  if (sep_pos == std::string::npos) {
    return target;
  }
  while (sep_pos != std::string::npos) {
    /* Get current sub-name, find or create collection with that name. */
    if (sep_pos > subname_start) {
      std::string subname = geom_name.substr(subname_start, sep_pos - subname_start);
      bool found = false;
      LISTBASE_FOREACH (CollectionChild *, child, &target->children) {
        if (GS(child->collection->id.name) == ID_GR &&
            STREQ(child->collection->id.name + 2, subname.c_str()))
        {
          target = child->collection;
          found = true;
          break;
        }
      }
      if (!found) {
        target = BKE_collection_add(bmain, target, subname.c_str());
      }
    }

    /* Proceed to next sub-name component. */
    subname_start = sep_pos + 1;
    if (subname_start >= geom_name.size()) {
      break;
    }
    sep_pos = geom_name.find(import_params.collection_separator, subname_start);
  }
  return target;
}

static void geometry_to_blender_geometry_set(const OBJImportParams &import_params,
                                             const Span<std::unique_ptr<Geometry>> all_geometries,
                                             const GlobalVertices &global_vertices,
                                             Vector<bke::GeometrySet> &geometries)
{
  for (const std::unique_ptr<Geometry> &geometry : all_geometries) {
    bke::GeometrySet geometry_set;

    if (geometry->geom_type_ == GEOM_MESH) {
      MeshFromGeometry mesh_ob_from_geometry{*geometry, global_vertices};
      Mesh *mesh = mesh_ob_from_geometry.create_mesh(import_params);
      geometry_set = bke::GeometrySet::from_mesh(mesh);
    }
    else if (geometry->geom_type_ == GEOM_CURVE) {
      CurveFromGeometry curve_ob_from_geometry(*geometry, global_vertices);
      Curves *curves_id = curve_ob_from_geometry.create_curve(import_params);
      geometry_set = bke::GeometrySet::from_curves(curves_id);
    }

    geometry_set.name = geometry->geometry_name_;
    geometries.append(std::move(geometry_set));
  }
}

/**
 * Make Blender Mesh, Curve etc from Geometry and add them to the import collection.
 */
static void geometry_to_blender_objects(Main *bmain,
                                        Scene *scene,
                                        ViewLayer *view_layer,
                                        const OBJImportParams &import_params,
                                        MutableSpan<std::unique_ptr<Geometry>> all_geometries,
                                        const GlobalVertices &global_vertices,
                                        Map<std::string, std::unique_ptr<MTLMaterial>> &materials,
                                        Map<std::string, Material *> &created_materials)
{
  LayerCollection *lc = BKE_layer_collection_get_active(view_layer);

  /* Sort objects by name: creating many objects is much faster if the creation
   * order is sorted by name. */
  blender::parallel_sort(
      all_geometries.begin(), all_geometries.end(), [](const auto &a, const auto &b) {
        const char *na = a ? a->geometry_name_.c_str() : "";
        const char *nb = b ? b->geometry_name_.c_str() : "";
        return BLI_strcasecmp(na, nb) < 0;
      });

  /* Create all the objects. */
  Vector<Object *> objects;
  objects.reserve(all_geometries.size());
  Set<Collection *> collections;
  for (const std::unique_ptr<Geometry> &geometry : all_geometries) {
    Object *obj = nullptr;
    if (geometry->geom_type_ == GEOM_MESH) {
      MeshFromGeometry mesh_ob_from_geometry{*geometry, global_vertices};
      obj = mesh_ob_from_geometry.create_mesh_object(
          bmain, materials, created_materials, import_params);
    }
    else if (geometry->geom_type_ == GEOM_CURVE) {
      CurveFromGeometry curve_ob_from_geometry(*geometry, global_vertices);
      obj = curve_ob_from_geometry.create_curve_object(bmain, import_params);
    }
    if (obj != nullptr) {
      Collection *target_collection = find_or_create_collection(
          bmain, lc->collection, geometry->geometry_name_, import_params);
      collections.add(target_collection);

      BKE_collection_object_add(bmain, target_collection, obj);
      objects.append(obj);
    }
  }

  /* Clamp object size if needed. */
  if (import_params.clamp_size > 0.0f) {
    std::optional<Bounds<float3>> bounds = std::nullopt;
    for (Object *obj : objects) {
      bounds = blender::bounds::merge(bounds, BKE_object_boundbox_get(obj));
    }
    if (bounds.has_value()) {
      const float max_diff = math::reduce_max(bounds->max - bounds->min);
      if (import_params.clamp_size < max_diff * import_params.global_scale) {
        const float scale = import_params.clamp_size / max_diff;
        for (Object *obj : objects) {
          copy_v3_fl(obj->scale, scale);
        }
      }
    }
  }

  /* Do object selections in a separate loop (allows just one view layer sync). */
  BKE_view_layer_synced_ensure(scene, view_layer);
  for (Object *obj : objects) {
    Base *base = BKE_view_layer_base_find(view_layer, obj);
    BKE_view_layer_base_select_and_set_active(view_layer, base);

    int flags = ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION |
                ID_RECALC_BASE_FLAGS;
    DEG_id_tag_update_ex(bmain, &obj->id, flags);
  }
  for (Collection *col : collections) {
    DEG_id_tag_update(&col->id, ID_RECALC_SYNC_TO_EVAL);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  DEG_relations_tag_update(bmain);
}

void importer_geometry(const OBJImportParams &import_params,
                       Vector<bke::GeometrySet> &geometries,
                       size_t read_buffer_size)
{
  /* List of geometries to be parsed from OBJ file. */
  Vector<std::unique_ptr<Geometry>> all_geometries;
  /* Container for vertex and UV vertex coordinates. */
  GlobalVertices global_vertices;

  OBJParser obj_parser{import_params, read_buffer_size};
  obj_parser.parse(all_geometries, global_vertices);

  geometry_to_blender_geometry_set(import_params, all_geometries, global_vertices, geometries);
}

void importer_main(bContext *C, const OBJImportParams &import_params)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  importer_main(bmain, scene, view_layer, import_params);
}

void importer_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   const OBJImportParams &import_params,
                   size_t read_buffer_size)
{
  /* List of geometries to be parsed from OBJ file. */
  Vector<std::unique_ptr<Geometry>> all_geometries;
  /* Container for vertex and UV vertex coordinates. */
  GlobalVertices global_vertices;
  /* List of MTLMaterial instances to be parsed from MTL file. */
  Map<std::string, std::unique_ptr<MTLMaterial>> materials;
  Map<std::string, Material *> created_materials;

  OBJParser obj_parser{import_params, read_buffer_size};
  obj_parser.parse(all_geometries, global_vertices);

  /* Parse all referenced MTL files */
  for (StringRefNull mtl_library : obj_parser.mtl_libraries()) {
    MTLParser mtl_parser{mtl_library, import_params.filepath};
    mtl_parser.parse_and_store(materials);
  }

  if (import_params.clear_selection) {
    BKE_view_layer_base_deselect_all(scene, view_layer);
  }

  /* Create Blender objects from the parsed geometries */
  geometry_to_blender_objects(bmain,
                              scene,
                              view_layer,
                              import_params,
                              all_geometries,
                              global_vertices,
                              materials,
                              created_materials);
}
}  // namespace blender::io::obj
