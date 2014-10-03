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
 * Contributor(s): Blender Foundation (2008), Juho Veps�l�inen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_brush.c
 *  \ingroup RNA
 */

#include <stdlib.h>
#include <assert.h>

#include "DNA_brush_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "IMB_imbuf.h"

#include "WM_types.h"

static EnumPropertyItem prop_direction_items[] = {
	{0, "ADD", 0, "Add", "Add effect of brush"},
	{BRUSH_DIR_IN, "SUBTRACT", 0, "Subtract", "Subtract effect of brush"},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem sculpt_stroke_method_items[] = {
	{0, "DOTS", 0, "Dots", "Apply paint on each mouse move step"},
	{BRUSH_DRAG_DOT, "DRAG_DOT", 0, "Drag Dot", "Allows a single dot to be carefully positioned"},
	{BRUSH_SPACE, "SPACE", 0, "Space", "Limit brush application to the distance specified by spacing"},
	{BRUSH_AIRBRUSH, "AIRBRUSH", 0, "Airbrush", "Keep applying paint effect while holding mouse (spray)"},
	{BRUSH_ANCHORED, "ANCHORED", 0, "Anchored", "Keep the brush anchored to the initial location"},
	{BRUSH_LINE, "LINE", 0, "Line", "Draw a line with dabs separated according to spacing"},
	{BRUSH_CURVE, "CURVE", 0, "Curve",
	              "Define the stroke curve with a bezier curve (dabs are separated according to spacing)"},
	{0, NULL, 0, NULL, NULL}
};


EnumPropertyItem brush_sculpt_tool_items[] = {
	{SCULPT_TOOL_BLOB, "BLOB", ICON_BRUSH_BLOB, "Blob", ""},
	{SCULPT_TOOL_CLAY, "CLAY", ICON_BRUSH_CLAY, "Clay", ""},
	{SCULPT_TOOL_CLAY_STRIPS, "CLAY_STRIPS", ICON_BRUSH_CLAY_STRIPS, "Clay Strips", ""},
	{SCULPT_TOOL_CREASE, "CREASE", ICON_BRUSH_CREASE, "Crease", ""},
	{SCULPT_TOOL_DRAW, "DRAW", ICON_BRUSH_SCULPT_DRAW, "Draw", ""},
	{SCULPT_TOOL_FILL, "FILL", ICON_BRUSH_FILL, "Fill", ""},
	{SCULPT_TOOL_FLATTEN, "FLATTEN", ICON_BRUSH_FLATTEN, "Flatten", ""},
	{SCULPT_TOOL_GRAB, "GRAB", ICON_BRUSH_GRAB, "Grab", ""},
	{SCULPT_TOOL_INFLATE, "INFLATE", ICON_BRUSH_INFLATE, "Inflate", ""},
	{SCULPT_TOOL_LAYER, "LAYER", ICON_BRUSH_LAYER, "Layer", ""},
	{SCULPT_TOOL_MASK, "MASK", ICON_BRUSH_MASK, "Mask", ""},
	{SCULPT_TOOL_NUDGE, "NUDGE", ICON_BRUSH_NUDGE, "Nudge", ""},
	{SCULPT_TOOL_PINCH, "PINCH", ICON_BRUSH_PINCH, "Pinch", ""},
	{SCULPT_TOOL_ROTATE, "ROTATE", ICON_BRUSH_ROTATE, "Rotate", ""},
	{SCULPT_TOOL_SCRAPE, "SCRAPE", ICON_BRUSH_SCRAPE, "Scrape", ""},
	{SCULPT_TOOL_SIMPLIFY, "SIMPLIFY", ICON_BRUSH_SUBTRACT /* icon TODO */, "Simplify", ""},
	{SCULPT_TOOL_SMOOTH, "SMOOTH", ICON_BRUSH_SMOOTH, "Smooth", ""},
	{SCULPT_TOOL_SNAKE_HOOK, "SNAKE_HOOK", ICON_BRUSH_SNAKE_HOOK, "Snake Hook", ""},
	{SCULPT_TOOL_THUMB, "THUMB", ICON_BRUSH_THUMB, "Thumb", ""},
	{0, NULL, 0, NULL, NULL}
};


EnumPropertyItem brush_vertex_tool_items[] = {
	{PAINT_BLEND_MIX, "MIX", ICON_BRUSH_MIX, "Mix", "Use mix blending mode while painting"},
	{PAINT_BLEND_ADD, "ADD", ICON_BRUSH_ADD, "Add", "Use add blending mode while painting"},
	{PAINT_BLEND_SUB, "SUB", ICON_BRUSH_SUBTRACT, "Subtract", "Use subtract blending mode while painting"},
	{PAINT_BLEND_MUL, "MUL", ICON_BRUSH_MULTIPLY, "Multiply", "Use multiply blending mode while painting"},
	{PAINT_BLEND_BLUR, "BLUR", ICON_BRUSH_BLUR, "Blur", "Blur the color with surrounding values"},
	{PAINT_BLEND_LIGHTEN, "LIGHTEN", ICON_BRUSH_LIGHTEN, "Lighten", "Use lighten blending mode while painting"},
	{PAINT_BLEND_DARKEN, "DARKEN", ICON_BRUSH_DARKEN, "Darken", "Use darken blending mode while painting"},
	{0, NULL, 0, NULL, NULL}
};
	
EnumPropertyItem brush_image_tool_items[] = {
	{PAINT_TOOL_DRAW, "DRAW", ICON_BRUSH_TEXDRAW, "Draw", ""},
	{PAINT_TOOL_SOFTEN, "SOFTEN", ICON_BRUSH_SOFTEN, "Soften", ""},
	{PAINT_TOOL_SMEAR, "SMEAR", ICON_BRUSH_SMEAR, "Smear", ""},
	{PAINT_TOOL_CLONE, "CLONE", ICON_BRUSH_CLONE, "Clone", ""},
	{PAINT_TOOL_FILL, "FILL", ICON_BRUSH_TEXFILL, "Fill", ""},
	{PAINT_TOOL_MASK, "MASK", ICON_BRUSH_TEXMASK, "Mask", ""},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "RNA_access.h"

#include "BKE_texture.h"
#include "BKE_brush.h"
#include "BKE_icons.h"
#include "BKE_paint.h"

#include "WM_api.h"

static int rna_SculptToolCapabilities_has_accumulate_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return ELEM(br->sculpt_tool,
	            SCULPT_TOOL_BLOB, SCULPT_TOOL_CLAY, SCULPT_TOOL_CREASE,
	            SCULPT_TOOL_DRAW, SCULPT_TOOL_INFLATE, SCULPT_TOOL_LAYER);
}

static int rna_SculptToolCapabilities_has_auto_smooth_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return !ELEM(br->sculpt_tool, SCULPT_TOOL_MASK, SCULPT_TOOL_SMOOTH);
}

static int rna_SculptToolCapabilities_has_height_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return br->sculpt_tool == SCULPT_TOOL_LAYER;
}

static int rna_SculptToolCapabilities_has_jitter_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return (!(br->flag & BRUSH_ANCHORED) &&
	        !(br->flag & BRUSH_DRAG_DOT) &&
	        !ELEM(br->sculpt_tool,
	              SCULPT_TOOL_GRAB, SCULPT_TOOL_ROTATE,
	              SCULPT_TOOL_SNAKE_HOOK, SCULPT_TOOL_THUMB));
}

