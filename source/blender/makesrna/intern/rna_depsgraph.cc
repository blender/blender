/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "BLI_path_utils.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "BKE_duplilist.hh"

#include "DEG_depsgraph.hh"

#define STATS_MAX_SIZE 16384

#ifdef RNA_RUNTIME

#  ifdef WITH_PYTHON
#    include "BPY_extern.hh"
#  endif

#  include "BLI_iterator.h"
#  include "BLI_math_matrix.h"
#  include "BLI_math_vector.h"
#  include "BLI_string.h"

#  include "DNA_scene_types.h"

#  include "RNA_access.hh"

#  include "BKE_object.hh"
#  include "BKE_report.hh"
#  include "BKE_scene.hh"

#  include "DEG_depsgraph_build.hh"
#  include "DEG_depsgraph_debug.hh"
#  include "DEG_depsgraph_query.hh"

#  include "MEM_guardedalloc.h"

/* **************** Object Instance **************** */

struct RNA_DepsgraphIterator {
  BLI_Iterator iter;
#  ifdef WITH_PYTHON
  /**
   * Store the Python instance so the #BPy_StructRNA can be set as invalid iteration is completed.
   * Otherwise accessing from Python (e.g. console auto-complete) crashes, see: #100286. */
  void *py_instance;
#  endif
};

#  ifdef WITH_PYTHON
void **rna_DepsgraphIterator_instance(PointerRNA *ptr)
{
  RNA_DepsgraphIterator *di = static_cast<RNA_DepsgraphIterator *>(ptr->data);
  return &di->py_instance;
}
#  endif

/* Temporary hack for Cycles until it is changed to work with the C API directly. */
extern "C" DupliObject *rna_hack_DepsgraphObjectInstance_dupli_object_get(PointerRNA *ptr)
{
  RNA_DepsgraphIterator *di = static_cast<RNA_DepsgraphIterator *>(ptr->data);
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)di->iter.data;
  return deg_iter->dupli_object_current;
}

static PointerRNA rna_DepsgraphObjectInstance_object_get(PointerRNA *ptr)
{
  RNA_DepsgraphIterator *di = static_cast<RNA_DepsgraphIterator *>(ptr->data);
  return RNA_id_pointer_create(reinterpret_cast<ID *>(di->iter.current));
}

static bool rna_DepsgraphObjectInstance_is_instance_get(PointerRNA *ptr)
{
  RNA_DepsgraphIterator *di = static_cast<RNA_DepsgraphIterator *>(ptr->data);
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)di->iter.data;
  return (deg_iter->dupli_object_current != nullptr);
}

static PointerRNA rna_DepsgraphObjectInstance_instance_object_get(PointerRNA *ptr)
{
  RNA_DepsgraphIterator *di = static_cast<RNA_DepsgraphIterator *>(ptr->data);
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)di->iter.data;
  Object *instance_object = nullptr;
  if (deg_iter->dupli_object_current != nullptr) {
    instance_object = deg_iter->dupli_object_current->ob;
  }
  return RNA_id_pointer_create(reinterpret_cast<ID *>(instance_object));
}

static bool rna_DepsgraphObjectInstance_show_self_get(PointerRNA *ptr)
{
  RNA_DepsgraphIterator *di = static_cast<RNA_DepsgraphIterator *>(ptr->data);
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)di->iter.data;
  int ob_visibility = BKE_object_visibility(static_cast<const Object *>(di->iter.current),
                                            deg_iter->eval_mode);
  return (ob_visibility & OB_VISIBLE_SELF) != 0;
}

static bool rna_DepsgraphObjectInstance_show_particles_get(PointerRNA *ptr)
{
  RNA_DepsgraphIterator *di = static_cast<RNA_DepsgraphIterator *>(ptr->data);
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)di->iter.data;
  int ob_visibility = BKE_object_visibility(static_cast<const Object *>(di->iter.current),
                                            deg_iter->eval_mode);
  return (ob_visibility & OB_VISIBLE_PARTICLES) != 0;
}

