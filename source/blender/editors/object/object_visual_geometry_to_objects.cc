/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_mesh.h"
#include "BKE_mesh.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_pointcloud.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_collection_types.h"
#include "DNA_curves_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

#include "ED_object.hh"
#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"

#include "object_intern.hh"

namespace blender::ed::object {

/** Sets the transform of the object to a specific matrix. */
static void set_local_object_transform(Object &ob, const float4x4 &transform)
{
  float3 location;
  math::EulerXYZ rotation;
  float3 scale;
  math::to_loc_rot_scale_safe<true>(transform, location, rotation, scale);
  copy_v3_v3(ob.loc, location);
  copy_v3_v3(ob.rot, float3(rotation.x().radian(), rotation.y().radian(), rotation.z().radian()));
  copy_v3_v3(ob.scale, scale);
  ob.rotmode = ROT_MODE_EUL;
}

/** Objects for a #GeometrySet. */
struct ComponentObjects {
  Object *mesh_ob = nullptr;
  Object *curves_ob = nullptr;
  Object *pointcloud_ob = nullptr;
  Object *greasepencil_ob = nullptr;
  Vector<Object *> instance_objects;

  Vector<Object *> all_objects() const
  {
    Vector<Object *> objects;
    if (mesh_ob) {
      objects.append(mesh_ob);
    }
    if (curves_ob) {
      objects.append(curves_ob);
    }
    if (pointcloud_ob) {
      objects.append(pointcloud_ob);
    }
    if (greasepencil_ob) {
      objects.append(greasepencil_ob);
    }
    for (Object *instance_object : instance_objects) {
      objects.append(instance_object);
    }
    return objects;
  }
};

static void copy_materials_to_new_geometry_object(const Object &src_ob_eval,
                                                  const ID &src_data_eval,
                                                  Object &dst_ob_orig,
                                                  ID &dst_data_orig)
{
  const int materials_num = BKE_id_material_used_eval(src_data_eval);
  if (materials_num == 0) {
    return;
  }
  *BKE_id_material_len_p(&dst_data_orig) = materials_num;
  dst_ob_orig.totcol = materials_num;

  dst_ob_orig.matbits = MEM_calloc_arrayN<char>(materials_num, __func__);
  dst_ob_orig.mat = MEM_calloc_arrayN<Material *>(materials_num, __func__);
  Material ***dst_materials = BKE_id_material_array_p(&dst_data_orig);
  *dst_materials = MEM_calloc_arrayN<Material *>(materials_num, __func__);

  for (int i = 0; i < materials_num; i++) {
    const Material *material_eval = BKE_object_material_get_eval(
        src_ob_eval, src_data_eval, i + 1);
    Material *material_orig = const_cast<Material *>(DEG_get_original(material_eval));
    if (material_orig) {
      (*dst_materials)[i] = material_orig;
      id_us_plus(&material_orig->id);
    }
  }
}

struct CollectionWithTransform {
  /* A collection that should be instanced. */
  Collection *collection = nullptr;
  /* A transform that needs to be applied to instances of that collection. */
  float4x4 transform = float4x4::identity();
};

/** Utility class to build objects for a #GeometrySet recursively. */
class GeometryToObjectsBuilder {
 private:
  Main &bmain_;
  Map<const ID *, Object *> new_object_by_generated_geometry_;
  Map<bke::InstanceReference, CollectionWithTransform> collection_by_instance_;
  Vector<Collection *> new_instance_collections_;

 public:
  GeometryToObjectsBuilder(Main &bmain) : bmain_(bmain) {}

  Collection *build_collection_for_geometry(const Object &src_ob_eval,
                                            const bke::GeometrySet &geometry)
  {
    ComponentObjects component_objects = this->get_objects_for_geometry(src_ob_eval, geometry);
    const StringRefNull name = geometry.name.empty() ? StringRefNull(BKE_id_name(src_ob_eval.id)) :
                                                       StringRefNull(geometry.name);
    return this->collection_from_component_objects(component_objects, name);
  }

