/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "IO_abstract_hierarchy_iterator.h"
#include "dupli_parent_finder.hh"

#include <string>

#include <fmt/core.h>

#include "BKE_anim_data.hh"
#include "BKE_duplilist.hh"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_key.hh"
#include "BKE_object.hh"
#include "BKE_particle.h"

#include "BLI_assert.h"
#include "BLI_math_matrix.h"
#include "BLI_set.hh"
#include "BLI_string_utils.hh"

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_rigidbody_types.h"

#include "DEG_depsgraph_query.hh"

namespace blender::io {

const HierarchyContext *HierarchyContext::root()
{
  return nullptr;
}

bool HierarchyContext::is_instance() const
{
  return !original_export_path.empty();
}
void HierarchyContext::mark_as_instance_of(const std::string &reference_export_path)
{
  original_export_path = reference_export_path;
}
void HierarchyContext::mark_as_not_instanced()
{
  original_export_path.clear();
}

bool HierarchyContext::is_prototype() const
{
  /* The context is for a prototype if it's for a duplisource or
   * for a duplicated object that was designated to be a prototype
   * because the original was not included in the export. */
  return is_duplisource || (duplicator != nullptr && !is_instance());
}

bool HierarchyContext::is_object_visible(const enum eEvaluationMode evaluation_mode) const
{
  const bool is_dupli = duplicator != nullptr;
  int base_flag;

  if (is_dupli) {
    /* Construct the object's base flags from its dupli-parent, just like is done in
     * deg_objects_dupli_iterator_next(). Without this, the visibility check below will fail. Doing
     * this here, instead of a more suitable location in AbstractHierarchyIterator, prevents
     * copying the Object for every dupli. */
    base_flag = object->base_flag;
    object->base_flag = duplicator->base_flag | BASE_FROM_DUPLI;
  }

  const int visibility = BKE_object_visibility(object, evaluation_mode);

  if (is_dupli) {
    object->base_flag = base_flag;
  }

  return (visibility & OB_VISIBLE_SELF) != 0;
}

EnsuredWriter::EnsuredWriter() : writer_(nullptr), newly_created_(false) {}

EnsuredWriter::EnsuredWriter(AbstractHierarchyWriter *writer, bool newly_created)
    : writer_(writer), newly_created_(newly_created)
{
}

EnsuredWriter EnsuredWriter::empty()
{
  return EnsuredWriter(nullptr, false);
}
EnsuredWriter EnsuredWriter::existing(AbstractHierarchyWriter *writer)
{
  return EnsuredWriter(writer, false);
}
EnsuredWriter EnsuredWriter::newly_created(AbstractHierarchyWriter *writer)
{
  return EnsuredWriter(writer, true);
}

bool EnsuredWriter::is_newly_created() const
{
  return newly_created_;
}

EnsuredWriter::operator bool() const
{
  return writer_ != nullptr;
}

AbstractHierarchyWriter *EnsuredWriter::operator->()
{
  return writer_;
}

bool AbstractHierarchyWriter::check_is_animated(const HierarchyContext &context) const
{
  Object *object = context.object;

  if (BKE_animdata_id_is_animated(static_cast<ID *>(object->data))) {
    return true;
  }
  if (BKE_key_from_object(object) != nullptr) {
    return true;
  }
  if (check_has_deforming_physics(context)) {
    return true;
  }

  /* Test modifiers. */
  /* TODO(Sybren): replace this with a check on the depsgraph to properly check for dependency on
   * time. */
  ModifierData *md = static_cast<ModifierData *>(object->modifiers.first);
  while (md) {
    if (md->type != eModifierType_Subsurf) {
      return true;
    }
    md = md->next;
  }

  return false;
}

bool AbstractHierarchyWriter::check_has_physics(const HierarchyContext &context)
{
  const RigidBodyOb *rbo = context.object->rigidbody_object;
  return rbo != nullptr && rbo->type == RBO_TYPE_ACTIVE;
}

bool AbstractHierarchyWriter::check_has_deforming_physics(const HierarchyContext &context)
{
  const RigidBodyOb *rbo = context.object->rigidbody_object;
  return rbo != nullptr && rbo->type == RBO_TYPE_ACTIVE && (rbo->flag & RBO_FLAG_USE_DEFORM) != 0;
}

bool HierarchyContext::is_point_instancer() const
{
  if (!object) {
    return false;
  }

  /* Collection instancers are handled elsewhere as part of Scene instancing. */
  if (object->type == OB_EMPTY && object->instance_collection != nullptr) {
    return false;
  }

  const bke::GeometrySet geometry_set = bke::object_get_evaluated_geometry_set(*object);
  return geometry_set.has_instances();
}

AbstractHierarchyIterator::AbstractHierarchyIterator(Main *bmain, Depsgraph *depsgraph)
    : bmain_(bmain), depsgraph_(depsgraph), export_subset_({true, true})
{
}

AbstractHierarchyIterator::~AbstractHierarchyIterator()
{
  /* release_writers() cannot be called here directly, as it calls into the pure-virtual
   * release_writer() function. By the time this destructor is called, the subclass that implements
   * that pure-virtual function is already destructed. */
  BLI_assert_msg(
      writers_.is_empty(),
      "release_writers() should be called before the AbstractHierarchyIterator goes out of scope");
}

void AbstractHierarchyIterator::iterate_and_write()
{
  export_graph_construct();
  connect_loose_objects();
  export_graph_prune();
  determine_export_paths(HierarchyContext::root());
  determine_duplication_references(HierarchyContext::root(), "");
  make_writers(HierarchyContext::root());
  export_graph_clear();
}

void AbstractHierarchyIterator::release_writers()
{
  for (AbstractHierarchyWriter *writer : writers_.values()) {
    release_writer(writer);
  }
  writers_.clear();
}

void AbstractHierarchyIterator::set_export_subset(ExportSubset export_subset)
{
  export_subset_ = export_subset;
}

std::string AbstractHierarchyIterator::make_valid_name(const std::string &name) const
{
  return name;
}

std::string AbstractHierarchyIterator::get_id_name(const ID *id) const
{
  if (id == nullptr) {
    return "";
  }

  return make_valid_name(std::string(id->name + 2));
}

std::string AbstractHierarchyIterator::make_unique_name(const std::string &original_name,
                                                        Set<std::string> &used_names)
{
  if (original_name.empty()) {
    return "";
  }

  std::string name = BLI_uniquename_cb(
      [&](const StringRef check_name) { return used_names.contains_as(check_name); },
      '_',
      make_valid_name(original_name));

  used_names.add_new(name);
  return name;
}

std::string AbstractHierarchyIterator::get_object_data_path(const HierarchyContext *context) const
{
  BLI_assert(!context->export_path.empty());
  BLI_assert(context->object->data);

  return path_concatenate(context->export_path, get_object_data_name(context->object));
}

void AbstractHierarchyIterator::debug_print_export_graph(const ExportGraph &graph) const
{
  size_t total_graph_size = 0;
  for (const auto item : graph.items()) {
    const ObjectIdentifier &parent_info = item.key;
    const Object *const export_parent = parent_info.object;
    const Object *const duplicator = parent_info.duplicated_by;

    if (duplicator != nullptr) {
      fmt::println("    DU {} (as dupped by {}):",
                   export_parent == nullptr ? "-null-" : (export_parent->id.name + 2),
                   duplicator->id.name + 2);
    }
    else {
      fmt::println("    OB {}:",
                   export_parent == nullptr ? "-null-" : (export_parent->id.name + 2));
    }

    total_graph_size += item.value.size();
    for (HierarchyContext *child_ctx : item.value) {
      if (child_ctx->duplicator == nullptr) {
        fmt::println("       - {}{}{}",
                     child_ctx->export_name.c_str(),
                     child_ctx->weak_export ? " (weak)" : "",
                     child_ctx->original_export_path.empty() ?
                         "" :
                         (std::string("ref ") + child_ctx->original_export_path).c_str());
      }
      else {
        fmt::println("       - {} (dup by {}{}) {}",
                     child_ctx->export_name.c_str(),
                     child_ctx->duplicator->id.name + 2,
                     child_ctx->weak_export ? ", weak" : "",
                     child_ctx->original_export_path.empty() ?
                         "" :
                         (std::string("ref ") + child_ctx->original_export_path).c_str());
      }
    }
  }
  fmt::println("    (Total graph size: {} objects)", total_graph_size);
}

void AbstractHierarchyIterator::export_graph_construct()
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph_);

  /* Add a "null" root node with no children immediately for the case where the top-most node in
   * the scene is not being exported and a root node otherwise wouldn't get added. */
  ObjectIdentifier root_node_id = ObjectIdentifier::for_real_object(nullptr);
  export_graph_.add_new(root_node_id, {});

  DEGObjectIterSettings deg_iter_settings{};
  deg_iter_settings.depsgraph = depsgraph_;
  deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                            DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET;
  DupliList duplilist;
  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, object) {
    /* Non-instanced objects always have their object-parent as export-parent. */
    const bool weak_export = mark_as_weak_export(object);
    visit_object(object, object->parent, weak_export);

    if (weak_export) {
      /* If a duplicator shouldn't be exported, its duplilist also shouldn't be. */
      continue;
    }

    /* Export the duplicated objects instanced by this object. */
    object_duplilist(depsgraph_, scene, object, nullptr, duplilist);
    if (!duplilist.is_empty()) {
      DupliParentFinder dupli_parent_finder;

      for (const DupliObject &dupli_object : duplilist) {
        if (!should_visit_dupli_object(&dupli_object)) {
          continue;
        }
        dupli_parent_finder.insert(&dupli_object);
      }

      for (const DupliObject &dupli_object : duplilist) {
        if (!should_visit_dupli_object(&dupli_object)) {
          continue;
        }
        visit_dupli_object(&dupli_object, object, dupli_parent_finder);
      }
    }

    duplilist.clear();
  }
  DEG_OBJECT_ITER_END;
}

