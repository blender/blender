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
 * Contributor(s): Blender Foundation (2009)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#include "UI_interface.h"

#else

static void rna_def_ui_layout(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "UILayout", NULL);
	RNA_def_struct_sdna(srna, "uiLayout");
	RNA_def_struct_ui_text(srna, "UI Layout", "User interface layout in a panel or header.");

	RNA_api_ui_layout(srna);
}

void RNA_def_ui(BlenderRNA *brna)
{
	rna_def_ui_layout(brna);
}

#endif

