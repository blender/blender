/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "DNA_brush_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_workspace_types.h"

#include "BKE_layer.h"
#include "BKE_sculpt.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "IMB_imbuf.h"

#include "WM_types.h"

static const EnumPropertyItem prop_direction_items[] = {
    {0, "ADD", ICON_ADD, "Add", "Add effect of brush"},
    {BRUSH_DIR_IN, "SUBTRACT", ICON_REMOVE, "Subtract", "Subtract effect of brush"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME
static const EnumPropertyItem prop_smooth_direction_items[] = {
    {0, "SMOOTH", ICON_ADD, "Smooth", "Smooth the surface"},
    {BRUSH_DIR_IN,
     "ENHANCE_DETAILS",
     ICON_REMOVE,
     "Enhance Details",
     "Enhance the surface detail"},
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

static const EnumPropertyItem sculpt_stroke_method_items[] = {
    {0, "DOTS", 0, "Dots", "Apply paint on each mouse move step"},
    {BRUSH_DRAG_DOT, "DRAG_DOT", 0, "Drag Dot", "Allows a single dot to be carefully positioned"},
    {BRUSH_SPACE,
     "SPACE",
     0,
     "Space",
     "Limit brush application to the distance specified by spacing"},
    {BRUSH_AIRBRUSH,
     "AIRBRUSH",
     0,
     "Airbrush",
     "Keep applying paint effect while holding mouse (spray)"},
    {BRUSH_ANCHORED, "ANCHORED", 0, "Anchored", "Keep the brush anchored to the initial location"},
    {BRUSH_LINE, "LINE", 0, "Line", "Draw a line with dabs separated according to spacing"},
    {int(BRUSH_CURVE),
     "CURVE",
     0,
     "Curve",
     "Define the stroke curve with a bezier curve (dabs are separated according to spacing)"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_brush_texture_slot_map_all_mode_items[] = {
    {MTEX_MAP_MODE_VIEW, "VIEW_PLANE", 0, "View Plane", ""},
    {MTEX_MAP_MODE_AREA, "AREA_PLANE", 0, "Area Plane", ""},
    {MTEX_MAP_MODE_TILED, "TILED", 0, "Tiled", ""},
    {MTEX_MAP_MODE_3D, "3D", 0, "3D", ""},
    {MTEX_MAP_MODE_RANDOM, "RANDOM", 0, "Random", ""},
    {MTEX_MAP_MODE_STENCIL, "STENCIL", 0, "Stencil", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME
static const EnumPropertyItem rna_enum_brush_texture_slot_map_texture_mode_items[] = {
    {MTEX_MAP_MODE_VIEW, "VIEW_PLANE", 0, "View Plane", ""},
    {MTEX_MAP_MODE_TILED, "TILED", 0, "Tiled", ""},
    {MTEX_MAP_MODE_3D, "3D", 0, "3D", ""},
    {MTEX_MAP_MODE_RANDOM, "RANDOM", 0, "Random", ""},
    {MTEX_MAP_MODE_STENCIL, "STENCIL", 0, "Stencil", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

/* Note: we don't actually turn these into a single enum bit-mask property,
 * instead we construct individual boolean properties. */
const EnumPropertyItem rna_enum_brush_automasking_flag_items[] = {
    {BRUSH_AUTOMASKING_TOPOLOGY,
     "use_automasking_topology",
     0,
     "Topology",
     "Affect only vertices connected to the active vertex under the brush"},
    {BRUSH_AUTOMASKING_FACE_SETS,
     "use_automasking_face_sets",
     0,
     "Face Sets",
     "Affect only vertices that share Face Sets with the active vertex"},
    {BRUSH_AUTOMASKING_BOUNDARY_EDGES,
     "use_automasking_boundary_edges",
     0,
     "Mesh Boundary Auto-Masking",
     "Do not affect non manifold boundary edges"},
    {BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS,
     "use_automasking_boundary_face_sets",
     0,
     "Face Sets Boundary Automasking",
     "Do not affect vertices that belong to a Face Set boundary"},
    {BRUSH_AUTOMASKING_CAVITY_NORMAL,
     "use_automasking_cavity",
     0,
     "Cavity Mask",
     "Do not affect vertices on peaks, based on the surface curvature"},
    {BRUSH_AUTOMASKING_CAVITY_INVERTED,
     "use_automasking_cavity_inverted",
     0,
     "Inverted Cavity Mask",
     "Do not affect vertices within crevices, based on the surface curvature"},
    {BRUSH_AUTOMASKING_CAVITY_USE_CURVE,
     "use_automasking_custom_cavity_curve",
     0,
     "Custom Cavity Curve",
     "Use custom curve"},
    {0, nullptr, 0, nullptr, nullptr}};

const EnumPropertyItem rna_enum_brush_sculpt_tool_items[] = {
    {SCULPT_TOOL_DRAW, "DRAW", ICON_BRUSH_SCULPT_DRAW, "Draw", ""},
    {SCULPT_TOOL_DRAW_SHARP, "DRAW_SHARP", ICON_BRUSH_SCULPT_DRAW, "Draw Sharp", ""},
    {SCULPT_TOOL_CLAY, "CLAY", ICON_BRUSH_CLAY, "Clay", ""},
    {SCULPT_TOOL_CLAY_STRIPS, "CLAY_STRIPS", ICON_BRUSH_CLAY_STRIPS, "Clay Strips", ""},
    {SCULPT_TOOL_CLAY_THUMB, "CLAY_THUMB", ICON_BRUSH_CLAY_STRIPS, "Clay Thumb", ""},
    {SCULPT_TOOL_LAYER, "LAYER", ICON_BRUSH_LAYER, "Layer", ""},
    {SCULPT_TOOL_INFLATE, "INFLATE", ICON_BRUSH_INFLATE, "Inflate", ""},
    {SCULPT_TOOL_BLOB, "BLOB", ICON_BRUSH_BLOB, "Blob", ""},
    {SCULPT_TOOL_CREASE, "CREASE", ICON_BRUSH_CREASE, "Crease", ""},
    RNA_ENUM_ITEM_SEPR,
    {SCULPT_TOOL_SMOOTH, "SMOOTH", ICON_BRUSH_SMOOTH, "Smooth", ""},
    {SCULPT_TOOL_FLATTEN, "FLATTEN", ICON_BRUSH_FLATTEN, "Flatten", ""},
    {SCULPT_TOOL_FILL, "FILL", ICON_BRUSH_FILL, "Fill", ""},
    {SCULPT_TOOL_SCRAPE, "SCRAPE", ICON_BRUSH_SCRAPE, "Scrape", ""},
    {SCULPT_TOOL_MULTIPLANE_SCRAPE,
     "MULTIPLANE_SCRAPE",
     ICON_BRUSH_SCRAPE,
     "Multi-plane Scrape",
     ""},
    {SCULPT_TOOL_PINCH, "PINCH", ICON_BRUSH_PINCH, "Pinch", ""},
    RNA_ENUM_ITEM_SEPR,
    {SCULPT_TOOL_GRAB, "GRAB", ICON_BRUSH_GRAB, "Grab", ""},
    {SCULPT_TOOL_ELASTIC_DEFORM, "ELASTIC_DEFORM", ICON_BRUSH_GRAB, "Elastic Deform", ""},
    {SCULPT_TOOL_SNAKE_HOOK, "SNAKE_HOOK", ICON_BRUSH_SNAKE_HOOK, "Snake Hook", ""},
    {SCULPT_TOOL_THUMB, "THUMB", ICON_BRUSH_THUMB, "Thumb", ""},
    {SCULPT_TOOL_POSE, "POSE", ICON_BRUSH_GRAB, "Pose", ""},
    {SCULPT_TOOL_NUDGE, "NUDGE", ICON_BRUSH_NUDGE, "Nudge", ""},
    {SCULPT_TOOL_ROTATE, "ROTATE", ICON_BRUSH_ROTATE, "Rotate", ""},
    {SCULPT_TOOL_SLIDE_RELAX, "TOPOLOGY", ICON_BRUSH_GRAB, "Slide Relax", ""},
    {SCULPT_TOOL_BOUNDARY, "BOUNDARY", ICON_BRUSH_GRAB, "Boundary", ""},
    RNA_ENUM_ITEM_SEPR,
    {SCULPT_TOOL_CLOTH, "CLOTH", ICON_BRUSH_SCULPT_DRAW, "Cloth", ""},
    {SCULPT_TOOL_SIMPLIFY, "SIMPLIFY", ICON_BRUSH_DATA, "Simplify", ""},
    {SCULPT_TOOL_MASK, "MASK", ICON_BRUSH_MASK, "Mask", ""},
    {SCULPT_TOOL_DRAW_FACE_SETS, "DRAW_FACE_SETS", ICON_BRUSH_MASK, "Draw Face Sets", ""},
    {SCULPT_TOOL_DISPLACEMENT_ERASER,
     "DISPLACEMENT_ERASER",
     ICON_BRUSH_SCULPT_DRAW,
     "Multires Displacement Eraser",
     ""},
    {SCULPT_TOOL_DISPLACEMENT_SMEAR,
     "DISPLACEMENT_SMEAR",
     ICON_BRUSH_SCULPT_DRAW,
     "Multires Displacement Smear",
     ""},
    {SCULPT_TOOL_PAINT, "PAINT", ICON_BRUSH_SCULPT_DRAW, "Paint", ""},
    {SCULPT_TOOL_SMEAR, "SMEAR", ICON_BRUSH_SCULPT_DRAW, "Smear", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_brush_uv_sculpt_tool_items[] = {
    {UV_SCULPT_TOOL_GRAB, "GRAB", 0, "Grab", "Grab UVs"},
    {UV_SCULPT_TOOL_RELAX, "RELAX", 0, "Relax", "Relax UVs"},
    {UV_SCULPT_TOOL_PINCH, "PINCH", 0, "Pinch", "Pinch UVs"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_brush_vertex_tool_items[] = {
    {VPAINT_TOOL_DRAW, "DRAW", ICON_BRUSH_MIX, "Draw", ""},
    {VPAINT_TOOL_BLUR, "BLUR", ICON_BRUSH_BLUR, "Blur", ""},
    {VPAINT_TOOL_AVERAGE, "AVERAGE", ICON_BRUSH_BLUR, "Average", ""},
    {VPAINT_TOOL_SMEAR, "SMEAR", ICON_BRUSH_BLUR, "Smear", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_brush_weight_tool_items[] = {
    {WPAINT_TOOL_DRAW, "DRAW", ICON_BRUSH_MIX, "Draw", ""},
    {WPAINT_TOOL_BLUR, "BLUR", ICON_BRUSH_BLUR, "Blur", ""},
    {WPAINT_TOOL_AVERAGE, "AVERAGE", ICON_BRUSH_BLUR, "Average", ""},
    {WPAINT_TOOL_SMEAR, "SMEAR", ICON_BRUSH_BLUR, "Smear", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_brush_image_tool_items[] = {
    {PAINT_TOOL_DRAW, "DRAW", ICON_BRUSH_TEXDRAW, "Draw", ""},
    {PAINT_TOOL_SOFTEN, "SOFTEN", ICON_BRUSH_SOFTEN, "Soften", ""},
    {PAINT_TOOL_SMEAR, "SMEAR", ICON_BRUSH_SMEAR, "Smear", ""},
    {PAINT_TOOL_CLONE, "CLONE", ICON_BRUSH_CLONE, "Clone", ""},
    {PAINT_TOOL_FILL, "FILL", ICON_BRUSH_TEXFILL, "Fill", ""},
    {PAINT_TOOL_MASK, "MASK", ICON_BRUSH_TEXMASK, "Mask", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_brush_gpencil_types_items[] = {
    {GPAINT_TOOL_DRAW,
     "DRAW",
     ICON_STROKE,
     "Draw",
     "The brush is of type used for drawing strokes"},
    {GPAINT_TOOL_FILL, "FILL", ICON_COLOR, "Fill", "The brush is of type used for filling areas"},
    {GPAINT_TOOL_ERASE,
     "ERASE",
     ICON_PANEL_CLOSE,
     "Erase",
     "The brush is used for erasing strokes"},
    {GPAINT_TOOL_TINT,
     "TINT",
     ICON_BRUSH_TEXDRAW,
     "Tint",
     "The brush is of type used for tinting strokes"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_brush_gpencil_vertex_types_items[] = {
    {GPVERTEX_TOOL_DRAW, "DRAW", ICON_BRUSH_MIX, "Draw", "Paint a color on stroke points"},
    {GPVERTEX_TOOL_BLUR,
     "BLUR",
     ICON_BRUSH_BLUR,
     "Blur",
     "Smooth out the colors of adjacent stroke points"},
    {GPVERTEX_TOOL_AVERAGE,
     "AVERAGE",
     ICON_BRUSH_BLUR,
     "Average",
     "Smooth out colors with the average color under the brush"},
    {GPVERTEX_TOOL_SMEAR,
     "SMEAR",
     ICON_BRUSH_BLUR,
     "Smear",
     "Smudge colors by grabbing and dragging them"},
    {GPVERTEX_TOOL_REPLACE,
     "REPLACE",
     ICON_BRUSH_BLUR,
     "Replace",
     "Replace the color of stroke points that already have a color applied"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_brush_gpencil_sculpt_types_items[] = {
    {GPSCULPT_TOOL_SMOOTH, "SMOOTH", ICON_GPBRUSH_SMOOTH, "Smooth", "Smooth stroke points"},
    {GPSCULPT_TOOL_THICKNESS,
     "THICKNESS",
     ICON_GPBRUSH_THICKNESS,
     "Thickness",
     "Adjust thickness of strokes"},
    {GPSCULPT_TOOL_STRENGTH,
     "STRENGTH",
     ICON_GPBRUSH_STRENGTH,
     "Strength",
     "Adjust color strength of strokes"},
    {GPSCULPT_TOOL_RANDOMIZE,
     "RANDOMIZE",
     ICON_GPBRUSH_RANDOMIZE,
     "Randomize",
     "Introduce jitter/randomness into strokes"},
    {GPSCULPT_TOOL_GRAB,
     "GRAB",
     ICON_GPBRUSH_GRAB,
     "Grab",
     "Translate the set of points initially within the brush circle"},
    {GPSCULPT_TOOL_PUSH,
     "PUSH",
     ICON_GPBRUSH_PUSH,
     "Push",
     "Move points out of the way, as if combing them"},
    {GPSCULPT_TOOL_TWIST,
     "TWIST",
     ICON_GPBRUSH_TWIST,
     "Twist",
     "Rotate points around the midpoint of the brush"},
    {GPSCULPT_TOOL_PINCH,
     "PINCH",
     ICON_GPBRUSH_PINCH,
     "Pinch",
     "Pull points towards the midpoint of the brush"},
    {GPSCULPT_TOOL_CLONE,
     "CLONE",
     ICON_GPBRUSH_CLONE,
     "Clone",
     "Paste copies of the strokes stored on the internal clipboard"},
    {0, nullptr, 0, nullptr, nullptr}};

const EnumPropertyItem rna_enum_brush_gpencil_weight_types_items[] = {
    {GPWEIGHT_TOOL_DRAW,
     "WEIGHT",
     ICON_GPBRUSH_WEIGHT,
     "Weight",
     "Paint weight in active vertex group"},
    {GPWEIGHT_TOOL_BLUR, "BLUR", ICON_BRUSH_BLUR, "Blur", "Blur weight in active vertex group"},
    {GPWEIGHT_TOOL_AVERAGE,
     "AVERAGE",
     ICON_BRUSH_BLUR,
     "Average",
     "Average weight in active vertex group"},
    {GPWEIGHT_TOOL_SMEAR,
     "SMEAR",
     ICON_BRUSH_SMEAR,
     "Smear",
     "Smear weight in active vertex group"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_brush_curves_sculpt_tool_items[] = {
    {CURVES_SCULPT_TOOL_SELECTION_PAINT,
     "SELECTION_PAINT",
     ICON_BRUSH_PAINT_SELECT,
     "Paint Selection",
     ""},
    RNA_ENUM_ITEM_SEPR,
    {CURVES_SCULPT_TOOL_ADD, "ADD", ICON_BRUSH_CURVES_ADD, "Add", ""},
    {CURVES_SCULPT_TOOL_DELETE, "DELETE", ICON_BRUSH_CURVES_DELETE, "Delete", ""},
    {CURVES_SCULPT_TOOL_DENSITY, "DENSITY", ICON_BRUSH_CURVES_DENSITY, "Density", ""},
    RNA_ENUM_ITEM_SEPR,
    {CURVES_SCULPT_TOOL_COMB, "COMB", ICON_BRUSH_CURVES_COMB, "Comb", ""},
    {CURVES_SCULPT_TOOL_SNAKE_HOOK, "SNAKE_HOOK", ICON_BRUSH_CURVES_SNAKE_HOOK, "Snake Hook", ""},
    {CURVES_SCULPT_TOOL_GROW_SHRINK,
     "GROW_SHRINK",
     ICON_BRUSH_CURVES_GROW_SHRINK,
     "Grow / Shrink",
     ""},
    {CURVES_SCULPT_TOOL_PINCH, "PINCH", ICON_BRUSH_CURVES_PINCH, "Pinch", ""},
    {CURVES_SCULPT_TOOL_PUFF, "PUFF", ICON_BRUSH_CURVES_PUFF, "Puff", ""},
    {CURVES_SCULPT_TOOL_SMOOTH, "SMOOTH", ICON_BRUSH_CURVES_SMOOTH, "Smooth", ""},
    {CURVES_SCULPT_TOOL_SLIDE, "SLIDE", ICON_BRUSH_CURVES_SLIDE, "Slide", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifndef RNA_RUNTIME
static EnumPropertyItem rna_enum_gpencil_brush_eraser_modes_items[] = {
    {GP_BRUSH_ERASER_SOFT,
     "SOFT",
     0,
     "Dissolve",
     "Erase strokes, fading their points strength and thickness"},
    {GP_BRUSH_ERASER_HARD, "HARD", 0, "Point", "Erase stroke points"},
    {GP_BRUSH_ERASER_STROKE, "STROKE", 0, "Stroke", "Erase entire strokes"},
    {0, nullptr, 0, nullptr, nullptr},
};

static EnumPropertyItem rna_enum_gpencil_fill_draw_modes_items[] = {
    {GP_FILL_DMODE_BOTH,
     "BOTH",
     0,
     "All",
     "Use both visible strokes and edit lines as fill boundary limits"},
    {GP_FILL_DMODE_STROKE, "STROKE", 0, "Strokes", "Use visible strokes as fill boundary limits"},
    {GP_FILL_DMODE_CONTROL, "CONTROL", 0, "Edit Lines", "Use edit lines as fill boundary limits"},
    {0, nullptr, 0, nullptr, nullptr}};

static EnumPropertyItem rna_enum_gpencil_fill_extend_modes_items[] = {
    {GP_FILL_EMODE_EXTEND, "EXTEND", 0, "Extend", "Extend strokes in straight lines"},
    {GP_FILL_EMODE_RADIUS, "RADIUS", 0, "Radius", "Connect endpoints that are close together"},
    {0, nullptr, 0, nullptr, nullptr}};

static EnumPropertyItem rna_enum_gpencil_fill_layers_modes_items[] = {
    {GP_FILL_GPLMODE_VISIBLE, "VISIBLE", 0, "Visible", "Visible layers"},
    {GP_FILL_GPLMODE_ACTIVE, "ACTIVE", 0, "Active", "Only active layer"},
    {GP_FILL_GPLMODE_ABOVE, "ABOVE", 0, "Layer Above", "Layer above active"},
    {GP_FILL_GPLMODE_BELOW, "BELOW", 0, "Layer Below", "Layer below active"},
    {GP_FILL_GPLMODE_ALL_ABOVE, "ALL_ABOVE", 0, "All Above", "All layers above active"},
    {GP_FILL_GPLMODE_ALL_BELOW, "ALL_BELOW", 0, "All Below", "All layers below active"},
    {0, nullptr, 0, nullptr, nullptr}};

static EnumPropertyItem rna_enum_gpencil_fill_direction_items[] = {
    {0, "NORMAL", ICON_ADD, "Normal", "Fill internal area"},
    {BRUSH_DIR_IN, "INVERT", ICON_REMOVE, "Inverted", "Fill inverted area"},
    {0, nullptr, 0, nullptr, nullptr},
};

static EnumPropertyItem rna_enum_gpencil_brush_modes_items[] = {
    {GP_BRUSH_MODE_ACTIVE, "ACTIVE", 0, "Active", "Use current mode"},
    {GP_BRUSH_MODE_MATERIAL, "MATERIAL", 0, "Material", "Use always material mode"},
    {GP_BRUSH_MODE_VERTEXCOLOR, "VERTEXCOLOR", 0, "Vertex Color", "Use always Vertex Color mode"},
    {0, nullptr, 0, nullptr, nullptr}};

static EnumPropertyItem rna_enum_gpencil_brush_paint_icons_items[] = {
    {GP_BRUSH_ICON_PENCIL, "PENCIL", ICON_GPBRUSH_PENCIL, "Pencil", ""},
    {GP_BRUSH_ICON_PEN, "PEN", ICON_GPBRUSH_PEN, "Pen", ""},
    {GP_BRUSH_ICON_INK, "INK", ICON_GPBRUSH_INK, "Ink", ""},
    {GP_BRUSH_ICON_INKNOISE, "INKNOISE", ICON_GPBRUSH_INKNOISE, "Ink Noise", ""},
    {GP_BRUSH_ICON_BLOCK, "BLOCK", ICON_GPBRUSH_BLOCK, "Block", ""},
    {GP_BRUSH_ICON_MARKER, "MARKER", ICON_GPBRUSH_MARKER, "Marker", ""},
    {GP_BRUSH_ICON_AIRBRUSH, "AIRBRUSH", ICON_GPBRUSH_AIRBRUSH, "Airbrush", ""},
    {GP_BRUSH_ICON_CHISEL, "CHISEL", ICON_GPBRUSH_CHISEL, "Chisel", ""},
    {GP_BRUSH_ICON_FILL, "FILL", ICON_GPBRUSH_FILL, "Fill", ""},
    {GP_BRUSH_ICON_ERASE_SOFT, "SOFT", ICON_GPBRUSH_ERASE_SOFT, "Eraser Soft", ""},
    {GP_BRUSH_ICON_ERASE_HARD, "HARD", ICON_GPBRUSH_ERASE_HARD, "Eraser Hard", ""},
    {GP_BRUSH_ICON_ERASE_STROKE, "STROKE", ICON_GPBRUSH_ERASE_STROKE, "Eraser Stroke", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static EnumPropertyItem rna_enum_gpencil_brush_sculpt_icons_items[] = {
    {GP_BRUSH_ICON_GPBRUSH_SMOOTH, "SMOOTH", ICON_GPBRUSH_SMOOTH, "Smooth", ""},
    {GP_BRUSH_ICON_GPBRUSH_THICKNESS, "THICKNESS", ICON_GPBRUSH_THICKNESS, "Thickness", ""},
    {GP_BRUSH_ICON_GPBRUSH_STRENGTH, "STRENGTH", ICON_GPBRUSH_STRENGTH, "Strength", ""},
    {GP_BRUSH_ICON_GPBRUSH_RANDOMIZE, "RANDOMIZE", ICON_GPBRUSH_RANDOMIZE, "Randomize", ""},
    {GP_BRUSH_ICON_GPBRUSH_GRAB, "GRAB", ICON_GPBRUSH_GRAB, "Grab", ""},
    {GP_BRUSH_ICON_GPBRUSH_PUSH, "PUSH", ICON_GPBRUSH_PUSH, "Push", ""},
    {GP_BRUSH_ICON_GPBRUSH_TWIST, "TWIST", ICON_GPBRUSH_TWIST, "Twist", ""},
    {GP_BRUSH_ICON_GPBRUSH_PINCH, "PINCH", ICON_GPBRUSH_PINCH, "Pinch", ""},
    {GP_BRUSH_ICON_GPBRUSH_CLONE, "CLONE", ICON_GPBRUSH_CLONE, "Clone", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static EnumPropertyItem rna_enum_gpencil_brush_weight_icons_items[] = {
    {GP_BRUSH_ICON_GPBRUSH_WEIGHT, "DRAW", ICON_GPBRUSH_WEIGHT, "Draw", ""},
    {GP_BRUSH_ICON_GPBRUSH_BLUR, "BLUR", ICON_BRUSH_BLUR, "Blur", ""},
    {GP_BRUSH_ICON_GPBRUSH_AVERAGE, "AVERAGE", ICON_BRUSH_BLUR, "Average", ""},
    {GP_BRUSH_ICON_GPBRUSH_SMEAR, "SMEAR", ICON_BRUSH_BLUR, "Smear", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
static EnumPropertyItem rna_enum_gpencil_brush_vertex_icons_items[] = {
    {GP_BRUSH_ICON_VERTEX_DRAW, "DRAW", ICON_BRUSH_MIX, "Draw", ""},
    {GP_BRUSH_ICON_VERTEX_BLUR, "BLUR", ICON_BRUSH_BLUR, "Blur", ""},
    {GP_BRUSH_ICON_VERTEX_AVERAGE, "AVERAGE", ICON_BRUSH_BLUR, "Average", ""},
    {GP_BRUSH_ICON_VERTEX_SMEAR, "SMEAR", ICON_BRUSH_BLUR, "Smear", ""},
    {GP_BRUSH_ICON_VERTEX_REPLACE, "REPLACE", ICON_BRUSH_MIX, "Replace", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static EnumPropertyItem rna_enum_brush_dyntopo_mode[] = {
    {DYNTOPO_DETAIL_RELATIVE, "RELATIVE", ICON_NONE, "Relative", ""},
    {DYNTOPO_DETAIL_CONSTANT, "CONSTANT", ICON_NONE, "Constant", ""},
    {DYNTOPO_DETAIL_MANUAL, "MANUAL", ICON_NONE, "Manual", ""},
    {DYNTOPO_DETAIL_BRUSH, "BRUSH", ICON_NONE, "Brush", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static EnumPropertyItem rna_enum_brush_dyntopo_inherit[] = {
    {DYNTOPO_SUBDIVIDE, "SUBDIVIDE", ICON_NONE, "Subdivide", ""},
    {DYNTOPO_COLLAPSE, "COLLAPSE", ICON_NONE, "Collapse", ""},
    {DYNTOPO_CLEANUP, "CLEANUP", ICON_NONE, "Cleanup", ""},
    {DYNTOPO_LOCAL_COLLAPSE, "LOCAL_COLLAPSE", ICON_NONE, "Local Collapse", ""},
    {DYNTOPO_LOCAL_SUBDIVIDE, "LOCAL_SUBDIVIDE", ICON_NONE, "Local Subdivide", ""},
    {DYNTOPO_DISABLED, "DISABLED", ICON_NONE, "Disable", ""},
    {DYNTOPO_INHERIT_DETAIL_PERCENT, "DETAIL_PERCENT", ICON_NONE, "Percent", ""},
    {DYNTOPO_INHERIT_MODE, "MODE", ICON_NONE, "Mode", ""},
    {DYNTOPO_INHERIT_CONSTANT_DETAIL, "CONSTANT_DETAIL", ICON_NONE, "Constant Detail", ""},
    {DYNTOPO_INHERIT_SPACING, "SPACING", ICON_NONE, "Spacing", ""},
    {DYNTOPO_INHERIT_DETAIL_SIZE, "DETAIL_SIZE", ICON_NONE, "Detail Size", ""},
    {DYNTOPO_INHERIT_RADIUS_SCALE, "RADIUS_SCALE", ICON_NONE, "Radius Scale", ""},
    {DYNTOPO_INHERIT_REPEAT,
     "REPEAT",
     ICON_NONE,
     "Repeat",
     "How many extra times to run the dyntopo remesher."},
    {DYNTOPO_INHERIT_QUALITY, "QUALITY", ICON_NONE, "Quality", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#endif

#ifdef RNA_RUNTIME

#  include "MEM_guardedalloc.h"

#  include "RNA_access.h"

#  include "BKE_brush.h"
#  include "BKE_colorband.h"
#  include "BKE_gpencil_legacy.h"
#  include "BKE_icons.h"
#  include "BKE_material.h"
#  include "BKE_paint.h"

#  include "WM_api.h"

static bool rna_BrushCapabilitiesSculpt_has_accumulate_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return SCULPT_TOOL_HAS_ACCUMULATE(br->sculpt_tool);
}

static bool rna_BrushCapabilitiesSculpt_has_topology_rake_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return SCULPT_TOOL_HAS_TOPOLOGY_RAKE(br->sculpt_tool);
}

static bool rna_BrushCapabilitiesSculpt_has_auto_smooth_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return !ELEM(
      br->sculpt_tool, SCULPT_TOOL_MASK, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR);
}

static bool rna_BrushCapabilitiesSculpt_has_height_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return br->sculpt_tool == SCULPT_TOOL_LAYER;
}

static bool rna_BrushCapabilitiesSculpt_has_jitter_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return (!(br->flag & BRUSH_ANCHORED) && !(br->flag & BRUSH_DRAG_DOT) &&
          !ELEM(br->sculpt_tool,
                SCULPT_TOOL_GRAB,
                SCULPT_TOOL_ROTATE,
                SCULPT_TOOL_SNAKE_HOOK,
                SCULPT_TOOL_THUMB));
}

static bool rna_BrushCapabilitiesSculpt_has_normal_weight_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return SCULPT_TOOL_HAS_NORMAL_WEIGHT(br->sculpt_tool);
}

static bool rna_BrushCapabilitiesSculpt_has_rake_factor_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return SCULPT_TOOL_HAS_RAKE(br->sculpt_tool);
}

static bool rna_BrushCapabilities_has_overlay_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return ELEM(
      br->mtex.brush_map_mode, MTEX_MAP_MODE_VIEW, MTEX_MAP_MODE_TILED, MTEX_MAP_MODE_STENCIL);
}

static bool rna_BrushCapabilitiesSculpt_has_persistence_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return ELEM(br->sculpt_tool, SCULPT_TOOL_LAYER, SCULPT_TOOL_CLOTH);
}

static bool rna_BrushCapabilitiesSculpt_has_pinch_factor_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return ELEM(br->sculpt_tool, SCULPT_TOOL_BLOB, SCULPT_TOOL_CREASE, SCULPT_TOOL_SNAKE_HOOK);
}

static bool rna_BrushCapabilitiesSculpt_has_plane_offset_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return ELEM(br->sculpt_tool,
              SCULPT_TOOL_CLAY,
              SCULPT_TOOL_CLAY_STRIPS,
              SCULPT_TOOL_CLAY_THUMB,
              SCULPT_TOOL_FILL,
              SCULPT_TOOL_FLATTEN,
              SCULPT_TOOL_SCRAPE);
}

static bool rna_BrushCapabilitiesSculpt_has_random_texture_angle_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return (!ELEM(br->sculpt_tool,
                SCULPT_TOOL_GRAB,
                SCULPT_TOOL_ROTATE,
                SCULPT_TOOL_SNAKE_HOOK,
                SCULPT_TOOL_THUMB));
}

static bool rna_TextureCapabilities_has_random_texture_angle_get(PointerRNA *ptr)
{
  MTex *mtex = (MTex *)ptr->data;
  return ELEM(mtex->brush_map_mode, MTEX_MAP_MODE_VIEW, MTEX_MAP_MODE_AREA, MTEX_MAP_MODE_RANDOM);
}

static bool rna_BrushCapabilities_has_random_texture_angle_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return !(br->flag & BRUSH_ANCHORED);
}

static bool rna_BrushCapabilitiesSculpt_has_sculpt_plane_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return !ELEM(br->sculpt_tool,
               SCULPT_TOOL_INFLATE,
               SCULPT_TOOL_MASK,
               SCULPT_TOOL_PINCH,
               SCULPT_TOOL_SMOOTH);
}

static bool rna_BrushCapabilitiesSculpt_has_color_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return ELEM(br->sculpt_tool, SCULPT_TOOL_PAINT);
}

static bool rna_BrushCapabilitiesSculpt_has_secondary_color_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return BKE_brush_sculpt_has_secondary_color(br);
}

static bool rna_BrushCapabilitiesSculpt_has_smooth_stroke_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return (!(br->flag & BRUSH_ANCHORED) && !(br->flag & BRUSH_DRAG_DOT) &&
          !(br->flag & BRUSH_LINE) && !(br->flag & BRUSH_CURVE) &&
          !ELEM(br->sculpt_tool,
                SCULPT_TOOL_GRAB,
                SCULPT_TOOL_ROTATE,
                SCULPT_TOOL_SNAKE_HOOK,
                SCULPT_TOOL_THUMB));
}

static bool rna_BrushCapabilities_has_smooth_stroke_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return (!(br->flag & BRUSH_ANCHORED) && !(br->flag & BRUSH_DRAG_DOT) &&
          !(br->flag & BRUSH_LINE) && !(br->flag & BRUSH_CURVE));
}

static bool rna_BrushCapabilitiesSculpt_has_space_attenuation_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return ((br->flag & (BRUSH_SPACE | BRUSH_LINE | BRUSH_CURVE)) && !ELEM(br->sculpt_tool,
                                                                         SCULPT_TOOL_GRAB,
                                                                         SCULPT_TOOL_ROTATE,
                                                                         SCULPT_TOOL_SMOOTH,
                                                                         SCULPT_TOOL_SNAKE_HOOK));
}

static bool rna_BrushCapabilitiesImagePaint_has_space_attenuation_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return (br->flag & (BRUSH_SPACE | BRUSH_LINE | BRUSH_CURVE)) &&
         br->imagepaint_tool != PAINT_TOOL_FILL;
}

static bool rna_BrushCapabilitiesImagePaint_has_color_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return ELEM(br->imagepaint_tool, PAINT_TOOL_DRAW, PAINT_TOOL_FILL);
}

static bool rna_BrushCapabilitiesVertexPaint_has_color_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return ELEM(br->vertexpaint_tool, VPAINT_TOOL_DRAW);
}

static bool rna_BrushCapabilitiesWeightPaint_has_weight_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return ELEM(br->weightpaint_tool, WPAINT_TOOL_DRAW);
}

static bool rna_BrushCapabilities_has_spacing_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return (!(br->flag & BRUSH_ANCHORED));
}

static bool rna_BrushCapabilitiesSculpt_has_strength_pressure_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return !ELEM(br->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_SNAKE_HOOK);
}

static bool rna_TextureCapabilities_has_texture_angle_get(PointerRNA *ptr)
{
  MTex *mtex = (MTex *)ptr->data;
  return mtex->brush_map_mode != MTEX_MAP_MODE_3D;
}

static bool rna_BrushCapabilitiesSculpt_has_direction_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return !ELEM(br->sculpt_tool,
               SCULPT_TOOL_DRAW,
               SCULPT_TOOL_DRAW_SHARP,
               SCULPT_TOOL_CLAY,
               SCULPT_TOOL_CLAY_STRIPS,
               SCULPT_TOOL_SMOOTH,
               SCULPT_TOOL_LAYER,
               SCULPT_TOOL_INFLATE,
               SCULPT_TOOL_BLOB,
               SCULPT_TOOL_CREASE,
               SCULPT_TOOL_FLATTEN,
               SCULPT_TOOL_FILL,
               SCULPT_TOOL_SCRAPE,
               SCULPT_TOOL_CLAY,
               SCULPT_TOOL_PINCH,
               SCULPT_TOOL_MASK);
}

static bool rna_BrushCapabilitiesSculpt_has_gravity_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return !ELEM(br->sculpt_tool, SCULPT_TOOL_MASK, SCULPT_TOOL_SMOOTH);
}

static bool rna_BrushCapabilitiesSculpt_has_tilt_get(PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  return ELEM(br->sculpt_tool,
              SCULPT_TOOL_DRAW,
              SCULPT_TOOL_DRAW_SHARP,
              SCULPT_TOOL_FLATTEN,
              SCULPT_TOOL_FILL,
              SCULPT_TOOL_SCRAPE,
              SCULPT_TOOL_CLAY_STRIPS,
              SCULPT_TOOL_CLAY_THUMB);
}

static bool rna_TextureCapabilities_has_texture_angle_source_get(PointerRNA *ptr)
{
  MTex *mtex = (MTex *)ptr->data;
  return ELEM(mtex->brush_map_mode, MTEX_MAP_MODE_VIEW, MTEX_MAP_MODE_AREA, MTEX_MAP_MODE_RANDOM);
}

static bool rna_BrushCapabilitiesImagePaint_has_accumulate_get(PointerRNA *ptr)
{
  /* only support for draw tool */
  Brush *br = (Brush *)ptr->data;

  return ((br->flag & BRUSH_AIRBRUSH) || (br->flag & BRUSH_DRAG_DOT) ||
          (br->flag & BRUSH_ANCHORED) || (br->imagepaint_tool == PAINT_TOOL_SOFTEN) ||
          (br->imagepaint_tool == PAINT_TOOL_SMEAR) || (br->imagepaint_tool == PAINT_TOOL_FILL) ||
          (br->mtex.tex && !ELEM(br->mtex.brush_map_mode,
                                 MTEX_MAP_MODE_TILED,
                                 MTEX_MAP_MODE_STENCIL,
                                 MTEX_MAP_MODE_3D))) ?
             false :
             true;
}

static bool rna_BrushCapabilitiesImagePaint_has_radius_get(PointerRNA *ptr)
{
  /* only support for draw tool */
  Brush *br = (Brush *)ptr->data;

  return (br->imagepaint_tool != PAINT_TOOL_FILL);
}

static PointerRNA rna_Sculpt_tool_capabilities_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_BrushCapabilitiesSculpt, ptr->owner_id);
}

static PointerRNA rna_Imapaint_tool_capabilities_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_BrushCapabilitiesImagePaint, ptr->owner_id);
}

static PointerRNA rna_Vertexpaint_tool_capabilities_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_BrushCapabilitiesVertexPaint, ptr->owner_id);
}

static PointerRNA rna_Weightpaint_tool_capabilities_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_BrushCapabilitiesWeightPaint, ptr->owner_id);
}

static PointerRNA rna_Brush_capabilities_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_BrushCapabilities, ptr->owner_id);
}

static void rna_Brush_reset_icon(Brush *br)
{
  ID *id = &br->id;

  if (br->flag & BRUSH_CUSTOM_ICON) {
    return;
  }

  if (id->icon_id >= BIFICONID_LAST) {
    BKE_icon_id_delete(id);
    BKE_previewimg_id_free(id);
  }

  id->icon_id = 0;
}

static void rna_Brush_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, br);
  // WM_main_add_notifier(NC_SPACE | ND_SPACE_VIEW3D, nullptr);
}