void AbstractHierarchyIterator::connect_loose_objects()
{
  /* Find those objects whose parent is not part of the export graph; these
   * objects would be skipped when traversing the graph as a hierarchy.
   * These objects will have to be re-attached to some parent object in order to
   * fit into the hierarchy. */
  ExportGraph loose_objects_graph = export_graph_;
  for (const ExportChildren &children : export_graph_.values()) {
    for (const HierarchyContext *child : children) {
      /* An object that is marked as a child of another object is not considered 'loose'. */
      ObjectIdentifier child_oid = ObjectIdentifier::for_hierarchy_context(child);
      loose_objects_graph.remove(child_oid);
    }
  }
  /* The root of the hierarchy is always found, so it's never considered 'loose'. */
  loose_objects_graph.remove_contained(ObjectIdentifier::for_graph_root());

  /* Iterate over the loose objects and connect them to their export parent. */
  for (const ObjectIdentifier &graph_key : loose_objects_graph.keys()) {
    Object *object = graph_key.object;

    while (true) {
      /* Loose objects will all be real objects, as duplicated objects always have
       * their duplicator or other exported duplicated object as ancestor. */

      const bool found = export_graph_.contains(ObjectIdentifier::for_real_object(object->parent));
      visit_object(object, object->parent, true);
      if (found) {
        break;
      }
      /* 'object->parent' will never be nullptr here, as the export graph contains the
       * root as nullptr and thus will cause a break above. */
      BLI_assert(object->parent != nullptr);

      object = object->parent;
    }
  }
}