static int rna_SculptToolCapabilities_has_normal_weight_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return ELEM(br->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_SNAKE_HOOK);
}

static int rna_BrushCapabilities_has_overlay_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return ELEM(br->mtex.brush_map_mode,
	            MTEX_MAP_MODE_VIEW,
	            MTEX_MAP_MODE_TILED,
	            MTEX_MAP_MODE_STENCIL);
}

static int rna_SculptToolCapabilities_has_persistence_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return br->sculpt_tool == SCULPT_TOOL_LAYER;
}

static int rna_SculptToolCapabilities_has_pinch_factor_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return ELEM(br->sculpt_tool, SCULPT_TOOL_BLOB, SCULPT_TOOL_CREASE);
}

static int rna_SculptToolCapabilities_has_plane_offset_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return ELEM(br->sculpt_tool, SCULPT_TOOL_CLAY, SCULPT_TOOL_CLAY_STRIPS,
	            SCULPT_TOOL_FILL, SCULPT_TOOL_FLATTEN, SCULPT_TOOL_SCRAPE);
}

static int rna_SculptToolCapabilities_has_random_texture_angle_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return (!ELEM(br->sculpt_tool,
	              SCULPT_TOOL_GRAB, SCULPT_TOOL_ROTATE,
	              SCULPT_TOOL_SNAKE_HOOK, SCULPT_TOOL_THUMB));
}

static int rna_BrushCapabilities_has_random_texture_angle_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return (ELEM(br->mtex.brush_map_mode,
	             MTEX_MAP_MODE_VIEW,
	             MTEX_MAP_MODE_AREA,
	             MTEX_MAP_MODE_RANDOM) &&
	        !(br->flag & BRUSH_ANCHORED));
}

static int rna_SculptToolCapabilities_has_sculpt_plane_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return !ELEM(br->sculpt_tool, SCULPT_TOOL_INFLATE,
	             SCULPT_TOOL_MASK, SCULPT_TOOL_PINCH,
	             SCULPT_TOOL_SMOOTH);
}

static int rna_SculptToolCapabilities_has_secondary_color_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return ELEM(br->sculpt_tool,
	            SCULPT_TOOL_BLOB, SCULPT_TOOL_CLAY, SCULPT_TOOL_CLAY_STRIPS,
	            SCULPT_TOOL_CREASE, SCULPT_TOOL_DRAW, SCULPT_TOOL_FILL,
	            SCULPT_TOOL_FLATTEN, SCULPT_TOOL_INFLATE, SCULPT_TOOL_PINCH,
	            SCULPT_TOOL_SCRAPE);
}

static int rna_SculptToolCapabilities_has_smooth_stroke_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return (!(br->flag & BRUSH_ANCHORED) &&
	        !(br->flag & BRUSH_DRAG_DOT) &&
	        !(br->flag & BRUSH_LINE) &&
	        !(br->flag & BRUSH_CURVE) &&
	        !ELEM(br->sculpt_tool,
	               SCULPT_TOOL_GRAB, SCULPT_TOOL_ROTATE,
	               SCULPT_TOOL_SNAKE_HOOK, SCULPT_TOOL_THUMB));
}

static int rna_BrushCapabilities_has_smooth_stroke_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return (!(br->flag & BRUSH_ANCHORED) &&
	        !(br->flag & BRUSH_DRAG_DOT) &&
	        !(br->flag & BRUSH_LINE) &&
	        !(br->flag & BRUSH_CURVE));
}

static int rna_SculptToolCapabilities_has_space_attenuation_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return ((br->flag & (BRUSH_SPACE | BRUSH_LINE | BRUSH_CURVE)) &&
	        !ELEM(br->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_ROTATE,
	               SCULPT_TOOL_SMOOTH, SCULPT_TOOL_SNAKE_HOOK));
}

static int rna_ImapaintToolCapabilities_has_space_attenuation_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return (br->flag & (BRUSH_SPACE | BRUSH_LINE | BRUSH_CURVE)) &&
	        br->imagepaint_tool != PAINT_TOOL_FILL;
}

static int rna_BrushCapabilities_has_spacing_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return (!(br->flag & BRUSH_ANCHORED));
}

static int rna_SculptToolCapabilities_has_strength_pressure_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return !ELEM(br->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_SNAKE_HOOK);
}

static int rna_BrushCapabilities_has_texture_angle_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return ELEM(br->mtex.brush_map_mode,
	            MTEX_MAP_MODE_VIEW,
	            MTEX_MAP_MODE_AREA,
	            MTEX_MAP_MODE_TILED,
	            MTEX_MAP_MODE_STENCIL,
	            MTEX_MAP_MODE_RANDOM);
}

static int rna_SculptToolCapabilities_has_gravity_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return !ELEM(br->sculpt_tool, SCULPT_TOOL_MASK, SCULPT_TOOL_SMOOTH);
}

static int rna_BrushCapabilities_has_texture_angle_source_get(PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	return ELEM(br->mtex.brush_map_mode,
	            MTEX_MAP_MODE_VIEW,
	            MTEX_MAP_MODE_AREA,
	            MTEX_MAP_MODE_RANDOM);
}

static int rna_ImapaintToolCapabilities_has_accumulate_get(PointerRNA *ptr)
{
	/* only support for draw tool */
	Brush *br = (Brush *)ptr->data;

	return ((br->flag & BRUSH_AIRBRUSH) ||
	        (br->flag & BRUSH_DRAG_DOT) ||
	        (br->flag & BRUSH_ANCHORED) ||
	        (br->imagepaint_tool == PAINT_TOOL_SOFTEN) ||
	        (br->imagepaint_tool == PAINT_TOOL_SMEAR) ||
	        (br->imagepaint_tool == PAINT_TOOL_FILL) ||
	        (br->mtex.tex && !ELEM(br->mtex.brush_map_mode, MTEX_MAP_MODE_TILED, MTEX_MAP_MODE_STENCIL, MTEX_MAP_MODE_3D))
	        ) ? false : true;
}

static int rna_ImapaintToolCapabilities_has_radius_get(PointerRNA *ptr)
{
	/* only support for draw tool */
	Brush *br = (Brush *)ptr->data;

	return (br->imagepaint_tool != PAINT_TOOL_FILL);
}


static PointerRNA rna_Sculpt_tool_capabilities_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_SculptToolCapabilities, ptr->id.data);
}

static PointerRNA rna_Imapaint_tool_capabilities_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_ImapaintToolCapabilities, ptr->id.data);
}

static PointerRNA rna_Brush_capabilities_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_BrushCapabilities, ptr->id.data);
}

static void rna_Brush_reset_icon(Brush *br, const char *UNUSED(type))
{
	ID *id = &br->id;

	if (br->flag & BRUSH_CUSTOM_ICON)
		return;

	if (id->icon_id >= BIFICONID_LAST) {
		BKE_icon_delete(id);
		BKE_previewimg_free_id(id);
	}

	id->icon_id = 0;
}

static void rna_Brush_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	WM_main_add_notifier(NC_BRUSH | NA_EDITED, br);
	/*WM_main_add_notifier(NC_SPACE|ND_SPACE_VIEW3D, NULL); */
}

static void rna_Brush_main_tex_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	BKE_paint_invalidate_overlay_tex(scene, br->mtex.tex);
	rna_Brush_update(bmain, scene, ptr);
}

