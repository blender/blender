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
 * The Original Code is Copyright (C) 2006 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel Genrich (Genscher)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_smoke_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_SMOKE_TYPES_H__
#define __DNA_SMOKE_TYPES_H__

/* flags */
enum {
	MOD_SMOKE_HIGHRES = (1 << 1),  /* enable high resolution */
	MOD_SMOKE_DISSOLVE = (1 << 2),  /* let smoke dissolve */
	MOD_SMOKE_DISSOLVE_LOG = (1 << 3),  /* using 1/x for dissolve */

#ifdef DNA_DEPRECATED
	MOD_SMOKE_HIGH_SMOOTH = (1 << 5),  /* -- Deprecated -- */
#endif
	MOD_SMOKE_FILE_LOAD = (1 << 6),  /* flag for file load */
	MOD_SMOKE_ADAPTIVE_DOMAIN = (1 << 7),
};

/* noise */
#define MOD_SMOKE_NOISEWAVE (1<<0)
#define MOD_SMOKE_NOISEFFT (1<<1)
#define MOD_SMOKE_NOISECURL (1<<2)
/* viewsettings */
#define MOD_SMOKE_VIEW_SHOWBIG (1<<0)

/* slice method */
enum {
	MOD_SMOKE_SLICE_VIEW_ALIGNED = 0,
	MOD_SMOKE_SLICE_AXIS_ALIGNED = 1,
};

/* axis aligned method */
enum {
	AXIS_SLICE_FULL   = 0,
	AXIS_SLICE_SINGLE = 1,
};

/* single slice direction */
enum {
	SLICE_AXIS_AUTO = 0,
	SLICE_AXIS_X    = 1,
	SLICE_AXIS_Y    = 2,
	SLICE_AXIS_Z    = 3,
};

enum {
	VECTOR_DRAW_NEEDLE     = 0,
	VECTOR_DRAW_STREAMLINE = 1,
};

enum {
	FLUID_FIELD_DENSITY    = 0,
	FLUID_FIELD_HEAT       = 1,
	FLUID_FIELD_FUEL       = 2,
	FLUID_FIELD_REACT      = 3,
	FLUID_FIELD_FLAME      = 4,
	FLUID_FIELD_VELOCITY_X = 5,
	FLUID_FIELD_VELOCITY_Y = 6,
	FLUID_FIELD_VELOCITY_Z = 7,
	FLUID_FIELD_COLOR_R    = 8,
	FLUID_FIELD_COLOR_G    = 9,
	FLUID_FIELD_COLOR_B    = 10,
	FLUID_FIELD_FORCE_X    = 11,
	FLUID_FIELD_FORCE_Y    = 12,
	FLUID_FIELD_FORCE_Z    = 13,
};

/* cache compression */
#define SM_CACHE_LIGHT		0
#define SM_CACHE_HEAVY		1

/* domain border collision */
#define SM_BORDER_OPEN		0
#define SM_BORDER_VERTICAL	1
#define SM_BORDER_CLOSED	2

/* collision types */
#define SM_COLL_STATIC		0
#define SM_COLL_RIGID		1
#define SM_COLL_ANIMATED	2

/* high resolution sampling types */
#define SM_HRES_NEAREST		0
#define SM_HRES_LINEAR		1
#define SM_HRES_FULLSAMPLE	2

/* smoke data fields (active_fields) */
#define SM_ACTIVE_HEAT		(1<<0)
#define SM_ACTIVE_FIRE		(1<<1)
#define SM_ACTIVE_COLORS	(1<<2)
#define SM_ACTIVE_COLOR_SET	(1<<3)

enum {
	VDB_COMPRESSION_BLOSC = 0,
	VDB_COMPRESSION_ZIP   = 1,
	VDB_COMPRESSION_NONE  = 2,
};

