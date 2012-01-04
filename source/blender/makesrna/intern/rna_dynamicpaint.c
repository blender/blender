/**
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Contributor(s): Miika Hämäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <limits.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "BKE_modifier.h"
#include "BKE_dynamicpaint.h"

#include "DNA_dynamicpaint_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "WM_types.h"

EnumPropertyItem prop_dynamicpaint_type_items[] = {
		{MOD_DYNAMICPAINT_TYPE_CANVAS, "CANVAS", 0, "Canvas", ""},
		{MOD_DYNAMICPAINT_TYPE_BRUSH, "BRUSH", 0, "Brush", ""},
		{0, NULL, 0, NULL, NULL}};


#ifdef RNA_RUNTIME

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_particle.h"


static char *rna_DynamicPaintCanvasSettings_path(PointerRNA *ptr)
{
	DynamicPaintCanvasSettings *settings = (DynamicPaintCanvasSettings*)ptr->data;
	ModifierData *md= (ModifierData *)settings->pmd;

	return BLI_sprintfN("modifiers[\"%s\"].canvas_settings", md->name);
}

static char *rna_DynamicPaintBrushSettings_path(PointerRNA *ptr)
{
	DynamicPaintBrushSettings *settings = (DynamicPaintBrushSettings*)ptr->data;
	ModifierData *md= (ModifierData *)settings->pmd;

	return BLI_sprintfN("modifiers[\"%s\"].brush_settings", md->name);
}

static char *rna_DynamicPaintSurface_path(PointerRNA *ptr)
{
	DynamicPaintSurface *surface = (DynamicPaintSurface*)ptr->data;
	ModifierData *md= (ModifierData *)surface->canvas->pmd;

	return BLI_sprintfN("modifiers[\"%s\"].canvas_settings.canvas_surfaces[\"%s\"]", md->name, surface->name);
}


/*
*	Surfaces
*/

static void rna_DynamicPaint_redoModifier(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	DAG_id_tag_update(ptr->id.data, OB_RECALC_DATA);
}

static void rna_DynamicPaintSurfaces_updateFrames(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	dynamicPaint_cacheUpdateFrames((DynamicPaintSurface*)ptr->data);
}

static void rna_DynamicPaintSurface_reset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	dynamicPaint_resetSurface((DynamicPaintSurface*)ptr->data);
	rna_DynamicPaint_redoModifier(bmain, scene, ptr);
}

static void rna_DynamicPaintSurface_initialcolortype(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	DynamicPaintSurface *surface = (DynamicPaintSurface*)ptr->data;

	surface->init_layername[0] = '\0';
	dynamicPaint_clearSurface(surface);
	rna_DynamicPaint_redoModifier(bmain, scene, ptr);
}

static void rna_DynamicPaintSurface_changePreview(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	DynamicPaintSurface *act_surface = (DynamicPaintSurface*)ptr->data;
	DynamicPaintSurface *surface = act_surface->canvas->surfaces.first;

	/* since only one color surface can show preview at time
	*  disable preview on other surfaces*/
	for(; surface; surface=surface->next) {
		if(surface != act_surface)
			surface->flags &= ~MOD_DPAINT_PREVIEW;
	}
	rna_DynamicPaint_redoModifier(bmain, scene, ptr);
}

static void rna_DynamicPaintSurface_uniqueName(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	dynamicPaintSurface_setUniqueName((DynamicPaintSurface*)ptr->data, ((DynamicPaintSurface*)ptr->data)->name);
}


static void rna_DynamicPaintSurface_changeType(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	dynamicPaintSurface_updateType((DynamicPaintSurface*)ptr->data);
	dynamicPaint_resetSurface((DynamicPaintSurface*)ptr->data);
	rna_DynamicPaintSurface_reset(bmain, scene, ptr);
}

static void rna_DynamicPaintSurfaces_changeFormat(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	DynamicPaintSurface *surface = (DynamicPaintSurface*)ptr->data;

	surface->type = MOD_DPAINT_SURFACE_T_PAINT;
	dynamicPaintSurface_updateType((DynamicPaintSurface*)ptr->data);
	rna_DynamicPaintSurface_reset(bmain, scene, ptr);
}

static void rna_DynamicPaint_resetDependancy(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	rna_DynamicPaintSurface_reset(bmain, scene, ptr);
	DAG_scene_sort(bmain, scene);
}

