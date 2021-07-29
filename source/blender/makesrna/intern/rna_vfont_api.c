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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2015 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_vfont_api.c
 *  \ingroup RNA
 */

#include "DNA_packedFile_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#include "BKE_packedFile.h"

static void rna_VectorFont_pack(VFont *vfont, Main *bmain, ReportList *reports)
{
	vfont->packedfile = newPackedFile(reports, vfont->name, ID_BLEND_PATH(bmain, &vfont->id));
}

static void rna_VectorFont_unpack(VFont *vfont, ReportList *reports, int method)
{
	if (!vfont->packedfile) {
		BKE_report(reports, RPT_ERROR, "Font not packed");
	}
	else {
		/* reports its own error on failure */
		unpackVFont(reports, vfont, method);
	}
}

#else

void RNA_api_vfont(StructRNA *srna)
{
	FunctionRNA *func;

	func = RNA_def_function(srna, "pack", "rna_VectorFont_pack");
	RNA_def_function_ui_description(func, "Pack the font into the current blend file");
	RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_MAIN);

	func = RNA_def_function(srna, "unpack", "rna_VectorFont_unpack");
	RNA_def_function_ui_description(func, "Unpack the font to the samples filename");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_enum(func, "method", rna_enum_unpack_method_items, PF_USE_LOCAL, "method", "How to unpack");
}

#endif
