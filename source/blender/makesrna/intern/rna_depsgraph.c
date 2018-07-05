/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Blender Foundation (2014).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_depsgraph.c
 *  \ingroup RNA
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

#include "BLI_iterator.h"

#include "BKE_anim.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_debug.h"
#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

/* **************** Object Instance **************** */

static PointerRNA rna_DepsgraphObjectInstance_object_get(PointerRNA *ptr)
{
	BLI_Iterator *iterator = ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_Object, iterator->current);
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
	return rna_pointer_inherit_refine(ptr, &RNA_ParticleSystem,
		deg_iter->dupli_object_current->particle_system);
}

static void rna_DepsgraphObjectInstance_persistent_id_get(PointerRNA *ptr, int *persistent_id)
{
	BLI_Iterator *iterator = ptr->data;
	DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
	memcpy(persistent_id, deg_iter->dupli_object_current->persistent_id,
	       sizeof(deg_iter->dupli_object_current->persistent_id));
}

static void rna_DepsgraphObjectInstance_orco_get(PointerRNA *ptr, float *orco)
{
	BLI_Iterator *iterator = ptr->data;
	DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
	memcpy(orco, deg_iter->dupli_object_current->orco,
	       sizeof(deg_iter->dupli_object_current->orco));
}

static unsigned int rna_DepsgraphObjectInstance_random_id_get(PointerRNA *ptr)
{
	BLI_Iterator *iterator = ptr->data;
	DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
	return deg_iter->dupli_object_current->random_id;
}

static void rna_DepsgraphObjectInstance_uv_get(PointerRNA *ptr, float *uv)
{
	BLI_Iterator *iterator = ptr->data;
	DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
	memcpy(uv, deg_iter->dupli_object_current->uv,
	       sizeof(deg_iter->dupli_object_current->uv));
}

static int rna_DepsgraphObjectInstance_is_instance_get(PointerRNA *ptr)
{
	BLI_Iterator *iterator = ptr->data;
	DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
	return (deg_iter->dupli_object_current != NULL);
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

static bool rna_DepsgraphUpdate_is_dirty_transform_get(PointerRNA *ptr)
{
	ID *id = ptr->data;
	return ((id->recalc & ID_RECALC_TRANSFORM) == 0);
}

static bool rna_DepsgraphUpdate_is_dirty_geometry_get(PointerRNA *ptr)
{
	ID *id = ptr->data;
	if (id->recalc & ID_RECALC_GEOMETRY) {
		return false;
	}
	if (GS(id->name) != ID_OB) {
		return true;
	}
	Object *object = (Object *)id;
	ID *data = object->data;
	if (data == NULL) {
		return true;
	}
	return ((data->recalc & ID_RECALC_ALL) == 0);
}

/* **************** Depsgraph **************** */

static void rna_Depsgraph_debug_relations_graphviz(Depsgraph *depsgraph,
                                                   const char *filename)
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
	BLI_snprintf(result, STATS_MAX_SIZE,
	            "Approx %lu Operations, %lu Relations, %lu Outer Nodes",
	             ops, rels, outer);
}

/* Iteration over objects, simple version */

static void rna_Depsgraph_objects_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	iter->internal.custom = MEM_callocN(sizeof(BLI_Iterator), __func__);
	DEGObjectIterData *data = MEM_callocN(sizeof(DEGObjectIterData), __func__);

	data->graph = (Depsgraph *)ptr->data;
	data->flag = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
	             DEG_ITER_OBJECT_FLAG_VISIBLE |
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