static PointerRNA rna_DepsgraphObjectInstance_parent_get(PointerRNA *ptr)
{
  RNA_DepsgraphIterator *di = static_cast<RNA_DepsgraphIterator *>(ptr->data);
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)di->iter.data;
  Object *dupli_parent = nullptr;
  if (deg_iter->dupli_object_current != nullptr) {
    dupli_parent = deg_iter->dupli_parent;
  }
  return RNA_id_pointer_create(reinterpret_cast<ID *>(dupli_parent));
}

static PointerRNA rna_DepsgraphObjectInstance_particle_system_get(PointerRNA *ptr)
{
  RNA_DepsgraphIterator *di = static_cast<RNA_DepsgraphIterator *>(ptr->data);
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)di->iter.data;
  ParticleSystem *particle_system = nullptr;
  if (deg_iter->dupli_object_current != nullptr) {
    particle_system = deg_iter->dupli_object_current->particle_system;
  }
  return RNA_pointer_create_with_parent(*ptr, &RNA_ParticleSystem, particle_system);
}

static void rna_DepsgraphObjectInstance_persistent_id_get(PointerRNA *ptr, int *persistent_id)
{
  RNA_DepsgraphIterator *di = static_cast<RNA_DepsgraphIterator *>(ptr->data);
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)di->iter.data;
  if (deg_iter->dupli_object_current != nullptr) {
    memcpy(persistent_id,
           deg_iter->dupli_object_current->persistent_id,
           sizeof(deg_iter->dupli_object_current->persistent_id));
  }
  else {
    memset(persistent_id, 0, sizeof(deg_iter->dupli_object_current->persistent_id));
  }
}

static int rna_DepsgraphObjectInstance_random_id_get(PointerRNA *ptr)
{
  RNA_DepsgraphIterator *di = static_cast<RNA_DepsgraphIterator *>(ptr->data);
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)di->iter.data;
  if (deg_iter->dupli_object_current != nullptr) {
    return int(deg_iter->dupli_object_current->random_id);
  }
  return 0;
}

static void rna_DepsgraphObjectInstance_matrix_world_get(PointerRNA *ptr, float *mat)
{
  RNA_DepsgraphIterator *di = static_cast<RNA_DepsgraphIterator *>(ptr->data);
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)di->iter.data;
  if (deg_iter->dupli_object_current != nullptr) {
    copy_m4_m4((float (*)[4])mat, deg_iter->dupli_object_current->mat);
  }
  else {
    /* We can return actual object's matrix here, no reason to return identity matrix
     * when this is not actually an instance... */
    Object *ob = (Object *)di->iter.current;
    copy_m4_m4((float (*)[4])mat, ob->object_to_world().ptr());
  }
}

static void rna_DepsgraphObjectInstance_orco_get(PointerRNA *ptr, float *orco)
{
  RNA_DepsgraphIterator *di = static_cast<RNA_DepsgraphIterator *>(ptr->data);
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)di->iter.data;
  if (deg_iter->dupli_object_current != nullptr) {
    copy_v3_v3(orco, deg_iter->dupli_object_current->orco);
  }
  else {
    zero_v3(orco);
  }
}

static void rna_DepsgraphObjectInstance_uv_get(PointerRNA *ptr, float *uv)
{
  RNA_DepsgraphIterator *di = static_cast<RNA_DepsgraphIterator *>(ptr->data);
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)di->iter.data;
  if (deg_iter->dupli_object_current != nullptr) {
    copy_v2_v2(uv, deg_iter->dupli_object_current->uv);
  }
  else {
    zero_v2(uv);
  }
}

/* ******************** Sorted  ***************** */

static int rna_Depsgraph_mode_get(PointerRNA *ptr)
{
  Depsgraph *depsgraph = static_cast<Depsgraph *>(ptr->data);
  return DEG_get_mode(depsgraph);
}

/* ******************** Updates ***************** */

static PointerRNA rna_DepsgraphUpdate_id_get(PointerRNA *ptr)
{
  return RNA_id_pointer_create(reinterpret_cast<ID *>(ptr->data));
}