  ComponentObjects get_objects_for_geometry(const Object &src_ob_eval,
                                            const bke::GeometrySet &geometry)
  {
    const StringRefNull name = geometry.name.empty() ? StringRefNull(BKE_id_name(src_ob_eval.id)) :
                                                       StringRefNull(geometry.name);
    ComponentObjects objects;
    if (const Mesh *mesh = geometry.get_mesh()) {
      if (mesh->verts_num > 0) {
        objects.mesh_ob = this->ensure_object_for_mesh(src_ob_eval, *mesh, name);
      }
    }
    if (const Curves *curves = geometry.get_curves()) {
      if (curves->geometry.curve_num > 0) {
        objects.curves_ob = this->ensure_object_for_curves(src_ob_eval, *curves, name);
      }
    }
    if (const PointCloud *pointcloud = geometry.get_pointcloud()) {
      if (pointcloud->totpoint > 0) {
        objects.pointcloud_ob = this->ensure_object_for_pointcloud(src_ob_eval, *pointcloud, name);
      }
    }
    if (const GreasePencil *greasepencil = geometry.get_grease_pencil()) {
      if (greasepencil->layers().size() > 0) {
        objects.greasepencil_ob = this->ensure_object_for_grease_pencil(
            src_ob_eval, *greasepencil, name);
      }
    }
    if (const bke::Instances *instances = geometry.get_instances()) {
      objects.instance_objects = this->create_objects_for_instances(src_ob_eval, *instances);
    }
    return objects;
  }

  Span<Collection *> new_instance_collections() const
  {
    return new_instance_collections_;
  }

 private:
  Collection *collection_from_component_objects(const ComponentObjects &component_objects,
                                                const StringRefNull name)
  {
    Collection *collection = BKE_collection_add(&bmain_, nullptr, name.c_str());
    for (Object *object : component_objects.all_objects()) {
      BKE_collection_object_add(&bmain_, collection, object);
    }
    return collection;
  }

  Object *ensure_object_for_mesh(const Object &src_ob_eval,
                                 const Mesh &src_mesh,
                                 const StringRefNull name)
  {
    return new_object_by_generated_geometry_.lookup_or_add_cb(&src_mesh.id, [&]() {
      Mesh *new_mesh = BKE_id_new<Mesh>(&bmain_, name.c_str());
      Object *new_ob = BKE_object_add_only_object(&bmain_, OB_MESH, name.c_str());
      new_ob->data = new_mesh;

      BKE_mesh_nomain_to_mesh(BKE_mesh_copy_for_eval(src_mesh), new_mesh, new_ob);
      new_mesh->attributes_for_write().remove_anonymous();
      copy_materials_to_new_geometry_object(src_ob_eval, src_mesh.id, *new_ob, new_mesh->id);
      bke::mesh_remove_invalid_attribute_strings(*new_mesh);
      multires_customdata_delete(new_mesh);
      return new_ob;
    });
  }

  Object *ensure_object_for_curves(const Object &src_ob_eval,
                                   const Curves &src_curves,
                                   const StringRefNull name)
  {
    return new_object_by_generated_geometry_.lookup_or_add_cb(&src_curves.id, [&]() {
      Curves *new_curves = BKE_id_new<Curves>(&bmain_, name.c_str());
      Object *new_ob = BKE_object_add_only_object(&bmain_, OB_CURVES, name.c_str());
      new_ob->data = new_curves;

      new_curves->geometry.wrap() = src_curves.geometry.wrap();
      new_curves->geometry.wrap().attributes_for_write().remove_anonymous();
      copy_materials_to_new_geometry_object(src_ob_eval, src_curves.id, *new_ob, new_curves->id);
      return new_ob;
    });
  }

  Object *ensure_object_for_pointcloud(const Object &src_ob_eval,
                                       const PointCloud &src_pointcloud,
                                       const StringRefNull name)
  {
    return new_object_by_generated_geometry_.lookup_or_add_cb(&src_pointcloud.id, [&]() {
      PointCloud *new_pointcloud = BKE_id_new<PointCloud>(&bmain_, name.c_str());
      Object *new_ob = BKE_object_add_only_object(&bmain_, OB_POINTCLOUD, name.c_str());
      new_ob->data = new_pointcloud;

      BKE_pointcloud_nomain_to_pointcloud(BKE_pointcloud_copy_for_eval(&src_pointcloud),
                                          new_pointcloud);
      new_pointcloud->attributes_for_write().remove_anonymous();
      copy_materials_to_new_geometry_object(
          src_ob_eval, src_pointcloud.id, *new_ob, new_pointcloud->id);
      return new_ob;
    });
  }