static void rna_Brush_dyntopo_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr) {}

static void rna_Brush_material_update(bContext * /*C*/, PointerRNA * /*ptr*/)
{
  /* number of material users changed */
  WM_main_add_notifier(NC_SPACE | ND_SPACE_PROPERTIES, nullptr);
}

static void rna_Brush_main_tex_update(bContext *C, PointerRNA *ptr)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Brush *br = (Brush *)ptr->data;
  BKE_paint_invalidate_overlay_tex(scene, view_layer, br->mtex.tex);
  rna_Brush_update(bmain, scene, ptr);
}

static void rna_Brush_secondary_tex_update(bContext *C, PointerRNA *ptr)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Brush *br = (Brush *)ptr->data;
  BKE_paint_invalidate_overlay_tex(scene, view_layer, br->mask_mtex.tex);
  rna_Brush_update(bmain, scene, ptr);
}

static void rna_Brush_size_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  BKE_paint_invalidate_overlay_all();
  rna_Brush_update(bmain, scene, ptr);
}

static void rna_Brush_update_and_reset_icon(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Brush *br = static_cast<Brush *>(ptr->data);
  rna_Brush_reset_icon(br);
  rna_Brush_update(bmain, scene, ptr);
}

static void rna_Brush_stroke_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, scene);
  rna_Brush_update(bmain, scene, ptr);
}