static bool rna_DepsgraphUpdate_is_updated_transform_get(PointerRNA *ptr)
{
  ID *id = static_cast<ID *>(ptr->data);
  return ((id->recalc & ID_RECALC_TRANSFORM) != 0);
}

static bool rna_DepsgraphUpdate_is_updated_shading_get(PointerRNA *ptr)
{
  /* Assume any animated parameters can affect shading, we don't have fine
   * grained enough updates to distinguish this. */
  ID *id = static_cast<ID *>(ptr->data);
  return ((id->recalc & (ID_RECALC_SHADING | ID_RECALC_ANIMATION)) != 0);
}

static bool rna_DepsgraphUpdate_is_updated_geometry_get(PointerRNA *ptr)
{
  ID *id = static_cast<ID *>(ptr->data);
  if (id->recalc & ID_RECALC_GEOMETRY) {
    return true;
  }
  if (GS(id->name) != ID_OB) {
    return false;
  }
  Object *object = (Object *)id;
  ID *data = static_cast<ID *>(object->data);
  if (data == nullptr) {
    return false;
  }
  return ((data->recalc & ID_RECALC_ALL) != 0);
}

/* **************** Depsgraph **************** */

static void rna_Depsgraph_debug_relations_graphviz(Depsgraph *depsgraph,
                                                   const char *filepath,
                                                   const char **r_str,
                                                   int *r_len)
{
  const std::string dot_str = DEG_debug_graph_to_dot(*depsgraph, "Depsgraph");
  *r_len = dot_str.size();
  *r_str = BLI_strdup(dot_str.c_str());

  if (filepath) {
    FILE *f = fopen(filepath, "w");
    if (f == nullptr) {
      return;
    }
    fprintf(f, "%s", dot_str.c_str());
    fclose(f);
  }
}

static void rna_Depsgraph_debug_stats_gnuplot(Depsgraph *depsgraph,
                                              const char *filepath,
                                              const char *output_filepath)
{
  FILE *f = fopen(filepath, "w");
  if (f == nullptr) {
    return;
  }
  DEG_debug_stats_gnuplot(depsgraph, f, "Timing Statistics", output_filepath);
  fclose(f);
}

static void rna_Depsgraph_debug_tag_update(Depsgraph *depsgraph)
{
  DEG_graph_tag_relations_update(depsgraph);
}

static void rna_Depsgraph_debug_stats(Depsgraph *depsgraph, char *result)
{
  size_t outer, ops, rels;
  DEG_stats_simple(depsgraph, &outer, &ops, &rels);
  BLI_snprintf(result,
               STATS_MAX_SIZE,
               "Approx %zu Operations, %zu Relations, %zu Outer Nodes",
               ops,
               rels,
               outer);
}

static void rna_Depsgraph_update(Depsgraph *depsgraph, Main *bmain, ReportList *reports)
{
  if (DEG_is_evaluating(depsgraph)) {
    BKE_report(reports, RPT_ERROR, "Dependency graph update requested during evaluation");
    return;
  }

#  ifdef WITH_PYTHON
  /* Allow drivers to be evaluated */
  BPy_BEGIN_ALLOW_THREADS;
#  endif

  BKE_scene_graph_update_tagged(depsgraph, bmain);

#  ifdef WITH_PYTHON
  BPy_END_ALLOW_THREADS;
#  endif
}

/* Iteration over objects, simple version */

