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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_gpencil_modifier_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_GPENCIL_MODIFIER_TYPES_H__
#define __DNA_GPENCIL_MODIFIER_TYPES_H__

#include "DNA_defs.h"
#include "DNA_listBase.h"

/* WARNING ALERT! TYPEDEF VALUES ARE WRITTEN IN FILES! SO DO NOT CHANGE!
 * (ONLY ADD NEW ITEMS AT THE END)
 */

struct RNG;

typedef enum GpencilModifierType {
	eGpencilModifierType_None      = 0,
	eGpencilModifierType_Noise     = 1,
	eGpencilModifierType_Subdiv    = 2,
	eGpencilModifierType_Thick     = 3,
	eGpencilModifierType_Tint      = 4,
	eGpencilModifierType_Instance  = 5,
	eGpencilModifierType_Build     = 6,
	eGpencilModifierType_Opacity   = 7,
	eGpencilModifierType_Color     = 8,
	eGpencilModifierType_Lattice   = 9,
	eGpencilModifierType_Simplify  = 10,
	eGpencilModifierType_Smooth    = 11,
	eGpencilModifierType_Hook      = 12,
	eGpencilModifierType_Offset    = 13,
	eGpencilModifierType_Mirror    = 14,
	NUM_GREASEPENCIL_MODIFIER_TYPES
} GpencilModifierType;

typedef enum GpencilModifierMode {
	eGpencilModifierMode_Realtime          = (1 << 0),
	eGpencilModifierMode_Render            = (1 << 1),
	eGpencilModifierMode_Editmode          = (1 << 2),
	eGpencilModifierMode_Expanded          = (1 << 3),
} GpencilModifierMode;

typedef enum {
	/* This modifier has been inserted in local override, and hence can be fully edited. */
	eGpencilModifierFlag_StaticOverride_Local = (1 << 0),
} GpencilModifierFlag;

typedef struct GpencilModifierData {
	struct GpencilModifierData *next, *prev;

	int type, mode;
	int stackindex;
	short flag;
	short pad;
	char name[64];  /* MAX_NAME */

	char *error;
} GpencilModifierData;

typedef struct NoiseGpencilModifierData {
	GpencilModifierData modifier;
	char layername[64];          /* layer name */
	char vgname[64];             /* optional vertexgroup name, MAX_VGROUP_NAME */
	int pass_index;               /* custom index for passes */
	int flag;                    /* several flags */
	float factor;                /* factor of noise */
	int step;                    /* how many frames before recalculate randoms */
	int gp_frame;                /* last gp frame used */
	int scene_frame;             /* last scene frame used */
	float vrand1, vrand2;        /* random values */
	struct RNG *rng;
} NoiseGpencilModifierData;

typedef enum eNoiseGpencil_Flag {
	GP_NOISE_USE_RANDOM     = (1 << 0),
	GP_NOISE_MOD_LOCATION   = (1 << 1),
	GP_NOISE_MOD_STRENGTH   = (1 << 2),
	GP_NOISE_MOD_THICKNESS  = (1 << 3),
	GP_NOISE_FULL_STROKE    = (1 << 4),
	GP_NOISE_MOVE_EXTREME   = (1 << 5),
	GP_NOISE_INVERT_LAYER   = (1 << 6),
	GP_NOISE_INVERT_PASS    = (1 << 7),
	GP_NOISE_INVERT_VGROUP  = (1 << 8),
	GP_NOISE_MOD_UV         = (1 << 9),
} eNoiseGpencil_Flag;

typedef struct SubdivGpencilModifierData {
	GpencilModifierData modifier;
	char layername[64];          /* layer name */
	int pass_index;               /* custom index for passes */
	int flag;                    /* flags */
	int level;                   /* factor of subdivision */
	char pad[4];
} SubdivGpencilModifierData;

typedef enum eSubdivGpencil_Flag {
	GP_SUBDIV_SIMPLE        = (1 << 0),
	GP_SUBDIV_INVERT_LAYER  = (1 << 1),
	GP_SUBDIV_INVERT_PASS   = (1 << 2),
} eSubdivGpencil_Flag;