typedef struct SmokeDomainSettings {
	struct SmokeModifierData *smd; /* for fast RNA access */
	struct FLUID_3D *fluid;
	void *fluid_mutex;
	struct Group *fluid_group;
	struct Group *eff_group; // UNUSED
	struct Group *coll_group; // collision objects group
	struct WTURBULENCE *wt; // WTURBULENCE object, if active
	struct GPUTexture *tex;
	struct GPUTexture *tex_wt;
	struct GPUTexture *tex_shadow;
	struct GPUTexture *tex_flame;
	float *shadow;

	/* simulation data */
	float p0[3]; /* start point of BB in local space (includes sub-cell shift for adaptive domain)*/
	float p1[3]; /* end point of BB in local space */
	float dp0[3]; /* difference from object center to grid start point */
	float cell_size[3]; /* size of simulation cell in local space */
	float global_size[3]; /* global size of domain axises */
	float prev_loc[3];
	int shift[3]; /* current domain shift in simulation cells */
	float shift_f[3]; /* exact domain shift */
	float obj_shift_f[3]; /* how much object has shifted since previous smoke frame (used to "lock" domain while drawing) */
	float imat[4][4]; /* domain object imat */
	float obmat[4][4]; /* domain obmat */
	float fluidmat[4][4]; /* low res fluid matrix */
	float fluidmat_wt[4][4]; /* high res fluid matrix */

	int base_res[3]; /* initial "non-adapted" resolution */
	int res_min[3]; /* cell min */
	int res_max[3]; /* cell max */
	int res[3]; /* data resolution (res_max-res_min) */
	int total_cells;
	float dx; /* 1.0f / res */
	float scale; /* largest domain size */

	/* user settings */
	int adapt_margin;
	int adapt_res;
	float adapt_threshold;

	float alpha;
	float beta;
	int amplify; /* wavelet amplification */
	int maxres; /* longest axis on the BB gets this resolution assigned */
	int flags; /* show up-res or low res, etc */
	int viewsettings;
	short noise; /* noise type: wave, curl, anisotropic */
	short diss_percent;
	int diss_speed;/* in frames */
	float strength;
	int res_wt[3];
	float dx_wt;
	/* point cache options */
	int cache_comp;
	int cache_high_comp;
	/* OpenVDB cache options */
	int openvdb_comp;
	char cache_file_format;
	char data_depth;
	char pad[2];

	/* Smoke uses only one cache from now on (index [0]), but keeping the array for now for reading old files. */
	struct PointCache *point_cache[2];	/* definition is in DNA_object_force_types.h */
	struct ListBase ptcaches[2];
	struct EffectorWeights *effector_weights;
	int border_collisions;	/* How domain border collisions are handled */
	float time_scale;
	float vorticity;
	int active_fields;
	float active_color[3]; /* monitor color situation of simulation */
	int highres_sampling;

	/* flame parameters */
	float burning_rate, flame_smoke, flame_vorticity;
	float flame_ignition, flame_max_temp;
	float flame_smoke_color[3];

	/* Display settings */
	char slice_method, axis_slice_method;
	char slice_axis, draw_velocity;
	float slice_per_voxel;
	float slice_depth;
	float display_thickness;

	struct ColorBand *coba;
	float vector_scale;
	char vector_draw_type;
	char use_coba;
	char coba_field;  /* simulation field used for the color mapping */
	char pad2;

	float clipping;
	float pad3;
} SmokeDomainSettings;


/* inflow / outflow */

/* type */
#define MOD_SMOKE_FLOW_TYPE_SMOKE 0
#define MOD_SMOKE_FLOW_TYPE_FIRE 1
#define MOD_SMOKE_FLOW_TYPE_OUTFLOW 2
#define MOD_SMOKE_FLOW_TYPE_SMOKEFIRE 3

/* flow source */
#define MOD_SMOKE_FLOW_SOURCE_PARTICLES 0
#define MOD_SMOKE_FLOW_SOURCE_MESH 1

/* flow texture type */
#define MOD_SMOKE_FLOW_TEXTURE_MAP_AUTO 0
#define MOD_SMOKE_FLOW_TEXTURE_MAP_UV 1

/* flags */
#define MOD_SMOKE_FLOW_ABSOLUTE (1<<1) /*old style emission*/
#define MOD_SMOKE_FLOW_INITVELOCITY (1<<2) /* passes particles speed to the smoke */
#define MOD_SMOKE_FLOW_TEXTUREEMIT (1<<3) /* use texture to control emission speed */
#define MOD_SMOKE_FLOW_USE_PART_SIZE (1<<4) /* use specific size for particles instead of closest cell */

typedef struct SmokeFlowSettings {
	struct SmokeModifierData *smd; /* for fast RNA access */
	struct DerivedMesh *dm;
	struct ParticleSystem *psys;
	struct Tex *noise_texture;

	/* initial velocity */
	float *verts_old; /* previous vertex positions in domain space */
	int numverts;
	float vel_multi; // Multiplier for inherited velocity
	float vel_normal;
	float vel_random;
	/* emission */
	float density;
	float color[3];
	float fuel_amount;
	float temp; /* delta temperature (temp - ambient temp) */
	float volume_density; /* density emitted within mesh volume */
	float surface_distance; /* maximum emission distance from mesh surface */
	float particle_size;
	int subframes;
	/* texture control */
	float texture_size;
	float texture_offset;
	int pad;
	char uvlayer_name[64];	/* MAX_CUSTOMDATA_LAYER_NAME */
	short vgroup_density;

	short type; /* smoke, flames, both, outflow */
	short source;
	short texture_type;
	int flags; /* absolute emission etc*/
} SmokeFlowSettings;


// struct BVHTreeFromMesh *bvh;
// float mat[4][4];
// float mat_old[4][4];

/* collision objects (filled with smoke) */
typedef struct SmokeCollSettings {
	struct SmokeModifierData *smd; /* for fast RNA access */
	struct DerivedMesh *dm;
	float *verts_old;
	int numverts;
	short type; // static = 0, rigid = 1, dynamic = 2
	short pad;
} SmokeCollSettings;

#endif
