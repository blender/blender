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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_scene_types.h"

#include "BKE_paint.h"

#include "WM_types.h"

static EnumPropertyItem particle_edit_hair_brush_items[] = {
	{PE_BRUSH_NONE, "NONE", 0, "None", "Don't use any brush."},
	{PE_BRUSH_COMB, "COMB", 0, "Comb", "Comb hairs."},
	{PE_BRUSH_SMOOTH, "SMOOTH", 0, "Smooth", "Smooth hairs."},
	{PE_BRUSH_ADD, "ADD", 0, "Add", "Add hairs."},
	{PE_BRUSH_LENGTH, "LENGTH", 0, "Length", "Make hairs longer or shorter."},
	{PE_BRUSH_PUFF, "PUFF", 0, "Puff", "Make hairs stand up."},
	{PE_BRUSH_CUT, "CUT", 0, "Cut", "Cut hairs."},
	{0, NULL, 0, NULL, NULL}};

#ifdef RNA_RUNTIME

#include "BKE_context.h"
#include "BKE_pointcache.h"
#include "BKE_particle.h"
#include "BKE_depsgraph.h"

#include "ED_particle.h"

static EnumPropertyItem particle_edit_cache_brush_items[] = {
	{PE_BRUSH_NONE, "NONE", 0, "None", "Don't use any brush."},
	{PE_BRUSH_COMB, "COMB", 0, "Comb", "Comb paths."},
	{PE_BRUSH_SMOOTH, "SMOOTH", 0, "Smooth", "Smooth paths."},
	{PE_BRUSH_LENGTH, "LENGTH", 0, "Length", "Make paths longer or shorter."},
	{0, NULL, 0, NULL, NULL}};

static PointerRNA rna_ParticleEdit_brush_get(PointerRNA *ptr)
{
	ParticleEditSettings *pset= (ParticleEditSettings*)ptr->data;
	ParticleBrushData *brush= NULL;;

	if(pset->brushtype != PE_BRUSH_NONE)
		brush= &pset->brush[pset->brushtype];

	return rna_pointer_inherit_refine(ptr, &RNA_ParticleBrush, brush);
}

static PointerRNA rna_ParticleBrush_curve_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_CurveMapping, NULL);
}

static void rna_Paint_brushes_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Paint *p= (Paint*)ptr->data;
	rna_iterator_array_begin(iter, (void*)p->brushes, sizeof(Brush*), p->brush_count, 0, NULL);
}

static int rna_Paint_brushes_length(PointerRNA *ptr)
{
	Paint *p= (Paint*)ptr->data;

	return p->brush_count;
}

static PointerRNA rna_Paint_active_brush_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_Brush, paint_brush(ptr->data));
}

static void rna_Paint_active_brush_set(PointerRNA *ptr, PointerRNA value)
{
	paint_brush_set(ptr->data, value.data);
}

static void rna_ParticleEdit_redo(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Object *ob= (scene->basact)? scene->basact->object: NULL;
	PTCacheEdit *edit = PE_get_current(scene, ob);

	if(!edit)
		return;

	psys_free_path_cache(edit->psys, edit);
}

static void rna_ParticleEdit_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Object *ob= (scene->basact)? scene->basact->object: NULL;

	if(ob) DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
}

static EnumPropertyItem *rna_ParticleEdit_tool_itemf(bContext *C, PointerRNA *ptr, int *free)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= (scene->basact)? scene->basact->object: NULL;
	PTCacheEdit *edit = PE_get_current(scene, ob);
	
	if(edit && edit->psys)
		return particle_edit_hair_brush_items;

	return particle_edit_cache_brush_items;
}

