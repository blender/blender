/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "scene_index.hh"

#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/xformSchema.h>

#include "BLI_utildefines.hh"

#include "BKE_layer.hh"
#include "BKE_object.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_ID.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "curves.hh"
#include "instancer.hh"
#include "light.hh"
#include "mesh.hh"
#include "populate_context.hh"
#include "util.hh"
#include "volume.hh"
#include "volume_modifier.hh"
#include "world.hh"

namespace blender::io::hydra {

CLG_LOGREF_DECLARE_GLOBAL(LOG_HYDRA_SCENE_INDEX, "hydra.scene_index");

bool HydraSceneIndex::object_is_supported(const Object *object) const
{
  switch (object->type) {
    case OB_MESH:
    case OB_SURF:
    case OB_FONT:
    case OB_CURVES_LEGACY:
    case OB_MBALL:
    case OB_LAMP:
    case OB_CURVES:
    case OB_VOLUME:
      return true;
    default:
      return false;
  }
}

static bool object_render_as_geometry(const Object *object)
{
  return ELEM(object->type, OB_MESH, OB_SURF, OB_FONT, OB_CURVES_LEGACY, OB_MBALL, OB_CURVES);
}

bool HydraSceneIndex::object_is_visible(Depsgraph *depsgraph,
                                        const View3D *view3d,
                                        const Object *object,
                                        const int mode) const
{
  const eEvaluationMode deg_mode = DEG_get_mode(depsgraph);
  const bool is_visible = BKE_object_visibility(object, deg_mode) & mode;
  return (deg_mode == DAG_EVAL_VIEWPORT) ?
             is_visible && BKE_object_is_visible_in_viewport(view3d, object) :
             is_visible;
}

HydraSceneIndex::HydraSceneIndex(const pxr::SdfPath &root_path,
                                 pxr::HdRenderDelegate *render_delegate,
                                 const bool use_materialx)
    : retained_(pxr::HdRetainedSceneIndex::New()),
      root_path_(root_path),
      render_delegate_(render_delegate),
      use_materialx_(use_materialx)
{
  if (render_delegate_) {
    material_render_contexts_ = render_delegate_->GetMaterialRenderContexts();
    shader_source_types_ = render_delegate_->GetShaderSourceTypes();
  }
}

HydraSceneIndex::~HydraSceneIndex()
{
  clear();
}

void HydraSceneIndex::populate(Depsgraph *depsgraph, View3D *view3d)
{
  Main *bmain = DEG_get_bmain(depsgraph);
  Scene *scene = DEG_get_evaluated_scene(depsgraph);

  /* Gather recalc flags. Note that we can not check recalc flags on pointers
   * in emitted object and material maps, as those datablocks might have been
   * deleted in the meantime. */
  Map<const ID *, int> id_recalc;
  {
    DEGIDIterData iter_data = {nullptr};
    iter_data.graph = depsgraph;
    iter_data.only_updated = true;
    ITER_BEGIN (
        DEG_iterator_ids_begin, DEG_iterator_ids_next, DEG_iterator_ids_end, &iter_data, ID *, id)
    {
      id_recalc.add(id, id->recalc);
    }
    ITER_END;
  }
  auto has_recalc = [&](const ID *id, const int flag) {
    const int *recalc = id_recalc.lookup_ptr(id);
    return recalc && (*recalc & flag) != 0;
  };

  /* Drop materials marked for update, we'll recreate them. */
  emitted_materials_.remove_if([&](const auto &item) {
    return has_recalc(&item.key->id, ID_RECALC_ALL) ||
           has_recalc(&item.key->nodetree->id, ID_RECALC_ALL);
  });

  /* Find geometry marked for update. */
  Set<const ID *> dirty_instance_geometries;
  for (const auto &item : instance_geometries_by_object_.items()) {
    if (has_recalc(&item.key->id, ID_RECALC_GEOMETRY)) {
      for (const ID *geom : item.value) {
        dirty_instance_geometries.add(geom);
      }
    }
  }

  /* Drop emitted geometry that was updated. */
  emitted_geometry_.remove_if([&](const auto &item) {
    return has_recalc(item.key.id, ID_RECALC_GEOMETRY) ||
           dirty_instance_geometries.contains(item.key.id);
  });

  PopulateContext ctx(depsgraph,
                      bmain,
                      scene,
                      view3d,
                      root_path_,
                      use_materialx_,
                      Span<pxr::TfToken>(material_render_contexts_),
                      Span<pxr::TfToken>(shader_source_types_),
                      emitted_materials_,
                      emitted_geometry_,
                      instance_geometries_by_object_,
                      emitted_paths_);

  DEGObjectIterSettings settings = {nullptr};
  settings.depsgraph = depsgraph;
  settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
  DEGObjectIterData data = {nullptr};
  data.settings = &settings;
  data.graph = settings.depsgraph;
  data.flag = settings.flags;

  ITER_BEGIN (DEG_iterator_objects_begin,
              DEG_iterator_objects_next,
              DEG_iterator_objects_end,
              &data,
              Object *,
              object)
  {
    /* TODO: backface culling is on material in Blender but on geometry in USD.
     * Currently we are not correctly updating geometry for changes to that. */
    const bool updated_geometry = (object->id.recalc & ID_RECALC_GEOMETRY) != 0 ||
                                  (object->data && object->data->recalc != 0);
    const bool updated_transform = (object->id.recalc & ID_RECALC_TRANSFORM) != 0;

    /* Remove outdated geometry emitted by this object. */
    if (updated_geometry && !data.dupli_object_current) {
      if (const EmittedObject *prev = emitted_objects_.lookup_ptr(object)) {
        for (const EmittedGeometryKey &k : prev->geometry_keys) {
          emitted_geometry_.remove(k);
        }
      }
    }

    if (!data.dupli_object_current && !updated_geometry) {
      if (EmittedObject *cached = emitted_objects_.lookup_ptr(object)) {
        if (!updated_transform) {
          /* Reuse the previous emission as-is. */
          for (const EmittedObjectPrim &item : cached->prims) {
            ctx.new_paths.add(item.path);
          }
          for (const Material *mat : cached->materials) {
            ctx.get_or_create_material(mat);
          }
          for (const EmittedGeometryKey &k : cached->geometry_keys) {
            ctx.used_emitted_geometry.add(k);
          }
          ctx.new_emitted_objects.add(object, *cached);
          continue;
        }
        /* Fast transform overlay and re-compose for transform only update. */
        const pxr::GfMatrix4d obj_xform = gf_matrix_from_transform(
            object->object_to_world().ptr());
        for (EmittedObjectPrim &item : cached->prims) {
          if (!item.base_data_source) {
            continue;
          }
          const pxr::GfMatrix4d new_xform = item.geometry_xform * obj_xform;
          item.xform_overlay = pxr::HdRetainedContainerDataSource::New(
              pxr::HdXformSchema::GetSchemaToken(),
              pxr::HdXformSchema::Builder()
                  .SetMatrix(
                      pxr::HdRetainedTypedSampledDataSource<pxr::GfMatrix4d>::New(new_xform))
                  .SetResetXformStack(pxr::HdRetainedTypedSampledDataSource<bool>::New(false))
                  .Build());
          ctx.new_paths.add(item.path);
          ctx.to_add.push_back({item.path, item.type, item.compose()});
          ctx.to_dirty.push_back({item.path, pxr::HdXformSchema::GetDefaultLocator()});
        }
        for (const Material *mat : cached->materials) {
          ctx.get_or_create_material(mat);
        }
        for (const EmittedGeometryKey &k : cached->geometry_keys) {
          ctx.used_emitted_geometry.add(k);
        }
        ctx.new_emitted_objects.add(object, *cached);
        continue;
      }
    }

    if (data.dupli_object_current) {
      /* Note `object` can be temporary on the stack, here we get the stable pointer. */
      Object *source = data.dupli_object_current->ob;
      if (!DEG_iterator_dupli_is_visible(data.dupli_object_current, DEG_get_mode(depsgraph))) {
        continue;
      }

      /* Lights and volumes aren't gprims and don't support instancer prototypes.
       * So we emit a new prim per dupli. */
      if (source->type == OB_LAMP) {
        emit_light_dupli(ctx, source, data.dupli_object_current->mat);
        continue;
      }
      if (source->type == OB_VOLUME) {
        emit_volume_dupli(ctx, source, data.dupli_object_current->mat);
        continue;
      }

      if (!object_render_as_geometry(source)) {
        continue;
      }

      /* Unique object info that works for geometry sets too. */
      const BObjectInfo info{object, source, object->data};

      /* Emit prototype once. */
      if (!ctx.proto_range_by_source.contains(source)) {
        const int proto_start = ctx.all_proto_paths.size();
        if (source->type == OB_CURVES) {
          emit_curves_proto(ctx, info);
        }
        else {
          emit_mesh_proto(ctx, info);
          emit_hair_proto(ctx, source);
        }
        ctx.proto_range_by_source.add(
            source, IndexRange(proto_start, ctx.all_proto_paths.size() - proto_start));
      }

      ctx.add_instance(source, data.dupli_object_current->mat);
      continue;
    }

    if (!object_is_supported(object)) {
      continue;
    }

    EmittedObject emitted;

    /* Emit hair particles separately. */
    emit_hair_for_object(ctx, object, emitted);

    if (!object_is_visible(depsgraph, view3d, object, OB_VISIBLE_SELF)) {
      if (!emitted.prims.is_empty()) {
        ctx.new_emitted_objects.add(object, std::move(emitted));
      }
      continue;
    }

    const BObjectInfo info{object, object, object->data};
    if (object->type == OB_LAMP) {
      emit_light_object(ctx, object, emitted);
    }
    else if (emit_volume_object(ctx, object, emitted)) {
      /* Emitted as volume prim instead. */
    }
    else if (object->type == OB_CURVES) {
      emit_curves_object(ctx, info, emitted);
    }
    else {
      emit_mesh_object(ctx, info, emitted);
    }

    if (!emitted.prims.is_empty()) {
      ctx.new_emitted_objects.add(object, std::move(emitted));
    }
  }
  ITER_END;

  /* Emit instancer prims. */
  if (!ctx.all_proto_paths.is_empty() && !ctx.instance_transforms.empty()) {
    const pxr::SdfPath instancer_path = ctx.instancer_prim_id();
    pxr::HdContainerDataSourceHandle prim_ds = build_instancer_prim_data_source(
        ctx.all_proto_paths.as_span(), ctx.per_proto_indices.as_span(), ctx.instance_transforms);
    ctx.emit_prim(instancer_path, pxr::HdPrimTypeTokens->instancer, prim_ds);
  }

  /* Drop unused materials, geometry, instances. */
  emitted_materials_.remove_if(
      [&](const auto &item) { return !ctx.used_emitted_materials.contains(item.key); });
  emitted_geometry_.remove_if(
      [&](const auto &item) { return !ctx.used_emitted_geometry.contains(item.key); });
  instance_geometries_by_object_.remove_if(
      [&](const auto &item) { return !ctx.used_instance_sources.contains(item.key); });

  /* Keep used and newly added material prims. */
  for (const auto &item : emitted_materials_.items()) {
    if (item.value.data_source) {
      ctx.new_paths.add(item.value.path);
    }
  }
  for (const Material *material : ctx.new_emitted_materials) {
    const EmittedMaterial *entry = emitted_materials_.lookup_ptr(material);
    if (entry && entry->data_source) {
      ctx.emit_prim(entry->path, pxr::HdPrimTypeTokens->material, entry->data_source);
    }
  }

  /* Emit world. */
  {
    const pxr::SdfPath world_path = root_path_.AppendElementString("World");
    const bool world_shading_changed = scene->world &&
                                       (has_recalc(&scene->world->id, ID_RECALC_ALL) ||
                                        has_recalc(&scene->world->nodetree->id, ID_RECALC_ALL));
    emitted_world_.emit(ctx, bmain, scene, view3d, world_path, world_shading_changed);
  }

  /* Add, remove and dirty prims as needed. */
  pxr::HdSceneIndexObserver::RemovedPrimEntries to_remove;
  for (const pxr::SdfPath &path : emitted_paths_) {
    if (!ctx.new_paths.contains(path)) {
      to_remove.push_back({path});
    }
  }

  if (!to_remove.empty()) {
    retained_->RemovePrims(to_remove);
  }
  if (!ctx.to_add.empty()) {
    retained_->AddPrims(ctx.to_add);
  }
  if (!ctx.to_dirty.empty()) {
    retained_->DirtyPrims(ctx.to_dirty);
  }

  emitted_paths_ = std::move(ctx.new_paths);
  emitted_objects_ = std::move(ctx.new_emitted_objects);
}

void HydraSceneIndex::clear()
{
  if (!emitted_paths_.is_empty()) {
    pxr::HdSceneIndexObserver::RemovedPrimEntries to_remove;
    for (const pxr::SdfPath &path : emitted_paths_) {
      to_remove.push_back({path});
    }
    retained_->RemovePrims(to_remove);
    emitted_paths_.clear();
  }
  emitted_objects_.clear();
  emitted_materials_.clear();
  emitted_geometry_.clear();
  instance_geometries_by_object_.clear();
  emitted_world_.clear();
}

}  // namespace blender::io::hydra
