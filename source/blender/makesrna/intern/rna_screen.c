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
 * Contributor(s): Blender Foundation (2008), Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_screen.c
 *  \ingroup RNA
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
	{0, NULL, 0, NULL, NULL}
};

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BKE_global.h"
#include "BKE_depsgraph.h"

#include "UI_view2d.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

static void rna_Screen_scene_set(PointerRNA *ptr, PointerRNA value)
{
	bScreen *sc = (bScreen *)ptr->data;

	if (value.data == NULL)
		return;

	sc->newscene = value.data;
}

static void rna_Screen_scene_update(bContext *C, PointerRNA *ptr)
{
	bScreen *sc = (bScreen *)ptr->data;

	/* exception: must use context so notifier gets to the right window  */
	if (sc->newscene) {
#ifdef WITH_PYTHON
		BPy_BEGIN_ALLOW_THREADS;
#endif

		ED_screen_set_scene(C, sc, sc->newscene);

#ifdef WITH_PYTHON
		BPy_END_ALLOW_THREADS;
#endif

		WM_event_add_notifier(C, NC_SCENE | ND_SCENEBROWSE, sc->newscene);

		if (G.debug & G_DEBUG)
			printf("scene set %p\n", sc->newscene);

		sc->newscene = NULL;
	}
}

static void rna_Screen_redraw_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	bScreen *screen = (bScreen *)ptr->data;

	/* the settings for this are currently only available from a menu in the TimeLine, hence refresh=SPACE_TIME */
	ED_screen_animation_timer_update(screen, screen->redraws_flag, SPACE_TIME);
}


static int rna_Screen_is_animation_playing_get(PointerRNA *UNUSED(ptr))
{
	return (ED_screen_animation_playing(G.main->wm.first) != NULL);
}

static int rna_Screen_fullscreen_get(PointerRNA *ptr)
{
	bScreen *sc = (bScreen *)ptr->data;
	return (sc->full != 0);
}

/* UI compatible list: should not be needed, but for now we need to keep EMPTY
 * at least in the static version of this enum for python scripts. */
static EnumPropertyItem *rna_Area_type_itemf(bContext *UNUSED(C), PointerRNA *UNUSED(ptr),
                                             PropertyRNA *UNUSED(prop), bool *UNUSED(r_free))
{
	/* +1 to skip SPACE_EMPTY */
	return space_type_items + 1;
}

static void rna_Area_type_set(PointerRNA *ptr, int value)
{
	ScrArea *sa = (ScrArea *)ptr->data;
	sa->butspacetype = value;
}

static void rna_Area_type_update(bContext *C, PointerRNA *ptr)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win;
	bScreen *sc = (bScreen *)ptr->id.data;
	ScrArea *sa = (ScrArea *)ptr->data;

	/* XXX this call still use context, so we trick it to work in the right context */
	for (win = wm->windows.first; win; win = win->next) {
		if (sc == win->screen) {
			wmWindow *prevwin = CTX_wm_window(C);
			ScrArea *prevsa = CTX_wm_area(C);
			ARegion *prevar = CTX_wm_region(C);

			CTX_wm_window_set(C, win);
			CTX_wm_area_set(C, sa);
			CTX_wm_region_set(C, NULL);

			ED_area_newspace(C, sa, sa->butspacetype);
			ED_area_tag_redraw(sa);

			/* It is possible that new layers becomes visible. */
			if (sa->spacetype == SPACE_VIEW3D) {
				DAG_on_visible_update(CTX_data_main(C), false);
			}

			CTX_wm_window_set(C, prevwin);
			CTX_wm_area_set(C, prevsa);
			CTX_wm_region_set(C, prevar);
			break;
		}
	}
}

static void rna_View2D_region_to_view(struct View2D *v2d, int x, int y, float result[2])
{
	UI_view2d_region_to_view(v2d, x, y, &result[0], &result[1]);
}

static void rna_View2D_view_to_region(struct View2D *v2d, float x, float y, int clip, int result[2])
{
	if (clip)
		UI_view2d_view_to_region_clip(v2d, x, y, &result[0], &result[1]);
	else
		UI_view2d_view_to_region(v2d, x, y, &result[0], &result[1]);
}

#else

