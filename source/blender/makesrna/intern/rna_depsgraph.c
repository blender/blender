/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup RNA
 */

#include <stdlib.h>

#include "BLI_utildefines.h"
#include "BLI_path_util.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_object_types.h"

#include "DEG_depsgraph.h"

#define STATS_MAX_SIZE 16384

#ifdef RNA_RUNTIME

#  ifdef WITH_PYTHON
#    include "BPY_extern.h"
#  endif

#  include "BLI_iterator.h"
#  include "BLI_math.h"

#  include "BKE_anim.h"
#  include "BKE_object.h"
#  include "BKE_scene.h"

#  include "DEG_depsgraph_build.h"
#  include "DEG_depsgraph_debug.h"
#  include "DEG_depsgraph_query.h"

#  include "MEM_guardedalloc.h"

/* **************** Object Instance **************** */

static PointerRNA rna_DepsgraphObjectInstance_object_get(PointerRNA *ptr)
{
  BLI_Iterator *iterator = ptr->data;
  return rna_pointer_inherit_refine(ptr, &RNA_Object, iterator->current);
}

static int rna_DepsgraphObjectInstance_is_instance_get(PointerRNA *ptr)
{
  BLI_Iterator *iterator = ptr->data;
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
  return (deg_iter->dupli_object_current != NULL);
}

static PointerRNA rna_DepsgraphObjectInstance_instance_object_get(PointerRNA *ptr)
{
  BLI_Iterator *iterator = ptr->data;
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
  Object *instance_object = NULL;
  if (deg_iter->dupli_object_current != NULL) {
    instance_object = deg_iter->dupli_object_current->ob;
  }
  return rna_pointer_inherit_refine(ptr, &RNA_Object, instance_object);
}

static bool rna_DepsgraphObjectInstance_show_self_get(PointerRNA *ptr)
{
  BLI_Iterator *iterator = ptr->data;
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
  int ob_visibility = BKE_object_visibility(iterator->current, deg_iter->eval_mode);
  return (ob_visibility & OB_VISIBLE_SELF) != 0;
}

static bool rna_DepsgraphObjectInstance_show_particles_get(PointerRNA *ptr)
{
  BLI_Iterator *iterator = ptr->data;
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
  int ob_visibility = BKE_object_visibility(iterator->current, deg_iter->eval_mode);
  return (ob_visibility & OB_VISIBLE_PARTICLES) != 0;
}

static PointerRNA rna_DepsgraphObjectInstance_parent_get(PointerRNA *ptr)
{
  BLI_Iterator *iterator = ptr->data;
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
  Object *dupli_parent = NULL;
  if (deg_iter->dupli_object_current != NULL) {
    dupli_parent = deg_iter->dupli_parent;
  }
  return rna_pointer_inherit_refine(ptr, &RNA_Object, dupli_parent);
}

static PointerRNA rna_DepsgraphObjectInstance_particle_system_get(PointerRNA *ptr)
{
  BLI_Iterator *iterator = ptr->data;
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
  struct ParticleSystem *particle_system = NULL;
  if (deg_iter->dupli_object_current != NULL) {
    particle_system = deg_iter->dupli_object_current->particle_system;
  }
  return rna_pointer_inherit_refine(ptr, &RNA_ParticleSystem, particle_system);
}

static void rna_DepsgraphObjectInstance_persistent_id_get(PointerRNA *ptr, int *persistent_id)
{
  BLI_Iterator *iterator = ptr->data;
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
  if (deg_iter->dupli_object_current != NULL) {
    memcpy(persistent_id,
           deg_iter->dupli_object_current->persistent_id,
           sizeof(deg_iter->dupli_object_current->persistent_id));
  }
  else {
    memset(persistent_id, 0, sizeof(deg_iter->dupli_object_current->persistent_id));
  }
}

static unsigned int rna_DepsgraphObjectInstance_random_id_get(PointerRNA *ptr)
{
  BLI_Iterator *iterator = ptr->data;
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
  if (deg_iter->dupli_object_current != NULL) {
    return deg_iter->dupli_object_current->random_id;
  }
  else {
    return 0;
  }
}