static bool remove_weak_subtrees(const HierarchyContext *context,
                                 AbstractHierarchyIterator::ExportGraph &clean_graph,
                                 const AbstractHierarchyIterator::ExportGraph &input_graph)
{
  bool all_is_weak = context != nullptr && context->weak_export;
  const ObjectIdentifier map_key = ObjectIdentifier::for_hierarchy_context(context);

  const AbstractHierarchyIterator::ExportChildren *children = input_graph.lookup_ptr(map_key);
  if (children) {
    for (HierarchyContext *child_context : *children) {
      bool child_tree_is_weak = remove_weak_subtrees(child_context, clean_graph, input_graph);
      all_is_weak &= child_tree_is_weak;

      if (child_tree_is_weak) {
        /* This subtree is all weak, so we can remove it from the current object's children. */
        clean_graph.lookup(map_key).remove(child_context);
        delete child_context;
      }
    }
  }

  if (all_is_weak) {
    /* This node and all its children are weak, so it can be removed from the export graph. */
    clean_graph.remove(map_key);
  }

  return all_is_weak;
}

void AbstractHierarchyIterator::export_graph_prune()
{
  /* Take a copy of the map so that we can modify while recusing. */
  ExportGraph unpruned_export_graph = export_graph_;
  remove_weak_subtrees(HierarchyContext::root(), export_graph_, unpruned_export_graph);
}