typedef struct ThickGpencilModifierData {
	GpencilModifierData modifier;
	char layername[64];          /* layer name */
	char vgname[64];             /* optional vertexgroup name, MAX_VGROUP_NAME */
	int pass_index;               /* custom index for passes */
	int flag;                    /* flags */
	int thickness;               /* Thickness change */
	char pad[4];
	struct CurveMapping *curve_thickness;
} ThickGpencilModifierData;

typedef enum eThickGpencil_Flag {
	GP_THICK_INVERT_LAYER   = (1 << 0),
	GP_THICK_INVERT_PASS    = (1 << 1),
	GP_THICK_INVERT_VGROUP  = (1 << 2),
	GP_THICK_CUSTOM_CURVE   = (1 << 3),
	GP_THICK_NORMALIZE      = (1 << 4),
} eThickGpencil_Flag;

typedef struct TintGpencilModifierData {
	GpencilModifierData modifier;
	char layername[64];          /* layer name */
	int pass_index;               /* custom index for passes */
	int flag;                    /* flags */
	float rgb[3];                /* Tint color */
	float factor;                /* Mix factor */
} TintGpencilModifierData;

typedef enum eTintGpencil_Flag {
	GP_TINT_CREATE_COLORS  = (1 << 0),
	GP_TINT_INVERT_LAYER   = (1 << 1),
	GP_TINT_INVERT_PASS    = (1 << 2),
} eTintGpencil_Flag;

typedef struct ColorGpencilModifierData {
	GpencilModifierData modifier;
	char layername[64];          /* layer name */
	int pass_index;               /* custom index for passes */
	int flag;                    /* flags */
	float hsv[3];                /* hsv factors */
	char pad[4];
} ColorGpencilModifierData;

typedef enum eColorGpencil_Flag {
	GP_COLOR_CREATE_COLORS  = (1 << 0),
	GP_COLOR_INVERT_LAYER   = (1 << 1),
	GP_COLOR_INVERT_PASS    = (1 << 2),
} eColorGpencil_Flag;

typedef struct OpacityGpencilModifierData {
	GpencilModifierData modifier;
	char layername[64];          /* layer name */
	char vgname[64];             /* optional vertexgroup name, MAX_VGROUP_NAME */
	int pass_index;               /* custom index for passes */
	int flag;                    /* flags */
	float factor;                /* Main Opacity factor */
	char pad[4];
} OpacityGpencilModifierData;

typedef enum eOpacityGpencil_Flag {
	GP_OPACITY_INVERT_LAYER  = (1 << 0),
	GP_OPACITY_INVERT_PASS   = (1 << 1),
	GP_OPACITY_INVERT_VGROUP = (1 << 2),
} eOpacityGpencil_Flag;

typedef struct InstanceGpencilModifierData {
	GpencilModifierData modifier;
	int count[3];                /* number of elements in array */
	int flag;                    /* several flags */
	float offset[3];             /* Location increments */
	float shift[3];              /* shift increment */
	float rnd_size;              /* random size factor */
	float rnd_rot;               /* random size factor */
	float rot[3];                /* Rotation changes */
	float scale[3];              /* Scale changes */
	float rnd[20];               /* (first element is the index) random values */
	int  lock_axis;              /* lock shift to one axis */

	int pass_index;              /* custom index for passes */
	char layername[64];          /* layer name */
} InstanceGpencilModifierData;

typedef enum eInstanceGpencil_Flag {
	GP_INSTANCE_RANDOM_SIZE   = (1 << 0),
	GP_INSTANCE_RANDOM_ROT    = (1 << 1),
	GP_INSTANCE_INVERT_LAYER  = (1 << 2),
	GP_INSTANCE_INVERT_PASS   = (1 << 3),
	GP_INSTANCE_MAKE_OBJECTS  = (1 << 4),
} eInstanceGpencil_Flag;