static void rna_Brush_secondary_tex_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	BKE_paint_invalidate_overlay_tex(scene, br->mask_mtex.tex);
	rna_Brush_update(bmain, scene, ptr);
}

static void rna_Brush_size_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	BKE_paint_invalidate_overlay_all();
	rna_Brush_update(bmain, scene, ptr);
}

static void rna_Brush_sculpt_tool_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	rna_Brush_reset_icon(br, "sculpt");
	rna_Brush_update(bmain, scene, ptr);
}
 
static void rna_Brush_vertex_tool_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	rna_Brush_reset_icon(br, "vertex_paint");
	rna_Brush_update(bmain, scene, ptr);
}
 
static void rna_Brush_imagepaint_tool_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;
	rna_Brush_reset_icon(br, "image_paint");
	rna_Brush_update(bmain, scene, ptr);
}

static void rna_Brush_stroke_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, scene);
	rna_Brush_update(bmain, scene, ptr);
}

static void rna_Brush_icon_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Brush *br = (Brush *)ptr->data;

	if (br->icon_imbuf) {
		IMB_freeImBuf(br->icon_imbuf);
		br->icon_imbuf = NULL;
	}

	br->id.icon_id = 0;

	if (br->flag & BRUSH_CUSTOM_ICON) {
		BKE_previewimg_get(&br->id);
		BKE_icon_changed(BKE_icon_getid(&br->id));
	}

	WM_main_add_notifier(NC_BRUSH | NA_EDITED, br);
}

static void rna_TextureSlot_brush_angle_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	MTex *mtex = ptr->data;
	/* skip invalidation of overlay for stencil mode */
	if (mtex->mapping != MTEX_MAP_MODE_STENCIL)
		BKE_paint_invalidate_overlay_tex(scene, mtex->tex);

	rna_TextureSlot_update(bmain, scene, ptr);
}

static void rna_Brush_set_size(PointerRNA *ptr, int value)
{
	Brush *brush = ptr->data;

	/* scale unprojected radius so it stays consistent with brush size */
	BKE_brush_scale_unprojected_radius(&brush->unprojected_radius,
	                                   value, brush->size);
	brush->size = value;
}

static void rna_Brush_use_gradient_set(PointerRNA *ptr, int value)
{
	Brush *br = (Brush *)ptr->data;

	if (value) br->flag |= BRUSH_USE_GRADIENT;
	else br->flag &= ~BRUSH_USE_GRADIENT;

	if ((br->flag & BRUSH_USE_GRADIENT) && br->gradient == NULL)
		br->gradient = add_colorband(true);
}

static void rna_Brush_set_unprojected_radius(PointerRNA *ptr, float value)
{
	Brush *brush = ptr->data;

	/* scale brush size so it stays consistent with unprojected_radius */
	BKE_brush_scale_size(&brush->size, value, brush->unprojected_radius);
	brush->unprojected_radius = value;
}

