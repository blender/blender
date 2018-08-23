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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_lattice_api.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stdio.h>

#include "RNA_define.h"

#include "BLI_sys_types.h"

#include "BLI_utildefines.h"

#include "rna_internal.h"  /* own include */

#ifdef RNA_RUNTIME
static void rna_Lattice_transform(Lattice *lt, float *mat, bool shape_keys)
{
	BKE_lattice_transform(lt, (float (*)[4])mat, shape_keys);

	DEG_id_tag_update(&lt->id, 0);
}

static void rna_Lattice_update_gpu_tag(Lattice *lt)
{
	BKE_lattice_batch_cache_dirty_tag(lt, BKE_LATTICE_BATCH_DIRTY_ALL);
}

#else

void RNA_api_lattice(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "transform", "rna_Lattice_transform");
	RNA_def_function_ui_description(func, "Transform lattice by a matrix");
	parm = RNA_def_float_matrix(func, "matrix", 4, 4, NULL, 0.0f, 0.0f, "", "Matrix", 0.0f, 0.0f);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_boolean(func, "shape_keys", 0, "", "Transform Shape Keys");

	RNA_def_function(srna, "update_gpu_tag", "rna_Lattice_update_gpu_tag");
}

#endif
