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

#include "BKE_depsgraph.h"

#define STATS_MAX_SIZE 16384

#ifdef RNA_RUNTIME

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_debug.h"

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

#else

static void rna_def_depsgraph(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

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
}

void RNA_def_depsgraph(BlenderRNA *brna)
{
	rna_def_depsgraph(brna);
}

#endif
