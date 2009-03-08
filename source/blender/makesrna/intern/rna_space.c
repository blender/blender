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
 * Contributor(s): Blender Foundation (2008)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_space_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "DNA_scene_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"

static StructRNA* rna_Space_refine(struct PointerRNA *ptr)
{
	SpaceLink *space= (SpaceLink*)ptr->data;

	switch(space->spacetype) {
		/*case SPACE_VIEW3D:
			return &RNA_SpaceView3D;
		case SPACE_IPO:
			return &RNA_SpaceGraphEditor;
		case SPACE_OOPS:
			return &RNA_SpaceOutliner;
		case SPACE_BUTS:
			return &RNA_SpaceButtonsWindow;
		case SPACE_FILE:
			return &RNA_SpaceFileBrowser;*/
		case SPACE_IMAGE:
			return &RNA_SpaceImageEditor;
		/*case SPACE_INFO:
			return &RNA_SpaceUserPreferences;
		case SPACE_SEQ:
			return &RNA_SpaceSequenceEditor;*/
		case SPACE_TEXT:
			return &RNA_SpaceTextEditor;
		//case SPACE_IMASEL:
		//	return &RNA_SpaceImageBrowser;
		/*case SPACE_SOUND:
			return &RNA_SpaceAudioWindow;
		case SPACE_ACTION:
			return &RNA_SpaceDopeSheetEditor;
		case SPACE_NLA:
			return &RNA_SpaceNLAEditor;
		case SPACE_SCRIPT:
			return &RNA_SpaceScriptsWindow;
		case SPACE_TIME:
			return &RNA_SpaceTimeline;
		case SPACE_NODE:
			return &RNA_SpaceNodeEditor;*/
		default:
			return &RNA_Space;
	}
}

static PointerRNA rna_SpaceImage_uvedit_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_SpaceUVEditor, ptr->data);
}

static void rna_SpaceImage_paint_update(bContext *C, PointerRNA *ptr)
{
	Scene *scene= CTX_data_scene(C);

	if(scene)
		brush_check_exists(&scene->toolsettings->imapaint.brush);
}

void rna_SpaceTextEditor_word_wrap_set(PointerRNA *ptr, int value)
{
	SpaceText *st= (SpaceText*)(ptr->data);

	st->wordwrap= value;
	st->left= 0;
}

#else

static void rna_def_space(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem type_items[] = {
		{SPACE_EMPTY, "EMPTY", "Empty", ""},
		{SPACE_VIEW3D, "VIEW_3D", "3D View", ""},
		{SPACE_IPO, "GRAPH_EDITOR", "Graph Editor", ""},
		{SPACE_OOPS, "OUTLINER", "Outliner", ""},
		{SPACE_BUTS, "BUTTONS_WINDOW", "Buttons Window", ""},
		{SPACE_FILE, "FILE_BROWSER", "File Browser", ""},
		{SPACE_IMAGE, "IMAGE_EDITOR", "Image Editor", ""},
		{SPACE_INFO, "USER_PREFERENCES", "User Preferences", ""},
		{SPACE_SEQ, "SEQUENCE_EDITOR", "Sequence Editor", ""},
		{SPACE_TEXT, "TEXT_EDITOR", "Text Editor", ""},
		//{SPACE_IMASEL, "IMAGE_BROWSER", "Image Browser", ""},
		{SPACE_SOUND, "AUDIO_WINDOW", "Audio Window", ""},
		{SPACE_ACTION, "DOPESHEET_EDITOR", "DopeSheet Editor", ""},
		{SPACE_NLA, "NLA_EDITOR", "NLA Editor", ""},
		{SPACE_SCRIPT, "SCRIPTS_WINDOW", "Scripts Window", ""},
		{SPACE_TIME, "TIMELINE", "Timeline", ""},
		{SPACE_NODE, "NODE_EDITOR", "Node Editor", ""},
		{0, NULL, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "Space", NULL);
	RNA_def_struct_sdna(srna, "SpaceLink");
	RNA_def_struct_ui_text(srna, "Space", "Space data for a screen area.");
	RNA_def_struct_refine_func(srna, "rna_Space_refine");
	
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "spacetype");
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Type", "Space data type.");
}