static void rna_Depsgraph_objects_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  iter->internal.custom = MEM_callocN<BLI_Iterator>(__func__);
  DEGObjectIterData *data = MEM_new<DEGObjectIterData>(__func__);
  DEGObjectIterSettings *deg_iter_settings = MEM_callocN<DEGObjectIterSettings>(__func__);
  deg_iter_settings->depsgraph = (Depsgraph *)ptr->data;
  deg_iter_settings->flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY | DEG_ITER_OBJECT_FLAG_VISIBLE |
                             DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET;

  data->settings = deg_iter_settings;
  data->graph = deg_iter_settings->depsgraph;
  data->flag = deg_iter_settings->flags;

  ((BLI_Iterator *)iter->internal.custom)->valid = true;
  DEG_iterator_objects_begin(static_cast<BLI_Iterator *>(iter->internal.custom), data);
  iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Depsgraph_objects_next(CollectionPropertyIterator *iter)
{
  DEG_iterator_objects_next(static_cast<BLI_Iterator *>(iter->internal.custom));
  iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Depsgraph_objects_end(CollectionPropertyIterator *iter)
{
  DEGObjectIterData *data = (DEGObjectIterData *)((BLI_Iterator *)iter->internal.custom)->data;
  DEG_iterator_objects_end(static_cast<BLI_Iterator *>(iter->internal.custom));
  MEM_freeN(data->settings);
  MEM_delete(data);
  MEM_freeN(iter->internal.custom);
}

static PointerRNA rna_Depsgraph_objects_get(CollectionPropertyIterator *iter)
{
  Object *ob = static_cast<Object *>(((BLI_Iterator *)iter->internal.custom)->current);
  return RNA_id_pointer_create(reinterpret_cast<ID *>(ob));
}

/* Iteration over objects, extended version
 *
 * Contains extra information about duplicator and persistent ID.
 */

/* XXX Ugly python seems to query next item of an iterator before using current one (see #57558).
 * This forces us to use that nasty ping-pong game between two sets of iterator data,
 * so that previous one remains valid memory for python to access to. Yuck.
 */
struct RNA_Depsgraph_Instances_Iterator {
  RNA_DepsgraphIterator iterators[2];
  DEGObjectIterData deg_data[2];
  DupliObject dupli_object_current[2];
  int counter;
};

static void rna_Depsgraph_object_instances_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  RNA_Depsgraph_Instances_Iterator *di_it = MEM_new<RNA_Depsgraph_Instances_Iterator>(__func__);
  iter->internal.custom = di_it;
  DEGObjectIterSettings *deg_iter_settings = MEM_callocN<DEGObjectIterSettings>(__func__);
  deg_iter_settings->depsgraph = (Depsgraph *)ptr->data;
  deg_iter_settings->flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                             DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET | DEG_ITER_OBJECT_FLAG_VISIBLE |
                             DEG_ITER_OBJECT_FLAG_DUPLI;

  DEGObjectIterData *data = &di_it->deg_data[0];
  data->settings = deg_iter_settings;
  data->graph = deg_iter_settings->depsgraph;
  data->flag = deg_iter_settings->flags;

  di_it->iterators[0].iter.valid = true;
  DEG_iterator_objects_begin(&di_it->iterators[0].iter, data);
  iter->valid = di_it->iterators[0].iter.valid;
}

static void rna_Depsgraph_object_instances_next(CollectionPropertyIterator *iter)
{
  RNA_Depsgraph_Instances_Iterator *di_it = (RNA_Depsgraph_Instances_Iterator *)
                                                iter->internal.custom;

  /* We need to copy current iterator status to next one being worked on. */
  di_it->iterators[(di_it->counter + 1) % 2].iter = di_it->iterators[di_it->counter % 2].iter;
  di_it->deg_data[(di_it->counter + 1) % 2].transfer_from(di_it->deg_data[di_it->counter % 2]);
  di_it->counter++;

  di_it->iterators[di_it->counter % 2].iter.data = &di_it->deg_data[di_it->counter % 2];
  DEG_iterator_objects_next(&di_it->iterators[di_it->counter % 2].iter);
  /* Dupli_object_current is also temp memory generated during the iterations,
   * it may be freed when last item has been iterated,
   * so we have same issue as with the iterator itself:
   * we need to keep a local copy, which memory remains valid a bit longer,
   * for Python accesses to work. */
  if (di_it->deg_data[di_it->counter % 2].dupli_object_current != nullptr) {
    di_it->dupli_object_current[di_it->counter % 2] =
        *di_it->deg_data[di_it->counter % 2].dupli_object_current;
    di_it->deg_data[di_it->counter % 2].dupli_object_current =
        &di_it->dupli_object_current[di_it->counter % 2];
  }
  iter->valid = di_it->iterators[di_it->counter % 2].iter.valid;
}