static void rna_DepsgraphObjectInstance_matrix_world_get(PointerRNA *ptr, float *mat)
{
  BLI_Iterator *iterator = ptr->data;
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
  if (deg_iter->dupli_object_current != NULL) {
    copy_m4_m4((float(*)[4])mat, deg_iter->dupli_object_current->mat);
  }
  else {
    /* We can return actual object's matrix here, no reason to return identity matrix
     * when this is not actually an instance... */
    Object *ob = (Object *)iterator->current;
    copy_m4_m4((float(*)[4])mat, ob->obmat);
  }
}

static void rna_DepsgraphObjectInstance_orco_get(PointerRNA *ptr, float *orco)
{
  BLI_Iterator *iterator = ptr->data;
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
  if (deg_iter->dupli_object_current != NULL) {
    copy_v3_v3(orco, deg_iter->dupli_object_current->orco);
  }
  else {
    zero_v3(orco);
  }
}

static void rna_DepsgraphObjectInstance_uv_get(PointerRNA *ptr, float *uv)
{
  BLI_Iterator *iterator = ptr->data;
  DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
  if (deg_iter->dupli_object_current != NULL) {
    copy_v2_v2(uv, deg_iter->dupli_object_current->uv);
  }
  else {
    zero_v2(uv);
  }
}

/* ******************** Sorted  ***************** */

static int rna_Depsgraph_mode_get(PointerRNA *ptr)
{
  Depsgraph *depsgraph = ptr->data;
  return DEG_get_mode(depsgraph);
}

/* ******************** Updates ***************** */

static PointerRNA rna_DepsgraphUpdate_id_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_ID, ptr->data);
}

static bool rna_DepsgraphUpdate_is_updated_transform_get(PointerRNA *ptr)
{
  ID *id = ptr->data;
  return ((id->recalc & ID_RECALC_TRANSFORM) != 0);
}

static bool rna_DepsgraphUpdate_is_updated_geometry_get(PointerRNA *ptr)
{
  ID *id = ptr->data;
  if (id->recalc & ID_RECALC_GEOMETRY) {
    return true;
  }
  if (GS(id->name) != ID_OB) {
    return false;
  }
  Object *object = (Object *)id;
  ID *data = object->data;
  if (data == NULL) {
    return false;
  }
  return ((data->recalc & ID_RECALC_ALL) != 0);
}

/* **************** Depsgraph **************** */

static void rna_Depsgraph_debug_relations_graphviz(Depsgraph *depsgraph, const char *filename)
{
  FILE *f = fopen(filename, "w");
  if (f == NULL) {
    return;
  }
  DEG_debug_relations_graphviz(depsgraph, f, "Depsgraph");
  fclose(f);
}

static void rna_Depsgraph_debug_stats_gnuplot(Depsgraph *depsgraph,
                                              const char *filename,
                                              const char *output_filename)
{
  FILE *f = fopen(filename, "w");
  if (f == NULL) {
    return;
  }
  DEG_debug_stats_gnuplot(depsgraph, f, "Timing Statistics", output_filename);
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
               "Approx %lu Operations, %lu Relations, %lu Outer Nodes",
               ops,
               rels,
               outer);
}

