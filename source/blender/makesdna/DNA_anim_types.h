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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * Contributor(s): Joshua Leung (full recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_anim_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_ANIM_TYPES_H__
#define __DNA_ANIM_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_ID.h"
#include "DNA_listBase.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"

/* ************************************************ */
/* F-Curve DataTypes */

/* Modifiers -------------------------------------- */

/* F-Curve Modifiers (fcm) 
 *
 * These alter the way F-Curves behave, by altering the value that is returned
 * when evaluating the curve's data at some time (t). 
 */
typedef struct FModifier {
	struct FModifier *next, *prev;
	
	void *data;			/* pointer to modifier data */
	
	char name[64];		/* user-defined description for the modifier */
	short type;			/* type of f-curve modifier */
	short flag;			/* settings for the modifier */
	
	float influence;	/* the amount that the modifier should influence the value */
	
	float sfra;			/* start frame of restricted frame-range */
	float efra;			/* end frame of restricted frame-range */
	float blendin;		/* number of frames from sfra before modifier takes full influence */
	float blendout;		/* number of frames from efra before modifier fades out */
} FModifier;

/* Types of F-Curve modifier 
 * WARNING: order here is important!
 */
typedef enum eFModifier_Types {
	FMODIFIER_TYPE_NULL = 0,
	FMODIFIER_TYPE_GENERATOR = 1,
	FMODIFIER_TYPE_FN_GENERATOR = 2,
	FMODIFIER_TYPE_ENVELOPE = 3,
	FMODIFIER_TYPE_CYCLES = 4,
	FMODIFIER_TYPE_NOISE = 5,
	FMODIFIER_TYPE_FILTER = 6,		/* unimplemented - for applying: fft, high/low pass filters, etc. */
	FMODIFIER_TYPE_PYTHON = 7,
	FMODIFIER_TYPE_LIMITS = 8,
	FMODIFIER_TYPE_STEPPED = 9,
	
	/* NOTE: all new modifiers must be added above this line */
	FMODIFIER_NUM_TYPES
} eFModifier_Types;

/* F-Curve Modifier Settings */
typedef enum eFModifier_Flags {
		/* modifier is not able to be evaluated for some reason, and should be skipped (internal) */
	FMODIFIER_FLAG_DISABLED		 = (1<<0),
		/* modifier's data is expanded (in UI) */
	FMODIFIER_FLAG_EXPANDED		 = (1<<1),
		/* modifier is active one (in UI) for editing purposes */
	FMODIFIER_FLAG_ACTIVE		 = (1<<2),
		/* user wants modifier to be skipped */
	FMODIFIER_FLAG_MUTED		 = (1<<3),
		/* restrict range that F-Modifier can be considered over */
	FMODIFIER_FLAG_RANGERESTRICT = (1<<4),
		/* use influence control */
	FMODIFIER_FLAG_USEINFLUENCE  = (1<<5)
} eFModifier_Flags; 

/* --- */

/* Generator modifier data */
typedef struct FMod_Generator {
		/* general generator information */
	float *coefficients;		/* coefficients array */
	unsigned int arraysize;		/* size of the coefficients array */
	
	int poly_order;				/* order of polynomial generated (i.e. 1 for linear, 2 for quadratic) */
	int mode;					/* which 'generator' to use eFMod_Generator_Modes */
	
		/* settings */
	int flag;					/* settings */
} FMod_Generator;

/* generator modes */
typedef enum eFMod_Generator_Modes {
	FCM_GENERATOR_POLYNOMIAL	= 0,
	FCM_GENERATOR_POLYNOMIAL_FACTORISED = 1
} eFMod_Generator_Modes;


/* generator flags 
 *	- shared by Generator and Function Generator
 */
typedef enum eFMod_Generator_Flags {
		/* generator works in conjunction with other modifiers (i.e. doesn't replace those before it) */
	FCM_GENERATOR_ADDITIVE	= (1<<0)
} eFMod_Generator_Flags;


/* 'Built-In Function' Generator modifier data
 * 
 * This uses the general equation for equations:
 * 		y = amplitude * fn(phase_multiplier*x + phase_offset) + y_offset
 *
 * where amplitude, phase_multiplier/offset, y_offset are user-defined coefficients,
 * x is the evaluation 'time', and 'y' is the resultant value
 */
typedef struct FMod_FunctionGenerator {
		/* coefficients for general equation (as above) */
	float amplitude;
	float phase_multiplier;
	float phase_offset;
	float value_offset;
	
		/* flags */
	int type;				/* eFMod_Generator_Functions */
	int flag;				/* eFMod_Generator_flags */
} FMod_FunctionGenerator;

/* 'function' generator types */
typedef enum eFMod_Generator_Functions {
	FCM_GENERATOR_FN_SIN	= 0,
	FCM_GENERATOR_FN_COS	= 1,
	FCM_GENERATOR_FN_TAN	= 2,
	FCM_GENERATOR_FN_SQRT	= 3,
	FCM_GENERATOR_FN_LN	= 4,
	FCM_GENERATOR_FN_SINC	= 5
} eFMod_Generator_Functions;


/* envelope modifier - envelope data */
typedef struct FCM_EnvelopeData {
	float min, max;				/* min/max values for envelope at this point (absolute values)  */
	float time;					/* time for that this sample-point occurs */
	
	short f1;					/* settings for 'min' control point */
	short f2;					/* settings for 'max' control point */
} FCM_EnvelopeData;

/* envelope-like adjustment to values (for fade in/out) */
typedef struct FMod_Envelope {
	FCM_EnvelopeData *data;		/* data-points defining envelope to apply (array)  */
	int totvert;				/* number of envelope points */
	
	float midval;				/* value that envelope's influence is centered around / based on */
	float min, max;				/* distances from 'middle-value' for 1:1 envelope influence */
} FMod_Envelope;


/* cycling/repetition modifier data */
// TODO: we can only do complete cycles...
typedef struct FMod_Cycles {
	short 	before_mode;		/* extrapolation mode to use before first keyframe */
	short 	after_mode;			/* extrapolation mode to use after last keyframe */
	short 	before_cycles;		/* number of 'cycles' before first keyframe to do */
	short 	after_cycles;		/* number of 'cycles' after last keyframe to do */
} FMod_Cycles;

/* cycling modes */
typedef enum eFMod_Cycling_Modes {
	FCM_EXTRAPOLATE_NONE = 0,			/* don't do anything */
	FCM_EXTRAPOLATE_CYCLIC,				/* repeat keyframe range as-is */
	FCM_EXTRAPOLATE_CYCLIC_OFFSET,		/* repeat keyframe range, but with offset based on gradient between values */
	FCM_EXTRAPOLATE_MIRROR				/* alternate between forward and reverse playback of keyframe range */
} eFMod_Cycling_Modes;


/* Python-script modifier data */
typedef struct FMod_Python {
	struct Text *script;		/* text buffer containing script to execute */
	IDProperty *prop;			/* ID-properties to provide 'custom' settings */
} FMod_Python;


/* limits modifier data */
typedef struct FMod_Limits {
	rctf rect;					/* rect defining the min/max values */
	int flag;					/* settings for limiting */
	int pad;
} FMod_Limits;

/* limiting flags */
typedef enum eFMod_Limit_Flags {
	FCM_LIMIT_XMIN		= (1<<0),
	FCM_LIMIT_XMAX		= (1<<1),
	FCM_LIMIT_YMIN		= (1<<2),
	FCM_LIMIT_YMAX		= (1<<3)
} eFMod_Limit_Flags;


/* noise modifier data */
typedef struct FMod_Noise {
	float size;
	float strength;
	float phase;
	float offset;
	
	short depth;
	short modification;
} FMod_Noise;
	
/* modification modes */
typedef enum eFMod_Noise_Modifications {
	FCM_NOISE_MODIF_REPLACE = 0,	/* Modify existing curve, matching it's shape */
	FCM_NOISE_MODIF_ADD,			/* Add noise to the curve */
	FCM_NOISE_MODIF_SUBTRACT,		/* Subtract noise from the curve */
	FCM_NOISE_MODIF_MULTIPLY		/* Multiply the curve by noise */
} eFMod_Noise_Modifications;


/* stepped modifier data */
typedef struct FMod_Stepped {
	float step_size;                /* Number of frames each interpolated value should be held */
	float offset;                   /* Reference frame number that stepping starts from */

	float start_frame;              /* start frame of the frame range that modifier works in */
	float end_frame;                /* end frame of the frame range that modifier works in */

	int flag;                       /* various settings */
} FMod_Stepped;

/* stepped modifier range flags */
typedef enum eFMod_Stepped_Flags {
	FCM_STEPPED_NO_BEFORE 	= (1<<0),	/* don't affect frames before the start frame */
	FCM_STEPPED_NO_AFTER 	= (1<<1),	/* don't affect frames after the end frame */
} eFMod_Stepped_Flags;

/* Drivers -------------------------------------- */

/* Driver Target (dtar)
 *
 * Defines how to access a dependency needed for a driver variable.
 */
typedef struct DriverTarget {
	ID 	*id;				/* ID-block which owns the target, no user count */
	
	char *rna_path;			/* RNA path defining the setting to use (for DVAR_TYPE_SINGLE_PROP) */
	
	char pchan_name[32];	/* name of the posebone to use (for vars where DTAR_FLAG_STRUCT_REF is used) */
	short transChan;		/* transform channel index (for DVAR_TYPE_TRANSFORM_CHAN)*/
	
	short flag;				/* flags for the validity of the target (NOTE: these get reset every time the types change) */
	int idtype;				/* type of ID-block that this target can use */
} DriverTarget;

/* Driver Target flags */
typedef enum eDriverTarget_Flag {
		/* used for targets that use the pchan_name instead of RNA path 
		 * (i.e. rotation difference) 
		 */
	DTAR_FLAG_STRUCT_REF	= (1 << 0),
		/* idtype can only be 'Object' */
	DTAR_FLAG_ID_OB_ONLY	= (1 << 1),
	
	/* "localspace" flags */
		/* base flag - basically "pre parent+constraints" */
	DTAR_FLAG_LOCALSPACE	= (1 << 2),
		/* include constraints transformed to space including parents */
	DTAR_FLAG_LOCAL_CONSTS	= (1 << 3),
	
	/* error flags */
	DTAR_FLAG_INVALID		= (1 << 4),
} eDriverTarget_Flag;

/* Transform Channels for Driver Targets */
typedef enum eDriverTarget_TransformChannels {
	DTAR_TRANSCHAN_LOCX = 0,
	DTAR_TRANSCHAN_LOCY,
	DTAR_TRANSCHAN_LOCZ,
	DTAR_TRANSCHAN_ROTX,
	DTAR_TRANSCHAN_ROTY,
	DTAR_TRANSCHAN_ROTZ,
	DTAR_TRANSCHAN_SCALEX,
	DTAR_TRANSCHAN_SCALEY,
	DTAR_TRANSCHAN_SCALEZ,
	
	MAX_DTAR_TRANSCHAN_TYPES
} eDriverTarget_TransformChannels;

/* --- */

/* maximum number of driver targets per variable */
#define MAX_DRIVER_TARGETS 	8


/* Driver Variable (dvar)
 *
 * A 'variable' for use as an input for the driver evaluation.
 * Defines a way of accessing some channel to use, that can be
 * referred to in the expression as a variable, thus simplifying
 * expressions and also Depsgraph building.
 */
typedef struct DriverVar {
	struct DriverVar *next, *prev;

	char name[64];              /* name of the variable to use in py-expression (must be valid python identifier) */

	DriverTarget targets[8];    /* MAX_DRIVER_TARGETS, target slots */
	short num_targets;          /* number of targets actually used by this variable */

	short type;                 /* type of driver target (eDriverTarget_Types) */
	float curval;               /* result of previous evaluation */
} DriverVar;

/* Driver Variable Types */
typedef enum eDriverVar_Types {
		/* single RNA property */
	DVAR_TYPE_SINGLE_PROP	= 0,
		/* rotation difference (between 2 bones) */
	DVAR_TYPE_ROT_DIFF,
		/* distance between objects/bones */
	DVAR_TYPE_LOC_DIFF,
		/* 'final' transform for object/bones */
	DVAR_TYPE_TRANSFORM_CHAN,
	
	/* maximum number of variable types 
	 * NOTE: this must always be th last item in this list,
	 * 		so add new types above this line
	 */
	MAX_DVAR_TYPES
} eDriverVar_Types;

/* --- */

/* Channel Driver (i.e. Drivers / Expressions) (driver)
 *
 * Channel Drivers are part of the dependency system, and are executed in addition to 
 * normal user-defined animation. They take the animation result of some channel(s), and
 * use that (optionally combined with its own F-Curve for modification of results) to define
 * the value of some setting semi-procedurally.
 *
 * Drivers are stored as part of F-Curve data, so that the F-Curve's RNA-path settings (for storing
 * what setting the driver will affect). The order in which they are stored defines the order that they're
 * evaluated in. This order is set by the Depsgraph's sorting stuff. 
 */
typedef struct ChannelDriver {
	ListBase variables;	/* targets for this driver (i.e. list of DriverVar) */
	
	/* python expression to execute (may call functions defined in an accessory file) 
	 * which relates the target 'variables' in some way to yield a single usable value
	 */
	char expression[256];	/* expression to compile for evaluation */
	void *expr_comp; 		/* PyObject - compiled expression, don't save this */
	
	float curval;		/* result of previous evaluation */
	float influence;	/* influence of driver on result */ // XXX to be implemented... this is like the constraint influence setting
	
		/* general settings */
	int type;			/* type of driver */
	int flag;			/* settings of driver */
} ChannelDriver;

/* driver type */
typedef enum eDriver_Types {
		/* target values are averaged together */
	DRIVER_TYPE_AVERAGE	= 0,
		/* python expression/function relates targets */
	DRIVER_TYPE_PYTHON,
		/* sum of all values */
	DRIVER_TYPE_SUM,
		/* smallest value */
	DRIVER_TYPE_MIN,
		/* largest value */
	DRIVER_TYPE_MAX
} eDriver_Types;

/* driver flags */
typedef enum eDriver_Flags {
		/* driver has invalid settings (internal flag)  */
	DRIVER_FLAG_INVALID		= (1<<0),
		/* driver needs recalculation (set by depsgraph) */
	DRIVER_FLAG_RECALC		= (1<<1),
		/* driver does replace value, but overrides (for layering of animation over driver) */
		// TODO: this needs to be implemented at some stage or left out...
	//DRIVER_FLAG_LAYERING	= (1<<2),
		/* use when the expression needs to be recompiled */
	DRIVER_FLAG_RECOMPILE	= (1<<3),
		/* the names are cached so they don't need have python unicode versions created each time */
	DRIVER_FLAG_RENAMEVAR	= (1<<4),
		/* intermediate values of driver should be shown in the UI for debugging purposes */
	DRIVER_FLAG_SHOWDEBUG	= (1<<5)
} eDriver_Flags;

/* F-Curves -------------------------------------- */

/* FPoint (fpt)
 *
 * This is the bare-minimum data required storing motion samples. Should be more efficient
 * than using BPoints, which contain a lot of other unnecessary data...
 */
typedef struct FPoint {
	float vec[2];		/* time + value */
	int flag;			/* selection info */
	int pad;
} FPoint;

/* 'Function-Curve' - defines values over time for a given setting (fcu) */
typedef struct FCurve {
	struct FCurve *next, *prev;
	
		/* group */
	bActionGroup *grp;		/* group that F-Curve belongs to */
	
		/* driver settings */
	ChannelDriver *driver;	/* only valid for drivers (i.e. stored in AnimData not Actions) */
		/* evaluation settings */
	ListBase modifiers;		/* FCurve Modifiers */
		
		/* motion data */
	BezTriple *bezt;		/* user-editable keyframes (array) */
	FPoint *fpt;			/* 'baked/imported' motion samples (array) */
	unsigned int totvert;	/* total number of points which define the curve (i.e. size of arrays in FPoints) */
	
		/* value cache + settings */
	float curval;			/* value stored from last time curve was evaluated */
	short flag;				/* user-editable settings for this curve */
	short extend;			/* value-extending mode for this curve (does not cover  */
	
		/* RNA - data link */
	int array_index;		/* if applicable, the index of the RNA-array item to get */
	char *rna_path;			/* RNA-path to resolve data-access */
	
		/* curve coloring (for editor) */
	int color_mode;			/* coloring method to use (eFCurve_Coloring) */
	float color[3];			/* the last-color this curve took */

	float prev_norm_factor, pad;
} FCurve;


/* user-editable flags/settings */
typedef enum eFCurve_Flags {
		/* curve/keyframes are visible in editor */
	FCURVE_VISIBLE		= (1<<0),
		/* curve is selected for editing  */
	FCURVE_SELECTED 	= (1<<1),
		/* curve is active one */
	FCURVE_ACTIVE		= (1<<2),
		/* keyframes (beztriples) cannot be edited */
	FCURVE_PROTECTED	= (1<<3),
		/* fcurve will not be evaluated for the next round */
	FCURVE_MUTED		= (1<<4),
	
		/* fcurve uses 'auto-handles', which stay horizontal... */
		// DEPRECATED
	FCURVE_AUTO_HANDLES	= (1<<5),
	
		/* skip evaluation, as RNA-path cannot be resolved (similar to muting, but cannot be set by user) */
	FCURVE_DISABLED			= (1<<10),
		/* curve can only have whole-number values (integer types) */
	FCURVE_INT_VALUES		= (1<<11),
		/* curve can only have certain discrete-number values (no interpolation at all, for enums/booleans) */
	FCURVE_DISCRETE_VALUES	= (1<<12),
	
		/* temporary tag for editing */
	FCURVE_TAGGED			= (1<<15)
} eFCurve_Flags;

/* extrapolation modes (only simple value 'extending') */
typedef enum eFCurve_Extend {
	FCURVE_EXTRAPOLATE_CONSTANT	= 0,	/* just extend min/max keyframe value  */
	FCURVE_EXTRAPOLATE_LINEAR			/* just extend gradient of segment between first segment keyframes */
} eFCurve_Extend;

/* curve coloring modes */
typedef enum eFCurve_Coloring {
	FCURVE_COLOR_AUTO_RAINBOW = 0,		/* automatically determine color using rainbow (calculated at drawtime) */
	FCURVE_COLOR_AUTO_RGB,				/* automatically determine color using XYZ (array index) <-> RGB */
	FCURVE_COLOR_CUSTOM					/* custom color */
} eFCurve_Coloring;

/* ************************************************ */
/* 'Action' Datatypes */

/* NOTE: Although these are part of the Animation System,
 * they are not stored here... see DNA_action_types.h instead
 */

 
/* ************************************************ */
/* Animation Reuse - i.e. users of Actions */

/* Retargetting ----------------------------------- */

/* Retargetting Pair
 *
 * Defines what parts of the paths should be remapped from 'abc' to 'xyz'.
 * TODO:
 *	- Regrex (possibly provided through PY, though having our own module might be faster)
 *	  would be important to have at some point. Current replacements are just simple
 *	  string matches...
 */
typedef struct AnimMapPair {
	char from[128];		/* part of path to bed replaced */
	char to[128];		/* part of path to replace with */
} AnimMapPair;

/* Retargetting Information for Actions 
 *
 * This should only be used if it is strictly necessary (i.e. user will need to explicitly 
 * add this when they find that some channels do not match, or motion is not going to right 
 * places). When executing an action, this will be checked to see if it provides any useful
 * remaps for the given paths.
 *
 * NOTE: we currently don't store this in the Action itself, as that causes too many problems.
 */
// FIXME: will this be too clumsy or slow? If we're using RNA paths anyway, we'll have to accept
// such consequences...
typedef struct AnimMapper {
	struct AnimMapper *next, *prev;
	
	bAction *target;		/* target action */
	ListBase mappings;		/* remapping table (bAnimMapPair) */
} AnimMapper;

/* ************************************************ */
/* NLA - Non-Linear Animation */

/* NLA Strips ------------------------------------- */

/* NLA Strip (strip)
 *
 * A NLA Strip is a container for the reuse of Action data, defining parameters
 * to control the remapping of the Action data to some destination. 
 */
typedef struct NlaStrip {
	struct NlaStrip *next, *prev;

	ListBase strips;            /* 'Child' strips (used for 'meta' strips) */
	bAction *act;               /* Action that is referenced by this strip (strip is 'user' of the action) */
	AnimMapper *remap;          /* Remapping info this strip (for tweaking correspondence of action with context) */

	ListBase fcurves;           /* F-Curves for controlling this strip's influence and timing */    // TODO: move out?
	ListBase modifiers;         /* F-Curve modifiers to be applied to the entire strip's referenced F-Curves */

	char name[64];              /* User-Visible Identifier for Strip */

	float influence;            /* Influence of strip */
	float strip_time;           /* Current 'time' within action being used (automatically evaluated, but can be overridden) */

	float start, end;           /* extents of the strip */
	float actstart, actend;     /* range of the action to use */

	float repeat;               /* The number of times to repeat the action range (only when no F-Curves) */
	float scale;                /* The amount the action range is scaled by (only when no F-Curves) */

	float blendin, blendout;    /* strip blending length (only used when there are no F-Curves) */
	short blendmode;            /* strip blending mode (layer-based mixing) */

	short extendmode;           /* strip extrapolation mode (time-based mixing) */
	short pad1;

	short type;                 /* type of NLA strip */

	void *speaker_handle;       /* handle for speaker objects */

	int flag;                   /* settings */
	int pad2;
} NlaStrip;

/* NLA Strip Blending Mode */
typedef enum eNlaStrip_Blend_Mode {
	NLASTRIP_MODE_REPLACE = 0,
	NLASTRIP_MODE_ADD,
	NLASTRIP_MODE_SUBTRACT,
	NLASTRIP_MODE_MULTIPLY
} eNlaStrip_Blend_Mode;

/* NLA Strip Extrpolation Mode */
typedef enum eNlaStrip_Extrapolate_Mode {
		/* extend before first frame if no previous strips in track, and always hold+extend last frame */
	NLASTRIP_EXTEND_HOLD = 0,
		/* only hold+extend last frame */
	NLASTRIP_EXTEND_HOLD_FORWARD = 1,
		/* don't contribute at all */
	NLASTRIP_EXTEND_NOTHING = 2
} eNlaStrip_Extrapolate_Mode;

/* NLA Strip Settings */
typedef enum eNlaStrip_Flag {
	/* UI selection flags */
		/* NLA strip is the active one in the track (also indicates if strip is being tweaked) */
	NLASTRIP_FLAG_ACTIVE        = (1<<0),
		/* NLA strip is selected for editing */
	NLASTRIP_FLAG_SELECT        = (1<<1),
//  NLASTRIP_FLAG_SELECT_L      = (1<<2),   // left handle selected
//  NLASTRIP_FLAG_SELECT_R      = (1<<3),   // right handle selected
		/* NLA strip uses the same action that the action being tweaked uses (not set for the twaking one though) */
	NLASTRIP_FLAG_TWEAKUSER     = (1<<4),

	/* controls driven by local F-Curves */
		/* strip influence is controlled by local F-Curve */
	NLASTRIP_FLAG_USR_INFLUENCE = (1<<5),
	NLASTRIP_FLAG_USR_TIME      = (1<<6),
	NLASTRIP_FLAG_USR_TIME_CYCLIC = (1<<7),

		/* NLA strip length is synced to the length of the referenced action */
	NLASTRIP_FLAG_SYNC_LENGTH   = (1<<9),

	/* playback flags (may be overridden by F-Curves) */
		/* NLA strip blendin/out values are set automatically based on overlaps */
	NLASTRIP_FLAG_AUTO_BLENDS   = (1<<10),
		/* NLA strip is played back in reverse order */
	NLASTRIP_FLAG_REVERSE       = (1<<11),
		/* NLA strip is muted (i.e. doesn't contribute in any way) */
	NLASTRIP_FLAG_MUTED         = (1<<12),
		/* NLA Strip is played back in 'ping-pong' style */
	NLASTRIP_FLAG_MIRROR        = (1<<13),

	/* temporary editing flags */
		/* NLA-Strip is really just a temporary meta used to facilitate easier transform code */
	NLASTRIP_FLAG_TEMP_META     = (1<<30),
	NLASTRIP_FLAG_EDIT_TOUCHED  = (1<<31)
} eNlaStrip_Flag;

/* NLA Strip Type */
typedef enum eNlaStrip_Type {	
		/* 'clip' - references an Action */
	NLASTRIP_TYPE_CLIP	= 0,
		/* 'transition' - blends between the adjacent strips */
	NLASTRIP_TYPE_TRANSITION,
		/* 'meta' - a strip which acts as a container for a few others */
	NLASTRIP_TYPE_META,
	
		/* 'emit sound' - a strip which is used for timing when speaker emits sounds */
	NLASTRIP_TYPE_SOUND
} eNlaStrip_Type;

/* NLA Tracks ------------------------------------- */

/* NLA Track (nlt)
 *
 * A track groups a bunch of 'strips', which should form a continuous set of 
 * motion, on top of which other such groups can be layered. This should allow
 * for animators to work in a non-destructive manner, layering tweaks, etc. over
 * 'rough' blocks of their work.
 */
typedef struct NlaTrack {
	struct NlaTrack *next, *prev;
	
	ListBase strips;		/* bActionStrips in this track */
	
	int flag;				/* settings for this track */
	int index;				/* index of the track in the stack (NOTE: not really useful, but we need a pad var anyways!) */
	
	char name[64];			/* short user-description of this track */
} NlaTrack;

/* settings for track */
typedef enum eNlaTrack_Flag {
		/* track is the one that settings can be modified on, also indicates if track is being 'tweaked' */
	NLATRACK_ACTIVE		= (1<<0),
		/* track is selected in UI for relevant editing operations */
	NLATRACK_SELECTED	= (1<<1),
		/* track is not evaluated */
	NLATRACK_MUTED		= (1<<2),
		/* track is the only one evaluated (must be used in conjunction with adt->flag) */
	NLATRACK_SOLO		= (1<<3),
		/* track's settings (and strips) cannot be edited (to guard against unwanted changes) */
	NLATRACK_PROTECTED	= (1<<4),
	
		/* track is not allowed to execute, usually as result of tweaking being enabled (internal flag) */
	NLATRACK_DISABLED	= (1<<10)
} eNlaTrack_Flag;


/* ************************************ */
/* KeyingSet Datatypes */

/* Path for use in KeyingSet definitions (ksp) 
 *
 * Paths may be either specific (specifying the exact sub-ID
 * dynamic data-block - such as PoseChannels - to act upon, ala
 * Maya's 'Character Sets' and XSI's 'Marking Sets'), or they may
 * be generic (using various placeholder template tags that will be
 * replaced with appropriate information from the context). 
 */
typedef struct KS_Path {
	struct KS_Path *next, *prev;
	
	ID *id;					/* ID block that keyframes are for */
	char group[64];			/* name of the group to add to */
	
	int idtype;				/* ID-type that path can be used on */
	
	short groupmode;		/* group naming (eKSP_Grouping) */
	short flag;				/* various settings, etc. */
	
	char *rna_path;			/* dynamically (or statically in the case of predefined sets) path */
	int array_index;		/* index that path affects */
	
	short keyingflag;		/* (eInsertKeyFlags) settings to supply insertkey() with */
	short keyingoverride;	/* (eInsertKeyFlags) for each flag set, the relevant keyingflag bit overrides the default */
} KS_Path;

/* KS_Path->flag */
typedef enum eKSP_Settings {
		/* entire array (not just the specified index) gets keyframed */
	KSP_FLAG_WHOLE_ARRAY	= (1<<0)
} eKSP_Settings;

/* KS_Path->groupmode */
typedef enum eKSP_Grouping {
		/* path should be grouped using group name stored in path */
	KSP_GROUP_NAMED = 0,
		/* path should not be grouped at all */
	KSP_GROUP_NONE,
		/* path should be grouped using KeyingSet's name */
	KSP_GROUP_KSNAME,
		/* path should be grouped using name of inner-most context item from templates 
		 * 	- this is most useful for relative KeyingSets only
		 */
	KSP_GROUP_TEMPLATE_ITEM
} eKSP_Grouping;

/* ---------------- */
 
/* KeyingSet definition (ks)
 *
 * A KeyingSet defines a group of properties that should
 * be keyframed together, providing a convenient way for animators
 * to insert keyframes without resorting to Auto-Keyframing.
 *
 * A few 'generic' (non-absolute and dependent on templates) KeyingSets 
 * are defined 'built-in' to facilitate easy animating for the casual
 * animator without the need to add extra steps to the rigging process.
 */
typedef struct KeyingSet {
	struct KeyingSet *next, *prev;
	
	ListBase paths;			/* (KS_Path) paths to keyframe to */
	
	char idname[64];		/* unique name (for search, etc.) */
	char name[64];			/* user-viewable name for KeyingSet (for menus, etc.) */
	char description[240];	/* (RNA_DYN_DESCR_MAX) short help text. */
	char typeinfo[64];		/* name of the typeinfo data used for the relative paths */
	
	int active_path;		/* index of the active path */
	
	short flag;				/* settings for KeyingSet */
	
	short keyingflag;		/* (eInsertKeyFlags) settings to supply insertkey() with */
	short keyingoverride;	/* (eInsertKeyFlags) for each flag set, the relevant keyingflag bit overrides the default */
	
	char pad[6];
} KeyingSet;

/* KeyingSet settings */
typedef enum eKS_Settings {
		/* keyingset cannot be removed (and doesn't need to be freed) */
	KEYINGSET_BUILTIN		= (1<<0),
		/* keyingset does not depend on context info (i.e. paths are absolute) */
	KEYINGSET_ABSOLUTE		= (1<<1)
} eKS_Settings;

/* Flags for use by keyframe creation/deletion calls */
typedef enum eInsertKeyFlags {
	INSERTKEY_NEEDED 	= (1<<0),	/* only insert keyframes where they're needed */
	INSERTKEY_MATRIX 	= (1<<1),	/* insert 'visual' keyframes where possible/needed */
	INSERTKEY_FAST 		= (1<<2),	/* don't recalculate handles,etc. after adding key */
	INSERTKEY_FASTR		= (1<<3),	/* don't realloc mem (or increase count, as array has already been set out) */
	INSERTKEY_REPLACE 	= (1<<4),	/* only replace an existing keyframe (this overrides INSERTKEY_NEEDED) */
	INSERTKEY_XYZ2RGB	= (1<<5),	/* transform F-Curves should have XYZ->RGB color mode */
	INSERTKEY_NO_USERPREF	= (1 << 6),	/* ignore user-prefs (needed for predictable API use) */
	/* Allow to make a full copy of new key into existing one, if any, instead of 'reusing' existing handles.
	 * Used by copy/paste code. */
	INSERTKEY_OVERWRITE_FULL = (1<<7),
} eInsertKeyFlags;

/* ************************************************ */
/* Animation Data */

/* AnimOverride ------------------------------------- */

/* Animation Override (aor) 
 *
 * This is used to as temporary storage of values which have been changed by the user, but not
 * yet keyframed (thus, would get overwritten by the animation system before the user had a chance
 * to see the changes that were made). 
 *
 * It is probably not needed for overriding keyframed values in most cases, as those will only get evaluated
 * on frame-change now. That situation may change in future.
 */
typedef struct AnimOverride {
	struct AnimOverride *next, *prev;
	
	char *rna_path;			/* RNA-path to use to resolve data-access */
	int array_index;		/* if applicable, the index of the RNA-array item to get */
	
	float value;			/* value to override setting with */
} AnimOverride;

/* AnimData ------------------------------------- */

/* Animation data for some ID block (adt)
 * 
 * This block of data is used to provide all of the necessary animation data for a datablock.
 * Currently, this data will not be reusable, as there shouldn't be any need to do so.
 * 
 * This information should be made available for most if not all ID-blocks, which should 
 * enable all of its settings to be animatable locally. Animation from 'higher-up' ID-AnimData
 * blocks may override local settings.
 *
 * This datablock should be placed immediately after the ID block where it is used, so that
 * the code which retrieves this data can do so in an easier manner. See blenkernel/intern/anim_sys.c for details.
 */
typedef struct AnimData {
		/* active action - acts as the 'tweaking track' for the NLA */
	bAction     *action;
		/* temp-storage for the 'real' active action (i.e. the one used before the tweaking-action
		 * took over to be edited in the Animation Editors)
		 */
	bAction     *tmpact;
		/* remapping-info for active action - should only be used if needed
		 * (for 'foreign' actions that aren't working correctly)
		 */
	AnimMapper  *remap;

		/* nla-tracks */
	ListBase    nla_tracks;
		/* active NLA-strip (only set/used during tweaking, so no need to worry about dangling pointers) */
	NlaStrip    *actstrip;

	/* 'drivers' for this ID-block's settings - FCurves, but are completely
	 * separate from those for animation data
	 */
	ListBase    drivers;    /* standard user-created Drivers/Expressions (used as part of a rig) */
	ListBase    overrides;  /* temp storage (AnimOverride) of values for settings that are animated (but the value hasn't been keyframed) */

		/* settings for animation evaluation */
	int flag;               /* user-defined settings */
	int recalc;             /* depsgraph recalculation flags */

		/* settings for active action evaluation (based on NLA strip settings) */
	short act_blendmode;    /* accumulation mode for active action */
	short act_extendmode;   /* extrapolation mode for active action */
	float act_influence;    /* influence for active action */
} AnimData;

/* Animation Data settings (mostly for NLA) */
typedef enum eAnimData_Flag {
		/* only evaluate a single track in the NLA */
	ADT_NLA_SOLO_TRACK      = (1<<0),
		/* don't use NLA */
	ADT_NLA_EVAL_OFF        = (1<<1),
		/* NLA is being 'tweaked' (i.e. in EditMode) */
	ADT_NLA_EDIT_ON         = (1<<2),
		/* active Action for 'tweaking' does not have mapping applied for editing */
	ADT_NLA_EDIT_NOMAP      = (1<<3),
		/* NLA-Strip F-Curves are expanded in UI */
	ADT_NLA_SKEYS_COLLAPSED = (1<<4),

		/* drivers expanded in UI */
	ADT_DRIVERS_COLLAPSED   = (1<<10),
		/* don't execute drivers */
	ADT_DRIVERS_DISABLED    = (1<<11),

		/* AnimData block is selected in UI */
	ADT_UI_SELECTED         = (1<<14),
		/* AnimData block is active in UI */
	ADT_UI_ACTIVE           = (1<<15),

		/* F-Curves from this AnimData block are not visible in the Graph Editor */
	ADT_CURVES_NOT_VISIBLE  = (1<<16)
} eAnimData_Flag;

/* Animation Data recalculation settings (to be set by depsgraph) */
typedef enum eAnimData_Recalc {
	ADT_RECALC_DRIVERS      = (1 << 0),
	ADT_RECALC_ANIM         = (1 << 1),
	ADT_RECALC_ALL          = (ADT_RECALC_DRIVERS | ADT_RECALC_ANIM)
} eAnimData_Recalc;

/* Base Struct for Anim ------------------------------------- */

/* Used for BKE_animdata_from_id() 
 * All ID-datablocks which have their own 'local' AnimData
 * should have the same arrangement in their structs.
 */
typedef struct IdAdtTemplate {
	ID id;
	AnimData *adt;
} IdAdtTemplate;

/* ************************************************ */

#ifdef __cplusplus
};
#endif

#endif /* __DNA_ANIM_TYPES_H__ */