static void rna_Brush_icon_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Brush *br = (Brush *)ptr->data;

  if (br->icon_imbuf) {
    IMB_freeImBuf(br->icon_imbuf);
    br->icon_imbuf = nullptr;
  }

  br->id.icon_id = 0;

  if (br->flag & BRUSH_CUSTOM_ICON) {
    BKE_icon_changed(BKE_icon_id_ensure(&br->id));
  }

  WM_main_add_notifier(NC_BRUSH | NA_EDITED, br);
}

static bool rna_Brush_imagetype_poll(PointerRNA * /*ptr*/, PointerRNA value)
{
  Image *image = (Image *)value.owner_id;
  return image->type != IMA_TYPE_R_RESULT && image->type != IMA_TYPE_COMPOSITE;
}

static void rna_TextureSlot_brush_angle_update(bContext *C, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  MTex *mtex = static_cast<MTex *>(ptr->data);
  /* skip invalidation of overlay for stencil mode */
  if (mtex->mapping != MTEX_MAP_MODE_STENCIL) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    BKE_paint_invalidate_overlay_tex(scene, view_layer, mtex->tex);
  }

  rna_TextureSlot_update(C, ptr);
}

static void rna_Brush_set_size(PointerRNA *ptr, int value)
{
  Brush *brush = static_cast<Brush *>(ptr->data);

  /* scale unprojected radius so it stays consistent with brush size */
  BKE_brush_scale_unprojected_radius(&brush->unprojected_radius, value, brush->size);

  brush->size = value;
}

static void rna_Brush_use_gradient_set(PointerRNA *ptr, int value)
{
  Brush *br = (Brush *)ptr->data;

  if (value & BRUSH_USE_GRADIENT) {
    br->flag |= BRUSH_USE_GRADIENT;
  }
  else {
    br->flag &= ~BRUSH_USE_GRADIENT;
  }

  if ((br->flag & BRUSH_USE_GRADIENT) && br->gradient == nullptr) {
    br->gradient = BKE_colorband_add(true);
  }
}

static void rna_Brush_set_unprojected_radius(PointerRNA *ptr, float value)
{
  Brush *brush = static_cast<Brush *>(ptr->data);

  /* scale brush size so it stays consistent with unprojected_radius */
  BKE_brush_scale_size(&brush->size, value, brush->unprojected_radius);
  brush->unprojected_radius = value;
}

