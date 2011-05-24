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
#ifndef DNA_DYNAMICPAINT_TYPES_H
#define DNA_DYNAMICPAINT_TYPES_H

struct CurveMapping;

/* flags */
#define MOD_DPAINT_ANTIALIAS (1<<0) /* do antialiasing */
#define MOD_DPAINT_DISSOLVE (1<<1) /* do dissolve */
#define MOD_DPAINT_FLATTEN (1<<2) /* do displace flatten */
#define MOD_DPAINT_MULALPHA (1<<3) /* Multiply color by alpha when saving */

#define MOD_DPAINT_DRY_LOG (1<<4) /* Use 1/x for drying paint */

/* output */
#define MOD_DPAINT_OUT_PAINT (1<<0) /* output paint texture */
#define MOD_DPAINT_OUT_WET (1<<1) /* output wetmap */
#define MOD_DPAINT_OUT_DISP (1<<2) /* output displace texture */

/* disp_type */
#define MOD_DPAINT_DISPFOR_PNG 0 /* displacement output format */
#define MOD_DPAINT_DISPFOR_OPENEXR 1 /* displacement output format */

/* disp_format */
#define MOD_DPAINT_DISP_DISPLACE 0 /* displacement output displace map */
#define MOD_DPAINT_DISP_DEPTH 1 /* displacement output depth data */

/* effect */
#define MOD_DPAINT_EFFECT_DO_SPREAD (1<<0) /* do spread effect */
#define MOD_DPAINT_EFFECT_DO_DRIP (1<<1) /* do spread effect */
#define MOD_DPAINT_EFFECT_DO_SHRINK (1<<2) /* do spread effect */


/* Canvas settings */
typedef struct DynamicPaintCanvasSettings {
	struct DynamicPaintModifierData *pmd; /* for fast RNA access */
	struct DerivedMesh *dm;
	struct PaintSurface *surface;
	
	int flags;
	int output;
	short disp_type, disp_format;
	int effect;
	short effect_ui;	// just ui selection box
	short pad;	// replace if need for another short

	int resolution;
	int start_frame, end_frame;
	int substeps;

	int dry_speed;
	int diss_speed;
	float disp_depth;
	int dflat_speed;	// displace flattening speed

	char paint_output_path[240], wet_output_path[240], displace_output_path[240];

	float spread_speed, drip_speed, shrink_speed;
	char uvlayer_name[32];

	char ui_info[128];	// UI info text
	char error[64];		// Bake error description

} DynamicPaintCanvasSettings;


/* flags */
#define MOD_DPAINT_PART_RAD (1<<0) /* use particle radius */
#define MOD_DPAINT_DO_PAINT (1<<1) /* use particle radius */
#define MOD_DPAINT_DO_WETNESS (1<<2) /* use particle radius */
#define MOD_DPAINT_DO_DISPLACE (1<<3) /* use particle radius */

#define MOD_DPAINT_USE_MATERIAL (1<<4) /* use object material */
#define MOD_DPAINT_ABS_ALPHA (1<<5) /* doesn't increase alpha unless
									paint alpha is higher than existing */
#define MOD_DPAINT_ERASE (1<<6) /* removes paint */

#define MOD_DPAINT_RAMP_ALPHA (1<<7) /* only read falloff ramp alpha */
#define MOD_DPAINT_EDGE_DISP (1<<8) /* add displacement to intersection edges */
#define MOD_DPAINT_PROX_FACEALIGNED (1<<9) /* do proximity check only in normal dir */

/* collision type */
#define MOD_DPAINT_COL_VOLUME 1 /* paint with mesh volume */
#define MOD_DPAINT_COL_DIST 2 /* paint using distance to mesh surface */
#define MOD_DPAINT_COL_VOLDIST 3 /* use both volume and distance */
#define MOD_DPAINT_COL_PSYS 4 /* use particle system */

/* proximity_falloff */
#define MOD_DPAINT_PRFALL_SHARP 0 /* no-falloff */
#define MOD_DPAINT_PRFALL_SMOOTH 1 /* smooth, linear falloff */
#define MOD_DPAINT_PRFALL_RAMP 2 /* use color ramp */


/* Painter settings */
typedef struct DynamicPaintPainterSettings {
	struct DynamicPaintModifierData *pmd; /* for fast RNA access */
	struct DerivedMesh *dm;
	struct ParticleSystem *psys;
	struct Material *mat;

	int flags;
	int collision;

	float r, g, b, alpha;
	float wetness;

	float particle_radius, particle_smooth;
	float paint_distance;
	float displace_distance, prox_displace_strength;

	// Falloff curves
	struct ColorBand *paint_ramp;	/* Proximity paint falloff */

	short proximity_falloff;
	short pad;	// replace if need for new value
	int pad2;	// replace if need for new value

	//int pad;
} DynamicPaintPainterSettings;

#endif