/* Area.spaces */
static void rna_def_area_spaces(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	RNA_def_property_srna(cprop, "AreaSpaces");
	srna = RNA_def_struct(brna, "AreaSpaces", NULL);
	RNA_def_struct_sdna(srna, "ScrArea");
	RNA_def_struct_ui_text(srna, "Area Spaces", "Collection of spaces");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "spacedata.first");
	RNA_def_property_struct_type(prop, "Space");
	RNA_def_property_ui_text(prop, "Active Space", "Space currently being displayed in this area");
}

static void rna_def_area(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;

	srna = RNA_def_struct(brna, "Area", NULL);
	RNA_def_struct_ui_text(srna, "Area", "Area in a subdivided screen, containing an editor");
	RNA_def_struct_sdna(srna, "ScrArea");

	prop = RNA_def_property(srna, "spaces", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "spacedata", NULL);
	RNA_def_property_struct_type(prop, "Space");
	RNA_def_property_ui_text(prop, "Spaces",
	                         "Spaces contained in this area, the first being the active space "
	                         "(NOTE: Useful for example to restore a previously used 3D view space "
	                         "in a certain area to get the old view orientation)");
	rna_def_area_spaces(brna, prop);

	prop = RNA_def_property(srna, "regions", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "regionbase", NULL);
	RNA_def_property_struct_type(prop, "Region");
	RNA_def_property_ui_text(prop, "Regions", "Regions this area is subdivided in");

	prop = RNA_def_property(srna, "show_menus", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", HEADER_NO_PULLDOWN);
	RNA_def_property_ui_text(prop, "Show Menus", "Show menus in the header");

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "spacetype");
	RNA_def_property_enum_items(prop, space_type_items);
	RNA_def_property_enum_default(prop, SPACE_VIEW3D);
	RNA_def_property_enum_funcs(prop, NULL, "rna_Area_type_set", "rna_Area_type_itemf");
	RNA_def_property_ui_text(prop, "Editor Type", "Current editor type for this area");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, 0, "rna_Area_type_update");

	prop = RNA_def_property(srna, "x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "totrct.xmin");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "X Position", "The window relative vertical location of the area");

	prop = RNA_def_property(srna, "y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "totrct.ymin");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Y Position", "The window relative horizontal location of the area");

	prop = RNA_def_property(srna, "width", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "winx");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Width", "Area width");

	prop = RNA_def_property(srna, "height", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "winy");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Height", "Area height");

	RNA_def_function(srna, "tag_redraw", "ED_area_tag_redraw");

	func = RNA_def_function(srna, "header_text_set", "ED_area_headerprint");
	RNA_def_function_ui_description(func, "Set the header text");
	RNA_def_string(func, "text", NULL, 0, "Text", "New string for the header, no argument clears the text");
}

static void rna_def_view2d_api(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;
	
	static const float view_default[2] = {0.0f, 0.0f};
	static const int region_default[2] = {0.0f, 0.0f};
	
	func = RNA_def_function(srna, "region_to_view", "rna_View2D_region_to_view");
	RNA_def_function_ui_description(func, "Transform region coordinates to 2D view");
	parm = RNA_def_int(func, "x", 0, INT_MIN, INT_MAX, "x", "Region x coordinate", -10000, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_int(func, "y", 0, INT_MIN, INT_MAX, "y", "Region y coordinate", -10000, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_float_array(func, "result", 2, view_default, -FLT_MAX, FLT_MAX, "Result", "View coordinates", -10000.0f, 10000.0f);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);

	func = RNA_def_function(srna, "view_to_region", "rna_View2D_view_to_region");
	RNA_def_function_ui_description(func, "Transform 2D view coordinates to region");
	parm = RNA_def_float(func, "x", 0.0f, -FLT_MAX, FLT_MAX, "x", "2D View x coordinate", -10000.0f, 10000.0f);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_float(func, "y", 0.0f, -FLT_MAX, FLT_MAX, "y", "2D View y coordinate", -10000.0f, 10000.0f);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_boolean(func, "clip", 1, "Clip", "Clip coordinates to the visible region");
	parm = RNA_def_int_array(func, "result", 2, region_default, INT_MIN, INT_MAX, "Result", "Region coordinates", -10000, 10000);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);
}

