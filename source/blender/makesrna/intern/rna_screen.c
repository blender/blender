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
 * Contributor(s): Blender Foundation (2008), Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_screen_types.h"
#include "DNA_scene_types.h"

#ifdef RNA_RUNTIME

static void *rna_Screen_scene_get(PointerRNA *ptr)
{
	bScreen *sc= (bScreen*)ptr->data;
	return sc->scene;
}

static void rna_Screen_areas_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	bScreen *scr= (bScreen*)ptr->data;
	rna_iterator_listbase_begin(iter, &scr->areabase, NULL);
}

#else

static void RNA_def_scrarea(BlenderRNA *brna)
{
	StructRNA *srna;
	
	srna= RNA_def_struct(brna, "Area", NULL);
	RNA_def_struct_ui_text(srna, "Area", "Area in a subdivided screen, containing an editor.");
	RNA_def_struct_sdna(srna, "ScrArea");
}

static void RNA_def_panel(BlenderRNA *brna)
{
	StructRNA *srna;
	
	srna= RNA_def_struct(brna, "Panel", NULL);
	RNA_def_struct_ui_text(srna, "Panel", "Buttons panel.");
	RNA_def_struct_sdna(srna, "Panel");
}

static void RNA_def_region(BlenderRNA *brna)
{
	StructRNA *srna;
	
	srna= RNA_def_struct(brna, "Region", NULL);
	RNA_def_struct_ui_text(srna, "Region", "Region in a subdivid screen area.");
	RNA_def_struct_sdna(srna, "ARegion");
}

static void RNA_def_bscreen(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Screen", "ID");
	RNA_def_struct_ui_text(srna, "Screen", "Screen datablock, defining the layout of areas in a window.");
	RNA_def_struct_sdna(srna, "bScreen");
	
	prop= RNA_def_property(srna, "scene", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_struct_type(prop, "Scene");
	RNA_def_property_pointer_funcs(prop, "rna_Screen_scene_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Scene", "Active scene to be edited in the screen.");
	
	prop= RNA_def_property(srna, "areas", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_struct_type(prop, "Area");
	RNA_def_property_collection_funcs(prop, "rna_Screen_areas_begin", "rna_iterator_listbase_next", "rna_iterator_listbase_end", "rna_iterator_listbase_get", 0, 0, 0, 0);
	RNA_def_property_ui_text(prop, "Areas", "All Screen Areas");
}

void RNA_def_screen(BlenderRNA *brna)
{
	RNA_def_bscreen(brna);
	RNA_def_scrarea(brna);
	RNA_def_panel(brna);
	RNA_def_region(brna);
}

#endif