static const EnumPropertyItem *rna_Brush_direction_itemf(bContext *C,
                                                         PointerRNA *ptr,
                                                         PropertyRNA * /*prop*/,
                                                         bool * /*r_free*/)
{
  ePaintMode mode = BKE_paintmode_get_active_from_context(C);

  /* sculpt mode */
  static const EnumPropertyItem prop_flatten_contrast_items[] = {
      {BRUSH_DIR_IN, "CONTRAST", ICON_ADD, "Contrast", "Subtract effect of brush"},
      {0, "FLATTEN", ICON_REMOVE, "Flatten", "Add effect of brush"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_fill_deepen_items[] = {
      {0, "FILL", ICON_ADD, "Fill", "Add effect of brush"},
      {BRUSH_DIR_IN, "DEEPEN", ICON_REMOVE, "Deepen", "Subtract effect of brush"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_scrape_peaks_items[] = {
      {0, "SCRAPE", ICON_ADD, "Scrape", "Add effect of brush"},
      {BRUSH_DIR_IN, "PEAKS", ICON_REMOVE, "Peaks", "Subtract effect of brush"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_pinch_magnify_items[] = {
      {BRUSH_DIR_IN, "MAGNIFY", ICON_ADD, "Magnify", "Subtract effect of brush"},
      {0, "PINCH", ICON_REMOVE, "Pinch", "Add effect of brush"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_inflate_deflate_items[] = {
      {0, "INFLATE", ICON_ADD, "Inflate", "Add effect of brush"},
      {BRUSH_DIR_IN, "DEFLATE", ICON_REMOVE, "Deflate", "Subtract effect of brush"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* texture paint mode */
  static const EnumPropertyItem prop_soften_sharpen_items[] = {
      {BRUSH_DIR_IN, "SHARPEN", ICON_ADD, "Sharpen", "Sharpen effect of brush"},
      {0, "SOFTEN", ICON_REMOVE, "Soften", "Blur effect of brush"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  Brush *me = (Brush *)(ptr->data);

  switch (mode) {
    case PAINT_MODE_SCULPT:
      switch (me->sculpt_tool) {
        case SCULPT_TOOL_DRAW:
        case SCULPT_TOOL_DRAW_SHARP:
        case SCULPT_TOOL_CREASE:
        case SCULPT_TOOL_BLOB:
        case SCULPT_TOOL_LAYER:
        case SCULPT_TOOL_CLAY:
        case SCULPT_TOOL_CLAY_STRIPS:
          return prop_direction_items;
        case SCULPT_TOOL_SMOOTH:
          return prop_smooth_direction_items;
        case SCULPT_TOOL_MASK:
          switch ((BrushMaskTool)me->mask_tool) {
            case BRUSH_MASK_DRAW:
              return prop_direction_items;

            case BRUSH_MASK_SMOOTH:
              return DummyRNA_DEFAULT_items;

            default:
              return DummyRNA_DEFAULT_items;
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
          return DummyRNA_DEFAULT_items;
      }

    case PAINT_MODE_TEXTURE_2D:
    case PAINT_MODE_TEXTURE_3D:
      switch (me->imagepaint_tool) {
        case PAINT_TOOL_SOFTEN:
          return prop_soften_sharpen_items;

        default:
          return DummyRNA_DEFAULT_items;
      }
    case PAINT_MODE_SCULPT_CURVES:
      switch (me->curves_sculpt_tool) {
        case CURVES_SCULPT_TOOL_GROW_SHRINK:
        case CURVES_SCULPT_TOOL_SELECTION_PAINT:
        case CURVES_SCULPT_TOOL_PINCH:
          return prop_direction_items;
        default:
          return DummyRNA_DEFAULT_items;
      }
    default:
      return DummyRNA_DEFAULT_items;
  }
}

static const EnumPropertyItem *rna_Brush_stroke_itemf(bContext *C,
                                                      PointerRNA * /*ptr*/,
                                                      PropertyRNA * /*prop*/,
                                                      bool * /*r_free*/)
{
  ePaintMode mode = BKE_paintmode_get_active_from_context(C);

  static const EnumPropertyItem brush_stroke_method_items[] = {
      {0, "DOTS", 0, "Dots", "Apply paint on each mouse move step"},
      {BRUSH_SPACE,
       "SPACE",
       0,
       "Space",
       "Limit brush application to the distance specified by spacing"},
      {BRUSH_AIRBRUSH,
       "AIRBRUSH",
       0,
       "Airbrush",
       "Keep applying paint effect while holding mouse (spray)"},
      {BRUSH_LINE, "LINE", 0, "Line", "Drag a line with dabs separated according to spacing"},
      {int(BRUSH_CURVE),
       "CURVE",
       0,
       "Curve",
       "Define the stroke curve with a bezier curve. Dabs are separated according to spacing"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  switch (mode) {
    case PAINT_MODE_SCULPT:
    case PAINT_MODE_TEXTURE_2D:
    case PAINT_MODE_TEXTURE_3D:
      return sculpt_stroke_method_items;

    default:
      return brush_stroke_method_items;
  }
}

/* Grease Pencil Drawing Brushes Settings */
static char *rna_BrushGpencilSettings_path(const PointerRNA * /*ptr*/)
{
  return BLI_strdup("gpencil_settings");
}

static void rna_BrushGpencilSettings_default_eraser_update(Main *bmain,
                                                           Scene *scene,
                                                           PointerRNA * /*ptr*/)
{
  ToolSettings *ts = scene->toolsettings;
  Paint *paint = &ts->gp_paint->paint;
  Brush *brush_cur = paint->brush;

  /* disable default eraser in all brushes */
  for (Brush *brush = static_cast<Brush *>(bmain->brushes.first); brush;
       brush = static_cast<Brush *>(brush->id.next))
  {
    if ((brush != brush_cur) && (brush->ob_mode == OB_MODE_PAINT_GPENCIL_LEGACY) &&
        (brush->gpencil_tool == GPAINT_TOOL_ERASE))
    {
      brush->gpencil_settings->flag &= ~GP_BRUSH_DEFAULT_ERASER;
    }
  }
}

static void rna_BrushGpencilSettings_use_material_pin_update(bContext *C, PointerRNA *ptr)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  Brush *brush = (Brush *)ptr->owner_id;

  if (brush->gpencil_settings->flag & GP_BRUSH_MATERIAL_PINNED) {
    Material *material = BKE_object_material_get(ob, ob->actcol);
    BKE_gpencil_brush_material_set(brush, material);
  }
  else {
    BKE_gpencil_brush_material_set(brush, nullptr);
  }

  /* number of material users changed */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_PROPERTIES, nullptr);
}

static void rna_BrushGpencilSettings_eraser_mode_update(Main * /*bmain*/,
                                                        Scene *scene,
                                                        PointerRNA * /*ptr*/)
{
  ToolSettings *ts = scene->toolsettings;
  Paint *paint = &ts->gp_paint->paint;
  Brush *brush = paint->brush;

  /* set eraser icon */
  if ((brush) && (brush->gpencil_tool == GPAINT_TOOL_ERASE)) {
    switch (brush->gpencil_settings->eraser_mode) {
      case GP_BRUSH_ERASER_SOFT:
        brush->gpencil_settings->icon_id = GP_BRUSH_ICON_ERASE_SOFT;
        break;
      case GP_BRUSH_ERASER_HARD:
        brush->gpencil_settings->icon_id = GP_BRUSH_ICON_ERASE_HARD;
        break;
      case GP_BRUSH_ERASER_STROKE:
        brush->gpencil_settings->icon_id = GP_BRUSH_ICON_ERASE_STROKE;
        break;
      default:
        brush->gpencil_settings->icon_id = GP_BRUSH_ICON_ERASE_SOFT;
        break;
    }
  }
}

static bool rna_BrushGpencilSettings_material_poll(PointerRNA * /*ptr*/, PointerRNA value)
{
  Material *ma = (Material *)value.data;

  /* GP materials only */
  return (ma->gp_style != nullptr);
}

static bool rna_GPencilBrush_pin_mode_get(PointerRNA *ptr)
{
  Brush *brush = (Brush *)ptr->owner_id;
  if ((brush != nullptr) && (brush->gpencil_settings != nullptr)) {
    return (brush->gpencil_settings->brush_draw_mode != GP_BRUSH_MODE_ACTIVE);
  }
  return false;
}

static void rna_GPencilBrush_pin_mode_set(PointerRNA * /*ptr*/, bool /*value*/)
{
  /* All data is set in update. Keep this function only to avoid RNA compilation errors. */
  return;
}

static void rna_GPencilBrush_pin_mode_update(bContext *C, PointerRNA *ptr)
{
  Brush *brush = (Brush *)ptr->owner_id;
  if ((brush != nullptr) && (brush->gpencil_settings != nullptr)) {
    if (brush->gpencil_settings->brush_draw_mode != GP_BRUSH_MODE_ACTIVE) {
      /* If not active, means that must be set to off. */
      brush->gpencil_settings->brush_draw_mode = GP_BRUSH_MODE_ACTIVE;
    }
    else {
      ToolSettings *ts = CTX_data_tool_settings(C);
      brush->gpencil_settings->brush_draw_mode = GPENCIL_USE_VERTEX_COLOR(ts) ?
                                                     GP_BRUSH_MODE_VERTEXCOLOR :
                                                     GP_BRUSH_MODE_MATERIAL;
    }
  }
}

static const EnumPropertyItem *rna_BrushTextureSlot_map_mode_itemf(bContext *C,
                                                                   PointerRNA * /*ptr*/,
                                                                   PropertyRNA * /*prop*/,
                                                                   bool * /*r_free*/)
{

  if (C == nullptr) {
    return rna_enum_brush_texture_slot_map_all_mode_items;
  }

#  define rna_enum_brush_texture_slot_map_sculpt_mode_items \
    rna_enum_brush_texture_slot_map_all_mode_items;

  const ePaintMode mode = BKE_paintmode_get_active_from_context(C);
  if (mode == PAINT_MODE_SCULPT) {
    return rna_enum_brush_texture_slot_map_sculpt_mode_items;
  }
  return rna_enum_brush_texture_slot_map_texture_mode_items;

#  undef rna_enum_brush_texture_slot_map_sculpt_mode_items
}

static void rna_Brush_automasking_invert_cavity_set(PointerRNA *ptr, bool val)
{
  Brush *brush = (Brush *)ptr->data;

  if (val) {
    brush->automasking_flags &= ~BRUSH_AUTOMASKING_CAVITY_NORMAL;
    brush->automasking_flags |= BRUSH_AUTOMASKING_CAVITY_INVERTED;
  }
  else {
    brush->automasking_flags &= ~BRUSH_AUTOMASKING_CAVITY_INVERTED;
  }
}

static void rna_Brush_automasking_cavity_set(PointerRNA *ptr, bool val)
{
  Brush *brush = (Brush *)ptr->data;

  if (val) {
    brush->automasking_flags &= ~BRUSH_AUTOMASKING_CAVITY_INVERTED;
    brush->automasking_flags |= BRUSH_AUTOMASKING_CAVITY_NORMAL;
  }
  else {
    brush->automasking_flags &= ~BRUSH_AUTOMASKING_CAVITY_NORMAL;
  }
}

static char *rna_BrushCurvesSculptSettings_path(const PointerRNA * /*ptr*/)
{
  return BLI_strdup("curves_sculpt_settings");
}

static int rna_DynTopoSettings_inherit_get(PointerRNA *ptr)
{
  Brush *brush;

  if (GS(ptr->owner_id->name) == ID_BR) {
    brush = (Brush *)ptr->owner_id;
  }
  else if (GS(ptr->owner_id->name) == ID_SCE) {
    /* Try and fetch the active brush. */
    Scene *scene = (Scene *)ptr->owner_id;
    if (scene->toolsettings && scene->toolsettings->sculpt) {
      brush = scene->toolsettings->sculpt->paint.brush;
    }
  }
  else {
    BLI_assert_unreachable();
    return 0;
  }

  if (!brush) {
    /* No brush, scene defines all settings. */
    return DYNTOPO_INHERIT_BITMASK;
  }

  return BKE_brush_dyntopo_inherit_flags(brush);
}
#else

static void rna_def_brush_texture_slot(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_mask_paint_map_mode_items[] = {
      {MTEX_MAP_MODE_VIEW, "VIEW_PLANE", 0, "View Plane", ""},
      {MTEX_MAP_MODE_TILED, "TILED", 0, "Tiled", ""},
      {MTEX_MAP_MODE_RANDOM, "RANDOM", 0, "Random", ""},
      {MTEX_MAP_MODE_STENCIL, "STENCIL", 0, "Stencil", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

#  define TEXTURE_CAPABILITY(prop_name_, ui_name_) \
    prop = RNA_def_property(srna, #prop_name_, PROP_BOOLEAN, PROP_NONE); \
    RNA_def_property_clear_flag(prop, PROP_EDITABLE); \
    RNA_def_property_boolean_funcs(prop, "rna_TextureCapabilities_" #prop_name_ "_get", nullptr); \
    RNA_def_property_ui_text(prop, ui_name_, nullptr)

  srna = RNA_def_struct(brna, "BrushTextureSlot", "TextureSlot");
  RNA_def_struct_sdna(srna, "MTex");
  RNA_def_struct_ui_text(
      srna, "Brush Texture Slot", "Texture slot for textures in a Brush data-block");

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "rot");
  RNA_def_property_range(prop, 0, M_PI * 2);
  RNA_def_property_ui_text(prop, "Angle", "Brush texture rotation");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_TextureSlot_brush_angle_update");

  prop = RNA_def_property(srna, "map_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "brush_map_mode");
  RNA_def_property_enum_items(prop, rna_enum_brush_texture_slot_map_all_mode_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_BrushTextureSlot_map_mode_itemf");
  RNA_def_property_ui_text(prop, "Mode", "");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

  prop = RNA_def_property(srna, "mask_map_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "brush_map_mode");
  RNA_def_property_enum_items(prop, prop_mask_paint_map_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

  prop = RNA_def_property(srna, "use_rake", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "brush_angle_mode", MTEX_ANGLE_RAKE);
  RNA_def_property_ui_text(prop, "Rake", "");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

  prop = RNA_def_property(srna, "use_random", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "brush_angle_mode", MTEX_ANGLE_RANDOM);
  RNA_def_property_ui_text(prop, "Random", "");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

  prop = RNA_def_property(srna, "random_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_range(prop, 0, M_PI * 2);
  RNA_def_property_ui_text(prop, "Random Angle", "Brush texture random angle");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_TextureSlot_update");

  TEXTURE_CAPABILITY(has_texture_angle_source, "Has Texture Angle Source");
  TEXTURE_CAPABILITY(has_random_texture_angle, "Has Random Texture Angle");
  TEXTURE_CAPABILITY(has_texture_angle, "Has Texture Angle Source");
}

static void rna_def_dyntopo_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "DynTopoSettings", nullptr);
  RNA_def_struct_sdna(srna, "DynTopoSettings");
  RNA_def_struct_ui_text(srna, "Dyntopo Settings", "");

  prop = RNA_def_property(srna, "spacing", PROP_INT, PROP_PERCENTAGE);
  RNA_def_property_int_sdna(prop, nullptr, "spacing");
  RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_range(prop, 0, 500, 5, -1);
  RNA_def_property_ui_text(prop,
                           "Spacing",
                           "Spacing between DynTopo daubs as a percentage of brush diameter; if "
                           "zero will use brush spacing");
  RNA_def_property_update(prop, 0, "rna_Brush_dyntopo_update");

  prop = RNA_def_property(srna, "detail_percent", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_sdna(prop, nullptr, "detail_percent");
  RNA_def_property_range(prop, 1, 1000);
  RNA_def_property_ui_range(prop, 1, 500, 5, -1);
  RNA_def_property_ui_text(prop, "Detail Percent", "");
  RNA_def_property_update(prop, 0, "rna_Brush_dyntopo_update");

  prop = RNA_def_property(srna, "detail_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "detail_size");
  RNA_def_property_range(prop, 0.0, 100.0);
  RNA_def_property_ui_range(prop, 0.0, 50.0, 0.1, 4);
  RNA_def_property_ui_text(prop, "Detail Size", "");
  RNA_def_property_update(prop, 0, "rna_Brush_dyntopo_update");

  prop = RNA_def_property(srna, "constant_detail", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "constant_detail");
  RNA_def_property_range(prop, 0.0001, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001, 1000.0, 10, 2);
  RNA_def_property_ui_text(prop,
                           "Resolution",
                           "Maximum edge length for dynamic topology sculpting (as divisor "
                           "of blender unit - higher value means smaller edge length)");
  RNA_def_property_update(prop, 0, "rna_Brush_dyntopo_update");

  prop = RNA_def_property(srna, "subdivide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", DYNTOPO_SUBDIVIDE);
  RNA_def_property_ui_icon(prop, ICON_NONE, 0);
  RNA_def_property_ui_text(prop, "Subdivide", "Enable Dyntopo Subdivision");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_dyntopo_update");

  prop = RNA_def_property(srna, "disabled", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", DYNTOPO_DISABLED);
  RNA_def_property_ui_icon(prop, ICON_NONE, 0);
  RNA_def_property_ui_text(prop, "No Dyntopo", "Disable Dyntopo for this brush");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_dyntopo_update");

  prop = RNA_def_property(srna, "cleanup", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", DYNTOPO_CLEANUP);
  RNA_def_property_ui_icon(prop, ICON_NONE, 0);
  RNA_def_property_ui_text(prop, "Cleanup", "Dissolve Verts With Only 3 or 4 faces");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_dyntopo_update");

  prop = RNA_def_property(srna, "collapse", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", DYNTOPO_COLLAPSE);
  RNA_def_property_ui_icon(prop, ICON_NONE, 0);
  RNA_def_property_ui_text(prop, "Collapse", "Enable Dyntopo Decimation");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_dyntopo_update");

  prop = RNA_def_property(srna, "local_collapse", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", DYNTOPO_LOCAL_COLLAPSE);
  RNA_def_property_ui_icon(prop, ICON_NONE, 0);
  RNA_def_property_ui_text(
      prop,
      "Local Collapse",
      "When collapse is disabled, collapse anyway based on local edge lengths under brush");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_dyntopo_update");

  prop = RNA_def_property(srna, "local_subdivide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", DYNTOPO_LOCAL_SUBDIVIDE);
  RNA_def_property_ui_icon(prop, ICON_NONE, 0);
  RNA_def_property_ui_text(
      prop,
      "Local Subdivide",
      "When subdivide is disabled, subdivide anyway based on local edge lengths under brush");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_dyntopo_update");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, 0);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, rna_enum_brush_dyntopo_mode);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Mode", "Detail Mode");

  /* Auto-generate is_XXX_overridden members. */
  EnumPropertyItem *item = rna_enum_brush_dyntopo_inherit;
  while (item->identifier) {
    char identifier[128];
    char name[128];

    /* Makesrna does not compile with BLI_snprintf. */
    sprintf(identifier, "is_%s_overridden", item->identifier);
    sprintf(name, "Overridden");

    /* Makesrna does not compile with BLI_str_tolower_ascii. */
    char *c = identifier;
    while (*c) {
      *c = tolower(*c);
      c++;
    }

    /* Use strdup to avoid memory leak warnings; identifier and name are global constants. */
    prop = RNA_def_property(srna, strdup(identifier), PROP_BOOLEAN, 0);
    RNA_def_property_boolean_negative_sdna(prop, nullptr, "inherit", item->value);
    RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE); /* Read-only. */
    RNA_def_property_ui_text(prop, strdup(name), "Brush overrides this setting.");

    item++;
  }

  prop = RNA_def_property(srna, "radius_scale", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "radius_scale");
  RNA_def_property_range(prop, 0.0f, 15.0f);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 0.001, 4);
  RNA_def_property_ui_text(prop, "Scale dyntopo radius", "");
  RNA_def_property_update(prop, 0, "rna_Brush_dyntopo_update");

  prop = RNA_def_property(srna, "repeat", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "repeat");
  RNA_def_property_range(prop, 0.0f, 15.0f);
  RNA_def_property_ui_text(prop, "Repeat", "How many times to run the dyntopo remesher.");
  RNA_def_property_update(prop, 0, "rna_Brush_dyntopo_update");

  prop = RNA_def_property(srna, "quality", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_sdna(prop, nullptr, "quality");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01, 3);
  RNA_def_property_ui_text(
      prop, "Quality", "Lower values are faster but make lower-quality geometry");
  RNA_def_property_update(prop, 0, "rna_Brush_dyntopo_update");
}

static void rna_def_sculpt_capabilities(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BrushCapabilitiesSculpt", nullptr);
  RNA_def_struct_sdna(srna, "Brush");
  RNA_def_struct_nested(brna, srna, "Brush");
  RNA_def_struct_ui_text(srna,
                         "Sculpt Capabilities",
                         "Read-only indications of which brush operations "
                         "are supported by the current sculpt tool");

#  define SCULPT_TOOL_CAPABILITY(prop_name_, ui_name_) \
    prop = RNA_def_property(srna, #prop_name_, PROP_BOOLEAN, PROP_NONE); \
    RNA_def_property_clear_flag(prop, PROP_EDITABLE); \
    RNA_def_property_boolean_funcs( \
        prop, "rna_BrushCapabilitiesSculpt_" #prop_name_ "_get", nullptr); \
    RNA_def_property_ui_text(prop, ui_name_, nullptr)

  SCULPT_TOOL_CAPABILITY(has_accumulate, "Has Accumulate");
  SCULPT_TOOL_CAPABILITY(has_auto_smooth, "Has Auto Smooth");
  SCULPT_TOOL_CAPABILITY(has_topology_rake, "Has Topology Rake");
  SCULPT_TOOL_CAPABILITY(has_height, "Has Height");
  SCULPT_TOOL_CAPABILITY(has_jitter, "Has Jitter");
  SCULPT_TOOL_CAPABILITY(has_normal_weight, "Has Crease/Pinch Factor");
  SCULPT_TOOL_CAPABILITY(has_rake_factor, "Has Rake Factor");
  SCULPT_TOOL_CAPABILITY(has_persistence, "Has Persistence");
  SCULPT_TOOL_CAPABILITY(has_pinch_factor, "Has Pinch Factor");
  SCULPT_TOOL_CAPABILITY(has_plane_offset, "Has Plane Offset");
  SCULPT_TOOL_CAPABILITY(has_random_texture_angle, "Has Random Texture Angle");
  SCULPT_TOOL_CAPABILITY(has_sculpt_plane, "Has Sculpt Plane");
  SCULPT_TOOL_CAPABILITY(has_color, "Has Color");
  SCULPT_TOOL_CAPABILITY(has_secondary_color, "Has Secondary Color");
  SCULPT_TOOL_CAPABILITY(has_smooth_stroke, "Has Smooth Stroke");
  SCULPT_TOOL_CAPABILITY(has_space_attenuation, "Has Space Attenuation");
  SCULPT_TOOL_CAPABILITY(has_strength_pressure, "Has Strength Pressure");
  SCULPT_TOOL_CAPABILITY(has_direction, "Has Direction");
  SCULPT_TOOL_CAPABILITY(has_gravity, "Has Gravity");
  SCULPT_TOOL_CAPABILITY(has_tilt, "Has Tilt");

#  undef SCULPT_CAPABILITY
}

static void rna_def_brush_capabilities(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BrushCapabilities", nullptr);
  RNA_def_struct_sdna(srna, "Brush");
  RNA_def_struct_nested(brna, srna, "Brush");
  RNA_def_struct_ui_text(
      srna, "Brush Capabilities", "Read-only indications of supported operations");

#  define BRUSH_CAPABILITY(prop_name_, ui_name_) \
    prop = RNA_def_property(srna, #prop_name_, PROP_BOOLEAN, PROP_NONE); \
    RNA_def_property_clear_flag(prop, PROP_EDITABLE); \
    RNA_def_property_boolean_funcs(prop, "rna_BrushCapabilities_" #prop_name_ "_get", nullptr); \
    RNA_def_property_ui_text(prop, ui_name_, nullptr)

  BRUSH_CAPABILITY(has_overlay, "Has Overlay");
  BRUSH_CAPABILITY(has_random_texture_angle, "Has Random Texture Angle");
  BRUSH_CAPABILITY(has_spacing, "Has Spacing");
  BRUSH_CAPABILITY(has_smooth_stroke, "Has Smooth Stroke");

#  undef BRUSH_CAPABILITY
}

static void rna_def_image_paint_capabilities(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BrushCapabilitiesImagePaint", nullptr);
  RNA_def_struct_sdna(srna, "Brush");
  RNA_def_struct_nested(brna, srna, "Brush");
  RNA_def_struct_ui_text(
      srna, "Image Paint Capabilities", "Read-only indications of supported operations");

#  define IMAPAINT_TOOL_CAPABILITY(prop_name_, ui_name_) \
    prop = RNA_def_property(srna, #prop_name_, PROP_BOOLEAN, PROP_NONE); \
    RNA_def_property_clear_flag(prop, PROP_EDITABLE); \
    RNA_def_property_boolean_funcs( \
        prop, "rna_BrushCapabilitiesImagePaint_" #prop_name_ "_get", nullptr); \
    RNA_def_property_ui_text(prop, ui_name_, nullptr)

  IMAPAINT_TOOL_CAPABILITY(has_accumulate, "Has Accumulate");
  IMAPAINT_TOOL_CAPABILITY(has_space_attenuation, "Has Space Attenuation");
  IMAPAINT_TOOL_CAPABILITY(has_radius, "Has Radius");
  IMAPAINT_TOOL_CAPABILITY(has_color, "Has Color");

#  undef IMAPAINT_TOOL_CAPABILITY
}

static void rna_def_vertex_paint_capabilities(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BrushCapabilitiesVertexPaint", nullptr);
  RNA_def_struct_sdna(srna, "Brush");
  RNA_def_struct_nested(brna, srna, "Brush");
  RNA_def_struct_ui_text(
      srna, "Vertex Paint Capabilities", "Read-only indications of supported operations");

#  define VPAINT_TOOL_CAPABILITY(prop_name_, ui_name_) \
    prop = RNA_def_property(srna, #prop_name_, PROP_BOOLEAN, PROP_NONE); \
    RNA_def_property_clear_flag(prop, PROP_EDITABLE); \
    RNA_def_property_boolean_funcs( \
        prop, "rna_BrushCapabilitiesVertexPaint_" #prop_name_ "_get", nullptr); \
    RNA_def_property_ui_text(prop, ui_name_, nullptr)

  VPAINT_TOOL_CAPABILITY(has_color, "Has Color");

#  undef VPAINT_TOOL_CAPABILITY
}

static void rna_def_weight_paint_capabilities(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BrushCapabilitiesWeightPaint", nullptr);
  RNA_def_struct_sdna(srna, "Brush");
  RNA_def_struct_nested(brna, srna, "Brush");
  RNA_def_struct_ui_text(
      srna, "Weight Paint Capabilities", "Read-only indications of supported operations");

#  define WPAINT_TOOL_CAPABILITY(prop_name_, ui_name_) \
    prop = RNA_def_property(srna, #prop_name_, PROP_BOOLEAN, PROP_NONE); \
    RNA_def_property_clear_flag(prop, PROP_EDITABLE); \
    RNA_def_property_boolean_funcs( \
        prop, "rna_BrushCapabilitiesWeightPaint_" #prop_name_ "_get", nullptr); \
    RNA_def_property_ui_text(prop, ui_name_, nullptr)

  WPAINT_TOOL_CAPABILITY(has_weight, "Has Weight");

#  undef WPAINT_TOOL_CAPABILITY
}

static void rna_def_gpencil_options(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* modes */
  static EnumPropertyItem gppaint_mode_types_items[] = {
      {GPPAINT_MODE_STROKE, "STROKE", 0, "Stroke", "Vertex Color affects to Stroke only"},
      {GPPAINT_MODE_FILL, "FILL", 0, "Fill", "Vertex Color affects to Fill only"},
      {GPPAINT_MODE_BOTH, "BOTH", 0, "Stroke & Fill", "Vertex Color affects to Stroke and Fill"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static EnumPropertyItem rna_enum_gpencil_brush_caps_types_items[] = {
      {GP_STROKE_CAP_ROUND, "ROUND", ICON_GP_CAPS_ROUND, "Round", ""},
      {GP_STROKE_CAP_FLAT, "FLAT", ICON_GP_CAPS_FLAT, "Flat", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "BrushGpencilSettings", nullptr);
  RNA_def_struct_sdna(srna, "BrushGpencilSettings");
  RNA_def_struct_path_func(srna, "rna_BrushGpencilSettings_path");
  RNA_def_struct_ui_text(srna, "Grease Pencil Brush Settings", "Settings for grease pencil brush");

  /* Strength factor for new strokes */
  prop = RNA_def_property(srna, "pen_strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "draw_strength");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(
      prop, "Strength", "Color strength for new strokes (affect alpha factor of color)");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Jitter factor for new strokes */
  prop = RNA_def_property(srna, "pen_jitter", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "draw_jitter");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Jitter", "Jitter factor for new strokes");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_BRUSH);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Randomness factor for pressure */
  prop = RNA_def_property(srna, "random_pressure", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "draw_random_press");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Pressure Randomness", "Randomness factor for pressure in new strokes");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Randomness factor for strength */
  prop = RNA_def_property(srna, "random_strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "draw_random_strength");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Strength Randomness", "Randomness factor strength in new strokes");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Angle when brush is full size */
  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "draw_angle");
  RNA_def_property_range(prop, -M_PI_2, M_PI_2);
  RNA_def_property_ui_text(prop,
                           "Angle",
                           "Direction of the stroke at which brush gives maximal thickness "
                           "(0° for horizontal)");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Factor to change brush size depending of angle */
  prop = RNA_def_property(srna, "angle_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "draw_angle_factor");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Angle Factor",
      "Reduce brush thickness by this factor when stroke is perpendicular to 'Angle' direction");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Smoothing factor for new strokes */
  prop = RNA_def_property(srna, "pen_smooth_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "draw_smoothfac");
  RNA_def_property_range(prop, 0.0, 2.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 10, 3);
  RNA_def_property_ui_text(
      prop,
      "Smooth",
      "Amount of smoothing to apply after finish newly created strokes, to reduce jitter/noise");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Iterations of the Smoothing factor */
  prop = RNA_def_property(srna, "pen_smooth_steps", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "draw_smoothlvl");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Iterations", "Number of times to smooth newly created strokes");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Subdivision level for new strokes */
  prop = RNA_def_property(srna, "pen_subdivision_steps", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "draw_subdivide");
  RNA_def_property_range(prop, 0, 3);
  RNA_def_property_ui_text(
      prop,
      "Subdivision Steps",
      "Number of times to subdivide newly created strokes, for less jagged strokes");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Simplify factor */
  prop = RNA_def_property(srna, "simplify_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "simplify_f");
  RNA_def_property_range(prop, 0, 100.0);
  RNA_def_property_ui_range(prop, 0, 100.0, 1.0f, 3);
  RNA_def_property_ui_text(prop, "Simplify", "Factor of Simplify using adaptive algorithm");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));

  /* Curves for pressure */
  prop = RNA_def_property(srna, "curve_sensitivity", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_sensitivity");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Curve Sensitivity", "Curve used for the sensitivity");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "curve_strength", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_strength");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Curve Strength", "Curve used for the strength");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "curve_jitter", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_jitter");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Curve Jitter", "Curve used for the jitter effect");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "curve_random_pressure", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_rand_pressure");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Random Curve", "Curve used for modulating effect");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "curve_random_strength", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_rand_strength");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Random Curve", "Curve used for modulating effect");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "curve_random_uv", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_rand_uv");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Random Curve", "Curve used for modulating effect");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "curve_random_hue", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_rand_hue");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Random Curve", "Curve used for modulating effect");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "curve_random_saturation", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_rand_saturation");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Random Curve", "Curve used for modulating effect");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "curve_random_value", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_rand_value");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Random Curve", "Curve used for modulating effect");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Fill threshold for transparency. */
  prop = RNA_def_property(srna, "fill_threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "fill_threshold");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Threshold", "Threshold to consider color transparent for filling");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* fill factor size */
  prop = RNA_def_property(srna, "fill_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fill_factor");
  RNA_def_property_range(prop, GPENCIL_MIN_FILL_FAC, GPENCIL_MAX_FILL_FAC);
  RNA_def_property_ui_text(
      prop,
      "Precision",
      "Factor for fill boundary accuracy, higher values are more accurate but slower");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* fill simplify steps */
  prop = RNA_def_property(srna, "fill_simplify_level", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "fill_simplylvl");
  RNA_def_property_range(prop, 0, 10);
  RNA_def_property_ui_text(
      prop, "Simplify", "Number of simplify steps (large values reduce fill accuracy)");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "uv_random", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "uv_random");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "UV Random", "Random factor for auto-generated UV rotation");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* gradient control */
  prop = RNA_def_property(srna, "hardness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "hardeness");
  RNA_def_property_range(prop, 0.001f, 1.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Hardness",
      "Gradient from the center of Dot and Box strokes (set to 1 for a solid stroke)");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));

  /* gradient shape ratio */
  prop = RNA_def_property(srna, "aspect", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "aspect_ratio");
  RNA_def_property_array(prop, 2);
  RNA_def_property_range(prop, 0.01f, 1.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Aspect", "");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));

  prop = RNA_def_property(srna, "input_samples", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "input_samples");
  RNA_def_property_range(prop, 0, GP_MAX_INPUT_SAMPLES);
  RNA_def_property_ui_text(
      prop,
      "Input Samples",
      "Generate intermediate points for very fast mouse movements. Set to 0 to disable");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* active smooth factor while drawing */
  prop = RNA_def_property(srna, "active_smooth_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "active_smooth");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Active Smooth", "Amount of smoothing while drawing");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "eraser_strength_factor", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_sdna(prop, nullptr, "era_strength_f");
  RNA_def_property_range(prop, 0.0, 100.0);
  RNA_def_property_ui_range(prop, 0.0, 100.0, 10, 1);
  RNA_def_property_ui_text(prop, "Affect Stroke Strength", "Amount of erasing for strength");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "eraser_thickness_factor", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_sdna(prop, nullptr, "era_thickness_f");
  RNA_def_property_range(prop, 0.0, 100.0);
  RNA_def_property_ui_range(prop, 0.0, 100.0, 10, 1);
  RNA_def_property_ui_text(prop, "Affect Stroke Thickness", "Amount of erasing for thickness");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* brush standard icon */
  prop = RNA_def_property(srna, "gpencil_paint_icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "icon_id");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_brush_paint_icons_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Grease Pencil Icon", "");

  prop = RNA_def_property(srna, "gpencil_sculpt_icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "icon_id");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_brush_sculpt_icons_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);
  RNA_def_property_ui_text(prop, "Grease Pencil Icon", "");

  prop = RNA_def_property(srna, "gpencil_weight_icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "icon_id");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_brush_weight_icons_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Grease Pencil Icon", "");

  prop = RNA_def_property(srna, "gpencil_vertex_icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "icon_id");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_brush_vertex_icons_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Grease Pencil Icon", "");

  /* Mode type. */
  prop = RNA_def_property(srna, "vertex_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "vertex_mode");
  RNA_def_property_enum_items(prop, gppaint_mode_types_items);
  RNA_def_property_ui_text(prop, "Mode Type", "Defines how vertex color affect to the strokes");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  /* Vertex Color mix factor. */
  prop = RNA_def_property(srna, "vertex_color_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "vertex_factor");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Vertex Color Factor", "Factor used to mix vertex color to get final color");

  /* Hue randomness. */
  prop = RNA_def_property(srna, "random_hue_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "random_hue");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_ui_text(prop, "Hue", "Random factor to modify original hue");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));

  /* Saturation randomness. */
  prop = RNA_def_property(srna, "random_saturation_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "random_saturation");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_ui_text(prop, "Saturation", "Random factor to modify original saturation");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));

  /* Value randomness. */
  prop = RNA_def_property(srna, "random_value_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "random_value");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_ui_text(prop, "Value", "Random factor to modify original value");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));

  /* Factor to extend stroke extremes in Fill tool. */
  prop = RNA_def_property(srna, "extend_stroke_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fill_extend_fac");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_ui_text(
      prop, "Closure Size", "Strokes end extension for closing gaps, use zero to disable");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));

  prop = RNA_def_property(srna, "fill_extend_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "fill_extend_mode");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_fill_extend_modes_items);
  RNA_def_property_ui_text(
      prop, "Closure Mode", "Types of stroke extensions used for closing gaps");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));

  /* Number of pixels to dilate fill area. Negative values contract the filled area. */
  prop = RNA_def_property(srna, "dilate", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "dilate_pixels");
  RNA_def_property_range(prop, -40, 40);
  RNA_def_property_int_default(prop, 1);
  RNA_def_property_ui_text(
      prop, "Dilate/Contract", "Number of pixels to expand or contract fill area");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Factor to determine outline external perimeter thickness. */
  prop = RNA_def_property(srna, "outline_thickness_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "outline_fac");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_ui_text(
      prop, "Thickness", "Thickness of the outline stroke relative to current brush thickness");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));

  /* Flags */
  prop = RNA_def_property(srna, "use_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BRUSH_USE_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure", "Use tablet pressure");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_strength_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BRUSH_USE_STRENGTH_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(
      prop, "Use Pressure Strength", "Use tablet pressure for color strength");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_jitter_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BRUSH_USE_JITTER_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure Jitter", "Use tablet pressure for jitter");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_stroke_random_hue", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", GP_BRUSH_USE_HUE_AT_STROKE);
  RNA_def_property_ui_icon(prop, ICON_GP_SELECT_STROKES, 0);
  RNA_def_property_ui_text(prop, "Stroke Random", "Use randomness at stroke level");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_stroke_random_sat", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", GP_BRUSH_USE_SAT_AT_STROKE);
  RNA_def_property_ui_icon(prop, ICON_GP_SELECT_STROKES, 0);
  RNA_def_property_ui_text(prop, "Stroke Random", "Use randomness at stroke level");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_stroke_random_val", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", GP_BRUSH_USE_VAL_AT_STROKE);
  RNA_def_property_ui_icon(prop, ICON_GP_SELECT_STROKES, 0);
  RNA_def_property_ui_text(prop, "Stroke Random", "Use randomness at stroke level");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_stroke_random_radius", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", GP_BRUSH_USE_PRESS_AT_STROKE);
  RNA_def_property_ui_icon(prop, ICON_GP_SELECT_STROKES, 0);
  RNA_def_property_ui_text(prop, "Stroke Random", "Use randomness at stroke level");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_stroke_random_strength", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", GP_BRUSH_USE_STRENGTH_AT_STROKE);
  RNA_def_property_ui_icon(prop, ICON_GP_SELECT_STROKES, 0);
  RNA_def_property_ui_text(prop, "Stroke Random", "Use randomness at stroke level");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_stroke_random_uv", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", GP_BRUSH_USE_UV_AT_STROKE);
  RNA_def_property_ui_icon(prop, ICON_GP_SELECT_STROKES, 0);
  RNA_def_property_ui_text(prop, "Stroke Random", "Use randomness at stroke level");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_random_press_hue", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", GP_BRUSH_USE_HUE_RAND_PRESS);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure", "Use pressure to modulate randomness");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_random_press_sat", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", GP_BRUSH_USE_SAT_RAND_PRESS);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure", "Use pressure to modulate randomness");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_random_press_val", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", GP_BRUSH_USE_VAL_RAND_PRESS);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure", "Use pressure to modulate randomness");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_random_press_radius", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", GP_BRUSH_USE_PRESSURE_RAND_PRESS);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure", "Use pressure to modulate randomness");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_random_press_strength", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", GP_BRUSH_USE_STRENGTH_RAND_PRESS);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure", "Use pressure to modulate randomness");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_random_press_uv", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", GP_BRUSH_USE_UV_RAND_PRESS);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure", "Use pressure to modulate randomness");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  prop = RNA_def_property(srna, "use_settings_stabilizer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BRUSH_STABILIZE_MOUSE);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop,
                           "Use Stabilizer",
                           "Draw lines with a delay to allow smooth strokes. Press Shift key to "
                           "override while drawing");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "eraser_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "eraser_mode");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_brush_eraser_modes_items);
  RNA_def_property_ui_text(prop, "Mode", "Eraser Mode");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(
      prop, NC_GPENCIL | ND_DATA, "rna_BrushGpencilSettings_eraser_mode_update");

  prop = RNA_def_property(srna, "caps_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "caps_type");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_brush_caps_types_items);
  RNA_def_property_ui_text(prop, "Caps Type", "The shape of the start and end of the stroke");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "fill_draw_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "fill_draw_mode");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_fill_draw_modes_items);
  RNA_def_property_ui_text(prop, "Mode", "Mode to draw boundary limits");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "fill_layer_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "fill_layer_mode");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_fill_layers_modes_items);
  RNA_def_property_ui_text(prop, "Layer Mode", "Layers used as boundaries");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "fill_direction", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "fill_direction");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_fill_direction_items);
  RNA_def_property_ui_text(prop, "Direction", "Direction of the fill");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "pin_draw_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_GPencilBrush_pin_mode_get", "rna_GPencilBrush_pin_mode_set");
  RNA_def_property_ui_icon(prop, ICON_UNPINNED, 1);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencilBrush_pin_mode_update");
  RNA_def_property_ui_text(prop, "Pin Mode", "Pin the mode to the brush");

  prop = RNA_def_property(srna, "brush_draw_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "brush_draw_mode");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_brush_modes_items);
  RNA_def_property_ui_text(prop, "Mode", "Preselected mode when using this brush");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "use_trim", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BRUSH_TRIM_STROKE);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(prop, "Trim Stroke Ends", "Trim intersecting stroke ends");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "use_settings_outline", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BRUSH_OUTLINE_STROKE);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(prop, "Outline", "Convert stroke to perimeter");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "direction", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "sculpt_flag");
  RNA_def_property_enum_items(prop, prop_direction_items);
  RNA_def_property_ui_text(prop, "Direction", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_edit_position", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "sculpt_mode_flag", GP_SCULPT_FLAGMODE_APPLY_POSITION);
  RNA_def_property_ui_text(prop, "Affect Position", "The brush affects the position of the point");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_edit_strength", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "sculpt_mode_flag", GP_SCULPT_FLAGMODE_APPLY_STRENGTH);
  RNA_def_property_ui_text(
      prop, "Affect Strength", "The brush affects the color strength of the point");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_edit_thickness", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "sculpt_mode_flag", GP_SCULPT_FLAGMODE_APPLY_THICKNESS);
  RNA_def_property_ui_text(
      prop, "Affect Thickness", "The brush affects the thickness of the point");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_edit_uv", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "sculpt_mode_flag", GP_SCULPT_FLAGMODE_APPLY_UV);
  RNA_def_property_ui_text(prop, "Affect UV", "The brush affects the UV rotation of the point");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  /* Material */
  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_BrushGpencilSettings_material_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK | PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Material", "Material used for strokes drawn using this brush");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_Brush_material_update");

  /* Secondary Material */
  prop = RNA_def_property(srna, "material_alt", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_BrushGpencilSettings_material_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK | PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Material", "Material used for secondary uses for this brush");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_Brush_material_update");

  prop = RNA_def_property(srna, "show_fill_boundary", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BRUSH_FILL_SHOW_HELPLINES);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Show Lines", "Show help lines for filling to see boundaries");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "show_fill_extend", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BRUSH_FILL_SHOW_EXTENDLINES);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Visual Aids", "Show help lines for stroke extension");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "use_collide_strokes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BRUSH_FILL_STROKE_COLLIDE);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(
      prop, "Strokes Collision", "Check if extend lines collide with strokes");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "show_fill", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", GP_BRUSH_FILL_HIDE);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(
      prop, "Show Fill", "Show transparent lines to use as boundary for filling");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "use_fill_limit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BRUSH_FILL_FIT_DISABLE);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Limit to Viewport", "Fill only visible areas in viewport");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "use_default_eraser", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BRUSH_DEFAULT_ERASER);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_icon(prop, ICON_UNPINNED, 1);
  RNA_def_property_ui_text(
      prop, "Default Eraser", "Use this brush when enable eraser with fast switch key");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(
      prop, NC_GPENCIL | ND_DATA, "rna_BrushGpencilSettings_default_eraser_update");

  prop = RNA_def_property(srna, "use_settings_postprocess", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BRUSH_GROUP_SETTINGS);
  RNA_def_property_ui_text(
      prop, "Use Post-Process Settings", "Additional post processing options for new strokes");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "use_settings_random", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BRUSH_GROUP_RANDOM);
  RNA_def_property_ui_text(prop, "Random Settings", "Random brush settings");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "use_material_pin", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BRUSH_MATERIAL_PINNED);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_icon(prop, ICON_UNPINNED, 1);
  RNA_def_property_ui_text(prop, "Pin Material", "Keep material assigned to brush");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(
      prop, NC_GPENCIL | ND_DATA, "rna_BrushGpencilSettings_use_material_pin_update");

  prop = RNA_def_property(srna, "show_lasso", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", GP_BRUSH_DISSABLE_LASSO);
  RNA_def_property_ui_text(
      prop, "Show Lasso", "Do not display fill color while drawing the stroke");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "use_occlude_eraser", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BRUSH_OCCLUDE_ERASER);
  RNA_def_property_ui_text(prop, "Occlude Eraser", "Erase only strokes visible and not occluded");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
}

static void rna_def_curves_sculpt_options(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem density_mode_items[] = {
      {BRUSH_CURVES_SCULPT_DENSITY_MODE_AUTO,
       "AUTO",
       ICON_AUTO,
       "Auto",
       "Either add or remove curves depending on the minimum distance of the curves under the "
       "cursor"},
      {BRUSH_CURVES_SCULPT_DENSITY_MODE_ADD,
       "ADD",
       ICON_ADD,
       "Add",
       "Add new curves between existing curves, taking the minimum distance into account"},
      {BRUSH_CURVES_SCULPT_DENSITY_MODE_REMOVE,
       "REMOVE",
       ICON_REMOVE,
       "Remove",
       "Remove curves whose root points are too close"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "BrushCurvesSculptSettings", nullptr);
  RNA_def_struct_path_func(srna, "rna_BrushCurvesSculptSettings_path");
  RNA_def_struct_sdna(srna, "BrushCurvesSculptSettings");
  RNA_def_struct_ui_text(srna, "Curves Sculpt Brush Settings", "");

  prop = RNA_def_property(srna, "add_amount", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, INT32_MAX);
  RNA_def_property_ui_text(prop, "Count", "Number of curves added by the Add brush");

  prop = RNA_def_property(srna, "points_per_curve", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 2, INT32_MAX);
  RNA_def_property_ui_text(
      prop, "Points per Curve", "Number of control points in a newly added curve");

  prop = RNA_def_property(srna, "scale_uniform", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_CURVES_SCULPT_FLAG_SCALE_UNIFORM);
  RNA_def_property_ui_text(prop,
                           "Scale Uniform",
                           "Grow or shrink curves by changing their size uniformly instead of "
                           "using trimming or extrapolation");

  prop = RNA_def_property(srna, "minimum_length", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(
      prop, "Minimum Length", "Avoid shrinking curves shorter than this length");

  prop = RNA_def_property(srna, "interpolate_length", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "flag", BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_LENGTH);
  RNA_def_property_ui_text(
      prop, "Interpolate Length", "Use length of the curves in close proximity");

  prop = RNA_def_property(srna, "interpolate_point_count", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "flag", BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_POINT_COUNT);
  RNA_def_property_ui_text(prop,
                           "Interpolate Point Count",
                           "Use the number of points from the curves in close proximity");

  prop = RNA_def_property(srna, "interpolate_shape", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_SHAPE);
  RNA_def_property_ui_text(
      prop, "Interpolate Shape", "Use shape of the curves in close proximity");

  prop = RNA_def_property(srna, "curve_length", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_text(
      prop,
      "Curve Length",
      "Length of newly added curves when it is not interpolated from other curves");

  prop = RNA_def_property(srna, "minimum_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1000.0f, 0.001, 2);
  RNA_def_property_ui_text(
      prop, "Minimum Distance", "Goal distance between curve roots for the Density brush");

  prop = RNA_def_property(srna, "density_add_attempts", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, INT32_MAX);
  RNA_def_property_ui_text(
      prop, "Density Add Attempts", "How many times the Density brush tries to add a new curve");

  prop = RNA_def_property(srna, "density_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, density_mode_items);
  RNA_def_property_ui_text(
      prop, "Density Mode", "Determines whether the brush adds or removes curves");

  prop = RNA_def_property(srna, "curve_parameter_falloff", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop,
                           "Curve Parameter Falloff",
                           "Falloff that is applied from the tip to the root of each curve");
}

static void rna_def_brush(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_blend_items[] = {
      {IMB_BLEND_MIX, "MIX", 0, "Mix", "Use Mix blending mode while painting"},
      RNA_ENUM_ITEM_SEPR,
      {IMB_BLEND_DARKEN, "DARKEN", 0, "Darken", "Use Darken blending mode while painting"},
      {IMB_BLEND_MUL, "MUL", 0, "Multiply", "Use Multiply blending mode while painting"},
      {IMB_BLEND_COLORBURN,
       "COLORBURN",
       0,
       "Color Burn",
       "Use Color Burn blending mode while painting"},
      {IMB_BLEND_LINEARBURN,
       "LINEARBURN",
       0,
       "Linear Burn",
       "Use Linear Burn blending mode while painting"},
      RNA_ENUM_ITEM_SEPR,
      {IMB_BLEND_LIGHTEN, "LIGHTEN", 0, "Lighten", "Use Lighten blending mode while painting"},
      {IMB_BLEND_SCREEN, "SCREEN", 0, "Screen", "Use Screen blending mode while painting"},
      {IMB_BLEND_COLORDODGE,
       "COLORDODGE",
       0,
       "Color Dodge",
       "Use Color Dodge blending mode while painting"},
      {IMB_BLEND_ADD, "ADD", 0, "Add", "Use Add blending mode while painting"},
      RNA_ENUM_ITEM_SEPR,
      {IMB_BLEND_OVERLAY, "OVERLAY", 0, "Overlay", "Use Overlay blending mode while painting"},
      {IMB_BLEND_SOFTLIGHT,
       "SOFTLIGHT",
       0,
       "Soft Light",
       "Use Soft Light blending mode while painting"},
      {IMB_BLEND_HARDLIGHT,
       "HARDLIGHT",
       0,
       "Hard Light",
       "Use Hard Light blending mode while painting"},
      {IMB_BLEND_VIVIDLIGHT,
       "VIVIDLIGHT",
       0,
       "Vivid Light",
       "Use Vivid Light blending mode while painting"},
      {IMB_BLEND_LINEARLIGHT,
       "LINEARLIGHT",
       0,
       "Linear Light",
       "Use Linear Light blending mode while painting"},
      {IMB_BLEND_PINLIGHT,
       "PINLIGHT",
       0,
       "Pin Light",
       "Use Pin Light blending mode while painting"},
      RNA_ENUM_ITEM_SEPR,
      {IMB_BLEND_DIFFERENCE,
       "DIFFERENCE",
       0,
       "Difference",
       "Use Difference blending mode while painting"},
      {IMB_BLEND_EXCLUSION,
       "EXCLUSION",
       0,
       "Exclusion",
       "Use Exclusion blending mode while painting"},
      {IMB_BLEND_SUB, "SUB", 0, "Subtract", "Use Subtract blending mode while painting"},
      RNA_ENUM_ITEM_SEPR,
      {IMB_BLEND_HUE, "HUE", 0, "Hue", "Use Hue blending mode while painting"},
      {IMB_BLEND_SATURATION,
       "SATURATION",
       0,
       "Saturation",
       "Use Saturation blending mode while painting"},
      {IMB_BLEND_COLOR, "COLOR", 0, "Color", "Use Color blending mode while painting"},
      {IMB_BLEND_LUMINOSITY, "LUMINOSITY", 0, "Value", "Use Value blending mode while painting"},
      RNA_ENUM_ITEM_SEPR,
      {IMB_BLEND_ERASE_ALPHA, "ERASE_ALPHA", 0, "Erase Alpha", "Erase alpha while painting"},
      {IMB_BLEND_ADD_ALPHA, "ADD_ALPHA", 0, "Add Alpha", "Add alpha while painting"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_sculpt_plane_items[] = {
      {SCULPT_DISP_DIR_AREA, "AREA", 0, "Area Plane", ""},
      {SCULPT_DISP_DIR_VIEW, "VIEW", 0, "View Plane", ""},
      {SCULPT_DISP_DIR_X, "X", 0, "X Plane", ""},
      {SCULPT_DISP_DIR_Y, "Y", 0, "Y Plane", ""},
      {SCULPT_DISP_DIR_Z, "Z", 0, "Z Plane", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_mask_tool_items[] = {
      {BRUSH_MASK_DRAW, "DRAW", 0, "Draw", ""},
      {BRUSH_MASK_SMOOTH, "SMOOTH", 0, "Smooth", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_blur_mode_items[] = {
      {KERNEL_BOX, "BOX", 0, "Box", ""},
      {KERNEL_GAUSSIAN, "GAUSSIAN", 0, "Gaussian", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_gradient_items[] = {
      {BRUSH_GRADIENT_PRESSURE, "PRESSURE", 0, "Pressure", ""},
      {BRUSH_GRADIENT_SPACING_REPEAT, "SPACING_REPEAT", 0, "Repeat", ""},
      {BRUSH_GRADIENT_SPACING_CLAMP, "SPACING_CLAMP", 0, "Clamp", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_gradient_fill_items[] = {
      {BRUSH_GRADIENT_LINEAR, "LINEAR", 0, "Linear", ""},
      {BRUSH_GRADIENT_RADIAL, "RADIAL", 0, "Radial", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_mask_pressure_items[] = {
      {0, "NONE", 0, "Off", ""},
      {BRUSH_MASK_PRESSURE_RAMP, "RAMP", ICON_STYLUS_PRESSURE, "Ramp", ""},
      {BRUSH_MASK_PRESSURE_CUTOFF, "CUTOFF", ICON_STYLUS_PRESSURE, "Cutoff", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_size_unit_items[] = {
      {0, "VIEW", 0, "View", "Measure brush size relative to the view"},
      {BRUSH_LOCK_SIZE, "SCENE", 0, "Scene", "Measure brush size relative to the scene"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem color_gradient_items[] = {
      {0, "COLOR", 0, "Color", "Paint with a single color"},
      {BRUSH_USE_GRADIENT, "GRADIENT", 0, "Gradient", "Paint with a gradient"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_spacing_unit_items[] = {
      {0, "VIEW", 0, "View", "Calculate brush spacing relative to the view"},
      {BRUSH_SCENE_SPACING,
       "SCENE",
       0,
       "Scene",
       "Calculate brush spacing relative to the scene using the stroke location"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_jitter_unit_items[] = {
      {BRUSH_ABSOLUTE_JITTER, "VIEW", 0, "View", "Jittering happens in screen space, in pixels"},
      {0, "BRUSH", 0, "Brush", "Jittering happens relative to the brush size"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem falloff_shape_unit_items[] = {
      {0, "SPHERE", 0, "Sphere", "Apply brush influence in a Sphere, outwards from the center"},
      {PAINT_FALLOFF_SHAPE_TUBE,
       "PROJECTED",
       0,
       "Projected",
       "Apply brush influence in a 2D circle, projected from the view"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_curve_preset_items[] = {
      {BRUSH_CURVE_CUSTOM, "CUSTOM", ICON_RNDCURVE, "Custom", ""},
      {BRUSH_CURVE_SMOOTH, "SMOOTH", ICON_SMOOTHCURVE, "Smooth", ""},
      {BRUSH_CURVE_SMOOTHER, "SMOOTHER", ICON_SMOOTHCURVE, "Smoother", ""},
      {BRUSH_CURVE_SPHERE, "SPHERE", ICON_SPHERECURVE, "Sphere", ""},
      {BRUSH_CURVE_ROOT, "ROOT", ICON_ROOTCURVE, "Root", ""},
      {BRUSH_CURVE_SHARP, "SHARP", ICON_SHARPCURVE, "Sharp", ""},
      {BRUSH_CURVE_LIN, "LIN", ICON_LINCURVE, "Linear", ""},
      {BRUSH_CURVE_POW4, "POW4", ICON_SHARPCURVE, "Sharper", ""},
      {BRUSH_CURVE_INVSQUARE, "INVSQUARE", ICON_INVERSESQUARECURVE, "Inverse Square", ""},
      {BRUSH_CURVE_CONSTANT, "CONSTANT", ICON_NOCURVE, "Constant", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_deformation_target_items[] = {
      {BRUSH_DEFORM_TARGET_GEOMETRY,
       "GEOMETRY",
       0,
       "Geometry",
       "Brush deformation displaces the vertices of the mesh"},
      {BRUSH_DEFORM_TARGET_CLOTH_SIM,
       "CLOTH_SIM",
       0,
       "Cloth Simulation",
       "Brush deforms the mesh by deforming the constraints of a cloth simulation"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_elastic_deform_type_items[] = {
      {BRUSH_ELASTIC_DEFORM_GRAB, "GRAB", 0, "Grab", ""},
      {BRUSH_ELASTIC_DEFORM_GRAB_BISCALE, "GRAB_BISCALE", 0, "Bi-Scale Grab", ""},
      {BRUSH_ELASTIC_DEFORM_GRAB_TRISCALE, "GRAB_TRISCALE", 0, "Tri-Scale Grab", ""},
      {BRUSH_ELASTIC_DEFORM_SCALE, "SCALE", 0, "Scale", ""},
      {BRUSH_ELASTIC_DEFORM_TWIST, "TWIST", 0, "Twist", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_snake_hook_deform_type_items[] = {
      {BRUSH_SNAKE_HOOK_DEFORM_FALLOFF,
       "FALLOFF",
       0,
       "Radius Falloff",
       "Applies the brush falloff in the tip of the brush"},
      {BRUSH_SNAKE_HOOK_DEFORM_ELASTIC,
       "ELASTIC",
       0,
       "Elastic",
       "Modifies the entire mesh using elastic deform"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_cloth_deform_type_items[] = {
      {BRUSH_CLOTH_DEFORM_DRAG, "DRAG", 0, "Drag", ""},
      {BRUSH_CLOTH_DEFORM_PUSH, "PUSH", 0, "Push", ""},
      {BRUSH_CLOTH_DEFORM_PINCH_POINT, "PINCH_POINT", 0, "Pinch Point", ""},
      {BRUSH_CLOTH_DEFORM_PINCH_PERPENDICULAR,
       "PINCH_PERPENDICULAR",
       0,
       "Pinch Perpendicular",
       ""},
      {BRUSH_CLOTH_DEFORM_INFLATE, "INFLATE", 0, "Inflate", ""},
      {BRUSH_CLOTH_DEFORM_GRAB, "GRAB", 0, "Grab", ""},
      {BRUSH_CLOTH_DEFORM_EXPAND, "EXPAND", 0, "Expand", ""},
      {BRUSH_CLOTH_DEFORM_SNAKE_HOOK, "SNAKE_HOOK", 0, "Snake Hook", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_cloth_force_falloff_type_items[] = {
      {BRUSH_CLOTH_FORCE_FALLOFF_RADIAL, "RADIAL", 0, "Radial", ""},
      {BRUSH_CLOTH_FORCE_FALLOFF_PLANE, "PLANE", 0, "Plane", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_boundary_falloff_type_items[] = {
      {BRUSH_BOUNDARY_FALLOFF_CONSTANT,
       "CONSTANT",
       0,
       "Constant",
       "Applies the same deformation in the entire boundary"},
      {BRUSH_BOUNDARY_FALLOFF_RADIUS,
       "RADIUS",
       0,
       "Brush Radius",
       "Applies the deformation in a localized area limited by the brush radius"},
      {BRUSH_BOUNDARY_FALLOFF_LOOP,
       "LOOP",
       0,
       "Loop",
       "Applies the brush falloff in a loop pattern"},
      {BRUSH_BOUNDARY_FALLOFF_LOOP_INVERT,
       "LOOP_INVERT",
       0,
       "Loop and Invert",
       "Applies the falloff radius in a loop pattern, inverting the displacement direction in "
       "each pattern repetition"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_cloth_simulation_area_type_items[] = {
      {BRUSH_CLOTH_SIMULATION_AREA_LOCAL,
       "LOCAL",
       0,
       "Local",
       "Simulates only a specific area around the brush limited by a fixed radius"},
      {BRUSH_CLOTH_SIMULATION_AREA_GLOBAL, "GLOBAL", 0, "Global", "Simulates the entire mesh"},
      {BRUSH_CLOTH_SIMULATION_AREA_DYNAMIC,
       "DYNAMIC",
       0,
       "Dynamic",
       "The active simulation area moves with the brush"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_smooth_deform_type_items[] = {
      {BRUSH_SMOOTH_DEFORM_LAPLACIAN,
       "LAPLACIAN",
       0,
       "Laplacian",
       "Smooths the surface and the volume"},
      {BRUSH_SMOOTH_DEFORM_SURFACE,
       "SURFACE",
       0,
       "Surface",
       "Smooths the surface of the mesh, preserving the volume"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_pose_deform_type_items[] = {
      {BRUSH_POSE_DEFORM_ROTATE_TWIST, "ROTATE_TWIST", 0, "Rotate/Twist", ""},
      {BRUSH_POSE_DEFORM_SCALE_TRASLATE, "SCALE_TRANSLATE", 0, "Scale/Translate", ""},
      {BRUSH_POSE_DEFORM_SQUASH_STRETCH, "SQUASH_STRETCH", 0, "Squash & Stretch", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_pose_origin_type_items[] = {
      {BRUSH_POSE_ORIGIN_TOPOLOGY,
       "TOPOLOGY",
       0,
       "Topology",
       "Sets the rotation origin automatically using the topology and shape of the mesh as a "
       "guide"},
      {BRUSH_POSE_ORIGIN_FACE_SETS,
       "FACE_SETS",
       0,
       "Face Sets",
       "Creates a pose segment per face sets, starting from the active face set"},
      {BRUSH_POSE_ORIGIN_FACE_SETS_FK,
       "FACE_SETS_FK",
       0,
       "Face Sets FK",
       "Simulates an FK deformation using the Face Set under the cursor as control"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_smear_deform_type_items[] = {
      {BRUSH_SMEAR_DEFORM_DRAG, "DRAG", 0, "Drag", ""},
      {BRUSH_SMEAR_DEFORM_PINCH, "PINCH", 0, "Pinch", ""},
      {BRUSH_SMEAR_DEFORM_EXPAND, "EXPAND", 0, "Expand", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_slide_deform_type_items[] = {
      {BRUSH_SLIDE_DEFORM_DRAG, "DRAG", 0, "Drag", ""},
      {BRUSH_SLIDE_DEFORM_PINCH, "PINCH", 0, "Pinch", ""},
      {BRUSH_SLIDE_DEFORM_EXPAND, "EXPAND", 0, "Expand", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem brush_boundary_deform_type_items[] = {
      {BRUSH_BOUNDARY_DEFORM_BEND, "BEND", 0, "Bend", ""},
      {BRUSH_BOUNDARY_DEFORM_EXPAND, "EXPAND", 0, "Expand", ""},
      {BRUSH_BOUNDARY_DEFORM_INFLATE, "INFLATE", 0, "Inflate", ""},
      {BRUSH_BOUNDARY_DEFORM_GRAB, "GRAB", 0, "Grab", ""},
      {BRUSH_BOUNDARY_DEFORM_TWIST, "TWIST", 0, "Twist", ""},
      {BRUSH_BOUNDARY_DEFORM_SMOOTH, "SMOOTH", 0, "Smooth", ""},
      {BRUSH_BOUNDARY_DEFORM_CIRCLE, "CIRCLE", 0, "Circle", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "Brush", "ID");
  RNA_def_struct_ui_text(
      srna, "Brush", "Brush data-block for storing brush settings for painting and sculpting");
  RNA_def_struct_ui_icon(srna, ICON_BRUSH_DATA);

  /* enums */
  prop = RNA_def_property(srna, "blend", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_blend_items);
  RNA_def_property_ui_text(prop, "Blending Mode", "Brush blending mode");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  /**
   * Begin per-mode tool properties.
   *
   * keep in sync with #BKE_paint_get_tool_prop_id_from_paintmode
   */
  prop = RNA_def_property(srna, "sculpt_tool", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_brush_sculpt_tool_items);
  RNA_def_property_ui_text(prop, "Sculpt Tool", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_BRUSH);
  RNA_def_property_update(prop, 0, "rna_Brush_update_and_reset_icon");

  prop = RNA_def_property(srna, "uv_sculpt_tool", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_brush_uv_sculpt_tool_items);
  RNA_def_property_ui_text(prop, "Sculpt Tool", "");
  RNA_def_property_update(prop, 0, "rna_Brush_update_and_reset_icon");

  prop = RNA_def_property(srna, "vertex_tool", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "vertexpaint_tool");
  RNA_def_property_enum_items(prop, rna_enum_brush_vertex_tool_items);
  RNA_def_property_ui_text(prop, "Vertex Paint Tool", "");
  RNA_def_property_update(prop, 0, "rna_Brush_update_and_reset_icon");

  prop = RNA_def_property(srna, "weight_tool", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "weightpaint_tool");
  RNA_def_property_enum_items(prop, rna_enum_brush_weight_tool_items);
  RNA_def_property_ui_text(prop, "Weight Paint Tool", "");
  RNA_def_property_update(prop, 0, "rna_Brush_update_and_reset_icon");

  prop = RNA_def_property(srna, "image_tool", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "imagepaint_tool");
  RNA_def_property_enum_items(prop, rna_enum_brush_image_tool_items);
  RNA_def_property_ui_text(prop, "Image Paint Tool", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_BRUSH);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_Brush_update_and_reset_icon");

  prop = RNA_def_property(srna, "gpencil_tool", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "gpencil_tool");
  RNA_def_property_enum_items(prop, rna_enum_brush_gpencil_types_items);
  RNA_def_property_ui_text(prop, "Grease Pencil Draw Tool", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_BRUSH);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "gpencil_vertex_tool", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "gpencil_vertex_tool");
  RNA_def_property_enum_items(prop, rna_enum_brush_gpencil_vertex_types_items);
  RNA_def_property_ui_text(prop, "Grease Pencil Vertex Paint Tool", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "gpencil_sculpt_tool", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "gpencil_sculpt_tool");
  RNA_def_property_enum_items(prop, rna_enum_brush_gpencil_sculpt_types_items);
  RNA_def_property_ui_text(prop, "Grease Pencil Sculpt Paint Tool", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "gpencil_weight_tool", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "gpencil_weight_tool");
  RNA_def_property_enum_items(prop, rna_enum_brush_gpencil_weight_types_items);
  RNA_def_property_ui_text(prop, "Grease Pencil Weight Paint Tool", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "curves_sculpt_tool", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_brush_curves_sculpt_tool_items);
  RNA_def_property_ui_text(prop, "Curves Sculpt Tool", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  /** End per mode tool properties. */

  prop = RNA_def_property(srna, "direction", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, prop_direction_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Brush_direction_itemf");
  RNA_def_property_ui_text(prop, "Direction", "");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "stroke_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, sculpt_stroke_method_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Brush_stroke_itemf");
  RNA_def_property_ui_text(prop, "Stroke Method", "");
  RNA_def_property_update(prop, 0, "rna_Brush_stroke_update");

  prop = RNA_def_property(srna, "sculpt_plane", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_sculpt_plane_items);
  RNA_def_property_ui_text(prop, "Sculpt Plane", "");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "mask_tool", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_mask_tool_items);
  RNA_def_property_ui_text(prop, "Mask Tool", "");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "curve_preset", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_curve_preset_items);
  RNA_def_property_ui_text(prop, "Curve Preset", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_CURVES); /* Abusing id_curves :/ */
  RNA_def_property_update(prop, 0, "rna_Brush_update");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "deform_target", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_deformation_target_items);
  RNA_def_property_ui_text(
      prop, "Deformation Target", "How the deformation of the brush will affect the object");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "elastic_deform_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_elastic_deform_type_items);
  RNA_def_property_ui_text(prop, "Deformation", "Deformation type that is used in the brush");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_BRUSH);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "snake_hook_deform_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_snake_hook_deform_type_items);
  RNA_def_property_ui_text(prop, "Deformation", "Deformation type that is used in the brush");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "cloth_deform_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_cloth_deform_type_items);
  RNA_def_property_ui_text(prop, "Deformation", "Deformation type that is used in the brush");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "cloth_force_falloff_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_cloth_force_falloff_type_items);
  RNA_def_property_ui_text(
      prop, "Force Falloff", "Shape used in the brush to apply force to the cloth");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "cloth_simulation_area_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_cloth_simulation_area_type_items);
  RNA_def_property_ui_text(
      prop,
      "Simulation Area",
      "Part of the mesh that is going to be simulated when the stroke is active");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "boundary_falloff_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_boundary_falloff_type_items);
  RNA_def_property_ui_text(
      prop, "Boundary Falloff", "How the brush falloff is applied across the boundary");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "smooth_deform_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_smooth_deform_type_items);
  RNA_def_property_ui_text(prop, "Deformation", "Deformation type that is used in the brush");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "smear_deform_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_smear_deform_type_items);
  RNA_def_property_ui_text(prop, "Deformation", "Deformation type that is used in the brush");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "smear_deform_blend", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "smear_deform_blend");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Blend", "Smear deformation blend");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "slide_deform_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_slide_deform_type_items);
  RNA_def_property_ui_text(prop, "Deformation", "Deformation type that is used in the brush");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "boundary_deform_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_boundary_deform_type_items);
  RNA_def_property_ui_text(prop, "Deformation", "Deformation type that is used in the brush");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_BRUSH);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "pose_deform_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_pose_deform_type_items);
  RNA_def_property_ui_text(prop, "Deformation", "Deformation type that is used in the brush");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "pose_origin_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_pose_origin_type_items);
  RNA_def_property_ui_text(prop,
                           "Rotation Origins",
                           "Method to set the rotation origins for the segments of the brush");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "jitter_unit", PROP_ENUM, PROP_NONE); /* as an enum */
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, brush_jitter_unit_items);
  RNA_def_property_ui_text(
      prop, "Jitter Unit", "Jitter in screen space or relative to brush size");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "falloff_shape", PROP_ENUM, PROP_NONE); /* as an enum */
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "falloff_shape");
  RNA_def_property_enum_items(prop, falloff_shape_unit_items);
  RNA_def_property_ui_text(prop, "Falloff Shape", "Use projected or spherical falloff");
  RNA_def_property_update(prop, 0, "rna_Brush_update");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  /* number values */
  prop = RNA_def_property(srna, "size", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_funcs(prop, nullptr, "rna_Brush_set_size", nullptr);
  RNA_def_property_range(prop, 1, MAX_BRUSH_PIXEL_RADIUS * 10);
  RNA_def_property_ui_range(prop, 1, MAX_BRUSH_PIXEL_RADIUS, 1, -1);
  RNA_def_property_ui_text(prop, "Radius", "Radius of the brush in pixels");
  RNA_def_property_update(prop, 0, "rna_Brush_size_update");

  prop = RNA_def_property(srna, "unprojected_radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_funcs(prop, nullptr, "rna_Brush_set_unprojected_radius", nullptr);
  RNA_def_property_range(prop, 0.001, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001, 1, 1, -1);
  RNA_def_property_ui_text(prop, "Unprojected Radius", "Radius of brush in Blender units");
  RNA_def_property_update(prop, 0, "rna_Brush_size_update");

  prop = RNA_def_property(srna, "jitter", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "jitter");
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 0.1, 4);
  RNA_def_property_ui_text(prop, "Jitter", "Jitter the position of the brush while painting");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_BRUSH);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "jitter_absolute", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "jitter_absolute");
  RNA_def_property_range(prop, 0, 1000000);
  RNA_def_property_ui_text(
      prop, "Jitter", "Jitter the position of the brush in pixels while painting");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_BRUSH);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "spacing", PROP_INT, PROP_PERCENTAGE);
  RNA_def_property_int_sdna(prop, nullptr, "spacing");
  RNA_def_property_range(prop, 1, 1000);
  RNA_def_property_ui_range(prop, 1, 500, 5, -1);
  RNA_def_property_ui_text(
      prop, "Spacing", "Spacing between brush daubs as a percentage of brush diameter");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "auto_smooth_spacing", PROP_INT, PROP_PERCENTAGE);
  RNA_def_property_int_sdna(prop, nullptr, "autosmooth_spacing");
  RNA_def_property_range(prop, 1, 1000);
  RNA_def_property_ui_range(prop, 1, 500, 5, -1);
  RNA_def_property_ui_text(
      prop, "Auto-Smooth Spacing", "Autosmooth spacing as a percentage of brush diameter");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "topology_rake_spacing", PROP_INT, PROP_PERCENTAGE);
  RNA_def_property_int_sdna(prop, nullptr, "topology_rake_spacing");
  RNA_def_property_range(prop, 1, 1000);
  RNA_def_property_ui_range(prop, 1, 500, 5, -1);
  RNA_def_property_ui_text(
      prop, "Topology Rake Spacing", "Topology rake spacing as a percentage of brush diameter");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "grad_spacing", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "gradient_spacing");
  RNA_def_property_range(prop, 1, 10000);
  RNA_def_property_ui_range(prop, 1, 10000, 5, -1);
  RNA_def_property_ui_text(
      prop, "Gradient Spacing", "Spacing before brush gradient goes full circle");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "smooth_stroke_radius", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 10, 200);
  RNA_def_property_ui_text(
      prop, "Smooth Stroke Radius", "Minimum distance from last point before stroke continues");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "smooth_stroke_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.5, 0.99);
  RNA_def_property_ui_text(prop, "Smooth Stroke Factor", "Higher values give a smoother stroke");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "rate", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "rate");
  RNA_def_property_range(prop, 0.0001f, 10000.0f);
  RNA_def_property_ui_range(prop, 0.01f, 1.0f, 1, 3);
  RNA_def_property_ui_text(prop, "Rate", "Interval between paints for Airbrush");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_float_sdna(prop, nullptr, "rgb");
  RNA_def_property_ui_text(prop, "Color", "");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "secondary_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_float_sdna(prop, nullptr, "secondary_rgb");
  RNA_def_property_ui_text(prop, "Secondary Color", "");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(prop, "Weight", "Vertex weight when brush is applied");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "alpha");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(
      prop, "Strength", "How powerful the effect of the brush is when applied");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "flow", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "flow");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(prop, "Flow", "Amount of paint that is applied per stroke sample");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "wet_mix", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "wet_mix");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(
      prop, "Wet Mix", "Amount of paint that is picked from the surface into the brush color");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "wet_persistence", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "wet_persistence");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(
      prop,
      "Wet Persistence",
      "Amount of wet paint that stays in the brush after applying paint to the surface");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "density", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "density");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(
      prop, "Density", "Amount of random elements that are going to be affected by the brush");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "tip_scale_x", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "tip_scale_x");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(prop, "Tip Scale X", "Scale of the brush tip in the X axis");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_hardness_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "paint_flags", BRUSH_PAINT_HARDNESS_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure for Hardness", "Use pressure to modulate hardness");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "invert_hardness_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "paint_flags", BRUSH_PAINT_HARDNESS_PRESSURE_INVERT);
  RNA_def_property_ui_icon(prop, ICON_ARROW_LEFTRIGHT, 0);
  RNA_def_property_ui_text(
      prop, "Invert Pressure for Hardness", "Invert the modulation of pressure in hardness");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_flow_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "paint_flags", BRUSH_PAINT_FLOW_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure for Flow", "Use pressure to modulate flow");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "invert_flow_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "paint_flags", BRUSH_PAINT_FLOW_PRESSURE_INVERT);
  RNA_def_property_ui_icon(prop, ICON_ARROW_LEFTRIGHT, 0);
  RNA_def_property_ui_text(
      prop, "Invert Pressure for Flow", "Invert the modulation of pressure in flow");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_wet_mix_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "paint_flags", BRUSH_PAINT_WET_MIX_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure for Wet Mix", "Use pressure to modulate wet mix");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "invert_wet_mix_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "paint_flags", BRUSH_PAINT_WET_MIX_PRESSURE_INVERT);
  RNA_def_property_ui_icon(prop, ICON_ARROW_LEFTRIGHT, 0);
  RNA_def_property_ui_text(
      prop, "Invert Pressure for Wet Mix", "Invert the modulation of pressure in wet mix");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_wet_persistence_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "paint_flags", BRUSH_PAINT_WET_PERSISTENCE_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(
      prop, "Use Pressure for Wet Persistence", "Use pressure to modulate wet persistence");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "invert_wet_persistence_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "paint_flags", BRUSH_PAINT_WET_PERSISTENCE_PRESSURE_INVERT);
  RNA_def_property_ui_icon(prop, ICON_ARROW_LEFTRIGHT, 0);
  RNA_def_property_ui_text(prop,
                           "Invert Pressure for Wet Persistence",
                           "Invert the modulation of pressure in wet persistence");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_density_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "paint_flags", BRUSH_PAINT_DENSITY_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure for Density", "Use pressure to modulate density");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "invert_density_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "paint_flags", BRUSH_PAINT_DENSITY_PRESSURE_INVERT);
  RNA_def_property_ui_icon(prop, ICON_ARROW_LEFTRIGHT, 0);
  RNA_def_property_ui_text(
      prop, "Invert Pressure for Density", "Invert the modulation of pressure in density");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "dash_ratio", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "dash_ratio");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(
      prop, "Dash Ratio", "Ratio of samples in a cycle that the brush is enabled");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "dash_samples", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "dash_samples");
  RNA_def_property_range(prop, 1, 10000);
  RNA_def_property_ui_range(prop, 1, 10000, 5, -1);
  RNA_def_property_ui_text(
      prop, "Dash Length", "Length of a dash cycle measured in stroke samples");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "plane_offset", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "plane_offset");
  RNA_def_property_float_default(prop, 0);
  RNA_def_property_range(prop, -2.0f, 2.0f);
  RNA_def_property_ui_range(prop, -0.5f, 0.5f, 0.001, 3);
  RNA_def_property_ui_text(
      prop,
      "Plane Offset",
      "Adjust plane on which the brush acts towards or away from the object surface");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "plane_trim", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "plane_trim");
  RNA_def_property_range(prop, 0, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Plane Trim",
      "If a vertex is further away from offset plane than this, then it is not affected");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "height", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "height");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0, 1.0f);
  RNA_def_property_ui_range(prop, 0, 0.2f, 1, 3);
  RNA_def_property_ui_text(
      prop, "Brush Height", "Affectable height of brush (layer height for layer tool, i.e.)");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "texture_sample_bias", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "texture_sample_bias");
  RNA_def_property_float_default(prop, 0);
  RNA_def_property_range(prop, -1, 1);
  RNA_def_property_ui_text(prop, "Texture Sample Bias", "Value added to texture samples");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_color_as_displacement", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_USE_COLOR_AS_DISPLACEMENT);
  RNA_def_property_ui_text(prop,
                           "Vector Displacement",
                           "Handles each pixel color as individual vector for displacement. Works "
                           "only with area plane mapping");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "normal_weight", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "normal_weight");
  RNA_def_property_float_default(prop, 0);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Normal Weight", "How much grab will pull vertices out of surface during a grab");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "elastic_deform_volume_preservation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "elastic_deform_volume_preservation");
  RNA_def_property_range(prop, 0.0f, 0.9f);
  RNA_def_property_ui_range(prop, 0.0f, 0.9f, 0.01f, 3);
  RNA_def_property_ui_text(prop,
                           "Volume Preservation",
                           "Poisson ratio for elastic deformation. Higher values preserve volume "
                           "more, but also lead to more bulging");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "rake_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "rake_factor");
  RNA_def_property_float_default(prop, 0);
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(prop, "Rake", "How much grab will follow cursor rotation");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "crease_pinch_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "crease_pinch_factor");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Crease Brush Pinch Factor", "How much the crease brush pinches");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "pose_offset", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "pose_offset");
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_ui_text(
      prop, "Pose Origin Offset", "Offset of the pose origin in relation to the brush radius");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "disconnected_distance_max", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "disconnected_distance_max");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_text(prop,
                           "Max Element Distance",
                           "Maximum distance to search for disconnected loose parts in the mesh");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "boundary_offset", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "boundary_offset");
  RNA_def_property_range(prop, 0.0f, 30.0f);
  RNA_def_property_ui_text(prop,
                           "Boundary Origin Offset",
                           "Offset of the boundary origin in relation to the brush radius");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "surface_smooth_shape_preservation", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "surface_smooth_shape_preservation");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Shape Preservation", "How much of the original shape is preserved when smoothing");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "surface_smooth_current_vertex", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "surface_smooth_current_vertex");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Per Vertex Displacement",
      "How much the position of each individual vertex influences the final result");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "surface_smooth_iterations", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "surface_smooth_iterations");
  RNA_def_property_range(prop, 1, 10);
  RNA_def_property_ui_range(prop, 1, 10, 1, 3);
  RNA_def_property_ui_text(prop, "Iterations", "Number of smoothing iterations per brush step");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "multiplane_scrape_angle", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "multiplane_scrape_angle");
  RNA_def_property_range(prop, 0.0f, 160.0f);
  RNA_def_property_ui_text(prop, "Plane Angle", "Angle between the planes of the crease");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "pose_smooth_iterations", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "pose_smooth_iterations");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(
      prop,
      "Smooth Iterations",
      "Smooth iterations applied after calculating the pose factor of each vertex");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "pose_ik_segments", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "pose_ik_segments");
  RNA_def_property_range(prop, 1, 20);
  RNA_def_property_ui_range(prop, 1, 20, 1, 3);
  RNA_def_property_ui_text(
      prop,
      "Pose IK Segments",
      "Number of segments of the inverse kinematics chain that will deform the mesh");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "tip_roundness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "tip_roundness");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Tip Roundness", "Roundness of the brush tip");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "cloth_mass", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "cloth_mass");
  RNA_def_property_range(prop, 0.01f, 2.0f);
  RNA_def_property_ui_text(prop, "Cloth Mass", "Mass of each simulation particle");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "cloth_damping", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "cloth_damping");
  RNA_def_property_range(prop, 0.01f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Cloth Damping", "How much the applied forces are propagated through the cloth");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "cloth_sim_limit", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "cloth_sim_limit");
  RNA_def_property_range(prop, 0.1f, 10.0f);
  RNA_def_property_ui_text(
      prop,
      "Simulation Limit",
      "Factor added relative to the size of the radius to limit the cloth simulation effects");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "cloth_sim_falloff", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "cloth_sim_falloff");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop,
                           "Simulation Falloff",
                           "Area to apply deformation falloff to the effects of the simulation");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "cloth_constraint_softbody_strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "cloth_constraint_softbody_strength");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Soft Body Plasticity",
      "How much the cloth preserves the original shape, acting as a soft body");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "hardness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "hardness");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Hardness", "How close the brush falloff starts from the edge of the brush");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(
      srna, "automasking_boundary_edges_propagation_steps", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "automasking_boundary_edges_propagation_steps");
  RNA_def_property_range(prop, 1, 20);
  RNA_def_property_ui_range(prop, 1, 20, 1, 3);
  RNA_def_property_ui_text(prop,
                           "Propagation Steps",
                           "Distance where boundary edge automasking is going to protect vertices "
                           "from the fully masked edge");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "auto_smooth_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "autosmooth_factor");
  RNA_def_property_float_default(prop, 0);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(
      prop, "Auto-Smooth", "Amount of smoothing to automatically apply to each stroke");

  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "auto_smooth_projection", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "autosmooth_projection");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(prop, "Preserve Volume", "How much autosmooth should preserve volume.");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "sharp_angle_limit", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "sharp_angle_limit");
  RNA_def_property_range(prop, 0.0f, M_PI);
  RNA_def_property_ui_text(prop, "Sharp Limit", "");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  RNA_def_property_update(prop, 0, "rna_Brush_update");
  prop = RNA_def_property(srna, "auto_smooth_radius_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "autosmooth_radius_factor");
  RNA_def_property_range(prop, 0.001f, 5.0f);
  RNA_def_property_ui_range(prop, 0.001f, 2.0f, 0.15, 3);
  RNA_def_property_ui_text(prop,
                           "Smooth Radius",
                           "Ratio between the brush radius and the radius that is going to be "
                           "used for smoothing");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "concave_mask_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "concave_mask_factor");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(prop, "Cavity Mask", "Mask to concave areas");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "topology_rake_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "topology_rake_factor");
  RNA_def_property_float_default(prop, 0);
  RNA_def_property_range(prop, 0.0f, 5.0f);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 0.001, 3);
  RNA_def_property_ui_text(prop,
                           "Topology Rake",
                           "Automatically align edges to the brush direction to "
                           "generate cleaner topology and define sharp features. "
                           "Best used on low-poly meshes as it has a performance impact");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "topology_rake_radius_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "topology_rake_radius_factor");
  RNA_def_property_range(prop, 0.001f, 5.0f);
  RNA_def_property_ui_range(prop, 0.0f, 3.0f, 0.1, 2);
  RNA_def_property_ui_text(prop,
                           "Rake Radius",
                           "Ratio between the brush radius and the radius that is going to be "
                           "used for topology rake");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "topology_rake_projection", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "topology_rake_projection");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(prop,
                           "Projection",
                           "How much topology rake should stick to surface"
                           "Lower values with have smoothing effect");

  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "boundary_smooth_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "boundary_smooth_factor");
  RNA_def_property_float_default(prop, 0);
  RNA_def_property_range(prop, -2.0f, 2.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(prop, "Boundary Smoothing", "How much to smooth sharp boundaries ");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "hard_corner_pin", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "hard_corner_pin");
  RNA_def_property_range(prop, -2.0f, 2.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(prop, "Corner Pin", "How much to pin corners in hard edge mode.");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "tilt_strength_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "tilt_strength_factor");
  RNA_def_property_float_default(prop, 0);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(
      prop, "Tilt Strength", "How much the tilt of the pen will affect the brush");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "normal_radius_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "normal_radius_factor");
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 0.001, 3);
  RNA_def_property_ui_text(prop,
                           "Normal Radius",
                           "Ratio between the brush radius and the radius that is going to be "
                           "used to sample the normal");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "area_radius_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "area_radius_factor");
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 0.001, 3);
  RNA_def_property_ui_text(prop,
                           "Area Radius",
                           "Ratio between the brush radius and the radius that is going to be "
                           "used to sample the area center");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "wet_paint_radius_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "wet_paint_radius_factor");
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 0.001, 3);
  RNA_def_property_ui_text(prop,
                           "Wet Paint Radius",
                           "Ratio between the brush radius and the radius that is going to be "
                           "used to sample the color to blend in wet paint");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "stencil_pos", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "stencil_pos");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Stencil Position", "Position of stencil in viewport");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "stencil_dimension", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "stencil_dimension");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Stencil Dimensions", "Dimensions of stencil in viewport");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "mask_stencil_pos", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "mask_stencil_pos");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Mask Stencil Position", "Position of mask stencil in viewport");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "mask_stencil_dimension", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "mask_stencil_dimension");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(
      prop, "Mask Stencil Dimensions", "Dimensions of mask stencil in viewport");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "sharp_threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 100.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, 3);
  RNA_def_property_float_sdna(prop, nullptr, "sharp_threshold");
  RNA_def_property_ui_text(
      prop, "Sharp Threshold", "Threshold below which, no sharpening is done");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "fill_threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 100.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, 3);
  RNA_def_property_float_sdna(prop, nullptr, "fill_threshold");
  RNA_def_property_ui_text(
      prop, "Fill Threshold", "Threshold above which filling is not propagated");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "blur_kernel_radius", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "blur_kernel_radius");
  RNA_def_property_range(prop, 1, 10000);
  RNA_def_property_ui_range(prop, 1, 50, 1, -1);
  RNA_def_property_ui_text(
      prop, "Kernel Radius", "Radius of kernel used for soften and sharpen in pixels");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "blur_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, brush_blur_mode_items);
  RNA_def_property_ui_text(prop, "Blur Mode", "");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "falloff_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "falloff_angle");
  RNA_def_property_range(prop, 0, M_PI_2);
  RNA_def_property_ui_text(
      prop,
      "Falloff Angle",
      "Paint most on faces pointing towards the view according to this angle");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  /* flag */
  prop = RNA_def_property(srna, "use_airbrush", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_AIRBRUSH);
  RNA_def_property_ui_text(
      prop, "Airbrush", "Keep applying paint effect while holding mouse (spray)");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_original_normal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_ORIGINAL_NORMAL);
  RNA_def_property_ui_text(prop,
                           "Original Normal",
                           "When locked keep using normal of surface where stroke was initiated");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_original_plane", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_ORIGINAL_PLANE);
  RNA_def_property_ui_text(
      prop,
      "Original Plane",
      "When locked keep using the plane origin of surface where stroke was initiated");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  const EnumPropertyItem *entry = rna_enum_brush_automasking_flag_items;
  do {
    prop = RNA_def_property(srna, entry->identifier, PROP_BOOLEAN, PROP_NONE);
    RNA_def_property_boolean_sdna(prop, nullptr, "automasking_flags", entry->value);
    RNA_def_property_ui_text(prop, entry->name, entry->description);

    if (entry->value == BRUSH_AUTOMASKING_CAVITY_NORMAL) {
      RNA_def_property_boolean_funcs(prop, nullptr, "rna_Brush_automasking_cavity_set");
    }
    else if (entry->value == BRUSH_AUTOMASKING_CAVITY_INVERTED) {
      RNA_def_property_boolean_funcs(prop, nullptr, "rna_Brush_automasking_invert_cavity_set");
    }

    RNA_def_property_update(prop, 0, "rna_Brush_update");
  } while ((++entry)->identifier);

  prop = RNA_def_property(srna, "automasking_cavity_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "automasking_cavity_factor");
  RNA_def_property_ui_text(prop, "Cavity Factor", "The contrast of the cavity mask");
  RNA_def_property_range(prop, 0.0f, 5.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "automasking_cavity_blur_steps", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "automasking_cavity_blur_steps");
  RNA_def_property_int_default(prop, 0);
  RNA_def_property_ui_text(prop, "Blur Steps", "The number of times the cavity mask is blurred");
  RNA_def_property_range(prop, 0, 25);
  RNA_def_property_ui_range(prop, 0, 10, 1, 1);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "automasking_cavity_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "automasking_cavity_curve");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Cavity Curve", "Curve used for the sensitivity");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_automasking_start_normal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "automasking_flags", BRUSH_AUTOMASKING_BRUSH_NORMAL);
  RNA_def_property_ui_text(
      prop,
      "Area Normal",
      "Affect only vertices with a similar normal to where the stroke starts");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_automasking_view_normal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "automasking_flags", BRUSH_AUTOMASKING_VIEW_NORMAL);
  RNA_def_property_ui_text(
      prop, "View Normal", "Affect only vertices with a normal that faces the viewer");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_automasking_view_occlusion", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "automasking_flags", BRUSH_AUTOMASKING_VIEW_OCCLUSION);
  RNA_def_property_ui_text(
      prop,
      "Occlusion",
      "Only affect vertices that are not occluded by other faces. (Slower performance)");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_scene_spacing", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, brush_spacing_unit_items);
  RNA_def_property_ui_text(
      prop, "Spacing Distance", "Calculate the brush spacing using view or scene distance");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_grab_active_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_GRAB_ACTIVE_VERTEX);
  RNA_def_property_ui_text(
      prop,
      "Grab Active Vertex",
      "Apply the maximum grab strength to the active vertex instead of the cursor location");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_weighted_smooth", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_SMOOTH_USE_AREA_WEIGHT);
  RNA_def_property_ui_text(prop, "Weight By Area", "Weight by face area to get a smoother result");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "hard_edge_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_HARD_EDGE_MODE);
  RNA_def_property_ui_text(
      prop, "Hard Edge Mode", "Hard edge mode; treat all face set boundaries as hard edges");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_grab_silhouette", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_GRAB_SILHOUETTE);
  RNA_def_property_ui_text(
      prop, "Grab Silhouette", "Grabs trying to automask the silhouette of the object");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_curvature_rake", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_CURVATURE_RAKE);
  RNA_def_property_ui_text(
      prop, "Curvature Rake", "Topology rake follows curvature instead of brush direction");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "ignore_falloff_for_topology_rake", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_TOPOLOGY_RAKE_IGNORE_BRUSH_FALLOFF);
  RNA_def_property_ui_text(
      prop, "Ignore Brush Falloff", "Ignore brush falloff settings for topology rake");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_custom_auto_smooth_spacing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_CUSTOM_AUTOSMOOTH_SPACING);
  RNA_def_property_ui_text(
      prop,
      "Use Custom Autosmooth Spacing",
      "Use custom spacing for autosmooth (must be larger then brush spacing)");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_custom_topology_rake_spacing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_CUSTOM_TOPOLOGY_RAKE_SPACING);
  RNA_def_property_ui_text(
      prop,
      "Use Custom Rake Spacing",
      "Use custom spacing for topology rake (must be larger then brush spacing)");

  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_paint_antialiasing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "sampling_flag", BRUSH_PAINT_ANTIALIASING);
  RNA_def_property_ui_text(prop, "Anti-Aliasing", "Smooths the edges of the strokes");

  prop = RNA_def_property(srna, "use_multiplane_scrape_dynamic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_MULTIPLANE_SCRAPE_DYNAMIC);
  RNA_def_property_ui_text(prop,
                           "Dynamic Mode",
                           "The angle between the planes changes during the stroke to fit the "
                           "surface under the cursor");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "show_multiplane_scrape_planes_preview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_MULTIPLANE_SCRAPE_PLANES_PREVIEW);
  RNA_def_property_ui_text(
      prop, "Show Cursor Preview", "Preview the scrape planes in the cursor during the stroke");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_pose_ik_anchored", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_POSE_IK_ANCHORED);
  RNA_def_property_ui_text(
      prop, "Keep Anchor Point", "Keep the position of the last segment in the IK chain fixed");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_pose_lock_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_POSE_USE_LOCK_ROTATION);
  RNA_def_property_ui_text(prop,
                           "Lock Rotation When Scaling",
                           "Do not rotate the segment when using the scale deform mode");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_connected_only", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_USE_CONNECTED_ONLY);
  RNA_def_property_ui_text(prop, "Connected Only", "Affect only topologically connected elements");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_cloth_pin_simulation_boundary", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_CLOTH_PIN_SIMULATION_BOUNDARY);
  RNA_def_property_ui_text(
      prop,
      "Pin Simulation Boundary",
      "Lock the position of the vertices in the simulation falloff area to avoid artifacts and "
      "create a softer transition with unaffected areas");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_cloth_collision", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_CLOTH_USE_COLLISION);
  RNA_def_property_ui_text(prop, "Enable Collision", "Collide with objects during the simulation");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "invert_to_scrape_fill", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_INVERT_TO_SCRAPE_FILL);
  RNA_def_property_ui_text(prop,
                           "Invert to Scrape or Fill",
                           "Use Scrape or Fill tool when inverting this brush instead of "
                           "inverting its displacement direction");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_pressure_strength", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_ALPHA_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(
      prop, "Strength Pressure", "Enable tablet pressure sensitivity for strength");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_offset_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_OFFSET_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(
      prop, "Plane Offset Pressure", "Enable tablet pressure sensitivity for offset");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_pressure_area_radius", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", BRUSH_AREA_RADIUS_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(
      prop, "Area Radius Pressure", "Enable tablet pressure sensitivity for area radius");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_pressure_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_SIZE_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Size Pressure", "Enable tablet pressure sensitivity for size");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_pressure_jitter", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_JITTER_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(
      prop, "Jitter Pressure", "Enable tablet pressure sensitivity for jitter");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_pressure_spacing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_SPACING_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(
      prop, "Spacing Pressure", "Enable tablet pressure sensitivity for spacing");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_pressure_masking", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mask_pressure");
  RNA_def_property_enum_items(prop, brush_mask_pressure_items);
  RNA_def_property_ui_text(
      prop, "Mask Pressure Mode", "Pen pressure makes texture influence smaller");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_inverse_smooth_pressure", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_INVERSE_SMOOTH_PRESSURE);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(
      prop, "Inverse Smooth Pressure", "Lighter pressure causes more smoothing to be applied");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_plane_trim", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_PLANE_TRIM);
  RNA_def_property_ui_text(
      prop,
      "Use Plane Trim",
      "Limit the distance from the offset plane that a vertex can be affected");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_frontface", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_FRONTFACE);
  RNA_def_property_ui_text(
      prop, "Use Front-Face", "Brush only affects vertices that face the viewer");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_frontface_falloff", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_FRONTFACE_FALLOFF);
  RNA_def_property_ui_text(
      prop, "Use Front-Face Falloff", "Blend brush influence by how much they face the front");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_anchor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_ANCHORED);
  RNA_def_property_ui_text(prop, "Anchored", "Keep the brush anchored to the initial location");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_space", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_SPACE);
  RNA_def_property_ui_text(
      prop, "Space", "Limit brush application to the distance specified by spacing");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_line", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_LINE);
  RNA_def_property_ui_text(prop, "Line", "Draw a line with dabs separated according to spacing");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_CURVE);
  RNA_def_property_ui_text(
      prop,
      "Curve",
      "Define the stroke curve with a bezier curve. Dabs are separated according to spacing");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_smooth_stroke", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_SMOOTH_STROKE);
  RNA_def_property_ui_text(
      prop, "Smooth Stroke", "Brush lags behind mouse and follows a smoother path");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_persistent", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_PERSISTENT);
  RNA_def_property_ui_text(prop, "Persistent", "Sculpt on a persistent layer of the mesh");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_accumulate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_ACCUMULATE);
  RNA_def_property_ui_text(prop, "Accumulate", "Accumulate stroke daubs on top of each other");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_space_attenuation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_SPACE_ATTEN);
  RNA_def_property_ui_text(
      prop,
      "Adjust Strength for Spacing",
      "Automatically adjust strength to give consistent results for different spacings");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  /* adaptive space is not implemented yet */
  prop = RNA_def_property(srna, "use_adaptive_space", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_ADAPTIVE_SPACE);
  RNA_def_property_ui_text(prop,
                           "Adaptive Spacing",
                           "Space daubs according to surface orientation instead of screen space");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_locked_size", PROP_ENUM, PROP_NONE); /* as an enum */
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, brush_size_unit_items);
  RNA_def_property_ui_text(
      prop, "Radius Unit", "Measure brush size relative to the view or the scene");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "color_type", PROP_ENUM, PROP_NONE); /* as an enum */
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, color_gradient_items);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_Brush_use_gradient_set", nullptr);
  RNA_def_property_ui_text(prop, "Color Type", "Use single color or gradient when painting");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_edge_to_edge", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_EDGE_TO_EDGE);
  RNA_def_property_ui_text(prop, "Edge-to-Edge", "Drag anchor brush from edge-to-edge");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_restore_mesh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_DRAG_DOT);
  RNA_def_property_ui_text(prop, "Restore Mesh", "Allow a single dot to be carefully positioned");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  /* only for projection paint & vertex paint, TODO: other paint modes. */
  prop = RNA_def_property(srna, "use_alpha", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", BRUSH_LOCK_ALPHA);
  RNA_def_property_ui_text(
      prop, "Affect Alpha", "When this is disabled, lock alpha while painting");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Curve", "Editable falloff curve");
  RNA_def_property_update(prop, 0, "rna_Brush_update");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "pressure_size_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Pressure/Size Curve", "Pressure/Size input curve");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "pressure_strength_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Pressure/Strength Curve", "Pressure/Strength input curve");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "paint_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Paint Curve", "Active paint curve");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "gradient", PROP_POINTER, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "gradient");
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
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay_flags", BRUSH_OVERLAY_PRIMARY);
  RNA_def_property_ui_text(prop, "Use Texture Overlay", "Show texture in viewport");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_secondary_overlay", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay_flags", BRUSH_OVERLAY_SECONDARY);
  RNA_def_property_ui_text(prop, "Use Texture Overlay", "Show texture in viewport");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_cursor_overlay", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay_flags", BRUSH_OVERLAY_CURSOR);
  RNA_def_property_ui_text(prop, "Use Cursor Overlay", "Show cursor in viewport");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_cursor_overlay_override", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "overlay_flags", BRUSH_OVERLAY_CURSOR_OVERRIDE_ON_STROKE);
  RNA_def_property_ui_text(prop, "Override Overlay", "Don't show overlay during a stroke");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_primary_overlay_override", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "overlay_flags", BRUSH_OVERLAY_PRIMARY_OVERRIDE_ON_STROKE);
  RNA_def_property_ui_text(prop, "Override Overlay", "Don't show overlay during a stroke");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_secondary_overlay_override", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "overlay_flags", BRUSH_OVERLAY_SECONDARY_OVERRIDE_ON_STROKE);
  RNA_def_property_ui_text(prop, "Override Overlay", "Don't show overlay during a stroke");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  /* paint mode flags */
  prop = RNA_def_property(srna, "use_paint_sculpt", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "ob_mode", OB_MODE_SCULPT);
  RNA_def_property_ui_text(prop, "Use Sculpt", "Use this brush in sculpt mode");

  prop = RNA_def_property(srna, "use_paint_uv_sculpt", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "ob_mode", OB_MODE_EDIT);
  RNA_def_property_ui_text(prop, "Use UV Sculpt", "Use this brush in UV sculpt mode");

  prop = RNA_def_property(srna, "use_paint_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "ob_mode", OB_MODE_VERTEX_PAINT);
  RNA_def_property_ui_text(prop, "Use Vertex", "Use this brush in vertex paint mode");

  prop = RNA_def_property(srna, "use_paint_weight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "ob_mode", OB_MODE_WEIGHT_PAINT);
  RNA_def_property_ui_text(prop, "Use Weight", "Use this brush in weight paint mode");

  prop = RNA_def_property(srna, "use_paint_image", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "ob_mode", OB_MODE_TEXTURE_PAINT);
  RNA_def_property_ui_text(prop, "Use Texture", "Use this brush in texture paint mode");

  prop = RNA_def_property(srna, "use_paint_grease_pencil", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "ob_mode", OB_MODE_PAINT_GPENCIL_LEGACY);
  RNA_def_property_ui_text(prop, "Use Paint", "Use this brush in grease pencil drawing mode");

  prop = RNA_def_property(srna, "use_vertex_grease_pencil", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "ob_mode", OB_MODE_VERTEX_GPENCIL_LEGACY);
  RNA_def_property_ui_text(
      prop, "Use Vertex", "Use this brush in grease pencil vertex color mode");

  prop = RNA_def_property(srna, "use_paint_sculpt_curves", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "ob_mode", OB_MODE_SCULPT_CURVES);
  RNA_def_property_ui_text(prop, "Use Sculpt", "Use this brush in sculpt curves mode");

  /* texture */
  prop = RNA_def_property(srna, "texture_slot", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "BrushTextureSlot");
  RNA_def_property_pointer_sdna(prop, nullptr, "mtex");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Texture Slot", "");

  prop = RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "mtex.tex");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Texture", "");
  RNA_def_property_update(prop, NC_TEXTURE, "rna_Brush_main_tex_update");

  prop = RNA_def_property(srna, "mask_texture_slot", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "BrushTextureSlot");
  RNA_def_property_pointer_sdna(prop, nullptr, "mask_mtex");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Mask Texture Slot", "");

  prop = RNA_def_property(srna, "mask_texture", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "mask_mtex.tex");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Mask Texture", "");
  RNA_def_property_update(prop, NC_TEXTURE, "rna_Brush_secondary_tex_update");

  prop = RNA_def_property(srna, "texture_overlay_alpha", PROP_INT, PROP_PERCENTAGE);
  RNA_def_property_int_sdna(prop, nullptr, "texture_overlay_alpha");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Texture Overlay Alpha", "");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "mask_overlay_alpha", PROP_INT, PROP_PERCENTAGE);
  RNA_def_property_int_sdna(prop, nullptr, "mask_overlay_alpha");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Mask Texture Overlay Alpha", "");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "cursor_overlay_alpha", PROP_INT, PROP_PERCENTAGE);
  RNA_def_property_int_sdna(prop, nullptr, "cursor_overlay_alpha");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Mask Texture Overlay Alpha", "");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "cursor_color_add", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "add_col");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Add Color", "Color of cursor when adding");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "cursor_color_subtract", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "sub_col");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Subtract Color", "Color of cursor when subtracting");
  RNA_def_property_update(prop, 0, "rna_Brush_update");

  prop = RNA_def_property(srna, "use_custom_icon", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BRUSH_CUSTOM_ICON);
  RNA_def_property_ui_text(prop, "Custom Icon", "Set the brush icon from an image file");
  RNA_def_property_update(prop, 0, "rna_Brush_icon_update");

  prop = RNA_def_property(srna, "icon_filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_sdna(prop, nullptr, "icon_filepath");
  RNA_def_property_ui_text(prop, "Brush Icon Filepath", "File path to brush icon");
  RNA_def_property_update(prop, 0, "rna_Brush_icon_update");

  /* clone tool */
  prop = RNA_def_property(srna, "clone_image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "clone.image");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Clone Image", "Image for clone tool");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_Brush_update");
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, "rna_Brush_imagetype_poll");

  prop = RNA_def_property(srna, "clone_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "clone.alpha");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Clone Alpha", "Opacity of clone image display");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_Brush_update");

  prop = RNA_def_property(srna, "clone_offset", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "clone.offset");
  RNA_def_property_ui_text(prop, "Clone Offset", "");
  RNA_def_property_ui_range(prop, -1.0f, 1.0f, 10.0f, 3);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_Brush_update");

  prop = RNA_def_property(srna, "brush_capabilities", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "BrushCapabilities");
  RNA_def_property_pointer_funcs(prop, "rna_Brush_capabilities_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Brush Capabilities", "Brush's capabilities");

  /* brush capabilities (mode-dependent) */
  prop = RNA_def_property(srna, "sculpt_capabilities", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "BrushCapabilitiesSculpt");
  RNA_def_property_pointer_funcs(
      prop, "rna_Sculpt_tool_capabilities_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Sculpt Capabilities", "");

  prop = RNA_def_property(srna, "image_paint_capabilities", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "BrushCapabilitiesImagePaint");
  RNA_def_property_pointer_funcs(
      prop, "rna_Imapaint_tool_capabilities_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Image Paint Capabilities", "");

  prop = RNA_def_property(srna, "vertex_paint_capabilities", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "BrushCapabilitiesVertexPaint");
  RNA_def_property_pointer_funcs(
      prop, "rna_Vertexpaint_tool_capabilities_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Vertex Paint Capabilities", "");

  prop = RNA_def_property(srna, "weight_paint_capabilities", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "BrushCapabilitiesWeightPaint");
  RNA_def_property_pointer_funcs(
      prop, "rna_Weightpaint_tool_capabilities_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Weight Paint Capabilities", "");

  prop = RNA_def_property(srna, "gpencil_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "BrushGpencilSettings");
  RNA_def_property_pointer_sdna(prop, nullptr, "gpencil_settings");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Gpencil Settings", "");

  prop = RNA_def_property(srna, "dyntopo", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "DynTopoSettings");
  RNA_def_property_pointer_sdna(prop, nullptr, "dyntopo");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Dyntopo Settings", "");

  prop = RNA_def_property(srna, "curves_sculpt_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "BrushCurvesSculptSettings");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Curves Sculpt Settings", "");
}