static int rna_ParticleEdit_editable_get(PointerRNA *ptr)
{
	ParticleEditSettings *pset= (ParticleEditSettings*)ptr->data;

	return (pset->object && PE_get_current(pset->scene, pset->object));
}
static int rna_ParticleEdit_hair_get(PointerRNA *ptr)
{
	ParticleEditSettings *pset= (ParticleEditSettings*)ptr->data;

	PTCacheEdit *edit = PE_get_current(pset->scene, pset->object);

	return (edit && edit->psys);
}

static void rna_Paint_active_brush_index_set(PointerRNA *ptr, int value)
{
	Paint *p= ptr->data;
	CLAMP(value, 0, p->brush_count-1);
	p->active_brush_index= value;
}

static void rna_Paint_active_brush_index_range(PointerRNA *ptr, int *min, int *max)
{
	Paint *p= ptr->data;
	*min= 0;
	*max= MAX2(p->brush_count-1, 0);
}

#else

static void rna_def_paint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "Paint", NULL);
	RNA_def_struct_ui_text(srna, "Paint", "");

	prop= RNA_def_property(srna, "brushes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Brush");
	RNA_def_property_collection_funcs(prop, "rna_Paint_brushes_begin",
					  "rna_iterator_array_next",
					  "rna_iterator_array_end",
					  "rna_iterator_array_dereference_get", 
					  "rna_Paint_brushes_length", 0, 0);
	RNA_def_property_ui_text(prop, "Brushes", "Brushes selected for this paint mode.");

	prop= RNA_def_property(srna, "active_brush_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_funcs(prop, NULL, "rna_Paint_active_brush_index_set", "rna_Paint_active_brush_index_range");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_update(prop, NC_BRUSH|NA_EDITED, NULL);

	/* Fake property to get active brush directly, rather than integer index */
	prop= RNA_def_property(srna, "brush", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Brush");
	RNA_def_property_pointer_funcs(prop, "rna_Paint_active_brush_get", "rna_Paint_active_brush_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Brush", "Active paint brush.");
	RNA_def_property_update(prop, NC_BRUSH|NA_EDITED, NULL);

	prop= RNA_def_property(srna, "show_brush", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", PAINT_SHOW_BRUSH);
	RNA_def_property_ui_text(prop, "Show Brush", "");

	prop= RNA_def_property(srna, "fast_navigate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", PAINT_FAST_NAVIGATE);
	RNA_def_property_ui_text(prop, "Fast Navigate", "For multires, show low resolution while navigating the view.");
}

static void rna_def_sculpt(BlenderRNA  *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "Sculpt", "Paint");
	RNA_def_struct_ui_text(srna, "Sculpt", "");
	
	prop= RNA_def_property(srna, "symmetry_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SCULPT_SYMM_X);
	RNA_def_property_ui_text(prop, "Symmetry X", "Mirror brush across the X axis.");

	prop= RNA_def_property(srna, "symmetry_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SCULPT_SYMM_Y);
	RNA_def_property_ui_text(prop, "Symmetry Y", "Mirror brush across the Y axis.");

	prop= RNA_def_property(srna, "symmetry_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SCULPT_SYMM_Z);
	RNA_def_property_ui_text(prop, "Symmetry Z", "Mirror brush across the Z axis.");

	prop= RNA_def_property(srna, "lock_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SCULPT_LOCK_X);
	RNA_def_property_ui_text(prop, "Lock X", "Disallow changes to the X axis of vertices.");

	prop= RNA_def_property(srna, "lock_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SCULPT_LOCK_Y);
	RNA_def_property_ui_text(prop, "Lock Y", "Disallow changes to the Y axis of vertices.");

	prop= RNA_def_property(srna, "lock_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", SCULPT_LOCK_Z);
	RNA_def_property_ui_text(prop, "Lock Z", "Disallow changes to the Z axis of vertices.");
}

static void rna_def_vertex_paint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem brush_mode_items[] = {
		{0, "MIX", 0, "Mix", "Use mix blending mode while painting."},
		{1, "ADD", 0, "Add", "Use add blending mode while painting."},
		{2, "SUB", 0, "Subtract", "Use subtract blending mode while painting."},
		{3, "MUL", 0, "Multiply", "Use multiply blending mode while painting."},
		{4, "BLUR", 0, "Blur", "Blur the color with surrounding values"},
		{5, "LIGHTEN", 0, "Lighten", "Use lighten blending mode while painting."},
		{6, "DARKEN", 0, "Darken", "Use darken blending mode while painting."},
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "VertexPaint", "Paint");
	RNA_def_struct_sdna(srna, "VPaint");
	RNA_def_struct_ui_text(srna, "Vertex Paint", "Properties of vertex and weight paint mode.");
    
	prop= RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, brush_mode_items);
	RNA_def_property_ui_text(prop, "Brush Mode", "Mode in which color is painted.");
	
	prop= RNA_def_property(srna, "all_faces", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", VP_AREA);
	RNA_def_property_ui_text(prop, "All Faces", "Paint on all faces inside brush.");
		
	prop= RNA_def_property(srna, "normals", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", VP_NORMALS);
	RNA_def_property_ui_text(prop, "Normals", "Applies the vertex normal before painting.");
	
	prop= RNA_def_property(srna, "spray", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", VP_SPRAY);
	RNA_def_property_ui_text(prop, "Spray", "Keep applying paint effect while holding mouse.");
}

static void rna_def_image_paint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem tool_items[] = {
		{PAINT_TOOL_DRAW, "DRAW", 0, "Draw", ""},
		{PAINT_TOOL_SOFTEN, "SOFTEN", 0, "Soften", ""},
		{PAINT_TOOL_SMEAR, "SMEAR", 0, "Smear", ""},
		{PAINT_TOOL_CLONE, "CLONE", 0, "Clone", ""},
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "ImagePaint", "Paint");
	RNA_def_struct_sdna(srna, "ImagePaintSettings");
	RNA_def_struct_ui_text(srna, "Image Paint", "Properties of image and texture painting mode.");
 
	prop= RNA_def_property(srna, "tool", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, tool_items);
	RNA_def_property_ui_text(prop, "Tool", "");

	/* booleans */

	prop= RNA_def_property(srna, "show_brush_draw", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", IMAGEPAINT_DRAW_TOOL);
	RNA_def_property_ui_text(prop, "Show Brush Draw", "Enables brush shape while drawing.");

	prop= RNA_def_property(srna, "show_brush", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", IMAGEPAINT_DRAW_TOOL_DRAWING);
	RNA_def_property_ui_text(prop, "Show Brush", "Enables brush shape while not drawing.");
		
	prop= RNA_def_property(srna, "use_projection", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", IMAGEPAINT_PROJECT_DISABLE);
	RNA_def_property_ui_text(prop, "Project Paint", "Use projection painting for improved consistency in the brush strokes.");
	
	prop= RNA_def_property(srna, "use_occlude", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", IMAGEPAINT_PROJECT_XRAY);
	RNA_def_property_ui_text(prop, "Occlude", "Only paint onto the faces directly under the brush (slower)");
	
	prop= RNA_def_property(srna, "use_backface_cull", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", IMAGEPAINT_PROJECT_BACKFACE);
	RNA_def_property_ui_text(prop, "Cull", "Ignore faces pointing away from the view (faster)");
	
	prop= RNA_def_property(srna, "use_normal_falloff", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", IMAGEPAINT_PROJECT_FLAT);
	RNA_def_property_ui_text(prop, "Normal", "Paint most on faces pointing towards the view");
	
	prop= RNA_def_property(srna, "use_stencil_layer", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", IMAGEPAINT_PROJECT_LAYER_MASK);
	RNA_def_property_ui_text(prop, "Stencil Layer", "Set the mask layer from the UV layer buttons");
	
	prop= RNA_def_property(srna, "invert_stencil", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", IMAGEPAINT_PROJECT_LAYER_MASK_INV);
	RNA_def_property_ui_text(prop, "Invert", "Invert the stencil layer");
	
	prop= RNA_def_property(srna, "use_clone_layer", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", IMAGEPAINT_PROJECT_LAYER_CLONE);
	RNA_def_property_ui_text(prop, "Clone Layer", "Use another UV layer as clone source, otherwise use 3D the cursor as the source");
	
	/* integers */
	
	prop= RNA_def_property(srna, "seam_bleed", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_range(prop, 0, 8, 0, 0);
	RNA_def_property_ui_text(prop, "Bleed", "Extend paint beyond the faces UVs to reduce seams (in pixels, slower).");

	prop= RNA_def_property(srna, "normal_angle", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_range(prop, 0, 90);
	RNA_def_property_ui_text(prop, "Angle", "Paint most on faces pointing towards the view acording to this angle.");
}

static void rna_def_particle_edit(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem select_mode_items[] = {
		{SCE_SELECT_PATH, "PATH", ICON_PARTICLE_PATH, "Path", "Path edit mode"},
		{SCE_SELECT_POINT, "POINT", ICON_PARTICLE_POINT, "Point", "Point select mode"},
		{SCE_SELECT_END, "TIP", ICON_PARTICLE_TIP, "Tip", "Tip select mode"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem puff_mode[] = {
		{0, "ADD", 0, "Add", "Make hairs more puffy."},
		{1, "SUB", 0, "Sub", "Make hairs less puffy."},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem length_mode[] = {
		{0, "GROW", 0, "Grow", "Make hairs longer."},
		{1, "SHRINK", 0, "Shrink", "Make hairs shorter."},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem edit_type_items[]= {
		{PE_TYPE_PARTICLES, "PARTICLES", 0, "Particles", ""},
		{PE_TYPE_SOFTBODY, "SOFT_BODY", 0, "Soft body", ""},
		{PE_TYPE_CLOTH, "CLOTH", 0, "Cloth", ""},
		{0, NULL, 0, NULL, NULL}
	};


	/* edit */

	srna= RNA_def_struct(brna, "ParticleEdit", NULL);
	RNA_def_struct_sdna(srna, "ParticleEditSettings");
	RNA_def_struct_ui_text(srna, "Particle Edit", "Properties of particle editing mode.");

	prop= RNA_def_property(srna, "tool", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "brushtype");
	RNA_def_property_enum_items(prop, particle_edit_hair_brush_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_ParticleEdit_tool_itemf");
	RNA_def_property_ui_text(prop, "Tool", "");

	prop= RNA_def_property(srna, "selection_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "selectmode");
	RNA_def_property_enum_items(prop, select_mode_items);
	RNA_def_property_ui_text(prop, "Selection Mode", "Particle select and display mode.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_ParticleEdit_update");

	prop= RNA_def_property(srna, "keep_lengths", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PE_KEEP_LENGTHS);
	RNA_def_property_ui_text(prop, "Keep Lengths", "Keep path lengths constant.");

	prop= RNA_def_property(srna, "keep_root", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PE_LOCK_FIRST);
	RNA_def_property_ui_text(prop, "Keep Root", "Keep root keys unmodified.");

	prop= RNA_def_property(srna, "emitter_deflect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PE_DEFLECT_EMITTER);
	RNA_def_property_ui_text(prop, "Deflect Emitter", "Keep paths from intersecting the emitter.");

	prop= RNA_def_property(srna, "emitter_distance", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "emitterdist");
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 10, 3);
	RNA_def_property_ui_text(prop, "Emitter Distance", "Distance to keep particles away from the emitter.");

	prop= RNA_def_property(srna, "fade_time", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PE_FADE_TIME);
	RNA_def_property_ui_text(prop, "Fade Time", "Fade paths and keys further away from current frame.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_ParticleEdit_update");

	prop= RNA_def_property(srna, "auto_velocity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PE_AUTO_VELOCITY);
	RNA_def_property_ui_text(prop, "Auto Velocity", "Calculate point velocities automatically.");

	prop= RNA_def_property(srna, "draw_particles", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PE_DRAW_PART);
	RNA_def_property_ui_text(prop, "Draw Particles", "Draw actual particles.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

	prop= RNA_def_property(srna, "mirror_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PE_X_MIRROR);
	RNA_def_property_ui_text(prop, "X-Axis Mirror", "Mirror operations over the X axis while editing.");

	prop= RNA_def_property(srna, "add_interpolate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PE_INTERPOLATE_ADDED);
	RNA_def_property_ui_text(prop, "Interpolate", "Interpolate new particles from the existing ones.");

	prop= RNA_def_property(srna, "add_keys", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "totaddkey");
	RNA_def_property_range(prop, 2, INT_MAX);
	RNA_def_property_ui_range(prop, 2, 20, 10, 3);
	RNA_def_property_ui_text(prop, "Keys", "How many keys to make new particles with.");

	prop= RNA_def_property(srna, "brush", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ParticleBrush");
	RNA_def_property_pointer_funcs(prop, "rna_ParticleEdit_brush_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Brush", "");

	prop= RNA_def_property(srna, "draw_step", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 2, 10);
	RNA_def_property_ui_text(prop, "Steps", "How many steps to draw the path with.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_ParticleEdit_redo");

	prop= RNA_def_property(srna, "fade_frames", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 2, 100);
	RNA_def_property_ui_text(prop, "Frames", "How many frames to fade.");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_ParticleEdit_update");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "edittype");
	RNA_def_property_enum_items(prop, edit_type_items);
	RNA_def_property_ui_text(prop, "Type", "");
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, "rna_ParticleEdit_redo");

	prop= RNA_def_property(srna, "editable", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_ParticleEdit_editable_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Editable", "A valid edit mode exists");

	prop= RNA_def_property(srna, "hair", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_ParticleEdit_hair_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Hair", "Editing hair");

	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "The edited object");


	/* brush */

	srna= RNA_def_struct(brna, "ParticleBrush", NULL);
	RNA_def_struct_sdna(srna, "ParticleBrushData");
	RNA_def_struct_ui_text(srna, "Particle Brush", "Particle editing brush.");

	prop= RNA_def_property(srna, "size", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 1, INT_MAX);
	RNA_def_property_ui_range(prop, 1, 100, 10, 3);
	RNA_def_property_ui_text(prop, "Size", "Brush size.");

	prop= RNA_def_property(srna, "strength", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 1, INT_MAX);
	RNA_def_property_ui_range(prop, 1, 100, 10, 3);
	RNA_def_property_ui_text(prop, "Strength", "Brush strength.");

	prop= RNA_def_property(srna, "steps", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "step");
	RNA_def_property_range(prop, 1, INT_MAX);
	RNA_def_property_ui_range(prop, 1, 50, 10, 3);
	RNA_def_property_ui_text(prop, "Steps", "Brush steps.");

	prop= RNA_def_property(srna, "puff_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "invert");
	RNA_def_property_enum_items(prop, puff_mode);
	RNA_def_property_ui_text(prop, "Puff Mode", "");

	prop= RNA_def_property(srna, "length_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "invert");
	RNA_def_property_enum_items(prop, length_mode);
	RNA_def_property_ui_text(prop, "Length Mode", "");

	/* dummy */
	prop= RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "CurveMapping");
	RNA_def_property_pointer_funcs(prop, "rna_ParticleBrush_curve_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Curve", "");
}

void RNA_def_sculpt_paint(BlenderRNA *brna)
{
	rna_def_paint(brna);
	rna_def_sculpt(brna);
	rna_def_vertex_paint(brna);
	rna_def_image_paint(brna);
	rna_def_particle_edit(brna);
}

#endif

