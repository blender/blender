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

#include "DEG_depsgraph.h"

#include "DNA_object_types.h"

#define STATS_MAX_SIZE 16384

#ifdef RNA_RUNTIME

#include "BLI_iterator.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_debug.h"
#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

/* **************** Depsgraph **************** */

static PointerRNA rna_DepsgraphIter_object_get(PointerRNA *ptr)
{
	BLI_Iterator *iterator = ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_Object, iterator->current);
}

static PointerRNA rna_DepsgraphIter_instance_object_get(PointerRNA *ptr)
{
	BLI_Iterator *iterator = ptr->data;
	DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
	Object *instance_object = NULL;
	if (deg_iter->dupli_object_current != NULL) {
		instance_object = deg_iter->dupli_object_current->ob;
	}
	return rna_pointer_inherit_refine(ptr, &RNA_Object, instance_object);
}

static PointerRNA rna_DepsgraphIter_parent_get(PointerRNA *ptr)
{
	BLI_Iterator *iterator = ptr->data;
	DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
	Object *dupli_parent = NULL;
	if (deg_iter->dupli_object_current != NULL) {
		dupli_parent = deg_iter->dupli_parent;
	}
	return rna_pointer_inherit_refine(ptr, &RNA_Object, dupli_parent);
}

static void rna_DepsgraphIter_persistent_id_get(PointerRNA *ptr, int *persistent_id)
{
	BLI_Iterator *iterator = ptr->data;
	DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
	memcpy(persistent_id, deg_iter->dupli_object_current->persistent_id,
	       sizeof(deg_iter->dupli_object_current->persistent_id));
}

static void rna_DepsgraphIter_orco_get(PointerRNA *ptr, float *orco)
{
	BLI_Iterator *iterator = ptr->data;
	DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
	memcpy(orco, deg_iter->dupli_object_current->orco,
	       sizeof(deg_iter->dupli_object_current->orco));
}

static unsigned int rna_DepsgraphIter_random_id_get(PointerRNA *ptr)
{
	BLI_Iterator *iterator = ptr->data;
	DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
	return deg_iter->dupli_object_current->random_id;
}

static void rna_DepsgraphIter_uv_get(PointerRNA *ptr, float *uv)
{
	BLI_Iterator *iterator = ptr->data;
	DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
	memcpy(uv, deg_iter->dupli_object_current->uv,
	       sizeof(deg_iter->dupli_object_current->uv));
}

static int rna_DepsgraphIter_is_instance_get(PointerRNA *ptr)
{
	BLI_Iterator *iterator = ptr->data;
	DEGObjectIterData *deg_iter = (DEGObjectIterData *)iterator->data;
	return (deg_iter->dupli_object_current != NULL);
}

/* **************** Depsgraph **************** */

static void rna_Depsgraph_debug_relations_graphviz(Depsgraph *graph,
                                                   const char *filename)
{
	FILE *f = fopen(filename, "w");
	if (f == NULL) {
		return;
	}
	DEG_debug_relations_graphviz(graph, f, "Depsgraph");
	fclose(f);
}

static void rna_Depsgraph_debug_stats_gnuplot(Depsgraph *graph,
                                              const char *filename,
                                              const char *output_filename)
{
	FILE *f = fopen(filename, "w");
	if (f == NULL) {
		return;
	}
	DEG_debug_stats_gnuplot(graph, f, "Timing Statistics", output_filename);
	fclose(f);
}

static void rna_Depsgraph_debug_tag_update(Depsgraph *graph)
{
	DEG_graph_tag_relations_update(graph);
}