void AbstractHierarchyIterator::export_graph_clear()
{
  for (const ExportChildren &children : export_graph_.values()) {
    for (HierarchyContext *context : children) {
      delete context;
    }
  }
  export_graph_.clear();
  used_names_.clear_and_keep_capacity();
}

void AbstractHierarchyIterator::visit_object(Object *object,
                                             Object *export_parent,
                                             bool weak_export)
{
  HierarchyContext *context = new HierarchyContext();
  context->object = object;
  context->is_object_data_context = false;
  context->export_name = get_object_name(object, export_parent);
  context->export_parent = export_parent;
  context->duplicator = nullptr;
  context->weak_export = weak_export;
  context->animation_check_include_parent = false;
  context->export_path = "";
  context->original_export_path = "";
  context->higher_up_export_path = "";
  context->is_duplisource = false;

  copy_m4_m4(context->matrix_world, object->object_to_world().ptr());

  ObjectIdentifier graph_index = determine_graph_index_object(context);
  context_update_for_graph_index(context, graph_index);

  /* Store this HierarchyContext as child of the export parent. */
  export_graph_.lookup_or_add(graph_index, {}).add_new(context);

  /* Create an empty entry for this object to indicate it is part of the export. This will be used
   * by connect_loose_objects(). Having such an "indicator" will make it possible to do an O(log n)
   * check on whether an object is part of the export, rather than having to check all objects in
   * the map. Note that it's not possible to simply search for (object->parent, nullptr), as the
   * object's parent in Blender may not be the same as its export-parent. */
  ObjectIdentifier object_key = ObjectIdentifier::for_real_object(object);
  export_graph_.add(object_key, {});
}

ObjectIdentifier AbstractHierarchyIterator::determine_graph_index_object(
    const HierarchyContext *context)
{
  return ObjectIdentifier::for_real_object(context->export_parent);
}

void AbstractHierarchyIterator::visit_dupli_object(const DupliObject *dupli_object,
                                                   Object *duplicator,
                                                   const DupliParentFinder &dupli_parent_finder)
{
  HierarchyContext *context = new HierarchyContext();
  context->object = dupli_object->ob;
  context->is_object_data_context = false;
  context->duplicator = duplicator;
  context->persistent_id = PersistentID(dupli_object);
  context->weak_export = false;
  context->export_path = "";
  context->original_export_path = "";
  context->animation_check_include_parent = false;
  context->is_duplisource = false;

  copy_m4_m4(context->matrix_world, dupli_object->mat);

  /* Construct export name for the dupli-instance. */
  std::string export_name = get_object_name(context->object) + "-" +
                            context->persistent_id.as_object_name_suffix();

  Set<std::string> &used_names = used_names_.lookup_or_add(duplicator->id.name, {});
  context->export_name = make_unique_name(make_valid_name(export_name), used_names);

  ObjectIdentifier graph_index = determine_graph_index_dupli(
      context, dupli_object, dupli_parent_finder);
  context_update_for_graph_index(context, graph_index);

  export_graph_.lookup_or_add(graph_index, {}).add_new(context);

  if (dupli_object->ob) {
    this->duplisources_.add(&dupli_object->ob->id);
  }
}