static PointerRNA rna_PaintSurface_active_get(PointerRNA *ptr)
{
	DynamicPaintCanvasSettings *canvas= (DynamicPaintCanvasSettings*)ptr->data;
	DynamicPaintSurface *surface = canvas->surfaces.first;
	int id=0;

	for(; surface; surface=surface->next) {
		if(id == canvas->active_sur)
			return rna_pointer_inherit_refine(ptr, &RNA_DynamicPaintSurface, surface);
		id++;
	}
	return rna_pointer_inherit_refine(ptr, &RNA_DynamicPaintSurface, NULL);
}

static void rna_DynamicPaint_surfaces_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	DynamicPaintCanvasSettings *canvas= (DynamicPaintCanvasSettings*)ptr->data;
	//rna_iterator_array_begin(iter, (void*)canvas->surfaces, sizeof(PaintSurface), canvas->totsur, 0, 0);
	rna_iterator_listbase_begin(iter, &canvas->surfaces, NULL);
}

static int rna_Surface_active_point_index_get(PointerRNA *ptr)
{
	DynamicPaintCanvasSettings *canvas= (DynamicPaintCanvasSettings*)ptr->data;
	return canvas->active_sur;
}

static void rna_Surface_active_point_index_set(struct PointerRNA *ptr, int value)
{
	DynamicPaintCanvasSettings *canvas= (DynamicPaintCanvasSettings*)ptr->data;
	canvas->active_sur = value;
	return;
}

static void rna_Surface_active_point_range(PointerRNA *ptr, int *min, int *max)
{
	DynamicPaintCanvasSettings *canvas= (DynamicPaintCanvasSettings*)ptr->data;

	*min= 0;
	*max= BLI_countlist(&canvas->surfaces)-1;
}

/* uvlayer */
static void rna_DynamicPaint_uvlayer_set(PointerRNA *ptr, const char *value)
{
	DynamicPaintCanvasSettings *canvas= ((DynamicPaintSurface*)ptr->data)->canvas;
	DynamicPaintSurface *surface = canvas->surfaces.first;
	int id=0;

	for(; surface; surface=surface->next) {
		if(id == canvas->active_sur) {
			rna_object_uvlayer_name_set(ptr, value, surface->uvlayer_name, sizeof(surface->uvlayer_name));
			return;
		}
		id++;
	}
}

/* is point cache used */
static int rna_DynamicPaint_is_cache_user_get(PointerRNA *ptr)
{
	DynamicPaintSurface *surface= (DynamicPaintSurface*)ptr->data;

	return (surface->format != MOD_DPAINT_SURFACE_F_IMAGESEQ) ?  1 : 0;
}

/* does output layer exist*/
static int rna_DynamicPaint_is_output_exists(DynamicPaintSurface *surface, Object *ob, int index)
{
	return dynamicPaint_outputLayerExists(surface, ob, index);
}


static EnumPropertyItem *rna_DynamicPaint_surface_type_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), int *free)
{
	DynamicPaintSurface *surface= (DynamicPaintSurface*)ptr->data;

	EnumPropertyItem *item= NULL;
	EnumPropertyItem tmp= {0, "", 0, "", ""};
	int totitem= 0;

	/* Paint type - available for all formats */
	tmp.value = MOD_DPAINT_SURFACE_T_PAINT;
	tmp.identifier = "PAINT";
	tmp.name = "Paint";
	RNA_enum_item_add(&item, &totitem, &tmp);

	/* Displace */
	if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX ||
		surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ) {
		tmp.value = MOD_DPAINT_SURFACE_T_DISPLACE;
		tmp.identifier = "DISPLACE";
		tmp.name = "Displace";
		RNA_enum_item_add(&item, &totitem, &tmp);
	}

	/* Weight */
	if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX) {
		tmp.value = MOD_DPAINT_SURFACE_T_WEIGHT;
		tmp.identifier = "WEIGHT";
		tmp.name = "Weight";
		RNA_enum_item_add(&item, &totitem, &tmp);
	}

	/* Height waves */
	{
		tmp.value = MOD_DPAINT_SURFACE_T_WAVE;
		tmp.identifier = "WAVE";
		tmp.name = "Waves";
		RNA_enum_item_add(&item, &totitem, &tmp);
	}

	RNA_enum_item_end(&item, &totitem);
	*free = 1;

	return item;
}

#else