  Object *ensure_object_for_grease_pencil(const Object &src_ob_eval,
                                          const GreasePencil &src_grease_pencil,
                                          const StringRefNull name)
  {
    return new_object_by_generated_geometry_.lookup_or_add_cb(&src_grease_pencil.id, [&]() {
      GreasePencil *new_grease_pencil = BKE_id_new<GreasePencil>(&bmain_, name.c_str());
      Object *new_ob = BKE_object_add_only_object(&bmain_, OB_GREASE_PENCIL, name.c_str());
      new_ob->data = new_grease_pencil;

      GreasePencil *greasepencil_to_move_from = BKE_grease_pencil_copy_for_eval(
          &src_grease_pencil);
      BKE_grease_pencil_nomain_to_grease_pencil(greasepencil_to_move_from, new_grease_pencil);
      new_grease_pencil->attributes_for_write().remove_anonymous();
      for (GreasePencilDrawingBase *base : new_grease_pencil->drawings()) {
        if (base->type != GP_DRAWING) {
          continue;
        }
        bke::greasepencil::Drawing &drawing =
            reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
        drawing.strokes_for_write().attributes_for_write().remove_anonymous();
      }
      copy_materials_to_new_geometry_object(
          src_ob_eval, src_grease_pencil.id, *new_ob, new_grease_pencil->id);
      return new_ob;
    });
  }

  Vector<Object *> create_objects_for_instances(const Object &src_ob_eval,
                                                const bke::Instances &src_instances)
  {
    if (std::optional<Vector<Object *>> simple_objects = this->create_objects_for_instances_simple(
            src_ob_eval, src_instances))
    {
      return *simple_objects;
    }

    bke::Instances instances = src_instances;
    instances.remove_unused_references();

    /* Each instance will be a collection instance, so we need to get the collection for each
     * #InstanceReference that is instanced. */
    Vector<CollectionWithTransform> data_by_handle;
    for (const bke::InstanceReference &reference : instances.references()) {
      data_by_handle.append(
          this->get_or_create_collection_for_instance_reference(src_ob_eval, reference));
    }

    const Span<int> handles = instances.reference_handles();
    const Span<float4x4> transforms = instances.transforms();

    Vector<Object *> objects;
    for (const int instance_i : IndexRange(instances.instances_num())) {
      const int handle = handles[instance_i];
      if (!data_by_handle.index_range().contains(handle)) {
        continue;
      }
      const CollectionWithTransform &instance = data_by_handle[handle];
      if (!instance.collection) {
        continue;
      }
      /* Create an empty object that then instances the collection. */
      Object *instance_object = BKE_object_add_only_object(
          &bmain_, OB_EMPTY, BKE_id_name(instance.collection->id));
      instance_object->transflag = OB_DUPLICOLLECTION;
      instance_object->instance_collection = instance.collection;
      id_us_plus(&instance.collection->id);

      const float4x4 &transform = transforms[instance_i];
      set_local_object_transform(*instance_object, transform * instance.transform);

      objects.append(instance_object);
    }
    return objects;
  }

