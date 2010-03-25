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
 * Contributor(s): Blender Foundation (2008), Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stddef.h>

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_screen_types.h"
#include "DNA_scene_types.h"

EnumPropertyItem region_type_items[] = {
	{RGN_TYPE_WINDOW, "WINDOW", 0, "Window", ""},
	{RGN_TYPE_HEADER, "HEADER", 0, "Header", ""},
	{RGN_TYPE_CHANNELS, "CHANNELS", 0, "Channels", ""},
	{RGN_TYPE_TEMPORARY, "TEMPORARY", 0, "Temporary", ""},
	{RGN_TYPE_UI, "UI", 0, "UI", ""},
	{RGN_TYPE_TOOLS, "TOOLS", 0, "Tools", ""},
	{RGN_TYPE_TOOL_PROPS, "TOOL_PROPS", 0, "Tool Properties", ""},
	{RGN_TYPE_PREVIEW, "PREVIEW", 0, "Preview", ""},
	{0, NULL, 0, NULL, NULL}};

#ifdef RNA_RUNTIME

#include "ED_screen.h"


#include "WM_api.h"
#include "WM_types.h"

static void rna_Screen_scene_set(PointerRNA *ptr, PointerRNA value)
{
	bScreen *sc= (bScreen*)ptr->data;

	if(value.data == NULL)
		return;

	/* exception: can't set screens inside of area/region handers */
	sc->newscene= value.data;
}

static void rna_Screen_scene_update(bContext *C, PointerRNA *ptr)
{
	bScreen *sc= (bScreen*)ptr->data;

	/* exception: can't set screens inside of area/region handers */
	if(sc->newscene) {
		WM_event_add_notifier(C, NC_SCENE|ND_SCENEBROWSE, sc->newscene);
		sc->newscene= NULL;
	}
}

static int rna_Screen_animation_playing_get(PointerRNA *ptr)
{
	bScreen *sc= (bScreen*)ptr->data;
	return (sc->animtimer != NULL);
}

static int rna_Screen_fullscreen_get(PointerRNA *ptr)
{
	bScreen *sc= (bScreen*)ptr->data;
	return (sc->full == SCREENFULL);
}

static void rna_Area_type_set(PointerRNA *ptr, int value)
{
	ScrArea *sa= (ScrArea*)ptr->data;
	sa->butspacetype= value;
}

static void rna_Area_type_update(bContext *C, PointerRNA *ptr)
{
	ScrArea *sa= (ScrArea*)ptr->data;

	ED_area_newspace(C, sa, sa->butspacetype); /* XXX - this uses the window */
	ED_area_tag_redraw(sa);
}

#else

static void rna_def_area(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Area", NULL);
	RNA_def_struct_ui_text(srna, "Area", "Area in a subdivided screen, containing an editor");
	RNA_def_struct_sdna(srna, "ScrArea");

	prop= RNA_def_property(srna, "spaces", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "spacedata", NULL);
	RNA_def_property_struct_type(prop, "Space");
	RNA_def_property_ui_text(prop, "Spaces", "Spaces contained in this area, the first space is active");

	prop= RNA_def_property(srna, "active_space", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "spacedata.first");
	RNA_def_property_struct_type(prop, "Space");
	RNA_def_property_ui_text(prop, "Active Space", "Space currently being displayed in this area");

	prop= RNA_def_property(srna, "regions", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "regionbase", NULL);
	RNA_def_property_struct_type(prop, "Region");
	RNA_def_property_ui_text(prop, "Regions", "Regions this area is subdivided in");

	prop= RNA_def_property(srna, "show_menus", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", HEADER_NO_PULLDOWN);
	RNA_def_property_ui_text(prop, "Show Menus", "Show menus in the header");
	
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "spacetype");
	RNA_def_property_enum_items(prop, space_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_Area_type_set", NULL);
	RNA_def_property_ui_text(prop, "Type", "Space type");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_Area_type_update");

	RNA_def_function(srna, "tag_redraw", "ED_area_tag_redraw");
}

static void rna_def_region(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Region", NULL);
	RNA_def_struct_ui_text(srna, "Region", "Region in a subdivided screen area");
	RNA_def_struct_sdna(srna, "ARegion");
	
	prop= RNA_def_property(srna, "id", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "swinid");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Region ID", "Unique ID for this region");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "regiontype");
	RNA_def_property_enum_items(prop, region_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Region Type", "Type of this region");

	prop= RNA_def_property(srna, "width", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "winx");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Width", "Region width");

	prop= RNA_def_property(srna, "height", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "winy");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Height", "Region height");
}

static void rna_def_screen(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Screen", "ID");
	RNA_def_struct_sdna(srna, "Screen"); /* it is actually bScreen but for 2.5 the dna is patched! */
	RNA_def_struct_ui_text(srna, "Screen", "Screen datablock, defining the layout of areas in a window");
	RNA_def_struct_ui_icon(srna, ICON_SPLITSCREEN);
	
	prop= RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE|PROP_NEVER_NULL);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Screen_scene_set", NULL);
	RNA_def_property_ui_text(prop, "Scene", "Active scene to be edited in the screen");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_Screen_scene_update");
	
	prop= RNA_def_property(srna, "areas", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "areabase", NULL);
	RNA_def_property_struct_type(prop, "Area");
	RNA_def_property_ui_text(prop, "Areas", "Areas the screen is subdivided into");

	prop= RNA_def_property(srna, "animation_playing", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Screen_animation_playing_get", NULL);
	RNA_def_property_ui_text(prop, "Animation Playing", "Animation playback is active");
	
	prop= RNA_def_property(srna, "fullscreen", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Screen_fullscreen_get", NULL);
	RNA_def_property_ui_text(prop, "Fullscreen", "An area is maximised, filling this screen");
}

void RNA_def_screen(BlenderRNA *brna)
{
	rna_def_screen(brna);
	rna_def_area(brna);
	rna_def_region(brna);
}

#endif