static void rna_Depsgraph_update(Depsgraph *depsgraph, Main *bmain)
{
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
  iter->internal.custom = MEM_callocN(sizeof(BLI_Iterator), __func__);
  DEGObjectIterData *data = MEM_callocN(sizeof(DEGObjectIterData), __func__);

  data->graph = (Depsgraph *)ptr->data;
  data->flag = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY | DEG_ITER_OBJECT_FLAG_VISIBLE |
               DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET;

  ((BLI_Iterator *)iter->internal.custom)->valid = true;
  DEG_iterator_objects_begin(iter->internal.custom, data);
  iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Depsgraph_objects_next(CollectionPropertyIterator *iter)
{
  DEG_iterator_objects_next(iter->internal.custom);
  iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Depsgraph_objects_end(CollectionPropertyIterator *iter)
{
  DEG_iterator_objects_end(iter->internal.custom);
  MEM_freeN(((BLI_Iterator *)iter->internal.custom)->data);
  MEM_freeN(iter->internal.custom);
}

static PointerRNA rna_Depsgraph_objects_get(CollectionPropertyIterator *iter)
{
  Object *ob = ((BLI_Iterator *)iter->internal.custom)->current;
  return rna_pointer_inherit_refine(&iter->parent, &RNA_Object, ob);
}

/* Iteration over objects, extended version
 *
 * Contains extra information about duplicator and persistent ID.
 */

/* XXX Ugly python seems to query next item of an iterator before using current one (see T57558).
 * This forces us to use that nasty ping-pong game between two sets of iterator data,
 * so that previous one remains valid memory for python to access to. Yuck.
 */
typedef struct RNA_Depsgraph_Instances_Iterator {
  BLI_Iterator iterators[2];
  DEGObjectIterData deg_data[2];
  DupliObject dupli_object_current[2];
  int counter;
} RNA_Depsgraph_Instances_Iterator;

static void rna_Depsgraph_object_instances_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  RNA_Depsgraph_Instances_Iterator *di_it = iter->internal.custom = MEM_callocN(sizeof(*di_it),
                                                                                __func__);

  DEGObjectIterData *data = &di_it->deg_data[0];
  data->graph = (Depsgraph *)ptr->data;
  data->flag = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY | DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET |
               DEG_ITER_OBJECT_FLAG_VISIBLE | DEG_ITER_OBJECT_FLAG_DUPLI;

  di_it->iterators[0].valid = true;
  DEG_iterator_objects_begin(&di_it->iterators[0], data);
  iter->valid = di_it->iterators[0].valid;
}

static void rna_Depsgraph_object_instances_next(CollectionPropertyIterator *iter)
{
  RNA_Depsgraph_Instances_Iterator *di_it = (RNA_Depsgraph_Instances_Iterator *)
                                                iter->internal.custom;

  /* We need to copy current iterator status to next one beeing worked on. */
  di_it->iterators[(di_it->counter + 1) % 2] = di_it->iterators[di_it->counter % 2];
  di_it->deg_data[(di_it->counter + 1) % 2] = di_it->deg_data[di_it->counter % 2];
  di_it->counter++;

  di_it->iterators[di_it->counter % 2].data = &di_it->deg_data[di_it->counter % 2];
  DEG_iterator_objects_next(&di_it->iterators[di_it->counter % 2]);
  /* Dupli_object_current is also temp memory generated during the iterations,
   * it may be freed when last item has been iterated,
   * so we have same issue as with the iterator itself:
   * we need to keep a local copy, which memory remains valid a bit longer,
   * for Python accesses to work. */
  if (di_it->deg_data[di_it->counter % 2].dupli_object_current != NULL) {
    di_it->dupli_object_current[di_it->counter % 2] =
        *di_it->deg_data[di_it->counter % 2].dupli_object_current;
    di_it->deg_data[di_it->counter % 2].dupli_object_current =
        &di_it->dupli_object_current[di_it->counter % 2];
  }
  iter->valid = di_it->iterators[di_it->counter % 2].valid;
}

static void rna_Depsgraph_object_instances_end(CollectionPropertyIterator *iter)
{
  RNA_Depsgraph_Instances_Iterator *di_it = (RNA_Depsgraph_Instances_Iterator *)
                                                iter->internal.custom;
  DEG_iterator_objects_end(&di_it->iterators[0]);
  DEG_iterator_objects_end(&di_it->iterators[1]);
  MEM_freeN(di_it);
}

static PointerRNA rna_Depsgraph_object_instances_get(CollectionPropertyIterator *iter)
{
  RNA_Depsgraph_Instances_Iterator *di_it = (RNA_Depsgraph_Instances_Iterator *)
                                                iter->internal.custom;
  BLI_Iterator *iterator = &di_it->iterators[di_it->counter % 2];
  return rna_pointer_inherit_refine(&iter->parent, &RNA_DepsgraphObjectInstance, iterator);
}

/* Iteration over evaluated IDs */

static void rna_Depsgraph_ids_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  iter->internal.custom = MEM_callocN(sizeof(BLI_Iterator), __func__);
  DEGIDIterData *data = MEM_callocN(sizeof(DEGIDIterData), __func__);

  data->graph = (Depsgraph *)ptr->data;

  ((BLI_Iterator *)iter->internal.custom)->valid = true;
  DEG_iterator_ids_begin(iter->internal.custom, data);
  iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Depsgraph_ids_next(CollectionPropertyIterator *iter)
{
  DEG_iterator_ids_next(iter->internal.custom);
  iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Depsgraph_ids_end(CollectionPropertyIterator *iter)
{
  DEG_iterator_ids_end(iter->internal.custom);
  MEM_freeN(((BLI_Iterator *)iter->internal.custom)->data);
  MEM_freeN(iter->internal.custom);
}

static PointerRNA rna_Depsgraph_ids_get(CollectionPropertyIterator *iter)
{
  ID *id = ((BLI_Iterator *)iter->internal.custom)->current;
  return rna_pointer_inherit_refine(&iter->parent, &RNA_ID, id);
}

static void rna_Depsgraph_updates_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  iter->internal.custom = MEM_callocN(sizeof(BLI_Iterator), __func__);
  DEGIDIterData *data = MEM_callocN(sizeof(DEGIDIterData), __func__);

  data->graph = (Depsgraph *)ptr->data;
  data->only_updated = true;

  ((BLI_Iterator *)iter->internal.custom)->valid = true;
  DEG_iterator_ids_begin(iter->internal.custom, data);
  iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static PointerRNA rna_Depsgraph_updates_get(CollectionPropertyIterator *iter)
{
  ID *id = ((BLI_Iterator *)iter->internal.custom)->current;
  return rna_pointer_inherit_refine(&iter->parent, &RNA_DepsgraphUpdate, id);
}

static ID *rna_Depsgraph_id_eval_get(Depsgraph *depsgraph, ID *id_orig)
{
  return DEG_get_evaluated_id(depsgraph, id_orig);
}

static bool rna_Depsgraph_id_type_updated(Depsgraph *depsgraph, int id_type)
{
  return DEG_id_type_updated(depsgraph, id_type);
}

static PointerRNA rna_Depsgraph_scene_get(PointerRNA *ptr)
{
  Depsgraph *depsgraph = (Depsgraph *)ptr->data;
  Scene *scene = DEG_get_input_scene(depsgraph);
  return rna_pointer_inherit_refine(ptr, &RNA_Scene, scene);
}

static PointerRNA rna_Depsgraph_view_layer_get(PointerRNA *ptr)
{
  Depsgraph *depsgraph = (Depsgraph *)ptr->data;
  ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);
  return rna_pointer_inherit_refine(ptr, &RNA_ViewLayer, view_layer);
}

static PointerRNA rna_Depsgraph_scene_eval_get(PointerRNA *ptr)
{
  Depsgraph *depsgraph = (Depsgraph *)ptr->data;
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  return rna_pointer_inherit_refine(ptr, &RNA_Scene, scene_eval);
}

static PointerRNA rna_Depsgraph_view_layer_eval_get(PointerRNA *ptr)
{
  Depsgraph *depsgraph = (Depsgraph *)ptr->data;
  ViewLayer *view_layer_eval = DEG_get_evaluated_view_layer(depsgraph);
  return rna_pointer_inherit_refine(ptr, &RNA_ViewLayer, view_layer_eval);
}

#else

static void rna_def_depsgraph_instance(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "DepsgraphObjectInstance", NULL);
  RNA_def_struct_ui_text(
      srna,
      "Dependency Graph Object Instance",
      "Extended information about dependency graph object iterator "
      "(WARNING: all data here is *evaluated* one, not original .blend IDs...)");

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_ui_text(prop, "Object", "Evaluated object the iterator points to");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, "rna_DepsgraphObjectInstance_object_get", NULL, NULL, NULL);

  prop = RNA_def_property(srna, "show_self", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Show Self", "The object geometry itself should be visible in the render");
  RNA_def_property_boolean_funcs(prop, "rna_DepsgraphObjectInstance_show_self_get", NULL);

  prop = RNA_def_property(srna, "show_particles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Show Particles", "Particles part of the object should be visible in the render");
  RNA_def_property_boolean_funcs(prop, "rna_DepsgraphObjectInstance_show_particles_get", NULL);

  prop = RNA_def_property(srna, "is_instance", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Is Instance", "Denotes if the object is generated by another object");
  RNA_def_property_boolean_funcs(prop, "rna_DepsgraphObjectInstance_is_instance_get", NULL);

  prop = RNA_def_property(srna, "instance_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_ui_text(
      prop, "Instance Object", "Evaluated object which is being instanced by this iterator");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_pointer_funcs(
      prop, "rna_DepsgraphObjectInstance_instance_object_get", NULL, NULL, NULL);

  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_ui_text(
      prop, "Parent", "If the object is an instance, the parent object that generated it");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, "rna_DepsgraphObjectInstance_parent_get", NULL, NULL, NULL);

  prop = RNA_def_property(srna, "particle_system", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ParticleSystem");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Particle System", "Evaluated particle system that this object was instanced from");
  RNA_def_property_pointer_funcs(
      prop, "rna_DepsgraphObjectInstance_particle_system_get", NULL, NULL, NULL);

  prop = RNA_def_property(srna, "persistent_id", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Persistent ID",
      "Persistent identifier for inter-frame matching of objects with motion blur");
  RNA_def_property_array(prop, 2 * MAX_DUPLI_RECUR);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_DepsgraphObjectInstance_persistent_id_get", NULL, NULL);

  prop = RNA_def_property(srna, "random_id", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Dupli random id", "Random id for this instance, typically for randomized shading");
  RNA_def_property_int_funcs(prop, "rna_DepsgraphObjectInstance_random_id_get", NULL, NULL);

  prop = RNA_def_property(srna, "matrix_world", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(prop, "Generated Matrix", "Generated transform matrix in world space");
  RNA_def_property_float_funcs(prop, "rna_DepsgraphObjectInstance_matrix_world_get", NULL, NULL);

  prop = RNA_def_property(srna, "orco", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Generated Coordinates", "Generated coordinates in parent object space");
  RNA_def_property_float_funcs(prop, "rna_DepsgraphObjectInstance_orco_get", NULL, NULL);

  prop = RNA_def_property(srna, "uv", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "UV Coordinates", "UV coordinates in parent object space");
  RNA_def_property_array(prop, 2);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_DepsgraphObjectInstance_uv_get", NULL, NULL);
}

static void rna_def_depsgraph_update(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "DepsgraphUpdate", NULL);
  RNA_def_struct_ui_text(srna, "Dependency Graph Update", "Information about ID that was updated");

  prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ID");
  RNA_def_property_ui_text(prop, "ID", "Updated datablock");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, "rna_DepsgraphUpdate_id_get", NULL, NULL, NULL);

  /* Use term 'is_updated' instead of 'is_dirty' here because this is a signal
   * that users of the depsgraph may want to update their data (render engines for eg). */

  prop = RNA_def_property(srna, "is_updated_transform", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Transform", "Object transformation is updated");
  RNA_def_property_boolean_funcs(prop, "rna_DepsgraphUpdate_is_updated_transform_get", NULL);

  prop = RNA_def_property(srna, "is_updated_geometry", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Geometry", "Object geometry is updated");
  RNA_def_property_boolean_funcs(prop, "rna_DepsgraphUpdate_is_updated_geometry_get", NULL);
}

static void rna_def_depsgraph(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;
  PropertyRNA *prop;

  static EnumPropertyItem enum_depsgraph_mode_items[] = {
      {DAG_EVAL_VIEWPORT, "VIEWPORT", 0, "Viewport", "Viewport non-rendered mode"},
      {DAG_EVAL_RENDER, "RENDER", 0, "Render", "Render"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "Depsgraph", NULL);
  RNA_def_struct_ui_text(srna, "Dependency Graph", "");

  prop = RNA_def_enum(srna, "mode", enum_depsgraph_mode_items, 0, "Mode", "Evaluation mode");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_funcs(prop, "rna_Depsgraph_mode_get", NULL, NULL);

  /* Debug helpers. */

  func = RNA_def_function(
      srna, "debug_relations_graphviz", "rna_Depsgraph_debug_relations_graphviz");
  parm = RNA_def_string_file_path(func,
                                  "filename",
                                  NULL,
                                  FILE_MAX,
                                  "File Name",
                                  "File in which to store graphviz debug output");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  func = RNA_def_function(srna, "debug_stats_gnuplot", "rna_Depsgraph_debug_stats_gnuplot");
  parm = RNA_def_string_file_path(func,
                                  "filename",
                                  NULL,
                                  FILE_MAX,
                                  "File Name",
                                  "File in which to store graphviz debug output");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_string_file_path(func,
                                  "output_filename",
                                  NULL,
                                  FILE_MAX,
                                  "Output File Name",
                                  "File name where gnuplot script will save the result");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  func = RNA_def_function(srna, "debug_tag_update", "rna_Depsgraph_debug_tag_update");

  func = RNA_def_function(srna, "debug_stats", "rna_Depsgraph_debug_stats");
  RNA_def_function_ui_description(func, "Report the number of elements in the Dependency Graph");
  /* weak!, no way to return dynamic string type */
  parm = RNA_def_string(func, "result", NULL, STATS_MAX_SIZE, "result", "");
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, 0); /* needed for string return value */
  RNA_def_function_output(func, parm);

  /* Updates. */

  func = RNA_def_function(srna, "update", "rna_Depsgraph_update");
  RNA_def_function_ui_description(
      func,
      "Re-evaluate any modified data-blocks, for example for animation or modifiers. "
      "This invalidates all references to evaluated data-blocks from this dependency graph.");
  RNA_def_function_flag(func, FUNC_USE_MAIN);

  /* Queries for original datablockls (the ones depsgraph is built for). */

  prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Scene");
  RNA_def_property_pointer_funcs(prop, "rna_Depsgraph_scene_get", NULL, NULL, NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Scene", "Original scene dependency graph is built for");

  prop = RNA_def_property(srna, "view_layer", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ViewLayer");
  RNA_def_property_pointer_funcs(prop, "rna_Depsgraph_view_layer_get", NULL, NULL, NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "View Layer", "Original view layer dependency graph is built for");

  /* Queries for evaluated datablockls (the ones depsgraph is evaluating). */

  func = RNA_def_function(srna, "id_eval_get", "rna_Depsgraph_id_eval_get");
  parm = RNA_def_pointer(
      func, "id", "ID", "", "Original ID to get evaluated complementary part for");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "id_eval", "ID", "", "Evaluated ID for the given original one");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "id_type_updated", "rna_Depsgraph_id_type_updated");
  parm = RNA_def_enum(func, "id_type", rna_enum_id_type_items, 0, "ID Type", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_boolean(func,
                         "updated",
                         false,
                         "Updated",
                         "True if any datablock with this type was added, updated or removed");
  RNA_def_function_return(func, parm);

  prop = RNA_def_property(srna, "scene_eval", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Scene");
  RNA_def_property_pointer_funcs(prop, "rna_Depsgraph_scene_eval_get", NULL, NULL, NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Scene", "Original scene dependency graph is built for");

  prop = RNA_def_property(srna, "view_layer_eval", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ViewLayer");
  RNA_def_property_pointer_funcs(prop, "rna_Depsgraph_view_layer_eval_get", NULL, NULL, NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "View Layer", "Original view layer dependency graph is built for");

  /* Iterators. */

  prop = RNA_def_property(srna, "ids", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "ID");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Depsgraph_ids_begin",
                                    "rna_Depsgraph_ids_next",
                                    "rna_Depsgraph_ids_end",
                                    "rna_Depsgraph_ids_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_ui_text(prop, "IDs", "All evaluated datablocks");

  prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Depsgraph_objects_begin",
                                    "rna_Depsgraph_objects_next",
                                    "rna_Depsgraph_objects_end",
                                    "rna_Depsgraph_objects_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_ui_text(prop, "Objects", "Evaluated objects in the dependency graph");

  prop = RNA_def_property(srna, "object_instances", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "DepsgraphObjectInstance");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Depsgraph_object_instances_begin",
                                    "rna_Depsgraph_object_instances_next",
                                    "rna_Depsgraph_object_instances_end",
                                    "rna_Depsgraph_object_instances_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_ui_text(prop,
                           "Object Instances",
                           "All object instances to display or render "
                           "(WARNING: only use this as an iterator, never as a sequence, "
                           "and do not keep any references to its items)");

  prop = RNA_def_property(srna, "updates", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "DepsgraphUpdate");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Depsgraph_updates_begin",
                                    "rna_Depsgraph_ids_next",
                                    "rna_Depsgraph_ids_end",
                                    "rna_Depsgraph_updates_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_ui_text(prop, "Updates", "Updates to datablocks");
}

void RNA_def_depsgraph(BlenderRNA *brna)
{
  rna_def_depsgraph_instance(brna);
  rna_def_depsgraph_update(brna);
  rna_def_depsgraph(brna);
}

#endif