static void rna_def_space_image_uv(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

#if 0
	static EnumPropertyItem select_mode_items[] = {
		{SI_SELECT_VERTEX, "VERTEX", "Vertex", "Vertex selection mode."},
		//{SI_SELECT_EDGE, "Edge", "Edge", "Edge selection mode."},
		{SI_SELECT_FACE, "FACE", "Face", "Face selection mode."},
		{SI_SELECT_ISLAND, "ISLAND", "Island", "Island selection mode."},
		{0, NULL, NULL, NULL}};
#endif

	static EnumPropertyItem sticky_mode_items[] = {
		{SI_STICKY_DISABLE, "DISABLED", "Disabled", "Sticky vertex selection disabled."},
		{SI_STICKY_LOC, "SHARED_LOCATION", "SHARED_LOCATION", "Select UVs that are at the same location and share a mesh vertex."},
		{SI_STICKY_VERTEX, "SHARED_VERTEX", "SHARED_VERTEX", "Select UVs that share mesh vertex, irrespective if they are in the same location."},
		{0, NULL, NULL, NULL}};

	static EnumPropertyItem dt_uv_items[] = {
		{SI_UVDT_OUTLINE, "OUTLINE", "Outline", "Draw white edges with black outline."},
		{SI_UVDT_DASH, "DASH", "Dash", "Draw dashed black-white edges."},
		{SI_UVDT_BLACK, "BLACK", "Black", "Draw black edges."},
		{SI_UVDT_WHITE, "WHITE", "White", "Draw white edges."},
		{0, NULL, NULL, NULL}};

	static EnumPropertyItem dt_uvstretch_items[] = {
		{SI_UVDT_STRETCH_ANGLE, "ANGLE", "Angle", "Angular distortion between UV and 3D angles."},
		{SI_UVDT_STRETCH_AREA, "AREA", "Area", "Area distortion between UV and 3D faces."},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "SpaceUVEditor", NULL);
	RNA_def_struct_sdna(srna, "SpaceImage");
	RNA_def_struct_nested(brna, srna, "SpaceImageEditor");
	RNA_def_struct_ui_text(srna, "Space UV Editor", "UV editor data for the image editor space.");

	/* selection */
	/*prop= RNA_def_property(srna, "selection_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "selectmode");
	RNA_def_property_enum_items(prop, select_mode_items);
	RNA_def_property_ui_text(prop, "Selection Mode", "UV selection and display mode.");*/

	prop= RNA_def_property(srna, "sticky_selection_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "sticky");
	RNA_def_property_enum_items(prop, sticky_mode_items);
	RNA_def_property_ui_text(prop, "Sticky Selection Mode", "Automatically select also UVs sharing the same vertex as the ones being selected.");

	/* drawing */
	prop= RNA_def_property(srna, "edge_draw_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "dt_uv");
	RNA_def_property_enum_items(prop, dt_uv_items);
	RNA_def_property_ui_text(prop, "Edge Draw Type", "Draw type for drawing UV edges.");

	prop= RNA_def_property(srna, "draw_smooth_edges", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_SMOOTH_UV);
	RNA_def_property_ui_text(prop, "Draw Smooth Edges", "Draw UV edges anti-aliased.");

	prop= RNA_def_property(srna, "draw_stretch", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_DRAW_STRETCH);
	RNA_def_property_ui_text(prop, "Draw Stretch", "Draw faces colored according to the difference in shape between UVs and their 3D coordinates (blue for low distortion, red for high distortion).");

	prop= RNA_def_property(srna, "draw_stretch_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "dt_uvstretch");
	RNA_def_property_enum_items(prop, dt_uvstretch_items);
	RNA_def_property_ui_text(prop, "Draw Stretch Type", "Type of stretch to draw.");

	prop= RNA_def_property(srna, "draw_modified_edges", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "dt_uvstretch");
	RNA_def_property_enum_items(prop, dt_uvstretch_items);
	RNA_def_property_ui_text(prop, "Draw Modified Edges", "Draw edges from the final mesh after object modifier evaluation.");

	/*prop= RNA_def_property(srna, "local_view", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_LOCAL_UV);
	RNA_def_property_ui_text(prop, "Local View", "Draw only faces with the currently displayed image assigned.");*/

	prop= RNA_def_property(srna, "display_normalized_coordinates", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_COORDFLOATS);
	RNA_def_property_ui_text(prop, "Display Normalized Coordinates", "Display UV coordinates from 0.0 to 1.0 rather than in pixels.");

	/* todo: move edge and face drawing options here from G.f */

	/* editing */
	/*prop= RNA_def_property(srna, "sync_selection", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_SYNC_UVSEL);
	RNA_def_property_ui_text(prop, "Sync Selection", "Keep UV and edit mode mesh selection in sync.");*/

	prop= RNA_def_property(srna, "snap_to_pixels", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_PIXELSNAP);
	RNA_def_property_ui_text(prop, "Snap to Pixels", "Snap UVs to pixel locations while editing.");

	prop= RNA_def_property(srna, "constrain_to_image_bounds", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_CLIP_UV);
	RNA_def_property_ui_text(prop, "Constrain to Image Bounds", "Constraint to stay within the image bounds while editing.");

	prop= RNA_def_property(srna, "live_unwrap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_LIVE_UNWRAP);
	RNA_def_property_ui_text(prop, "Live Unwrap", "Continuously unwrap the selected UV island while transforming pinned vertices.");
}

static void rna_def_space_image(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem draw_channels_items[] = {
		{0, "COLOR", "Color", "Draw image with RGB colors."},
		{SI_USE_ALPHA, "COLOR_ALPHA", "Color and Alpha", "Draw image with RGB colors and alpha transparency."},
		{SI_SHOW_ALPHA, "ALPHA", "Alpha", "Draw alpha transparency channel."},
		{SI_SHOW_ZBUF, "Z_BUFFER", "Z-Buffer", "Draw Z-buffer associated with image (mapped from camera clip start to end)."},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "SpaceImageEditor", "Space");
	RNA_def_struct_sdna(srna, "SpaceImage");
	RNA_def_struct_ui_text(srna, "Space Image Editor", "Image and UV editor space data.");

	/* image */
	prop= RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Image", "Image displayed and edited in this space.");

	prop= RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "iuser");
	RNA_def_property_ui_text(prop, "Image User", "Parameters defining which layer, pass and frame of the image is displayed.");

	prop= RNA_def_property(srna, "curves", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "cumap");
	RNA_def_property_ui_text(prop, "Curves", "Color curve mapping to use for displaying the image.");

	prop= RNA_def_property(srna, "image_pin", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pin", 0);
	RNA_def_property_ui_text(prop, "Image Pin", "Display current image regardless of object selection.");

	/* image draw */
	prop= RNA_def_property(srna, "draw_repeated", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_DRAW_TILE);
	RNA_def_property_ui_text(prop, "Draw Repeated", "Draw the image repeated outside of the main view.");

	prop= RNA_def_property(srna, "draw_channels", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, draw_channels_items);
	RNA_def_property_ui_text(prop, "Draw Channels", "Channels of the image to draw.");

	/* uv */
	prop= RNA_def_property(srna, "uv_editor", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "SpaceUVEditor");
	RNA_def_property_pointer_funcs(prop, "rna_SpaceImage_uvedit_get", NULL);
	RNA_def_property_ui_text(prop, "UV Editor", "UV editor settings.");
	
	/* paint */
	prop= RNA_def_property(srna, "image_painting", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_DRAWTOOL);
	RNA_def_property_ui_text(prop, "Image Painting", "Enable image painting mode.");
	RNA_def_property_update(prop, 0, "rna_SpaceImage_paint_update");

	/* grease pencil */
	prop= RNA_def_property(srna, "grease_pencil", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "gpd");
	RNA_def_property_struct_type(prop, "UnknownType");
	RNA_def_property_ui_text(prop, "Grease Pencil", "Grease pencil data for this space.");

	prop= RNA_def_property(srna, "use_grease_pencil", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_DISPGP);
	RNA_def_property_ui_text(prop, "Use Grease Pencil", "Display and edit the grease pencil freehand annotations overlay.");

	/* update */
	prop= RNA_def_property(srna, "update_automatically", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "lock", 0);
	RNA_def_property_ui_text(prop, "Update Automatically", "Update other affected window spaces automatically to reflect changes during interactive operations such as transform.");

	rna_def_space_image_uv(brna);
}

static void rna_def_space_text(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem font_size_items[] = {
		{12, "SCREEN_12", "Screen 12", ""},
		{15, "SCREEN_15", "Screen 15", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "SpaceTextEditor", "Space");
	RNA_def_struct_sdna(srna, "SpaceText");
	RNA_def_struct_ui_text(srna, "Space Text Editor", "Text editor space data.");

	/* text */
	prop= RNA_def_property(srna, "text", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Text", "Text displayed and edited in this space.");

	/* display */
	prop= RNA_def_property(srna, "syntax_highlight", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "showsyntax", 0);
	RNA_def_property_ui_text(prop, "Syntax Highlight", "Syntax highlight for scripting.");
	RNA_def_property_update(prop, NC_TEXT|ND_DISPLAY, NULL);

	prop= RNA_def_property(srna, "word_wrap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "wordwrap", 0);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_SpaceTextEditor_word_wrap_set");
	RNA_def_property_ui_text(prop, "Word Wrap", "Wrap words if there is not enough horizontal space.");
	RNA_def_property_update(prop, NC_TEXT|ND_DISPLAY, NULL);

	prop= RNA_def_property(srna, "line_numbers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "showlinenrs", 0);
	RNA_def_property_ui_text(prop, "Line Numbers", "Show line numbers next to the text.");
	RNA_def_property_update(prop, NC_TEXT|ND_DISPLAY, NULL);

	prop= RNA_def_property(srna, "overwrite", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Overwrite", "Overwrite characters when typing rather than inserting them.");
	RNA_def_property_update(prop, NC_TEXT|ND_DISPLAY, NULL);

	prop= RNA_def_property(srna, "tab_width", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "tabnumber");
	RNA_def_property_range(prop, 2, 8);
	RNA_def_property_ui_text(prop, "Tab Width", "Number of spaces to display tabs with.");
	RNA_def_property_update(prop, NC_TEXT|ND_DISPLAY, NULL);

	prop= RNA_def_property(srna, "font_size", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "lheight");
	RNA_def_property_enum_items(prop, font_size_items);
	RNA_def_property_ui_text(prop, "Font Size", "Font size to use for displaying the text.");
	RNA_def_property_update(prop, NC_TEXT|ND_DISPLAY, NULL);

	/* find */
	prop= RNA_def_property(srna, "find_all", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", ST_FIND_ALL);
	RNA_def_property_ui_text(prop, "Find All", "Search in all text datablocks, instead of only the active one.");

	prop= RNA_def_property(srna, "find_wrap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", ST_FIND_WRAP);
	RNA_def_property_ui_text(prop, "Find Wrap", "Search again from the start of the file when reaching the end.");

	prop= RNA_def_property(srna, "find_text", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "findstr");
	RNA_def_property_ui_text(prop, "Find Text", "Text to search for with the find tool.");

	prop= RNA_def_property(srna, "replace_text", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "replacestr");
	RNA_def_property_ui_text(prop, "Replace Text", "Text to replace selected text with using the replace tool.");
}

void RNA_def_space(BlenderRNA *brna)
{
	rna_def_space(brna);
	rna_def_space_image(brna);
	rna_def_space_text(brna);
}

#endif

