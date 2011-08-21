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

#include "DNA_listBase.h"
struct CurveMapping;
struct PaintSurfaceData;

/* surface format */
#define MOD_DPAINT_SURFACE_F_PTEX 0
#define MOD_DPAINT_SURFACE_F_VERTEX 1
#define MOD_DPAINT_SURFACE_F_IMAGESEQ 2

/* surface type */
#define MOD_DPAINT_SURFACE_T_PAINT 0
#define MOD_DPAINT_SURFACE_T_DISPLACE 1
#define MOD_DPAINT_SURFACE_T_WEIGHT 2
#define MOD_DPAINT_SURFACE_T_WAVE 3

/* surface flags */
#define MOD_DPAINT_ACTIVE (1<<0) /* Is surface enabled */

#define MOD_DPAINT_ANTIALIAS (1<<1) /* do antialiasing */
#define MOD_DPAINT_DISSOLVE (1<<2) /* do dissolve */
#define MOD_DPAINT_MULALPHA (1<<3) /* Multiply color by alpha when saving image */
#define MOD_DPAINT_DISSOLVE_LOG (1<<4) /* Use 1/x for surface dissolve */
#define MOD_DPAINT_DRY_LOG (1<<5) /* Use 1/x for drying paint */
#define MOD_DPAINT_PREVIEW (1<<6) /* preview this surface on viewport*/

#define MOD_DPAINT_WAVE_OPEN_BORDERS (1<<7) /* passes waves through mesh edges */
#define MOD_DPAINT_DISP_INCREMENTAL (1<<8) /* builds displace on top of earlier values */

#define MOD_DPAINT_OUT1 (1<<10) /* output primary surface */
#define MOD_DPAINT_OUT2 (1<<11) /* output secondary surface */

/* image_fileformat */
#define MOD_DPAINT_IMGFORMAT_PNG 0
#define MOD_DPAINT_IMGFORMAT_OPENEXR 1

/* disp_format */
#define MOD_DPAINT_DISP_DISPLACE 0 /* displacement output displace map */
#define MOD_DPAINT_DISP_DEPTH 1 /* displacement output depth data */

/* effect */
#define MOD_DPAINT_EFFECT_DO_SPREAD (1<<0) /* do spread effect */
#define MOD_DPAINT_EFFECT_DO_DRIP (1<<1) /* do spread effect */
#define MOD_DPAINT_EFFECT_DO_SHRINK (1<<2) /* do spread effect */

typedef struct DynamicPaintSurface {
	
	struct DynamicPaintSurface *next, *prev;
	struct DynamicPaintCanvasSettings *canvas; /* for fast RNA access */
	struct PaintSurfaceData *data;

	struct Group *brush_group;
	struct EffectorWeights *effector_weights;

	/* cache */
	struct PointCache *pointcache;
	struct ListBase ptcaches;
	int current_frame;

	/* surface */
	char name[64];
	short format, type;
	short disp_type, image_fileformat;
	short effect_ui;	/* just ui selection box */
	short pad;
	int flags, effect;

	int image_resolution, substeps;
	int start_frame, end_frame;

	int dry_speed, diss_speed;
	float disp_clamp;

	float spread_speed, color_spread_speed, shrink_speed;
	float drip_vel, drip_acc;

	/* wave settings */
	float wave_damping, wave_speed, wave_timescale, wave_spring;
	char uvlayer_name[32];

	char image_output_path[240];
	char output_name[40];
	char output_name2[40]; /* some surfaces have 2 outputs */

} DynamicPaintSurface;

/* canvas flags */
#define MOD_DPAINT_PREVIEW_READY (1<<0) /* if viewport preview is ready */
#define MOD_DPAINT_BAKING (1<<1) /* surface is already baking, so it wont get updated (loop) */

/* Canvas settings */
typedef struct DynamicPaintCanvasSettings {
	struct DynamicPaintModifierData *pmd; /* for fast RNA access */
	struct DerivedMesh *dm;

	struct ListBase surfaces;
	short active_sur, flags;
	int pad;

	
	char ui_info[128];	// UI info text
	char error[64];		// Bake error description

} DynamicPaintCanvasSettings;


/* flags */
#define MOD_DPAINT_PART_RAD (1<<0) /* use particle radius */
#define MOD_DPAINT_USE_MATERIAL (1<<1) /* use object material */
#define MOD_DPAINT_ABS_ALPHA (1<<2) /* don't increase alpha unless
									paint alpha is higher than existing */
#define MOD_DPAINT_ERASE (1<<3) /* removes paint */

#define MOD_DPAINT_RAMP_ALPHA (1<<4) /* only read falloff ramp alpha */
#define MOD_DPAINT_PROX_FACEALIGNED (1<<5) /* do proximity check only in normal dir */
#define MOD_DPAINT_INVERSE_PROX (1<<6) /* inverse proximity painting */
#define MOD_DPAINT_ACCEPT_NONCLOSED (1<<7) /* allows volume brushes to work with non-closed volumes */

#define MOD_DPAINT_DO_SMUDGE (1<<8) /* brush smudges existing paint */
#define MOD_DPAINT_VELOCITY_ALPHA (1<<9) /* multiply brush influence by velocity */
#define MOD_DPAINT_VELOCITY_COLOR (1<<10) /* replace brush color by velocity color ramp */
#define MOD_DPAINT_VELOCITY_DEPTH (1<<11) /* multiply brush intersection depth by velocity */

#define MOD_DPAINT_USES_VELOCITY ((1<<8)|(1<<9)|(1<<10)|(1<<11))

/* collision type */
#define MOD_DPAINT_COL_VOLUME 0 /* paint with mesh volume */
#define MOD_DPAINT_COL_DIST 1 /* paint using distance to mesh surface */
#define MOD_DPAINT_COL_VOLDIST 2 /* use both volume and distance */
#define MOD_DPAINT_COL_PSYS 3 /* use particle system */
#define MOD_DPAINT_COL_POINT 4 /* use distance to object center point */

/* proximity_falloff */
#define MOD_DPAINT_PRFALL_SHARP 0 /* no-falloff */
#define MOD_DPAINT_PRFALL_SMOOTH 1 /* smooth, linear falloff */
#define MOD_DPAINT_PRFALL_RAMP 2 /* use color ramp */

/* wave_brush_type */
#define MOD_DPAINT_WAVEB_DEPTH 0 /* use intersection depth */
#define MOD_DPAINT_WAVEB_FORCE 1 /* act as a force on intersection area */
#define MOD_DPAINT_WAVEB_REFLECT 2 /* obstacle that reflects waves */

/* brush ray_dir */
#define MOD_DPAINT_RAY_CANVAS 0
#define MOD_DPAINT_RAY_ZPLUS 1


/* Brush settings */
typedef struct DynamicPaintBrushSettings {
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

	/* color ramps */
	struct ColorBand *paint_ramp;	/* Proximity paint falloff */
	struct ColorBand *vel_ramp;		/* Velocity paint ramp */

	short proximity_falloff;
	short brush_settings_context;	/* ui settings display */
	short wave_type;
	short ray_dir;

	float wave_factor;
	float max_velocity, smudge_strength;
	float pad;
} DynamicPaintBrushSettings;

#endif