static void rna_def_view2d(BlenderRNA *brna)
{
	StructRNA *srna;
	/* PropertyRNA *prop; */

	srna = RNA_def_struct(brna, "View2D", NULL);
	RNA_def_struct_ui_text(srna, "View2D", "Scroll and zoom for a 2D region");
	RNA_def_struct_sdna(srna, "View2D");
	
	/* TODO more View2D properties could be exposed here (read-only) */
	
	rna_def_view2d_api(srna);
}

static void rna_def_region(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Region", NULL);
	RNA_def_struct_ui_text(srna, "Region", "Region in a subdivided screen area");
	RNA_def_struct_sdna(srna, "ARegion");

	prop = RNA_def_property(srna, "id", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "swinid");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Region ID", "Unique ID for this region");

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "regiontype");
	RNA_def_property_enum_items(prop, region_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Region Type", "Type of this region");

	prop = RNA_def_property(srna, "x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "winrct.xmin");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "X Position", "The window relative vertical location of the region");

	prop = RNA_def_property(srna, "y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "winrct.ymin");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Y Position", "The window relative horizontal location of the region");

	prop = RNA_def_property(srna, "width", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "winx");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Width", "Region width");

	prop = RNA_def_property(srna, "height", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "winy");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Height", "Region height");

	prop = RNA_def_property(srna, "view2d", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "v2d");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_ui_text(prop, "View2D", "2D view of the region");

	RNA_def_function(srna, "tag_redraw", "ED_region_tag_redraw");
}

static void rna_def_screen(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Screen", "ID");
	RNA_def_struct_sdna(srna, "Screen"); /* it is actually bScreen but for 2.5 the dna is patched! */
	RNA_def_struct_ui_text(srna, "Screen", "Screen datablock, defining the layout of areas in a window");
	RNA_def_struct_ui_icon(srna, ICON_SPLITSCREEN);

	/* pointers */
	prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Screen_scene_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Scene", "Active scene to be edited in the screen");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_Screen_scene_update");

	/* collections */
	prop = RNA_def_property(srna, "areas", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "areabase", NULL);
	RNA_def_property_struct_type(prop, "Area");
	RNA_def_property_ui_text(prop, "Areas", "Areas the screen is subdivided into");

	/* readonly status indicators */
	prop = RNA_def_property(srna, "is_animation_playing", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Screen_is_animation_playing_get", NULL);
	RNA_def_property_ui_text(prop, "Animation Playing", "Animation playback is active");

	prop = RNA_def_property(srna, "show_fullscreen", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_boolean_funcs(prop, "rna_Screen_fullscreen_get", NULL);
	RNA_def_property_ui_text(prop, "Fullscreen", "An area is maximized, filling this screen");

	/* Define Anim Playback Areas */
	prop = RNA_def_property(srna, "use_play_top_left_3d_editor", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_REGION);
	RNA_def_property_ui_text(prop, "Top-Left 3D Editor", "");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");

	prop = RNA_def_property(srna, "use_play_3d_editors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_ALL_3D_WIN);
	RNA_def_property_ui_text(prop, "All 3D View Editors", "");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");

	prop = RNA_def_property(srna, "use_play_animation_editors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_ALL_ANIM_WIN);
	RNA_def_property_ui_text(prop, "Animation Editors", "");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");

	prop = RNA_def_property(srna, "use_play_properties_editors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_ALL_BUTS_WIN);
	RNA_def_property_ui_text(prop, "Property Editors", "");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");

	prop = RNA_def_property(srna, "use_play_image_editors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_ALL_IMAGE_WIN);
	RNA_def_property_ui_text(prop, "Image Editors", "");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");

	prop = RNA_def_property(srna, "use_play_sequence_editors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_SEQ);
	RNA_def_property_ui_text(prop, "Sequencer Editors", "");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");

	prop = RNA_def_property(srna, "use_play_node_editors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_NODES);
	RNA_def_property_ui_text(prop, "Node Editors", "");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");

	prop = RNA_def_property(srna, "use_play_clip_editors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_CLIPS);
	RNA_def_property_ui_text(prop, "Clip Editors", "");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");
}

void RNA_def_screen(BlenderRNA *brna)
{
	rna_def_screen(brna);
	rna_def_area(brna);
	rna_def_region(brna);
	rna_def_view2d(brna);
}

#endif