ObjectIdentifier AbstractHierarchyIterator::determine_graph_index_dupli(
    const HierarchyContext *context,
    const DupliObject *dupli_object,
    const DupliParentFinder &dupli_parent_finder)
{
  const DupliObject *dupli_parent = dupli_parent_finder.find_suitable_export_parent(dupli_object);

  if (dupli_parent != nullptr) {
    return ObjectIdentifier::for_duplicated_object(dupli_parent, context->duplicator);
  }
  return ObjectIdentifier::for_real_object(context->duplicator);
}

void AbstractHierarchyIterator::context_update_for_graph_index(
    HierarchyContext *context, const ObjectIdentifier &graph_index) const
{
  /* Update the HierarchyContext so that it is consistent with the graph index. */
  context->export_parent = graph_index.object;

  /* If the parent type is such that it cannot be exported (at least not currently to USD or
   * Alembic), always check the parent for animation. */
  const short partype = context->object->partype & PARTYPE;
  context->animation_check_include_parent |= ELEM(partype, PARBONE, PARVERT1, PARVERT3, PARSKEL);

  if (context->export_parent != context->object->parent) {
    /* The parent object in Blender is NOT used as the export parent. This means
     * that the world transform of this object can be influenced by objects that
     * are not part of its export graph. */
    context->animation_check_include_parent = true;
  }
}

AbstractHierarchyIterator::ExportChildren *AbstractHierarchyIterator::graph_children(
    const HierarchyContext *context)
{
  /* Note: `graph_children` is called during recursive iteration and MUST NOT change the export
   * graph, which would invalidate the iteration. As a result, we cannot add an entry in the
   * graph if the incoming `context` is not found. */
  return export_graph_.lookup_ptr(ObjectIdentifier::for_hierarchy_context(context));
}

void AbstractHierarchyIterator::determine_export_paths(const HierarchyContext *parent_context)
{
  const std::string &parent_export_path = parent_context ? parent_context->export_path : "";

  const ExportChildren *children = graph_children(parent_context);
  if (!children) {
    return;
  }

  for (HierarchyContext *context : *children) {
    context->export_path = path_concatenate(parent_export_path, context->export_name);

    if (context->duplicator == nullptr) {
      /* This is an original (i.e. non-instanced) object, so we should keep track of where it was
       * exported to, just in case it gets instanced somewhere. */
      ID *source_ob = &context->object->id;
      duplisource_export_path_.add(source_ob, context->export_path);

      if (context->object->data != nullptr) {
        ID *source_data = static_cast<ID *>(context->object->data);
        duplisource_export_path_.add(source_data, get_object_data_path(context));
      }
    }

    determine_export_paths(context);
  }
}

bool AbstractHierarchyIterator::determine_duplication_references(
    const HierarchyContext *parent_context, const std::string &indent)
{
  const ExportChildren *children = graph_children(parent_context);
  if (!children) {
    return false;
  }

  /* Will be set to true if any child contexts are instances that were designated
   * as proxies for the original prototype. */
  bool contains_proxy_prototype = false;

  for (HierarchyContext *context : *children) {
    if (context->duplicator != nullptr) {
      ID *source_id = &context->object->id;
      const std::string *source_path = duplisource_export_path_.lookup_ptr(source_id);
      if (!source_path) {
        /* The original was not found, so mark this instance as "the original". */
        context->mark_as_not_instanced();
        duplisource_export_path_.add_new(source_id, context->export_path);
        contains_proxy_prototype = true;
      }
      else {
        context->mark_as_instance_of(*source_path);
      }

      if (context->object->data) {
        ID *source_data_id = (ID *)context->object->data;
        if (!duplisource_export_path_.contains(source_data_id)) {
          /* The original was not found, so mark this instance as "original". */
          std::string data_path = get_object_data_path(context);
          context->mark_as_not_instanced();
          duplisource_export_path_.add_overwrite(source_id, context->export_path);
          duplisource_export_path_.add_new(source_data_id, data_path);
        }
      }
    }
    else {
      /* Determine is this context is for an instance prototype. */
      ID *id = &context->object->id;
      if (duplisources_.contains(id)) {
        context->is_duplisource = true;
      }
    }

    if (determine_duplication_references(context, indent + "  ")) {
      /* A descendant was designated a prototype proxy. If the current context
       * is an instance, we must change it to a prototype proxy as well. */
      if (context->is_instance()) {
        context->mark_as_not_instanced();
        ID *source_id = &context->object->id;
        duplisource_export_path_.add_overwrite(source_id, context->export_path);
      }
      contains_proxy_prototype = true;
    }
  }
  return contains_proxy_prototype;
}