typedef struct BuildGpencilModifierData {
	GpencilModifierData modifier;

	char layername[64];   /* if set, restrict modifier to operating on this layer */
	int pass_index;

	int pad;

	float start_frame;    /* If GP_BUILD_RESTRICT_TIME is set, the defines the frame range where GP frames are considered */
	float end_frame;

	float start_delay;    /* For each pair of gp keys, number of frames before strokes start appearing */
	float length;         /* For each pair of gp keys, number of frames that build effect must be completed within */

	short flag;           /* (eGpencilBuild_Flag) Options for controlling modifier behaviour */

	short mode;           /* (eGpencilBuild_Mode) How are strokes ordered */
	short transition;     /* (eGpencilBuild_Transition) In what order do stroke points appear/disappear */

	short time_alignment; /* (eGpencilBuild_TimeAlignment) For the "Concurrent" mode, when should "shorter" strips start/end */
} BuildGpencilModifierData;

typedef enum eBuildGpencil_Mode {
	/* Strokes are shown one by one until all have appeared */
	GP_BUILD_MODE_SEQUENTIAL = 0,
	/* All strokes start at the same time */
	GP_BUILD_MODE_CONCURRENT = 1,
} eBuildGpencil_Mode;

typedef enum eBuildGpencil_Transition {
	/* Show in forward order */
	GP_BUILD_TRANSITION_GROW    = 0,
	/* Hide in reverse order */
	GP_BUILD_TRANSITION_SHRINK  = 1,
	/* Hide in forward order */
	GP_BUILD_TRANSITION_FADE    = 2,
} eBuildGpencil_Transition;

typedef enum eBuildGpencil_TimeAlignment {
	/* All strokes start at same time */
	GP_BUILD_TIMEALIGN_START = 0,
	/* All strokes end at same time */
	GP_BUILD_TIMEALIGN_END   = 1,

	/* TODO: Random Offsets, Stretch-to-Fill */
} eBuildGpencil_TimeAlignment;

typedef enum eBuildGpencil_Flag {
	/* Restrict modifier to particular layer/passes? */
	GP_BUILD_INVERT_LAYER  = (1 << 0),
	GP_BUILD_INVERT_PASS   = (1 << 1),

	/* Restrict modifier to only operating between the nominated frames */
	GP_BUILD_RESTRICT_TIME  = (1 << 2),
} eBuildGpencil_Flag;

typedef struct LatticeGpencilModifierData {
	GpencilModifierData modifier;
	struct Object *object;
	char layername[64];          /* layer name */
	char vgname[64];             /* optional vertexgroup name, MAX_VGROUP_NAME */
	int pass_index;               /* custom index for passes */
	int flag;                    /* flags */
	float strength;
	char pad[4];
	void *cache_data; /* runtime only (LatticeDeformData) */
} LatticeGpencilModifierData;

typedef enum eLatticeGpencil_Flag {
	GP_LATTICE_INVERT_LAYER  = (1 << 0),
	GP_LATTICE_INVERT_PASS   = (1 << 1),
	GP_LATTICE_INVERT_VGROUP = (1 << 2),
} eLatticeGpencil_Flag;

typedef struct MirrorGpencilModifierData {
	GpencilModifierData modifier;
	struct Object *object;
	char layername[64];          /* layer name */
	int pass_index;              /* custom index for passes */
	int flag;                    /* flags */
} MirrorGpencilModifierData;

typedef enum eMirrorGpencil_Flag {
	GP_MIRROR_INVERT_LAYER  = (1 << 0),
	GP_MIRROR_INVERT_PASS   = (1 << 1),
	GP_MIRROR_CLIPPING      = (1 << 2),
	GP_MIRROR_AXIS_X        = (1 << 3),
	GP_MIRROR_AXIS_Y        = (1 << 4),
	GP_MIRROR_AXIS_Z        = (1 << 5),
} eMirrorGpencil_Flag;

typedef struct HookGpencilModifierData {
	GpencilModifierData modifier;

	struct Object *object;
	char subtarget[64];     /* optional name of bone target, MAX_ID_NAME-2 */
	char layername[64];     /* layer name */
	char vgname[64];        /* optional vertexgroup name, MAX_VGROUP_NAME */
	int pass_index;         /* custom index for passes */

	int flag;
	char falloff_type;      /* use enums from WarpGpencilModifier (exact same functionality) */
	char pad[3];
	float parentinv[4][4];  /* matrix making current transform unmodified */
	float cent[3];          /* visualization of hook */
	float falloff;          /* if not zero, falloff is distance where influence zero */
	float force;
	struct CurveMapping *curfalloff;
} HookGpencilModifierData;

