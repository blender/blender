/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "IO_abstract_hierarchy_iterator.h"
#include "dupli_parent_finder.hh"

#include <climits>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

#include "BKE_anim_data.h"
#include "BKE_duplilist.hh"
#include "BKE_key.hh"
#include "BKE_object.hh"
#include "BKE_particle.h"

#include "BLI_assert.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"

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

bool HierarchyContext::operator<(const HierarchyContext &other) const
{
  if (object != other.object) {
    return object < other.object;
  }
  if (duplicator != nullptr && duplicator == other.duplicator) {
    /* Only resort to string comparisons when both objects are created by the same duplicator. */
    return export_name < other.export_name;
  }

  return export_parent < other.export_parent;
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

AbstractHierarchyIterator::AbstractHierarchyIterator(Main *bmain, Depsgraph *depsgraph)
    : bmain_(bmain), depsgraph_(depsgraph), export_subset_({true, true})
{
}

AbstractHierarchyIterator::~AbstractHierarchyIterator()
{
  /* release_writers() cannot be called here directly, as it calls into the pure-virtual
   * release_writer() function. By the time this destructor is called, the subclass that implements
   * that pure-virtual function is already destructed. */
  BLI_assert(writers_.empty() || !"release_writers() should be called before the AbstractHierarchyIterator goes out of scope");
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
  for (WriterMap::value_type it : writers_) {
    release_writer(it.second);
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

std::string AbstractHierarchyIterator::get_object_data_path(const HierarchyContext *context) const
{
  BLI_assert(!context->export_path.empty());
  BLI_assert(context->object->data);

  return path_concatenate(context->export_path, get_object_data_name(context->object));
}

std::string AbstractHierarchyIterator::get_object_export_path(const ID *id) const
{
  const ExportPathMap::const_iterator &it = duplisource_export_path_.find(id);

  if (it != duplisource_export_path_.end()) {
    return it->second;
  }

  return std::string();
}

bool AbstractHierarchyIterator::is_prototype(const ID *id) const
{
  return prototypes_.find(id) != prototypes_.end();
}

bool AbstractHierarchyIterator::is_prototype(const Object *obj) const
{
  const ID *obj_id = reinterpret_cast<const ID *>(obj);
  return is_prototype(obj_id);
}

void AbstractHierarchyIterator::debug_print_export_graph(const ExportGraph &graph) const
{
  size_t total_graph_size = 0;
  for (const ExportGraph::value_type &map_iter : graph) {
    const ObjectIdentifier &parent_info = map_iter.first;
    const Object *const export_parent = parent_info.object;
    const Object *const duplicator = parent_info.duplicated_by;

    if (duplicator != nullptr) {
      printf("    DU %s (as dupped by %s):\n",
             export_parent == nullptr ? "-null-" : (export_parent->id.name + 2),
             duplicator->id.name + 2);
    }
    else {
      printf("    OB %s:\n", export_parent == nullptr ? "-null-" : (export_parent->id.name + 2));
    }

    total_graph_size += map_iter.second.size();
    for (HierarchyContext *child_ctx : map_iter.second) {
      if (child_ctx->duplicator == nullptr) {
        printf("       - %s%s%s\n",
               child_ctx->export_name.c_str(),
               child_ctx->weak_export ? " (weak)" : "",
               child_ctx->original_export_path.empty() ?
                   "" :
                   (std::string("ref ") + child_ctx->original_export_path).c_str());
      }
      else {
        printf("       - %s (dup by %s%s) %s\n",
               child_ctx->export_name.c_str(),
               child_ctx->duplicator->id.name + 2,
               child_ctx->weak_export ? ", weak" : "",
               child_ctx->original_export_path.empty() ?
                   "" :
                   (std::string("ref ") + child_ctx->original_export_path).c_str());
      }
    }
  }
  printf("    (Total graph size: %zu objects)\n", total_graph_size);
}

void AbstractHierarchyIterator::export_graph_construct()
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph_);

  /* Add a "null" root node with no children immediately for the case where the top-most node in
   * the scene is not being exported and a root node otherwise wouldn't get added. */
  ExportGraph::key_type root_node_id = ObjectIdentifier::for_real_object(nullptr);
  export_graph_[root_node_id] = ExportChildren();

  DEGObjectIterSettings deg_iter_settings{};
  deg_iter_settings.depsgraph = depsgraph_;
  deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                            DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET;
  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, object) {
    /* Non-instanced objects always have their object-parent as export-parent. */
    const bool weak_export = mark_as_weak_export(object);
    visit_object(object, object->parent, weak_export);

    if (weak_export) {
      /* If a duplicator shouldn't be exported, its duplilist also shouldn't be. */
      continue;
    }

    /* Export the duplicated objects instanced by this object. */
    ListBase *lb = object_duplilist(depsgraph_, scene, object);

    if (lb && object->particlesystem.first == nullptr) {
      DupliParentFinder dupli_parent_finder;

      // Construct the set of duplicated objects, so that later we can determine whether a parent
      // is also duplicated itself.
      std::set<Object *> dupli_set;

      LISTBASE_FOREACH (DupliObject *, dupli_object, lb) {
        PersistentID persistent_id(dupli_object);
        if (!should_visit_dupli_object(dupli_object)) {
          continue;
        }
        dupli_parent_finder.insert(dupli_object);
      }

      LISTBASE_FOREACH (DupliObject *, dupli_object, lb) {
        if (!should_visit_dupli_object(dupli_object)) {
          continue;
        }
        visit_dupli_object(dupli_object, object, dupli_parent_finder);
      }
    }

    free_object_duplilist(lb);
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
  for (const ExportGraph::value_type &map_iter : export_graph_) {
    for (const HierarchyContext *child : map_iter.second) {
      /* An object that is marked as a child of another object is not considered 'loose'. */
      ObjectIdentifier child_oid = ObjectIdentifier::for_hierarchy_context(child);
      loose_objects_graph.erase(child_oid);
    }
  }
  /* The root of the hierarchy is always found, so it's never considered 'loose'. */
  loose_objects_graph.erase(ObjectIdentifier::for_graph_root());

  /* Iterate over the loose objects and connect them to their export parent. */
  for (const ExportGraph::value_type &map_iter : loose_objects_graph) {
    const ObjectIdentifier &graph_key = map_iter.first;
    Object *object = graph_key.object;

    while (true) {
      /* Loose objects will all be real objects, as duplicated objects always have
       * their duplicator or other exported duplicated object as ancestor. */

      ExportGraph::iterator found_parent_iter = export_graph_.find(
          ObjectIdentifier::for_real_object(object->parent));
      visit_object(object, object->parent, true);
      if (found_parent_iter != export_graph_.end()) {
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

  AbstractHierarchyIterator::ExportGraph::const_iterator child_iterator;

  child_iterator = input_graph.find(map_key);
  if (child_iterator != input_graph.end()) {
    for (HierarchyContext *child_context : child_iterator->second) {
      bool child_tree_is_weak = remove_weak_subtrees(child_context, clean_graph, input_graph);
      all_is_weak &= child_tree_is_weak;

      if (child_tree_is_weak) {
        /* This subtree is all weak, so we can remove it from the current object's children. */
        clean_graph[map_key].erase(child_context);
        delete child_context;
      }
    }
  }

  if (all_is_weak) {
    /* This node and all its children are weak, so it can be removed from the export graph. */
    clean_graph.erase(map_key);
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
  for (ExportGraph::iterator::value_type &it : export_graph_) {
    for (HierarchyContext *context : it.second) {
      delete context;
    }
  }
  export_graph_.clear();
}

void AbstractHierarchyIterator::visit_object(Object *object,
                                             Object *export_parent,
                                             bool weak_export)
{
  HierarchyContext *context = new HierarchyContext();
  context->object = object;
  context->export_name = get_object_name(object);
  context->export_parent = export_parent;
  context->duplicator = nullptr;
  context->weak_export = weak_export;
  context->animation_check_include_parent = true;
  context->export_path = "";
  context->original_export_path = "";
  context->higher_up_export_path = "";

  copy_m4_m4(context->matrix_world, object->object_to_world().ptr());

  ExportGraph::key_type graph_index = determine_graph_index_object(context);
  context_update_for_graph_index(context, graph_index);

  /* Store this HierarchyContext as child of the export parent. */
  export_graph_[graph_index].insert(context);

  /* Create an empty entry for this object to indicate it is part of the export. This will be used
   * by connect_loose_objects(). Having such an "indicator" will make it possible to do an O(log n)
   * check on whether an object is part of the export, rather than having to check all objects in
   * the map. Note that it's not possible to simply search for (object->parent, nullptr), as the
   * object's parent in Blender may not be the same as its export-parent. */
  ExportGraph::key_type object_key = ObjectIdentifier::for_real_object(object);
  if (export_graph_.find(object_key) == export_graph_.end()) {
    export_graph_[object_key] = ExportChildren();
  }
}

AbstractHierarchyIterator::ExportGraph::key_type AbstractHierarchyIterator::
    determine_graph_index_object(const HierarchyContext *context)
{
  return ObjectIdentifier::for_real_object(context->export_parent);
}

void AbstractHierarchyIterator::visit_dupli_object(DupliObject *dupli_object,
                                                   Object *duplicator,
                                                   const DupliParentFinder &dupli_parent_finder)
{

  HierarchyContext *context = new HierarchyContext();
  context->object = dupli_object->ob;
  context->duplicator = duplicator;
  context->persistent_id = PersistentID(dupli_object);
  context->weak_export = false;
  context->export_path = "";
  context->original_export_path = "";
  context->animation_check_include_parent = false;

  copy_m4_m4(context->matrix_world, dupli_object->mat);

  /* Construct export name for the dupli-instance. */
  std::stringstream export_name_stream;
  export_name_stream << get_object_name(context->object) << "-"
                     << context->persistent_id.as_object_name_suffix();
  context->export_name = make_valid_name(export_name_stream.str());

  ExportGraph::key_type graph_index = determine_graph_index_dupli(
      context, dupli_object, dupli_parent_finder);
  context_update_for_graph_index(context, graph_index);

  export_graph_[graph_index].insert(context);

  // ExportGraph::key_type graph_index;
  // bool animation_check_include_parent = true;

  // HierarchyContext *context = new HierarchyContext();
  // context->object = dupli_object->ob;
  // context->duplicator = duplicator;
  // context->persistent_id = PersistentID(dupli_object);
  // context->weak_export = false;
  // context->export_path = "";
  // context->original_export_path = "";
  // context->export_path = "";
  // context->animation_check_include_parent = false;

  ///* If the dupli-object's parent is also instanced by this object, use that as the
  // * export parent. Otherwise use the dupli-parent as export parent. */
  // Object *parent = dupli_object->ob->parent;
  // if (parent != nullptr && dupli_set.find(parent) != dupli_set.end()) {
  //  // The parent object is part of the duplicated collection.
  //  context->export_parent = parent;
  //  graph_index = std::make_pair(parent, duplicator);
  //  // This bool used to be false by default
  //  // This was stopping a certain combination of drivers
  //  // and rigging to not properly export.
  //  // For now, we have switched to only setting to false here
  //  animation_check_include_parent = false;
  //}
  // else {
  //  /* The parent object is NOT part of the duplicated collection. This means that the world
  //   * transform of this dupli-object can be influenced by objects that are not part of its
  //   * export graph. */
  //  context->export_parent = duplicator;
  //  graph_index = std::make_pair(duplicator, nullptr);
  //}

  // context->animation_check_include_parent = animation_check_include_parent;
  //
  // copy_m4_m4(context->matrix_world, dupli_object->mat);

  ///* Construct export name for the dupli-instance. */
  // std::stringstream export_name_stream;
  // export_name_stream << get_object_name(context->object) << "-"
  //                   << context->persistent_id.as_object_name_suffix();
  // context->export_name = make_valid_name(export_name_stream.str());

  // ExportGraph::key_type graph_index = determine_graph_index_dupli(
  //    context, dupli_object, dupli_parent_finder);
  // context_update_for_graph_index(context, graph_index);

  // export_graph_[graph_index].insert(context);
}

AbstractHierarchyIterator::ExportGraph::key_type AbstractHierarchyIterator::
    determine_graph_index_dupli(const HierarchyContext *context,
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
    HierarchyContext *context, const ExportGraph::key_type &graph_index) const
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

AbstractHierarchyIterator::ExportChildren &AbstractHierarchyIterator::graph_children(
    const HierarchyContext *context)
{
  return export_graph_[ObjectIdentifier::for_hierarchy_context(context)];
}

void AbstractHierarchyIterator::determine_export_paths(const HierarchyContext *parent_context)
{
  const std::string &parent_export_path = parent_context ? parent_context->export_path : "";

  for (HierarchyContext *context : graph_children(parent_context)) {
    context->export_path = path_concatenate(parent_export_path, context->export_name);

    if (context->duplicator == nullptr) {
      /* This is an original (i.e. non-instanced) object, so we should keep track of where it was
       * exported to, just in case it gets instanced somewhere. */
      ID *source_ob = &context->object->id;
      duplisource_export_path_[source_ob] = context->export_path;

      if (context->object->data != nullptr) {
        ID *source_data = static_cast<ID *>(context->object->data);
        duplisource_export_path_[source_data] = get_object_data_path(context);
      }
    }

    determine_export_paths(context);
  }
}

void AbstractHierarchyIterator::determine_duplication_references(
    const HierarchyContext *parent_context, const std::string &indent)
{
  ExportChildren children = graph_children(parent_context);

  for (HierarchyContext *context : children) {
    if (context->duplicator != nullptr) {
      ID *source_id = &context->object->id;
      const ExportPathMap::const_iterator &it = duplisource_export_path_.find(source_id);

      if (it == duplisource_export_path_.end()) {
        /* The original was not found, so mark this instance as "the original". */
        context->mark_as_not_instanced();
        duplisource_export_path_[source_id] = context->export_path;

        if (context->object->data) {
          ID* source_data_id = (ID*)context->object->data;
          const ExportPathMap::const_iterator& it = duplisource_export_path_.find(source_data_id);

          if (it == duplisource_export_path_.end()) {
            /* The original data was not found, so mark this instance data as "original". */
            std::string data_path = get_object_data_path(context);
            duplisource_export_path_[source_data_id] = data_path;
          }
        }
      }
      else {
        /* Mark this as an instance of the original, but be careful
         * to handle the case where this instance was previously marked
         * as "the original", per the logic in the "if" clause above.
         * I.e., if we are animating and if the instance source_id
         * was marked as "the original" in a previous frame, then
         * we should not mark this context as an instance. We can
         * prevent this from happening by ensuring that we never pass
         * the context's export path as an argument to mark_as_instance_of(). */
        if (context->export_path != it->second) {
          context->mark_as_instance_of(it->second);
        }
      }

      prototypes_.insert(source_id);
    }

    determine_duplication_references(context, indent + "  ");
  }
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

  for (HierarchyContext *context : graph_children(parent_context)) {
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
      return;
    }

    BLI_assert(DEG_is_evaluated_object(context->object));
    if (transform_writer.is_newly_created() || export_subset_.transforms) {
      /* XXX This can lead to too many XForms being written. For example, a camera writer can
       * refuse to write an orthographic camera. By the time that this is known, the XForm has
       * already been written. */
      transform_writer->write(*context);
    }

    if (!context->weak_export && include_data_writers(context)) {
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
  data_context.higher_up_export_path = object_context->export_path;
  data_context.export_name = get_object_data_name(data_context.object);
  data_context.export_path = path_concatenate(data_context.higher_up_export_path,
                                              data_context.export_name);
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
    data_context.original_export_path = duplisource_export_path_[object_data];

    /* If the object is marked as an instance, so should the object data. */
    /* TODO(makowalski): this fails when testing with collection instances. */
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

std::string AbstractHierarchyIterator::get_object_data_name(const Object *object) const
{
  ID *object_data = static_cast<ID *>(object->data);
  return get_id_name(object_data);
}

AbstractHierarchyWriter *AbstractHierarchyIterator::get_writer(
    const std::string &export_path) const
{
  WriterMap::const_iterator it = writers_.find(export_path);

  if (it == writers_.end()) {
    return nullptr;
  }
  return it->second;
}

EnsuredWriter AbstractHierarchyIterator::ensure_writer(
    HierarchyContext *context, AbstractHierarchyIterator::create_writer_func create_func)
{
  AbstractHierarchyWriter *writer = get_writer(context->export_path);
  if (writer != nullptr) {
    return EnsuredWriter::existing(writer);
  }

  writer = (this->*create_func)(context);
  if (writer == nullptr) {
    return EnsuredWriter::empty();
  }

  writers_[context->export_path] = writer;
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
  /* Removing dupli_object->no_draw hides things like custom bone shapes. */
  return !dupli_object->no_draw;
}

}  // namespace blender::io