static void rna_Depsgraph_object_instances_end(CollectionPropertyIterator *iter)
{
  RNA_Depsgraph_Instances_Iterator *di_it = (RNA_Depsgraph_Instances_Iterator *)
                                                iter->internal.custom;
  for (int i = 0; i < ARRAY_SIZE(di_it->iterators); i++) {
    RNA_DepsgraphIterator *di = &di_it->iterators[i];
    DEGObjectIterData *data = &di_it->deg_data[i];
    if (i == 0) {
      /* Is shared between both iterators. */
      MEM_freeN(data->settings);
    }
    DEG_iterator_objects_end(&di->iter);

#  ifdef WITH_PYTHON
    if (di->py_instance) {
      BPY_DECREF_RNA_INVALIDATE(di->py_instance);
    }
#  endif
  }

  MEM_delete(di_it);
}

static PointerRNA rna_Depsgraph_object_instances_get(CollectionPropertyIterator *iter)
{
  RNA_Depsgraph_Instances_Iterator *di_it = (RNA_Depsgraph_Instances_Iterator *)
                                                iter->internal.custom;
  RNA_DepsgraphIterator *di = &di_it->iterators[di_it->counter % 2];
  return RNA_pointer_create_with_parent(iter->parent, &RNA_DepsgraphObjectInstance, di);
}

/* Iteration over evaluated IDs */

static void rna_Depsgraph_ids_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  iter->internal.custom = MEM_callocN<BLI_Iterator>(__func__);
  DEGIDIterData *data = MEM_callocN<DEGIDIterData>(__func__);

  data->graph = (Depsgraph *)ptr->data;

  ((BLI_Iterator *)iter->internal.custom)->valid = true;
  DEG_iterator_ids_begin(static_cast<BLI_Iterator *>(iter->internal.custom), data);
  iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Depsgraph_ids_next(CollectionPropertyIterator *iter)
{
  DEG_iterator_ids_next(static_cast<BLI_Iterator *>(iter->internal.custom));
  iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Depsgraph_ids_end(CollectionPropertyIterator *iter)
{
  DEG_iterator_ids_end(static_cast<BLI_Iterator *>(iter->internal.custom));
  MEM_freeN(((BLI_Iterator *)iter->internal.custom)->data);
  MEM_freeN(iter->internal.custom);
}

static PointerRNA rna_Depsgraph_ids_get(CollectionPropertyIterator *iter)
{
  ID *id = static_cast<ID *>(((BLI_Iterator *)iter->internal.custom)->current);
  return RNA_id_pointer_create(id);
}

static void rna_Depsgraph_updates_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  iter->internal.custom = MEM_callocN<BLI_Iterator>(__func__);
  DEGIDIterData *data = MEM_callocN<DEGIDIterData>(__func__);

  data->graph = (Depsgraph *)ptr->data;
  data->only_updated = true;

  ((BLI_Iterator *)iter->internal.custom)->valid = true;
  DEG_iterator_ids_begin(static_cast<BLI_Iterator *>(iter->internal.custom), data);
  iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static PointerRNA rna_Depsgraph_updates_get(CollectionPropertyIterator *iter)
{
  ID *id = static_cast<ID *>(((BLI_Iterator *)iter->internal.custom)->current);
  return RNA_pointer_create_with_parent(iter->parent, &RNA_DepsgraphUpdate, id);
}

static ID *rna_Depsgraph_id_eval_get(Depsgraph *depsgraph, ID *id_orig)
{
  return DEG_get_evaluated(depsgraph, id_orig);
}

static bool rna_Depsgraph_id_type_updated(Depsgraph *depsgraph, int id_type)
{
  return DEG_id_type_updated(depsgraph, id_type);
}

static PointerRNA rna_Depsgraph_scene_get(PointerRNA *ptr)
{
  Depsgraph *depsgraph = (Depsgraph *)ptr->data;
  Scene *scene = DEG_get_input_scene(depsgraph);
  PointerRNA newptr = RNA_id_pointer_create(&scene->id);
  return newptr;
}

static PointerRNA rna_Depsgraph_view_layer_get(PointerRNA *ptr)
{
  Depsgraph *depsgraph = (Depsgraph *)ptr->data;
  Scene *scene = DEG_get_input_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);
  PointerRNA newptr = RNA_pointer_create_id_subdata(scene->id, &RNA_ViewLayer, view_layer);
  return newptr;
}