static void rna_Depsgraph_debug_stats(Depsgraph *graph, char *result)
{
	size_t outer, ops, rels;
	DEG_stats_simple(graph, &outer, &ops, &rels);
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
	data->mode = DEG_ITER_OBJECT_MODE_RENDER;

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

static void rna_Depsgraph_duplis_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	iter->internal.custom = MEM_callocN(sizeof(BLI_Iterator), __func__);
	DEGObjectIterData *data = MEM_callocN(sizeof(DEGObjectIterData), __func__);

	data->graph = (Depsgraph *)ptr->data;
	data->flag = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
	             DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET |
	             DEG_ITER_OBJECT_FLAG_VISIBLE |
	             DEG_ITER_OBJECT_FLAG_DUPLI;
	data->mode = DEG_ITER_OBJECT_MODE_RENDER;

	((BLI_Iterator *)iter->internal.custom)->valid = true;
	DEG_iterator_objects_begin(iter->internal.custom, data);
	iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Depsgraph_duplis_next(CollectionPropertyIterator *iter)
{
	DEG_iterator_objects_next(iter->internal.custom);
	iter->valid = ((BLI_Iterator *)iter->internal.custom)->valid;
}

static void rna_Depsgraph_duplis_end(CollectionPropertyIterator *iter)
{
	DEG_iterator_objects_end(iter->internal.custom);
	MEM_freeN(((BLI_Iterator *)iter->internal.custom)->data);
	MEM_freeN(iter->internal.custom);
}

static PointerRNA rna_Depsgraph_duplis_get(CollectionPropertyIterator *iter)
{
	BLI_Iterator *iterator = (BLI_Iterator *)iter->internal.custom;
	return rna_pointer_inherit_refine(&iter->parent, &RNA_DepsgraphIter, iterator);
}

static ID *rna_Depsgraph_evaluated_id_get(Depsgraph *depsgraph, ID *id_orig)
{
	return DEG_get_evaluated_id(depsgraph, id_orig);
}

#else

static void rna_def_depsgraph_iter(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "DepsgraphIter", NULL);
	RNA_def_struct_ui_text(srna, "Dependency Graph Iterator",
	                       "Extended information about dependency graph object iterator");

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_ui_text(prop, "Object", "Object the iterator points to");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_DepsgraphIter_object_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "instance_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_ui_text(prop, "Instance Object", "Object which is being instanced by this iterator");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_DepsgraphIter_instance_object_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_ui_text(prop, "Parent", "Parent of the duplication list");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_DepsgraphIter_parent_get", NULL, NULL, NULL);

	prop = RNA_def_property(srna, "persistent_id", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Persistent ID",
	                         "Persistent identifier for inter-frame matching of objects with motion blur");
	RNA_def_property_array(prop, 2 * MAX_DUPLI_RECUR);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_DepsgraphIter_persistent_id_get", NULL, NULL);

	prop = RNA_def_property(srna, "orco", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	/* Seems system is not smart enough to figure that getter function should return
	 * array for PROP_TRANSLATION.
	 */
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Generated Coordinates", "Generated coordinates in parent object space");
	RNA_def_property_float_funcs(prop, "rna_DepsgraphIter_orco_get", NULL, NULL);

	prop = RNA_def_property(srna, "random_id", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Dupli random id", "Random id for this dupli object");
	RNA_def_property_int_funcs(prop, "rna_DepsgraphIter_random_id_get", NULL, NULL);

	prop = RNA_def_property(srna, "uv", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "UV Coordinates", "UV coordinates in parent object space");
	RNA_def_property_array(prop, 2);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_DepsgraphIter_uv_get", NULL, NULL);

	prop = RNA_def_property(srna, "is_instance", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Is Instance", "Denotes whether the object is ocming from dupli-list");
	RNA_def_property_boolean_funcs(prop, "rna_DepsgraphIter_is_instance_get", NULL);
}

static void rna_def_depsgraph(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Depsgraph", NULL);
	RNA_def_struct_ui_text(srna, "Dependency Graph", "");

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

	prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_collection_funcs(prop,
	                                  "rna_Depsgraph_objects_begin",
	                                  "rna_Depsgraph_objects_next",
	                                  "rna_Depsgraph_objects_end",
	                                  "rna_Depsgraph_objects_get",
	                                  NULL, NULL, NULL, NULL);

	func = RNA_def_function(srna, "evaluated_id_get", "rna_Depsgraph_evaluated_id_get");
	parm = RNA_def_pointer(func, "id", "ID", "", "Original ID to get evaluated complementary part for");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "evaluated_id", "ID", "", "Evaluated ID for the given original one");
	RNA_def_function_return(func, parm);

	/* TODO(sergey): Find a better name. */
	prop = RNA_def_property(srna, "duplis", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "DepsgraphIter");
	RNA_def_property_collection_funcs(prop,
	                                  "rna_Depsgraph_duplis_begin",
	                                  "rna_Depsgraph_duplis_next",
	                                  "rna_Depsgraph_duplis_end",
	                                  "rna_Depsgraph_duplis_get",
	                                  NULL, NULL, NULL, NULL);
}

void RNA_def_depsgraph(BlenderRNA *brna)
{
	rna_def_depsgraph_iter(brna);
	rna_def_depsgraph(brna);
}

#endif