void AbstractHierarchyIterator::make_writers(const HierarchyContext *parent_context)
{
  float parent_matrix_inv_world[4][4];

  if (parent_context) {
    invert_m4_m4(parent_matrix_inv_world, parent_context->matrix_world);
  }
  else {
    unit_m4(parent_matrix_inv_world);
  }

  const ExportChildren *children = graph_children(parent_context);
  if (!children) {
    return;
  }

  bool has_point_instance_ancestor = false;
  if (parent_context &&
      (parent_context->is_point_instance || parent_context->has_point_instance_ancestor))
  {
    has_point_instance_ancestor = true;
  }

  for (HierarchyContext *context : *children) {
    context->has_point_instance_ancestor = has_point_instance_ancestor;

    /* Update the context so that it is correct for this parent-child relation. */
    copy_m4_m4(context->parent_matrix_inv_world, parent_matrix_inv_world);
    if (parent_context != nullptr) {
      context->higher_up_export_path = parent_context->export_path;
    }

    /* Get or create the transform writer. */
    EnsuredWriter transform_writer = ensure_writer(
        context, &AbstractHierarchyIterator::create_transform_writer);

    if (!transform_writer) {
      /* Unable to export, so there is nothing to attach any children to; just abort this entire
       * branch of the export hierarchy. */
      continue;
    }

    const bool need_writers = context->is_point_proto || (!context->is_point_instance &&
                                                          !context->has_point_instance_ancestor);

    BLI_assert(DEG_is_evaluated_id(&context->object->id));
    if ((transform_writer.is_newly_created() || export_subset_.transforms) && need_writers) {
      /* XXX This can lead to too many XForms being written. For example, a camera writer can
       * refuse to write an orthographic camera. By the time that this is known, the XForm has
       * already been written. */
      transform_writer->write(*context);
    }

    if (!context->weak_export && include_data_writers(context) && need_writers) {
      make_writers_particle_systems(context);
      make_writer_object_data(context);
    }

    if (include_child_writers(context)) {
      /* Recurse into this object's children. */
      make_writers(context);
    }
  }

  /* TODO(Sybren): iterate over all unused writers and call unused_during_iteration() or something.
   */
}

HierarchyContext AbstractHierarchyIterator::context_for_object_data(
    const HierarchyContext *object_context) const
{
  HierarchyContext data_context = *object_context;
  data_context.is_object_data_context = true;
  data_context.higher_up_export_path = object_context->export_path;
  data_context.export_name = get_object_data_name(data_context.object);
  data_context.export_path = path_concatenate(data_context.higher_up_export_path,
                                              data_context.export_name);

  const ObjectIdentifier object_key = ObjectIdentifier::for_hierarchy_context(&data_context);
  const ExportChildren *children = export_graph_.lookup_ptr(object_key);
  data_context.is_parent = children ? (children->size() > 0) : false;

  return data_context;
}

void AbstractHierarchyIterator::make_writer_object_data(const HierarchyContext *context)
{
  if (context->object->data == nullptr) {
    return;
  }

  HierarchyContext data_context = context_for_object_data(context);
  if (data_context.is_instance()) {
    ID *object_data = static_cast<ID *>(context->object->data);
    data_context.original_export_path = duplisource_export_path_.lookup(object_data);

    /* If the object is marked as an instance, so should the object data. */
    BLI_assert(data_context.is_instance());
  }

  /* Always write upon creation, otherwise depend on which subset is active. */
  EnsuredWriter data_writer = ensure_writer(&data_context,
                                            &AbstractHierarchyIterator::create_data_writer);
  if (!data_writer) {
    return;
  }

  if (data_writer.is_newly_created() || export_subset_.shapes) {
    data_writer->write(data_context);
  }
}