/**
 * A brush stroke is a list of changes to the brush that
 * can occur during a stroke
 *
 * - 3D location of the brush
 * - 2D mouse location
 * - Tablet pressure
 * - Direction flip
 * - Tool switch
 * - Time
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

  prop = RNA_def_property(srna, "mouse_event", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Mouse Event", "");

  prop = RNA_def_property(srna, "pressure", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Pressure", "Tablet pressure");

  prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop, "Brush Size", "Brush size in screen space");

  prop = RNA_def_property(srna, "pen_flip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_ui_text(prop, "Flip", "");

  prop = RNA_def_property(srna, "x_tilt", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Tilt X", "");

  prop = RNA_def_property(srna, "y_tilt", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Tilt Y", "");

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

  prop = RNA_def_property(srna, "mouse_cubic", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_array(prop, 8);
  RNA_def_property_ui_text(prop, "Mouse", "");

  prop = RNA_def_property(srna, "world_cubic", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_array(prop, 12);
  RNA_def_property_ui_text(prop, "Mouse", "");
}

void RNA_def_brush(BlenderRNA *brna)
{
  rna_def_dyntopo_settings(brna);
  rna_def_brush(brna);
  rna_def_brush_capabilities(brna);
  rna_def_sculpt_capabilities(brna);
  rna_def_image_paint_capabilities(brna);
  rna_def_vertex_paint_capabilities(brna);
  rna_def_weight_paint_capabilities(brna);
  rna_def_gpencil_options(brna);
  rna_def_curves_sculpt_options(brna);
  rna_def_brush_texture_slot(brna);
  rna_def_operator_stroke_element(brna);
}

#endif