  /**
   * Under some circumstances, additional nested collection instances can be avoided and objects
   * can be instanced directly. This is the case when the instances have the identity transform.
   * If nullopt is returned, a fallback method has to be used that creates additional collections.
   */
  std::optional<Vector<Object *>> create_objects_for_instances_simple(
      const Object &src_ob_eval, const bke::Instances &src_instances)
  {
    const Span<float4x4> transforms = src_instances.transforms();
    const Span<int> handles = src_instances.reference_handles();
    const Span<bke::InstanceReference> references = src_instances.references();

    Vector<Object *> objects;
    for (const int i : IndexRange(src_instances.instances_num())) {
      const float4x4 &transform = transforms[i];
      if (transform != float4x4::identity()) {
        return std::nullopt;
      }
      const int handle = handles[i];
      if (handle < 0 || handle >= references.size()) {
        return std::nullopt;
      }
      const bke::InstanceReference &reference = references[handle];
      switch (reference.type()) {
        case bke::InstanceReference::Type::None: {
          break;
        }
        case bke::InstanceReference::Type::Object: {
          Object &object_eval = reference.object();
          Object *object_orig = DEG_get_original(&object_eval);
          if (ELEM(object_orig, &src_ob_eval, nullptr)) {
            return std::nullopt;
          }
          objects.append(object_orig);
          break;
        }
        case bke::InstanceReference::Type::Collection: {
          return std::nullopt;
        }
        case bke::InstanceReference::Type::GeometrySet: {
          const ComponentObjects component_objects = this->get_objects_for_geometry(
              src_ob_eval, reference.geometry_set());
          objects.extend(component_objects.all_objects());
          break;
        }
      }
    }

    return objects;
  }