static void rna_Depsgraph_object_instances_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	iter->internal.custom = MEM_callocN(sizeof(BLI_Iterator), __func__);
	DEGObjectIterData *data = MEM_callocN(sizeof(DEGObjectIterData), __func__);

	data->graph = (Depsgraph *)ptr->data;
	data->flag = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
	             DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET |
	             DEG_ITER_OBJECT_FLAG_VISIBLE |
	             DEG_ITER_OBJECT_FLAG_DUPLI;

	((BLI_Iterator *)iter->internal.custom)->valid = true;
	DEG_iterator_objects_begin(iter->internal.custom, data);
	iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Depsgraph_object_instances_next(CollectionPropertyIterator *iter)
{
	DEG_iterator_objects_next(iter->internal.custom);
	iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Depsgraph_object_instances_end(CollectionPropertyIterator *iter)
{
	DEG_iterator_objects_end(iter->internal.custom);
	MEM_freeN(((BLI_Iterator *)iter->internal.custom)->data);
	MEM_freeN(iter->internal.custom);
}

static PointerRNA rna_Depsgraph_object_instances_get(CollectionPropertyIterator *iter)
{
	BLI_Iterator *iterator = (BLI_Iterator *)iter->internal.custom;
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
	RNA_def_struct_ui_text(srna, "Dependency Graph Object Instance",
	                       "Extended information about dependency graph object iterator");

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_ui_text(prop, "Object", "Object the iterator points to");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_DepsgraphObjectInstance_object_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "instance_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_ui_text(prop, "Instance Object", "Object which is being instanced by this iterator");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_DepsgraphObjectInstance_instance_object_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_ui_text(prop, "Parent", "Parent of the duplication list");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_DepsgraphObjectInstance_parent_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "particle_system", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ParticleSystem");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Particle System", "Particle system that this object was instanced from");
	RNA_def_property_pointer_funcs(prop, "rna_DepsgraphObjectInstance_particle_system_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "persistent_id", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Persistent ID",
	                         "Persistent identifier for inter-frame matching of objects with motion blur");
	RNA_def_property_array(prop, 2 * MAX_DUPLI_RECUR);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_DepsgraphObjectInstance_persistent_id_get", NULL, NULL);

	prop = RNA_def_property(srna, "orco", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	/* Seems system is not smart enough to figure that getter function should return
	 * array for PROP_TRANSLATION.
	 */
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Generated Coordinates", "Generated coordinates in parent object space");
	RNA_def_property_float_funcs(prop, "rna_DepsgraphObjectInstance_orco_get", NULL, NULL);

	prop = RNA_def_property(srna, "random_id", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Dupli random id", "Random id for this dupli object");
	RNA_def_property_int_funcs(prop, "rna_DepsgraphObjectInstance_random_id_get", NULL, NULL);

	prop = RNA_def_property(srna, "uv", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "UV Coordinates", "UV coordinates in parent object space");
	RNA_def_property_array(prop, 2);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_DepsgraphObjectInstance_uv_get", NULL, NULL);

	prop = RNA_def_property(srna, "is_instance", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Is Instance", "Denotes whether the object is coming from dupli-list");
	RNA_def_property_boolean_funcs(prop, "rna_DepsgraphObjectInstance_is_instance_get", NULL);
}

static void rna_def_depsgraph_update(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "DepsgraphUpdate", NULL);
	RNA_def_struct_ui_text(srna, "Dependency Graph Update",
	                       "Information about ID that was updated");

	prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_ui_text(prop, "ID", "Updated datablock");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_DepsgraphUpdate_id_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "is_dirty_transform", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Transform", "Object transformation is not updated");
	RNA_def_property_boolean_funcs(prop, "rna_DepsgraphUpdate_is_dirty_transform_get", NULL);

	prop = RNA_def_property(srna, "is_dirty_geometry", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Geometry", "Object geometry is not updated");
	RNA_def_property_boolean_funcs(prop, "rna_DepsgraphUpdate_is_dirty_geometry_get", NULL);
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
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "Depsgraph", NULL);
	RNA_def_struct_ui_text(srna, "Dependency Graph", "");

	prop = RNA_def_enum(srna, "mode", enum_depsgraph_mode_items, 0, "Mode", "Evaluation mode");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_funcs(prop, "rna_Depsgraph_mode_get", NULL, NULL);

	/* Debug helpers. */

	func = RNA_def_function(srna, "debug_relations_graphviz", "rna_Depsgraph_debug_relations_graphviz");
	parm = RNA_def_string_file_path(func, "filename", NULL, FILE_MAX, "File Name",
	                                "File in which to store graphviz debug output");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	func = RNA_def_function(srna, "debug_stats_gnuplot", "rna_Depsgraph_debug_stats_gnuplot");
	parm = RNA_def_string_file_path(func, "filename", NULL, FILE_MAX, "File Name",
	                                "File in which to store graphviz debug output");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_string_file_path(func, "output_filename", NULL, FILE_MAX, "Output File Name",
	                                "File name where gnuplot script will save the result");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

	func = RNA_def_function(srna, "debug_tag_update", "rna_Depsgraph_debug_tag_update");

	func = RNA_def_function(srna, "debug_stats", "rna_Depsgraph_debug_stats");
	RNA_def_function_ui_description(func, "Report the number of elements in the Dependency Graph");
	/* weak!, no way to return dynamic string type */
	parm = RNA_def_string(func, "result", NULL, STATS_MAX_SIZE, "result", "");
	RNA_def_parameter_flags(parm, PROP_THICK_WRAP, 0); /* needed for string return value */
	RNA_def_function_output(func, parm);

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
	RNA_def_property_ui_text(prop, "View Layer", "Original view layer dependency graph is built for");

	/* Queries for evaluated datablockls (the ones depsgraph is evaluating). */

	func = RNA_def_function(srna, "id_eval_get", "rna_Depsgraph_id_eval_get");
	parm = RNA_def_pointer(func, "id", "ID", "", "Original ID to get evaluated complementary part for");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "id_eval", "ID", "", "Evaluated ID for the given original one");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "id_type_updated", "rna_Depsgraph_id_type_updated");
	parm = RNA_def_enum(func, "id_type", rna_enum_id_type_items, 0, "ID Type", "");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_boolean(func, "updated", false, "Updated", "True if any datablock with this type was added, updated or removed");
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
	RNA_def_property_ui_text(prop, "View Layer", "Original view layer dependency graph is built for");

	/* Iterators. */

	prop = RNA_def_property(srna, "ids", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_collection_funcs(prop,
	                                  "rna_Depsgraph_ids_begin",
	                                  "rna_Depsgraph_ids_next",
	                                  "rna_Depsgraph_ids_end",
	                                  "rna_Depsgraph_ids_get",
	                                  NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "IDs", "All evaluated datablocks");

	prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_collection_funcs(prop,
	                                  "rna_Depsgraph_objects_begin",
	                                  "rna_Depsgraph_objects_next",
	                                  "rna_Depsgraph_objects_end",
	                                  "rna_Depsgraph_objects_get",
	                                  NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Objects", "Evaluated objects in the dependency graph");

	prop = RNA_def_property(srna, "object_instances", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "DepsgraphObjectInstance");
	RNA_def_property_collection_funcs(prop,
	                                  "rna_Depsgraph_object_instances_begin",
	                                  "rna_Depsgraph_object_instances_next",
	                                  "rna_Depsgraph_object_instances_end",
	                                  "rna_Depsgraph_object_instances_get",
	                                  NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Object Instances", "All object instances to display or render");

	prop = RNA_def_property(srna, "updates", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "DepsgraphUpdate");
	RNA_def_property_collection_funcs(prop,
	                                  "rna_Depsgraph_updates_begin",
	                                  "rna_Depsgraph_ids_next",
	                                  "rna_Depsgraph_ids_end",
	                                  "rna_Depsgraph_updates_get",
	                                  NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Updates", "Updates to datablocks");
}

void RNA_def_depsgraph(BlenderRNA *brna)
{
	rna_def_depsgraph_instance(brna);
	rna_def_depsgraph_update(brna);
	rna_def_depsgraph(brna);
}

#endif