static PointerRNA rna_Depsgraph_scene_eval_get(PointerRNA *ptr)
{
  Depsgraph *depsgraph = (Depsgraph *)ptr->data;
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  PointerRNA newptr = RNA_id_pointer_create(&scene_eval->id);
  return newptr;
}

static PointerRNA rna_Depsgraph_view_layer_eval_get(PointerRNA *ptr)
{
  Depsgraph *depsgraph = (Depsgraph *)ptr->data;
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer_eval = DEG_get_evaluated_view_layer(depsgraph);
  PointerRNA newptr = RNA_pointer_create_id_subdata(
      scene_eval->id, &RNA_ViewLayer, view_layer_eval);
  return newptr;
}

#else

static void rna_def_depsgraph_instance(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "DepsgraphObjectInstance", nullptr);
  RNA_def_struct_ui_text(srna,
                         "Dependency Graph Object Instance",
                         "Extended information about dependency graph object iterator "
                         "(Warning: All data here is 'evaluated' one, not original .blend IDs)");

#  ifdef WITH_PYTHON
  RNA_def_struct_register_funcs(srna, nullptr, nullptr, "rna_DepsgraphIterator_instance");
#  endif

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_ui_text(prop, "Object", "Evaluated object the iterator points to");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_pointer_funcs(
      prop, "rna_DepsgraphObjectInstance_object_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "show_self", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Show Self", "The object geometry itself should be visible in the render");
  RNA_def_property_boolean_funcs(prop, "rna_DepsgraphObjectInstance_show_self_get", nullptr);

  prop = RNA_def_property(srna, "show_particles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Show Particles", "Particles part of the object should be visible in the render");
  RNA_def_property_boolean_funcs(prop, "rna_DepsgraphObjectInstance_show_particles_get", nullptr);

  prop = RNA_def_property(srna, "is_instance", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Is Instance", "Denotes if the object is generated by another object");
  RNA_def_property_boolean_funcs(prop, "rna_DepsgraphObjectInstance_is_instance_get", nullptr);

  prop = RNA_def_property(srna, "instance_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_ui_text(
      prop, "Instance Object", "Evaluated object which is being instanced by this iterator");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_pointer_funcs(
      prop, "rna_DepsgraphObjectInstance_instance_object_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_ui_text(
      prop, "Parent", "If the object is an instance, the parent object that generated it");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_pointer_funcs(
      prop, "rna_DepsgraphObjectInstance_parent_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "particle_system", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ParticleSystem");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Particle System", "Evaluated particle system that this object was instanced from");
  RNA_def_property_pointer_funcs(
      prop, "rna_DepsgraphObjectInstance_particle_system_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "persistent_id", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Persistent ID",
      "Persistent identifier for inter-frame matching of objects with motion blur");
  RNA_def_property_array(prop, MAX_DUPLI_RECUR);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_int_funcs(
      prop, "rna_DepsgraphObjectInstance_persistent_id_get", nullptr, nullptr);

  prop = RNA_def_property(srna, "random_id", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Instance Random ID", "Random id for this instance, typically for randomized shading");
  RNA_def_property_int_funcs(prop, "rna_DepsgraphObjectInstance_random_id_get", nullptr, nullptr);

  prop = RNA_def_property(srna, "matrix_world", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(prop, "Generated Matrix", "Generated transform matrix in world space");
  RNA_def_property_float_funcs(
      prop, "rna_DepsgraphObjectInstance_matrix_world_get", nullptr, nullptr);

  prop = RNA_def_property(srna, "orco", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Generated Coordinates", "Generated coordinates in parent object space");
  RNA_def_property_float_funcs(prop, "rna_DepsgraphObjectInstance_orco_get", nullptr, nullptr);

  prop = RNA_def_property(srna, "uv", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "UV Coordinates", "UV coordinates in parent object space");
  RNA_def_property_array(prop, 2);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_DepsgraphObjectInstance_uv_get", nullptr, nullptr);
}

static void rna_def_depsgraph_update(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "DepsgraphUpdate", nullptr);
  RNA_def_struct_ui_text(srna, "Dependency Graph Update", "Information about ID that was updated");

  prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ID");
  RNA_def_property_ui_text(prop, "ID", "Updated data-block");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, "rna_DepsgraphUpdate_id_get", nullptr, nullptr, nullptr);

  /* Use term 'is_updated' instead of 'is_dirty' here because this is a signal
   * that users of the depsgraph may want to update their data (render engines for eg). */

  prop = RNA_def_property(srna, "is_updated_transform", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Transform", "Object transformation is updated");
  RNA_def_property_boolean_funcs(prop, "rna_DepsgraphUpdate_is_updated_transform_get", nullptr);

  prop = RNA_def_property(srna, "is_updated_geometry", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Geometry", "Object geometry is updated");
  RNA_def_property_boolean_funcs(prop, "rna_DepsgraphUpdate_is_updated_geometry_get", nullptr);

  prop = RNA_def_property(srna, "is_updated_shading", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Shading", "Object shading is updated");
  RNA_def_property_boolean_funcs(prop, "rna_DepsgraphUpdate_is_updated_shading_get", nullptr);
}

static void rna_def_depsgraph(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;
  PropertyRNA *prop;

  static const EnumPropertyItem enum_depsgraph_mode_items[] = {
      {DAG_EVAL_VIEWPORT, "VIEWPORT", 0, "Viewport", "Viewport non-rendered mode"},
      {DAG_EVAL_RENDER, "RENDER", 0, "Render", "Render"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "Depsgraph", nullptr);
  RNA_def_struct_ui_text(srna, "Dependency Graph", "");

  prop = RNA_def_enum(srna, "mode", enum_depsgraph_mode_items, 0, "Mode", "Evaluation mode");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_funcs(prop, "rna_Depsgraph_mode_get", nullptr, nullptr);

  /* Debug helpers. */

  func = RNA_def_function(
      srna, "debug_relations_graphviz", "rna_Depsgraph_debug_relations_graphviz");
  parm = RNA_def_string_file_path(func,
                                  "filepath",
                                  nullptr,
                                  FILE_MAX,
                                  "File Name",
                                  "Optional output path for the graphviz debug file");
  parm = RNA_def_string(func, "dot_graph", nullptr, INT32_MAX, "Dot Graph", "Graph in dot format");
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, ParameterFlag(0));
  RNA_def_parameter_clear_flags(parm, PROP_NEVER_NULL, ParameterFlag(0));
  RNA_def_function_output(func, parm);

  func = RNA_def_function(srna, "debug_stats_gnuplot", "rna_Depsgraph_debug_stats_gnuplot");
  parm = RNA_def_string_file_path(
      func, "filepath", nullptr, FILE_MAX, "File Name", "Output path for the gnuplot debug file");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string_file_path(func,
                                  "output_filepath",
                                  nullptr,
                                  FILE_MAX,
                                  "Output File Name",
                                  "File name where gnuplot script will save the result");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "debug_tag_update", "rna_Depsgraph_debug_tag_update");

  func = RNA_def_function(srna, "debug_stats", "rna_Depsgraph_debug_stats");
  RNA_def_function_ui_description(func, "Report the number of elements in the Dependency Graph");
  /* weak!, no way to return dynamic string type */
  parm = RNA_def_string(func, "result", nullptr, STATS_MAX_SIZE, "result", "");
  RNA_def_parameter_flags(
      parm, PROP_THICK_WRAP, ParameterFlag(0)); /* needed for string return value */
  RNA_def_function_output(func, parm);

  /* Updates. */

  func = RNA_def_function(srna, "update", "rna_Depsgraph_update");
  RNA_def_function_ui_description(
      func,
      "Re-evaluate any modified data-blocks, for example for animation or modifiers. "
      "This invalidates all references to evaluated data-blocks from this dependency graph.");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);

  /* Queries for original data-blocks (the ones depsgraph is built for). */

  prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Scene");
  RNA_def_property_pointer_funcs(prop, "rna_Depsgraph_scene_get", nullptr, nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Scene", "Original scene dependency graph is built for");

  prop = RNA_def_property(srna, "view_layer", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ViewLayer");
  RNA_def_property_pointer_funcs(prop, "rna_Depsgraph_view_layer_get", nullptr, nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "View Layer", "Original view layer dependency graph is built for");

  /* Queries for evaluated data-blocks (the ones depsgraph is evaluating). */

  func = RNA_def_function(srna, "id_eval_get", "rna_Depsgraph_id_eval_get");
  parm = RNA_def_pointer(
      func, "id", "ID", "", "Original ID to get evaluated complementary part for");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "id_eval", "ID", "", "Evaluated ID for the given original one");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "id_type_updated", "rna_Depsgraph_id_type_updated");
  parm = RNA_def_enum(func, "id_type", rna_enum_id_type_items, 0, "ID Type", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_boolean(func,
                         "updated",
                         false,
                         "Updated",
                         "True if any data-block with this type was added, updated or removed");
  RNA_def_function_return(func, parm);

  prop = RNA_def_property(srna, "scene_eval", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Scene");
  RNA_def_property_pointer_funcs(prop, "rna_Depsgraph_scene_eval_get", nullptr, nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Scene", "Scene at its evaluated state");

  prop = RNA_def_property(srna, "view_layer_eval", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ViewLayer");
  RNA_def_property_pointer_funcs(
      prop, "rna_Depsgraph_view_layer_eval_get", nullptr, nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "View Layer", "View layer at its evaluated state");

  /* Iterators. */

  prop = RNA_def_property(srna, "ids", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "ID");
  RNA_def_property_flag_hide_from_ui_workaround(prop);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Depsgraph_ids_begin",
                                    "rna_Depsgraph_ids_next",
                                    "rna_Depsgraph_ids_end",
                                    "rna_Depsgraph_ids_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "IDs", "All evaluated data-blocks");

  prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_flag_hide_from_ui_workaround(prop);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Depsgraph_objects_begin",
                                    "rna_Depsgraph_objects_next",
                                    "rna_Depsgraph_objects_end",
                                    "rna_Depsgraph_objects_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Objects", "Evaluated objects in the dependency graph");

  prop = RNA_def_property(srna, "object_instances", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "DepsgraphObjectInstance");
  RNA_def_property_flag_hide_from_ui_workaround(prop);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Depsgraph_object_instances_begin",
                                    "rna_Depsgraph_object_instances_next",
                                    "rna_Depsgraph_object_instances_end",
                                    "rna_Depsgraph_object_instances_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop,
                           "Object Instances",
                           "All object instances to display or render "
                           "(Warning: Only use this as an iterator, never as a sequence, "
                           "and do not keep any references to its items)");

  prop = RNA_def_property(srna, "updates", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "DepsgraphUpdate");
  RNA_def_property_flag_hide_from_ui_workaround(prop);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Depsgraph_updates_begin",
                                    "rna_Depsgraph_ids_next",
                                    "rna_Depsgraph_ids_end",
                                    "rna_Depsgraph_updates_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Updates", "Updates to data-blocks");
}

void RNA_def_depsgraph(BlenderRNA *brna)
{
  rna_def_depsgraph_instance(brna);
  rna_def_depsgraph_update(brna);
  rna_def_depsgraph(brna);
}

#endif