/* canvas.canvas_surfaces */
static void rna_def_canvas_surfaces(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	RNA_def_property_srna(cprop, "DynamicPaintSurfaces");
	srna= RNA_def_struct(brna, "DynamicPaintSurfaces", NULL);
	RNA_def_struct_sdna(srna, "DynamicPaintCanvasSettings");
	RNA_def_struct_ui_text(srna, "Canvas Surfaces", "Collection of Dynamic Paint Canvas surfaces");

	prop= RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_funcs(prop, "rna_Surface_active_point_index_get", "rna_Surface_active_point_index_set", "rna_Surface_active_point_range");
	RNA_def_property_ui_text(prop, "Active Point Cache Index", "");

	prop= RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "DynamicPaintSurface");
	RNA_def_property_pointer_funcs(prop, "rna_PaintSurface_active_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Surface", "Active Dynamic Paint surface being displayed");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);
}


static void rna_def_canvas_surface(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	PropertyRNA *parm;
	FunctionRNA *func;

	/*  Surface format */
	static EnumPropertyItem prop_dynamicpaint_surface_format[] = {
			//{MOD_DPAINT_SURFACE_F_PTEX, "PTEX", ICON_TEXTURE_SHADED, "Ptex", ""},
			{MOD_DPAINT_SURFACE_F_VERTEX, "VERTEX", ICON_OUTLINER_DATA_MESH, "Vertex", ""},
			{MOD_DPAINT_SURFACE_F_IMAGESEQ, "IMAGE", ICON_FILE_IMAGE, "Image Sequence", ""},
			{0, NULL, 0, NULL, NULL}};

	/*  Surface type - generated dynamically based on surface format */
	static EnumPropertyItem prop_dynamicpaint_surface_type[] = {
			{MOD_DPAINT_SURFACE_T_PAINT, "PAINT", 0, "Paint", ""},
			{0, NULL, 0, NULL, NULL}};

	/*  Surface output preview. currently only paint has multiple outputs */
	static EnumPropertyItem prop_dynamicpaint_surface_preview[] = {
			{MOD_DPAINT_SURFACE_PREV_PAINT, "PAINT", 0, "Paint", ""},
			{MOD_DPAINT_SURFACE_PREV_WETMAP, "WETMAP", 0, "Wetmap", ""},
			{0, NULL, 0, NULL, NULL}};

	/*  Initial color setting */
	static EnumPropertyItem prop_dynamicpaint_init_color_type[] = {
			{MOD_DPAINT_INITIAL_NONE, "NONE", 0, "None", ""},
			{MOD_DPAINT_INITIAL_COLOR, "COLOR", ICON_COLOR, "Color", ""},
			{MOD_DPAINT_INITIAL_TEXTURE, "TEXTURE", ICON_TEXTURE, "UV Texture", ""},
			{MOD_DPAINT_INITIAL_VERTEXCOLOR, "VERTEX_COLOR", ICON_GROUP_VCOL, "Vertex Color", ""},
			{0, NULL, 0, NULL, NULL}};

	/*  Effect type
	*   Only used by ui to view per effect settings */
	static EnumPropertyItem prop_dynamicpaint_effecttype[] = {
			{1, "SPREAD", 0, "Spread", ""},
			{2, "DRIP", 0, "Drip", ""},
			{3, "SHRINK", 0, "Shrink", ""},
			{0, NULL, 0, NULL, NULL}};

	/* Displacemap file format */
	static EnumPropertyItem prop_dynamicpaint_image_fileformat[] = {
			{MOD_DPAINT_IMGFORMAT_PNG, "PNG", 0, "PNG", ""},
#ifdef WITH_OPENEXR
			{MOD_DPAINT_IMGFORMAT_OPENEXR, "OPENEXR", 0, "OpenEXR", ""},
#endif
			{0, NULL, 0, NULL, NULL}};

	/* Displacemap type */
	static EnumPropertyItem prop_dynamicpaint_displace_type[] = {
			{MOD_DPAINT_DISP_DISPLACE, "DISPLACE", 0, "Displacement", ""},
			{MOD_DPAINT_DISP_DEPTH, "DEPTH", 0, "Depth", ""},
			{0, NULL, 0, NULL, NULL}};



	/* Surface */
	srna= RNA_def_struct(brna, "DynamicPaintSurface", NULL);
	RNA_def_struct_sdna(srna, "DynamicPaintSurface");
	RNA_def_struct_ui_text(srna, "Paint Surface", "A canvas surface layer");
	RNA_def_struct_path_func(srna, "rna_DynamicPaintSurface_path");

	prop= RNA_def_property(srna, "surface_format", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_sdna(prop, NULL, "format");
	RNA_def_property_enum_items(prop, prop_dynamicpaint_surface_format);
	RNA_def_property_ui_text(prop, "Format", "Surface Format");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaintSurfaces_changeFormat");

	prop= RNA_def_property(srna, "surface_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_dynamicpaint_surface_type);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_DynamicPaint_surface_type_itemf");
	RNA_def_property_ui_text(prop, "Surface Type", "Surface Type");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaintSurface_changeType");

	prop= RNA_def_property(srna, "is_active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_ACTIVE);
	RNA_def_property_ui_text(prop, "Is Active", "Toggle whether surface is processed or ignored");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");

	prop= RNA_def_property(srna, "show_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_PREVIEW);
	RNA_def_property_ui_text(prop, "Show Preview", "Display surface preview in 3D-views");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaintSurface_changePreview");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Surface name");
	RNA_def_property_update(prop, NC_OBJECT, "rna_DynamicPaintSurface_uniqueName");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "brush_group", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Brush Group", "Only use brush objects from this group");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_resetDependancy");


	/*
	*   Paint, wet and displace
	*/

	prop= RNA_def_property(srna, "use_dissolve", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_DISSOLVE);
	RNA_def_property_ui_text(prop, "Dissolve", "Enable to make surface changes disappear over time");
	
	prop= RNA_def_property(srna, "dissolve_speed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "diss_speed");
	RNA_def_property_range(prop, 1.0, 10000.0);
	RNA_def_property_ui_range(prop, 1.0, 10000.0, 5, 0);
	RNA_def_property_ui_text(prop, "Dissolve Speed", "Approximately in how many frames should dissolve happen");
	
	prop= RNA_def_property(srna, "dry_speed", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 1.0, 10000.0);
	RNA_def_property_ui_range(prop, 1.0, 10000.0, 5, 0);
	RNA_def_property_ui_text(prop, "Dry Speed", "Approximately in how many frames should drying happen");
	
	/*
	*   Simulation settings
	*/
	prop= RNA_def_property(srna, "image_resolution", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 16.0, 4096.0);
	RNA_def_property_ui_range(prop, 16.0, 4096.0, 1, 0);
	RNA_def_property_ui_text(prop, "Resolution", "Output image resolution");
	
	prop= RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "uvlayer_name");
	RNA_def_property_ui_text(prop, "UV Map", "UV map name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_DynamicPaint_uvlayer_set");
	
	prop= RNA_def_property(srna, "frame_start", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "start_frame");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 1.0, 9999.0);
	RNA_def_property_ui_range(prop, 1.0, 9999, 1, 0);
	RNA_def_property_ui_text(prop, "Start Frame", "Simulation start frame");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaintSurfaces_updateFrames");
	
	prop= RNA_def_property(srna, "frame_end", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "end_frame");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 1.0, 9999.0);
	RNA_def_property_ui_range(prop, 1.0, 9999.0, 1, 0);
	RNA_def_property_ui_text(prop, "End Frame", "Simulation end frame");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaintSurfaces_updateFrames");
	
	prop= RNA_def_property(srna, "frame_substeps", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "substeps");
	RNA_def_property_range(prop, 0.0, 20.0);
	RNA_def_property_ui_range(prop, 0.0, 10, 1, 0);
	RNA_def_property_ui_text(prop, "Sub-Steps", "Do extra frames between scene frames to ensure smooth motion");
	
	prop= RNA_def_property(srna, "use_antialiasing", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_ANTIALIAS);
	RNA_def_property_ui_text(prop, "Anti-aliasing", "Use 5x multisampling to smoothen paint edges");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaintSurface_reset");

	/*
	*	Initial Color
	*/

	prop= RNA_def_property(srna, "init_color_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, prop_dynamicpaint_init_color_type);
	RNA_def_property_ui_text(prop, "Initial Color", "");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING_DRAW|ND_MODIFIER, "rna_DynamicPaintSurface_initialcolortype");

	prop= RNA_def_property(srna, "init_color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_array(prop, 4);
	RNA_def_property_ui_text(prop, "Color", "Initial color of the surface");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING_DRAW|ND_MODIFIER, "rna_DynamicPaintSurface_reset");

	prop= RNA_def_property(srna, "init_texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Texture", "");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING_DRAW|ND_MODIFIER, "rna_DynamicPaintSurface_reset");

	prop= RNA_def_property(srna, "init_layername", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Data Layer", "");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING_DRAW|ND_MODIFIER, "rna_DynamicPaintSurface_reset");

	/*
	*   Effect Settings
	*/
	prop= RNA_def_property(srna, "effect_ui", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, prop_dynamicpaint_effecttype);
	RNA_def_property_ui_text(prop, "Effect Type", "");
	
	prop= RNA_def_property(srna, "use_dry_log", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_DRY_LOG);
	RNA_def_property_ui_text(prop, "Slow", "Use logarithmic drying (makes high values to dry faster than low values)");

	prop= RNA_def_property(srna, "use_dissolve_log", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_DISSOLVE_LOG);
	RNA_def_property_ui_text(prop, "Slow", "Use logarithmic dissolve (makes high values to fade faster than low values)");
	
	prop= RNA_def_property(srna, "use_spread", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "effect", MOD_DPAINT_EFFECT_DO_SPREAD);
	RNA_def_property_ui_text(prop, "Use Spread", "Process spread effect (spread wet paint around surface)");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaintSurface_reset");
	
	prop= RNA_def_property(srna, "spread_speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "spread_speed");
	RNA_def_property_range(prop, 0.001, 10.0);
	RNA_def_property_ui_range(prop, 0.01, 5.0, 1, 2);
	RNA_def_property_ui_text(prop, "Spread Speed", "How fast spread effect moves on the canvas surface");

	prop= RNA_def_property(srna, "color_spread_speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "color_spread_speed");
	RNA_def_property_range(prop, 0.0, 2.0);
	RNA_def_property_ui_range(prop, 0.0, 2.0, 1, 2);
	RNA_def_property_ui_text(prop, "Color Spread", "How fast colors get mixed within wet paint");
	
	prop= RNA_def_property(srna, "use_drip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "effect", MOD_DPAINT_EFFECT_DO_DRIP);
	RNA_def_property_ui_text(prop, "Use Drip", "Process drip effect (drip wet paint to gravity direction)");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaintSurface_reset");
	
	prop= RNA_def_property(srna, "use_shrink", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "effect", MOD_DPAINT_EFFECT_DO_SHRINK);
	RNA_def_property_ui_text(prop, "Use Shrink", "Process shrink effect (shrink paint areas)");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaintSurface_reset");
	
	prop= RNA_def_property(srna, "shrink_speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shrink_speed");
	RNA_def_property_range(prop, 0.001, 10.0);
	RNA_def_property_ui_range(prop, 0.01, 5.0, 1, 2);
	RNA_def_property_ui_text(prop, "Shrink Speed", "How fast shrink effect moves on the canvas surface");

	prop= RNA_def_property(srna, "effector_weights", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "EffectorWeights");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Effector Weights", "");

	prop= RNA_def_property(srna, "drip_velocity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "drip_vel");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Velocity", "How much surface velocity affects dripping");

	prop= RNA_def_property(srna, "drip_acceleration", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "drip_acc");
	RNA_def_property_range(prop, -200.0f, 200.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Acceleration", "How much surface acceleration affects dripping");

	/*
	*   Output settings
	*/
	prop= RNA_def_property(srna, "use_premultiply", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_MULALPHA);
	RNA_def_property_ui_text(prop, "Premultiply alpha", "Multiply color by alpha (recommended for Blender input)");
	
	prop= RNA_def_property(srna, "image_output_path", PROP_STRING, PROP_DIRPATH);
	RNA_def_property_string_sdna(prop, NULL, "image_output_path");
	RNA_def_property_ui_text(prop, "Output Path", "Directory to save the textures");

	/* output for primary surface data */
	prop= RNA_def_property(srna, "output_name_a", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "output_name");
	RNA_def_property_ui_text(prop, "Output Name", "Name used to save output from this surface");

	prop= RNA_def_property(srna, "use_output_a", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_OUT1);
	RNA_def_property_ui_text(prop, "Use Output", "Save this output layer");

	/* output for secondary sufrace data */
	prop= RNA_def_property(srna, "output_name_b", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "output_name2");
	RNA_def_property_ui_text(prop, "Output Name", "Name used to save output from this surface");

	prop= RNA_def_property(srna, "use_output_b", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_OUT2);
	RNA_def_property_ui_text(prop, "Use Output", "Save this output layer");

	prop= RNA_def_property(srna, "preview_id", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_sdna(prop, NULL, "preview_id");
	RNA_def_property_enum_items(prop, prop_dynamicpaint_surface_preview);
	RNA_def_property_ui_text(prop, "Preview", "");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");

	/* to check if output name exists */
	func = RNA_def_function(srna, "output_exists", "rna_DynamicPaint_is_output_exists");
	RNA_def_function_ui_description(func, "Checks if surface output layer of given name exists");
	parm= RNA_def_pointer(func, "object", "Object", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);
	parm= RNA_def_int(func, "index", 0, 0, 1, "Index", "", 0, 1);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm= RNA_def_boolean(func, "exists", 0, "", "");
	RNA_def_function_return(func, parm);
	
	prop= RNA_def_property(srna, "depth_clamp", PROP_FLOAT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.00, 50.0);
	RNA_def_property_ui_range(prop, 0.00, 5.0, 1, 2);
	RNA_def_property_ui_text(prop, "Max Displace", "Maximum level of depth intersection in object space (use 0.0 to disable)");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");

	prop= RNA_def_property(srna, "displace_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "disp_factor");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, -50.0, 50.0);
	RNA_def_property_ui_range(prop, -5.0, 5.0, 1, 2);
	RNA_def_property_ui_text(prop, "Displace Factor", "Strength of displace when applied to the mesh");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");
	
	prop= RNA_def_property(srna, "image_fileformat", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, prop_dynamicpaint_image_fileformat);
	RNA_def_property_ui_text(prop, "File Format", "");
	
	prop= RNA_def_property(srna, "displace_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "disp_type");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, prop_dynamicpaint_displace_type);
	RNA_def_property_ui_text(prop, "Data Type", "");

	prop= RNA_def_property(srna, "use_incremental_displace", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_DISP_INCREMENTAL);
	RNA_def_property_ui_text(prop, "Incremental", "New displace is added cumulatively on top of existing");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaintSurface_reset");

	/* wave simulator settings */
	prop= RNA_def_property(srna, "wave_damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_range(prop, 0.01, 1.0, 1, 2);
	RNA_def_property_ui_text(prop, "Damping", "Wave damping factor");

	prop= RNA_def_property(srna, "wave_speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01, 5.0);
	RNA_def_property_ui_range(prop, 0.20, 4.0, 1, 2);
	RNA_def_property_ui_text(prop, "Speed", "Wave propogation speed");

	prop= RNA_def_property(srna, "wave_timescale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01, 3.0);
	RNA_def_property_ui_range(prop, 0.01, 1.5, 1, 2);
	RNA_def_property_ui_text(prop, "Timescale", "Wave time scaling factor");

	prop= RNA_def_property(srna, "wave_spring", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_range(prop, 0.01, 1.0, 1, 2);
	RNA_def_property_ui_text(prop, "Spring", "Spring force that pulls water level back to zero");

	prop= RNA_def_property(srna, "use_wave_open_border", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_WAVE_OPEN_BORDERS);
	RNA_def_property_ui_text(prop, "Open Borders", "Pass waves through mesh edges");

	
	/* cache */
	prop= RNA_def_property(srna, "point_cache", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "pointcache");
	RNA_def_property_ui_text(prop, "Point Cache", "");

	/* is cache used */
	prop= RNA_def_property(srna, "is_cache_user", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_DynamicPaint_is_cache_user_get", NULL);
	RNA_def_property_ui_text(prop, "Use Cache", "");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE|PROP_EDITABLE);
}

static void rna_def_dynamic_paint_canvas_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "DynamicPaintCanvasSettings", NULL);
	RNA_def_struct_ui_text(srna, "Canvas Settings", "Dynamic Paint canvas settings");
	RNA_def_struct_sdna(srna, "DynamicPaintCanvasSettings");
	RNA_def_struct_path_func(srna, "rna_DynamicPaintCanvasSettings_path");

	/*
	*	Surface Slots
	*/
	prop= RNA_def_property(srna, "canvas_surfaces", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_DynamicPaint_surfaces_begin", "rna_iterator_listbase_next", "rna_iterator_listbase_end", "rna_iterator_listbase_get", NULL, NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "DynamicPaintSurface");
	RNA_def_property_ui_text(prop, "Paint Surface List", "Paint surface list");
	rna_def_canvas_surfaces(brna, prop);
}

static void rna_def_dynamic_paint_brush_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* paint collision type */
	static EnumPropertyItem prop_dynamicpaint_collisiontype[] = {
			{MOD_DPAINT_COL_PSYS, "PARTICLE_SYSTEM", ICON_PARTICLES, "Particle System", ""},
			{MOD_DPAINT_COL_POINT, "POINT", ICON_META_EMPTY, "Object Center", ""},
			{MOD_DPAINT_COL_DIST, "DISTANCE", ICON_META_EMPTY, "Proximity", ""},
			{MOD_DPAINT_COL_VOLDIST, "VOLUME_DISTANCE", ICON_META_CUBE, "Mesh Volume + Proximity", ""},
			{MOD_DPAINT_COL_VOLUME, "VOLUME", ICON_MESH_CUBE, "Mesh Volume", ""},
			{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem prop_dynamicpaint_prox_falloff[] = {
			{MOD_DPAINT_PRFALL_SMOOTH, "SMOOTH", ICON_SPHERECURVE, "Smooth", ""},
			{MOD_DPAINT_PRFALL_CONSTANT, "CONSTANT", ICON_NOCURVE, "Constant", ""},
			{MOD_DPAINT_PRFALL_RAMP, "RAMP", ICON_COLOR, "Color Ramp", ""},
			{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem prop_dynamicpaint_brush_wave_type[] = {
			{MOD_DPAINT_WAVEB_CHANGE, "CHANGE", 0, "Depth Change", ""},
			{MOD_DPAINT_WAVEB_DEPTH, "DEPTH", 0, "Obstacle", ""},
			{MOD_DPAINT_WAVEB_FORCE, "FORCE", 0, "Force", ""},
			{MOD_DPAINT_WAVEB_REFLECT, "REFLECT", 0, "Reflect Only", ""},
			{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem prop_dynamicpaint_brush_ray_dir[] = {
			{MOD_DPAINT_RAY_CANVAS, "CANVAS", 0, "Canvas Normal", ""},
			{MOD_DPAINT_RAY_BRUSH_AVG, "BRUSH", 0, "Brush Normal", ""},
			{MOD_DPAINT_RAY_ZPLUS, "Z_AXIS", 0, "Z-Axis", ""},
			{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "DynamicPaintBrushSettings", NULL);
	RNA_def_struct_ui_text(srna, "Brush Settings", "Brush settings");
	RNA_def_struct_sdna(srna, "DynamicPaintBrushSettings");
	RNA_def_struct_path_func(srna, "rna_DynamicPaintBrushSettings_path");

	/*
	*   Paint
	*/
	prop= RNA_def_property(srna, "paint_color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "r");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Paint Color", "Color of the paint");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");

	prop= RNA_def_property(srna, "paint_alpha", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "alpha");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 5, 2);
	RNA_def_property_ui_text(prop, "Paint Alpha", "Paint alpha");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");
	
	prop= RNA_def_property(srna, "use_material", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_USE_MATERIAL);
	RNA_def_property_ui_text(prop, "Use object material", "Use object material to define color and influence");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");

	prop= RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "mat");
	RNA_def_property_ui_text(prop, "Material", "Material to use (if not defined, material linked to the mesh is used)");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");
	
	prop= RNA_def_property(srna, "use_absolute_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_ABS_ALPHA);
	RNA_def_property_ui_text(prop, "Absolute Alpha", "Only increase alpha value if paint alpha is higher than existing");
	
	prop= RNA_def_property(srna, "paint_wetness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "wetness");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 5, 2);
	RNA_def_property_ui_text(prop, "Paint Wetness", "Paint wetness, visible in wetmap (some effects only affect wet paint)");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");
	
	prop= RNA_def_property(srna, "use_paint_erase", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_ERASE);
	RNA_def_property_ui_text(prop, "Erase Paint", "Erase / remove paint instead of adding it");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");

	prop= RNA_def_property(srna, "wave_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, prop_dynamicpaint_brush_wave_type);
	RNA_def_property_ui_text(prop, "Wave Type", "");

	prop= RNA_def_property(srna, "wave_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -2.0, 2.0);
	RNA_def_property_ui_range(prop, -1.0, 1.0, 5, 2);
	RNA_def_property_ui_text(prop, "Factor", "Multiplier for wave influence of this brush");

	prop= RNA_def_property(srna, "wave_clamp", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.00, 50.0);
	RNA_def_property_ui_range(prop, 0.00, 5.0, 1, 2);
	RNA_def_property_ui_text(prop, "Clamp Waves", "Maximum level of surface intersection used to influence waves (use 0.0 to disable)");

	prop= RNA_def_property(srna, "use_smudge", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_DO_SMUDGE);
	RNA_def_property_ui_text(prop, "Do Smudge", "Make this brush to smudge existing paint as it moves");

	prop= RNA_def_property(srna, "smudge_strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 5, 2);
	RNA_def_property_ui_text(prop, "Smudge Strength", "Smudge effect strength");

	prop= RNA_def_property(srna, "velocity_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max_velocity");
	RNA_def_property_range(prop, 0.0001, 10.0);
	RNA_def_property_ui_range(prop, 0.1, 2.0, 5, 2);
	RNA_def_property_ui_text(prop, "Max Velocity", "Velocity considered as maximum influence (Blender units per frame)");

	prop= RNA_def_property(srna, "use_velocity_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_VELOCITY_ALPHA);
	RNA_def_property_ui_text(prop, "Multiply Alpha", "Multiply brush influence by velocity color ramp alpha");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");

	prop= RNA_def_property(srna, "use_velocity_depth", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_VELOCITY_DEPTH);
	RNA_def_property_ui_text(prop, "Multiply Depth", "Multiply brush intersection depth (displace, waves) by velocity ramp alpha");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");

	prop= RNA_def_property(srna, "use_velocity_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_VELOCITY_COLOR);
	RNA_def_property_ui_text(prop, "Replace Color", "Replace brush color by velocity color ramp");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");
	
	/*
	*   Paint Area / Collision
	*/
	prop= RNA_def_property(srna, "paint_source", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_sdna(prop, NULL, "collision");
	RNA_def_property_enum_items(prop, prop_dynamicpaint_collisiontype);
	RNA_def_property_ui_text(prop, "Paint Source", "");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");
	
	prop= RNA_def_property(srna, "paint_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "paint_distance");
	RNA_def_property_range(prop, 0.0, 500.0);
	RNA_def_property_ui_range(prop, 0.0, 500.0, 10, 3);
	RNA_def_property_ui_text(prop, "Proximity Distance", "Maximum distance from brush to mesh surface to affect paint");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");
	
	prop= RNA_def_property(srna, "use_proximity_ramp_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_RAMP_ALPHA);
	RNA_def_property_ui_text(prop, "Only Use Alpha", "Only read color ramp alpha");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");
	
	prop= RNA_def_property(srna, "proximity_falloff", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_sdna(prop, NULL, "proximity_falloff");
	RNA_def_property_enum_items(prop, prop_dynamicpaint_prox_falloff);
	RNA_def_property_ui_text(prop, "Falloff", "Proximity falloff type");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");
	
	prop= RNA_def_property(srna, "use_proximity_project", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_PROX_PROJECT);
	RNA_def_property_ui_text(prop, "Project", "Brush is projected to canvas from defined direction within brush proximity");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");

	prop= RNA_def_property(srna, "ray_direction", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ray_dir");
	RNA_def_property_enum_items(prop, prop_dynamicpaint_brush_ray_dir);
	RNA_def_property_ui_text(prop, "Ray Direction", "Ray direction to use for projection (if brush object is located in that direction it's painted)");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");

	prop= RNA_def_property(srna, "invert_proximity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_INVERSE_PROX);
	RNA_def_property_ui_text(prop, "Inner Proximity", "Proximity falloff is applied inside the volume");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");

	prop= RNA_def_property(srna, "use_negative_volume", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_NEGATE_VOLUME);
	RNA_def_property_ui_text(prop, "Negate Volume", "Negate influence inside the volume");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");
	

	/*
	*   Particle
	*/
	prop= RNA_def_property(srna, "particle_system", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "psys");
	RNA_def_property_struct_type(prop, "ParticleSystem");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Particle Systems", "The particle system to paint with");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_resetDependancy");

	
	prop= RNA_def_property(srna, "use_particle_radius", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_PART_RAD);
	RNA_def_property_ui_text(prop, "Use Particle Radius", "Use radius from particle settings");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");
	
	prop= RNA_def_property(srna, "solid_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "particle_radius");
	RNA_def_property_range(prop, 0.01, 10.0);
	RNA_def_property_ui_range(prop, 0.01, 2.0, 5, 3);
	RNA_def_property_ui_text(prop, "Solid Radius", "Radius that will be painted solid");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");

	prop= RNA_def_property(srna, "smooth_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "particle_smooth");
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 5, 0);
	RNA_def_property_ui_text(prop, "Smooth Radius", "Smooth falloff added after solid radius");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");
	

	/*
	* Color ramps
	*/
	prop= RNA_def_property(srna, "paint_ramp", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "paint_ramp");
	RNA_def_property_struct_type(prop, "ColorRamp");
	RNA_def_property_ui_text(prop, "Paint Color Ramp", "Color ramp used to define proximity falloff");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");

	prop= RNA_def_property(srna, "velocity_ramp", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "vel_ramp");
	RNA_def_property_struct_type(prop, "ColorRamp");
	RNA_def_property_ui_text(prop, "Velocity Color Ramp", "Color ramp used to define brush velocity effect");
	RNA_def_property_update(prop, NC_OBJECT|ND_MODIFIER, "rna_DynamicPaint_redoModifier");
}

void RNA_def_dynamic_paint(BlenderRNA *brna)
{
	rna_def_dynamic_paint_canvas_settings(brna);
	rna_def_dynamic_paint_brush_settings(brna);
	rna_def_canvas_surface(brna);
}

#endif