void AbstractHierarchyIterator::make_writers_particle_systems(
    const HierarchyContext *transform_context)
{
  Object *object = transform_context->object;
  ParticleSystem *psys = static_cast<ParticleSystem *>(object->particlesystem.first);
  for (; psys; psys = psys->next) {
    if (!psys_check_enabled(object, psys, true)) {
      continue;
    }

    HierarchyContext hair_context = *transform_context;
    hair_context.export_name = make_valid_name(psys->name);
    hair_context.export_path = path_concatenate(transform_context->export_path,
                                                hair_context.export_name);
    hair_context.higher_up_export_path = transform_context->export_path;
    hair_context.particle_system = psys;

    EnsuredWriter writer;
    switch (psys->part->type) {
      case PART_HAIR:
        writer = ensure_writer(&hair_context, &AbstractHierarchyIterator::create_hair_writer);
        break;
      case PART_EMITTER:
      case PART_FLUID_FLIP:
      case PART_FLUID_SPRAY:
      case PART_FLUID_BUBBLE:
      case PART_FLUID_FOAM:
      case PART_FLUID_TRACER:
      case PART_FLUID_SPRAYFOAM:
      case PART_FLUID_SPRAYBUBBLE:
      case PART_FLUID_FOAMBUBBLE:
      case PART_FLUID_SPRAYFOAMBUBBLE:
        writer = ensure_writer(&hair_context, &AbstractHierarchyIterator::create_particle_writer);
        break;
    }
    if (!writer) {
      continue;
    }

    /* Always write upon creation, otherwise depend on which subset is active. */
    if (writer.is_newly_created() || export_subset_.shapes) {
      writer->write(hair_context);
    }
  }
}

std::string AbstractHierarchyIterator::get_object_name(const Object *object) const
{
  return get_id_name(&object->id);
}

std::string AbstractHierarchyIterator::get_object_name(const Object *object, const Object *parent)
{
  Set<std::string> &used_names = used_names_.lookup_or_add(parent ? parent->id.name : "", {});
  return make_unique_name(object->id.name + 2, used_names);
}

std::string AbstractHierarchyIterator::get_object_data_name(const Object *object) const
{
  const ID *object_data = static_cast<ID *>(object->data);
  return get_id_name(object_data);
}

AbstractHierarchyWriter *AbstractHierarchyIterator::get_writer(
    const std::string &export_path) const
{
  return writers_.lookup_default(export_path, nullptr);
}

EnsuredWriter AbstractHierarchyIterator::ensure_writer(
    const HierarchyContext *context, AbstractHierarchyIterator::create_writer_func create_func)
{
  AbstractHierarchyWriter *writer = get_writer(context->export_path);
  if (writer != nullptr) {
    return EnsuredWriter::existing(writer);
  }

  writer = (this->*create_func)(context);
  if (writer == nullptr) {
    return EnsuredWriter::empty();
  }

  writers_.add_new(context->export_path, writer);
  return EnsuredWriter::newly_created(writer);
}

std::string AbstractHierarchyIterator::path_concatenate(const std::string &parent_path,
                                                        const std::string &child_path) const
{
  return parent_path + "/" + child_path;
}

bool AbstractHierarchyIterator::mark_as_weak_export(const Object * /*object*/) const
{
  return false;
}
bool AbstractHierarchyIterator::should_visit_dupli_object(const DupliObject *dupli_object) const
{
  /* Do not visit dupli objects if their `no_draw` flag is set (things like custom bone shapes) or
   * if they are meta-balls / text objects. */
  if (dupli_object->no_draw || ELEM(dupli_object->ob->type, OB_MBALL, OB_FONT)) {
    return false;
  }

  return true;
}

}  // namespace blender::io
