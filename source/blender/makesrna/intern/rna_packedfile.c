/**
 * $Id$
 *
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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_packedFile_types.h"

EnumPropertyItem unpack_method_items[] = {
	{PF_USE_LOCAL, "USE_LOCAL", 0, "Use Local File", ""},
	{PF_WRITE_LOCAL, "WRITE_LOCAL", 0, "Write Local File (overwrite existing)", ""},
	{PF_USE_ORIGINAL, "USE_ORIGINAL", 0, "Use Original File", ""},
	{PF_WRITE_ORIGINAL, "WRITE_ORIGINAL", 0, "Write Original File (overwrite existing)", ""},
	{0, NULL, 0, NULL, NULL}};

#ifdef RNA_RUNTIME
#else

void RNA_def_packedfile(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "PackedFile", NULL);
	RNA_def_struct_ui_text(srna, "Packed File", "External file packed into the .blend file");

	prop= RNA_def_property(srna, "size", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Size", "Size of packed file in bytes");

}

#endif