  CollectionWithTransform get_or_create_collection_for_instance_reference(
      const Object &src_ob_eval, const bke::InstanceReference &reference)
  {
    if (const CollectionWithTransform *instance = collection_by_instance_.lookup_ptr(reference)) {
      return *instance;
    }
    CollectionWithTransform instance;
    switch (reference.type()) {
      case bke::InstanceReference::Type::None: {
        break;
      }
      case bke::InstanceReference::Type::Object: {
        /* Create a collection for the object because we can't instance objects directly. */
        Object &object_eval = reference.object();
        Object *object_orig = DEG_get_original(&object_eval);

        if (object_orig->type == OB_EMPTY && object_orig->instance_collection) {
          instance.collection = object_orig->instance_collection;
        }
        else {
          instance.collection = BKE_collection_add(&bmain_, nullptr, BKE_id_name(object_orig->id));
          new_instance_collections_.append(instance.collection);
          BKE_collection_object_add(&bmain_, instance.collection, object_orig);

          /* Handle the object transform because it may not be the identity matrix. The location is
           * handled by setting the collection instance offset to it. The rotation and scale are
           * handled by offsetting the instance using the collection by the inverse amount. */
          float4x4 object_transform;
          BKE_object_to_mat4(object_orig, object_transform.ptr());
          instance.transform = float4x4(math::invert(float3x3(object_transform)));
          copy_v3_v3(instance.collection->instance_offset, object_transform.location());
        }
        break;
      }
      case bke::InstanceReference::Type::Collection: {
        /* For collections, we don't need to create a new wrapper collection, we can just create
         * objects that instance the existing collection. */
        Collection &collection_eval = reference.collection();
        Collection *collection_orig = DEG_get_original(&collection_eval);
        instance.collection = collection_orig;
        break;
      }
      case bke::InstanceReference::Type::GeometrySet: {
        instance.collection = this->build_collection_for_geometry(src_ob_eval,
                                                                  reference.geometry_set());
        new_instance_collections_.append(instance.collection);
        break;
      }
    }
    collection_by_instance_.add(reference, instance);
    return instance;
  }
};

static Vector<Collection *> find_collections_containing_object(Main &bmain,
                                                               Scene *scene,
                                                               Object &object)
{
  VectorSet<Collection *> collections;
  FOREACH_COLLECTION_BEGIN (&bmain, scene, Collection *, collection) {
    if (BKE_collection_has_object(collection, &object)) {
      collections.add(collection);
    }
  }
  FOREACH_COLLECTION_END;
  return collections.extract_vector();
}

static wmOperatorStatus visual_geometry_to_objects_exec(bContext *C, wmOperator * /*op*/)
{
  Main &bmain = *CTX_data_main(C);
  Scene &scene = *CTX_data_scene(C);
  Depsgraph &depsgraph = *CTX_data_ensure_evaluated_depsgraph(C);
  ViewLayer &active_view_layer = *CTX_data_view_layer(C);

  Vector<Object *> selected_objects_orig;
  CTX_DATA_BEGIN (C, Object *, src_ob_orig, selected_objects) {
    selected_objects_orig.append(src_ob_orig);
  }
  CTX_DATA_END;

  /* Create all required objects and collections and add them to bmain. However, so far nothing is
   * linked to the scene or view layer. That happens below. */
  GeometryToObjectsBuilder op(bmain);
  Vector<Object *> all_new_top_level_objects;
  for (Object *src_ob_orig : selected_objects_orig) {
    Object *src_ob_eval = DEG_get_evaluated(&depsgraph, src_ob_orig);
    bke::GeometrySet geometry_eval = bke::object_get_evaluated_geometry_set(*src_ob_eval);
    const ComponentObjects new_component_objects = op.get_objects_for_geometry(*src_ob_eval,
                                                                               geometry_eval);
    const Vector<Object *> top_level_objects = new_component_objects.all_objects();
    all_new_top_level_objects.extend(top_level_objects);
    /* Find the collections that the active object is in, because we want to add the new objects
     * in the same place. */
    const Vector<Collection *> collections_to_add_to = find_collections_containing_object(
        bmain, &scene, *src_ob_orig);

    float4x4 src_ob_local_transform;
    BKE_object_to_mat4(src_ob_eval, src_ob_local_transform.ptr());

    for (Object *object : top_level_objects) {
      /* Link the new objects into some collections. */
      for (Collection *collection_to_add_to : collections_to_add_to) {
        BKE_collection_object_add(&bmain, collection_to_add_to, object);
      }
      /* Transform and parent the objects so that they align with the source object. */
      float4x4 old_transform;
      BKE_object_to_mat4(object, old_transform.ptr());
      set_local_object_transform(*object, src_ob_local_transform * old_transform);
      object->parent = src_ob_orig->parent;
      copy_m4_m4(object->parentinv, src_ob_orig->parentinv);
    }
  }

  const Span<Collection *> new_instance_collections = op.new_instance_collections();
  for (Collection *new_collection : new_instance_collections) {
    /* Add the new collections to the scene collection. This makes them more visible to the user,
     * compared to having collection instances which use collections that are not in the scene. */
    BKE_collection_child_add(&bmain, scene.master_collection, new_collection);
  }
  /* Ensure that the #Base for objects and #LayerCollection for collections are created. */
  BKE_scene_view_layers_synced_ensure(&scene);

  /* Deselect everything so that we can select the new objects. */
  BKE_view_layer_base_deselect_all(&scene, &active_view_layer);
  /* Select the new objects. */
  for (Object *object : all_new_top_level_objects) {
    Base *base = BKE_view_layer_base_find(&active_view_layer, object);
    base->flag |= BASE_SELECTED;
  }
  /* Make one of the new objects active. */
  if (!all_new_top_level_objects.is_empty()) {
    Base *first_base = BKE_view_layer_base_find(&active_view_layer, all_new_top_level_objects[0]);
    BKE_view_layer_base_select_and_set_active(&active_view_layer, first_base);
    base_active_refresh(&bmain, &scene, &active_view_layer);
  }
  /* Exclude the new collections. This is done because they are only instanced by other objects but
   * should not be visible by themselves. */
  LISTBASE_FOREACH (ViewLayer *, view_layer, &scene.view_layers) {
    for (Collection *new_collection : new_instance_collections) {
      LayerCollection *new_layer_collection = BKE_layer_collection_first_from_scene_collection(
          view_layer, new_collection);
      BKE_layer_collection_set_flag(new_layer_collection, LAYER_COLLECTION_EXCLUDE, true);
    }
  }
  BKE_view_layer_need_resync_tag(&active_view_layer);
  DEG_id_tag_update(&scene.id, ID_RECALC_BASE_FLAGS);

  DEG_relations_tag_update(&bmain);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, &scene);
  WM_main_add_notifier(NC_SCENE | ND_LAYER, nullptr);
  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, nullptr);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);
  return OPERATOR_FINISHED;
}

void OBJECT_OT_visual_geometry_to_objects(wmOperatorType *ot)
{
  ot->name = "Visual Geometry to Objects";
  ot->description = "Convert geometry and instances into editable objects and collections";
  ot->idname = "OBJECT_OT_visual_geometry_to_objects";

  ot->exec = visual_geometry_to_objects_exec;
  ot->poll = ED_operator_object_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

}  // namespace blender::ed::object
