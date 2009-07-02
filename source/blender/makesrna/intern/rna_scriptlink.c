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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_scriptlink_types.h"

#ifdef RNA_RUNTIME

#else

void RNA_def_scriptlink(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "ScriptLink", NULL);
	RNA_def_struct_ui_text(srna, "Script Link", "Scripts linked to a datablock, to be executed on changes to the datablock.");
	RNA_def_struct_ui_icon(srna, ICON_PYTHON);
}

#endif