typedef enum eHookGpencil_Flag {
	GP_HOOK_INVERT_LAYER  = (1 << 0),
	GP_HOOK_INVERT_PASS   = (1 << 1),
	GP_HOOK_INVERT_VGROUP = (1 << 2),
	GP_HOOK_UNIFORM_SPACE = (1 << 3),
} eHookGpencil_Flag;

typedef enum eHookGpencil_Falloff {
	eGPHook_Falloff_None = 0,
	eGPHook_Falloff_Curve = 1,
	eGPHook_Falloff_Sharp = 2,
	eGPHook_Falloff_Smooth = 3,
	eGPHook_Falloff_Root = 4,
	eGPHook_Falloff_Linear = 5,
	eGPHook_Falloff_Const = 6,
	eGPHook_Falloff_Sphere = 7,
	eGPHook_Falloff_InvSquare = 8,
} eHookGpencil_Falloff;

typedef struct SimplifyGpencilModifierData {
	GpencilModifierData modifier;
	char layername[64];          /* layer name */
	int pass_index;               /* custom index for passes */
	int flag;                    /* flags */
	float factor;                /* factor of simplify */
	short mode;                  /* type of simplify */
	short step;                  /* every n vertex to keep */
} SimplifyGpencilModifierData;

typedef enum eSimplifyGpencil_Flag {
	GP_SIMPLIFY_INVERT_LAYER = (1 << 0),
	GP_SIMPLIFY_INVERT_PASS  = (1 << 1),
} eSimplifyGpencil_Flag;

typedef enum eSimplifyGpencil_Mode {
	/* Keep only one vertex every n vertices */
	GP_SIMPLIFY_FIXED = 0,
	/* Use RDP algorithm */
	GP_SIMPLIFY_ADAPTATIVE = 1,
} eSimplifyGpencil_Mode;

typedef struct OffsetGpencilModifierData {
	GpencilModifierData modifier;
	char layername[64];          /* layer name */
	char vgname[64];             /* optional vertexgroup name, MAX_VGROUP_NAME */
	int pass_index;               /* custom index for passes */
	int flag;                    /* flags */
	float loc[3];
	float rot[3];
	float scale[3];
	char pad[4];
} OffsetGpencilModifierData;

typedef enum eOffsetGpencil_Flag {
	GP_OFFSET_INVERT_LAYER  = (1 << 0),
	GP_OFFSET_INVERT_PASS   = (1 << 1),
	GP_OFFSET_INVERT_VGROUP = (1 << 2)
} eOffsetGpencil_Flag;

typedef struct SmoothGpencilModifierData {
	GpencilModifierData modifier;
	char layername[64];          /* layer name */
	char vgname[64];             /* optional vertexgroup name, MAX_VGROUP_NAME */
	int pass_index;              /* custom index for passes */
	int flag;                    /* several flags */
	float factor;                /* factor of noise */
	int step;                    /* how many times apply smooth */
} SmoothGpencilModifierData;

typedef enum eSmoothGpencil_Flag {
	GP_SMOOTH_MOD_LOCATION  = (1 << 0),
	GP_SMOOTH_MOD_STRENGTH  = (1 << 1),
	GP_SMOOTH_MOD_THICKNESS = (1 << 2),
	GP_SMOOTH_INVERT_LAYER  = (1 << 3),
	GP_SMOOTH_INVERT_PASS   = (1 << 4),
	GP_SMOOTH_INVERT_VGROUP = (1 << 5),
	GP_SMOOTH_MOD_UV         = (1 << 6),
} eSmoothGpencil_Flag;

#define MOD_MESHSEQ_READ_ALL \
	(MOD_MESHSEQ_READ_VERT | MOD_MESHSEQ_READ_POLY | MOD_MESHSEQ_READ_UV | MOD_MESHSEQ_READ_COLOR)

#endif  /* __DNA_GPENCIL_MODIFIER_TYPES_H__ */