static EnumPropertyItem *rna_Brush_direction_itemf(bContext *C, PointerRNA *ptr,
                                                   PropertyRNA *UNUSED(prop), bool *UNUSED(r_free))
{
	PaintMode mode = BKE_paintmode_get_active_from_context(C);

	static EnumPropertyItem prop_default_items[] = {
		{0, NULL, 0, NULL, NULL}
	};

	/* sculpt mode */
	static EnumPropertyItem prop_flatten_contrast_items[] = {
		{0, "FLATTEN", 0, "Flatten", "Add effect of brush"},
		{BRUSH_DIR_IN, "CONTRAST", 0, "Contrast", "Subtract effect of brush"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_fill_deepen_items[] = {
		{0, "FILL", 0, "Fill", "Add effect of brush"},
		{BRUSH_DIR_IN, "DEEPEN", 0, "Deepen", "Subtract effect of brush"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_scrape_peaks_items[] = {
		{0, "SCRAPE", 0, "Scrape", "Add effect of brush"},
		{BRUSH_DIR_IN, "PEAKS", 0, "Peaks", "Subtract effect of brush"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_pinch_magnify_items[] = {
		{0, "PINCH", 0, "Pinch", "Add effect of brush"},
		{BRUSH_DIR_IN, "MAGNIFY", 0, "Magnify", "Subtract effect of brush"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_inflate_deflate_items[] = {
		{0, "INFLATE", 0, "Inflate", "Add effect of brush"},
		{BRUSH_DIR_IN, "DEFLATE", 0, "Deflate", "Subtract effect of brush"},
		{0, NULL, 0, NULL, NULL}
	};

	/* texture paint mode */
	static EnumPropertyItem prop_soften_sharpen_items[] = {
		{0, "SOFTEN", 0, "Soften", "Blur effect of brush"},
		{BRUSH_DIR_IN, "SHARPEN", 0, "Sharpen", "Sharpen effect of brush"},
		{0, NULL, 0, NULL, NULL}
	};

	Brush *me = (Brush *)(ptr->data);

	switch (mode) {
		case PAINT_SCULPT:
			switch (me->sculpt_tool) {
				case SCULPT_TOOL_DRAW:
				case SCULPT_TOOL_CREASE:
				case SCULPT_TOOL_BLOB:
				case SCULPT_TOOL_LAYER:
				case SCULPT_TOOL_CLAY:
				case SCULPT_TOOL_CLAY_STRIPS:
					return prop_direction_items;

				case SCULPT_TOOL_MASK:
					switch ((BrushMaskTool)me->mask_tool) {
						case BRUSH_MASK_DRAW:
							return prop_direction_items;
							break;
						case BRUSH_MASK_SMOOTH:
							return prop_default_items;
							break;
					}

				case SCULPT_TOOL_FLATTEN:
					return prop_flatten_contrast_items;

				case SCULPT_TOOL_FILL:
					return prop_fill_deepen_items;

				case SCULPT_TOOL_SCRAPE:
					return prop_scrape_peaks_items;

				case SCULPT_TOOL_PINCH:
					return prop_pinch_magnify_items;

				case SCULPT_TOOL_INFLATE:
					return prop_inflate_deflate_items;

				default:
					return prop_default_items;
			}
			break;

		case PAINT_TEXTURE_2D:
		case PAINT_TEXTURE_PROJECTIVE:
			switch (me->imagepaint_tool) {
				case PAINT_TOOL_SOFTEN:
					return prop_soften_sharpen_items;

				default:
					return prop_default_items;
			}
			break;

		default:
			return prop_default_items;
	}
}

static EnumPropertyItem *rna_Brush_stroke_itemf(bContext *C, PointerRNA *UNUSED(ptr),
                                                PropertyRNA *UNUSED(prop), bool *UNUSED(r_free))
{
	PaintMode mode = BKE_paintmode_get_active_from_context(C);

	static EnumPropertyItem brush_stroke_method_items[] = {
		{0, "DOTS", 0, "Dots", "Apply paint on each mouse move step"},
		{BRUSH_SPACE, "SPACE", 0, "Space", "Limit brush application to the distance specified by spacing"},
		{BRUSH_AIRBRUSH, "AIRBRUSH", 0, "Airbrush", "Keep applying paint effect while holding mouse (spray)"},
		{BRUSH_LINE, "LINE", 0, "Line", "Drag a line with dabs separated according to spacing"},
		{BRUSH_CURVE, "CURVE", 0, "Curve", "Define the stroke curve with a bezier curve. Dabs are separated according to spacing"},
		{0, NULL, 0, NULL, NULL}
	};

	switch (mode) {
		case PAINT_SCULPT:
		case PAINT_TEXTURE_2D:
		case PAINT_TEXTURE_PROJECTIVE:
			return sculpt_stroke_method_items;

		default:
			return brush_stroke_method_items;
	}
}
#else

static void rna_def_brush_texture_slot(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_map_mode_items[] = {
		{MTEX_MAP_MODE_VIEW, "VIEW_PLANE", 0, "View Plane", ""},
		{MTEX_MAP_MODE_AREA, "AREA_PLANE", 0, "Area Plane", ""},
		{MTEX_MAP_MODE_TILED, "TILED", 0, "Tiled", ""},
		{MTEX_MAP_MODE_3D, "3D", 0, "3D", ""},
		{MTEX_MAP_MODE_RANDOM, "RANDOM", 0, "Random", ""},
		{MTEX_MAP_MODE_STENCIL, "STENCIL", 0, "Stencil", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_tex_paint_map_mode_items[] = {
		{MTEX_MAP_MODE_VIEW, "VIEW_PLANE", 0, "View Plane", ""},
		{MTEX_MAP_MODE_TILED, "TILED", 0, "Tiled", ""},
		{MTEX_MAP_MODE_3D, "3D", 0, "3D", ""},
		{MTEX_MAP_MODE_RANDOM, "RANDOM", 0, "Random", ""},
		{MTEX_MAP_MODE_STENCIL, "STENCIL", 0, "Stencil", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_mask_paint_map_mode_items[] = {
		{MTEX_MAP_MODE_VIEW, "VIEW_PLANE", 0, "View Plane", ""},
		{MTEX_MAP_MODE_TILED, "TILED", 0, "Tiled", ""},
		{MTEX_MAP_MODE_RANDOM, "RANDOM", 0, "Random", ""},
		{MTEX_MAP_MODE_STENCIL, "STENCIL", 0, "Stencil", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "BrushTextureSlot", "TextureSlot");
	RNA_def_struct_sdna(srna, "MTex");
	RNA_def_struct_ui_text(srna, "Brush Texture Slot", "Texture slot for textures in a Brush datablock");

	prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "rot");
	RNA_def_property_range(prop, 0, M_PI * 2);
	RNA_def_property_ui_text(prop, "Angle", "Brush texture rotation");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_brush_angle_update");

	prop = RNA_def_property(srna, "map_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "brush_map_mode");
	RNA_def_property_enum_items(prop, prop_map_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_brush_update");

	prop = RNA_def_property(srna, "tex_paint_map_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "brush_map_mode");
	RNA_def_property_enum_items(prop, prop_tex_paint_map_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_brush_update");

	prop = RNA_def_property(srna, "mask_map_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "brush_map_mode");
	RNA_def_property_enum_items(prop, prop_mask_paint_map_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, 0, "rna_TextureSlot_brush_update");
}

static void rna_def_sculpt_capabilities(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "SculptToolCapabilities", NULL);
	RNA_def_struct_sdna(srna, "Brush");
	RNA_def_struct_nested(brna, srna, "Brush");
	RNA_def_struct_ui_text(srna, "Sculpt Capabilities",
	                       "Read-only indications of which brush operations "
	                       "are supported by the current sculpt tool");

#define SCULPT_TOOL_CAPABILITY(prop_name_, ui_name_)                      \
	prop = RNA_def_property(srna, #prop_name_,                          \
	                        PROP_BOOLEAN, PROP_NONE);                   \
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);                   \
	RNA_def_property_boolean_funcs(prop, "rna_SculptToolCapabilities_"      \
	                               #prop_name_ "_get", NULL);           \
	RNA_def_property_ui_text(prop, ui_name_, NULL)

	SCULPT_TOOL_CAPABILITY(has_accumulate, "Has Accumulate");
	SCULPT_TOOL_CAPABILITY(has_auto_smooth, "Has Auto Smooth");
	SCULPT_TOOL_CAPABILITY(has_height, "Has Height");
	SCULPT_TOOL_CAPABILITY(has_jitter, "Has Jitter");
	SCULPT_TOOL_CAPABILITY(has_normal_weight, "Has Crease/Pinch Factor");
	SCULPT_TOOL_CAPABILITY(has_persistence, "Has Persistence");
	SCULPT_TOOL_CAPABILITY(has_pinch_factor, "Has Pinch Factor");
	SCULPT_TOOL_CAPABILITY(has_plane_offset, "Has Plane Offset");
	SCULPT_TOOL_CAPABILITY(has_random_texture_angle, "Has Random Texture Angle");
	SCULPT_TOOL_CAPABILITY(has_sculpt_plane, "Has Sculpt Plane");
	SCULPT_TOOL_CAPABILITY(has_secondary_color, "Has Secondary Color");
	SCULPT_TOOL_CAPABILITY(has_smooth_stroke, "Has Smooth Stroke");
	SCULPT_TOOL_CAPABILITY(has_space_attenuation, "Has Space Attenuation");
	SCULPT_TOOL_CAPABILITY(has_strength_pressure, "Has Strength Pressure");
	SCULPT_TOOL_CAPABILITY(has_gravity, "Has Gravity");

#undef SCULPT_CAPABILITY
}

static void rna_def_brush_capabilities(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "BrushCapabilities", NULL);
	RNA_def_struct_sdna(srna, "Brush");
	RNA_def_struct_nested(brna, srna, "Brush");
	RNA_def_struct_ui_text(srna, "Brush Capabilities",
	                       "Read-only indications of which brush operations "
	                       "are supported by the current brush");

#define BRUSH_CAPABILITY(prop_name_, ui_name_)                          \
	prop = RNA_def_property(srna, #prop_name_,                          \
	                        PROP_BOOLEAN, PROP_NONE);                   \
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);                   \
	RNA_def_property_boolean_funcs(prop, "rna_BrushCapabilities_"      \
	                               #prop_name_ "_get", NULL);           \
	RNA_def_property_ui_text(prop, ui_name_, NULL)

	BRUSH_CAPABILITY(has_overlay, "Has Overlay");
	BRUSH_CAPABILITY(has_random_texture_angle, "Has Random Texture Angle");
	BRUSH_CAPABILITY(has_texture_angle, "Has Texture Angle");
	BRUSH_CAPABILITY(has_texture_angle_source, "Has Texture Angle Source");
	BRUSH_CAPABILITY(has_spacing, "Has Spacing");
	BRUSH_CAPABILITY(has_smooth_stroke, "Has Smooth Stroke");


#undef BRUSH_CAPABILITY
}

static void rna_def_image_paint_capabilities(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ImapaintToolCapabilities", NULL);
	RNA_def_struct_sdna(srna, "Brush");
	RNA_def_struct_nested(brna, srna, "Brush");
	RNA_def_struct_ui_text(srna, "Image Paint Capabilities",
	                       "Read-only indications of which brush operations "
	                       "are supported by the current image paint brush");

#define IMAPAINT_TOOL_CAPABILITY(prop_name_, ui_name_)                       \
	prop = RNA_def_property(srna, #prop_name_,                               \
	                        PROP_BOOLEAN, PROP_NONE);                        \
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);                        \
	RNA_def_property_boolean_funcs(prop, "rna_ImapaintToolCapabilities_"     \
	                               #prop_name_ "_get", NULL);                \
	RNA_def_property_ui_text(prop, ui_name_, NULL)

	IMAPAINT_TOOL_CAPABILITY(has_accumulate, "Has Accumulate");
	IMAPAINT_TOOL_CAPABILITY(has_space_attenuation, "Has Space Attenuation");
	IMAPAINT_TOOL_CAPABILITY(has_radius, "Has Radius");

#undef IMAPAINT_TOOL_CAPABILITY
}

static void rna_def_brush(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_blend_items[] = {
		{IMB_BLEND_MIX, "MIX", 0, "Mix", "Use mix blending mode while painting"},
		{IMB_BLEND_ADD, "ADD", 0, "Add", "Use add blending mode while painting"},
		{IMB_BLEND_SUB, "SUB", 0, "Subtract", "Use subtract blending mode while painting"},
		{IMB_BLEND_MUL, "MUL", 0, "Multiply", "Use multiply blending mode while painting"},
		{IMB_BLEND_LIGHTEN, "LIGHTEN", 0, "Lighten", "Use lighten blending mode while painting"},
		{IMB_BLEND_DARKEN, "DARKEN", 0, "Darken", "Use darken blending mode while painting"},
		{IMB_BLEND_ERASE_ALPHA, "ERASE_ALPHA", 0, "Erase Alpha", "Erase alpha while painting"},
		{IMB_BLEND_ADD_ALPHA, "ADD_ALPHA", 0, "Add Alpha", "Add alpha while painting"},
		{IMB_BLEND_OVERLAY, "OVERLAY", 0, "Overlay", "Use overlay blending mode while painting"},
		{IMB_BLEND_HARDLIGHT, "HARDLIGHT", 0, "Hard light", "Use hard light blending mode while painting"},
		{IMB_BLEND_COLORBURN, "COLORBURN", 0, "Color burn", "Use color burn blending mode while painting"},
		{IMB_BLEND_LINEARBURN, "LINEARBURN", 0, "Linear burn", "Use linear burn blending mode while painting"},
		{IMB_BLEND_COLORDODGE, "COLORDODGE", 0, "Color dodge", "Use color dodge blending mode while painting"},
		{IMB_BLEND_SCREEN, "SCREEN", 0, "Screen", "Use screen blending mode while painting"},
		{IMB_BLEND_SOFTLIGHT, "SOFTLIGHT", 0, "Soft light", "Use softlight blending mode while painting"},
		{IMB_BLEND_PINLIGHT, "PINLIGHT", 0, "Pin light", "Use pinlight blending mode while painting"},
		{IMB_BLEND_VIVIDLIGHT, "VIVIDLIGHT", 0, "Vivid light", "Use vividlight blending mode while painting"},
		{IMB_BLEND_LINEARLIGHT, "LINEARLIGHT", 0, "Linear light", "Use linearlight blending mode while painting"},
		{IMB_BLEND_DIFFERENCE, "DIFFERENCE", 0, "Difference", "Use difference blending mode while painting"},
		{IMB_BLEND_EXCLUSION, "EXCLUSION", 0, "Exclusion", "Use exclusion blending mode while painting"},
		{IMB_BLEND_HUE, "HUE", 0, "Hue", "Use hue blending mode while painting"},
		{IMB_BLEND_SATURATION, "SATURATION", 0, "Saturation", "Use saturation blending mode while painting"},
		{IMB_BLEND_LUMINOSITY, "LUMINOSITY", 0, "Luminosity", "Use luminosity blending mode while painting"},
		{IMB_BLEND_COLOR, "COLOR", 0, "Color", "Use color blending mode while painting"},
		{0, NULL, 0, NULL, NULL}
	};
	
	static EnumPropertyItem texture_angle_source_items[] = {
		{0, "USER", 0, "User", "Rotate the brush texture by given angle"},
		{BRUSH_RAKE, "RAKE", 0, "Rake", "Rotate the brush texture to match the stroke direction"},
		{BRUSH_RANDOM_ROTATION, "RANDOM", 0, "Random", "Rotate the brush texture at random"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem texture_angle_source_no_random_items[] = {
		{0, "USER", 0, "User", "Rotate the brush texture by given angle"},
		{BRUSH_RAKE, "RAKE", 0, "Rake", "Rotate the brush texture to match the stroke direction"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem brush_sculpt_plane_items[] = {
		{SCULPT_DISP_DIR_AREA, "AREA", 0, "Area Plane", ""},
		{SCULPT_DISP_DIR_VIEW, "VIEW", 0, "View Plane", ""},
		{SCULPT_DISP_DIR_X, "X", 0, "X Plane", ""},
		{SCULPT_DISP_DIR_Y, "Y", 0, "Y Plane", ""},
		{SCULPT_DISP_DIR_Z, "Z", 0, "Z Plane", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem brush_mask_tool_items[] = {
		{BRUSH_MASK_DRAW, "DRAW", 0, "Draw", ""},
		{BRUSH_MASK_SMOOTH, "SMOOTH", 0, "Smooth", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem brush_blur_mode_items[] = {
		{KERNEL_BOX, "BOX", 0, "Box", ""},
		{KERNEL_GAUSSIAN, "GAUSSIAN", 0, "Gaussian", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem brush_gradient_items[] = {
		{BRUSH_GRADIENT_PRESSURE, "PRESSURE", 0, "Pressure", ""},
		{BRUSH_GRADIENT_SPACING_REPEAT, "SPACING_REPEAT", 0, "Repeat", ""},
		{BRUSH_GRADIENT_SPACING_CLAMP, "SPACING_CLAMP", 0, "Clamp", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem brush_gradient_fill_items[] = {
		{BRUSH_GRADIENT_LINEAR, "LINEAR", 0, "Linear", ""},
		{BRUSH_GRADIENT_RADIAL, "RADIAL", 0, "Radial", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem brush_mask_pressure_items[] = {
		{0, "NONE", 0, "Off", ""},
		{BRUSH_MASK_PRESSURE_RAMP, "RAMP", ICON_STYLUS_PRESSURE, "Ramp", ""},
		{BRUSH_MASK_PRESSURE_CUTOFF, "CUTOFF", ICON_STYLUS_PRESSURE, "Cutoff", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "Brush", "ID");
	RNA_def_struct_ui_text(srna, "Brush", "Brush datablock for storing brush settings for painting and sculpting");
	RNA_def_struct_ui_icon(srna, ICON_BRUSH_DATA);

	/* enums */
	prop = RNA_def_property(srna, "blend", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_blend_items);
	RNA_def_property_ui_text(prop, "Blending mode", "Brush blending mode");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "sculpt_tool", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, brush_sculpt_tool_items);
	RNA_def_property_ui_text(prop, "Sculpt Tool", "");
	RNA_def_property_update(prop, 0, "rna_Brush_sculpt_tool_update");

	prop = RNA_def_property(srna, "vertex_tool", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "vertexpaint_tool");
	RNA_def_property_enum_items(prop, brush_vertex_tool_items);
	RNA_def_property_ui_text(prop, "Blending mode", "Brush blending mode");
	RNA_def_property_update(prop, 0, "rna_Brush_vertex_tool_update");
	
	prop = RNA_def_property(srna, "image_tool", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "imagepaint_tool");
	RNA_def_property_enum_items(prop, brush_image_tool_items);
	RNA_def_property_ui_text(prop, "Image Paint Tool", "");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_Brush_imagepaint_tool_update");

	prop = RNA_def_property(srna, "direction", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, prop_direction_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Brush_direction_itemf");
	RNA_def_property_ui_text(prop, "Direction", "");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "stroke_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, sculpt_stroke_method_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Brush_stroke_itemf");
	RNA_def_property_ui_text(prop, "Stroke Method", "");
	RNA_def_property_update(prop, 0, "rna_Brush_stroke_update");

	prop = RNA_def_property(srna, "texture_angle_source_random", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, texture_angle_source_items);
	RNA_def_property_ui_text(prop, "Texture Angle Source", "");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "texture_angle_source_no_random", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, texture_angle_source_no_random_items);
	RNA_def_property_ui_text(prop, "Texture Angle Source", "");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "sculpt_plane", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, brush_sculpt_plane_items);
	RNA_def_property_ui_text(prop, "Sculpt Plane", "");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "mask_tool", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, brush_mask_tool_items);
	RNA_def_property_ui_text(prop, "Mask Tool", "");
	RNA_def_property_update(prop, 0, "rna_Brush_update");
	
	/* number values */
	prop = RNA_def_property(srna, "size", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_funcs(prop, NULL, "rna_Brush_set_size", NULL);
	RNA_def_property_range(prop, 1, MAX_BRUSH_PIXEL_RADIUS * 10);
	RNA_def_property_ui_range(prop, 1, MAX_BRUSH_PIXEL_RADIUS, 1, -1);
	RNA_def_property_ui_text(prop, "Radius", "Radius of the brush in pixels");
	RNA_def_property_update(prop, 0, "rna_Brush_size_update");
	
	prop = RNA_def_property(srna, "unprojected_radius", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_funcs(prop, NULL, "rna_Brush_set_unprojected_radius", NULL);
	RNA_def_property_range(prop, 0.001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.001, 1, 0, -1);
	RNA_def_property_ui_text(prop, "Unprojected Radius", "Radius of brush in Blender units");
	RNA_def_property_update(prop, 0, "rna_Brush_size_update");

	prop = RNA_def_property(srna, "jitter", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "jitter");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_range(prop, 0.0f, 2.0f, 0.1, 4);
	RNA_def_property_ui_text(prop, "Jitter", "Jitter the position of the brush while painting");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "jitter_absolute", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "jitter_absolute");
	RNA_def_property_range(prop, 0, 1000000);
	RNA_def_property_ui_text(prop, "Jitter", "Jitter the position of the brush in pixels while painting");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "spacing", PROP_INT, PROP_PERCENTAGE);
	RNA_def_property_int_sdna(prop, NULL, "spacing");
	RNA_def_property_range(prop, 1, 1000);
	RNA_def_property_ui_range(prop, 1, 500, 5, -1);
	RNA_def_property_ui_text(prop, "Spacing", "Spacing between brush daubs as a percentage of brush diameter");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "grad_spacing", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "gradient_spacing");
	RNA_def_property_range(prop, 1, 10000);
	RNA_def_property_ui_range(prop, 1, 10000, 5, -1);
	RNA_def_property_ui_text(prop, "Gradient Spacing", "Spacing before brush gradient goes full circle");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "smooth_stroke_radius", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 10, 200);
	RNA_def_property_ui_text(prop, "Smooth Stroke Radius", "Minimum distance from last point before stroke continues");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "smooth_stroke_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.5, 0.99);
	RNA_def_property_ui_text(prop, "Smooth Stroke Factor", "Higher values give a smoother stroke");
	RNA_def_property_update(prop, 0, "rna_Brush_update");
	
	prop = RNA_def_property(srna, "rate", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rate");
	RNA_def_property_range(prop, 0.0001f, 10000.0f);
	RNA_def_property_ui_range(prop, 0.01f, 1.0f, 1, 3);
	RNA_def_property_ui_text(prop, "Rate", "Interval between paints for Airbrush");
	RNA_def_property_update(prop, 0, "rna_Brush_update");
	
	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_float_sdna(prop, NULL, "rgb");
	RNA_def_property_ui_text(prop, "Color", "");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "secondary_color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_float_sdna(prop, NULL, "secondary_rgb");
	RNA_def_property_ui_text(prop, "Secondary Color", "");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
	RNA_def_property_ui_text(prop, "Weight", "Vertex weight when brush is applied");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "alpha");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
	RNA_def_property_ui_text(prop, "Strength", "How powerful the effect of the brush is when applied");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "plane_offset", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "plane_offset");
	RNA_def_property_float_default(prop, 0);
	RNA_def_property_range(prop, -2.0f, 2.0f);
	RNA_def_property_ui_range(prop, -0.5f, 0.5f, 0.001, 3);
	RNA_def_property_ui_text(prop, "Plane Offset",
	                         "Adjust plane on which the brush acts towards or away from the object surface");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "plane_trim", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "plane_trim");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, 0, 1.0f);
	RNA_def_property_ui_text(prop, "Plane Trim",
	                         "If a vertex is further away from offset plane than this, then it is not affected");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "height", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "height");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, 0, 1.0f);
	RNA_def_property_ui_text(prop, "Brush Height", "Affectable height of brush (layer height for layer tool, i.e.)");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "texture_sample_bias", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "texture_sample_bias");
	RNA_def_property_float_default(prop, 0);
	RNA_def_property_range(prop, -1, 1);
	RNA_def_property_ui_text(prop, "Texture Sample Bias", "Value added to texture samples");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "normal_weight", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "normal_weight");
	RNA_def_property_float_default(prop, 0);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Normal Weight", "How much grab will pull vertexes out of surface during a grab");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "crease_pinch_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "crease_pinch_factor");
	RNA_def_property_float_default(prop, 2.0f / 3.0f);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Crease Brush Pinch Factor", "How much the crease brush pinches");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "auto_smooth_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "autosmooth_factor");
	RNA_def_property_float_default(prop, 0);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
	RNA_def_property_ui_text(prop, "Autosmooth", "Amount of smoothing to automatically apply to each stroke");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "stencil_pos", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "stencil_pos");
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_text(prop, "Stencil Position", "Position of stencil in viewport");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "stencil_dimension", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "stencil_dimension");
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_text(prop, "Stencil Dimensions", "Dimensions of stencil in viewport");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "mask_stencil_pos", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "mask_stencil_pos");
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_text(prop, "Mask Stencil Position", "Position of mask stencil in viewport");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "mask_stencil_dimension", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "mask_stencil_dimension");
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_text(prop, "Mask Stencil Dimensions", "Dimensions of mask stencil in viewport");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "sharp_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 100.0);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1, 3);
	RNA_def_property_float_sdna(prop, NULL, "sharp_threshold");
	RNA_def_property_ui_text(prop, "Sharp Threshold", "Threshold below which, no sharpening is done");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "fill_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 100.0);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1, 3);
	RNA_def_property_float_sdna(prop, NULL, "fill_threshold");
	RNA_def_property_ui_text(prop, "Fill Threshold", "Threshold above which filling is not propagated");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "blur_kernel_radius", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "blur_kernel_radius");
	RNA_def_property_range(prop, 1, 10000);
	RNA_def_property_ui_range(prop, 1, 50, 1, -1);
	RNA_def_property_ui_text(prop, "Kernel Radius", "Radius of kernel used for soften and sharpen in pixels");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "blur_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, brush_blur_mode_items);
	RNA_def_property_ui_text(prop, "Blur Mode", "");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	/* flag */
	prop = RNA_def_property(srna, "use_airbrush", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_AIRBRUSH);
	RNA_def_property_ui_text(prop, "Airbrush", "Keep applying paint effect while holding mouse (spray)");
	RNA_def_property_update(prop, 0, "rna_Brush_update");
	
	prop = RNA_def_property(srna, "use_original_normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_ORIGINAL_NORMAL);
	RNA_def_property_ui_icon(prop, ICON_UNLOCKED, true);
	RNA_def_property_ui_text(prop, "Original Normal",
	                         "When locked keep using normal of surface where stroke was initiated");
	RNA_def_property_update(prop, 0, "rna_Brush_update");
	
	prop = RNA_def_property(srna, "use_wrap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_TORUS);
	RNA_def_property_ui_text(prop, "Wrap", "Enable torus wrapping while painting");
	RNA_def_property_update(prop, 0, "rna_Brush_update");
	
	prop = RNA_def_property(srna, "use_pressure_strength", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_ALPHA_PRESSURE);
	RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
	RNA_def_property_ui_text(prop, "Strength Pressure", "Enable tablet pressure sensitivity for strength");
	RNA_def_property_update(prop, 0, "rna_Brush_update");
	
	prop = RNA_def_property(srna, "use_offset_pressure", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_OFFSET_PRESSURE);
	RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
	RNA_def_property_ui_text(prop, "Plane Offset Pressure", "Enable tablet pressure sensitivity for offset");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_pressure_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_SIZE_PRESSURE);
	RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
	RNA_def_property_ui_text(prop, "Size Pressure", "Enable tablet pressure sensitivity for size");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_gradient", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_USE_GRADIENT);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Brush_use_gradient_set");
	RNA_def_property_ui_text(prop, "Use Gradient", "Use Gradient by utilizing a sampling method");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_pressure_jitter", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_JITTER_PRESSURE);
	RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
	RNA_def_property_ui_text(prop, "Jitter Pressure", "Enable tablet pressure sensitivity for jitter");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_pressure_spacing", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_SPACING_PRESSURE);
	RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
	RNA_def_property_ui_text(prop, "Spacing Pressure", "Enable tablet pressure sensitivity for spacing");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_pressure_masking", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mask_pressure");
	RNA_def_property_enum_items(prop, brush_mask_pressure_items);
	RNA_def_property_ui_text(prop, "Mask Pressure Mode", "Pen pressure makes texture influence smaller");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_inverse_smooth_pressure", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_INVERSE_SMOOTH_PRESSURE);
	RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
	RNA_def_property_ui_text(prop, "Inverse Smooth Pressure", "Lighter pressure causes more smoothing to be applied");
	RNA_def_property_update(prop, 0, "rna_Brush_update");
	
	prop = RNA_def_property(srna, "use_rake", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_RAKE);
	RNA_def_property_ui_text(prop, "Rake", "Rotate the brush texture to match the stroke direction");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_relative_jitter", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", BRUSH_ABSOLUTE_JITTER);
	RNA_def_property_ui_icon(prop, ICON_UNLOCKED, true);
	RNA_def_property_ui_text(prop, "Absolute Jitter", "Jittering happens in screen space, not relative to brush size");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_random_rotation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_RANDOM_ROTATION);
	RNA_def_property_ui_text(prop, "Random Rotation", "Rotate the brush texture at random");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_plane_trim", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_PLANE_TRIM);
	RNA_def_property_ui_text(prop, "Use Plane Trim", "Enable Plane Trim");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_frontface", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_FRONTFACE);
	RNA_def_property_ui_text(prop, "Use Front-Face", "Brush only affects vertexes that face the viewer");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_anchor", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_ANCHORED);
	RNA_def_property_ui_text(prop, "Anchored", "Keep the brush anchored to the initial location");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_space", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_SPACE);
	RNA_def_property_ui_text(prop, "Space", "Limit brush application to the distance specified by spacing");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_line", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_LINE);
	RNA_def_property_ui_text(prop, "Line", "Draw a line with dabs separated according to spacing");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_curve", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_CURVE);
	RNA_def_property_ui_text(prop, "Curve", "Define the stroke curve with a bezier curve. Dabs are separated according to spacing");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_smooth_stroke", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_SMOOTH_STROKE);
	RNA_def_property_ui_text(prop, "Smooth Stroke", "Brush lags behind mouse and follows a smoother path");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_persistent", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_PERSISTENT);
	RNA_def_property_ui_text(prop, "Persistent", "Sculpt on a persistent layer of the mesh");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_accumulate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_ACCUMULATE);
	RNA_def_property_ui_text(prop, "Accumulate", "Accumulate stroke daubs on top of each other");
	RNA_def_property_update(prop, 0, "rna_Brush_update");
	
	prop = RNA_def_property(srna, "use_space_attenuation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_SPACE_ATTEN);
	RNA_def_property_ui_icon(prop, ICON_UNLOCKED, true);
	RNA_def_property_ui_text(prop, "Use Automatic Strength Adjustment",
	                         "Automatically adjust strength to give consistent results for different spacings");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	/* adaptive space is not implemented yet */
	prop = RNA_def_property(srna, "use_adaptive_space", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_ADAPTIVE_SPACE);
	RNA_def_property_ui_text(prop, "Adaptive Spacing",
	                         "Space daubs according to surface orientation instead of screen space");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_locked_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_LOCK_SIZE);
	RNA_def_property_ui_text(prop, "Use Blender Units",
	                         "When locked brush stays same size relative to object; when unlocked brush size is "
	                         "given in pixels");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_edge_to_edge", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_EDGE_TO_EDGE);
	RNA_def_property_ui_text(prop, "Edge-to-edge", "Drag anchor brush from edge-to-edge");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_restore_mesh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_DRAG_DOT);
	RNA_def_property_ui_text(prop, "Restore Mesh", "Allow a single dot to be carefully positioned");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	/* only for projection paint, TODO, other paint modes */
	prop = RNA_def_property(srna, "use_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", BRUSH_LOCK_ALPHA);
	RNA_def_property_ui_text(prop, "Alpha", "When this is disabled, lock alpha while painting");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_ui_text(prop, "Curve", "Editable falloff curve");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "paint_curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Paint Curve", "Active Paint Curve");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "gradient", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "gradient");
	RNA_def_property_struct_type(prop, "ColorRamp");
	RNA_def_property_ui_text(prop, "Gradient", "");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	/* gradient source */
	prop = RNA_def_property(srna, "gradient_stroke_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, brush_gradient_items);
	RNA_def_property_ui_text(prop, "Gradient Stroke Mode", "");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "gradient_fill_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, brush_gradient_fill_items);
	RNA_def_property_ui_text(prop, "Gradient Fill Mode", "");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	/* overlay flags */
	prop = RNA_def_property(srna, "use_primary_overlay", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "overlay_flags", BRUSH_OVERLAY_PRIMARY);
	RNA_def_property_ui_text(prop, "Use Texture Overlay", "Show texture in viewport");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_secondary_overlay", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "overlay_flags", BRUSH_OVERLAY_SECONDARY);
	RNA_def_property_ui_text(prop, "Use Texture Overlay", "Show texture in viewport");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_cursor_overlay", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "overlay_flags", BRUSH_OVERLAY_CURSOR);
	RNA_def_property_ui_text(prop, "Use Cursor Overlay", "Show cursor in viewport");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_cursor_overlay_override", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "overlay_flags", BRUSH_OVERLAY_CURSOR_OVERRIDE_ON_STROKE);
	RNA_def_property_ui_text(prop, "Override Overlay", "Don't show overlay during a stroke");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_primary_overlay_override", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "overlay_flags", BRUSH_OVERLAY_PRIMARY_OVERRIDE_ON_STROKE);
	RNA_def_property_ui_text(prop, "Override Overlay", "Don't show overlay during a stroke");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_secondary_overlay_override", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "overlay_flags", BRUSH_OVERLAY_SECONDARY_OVERRIDE_ON_STROKE);
	RNA_def_property_ui_text(prop, "Override Overlay", "Don't show overlay during a stroke");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	/* paint mode flags */
	prop = RNA_def_property(srna, "use_paint_sculpt", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ob_mode", OB_MODE_SCULPT);
	RNA_def_property_ui_text(prop, "Use Sculpt", "Use this brush in sculpt mode");

	prop = RNA_def_property(srna, "use_paint_vertex", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ob_mode", OB_MODE_VERTEX_PAINT);
	RNA_def_property_ui_text(prop, "Use Vertex", "Use this brush in vertex paint mode");

	prop = RNA_def_property(srna, "use_paint_weight", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ob_mode", OB_MODE_WEIGHT_PAINT);
	RNA_def_property_ui_text(prop, "Use Weight", "Use this brush in weight paint mode");

	prop = RNA_def_property(srna, "use_paint_image", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ob_mode", OB_MODE_TEXTURE_PAINT);
	RNA_def_property_ui_text(prop, "Use Texture", "Use this brush in texture paint mode");

	/* texture */
	prop = RNA_def_property(srna, "texture_slot", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "BrushTextureSlot");
	RNA_def_property_pointer_sdna(prop, NULL, "mtex");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Texture Slot", "");
	
	prop = RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "mtex.tex");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Texture", "");
	RNA_def_property_update(prop, NC_TEXTURE, "rna_Brush_main_tex_update");

	prop = RNA_def_property(srna, "mask_texture_slot", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "BrushTextureSlot");
	RNA_def_property_pointer_sdna(prop, NULL, "mask_mtex");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mask Texture Slot", "");

	prop = RNA_def_property(srna, "mask_texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "mask_mtex.tex");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mask Texture", "");
	RNA_def_property_update(prop, NC_TEXTURE, "rna_Brush_secondary_tex_update");

	prop = RNA_def_property(srna, "texture_overlay_alpha", PROP_INT, PROP_PERCENTAGE);
	RNA_def_property_int_sdna(prop, NULL, "texture_overlay_alpha");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Texture Overlay Alpha", "");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "mask_overlay_alpha", PROP_INT, PROP_PERCENTAGE);
	RNA_def_property_int_sdna(prop, NULL, "mask_overlay_alpha");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Mask Texture Overlay Alpha", "");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "cursor_overlay_alpha", PROP_INT, PROP_PERCENTAGE);
	RNA_def_property_int_sdna(prop, NULL, "cursor_overlay_alpha");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Mask Texture Overlay Alpha", "");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "cursor_color_add", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "add_col");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Add Color", "Color of cursor when adding");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "cursor_color_subtract", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "sub_col");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Subtract Color", "Color of cursor when subtracting");
	RNA_def_property_update(prop, 0, "rna_Brush_update");

	prop = RNA_def_property(srna, "use_custom_icon", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_CUSTOM_ICON);
	RNA_def_property_ui_text(prop, "Custom Icon", "Set the brush icon from an image file");
	RNA_def_property_update(prop, 0, "rna_Brush_icon_update");

	prop = RNA_def_property(srna, "icon_filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "icon_filepath");
	RNA_def_property_ui_text(prop, "Brush Icon Filepath", "File path to brush icon");
	RNA_def_property_update(prop, 0, "rna_Brush_icon_update");

	/* clone tool */
	prop = RNA_def_property(srna, "clone_image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "clone.image");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Clone Image", "Image for clone tool");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_Brush_update");
	
	prop = RNA_def_property(srna, "clone_alpha", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clone.alpha");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Clone Alpha", "Opacity of clone image display");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_Brush_update");

	prop = RNA_def_property(srna, "clone_offset", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "clone.offset");
	RNA_def_property_ui_text(prop, "Clone Offset", "");
	RNA_def_property_ui_range(prop, -1.0f, 1.0f, 10.0f, 3);
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_Brush_update");

	prop = RNA_def_property(srna, "brush_capabilities", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "BrushCapabilities");
	RNA_def_property_pointer_funcs(prop, "rna_Brush_capabilities_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Brush Capabilities", "Brush's capabilities");

	/* brush capabilities (mode-dependent) */
	prop = RNA_def_property(srna, "sculpt_capabilities", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "SculptToolCapabilities");
	RNA_def_property_pointer_funcs(prop, "rna_Sculpt_tool_capabilities_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Sculpt Capabilities", "Brush's capabilities in sculpt mode");

	prop = RNA_def_property(srna, "image_paint_capabilities", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "ImapaintToolCapabilities");
	RNA_def_property_pointer_funcs(prop, "rna_Imapaint_tool_capabilities_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Image Painting Capabilities", "Brush's capabilities in image paint mode");
}


/* A brush stroke is a list of changes to the brush that
 * can occur during a stroke
 *
 *  o 3D location of the brush
 *  o 2D mouse location
 *  o Tablet pressure
 *  o Direction flip
 *  o Tool switch
 *  o Time
 */
static void rna_def_operator_stroke_element(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "OperatorStrokeElement", "PropertyGroup");
	RNA_def_struct_ui_text(srna, "Operator Stroke Element", "");

	prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Location", "");

	prop = RNA_def_property(srna, "mouse", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_text(prop, "Mouse", "");

	prop = RNA_def_property(srna, "pressure", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Pressure", "Tablet pressure");

	prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_text(prop, "Brush Size", "Brush Size in screen space");

	prop = RNA_def_property(srna, "pen_flip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_ui_text(prop, "Flip", "");

	/* used in uv painting */
	prop = RNA_def_property(srna, "time", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_ui_text(prop, "Time", "");
	
	/* used for Grease Pencil sketching sessions */
	prop = RNA_def_property(srna, "is_start", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_ui_text(prop, "Is Stroke Start", "");

	/* XXX: Tool (this will be for pressing a modifier key for a different brush,
	 *      e.g. switching to a Smooth brush in the middle of the stroke */

	/* XXX: i don't think blender currently supports the ability to properly do a remappable modifier
	 *      in the middle of a stroke */
}

void RNA_def_brush(BlenderRNA *brna)
{
	rna_def_brush(brna);
	rna_def_brush_capabilities(brna);
	rna_def_sculpt_capabilities(brna);
	rna_def_image_paint_capabilities(brna);
	rna_def_brush_texture_slot(brna);
	rna_def_operator_stroke_element(brna);
}

#endif
