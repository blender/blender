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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Miika Hämäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file DNA_dynamicpaint_types.h
 *  \ingroup DNA
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

/* preview_id */
#define MOD_DPAINT_SURFACE_PREV_PAINT 0
#define MOD_DPAINT_SURFACE_PREV_WETMAP 1

/* init_color_type */
#define MOD_DPAINT_INITIAL_NONE 0
#define MOD_DPAINT_INITIAL_COLOR 1
#define MOD_DPAINT_INITIAL_TEXTURE 2
#define MOD_DPAINT_INITIAL_VERTEXCOLOR 3

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
	short effect_ui;	/* ui selection box */
	short preview_id;	/* surface output id to preview */
	short init_color_type, pad_s;
	int flags, effect;

	int image_resolution, substeps;
	int start_frame, end_frame, pad;

	/* initial color */
	float init_color[4];
	struct Tex *init_texture;
	char init_layername[64];  /* MAX_CUSTOMDATA_LAYER_NAME */

	int dry_speed, diss_speed;
	float depth_clamp, disp_factor;

	float spread_speed, color_spread_speed, shrink_speed;
	float drip_vel, drip_acc;

	/* wave settings */
	float wave_damping, wave_speed, wave_timescale, wave_spring;

	int pad_;

	char uvlayer_name[64];	/* MAX_CUSTOMDATA_LAYER_NAME */
	char image_output_path[240];
	char output_name[64];  /* MAX_CUSTOMDATA_LAYER_NAME */
	char output_name2[64]; /* MAX_CUSTOMDATA_LAYER_NAME */ /* some surfaces have 2 outputs */

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

	char error[64];		/* Bake error description */

} DynamicPaintCanvasSettings;


/* flags */
#define MOD_DPAINT_PART_RAD (1<<0) /* use particle radius */
#define MOD_DPAINT_USE_MATERIAL (1<<1) /* use object material */
#define MOD_DPAINT_ABS_ALPHA (1<<2) /* don't increase alpha unless
									paint alpha is higher than existing */
#define MOD_DPAINT_ERASE (1<<3) /* removes paint */

#define MOD_DPAINT_RAMP_ALPHA (1<<4) /* only read falloff ramp alpha */
#define MOD_DPAINT_PROX_PROJECT (1<<5) /* do proximity check only in defined dir */
#define MOD_DPAINT_INVERSE_PROX (1<<6) /* inverse proximity painting */
#define MOD_DPAINT_NEGATE_VOLUME (1<<7) /* negates volume influence on "volume + prox" mode */

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
#define MOD_DPAINT_PRFALL_CONSTANT 0 /* no-falloff */
#define MOD_DPAINT_PRFALL_SMOOTH 1 /* smooth, linear falloff */
#define MOD_DPAINT_PRFALL_RAMP 2 /* use color ramp */

/* wave_brush_type */
#define MOD_DPAINT_WAVEB_DEPTH 0 /* use intersection depth */
#define MOD_DPAINT_WAVEB_FORCE 1 /* act as a force on intersection area */
#define MOD_DPAINT_WAVEB_REFLECT 2 /* obstacle that reflects waves */
#define MOD_DPAINT_WAVEB_CHANGE 3  /* use change of intersection depth from previous frame */

/* brush ray_dir */
#define MOD_DPAINT_RAY_CANVAS 0
#define MOD_DPAINT_RAY_BRUSH_AVG 1
#define MOD_DPAINT_RAY_ZPLUS 2


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
	short wave_type;
	short ray_dir;
	short pad;

	float wave_factor, wave_clamp;
	float max_velocity, smudge_strength;
} DynamicPaintBrushSettings;

#endif
